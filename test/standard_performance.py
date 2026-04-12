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

from performance_log_graph import render_performance_log_graph


DEFAULT_WORKLOADS = [
    {
        "name": "regression_api_smoke",
        "cwd": "test",
        "command": ["./build/regression_api", "../hands/list10.txt", "../hands/thomas1.txt"],
        "requires_dyld": True,
    },
    {
        "name": "dtest_solve_list10",
        "cwd": "test",
        "command": ["./build/dtest", "-f", "../hands/list10.txt", "-s", "solve"],
        "requires_dyld": True,
    },
    {
        "name": "dtest_solve_list100",
        "cwd": "test",
        "command": ["./build/dtest", "-f", "../hands/list100.txt", "-s", "solve"],
        "requires_dyld": True,
    },
    {
        "name": "play_analysis_benchmark",
        "cwd": "test",
        "command": ["./build/play_analysis_benchmark"],
        "requires_dyld": True,
    },
    {
        "name": "alpha_mu_prototype_default",
        "cwd": "test",
        "command": ["./build/alpha_mu_prototype"],
        "requires_dyld": True,
    },
    {
        "name": "alpha_mu_prototype_bridge_dds",
        "cwd": "test",
        "command": ["./build/alpha_mu_prototype", "bridge_dds"],
        "requires_dyld": True,
    },
]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


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


def git_text(root: Path, *args: str) -> str:
    proc = subprocess.run(
        ["git", *args],
        cwd=str(root),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        return "unknown"
    return proc.stdout.strip()


def build_steps(root: Path) -> list[tuple[str, list[str], Path]]:
    return [
        ("build_library", ["make", "macos"], root / "src"),
        (
            "build_test_binaries",
            [
                "make",
                "-f",
                "Makefiles/Makefile_Mac_clang",
                "regression_api",
                "dtest",
                "play_analysis_benchmark",
                "alpha_mu_prototype",
            ],
            root / "test",
        ),
    ]


def summarize_workload_runs(name: str, command_text: str, runs: list[dict[str, Any]]) -> dict[str, Any]:
    elapsed_values = [run["elapsed_seconds"] for run in runs]
    return {
        "name": name,
        "command": command_text,
        "runs": runs,
        "run_count": len(runs),
        "median_elapsed_seconds": statistics.median(elapsed_values),
        "mean_elapsed_seconds": statistics.fmean(elapsed_values),
        "min_elapsed_seconds": min(elapsed_values),
        "max_elapsed_seconds": max(elapsed_values),
    }


def markdown_summary(summary: dict[str, Any]) -> str:
    lines: list[str] = []
    lines.append("# Standard Performance Summary")
    lines.append("")
    lines.append(f"- Generated: {summary['generated_at']}")
    lines.append(f"- Repository: `{summary['repository']}`")
    lines.append(f"- Commit: `{summary['git_commit']}`")
    lines.append(f"- Dirty tree: `{summary['git_dirty']}`")
    lines.append(f"- Platform: `{summary['platform']}`")
    lines.append(f"- Python: `{summary['python']}`")
    lines.append(f"- Repeats per workload: `{summary['repeats']}`")
    lines.append("")
    lines.append("## Build steps")
    lines.append("")
    for step in summary["build_steps"]:
        lines.append(f"- `{step['name']}`: `{step['command']}` → rc `{step['returncode']}` in `{step['elapsed_seconds']:.3f}` s")
    lines.append("")
    lines.append("## Workloads")
    lines.append("")
    lines.append("| Workload | Median (s) | Mean (s) | Min (s) | Max (s) | Runs |")
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: |")
    for workload in summary["workloads"]:
        lines.append(
            f"| `{workload['name']}` | {workload['median_elapsed_seconds']:.3f} | "
            f"{workload['mean_elapsed_seconds']:.3f} | {workload['min_elapsed_seconds']:.3f} | "
            f"{workload['max_elapsed_seconds']:.3f} | {workload['run_count']} |"
        )
    lines.append("")
    lines.append("## Notes")
    lines.append("")
    lines.append("- This suite is intended for routine post-change performance checks after important code modifications.")
    lines.append("- Detailed alpha-mu root instrumentation remains available separately via `test/alpha_mu_benchmark.py`.")
    lines.append("")
    return "\n".join(lines)


def ensure_log_header(path: Path) -> None:
    if path.exists():
        return
    write_text(
        path,
        "# Performance Log\n\n"
        "This file records standardized post-change performance runs from `test/standard_performance.py`.\n\n"
        "Each entry links to a timestamped result bundle under `test/build/performance_runs/`.\n",
    )


def append_log_entry(log_path: Path, summary: dict[str, Any]) -> None:
    ensure_log_header(log_path)
    relative_output_dir = Path(summary["output_dir"]).relative_to(Path(summary["repository"]))

    lines: list[str] = []
    lines.append("")
    lines.append(f"## {summary['generated_at']} — commit `{summary['git_commit']}`{' (dirty)' if summary['git_dirty'] else ''}")
    lines.append("")
    lines.append(f"- Output bundle: `{relative_output_dir}`")
    lines.append(f"- Platform: `{summary['platform']}`")
    lines.append(f"- Repeats per workload: `{summary['repeats']}`")
    lines.append("")
    lines.append("| Workload | Median (s) | Mean (s) | Min (s) | Max (s) |")
    lines.append("| --- | ---: | ---: | ---: | ---: |")
    for workload in summary["workloads"]:
        lines.append(
            f"| `{workload['name']}` | {workload['median_elapsed_seconds']:.3f} | "
            f"{workload['mean_elapsed_seconds']:.3f} | {workload['min_elapsed_seconds']:.3f} | "
            f"{workload['max_elapsed_seconds']:.3f} |"
        )
    lines.append("")
    with log_path.open("a", encoding="utf-8") as handle:
        handle.write("\n".join(lines))


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the standardized DDS performance suite and record the results.")
    parser.add_argument("--repeats", type=int, default=1, help="Number of times to run each workload.")
    parser.add_argument(
        "--output-dir",
        default="",
        help="Optional output directory. Defaults to test/build/performance_runs/<timestamp>/.",
    )
    parser.add_argument("--skip-build", action="store_true", help="Skip rebuilding the library and test binaries.")
    parser.add_argument("--no-log-update", action="store_true", help="Do not append the run to docs/performance-log.md.")
    args = parser.parse_args()

    if args.repeats <= 0:
        raise SystemExit("--repeats must be positive")

    root = repo_root()
    output_dir = (
        Path(args.output_dir).resolve()
        if args.output_dir
        else root / "test" / "build" / "performance_runs" / timestamp_tag()
    )
    output_dir.mkdir(parents=True, exist_ok=True)

    run_env = os.environ.copy()
    run_env["DYLD_LIBRARY_PATH"] = str(root / "src" / "build")

    build_results: list[dict[str, Any]] = []
    if not args.skip_build:
        for index, (name, command, cwd) in enumerate(build_steps(root), start=1):
            result = run_command(command, cwd)
            result["name"] = name
            build_results.append(result)
            write_text(output_dir / f"{index:02d}_{name}.log", result["output"])
            if result["returncode"] != 0:
                print(result["output"], end="")
                raise RuntimeError(f"Build step '{name}' failed with exit code {result['returncode']}")

    workload_summaries: list[dict[str, Any]] = []
    for workload_index, workload in enumerate(DEFAULT_WORKLOADS, start=len(build_results) + 1):
        cwd = root / workload["cwd"]
        env = run_env if workload.get("requires_dyld") else None
        runs: list[dict[str, Any]] = []
        for run_number in range(1, args.repeats + 1):
            result = run_command(workload["command"], cwd, env)
            runs.append(
                {
                    "returncode": result["returncode"],
                    "elapsed_seconds": result["elapsed_seconds"],
                }
            )
            log_name = f"{workload_index:02d}_{workload['name']}_run{run_number}.log"
            write_text(output_dir / log_name, result["output"])
            if result["returncode"] != 0:
                print(result["output"], end="")
                raise RuntimeError(
                    f"Workload '{workload['name']}' failed on run {run_number} with exit code {result['returncode']}"
                )

        workload_summaries.append(
            summarize_workload_runs(workload["name"], shlex.join(workload["command"]), runs)
        )

    summary = {
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "repository": str(root),
        "output_dir": str(output_dir),
        "git_commit": git_text(root, "rev-parse", "--short", "HEAD"),
        "git_dirty": bool(git_text(root, "status", "--short")),
        "platform": platform.platform(),
        "python": sys.version.split()[0],
        "repeats": args.repeats,
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
        "workloads": workload_summaries,
    }

    write_text(output_dir / "summary.json", json.dumps(summary, indent=2, sort_keys=True))
    write_text(output_dir / "summary.md", markdown_summary(summary))

    if not args.no_log_update:
        log_path = root / "docs" / "performance-log.md"
        append_log_entry(log_path, summary)
        render_performance_log_graph(log_path, root / "docs" / "performance-log.svg")

    print(f"Output directory: {output_dir}")
    for workload in workload_summaries:
        print(
            f"{workload['name']}: median={workload['median_elapsed_seconds']:.3f}s, "
            f"min={workload['min_elapsed_seconds']:.3f}s, max={workload['max_elapsed_seconds']:.3f}s"
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
