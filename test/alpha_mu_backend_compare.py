#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import platform
import shlex
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
TEST_DIR = ROOT / "test"
BUILD_DIR = TEST_DIR / "build"
DEFAULT_BACKENDS = ["stl", "gcd"]


def timestamp_tag() -> str:
    return time.strftime("%Y%m%d-%H%M%S")


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def run_command(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> dict[str, Any]:
    started = time.perf_counter()
    proc = subprocess.run(
        command,
        cwd=str(cwd),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    elapsed = time.perf_counter() - started
    return {
        "command": command,
        "cwd": str(cwd),
        "returncode": proc.returncode,
        "elapsed_seconds": elapsed,
        "output": proc.stdout,
    }


def git_text(*args: str) -> str:
    proc = subprocess.run(
        ["git", *args],
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        return "unknown"
    return proc.stdout.strip()


def git_is_dirty() -> bool:
    return git_text("status", "--short") not in ("", "unknown")


def build_steps() -> list[tuple[str, list[str], Path]]:
    return [
        ("build_library", ["make", "macos"], ROOT / "src"),
        (
            "build_alpha_mu",
            ["make", "-f", "Makefiles/Makefile_Mac_clang", "alpha_mu"],
            ROOT / "test",
        ),
    ]


def parse_machine_line(line: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for token in line.strip().split()[1:]:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        fields[key] = value
    return fields


def parse_summary_fields(summary_line: str) -> dict[str, Any]:
    raw_fields = parse_machine_line(summary_line)
    fields: dict[str, Any] = dict(raw_fields)
    for name in (
        "boards",
        "depth",
        "board_workers",
        "root_workers",
        "dds_thread_id",
        "configured_board_workers",
        "mismatches",
        "search_nodes",
        "dds_leaf_calls",
    ):
        if name in fields:
            fields[name] = int(fields[name])
    for name in ("total_seconds", "per_board_seconds", "dds_leaf_seconds", "bridge_search_seconds"):
        if name in fields:
            fields[name] = float(fields[name])
    return fields


def summarize_series(values: list[float]) -> dict[str, float]:
    if not values:
        return {"median": 0.0, "mean": 0.0, "min": 0.0, "max": 0.0, "stdev": 0.0, "cv": 0.0}

    mean_value = statistics.fmean(values)
    stdev_value = statistics.stdev(values) if len(values) > 1 else 0.0
    cv_value = 0.0 if mean_value == 0.0 else stdev_value / mean_value
    return {
        "median": statistics.median(values),
        "mean": mean_value,
        "min": min(values),
        "max": max(values),
        "stdev": stdev_value,
        "cv": cv_value,
    }


def semantic_signature_from_status(status: dict[str, Any]) -> dict[str, Any]:
    summary_fields = parse_summary_fields(status["benchmark_summary_line"])
    per_board_signature = []
    for entry in status.get("per_board_timings", []):
        per_board_signature.append(
            {
                "board": int(entry["board"]),
                "search_nodes": int(entry.get("search_nodes", 0)),
                "dds_leaf_calls": int(entry.get("dds_leaf_calls", 0)),
            }
        )

    return {
        "boards": int(summary_fields.get("boards", 0)),
        "depth": int(summary_fields.get("depth", 0)),
        "mismatches": int(summary_fields.get("mismatches", 0)),
        "parallel": str(summary_fields.get("parallel", "")),
        "board_workers": int(summary_fields.get("board_workers", 0)),
        "root_workers": int(summary_fields.get("root_workers", 0)),
        "dds_thread_id": int(summary_fields.get("dds_thread_id", 0)),
        "configured_board_workers": int(summary_fields.get("configured_board_workers", 0)),
        "search_nodes": int(summary_fields.get("search_nodes", 0)),
        "dds_leaf_calls": int(summary_fields.get("dds_leaf_calls", 0)),
        "completed_board_numbers": [int(x) for x in status.get("completed_board_numbers", [])],
        "per_board_signature": per_board_signature,
    }


def backend_semantic_signature(signature: dict[str, Any]) -> dict[str, Any]:
    return {
        "boards": signature["boards"],
        "depth": signature["depth"],
        "mismatches": signature["mismatches"],
        "completed_board_numbers": signature["completed_board_numbers"],
        "search_nodes": signature["search_nodes"],
        "dds_leaf_calls": signature["dds_leaf_calls"],
        "per_board_signature": signature["per_board_signature"],
    }


def validate_status(
    status: dict[str, Any],
    expected_backend: str,
    expected_parallel: str,
    expected_board_workers: int,
    expected_root_workers: int,
) -> dict[str, Any]:
    if status.get("returncode") != 0:
        raise RuntimeError(f"backend {expected_backend} run returned {status.get('returncode')}")
    if not status.get("benchmark_summary_line"):
        raise RuntimeError(f"backend {expected_backend} run did not produce a benchmark summary line")
    if status.get("benchmark_worker_backend") != expected_backend:
        raise RuntimeError(
            f"backend {expected_backend} run reported benchmark_worker_backend={status.get('benchmark_worker_backend')}"
        )
    if status.get("benchmark_parallel") != expected_parallel:
        raise RuntimeError(
            f"backend {expected_backend} run reported benchmark_parallel={status.get('benchmark_parallel')}"
        )
    if status.get("benchmark_board_workers") != expected_board_workers:
        raise RuntimeError(
            f"backend {expected_backend} run reported benchmark_board_workers={status.get('benchmark_board_workers')}"
        )
    if status.get("benchmark_root_workers") != expected_root_workers:
        raise RuntimeError(
            f"backend {expected_backend} run reported benchmark_root_workers={status.get('benchmark_root_workers')}"
        )

    signature = semantic_signature_from_status(status)
    if signature["mismatches"] != 0:
        raise RuntimeError(f"backend {expected_backend} run reported mismatches={signature['mismatches']}")
    if not signature["completed_board_numbers"]:
        raise RuntimeError(f"backend {expected_backend} run did not report completed boards")
    return signature


def markdown_summary(summary: dict[str, Any]) -> str:
    lines: list[str] = []
    lines.append("# Alpha-Mu Worker Backend Comparison Summary")
    lines.append("")
    lines.append(f"- Generated: {summary['generated_at']}")
    lines.append(f"- Repository: `{summary['repository']}`")
    lines.append(f"- Commit: `{summary['git_commit']}`")
    lines.append(f"- Dirty tree: `{summary['git_dirty']}`")
    lines.append(f"- Platform: `{summary['platform']}`")
    lines.append(f"- Python: `{summary['python']}`")
    lines.append(f"- Hand file: `{summary['hand_file']}`")
    lines.append(f"- Depth: `{summary['depth']}`")
    lines.append(f"- Max boards: `{summary['max_boards']}`")
    lines.append(f"- Skip boards: `{summary['skip_boards']}`")
    lines.append(f"- Parallel mode: `{summary['parallel']}`")
    lines.append(f"- Board workers: `{summary['board_workers']}`")
    lines.append(f"- Root workers: `{summary['root_workers']}`")
    lines.append(f"- DDS thread id: `{summary['dds_thread_id']}`")
    lines.append(f"- Warmup runs per backend: `{summary['warmups']}`")
    lines.append(f"- Measured repeats per backend: `{summary['repeats']}`")
    lines.append("")
    lines.append("## Build steps")
    lines.append("")
    for step in summary["build_steps"]:
        lines.append(
            f"- `{step['name']}`: `{step['command']}` → rc `{step['returncode']}` in `{step['elapsed_seconds']:.3f}` s"
        )
    lines.append("")
    lines.append("## Backend medians")
    lines.append("")
    lines.append("| Backend | Median total (s) | Median per board (s) | Min (s) | Max (s) | CV | Deterministic semantics |")
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: | --- |")
    for backend in summary["backends"]:
        timing = backend["timing"]
        lines.append(
            f"| `{backend['backend']}` | {timing['total_seconds']['median']:.6f} | {timing['per_board_seconds']['median']:.6f} | "
            f"{timing['total_seconds']['min']:.6f} | {timing['total_seconds']['max']:.6f} | {timing['total_seconds']['cv']:.4f} | "
            f"{'yes' if backend['semantics_stable'] else 'no'} |"
        )
    lines.append("")
    lines.append("## Cross-backend semantic comparison")
    lines.append("")
    lines.append(
        f"- Shared semantic signature across all compared backends: `{'yes' if summary['cross_backend_semantics_match'] else 'no'}`"
    )
    lines.append("")
    lines.append("## Notes")
    lines.append("")
    lines.append("- This routine reuses `test/run_alpha_mu_benchmark.py` so each run still emits the normal machine-readable benchmark lines and `.status.json` sidecar.")
    lines.append("- Timing-noise warnings are advisory by default; semantic drift remains a hard failure.")
    warning_lines = summary.get("warnings", [])
    if warning_lines:
        lines.append("")
        lines.append("## Timing-noise warnings")
        lines.append("")
        for warning in warning_lines:
            lines.append(f"- {warning}")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare repeated alpha-mu board-parallel benchmark runs across worker backends."
    )
    parser.add_argument("--hand-file", default="hands/list10.txt")
    parser.add_argument("--depth", type=int, default=3)
    parser.add_argument("--max-boards", type=int, default=0)
    parser.add_argument("--skip-boards", default="2")
    parser.add_argument("--parallel", choices=["serial", "board", "root"], default="board")
    parser.add_argument("--board-workers", type=int, default=4)
    parser.add_argument("--root-workers", type=int, default=1)
    parser.add_argument("--dds-thread-id", type=int, default=0)
    parser.add_argument("--warmups", type=int, default=1)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--warn-cv", type=float, default=0.10)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--output-dir", default="")
    parser.add_argument(
        "--backend",
        action="append",
        default=[],
        help="Optional worker backend to compare. Repeat to compare more than one backend. Defaults to stl and gcd.",
    )
    args = parser.parse_args()

    if args.depth < 0:
        raise SystemExit("--depth must be non-negative")
    if args.max_boards < 0:
        raise SystemExit("--max-boards must be non-negative")
    if args.board_workers < 1:
        raise SystemExit("--board-workers must be positive")
    if args.root_workers < 1:
        raise SystemExit("--root-workers must be positive")
    if args.dds_thread_id < 0:
        raise SystemExit("--dds-thread-id must be non-negative")
    if args.warmups < 0:
        raise SystemExit("--warmups must be non-negative")
    if args.repeats <= 0:
        raise SystemExit("--repeats must be positive")
    if args.warn_cv < 0.0:
        raise SystemExit("--warn-cv must be non-negative")

    requested_backends = args.backend or list(DEFAULT_BACKENDS)
    backends: list[str] = []
    for backend in requested_backends:
        if backend not in ("stl", "gcd"):
            raise SystemExit(f"unsupported backend '{backend}'")
        if backend not in backends:
            backends.append(backend)
    if len(backends) < 2:
        raise SystemExit("compare at least two backends to make PR3 useful")

    output_dir = Path(args.output_dir).resolve() if args.output_dir else BUILD_DIR / "alpha_mu_backend_compare" / timestamp_tag()
    output_dir.mkdir(parents=True, exist_ok=True)

    runner_path = ROOT / "test" / "run_alpha_mu_benchmark.py"
    build_results: list[dict[str, Any]] = []
    if not args.skip_build:
        for index, (name, command, cwd) in enumerate(build_steps(), start=1):
            result = run_command(command, cwd)
            result["name"] = name
            build_results.append(result)
            write_text(output_dir / f"{index:02d}_{name}.log", result["output"])
            if result["returncode"] != 0:
                print(result["output"], end="")
                raise RuntimeError(f"Build step '{name}' failed with exit code {result['returncode']}")

    run_env = os.environ.copy()
    run_env["DYLD_LIBRARY_PATH"] = str(ROOT / "src" / "build")

    backend_summaries: list[dict[str, Any]] = []
    warnings: list[str] = []
    reference_semantic_signature: dict[str, Any] | None = None

    for backend_index, backend in enumerate(backends, start=len(build_results) + 1):
        measured_runs: list[dict[str, Any]] = []
        backend_reference_signature: dict[str, Any] | None = None

        for warmup_number in range(1, args.warmups + 1):
            log_path = output_dir / f"{backend_index:02d}_{backend}_warmup{warmup_number}.log"
            command = [
                "python3",
                str(runner_path),
                "--hand-file",
                args.hand_file,
                "--depth",
                str(args.depth),
                "--max-boards",
                str(args.max_boards),
                "--skip-boards",
                args.skip_boards,
                "--parallel",
                args.parallel,
                "--worker-backend",
                backend,
                "--board-workers",
                str(args.board_workers),
                "--root-workers",
                str(args.root_workers),
                "--dds-thread-id",
                str(args.dds_thread_id),
                "--checkpoint-seconds",
                "0",
                "--heartbeat-seconds",
                "1",
                "--log-path",
                str(log_path),
            ]
            result = run_command(command, ROOT, run_env)
            write_text(output_dir / f"{backend_index:02d}_{backend}_warmup{warmup_number}.runner.log", result["output"])
            if result["returncode"] != 0:
                print(result["output"], end="")
                raise RuntimeError(
                    f"backend {backend} warmup {warmup_number} failed with exit code {result['returncode']}"
                )

        for run_number in range(1, args.repeats + 1):
            log_path = output_dir / f"{backend_index:02d}_{backend}_run{run_number}.log"
            command = [
                "python3",
                str(runner_path),
                "--hand-file",
                args.hand_file,
                "--depth",
                str(args.depth),
                "--max-boards",
                str(args.max_boards),
                "--skip-boards",
                args.skip_boards,
                "--parallel",
                args.parallel,
                "--worker-backend",
                backend,
                "--board-workers",
                str(args.board_workers),
                "--root-workers",
                str(args.root_workers),
                "--dds-thread-id",
                str(args.dds_thread_id),
                "--checkpoint-seconds",
                "0",
                "--heartbeat-seconds",
                "1",
                "--log-path",
                str(log_path),
            ]
            result = run_command(command, ROOT, run_env)
            write_text(output_dir / f"{backend_index:02d}_{backend}_run{run_number}.runner.log", result["output"])
            if result["returncode"] != 0:
                print(result["output"], end="")
                raise RuntimeError(f"backend {backend} run {run_number} failed with exit code {result['returncode']}")

            status_path = Path(f"{log_path}.status.json")
            status = json.loads(status_path.read_text(encoding="utf-8"))
            signature = validate_status(
                status,
                expected_backend=backend,
                expected_parallel=args.parallel,
                expected_board_workers=args.board_workers,
                expected_root_workers=args.root_workers,
            )
            if backend_reference_signature is None:
                backend_reference_signature = signature
            elif signature != backend_reference_signature:
                raise RuntimeError(f"backend {backend} semantic signature drifted between measured runs")

            measured_runs.append(
                {
                    "run_number": run_number,
                    "runner": {
                        "returncode": result["returncode"],
                        "elapsed_seconds": result["elapsed_seconds"],
                        "command": command,
                    },
                    "status": status,
                    "summary_fields": parse_summary_fields(status["benchmark_summary_line"]),
                    "semantic_signature": signature,
                }
            )

        assert backend_reference_signature is not None
        backend_comparison_signature = backend_semantic_signature(backend_reference_signature)
        if reference_semantic_signature is None:
            reference_semantic_signature = backend_comparison_signature
        elif backend_comparison_signature != reference_semantic_signature:
            raise RuntimeError(f"backend {backend} does not match the reference backend semantic signature")

        total_seconds = [float(run["summary_fields"]["total_seconds"]) for run in measured_runs]
        per_board_seconds = [float(run["summary_fields"]["per_board_seconds"]) for run in measured_runs]
        timing_summary = {
            "total_seconds": summarize_series(total_seconds),
            "per_board_seconds": summarize_series(per_board_seconds),
        }

        if timing_summary["total_seconds"]["cv"] > args.warn_cv:
            warnings.append(
                f"backend {backend} total_seconds coefficient of variation {timing_summary['total_seconds']['cv']:.4f} exceeded warn threshold {args.warn_cv:.4f}"
            )

        backend_summaries.append(
            {
                "backend": backend,
                "timing": timing_summary,
                "semantics_stable": True,
                "semantic_signature": backend_reference_signature,
                "runs": measured_runs,
            }
        )

    summary = {
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "repository": str(ROOT),
        "git_commit": git_text("rev-parse", "HEAD"),
        "git_dirty": git_is_dirty(),
        "platform": platform.platform(),
        "python": sys.version.split()[0],
        "hand_file": args.hand_file,
        "depth": args.depth,
        "max_boards": args.max_boards,
        "skip_boards": args.skip_boards,
        "parallel": args.parallel,
        "board_workers": args.board_workers,
        "root_workers": args.root_workers,
        "dds_thread_id": args.dds_thread_id,
        "warmups": args.warmups,
        "repeats": args.repeats,
        "warn_cv": args.warn_cv,
        "build_steps": [
            {
                "name": step["name"],
                "command": shlex.join(step["command"]),
                "cwd": step["cwd"],
                "returncode": step["returncode"],
                "elapsed_seconds": step["elapsed_seconds"],
            }
            for step in build_results
        ],
        "backends": backend_summaries,
        "cross_backend_semantics_match": True,
        "warnings": warnings,
    }

    write_text(output_dir / "manifest.json", json.dumps({
        "hand_file": args.hand_file,
        "depth": args.depth,
        "max_boards": args.max_boards,
        "skip_boards": args.skip_boards,
        "parallel": args.parallel,
        "board_workers": args.board_workers,
        "root_workers": args.root_workers,
        "dds_thread_id": args.dds_thread_id,
        "warmups": args.warmups,
        "repeats": args.repeats,
        "backends": backends,
        "warn_cv": args.warn_cv,
    }, indent=2, sort_keys=True))
    write_text(output_dir / "summary.json", json.dumps(summary, indent=2, sort_keys=True))
    write_text(output_dir / "summary.md", markdown_summary(summary))

    print(f"Output directory: {output_dir}")
    for backend in backend_summaries:
        print(
            f"{backend['backend']}: median_total={backend['timing']['total_seconds']['median']:.6f}s "
            f"median_per_board={backend['timing']['per_board_seconds']['median']:.6f}s "
            f"cv={backend['timing']['total_seconds']['cv']:.4f}"
        )
    if warnings:
        print("Warnings:")
        for warning in warnings:
            print(f"- {warning}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

