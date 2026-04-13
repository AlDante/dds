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


SHORT_WORKLOAD_POLICY = {
    "warmup_runs": 1,
    "minimum_repeats": 3,
    "minimum_measured_seconds": 1.0,
    "maximum_repeats": 10,
}

TIMING_GOAL_TEXT = (
    "short workloads are measured with enough warmup and repeats to make the reported medians "
    "stable to about 0.1 s or better."
)


def workload_definition(
    name: str,
    command: list[str],
    *,
    cwd: str = "test",
    requires_dyld: bool = True,
    short_workload: bool = False,
) -> dict[str, Any]:
    definition = {
        "name": name,
        "cwd": cwd,
        "command": command,
        "requires_dyld": requires_dyld,
    }
    if short_workload:
        definition.update(SHORT_WORKLOAD_POLICY)
    return definition


DEFAULT_WORKLOADS = [
    workload_definition(
        "regression_api_smoke",
        ["./build/regression_api", "../hands/list10.txt", "../hands/thomas1.txt"],
    ),
    workload_definition(
        "dtest_solve_list10",
        ["./build/dtest", "-f", "../hands/list10.txt", "-s", "solve"],
        short_workload=True,
    ),
    workload_definition(
        "dtest_solve_list100",
        ["./build/dtest", "-f", "../hands/list100.txt", "-s", "solve"],
        short_workload=True,
    ),
    workload_definition(
        "play_analysis_benchmark",
        ["./build/play_analysis_benchmark"],
        short_workload=True,
    ),
    workload_definition(
        "alpha_mu_prototype_default",
        ["./build/alpha_mu_prototype"],
        short_workload=True,
    ),
    workload_definition(
        "alpha_mu_prototype_bridge_dds",
        ["./build/alpha_mu_prototype", "bridge_dds"],
        short_workload=True,
    ),
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


def git_is_dirty(root: Path) -> bool:
    return git_text(root, "status", "--short") not in ("", "unknown")


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


def actual_repeats_for_workload(workload: dict[str, Any], requested_repeats: int) -> int:
    return max(requested_repeats, int(workload.get("minimum_repeats", 1)))


def warmup_runs_for_workload(workload: dict[str, Any]) -> int:
    return int(workload.get("warmup_runs", 0))


def minimum_measured_seconds_for_workload(workload: dict[str, Any]) -> float:
    return float(workload.get("minimum_measured_seconds", 0.0))


def maximum_repeats_for_workload(workload: dict[str, Any], requested_repeats: int) -> int:
    configured_max = int(workload.get("maximum_repeats", requested_repeats))
    return max(configured_max, requested_repeats)


def timing_note_for_workload(
    workload: dict[str, Any], requested_repeats: int, measured_repeats: int, warmup_runs: int
) -> str:
    minimum_repeats = int(workload.get("minimum_repeats", 1))
    minimum_measured_seconds = minimum_measured_seconds_for_workload(workload)
    maximum_repeats = int(workload.get("maximum_repeats", measured_repeats))

    if warmup_runs == 0 and minimum_repeats <= requested_repeats and minimum_measured_seconds <= 0.0:
        return ""

    pieces: list[str] = []
    if warmup_runs:
        pieces.append(f"{warmup_runs} unmeasured warmup run{'s' if warmup_runs != 1 else ''}")
    pieces.append(f"{measured_repeats} measured repeat{'s' if measured_repeats != 1 else ''}")
    if minimum_repeats > requested_repeats:
        pieces.append(f"minimum measured repeat count raised from requested {requested_repeats} to {minimum_repeats}")
    if minimum_measured_seconds > 0.0:
        pieces.append(f"cumulative measured wall time target ≥ {minimum_measured_seconds:.1f} s")
    if maximum_repeats > max(requested_repeats, minimum_repeats) and minimum_measured_seconds > 0.0:
        pieces.append(f"automatic repeats capped at {maximum_repeats} unless the user requests more")
    return "; ".join(pieces) + "."


def timing_notes(summary: dict[str, Any]) -> list[str]:
    notes: list[str] = []
    for workload in summary["workloads"]:
        note = workload.get("timing_note")
        if note:
            notes.append(f"`{workload['name']}`: {note}")
    return notes


def markdown_workload_row(workload: dict[str, Any], include_run_count: bool = False) -> str:
    row = (
        f"| `{workload['name']}` | {workload['median_elapsed_seconds']:.3f} | "
        f"{workload['mean_elapsed_seconds']:.3f} | {workload['min_elapsed_seconds']:.3f} | "
        f"{workload['max_elapsed_seconds']:.3f} |"
    )
    if include_run_count:
        return row[:-1] + f" {workload['run_count']} |"
    return row


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
    lines.append(f"- Requested repeats per workload: `{summary['repeats']}`")
    lines.append(f"- Timing goal: {TIMING_GOAL_TEXT}")
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
        lines.append(markdown_workload_row(workload, include_run_count=True))
    lines.append("")
    note_lines = timing_notes(summary)
    if note_lines:
        lines.append("## Timing stabilization notes")
        lines.append("")
        for note in note_lines:
            lines.append(f"- {note}")
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
    lines.append(f"- Requested repeats per workload: `{summary['repeats']}`")
    note_lines = timing_notes(summary)
    if note_lines:
        lines.append("- Timing stabilization:")
        for note in note_lines:
            lines.append(f"  - {note}")
    lines.append("")
    lines.append("| Workload | Median (s) | Mean (s) | Min (s) | Max (s) |")
    lines.append("| --- | ---: | ---: | ---: | ---: |")
    for workload in summary["workloads"]:
        lines.append(markdown_workload_row(workload))
    lines.append("")
    with log_path.open("a", encoding="utf-8") as handle:
        handle.write("\n".join(lines))


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the standardized DDS performance suite and record the results.")
    parser.add_argument(
        "--repeats",
        type=int,
        default=1,
        help="Requested number of measured runs per workload; some short workloads may use more repeats for timing stability.",
    )
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
        warmup_runs = warmup_runs_for_workload(workload)
        for warmup_number in range(1, warmup_runs + 1):
            warmup_result = run_command(workload["command"], cwd, env)
            log_name = f"{workload_index:02d}_{workload['name']}_warmup{warmup_number}.log"
            write_text(output_dir / log_name, warmup_result["output"])
            if warmup_result["returncode"] != 0:
                print(warmup_result["output"], end="")
                raise RuntimeError(
                    f"Workload '{workload['name']}' warmup {warmup_number} failed with exit code {warmup_result['returncode']}"
                )

        measured_repeats = actual_repeats_for_workload(workload, args.repeats)
        minimum_measured_seconds = minimum_measured_seconds_for_workload(workload)
        maximum_repeats = maximum_repeats_for_workload(workload, args.repeats)
        runs: list[dict[str, Any]] = []
        measured_elapsed_seconds = 0.0
        run_number = 0
        while run_number < measured_repeats or (
            minimum_measured_seconds > 0.0
            and measured_elapsed_seconds < minimum_measured_seconds
            and run_number < maximum_repeats
        ):
            run_number += 1
            result = run_command(workload["command"], cwd, env)
            runs.append(
                {
                    "returncode": result["returncode"],
                    "elapsed_seconds": result["elapsed_seconds"],
                }
            )
            measured_elapsed_seconds += result["elapsed_seconds"]
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
        workload_summaries[-1]["warmup_runs"] = warmup_runs
        workload_summaries[-1]["timing_note"] = timing_note_for_workload(
            workload, args.repeats, workload_summaries[-1]["run_count"], warmup_runs
        )

    summary = {
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "repository": str(root),
        "output_dir": str(output_dir),
        "git_commit": git_text(root, "rev-parse", "--short", "HEAD"),
        "git_dirty": git_is_dirty(root),
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
