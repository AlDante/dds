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


def build_command(
    method: str,
    hand_file: str,
    depth: int,
    max_boards: int,
    skip_boards: str,
    parallel: str,
    worker_backend: str,
    board_workers: int,
    root_workers: int,
    dds_thread_id: int,
) -> list[str]:
    if method == "dds":
        command = ["./build/alpha_mu", "benchmark_dds", hand_file, str(max_boards)]
        if skip_boards:
            command.append(skip_boards)
        return command
    command = [
        "./build/alpha_mu",
        "benchmark_alpha",
        hand_file,
        str(depth),
        str(max_boards),
    ]
    if skip_boards:
        command.append(skip_boards)
    command.extend(
        [
            "--parallel",
            parallel,
            "--worker-backend",
            worker_backend,
            "--board-workers",
            str(board_workers),
            "--root-workers",
            str(root_workers),
            "--dds-thread-id",
            str(dds_thread_id),
        ]
    )
    return command


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


def update_reported_benchmark_settings(
    status: dict[str, Any], fields: dict[str, str]
) -> None:
    parallel_value = fields.get("parallel")
    if parallel_value is not None:
        status["benchmark_parallel"] = parallel_value

    for field_name, status_name in (
        ("worker_backend", "benchmark_worker_backend"),
        ("board_workers", "benchmark_board_workers"),
        ("root_workers", "benchmark_root_workers"),
        ("dds_thread_id", "benchmark_dds_thread_id"),
        ("configured_board_workers", "benchmark_configured_board_workers"),
    ):
        raw_value = fields.get(field_name)
        if raw_value is None:
            continue
        if field_name == "worker_backend":
            status[status_name] = raw_value
            continue
        try:
            status[status_name] = int(raw_value)
        except ValueError:
            status[status_name] = raw_value


def parse_int_field(fields: dict[str, str], name: str) -> int | str | None:
    raw_value = fields.get(name)
    if raw_value is None:
        return None
    try:
        return int(raw_value)
    except ValueError:
        return raw_value


def parse_float_field(fields: dict[str, str], name: str) -> float | str | None:
    raw_value = fields.get(name)
    if raw_value is None:
        return None
    try:
        return float(raw_value)
    except ValueError:
        return raw_value


def extract_alpha_mu_benchmark_metrics(fields: dict[str, str]) -> dict[str, Any]:
    metrics: dict[str, Any] = {}
    for field_name in ("search_nodes", "dds_leaf_calls"):
        value = parse_int_field(fields, field_name)
        if value is not None:
            metrics[field_name] = value
    for field_name in ("dds_leaf_seconds", "bridge_search_seconds"):
        value = parse_float_field(fields, field_name)
        if value is not None:
            metrics[field_name] = value
    return metrics


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run alpha_mu benchmark modes with live checkpoint logging."
    )
    parser.add_argument("--method", choices=["alpha_mu", "dds"], default="alpha_mu")
    parser.add_argument("--hand-file", default="hands/list1.txt")
    parser.add_argument("--depth", type=int, default=2)
    parser.add_argument("--max-boards", type=int, default=0)
    parser.add_argument("--checkpoint-seconds", type=float, default=30.0)
    parser.add_argument("--heartbeat-seconds", type=float, default=30.0)
    parser.add_argument("--skip-boards", default="")
    parser.add_argument("--parallel", choices=["serial", "board", "root"], default="serial")
    parser.add_argument("--worker-backend", choices=["stl", "gcd"], default="stl")
    parser.add_argument("--board-workers", type=int, default=1)
    parser.add_argument("--root-workers", type=int, default=1)
    parser.add_argument("--dds-thread-id", type=int, default=0)
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
    if args.board_workers < 1:
        raise SystemExit("--board-workers must be positive")
    if args.root_workers < 1:
        raise SystemExit("--root-workers must be positive")
    if args.dds_thread_id < 0:
        raise SystemExit("--dds-thread-id must be non-negative")

    log_path = Path(args.log_path).resolve() if args.log_path else default_log_path(
        args.method, args.hand_file, args.depth
    )
    status_path = Path(f"{log_path}.status.json")
    command = build_command(
        args.method,
        args.hand_file,
        args.depth,
        args.max_boards,
        args.skip_boards,
        args.parallel,
        args.worker_backend,
        args.board_workers,
        args.root_workers,
        args.dds_thread_id,
    )

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
        f"skip_boards={args.skip_boards}\n",
        f"parallel={args.parallel}\n",
        f"worker_backend={args.worker_backend}\n",
        f"board_workers={args.board_workers}\n",
        f"root_workers={args.root_workers}\n",
        f"dds_thread_id={args.dds_thread_id}\n",
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
        "skip_boards": args.skip_boards,
        "parallel": args.parallel,
        "worker_backend": args.worker_backend,
        "board_workers": args.board_workers,
        "root_workers": args.root_workers,
        "dds_thread_id": args.dds_thread_id,
        "command": command,
        "pid": proc.pid,
        "log_path": str(log_path),
        "checkpoint_seconds": args.checkpoint_seconds,
        "heartbeat_seconds": args.heartbeat_seconds,
        "benchmark_parallel": None,
        "benchmark_worker_backend": None,
        "benchmark_board_workers": None,
        "benchmark_root_workers": None,
        "benchmark_dds_thread_id": None,
        "benchmark_configured_board_workers": None,
        "last_checkpoint_line": "",
        "last_progress_line": "",
        "benchmark_summary_line": "",
        "benchmark_summary_metrics": {},
        "per_board_seconds": [],
        "completed_board_numbers": [],
        "per_board_timings": [],
        "current_board_progress": {},
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
        elif item.startswith("ALPHA_MU_BENCHMARK_PROGRESS "):
            fields: dict[str, str] = {}
            for token in item.strip().split()[1:]:
                if "=" not in token:
                    continue
                key, value = token.split("=", 1)
                fields[key] = value
            progress: dict[str, Any] = {}
            for key, value in fields.items():
                if key in {"board", "total_boards", "depth", "recursive_calls", "dds_leaf_calls", "tricks_remaining", "active_worlds", "current_trick_size", "player"}:
                    try:
                        progress[key] = int(value)
                    except ValueError:
                        progress[key] = value
                elif key in {"board_elapsed_seconds", "elapsed_seconds"}:
                    try:
                        progress[key] = float(value)
                    except ValueError:
                        progress[key] = value
                else:
                    progress[key] = value
            update_reported_benchmark_settings(status, fields)
            status["last_progress_line"] = item.rstrip("\n")
            status["current_board_progress"] = progress
        elif item.startswith("ALPHA_MU_BENCHMARK_BOARD "):
            fields: dict[str, str] = {}
            for token in item.strip().split()[1:]:
                if "=" not in token:
                    continue
                key, value = token.split("=", 1)
                fields[key] = value
            update_reported_benchmark_settings(status, fields)
            board_metrics = extract_alpha_mu_benchmark_metrics(fields)
            board_value = fields.get("board_seconds")
            board_number_value = fields.get("board")
            if board_value is not None:
                try:
                    status["per_board_seconds"].append(float(board_value))
                except ValueError:
                    pass
            if board_number_value is not None and board_value is not None:
                try:
                    board_number = int(board_number_value)
                    board_seconds = float(board_value)
                    status["completed_board_numbers"].append(board_number)
                    timing_entry: dict[str, Any] = {
                        "board": board_number,
                        "seconds": board_seconds,
                    }
                    timing_entry.update(board_metrics)
                    status["per_board_timings"].append(timing_entry)
                    current_progress = status.get("current_board_progress")
                    if isinstance(current_progress, dict) and current_progress.get("board") == board_number:
                        status["current_board_progress"] = {}
                except ValueError:
                    pass
        elif item.startswith("ALPHA_MU_BENCHMARK "):
            fields: dict[str, str] = {}
            for token in item.strip().split()[1:]:
                if "=" not in token:
                    continue
                key, value = token.split("=", 1)
                fields[key] = value
            update_reported_benchmark_settings(status, fields)
            status["benchmark_summary_line"] = item.rstrip("\n")
            status["benchmark_summary_metrics"] = extract_alpha_mu_benchmark_metrics(fields)
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

