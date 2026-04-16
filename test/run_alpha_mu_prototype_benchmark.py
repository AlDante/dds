#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import queue
import subprocess
import threading
import time
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
TEST_DIR = ROOT / "test"
BUILD_DIR = TEST_DIR / "build"


def timestamp_tag() -> str:
    return time.strftime("%Y%m%d-%H%M%S")


def default_log_path(method: str, hand_file: str, depth: int) -> Path:
    stem = Path(hand_file).stem
    if method == "dds":
        name = f"{stem}_{method}_{timestamp_tag()}.log"
    else:
        name = f"{stem}_{method}_depth{depth}_{timestamp_tag()}.log"
    return BUILD_DIR / name


def build_command(method: str, hand_file: str, depth: int, max_boards: int) -> list[str]:
    if method == "dds":
        return ["./build/alpha_mu_prototype", "benchmark_dds", hand_file, str(max_boards)]
    return [
        "./build/alpha_mu_prototype",
        "benchmark_alpha",
        hand_file,
        str(depth),
        str(max_boards),
    ]


def append_log_line(path: Path, line: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as handle:
        handle.write(line)
        handle.flush()
        os.fsync(handle.fileno())


def write_status(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")


def stream_output(proc: subprocess.Popen[str], output_queue: queue.Queue[str | None]) -> None:
    assert proc.stdout is not None
    try:
        for line in proc.stdout:
            output_queue.put(line)
    finally:
        output_queue.put(None)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run alpha_mu_prototype benchmark modes with live checkpoint logging."
    )
    parser.add_argument("--method", choices=["alpha_mu", "dds"], default="alpha_mu")
    parser.add_argument("--hand-file", default="hands/list1.txt")
    parser.add_argument("--depth", type=int, default=2)
    parser.add_argument("--max-boards", type=int, default=0)
    parser.add_argument("--checkpoint-seconds", type=float, default=30.0)
    parser.add_argument("--heartbeat-seconds", type=float, default=30.0)
    parser.add_argument("--log-path", default="")
    args = parser.parse_args()

    if args.depth < 0:
        raise SystemExit("--depth must be non-negative")
    if args.max_boards < 0:
        raise SystemExit("--max-boards must be non-negative")
    if args.checkpoint_seconds < 0.0:
        raise SystemExit("--checkpoint-seconds must be non-negative")
    if args.heartbeat_seconds <= 0.0:
        raise SystemExit("--heartbeat-seconds must be positive")

    log_path = Path(args.log_path).resolve() if args.log_path else default_log_path(
        args.method, args.hand_file, args.depth
    )
    status_path = Path(f"{log_path}.status.json")
    command = build_command(args.method, args.hand_file, args.depth, args.max_boards)

    env = os.environ.copy()
    env["DYLD_LIBRARY_PATH"] = str(ROOT / "src" / "build")
    if args.checkpoint_seconds > 0.0:
        env["DDS_ALPHA_MU_BENCHMARK_CHECKPOINT_SECONDS"] = f"{args.checkpoint_seconds:.6f}"
    else:
        env.pop("DDS_ALPHA_MU_BENCHMARK_CHECKPOINT_SECONDS", None)

    started = time.perf_counter()
    started_wall = time.strftime("%Y-%m-%d %H:%M:%S")
    header_lines = [
        f"started_at={started_wall}\n",
        f"method={args.method}\n",
        f"hand_file={args.hand_file}\n",
        f"depth={args.depth}\n",
        f"max_boards={args.max_boards}\n",
        f"checkpoint_seconds={args.checkpoint_seconds:.6f}\n",
        f"heartbeat_seconds={args.heartbeat_seconds:.6f}\n",
        f"command={' '.join(command)}\n",
        "\n",
    ]
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text("".join(header_lines), encoding="utf-8")

    proc = subprocess.Popen(
        command,
        cwd=str(TEST_DIR),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    status: dict[str, Any] = {
        "state": "running",
        "started_at": started_wall,
        "method": args.method,
        "hand_file": args.hand_file,
        "depth": args.depth,
        "max_boards": args.max_boards,
        "command": command,
        "pid": proc.pid,
        "log_path": str(log_path),
        "checkpoint_seconds": args.checkpoint_seconds,
        "heartbeat_seconds": args.heartbeat_seconds,
        "last_checkpoint_line": "",
        "last_output_line": "",
        "elapsed_seconds": 0.0,
        "returncode": None,
    }
    write_status(status_path, status)

    output_queue: queue.Queue[str | None] = queue.Queue()
    reader = threading.Thread(target=stream_output, args=(proc, output_queue), daemon=True)
    reader.start()

    stream_closed = False
    last_heartbeat = started
    while not stream_closed:
        try:
            item = output_queue.get(timeout=0.5)
        except queue.Empty:
            now = time.perf_counter()
            if now - last_heartbeat >= args.heartbeat_seconds:
                elapsed = now - started
                heartbeat_line = f"RUNNER_HEARTBEAT elapsed_seconds={elapsed:.6f}\n"
                append_log_line(log_path, heartbeat_line)
                print(heartbeat_line, end="")
                status["elapsed_seconds"] = elapsed
                status["last_output_line"] = heartbeat_line.strip()
                write_status(status_path, status)
                last_heartbeat = now
            continue

        if item is None:
            stream_closed = True
            continue

        append_log_line(log_path, item)
        print(item, end="")
        status["elapsed_seconds"] = time.perf_counter() - started
        status["last_output_line"] = item.rstrip("\n")
        if item.startswith("ALPHA_MU_BENCHMARK_CHECKPOINT "):
            status["last_checkpoint_line"] = item.rstrip("\n")
        write_status(status_path, status)

    returncode = proc.wait()
    finished_wall = time.strftime("%Y-%m-%d %H:%M:%S")
    elapsed = time.perf_counter() - started
    footer = (
        "\n"
        f"finished_at={finished_wall}\n"
        f"elapsed_seconds={elapsed:.6f}\n"
        f"returncode={returncode}\n"
    )
    append_log_line(log_path, footer)

    status["state"] = "finished"
    status["finished_at"] = finished_wall
    status["elapsed_seconds"] = elapsed
    status["returncode"] = returncode
    write_status(status_path, status)

    print(f"Log path: {log_path}")
    print(f"Status path: {status_path}")
    return returncode


if __name__ == "__main__":
    raise SystemExit(main())

