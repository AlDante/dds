#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

ROOT_LINE_RE = re.compile(
    r"ALPHA_MU root context=(?P<context>\S+) "
    r"probes=(?P<probes>\d+) "
    r"initial_guess=(?P<initial_guess>-?\d+) "
    r"initial_bounds=\[(?P<lower>-?\d+),(?P<upper>-?\d+)\] "
    r"final_score=(?P<final_score>-?\d+) "
    r"guess_relation=(?P<guess_relation>\S+)"
)

AVG_USER_TIME_RE = re.compile(r"Avg user time \(ms\)\s+(?P<value>[0-9]+(?:\.[0-9]+)?)")
REGRESSION_OK_RE = re.compile(r"regression_api: OK")


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
]

FULL_WORKLOADS = [
    {
        "name": "regression_api_thomas2",
        "cwd": "test",
        "command": ["./build/regression_api", "../hands/thomas2.txt"],
        "requires_dyld": True,
    },
    {
        "name": "dtest_solve_list1000",
        "cwd": "test",
        "command": ["./build/dtest", "-f", "../hands/list1000.txt", "-s", "solve"],
        "requires_dyld": True,
    },
]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def timestamp_tag() -> str:
    return time.strftime("%Y%m%d-%H%M%S")


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


def parse_root_lines(output: str) -> list[dict[str, Any]]:
    entries: list[dict[str, Any]] = []
    for match in ROOT_LINE_RE.finditer(output):
        entries.append(
            {
                "context": match.group("context"),
                "probes": int(match.group("probes")),
                "initial_guess": int(match.group("initial_guess")),
                "initial_lowerbound": int(match.group("lower")),
                "initial_upperbound": int(match.group("upper")),
                "final_score": int(match.group("final_score")),
                "guess_relation": match.group("guess_relation"),
            }
        )
    return entries


def average(values: list[float]) -> float:
    return sum(values) / len(values) if values else 0.0


def aggregate_root_stats(entries: list[dict[str, Any]]) -> dict[str, Any]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for entry in entries:
        grouped[entry["context"]].append(entry)

    summary: dict[str, Any] = {}
    for context, context_entries in grouped.items():
        probe_values = [entry["probes"] for entry in context_entries]
        guess_values = [entry["initial_guess"] for entry in context_entries]
        score_values = [entry["final_score"] for entry in context_entries]
        relation_counts = Counter(entry["guess_relation"] for entry in context_entries)
        score_counts = Counter(entry["final_score"] for entry in context_entries)

        summary[context] = {
            "count": len(context_entries),
            "avg_probes": average([float(v) for v in probe_values]),
            "max_probes": max(probe_values),
            "min_probes": min(probe_values),
            "avg_initial_guess": average([float(v) for v in guess_values]),
            "avg_final_score": average([float(v) for v in score_values]),
            "guess_relation_counts": dict(sorted(relation_counts.items())),
            "final_score_counts": dict(sorted(score_counts.items())),
        }

    return summary


def extract_avg_user_time(output: str) -> float | None:
    match = AVG_USER_TIME_RE.search(output)
    if not match:
        return None
    return float(match.group("value"))


def build_commands(root: Path, restore_normal_build: bool) -> list[tuple[str, list[str], Path, dict[str, str] | None]]:
    src_dir = root / "src"
    test_dir = root / "test"
    return [
        (
            "build_instrumented_library",
            ["make", "clean"],
            src_dir,
            None,
        ),
        (
            "build_instrumented_library",
            ["make", "macos", "DDS_BEHAVIOR=-DDDS_ALPHA_MU_STATS"],
            src_dir,
            None,
        ),
        (
            "build_test_binaries",
            [
                "make",
                "-f",
                "Makefiles/Makefile_Mac_clang",
                "regression_api",
                "dtest",
                "play_analysis_benchmark",
            ],
            test_dir,
            None,
        ),
    ] + ([] if not restore_normal_build else [])


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def markdown_summary(summary: dict[str, Any]) -> str:
    lines: list[str] = []
    lines.append("# Alpha-Mu Benchmark Summary")
    lines.append("")
    lines.append(f"- Generated: {summary['generated_at']}")
    lines.append(f"- Repository: `{summary['repository']}`")
    lines.append(f"- Instrumentation macro: `DDS_ALPHA_MU_STATS`")
    lines.append("")
    lines.append("## Workloads")
    lines.append("")
    for workload in summary["workloads"]:
        lines.append(f"### {workload['name']}")
        lines.append("")
        lines.append(f"- Command: `{workload['command']}`")
        lines.append(f"- Return code: `{workload['returncode']}`")
        lines.append(f"- Elapsed seconds: `{workload['elapsed_seconds']:.3f}`")
        if workload.get("avg_user_time_ms") is not None:
            lines.append(f"- Avg user time (ms): `{workload['avg_user_time_ms']:.2f}`")
        if workload.get("regression_ok") is not None:
            lines.append(f"- Regression OK: `{workload['regression_ok']}`")
        lines.append(f"- Root stat lines: `{workload['root_line_count']}`")
        lines.append("")

    lines.append("## Aggregated root-search stats")
    lines.append("")
    if not summary["root_stats_by_context"]:
        lines.append("No alpha-mu root lines were captured.")
        lines.append("")
    else:
        for context, context_summary in sorted(summary["root_stats_by_context"].items()):
            lines.append(f"### {context}")
            lines.append("")
            lines.append(f"- Count: `{context_summary['count']}`")
            lines.append(f"- Avg probes: `{context_summary['avg_probes']:.2f}`")
            lines.append(f"- Min probes: `{context_summary['min_probes']}`")
            lines.append(f"- Max probes: `{context_summary['max_probes']}`")
            lines.append(f"- Avg initial guess: `{context_summary['avg_initial_guess']:.2f}`")
            lines.append(f"- Avg final score: `{context_summary['avg_final_score']:.2f}`")
            lines.append(f"- Guess relation counts: `{json.dumps(context_summary['guess_relation_counts'], sort_keys=True)}`")
            lines.append(f"- Final score counts: `{json.dumps(context_summary['final_score_counts'], sort_keys=True)}`")
            lines.append("")

    lines.append("## Notes")
    lines.append("")
    lines.append("- `SolveSameBoard` observations come from repeat-solve paths exercised by the existing harnesses.")
    lines.append("- In the current default mix, `regression_api` is the main source of `SolveBoardInternal` and `SolveSameBoard` measurements, while `dtest` provides throughput timing.")
    lines.append("- `play_analysis_benchmark` is the dedicated source of `AnalyseLaterBoard` measurements.")
    lines.append("- The benchmark runner restores a normal non-instrumented library build by default.")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run alpha-mu root instrumentation benchmarks for DDS.")
    parser.add_argument(
        "--full",
        action="store_true",
        help="Include heavier workloads such as thomas2 and list1000.",
    )
    parser.add_argument(
        "--no-restore",
        action="store_true",
        help="Leave the instrumented library build in place instead of rebuilding the normal library at the end.",
    )
    parser.add_argument(
        "--output-dir",
        default="",
        help="Optional output directory. Defaults to test/build/alpha_mu_stats/<timestamp>/.",
    )
    args = parser.parse_args()

    root = repo_root()
    output_dir = (
        Path(args.output_dir).resolve()
        if args.output_dir
        else root / "test" / "build" / "alpha_mu_stats" / timestamp_tag()
    )
    output_dir.mkdir(parents=True, exist_ok=True)

    restore_normal_build = not args.no_restore
    workloads = list(DEFAULT_WORKLOADS)
    if args.full:
        workloads.extend(FULL_WORKLOADS)

    src_dir = root / "src"
    test_dir = root / "test"
    run_env = os.environ.copy()
    run_env["DYLD_LIBRARY_PATH"] = str(root / "src" / "build")

    steps: list[dict[str, Any]] = []
    all_root_entries: list[dict[str, Any]] = []

    try:
        build_step_defs = build_commands(root, restore_normal_build)
        for index, (name, command, cwd, env) in enumerate(build_step_defs, start=1):
            result = run_command(command, cwd, env)
            result["name"] = name
            steps.append(result)
            log_name = f"{index:02d}_{name}.log"
            write_text(output_dir / log_name, result["output"])
            if result["returncode"] != 0:
                print(result["output"], end="")
                raise RuntimeError(f"Step '{name}' failed with exit code {result['returncode']}")

        for offset, workload in enumerate(workloads, start=len(steps) + 1):
            cwd = root / workload["cwd"]
            env = run_env if workload.get("requires_dyld") else None
            result = run_command(workload["command"], cwd, env)
            root_entries = parse_root_lines(result["output"])
            all_root_entries.extend(root_entries)

            workload_summary = {
                "name": workload["name"],
                "command": shlex.join(workload["command"]),
                "cwd": str(cwd),
                "returncode": result["returncode"],
                "elapsed_seconds": result["elapsed_seconds"],
                "avg_user_time_ms": extract_avg_user_time(result["output"]),
                "regression_ok": bool(REGRESSION_OK_RE.search(result["output"])) if "regression_api" in workload["name"] else None,
                "root_line_count": len(root_entries),
            }
            steps.append({**result, **workload_summary})
            log_name = f"{offset:02d}_{workload['name']}.log"
            write_text(output_dir / log_name, result["output"])
            if result["returncode"] != 0:
                print(result["output"], end="")
                raise RuntimeError(f"Workload '{workload['name']}' failed with exit code {result['returncode']}")
    finally:
        if restore_normal_build:
            restore_clean = run_command(["make", "clean"], src_dir)
            write_text(output_dir / "zz_restore_clean.log", restore_clean["output"])
            restore_build = run_command(["make", "macos"], src_dir)
            write_text(output_dir / "zz_restore_build.log", restore_build["output"])
            if restore_build["returncode"] != 0:
                print(restore_build["output"], end="", file=sys.stderr)
                raise RuntimeError("Failed to restore the normal DDS shared library build")

    command_summaries = []
    for entry in steps:
        if "command" not in entry:
            continue
        if isinstance(entry["command"], list):
            command_text = shlex.join(entry["command"])
        else:
            command_text = entry["command"]
        command_summaries.append(
            {
                "name": entry.get("name", "command"),
                "command": command_text,
                "cwd": entry["cwd"],
                "returncode": entry["returncode"],
                "elapsed_seconds": entry["elapsed_seconds"],
                "avg_user_time_ms": entry.get("avg_user_time_ms"),
                "regression_ok": entry.get("regression_ok"),
                "root_line_count": entry.get("root_line_count", 0),
            }
        )

    summary = {
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "repository": str(root),
        "output_dir": str(output_dir),
        "restore_normal_build": restore_normal_build,
        "workloads": command_summaries,
        "root_stats_by_context": aggregate_root_stats(all_root_entries),
        "root_entry_count": len(all_root_entries),
    }

    write_text(output_dir / "summary.json", json.dumps(summary, indent=2, sort_keys=True))
    write_text(output_dir / "summary.md", markdown_summary(summary))

    print(f"Output directory: {output_dir}")
    print(f"Captured root-stat lines: {len(all_root_entries)}")
    for context, context_summary in sorted(summary["root_stats_by_context"].items()):
        print(
            f"{context}: count={context_summary['count']}, "
            f"avg_probes={context_summary['avg_probes']:.2f}, "
            f"max_probes={context_summary['max_probes']}, "
            f"guess_relations={json.dumps(context_summary['guess_relation_counts'], sort_keys=True)}"
        )

    if "AnalyseLaterBoard" not in summary["root_stats_by_context"]:
        print("AnalyseLaterBoard was not observed in this run; add a dedicated play-analysis workload next.")

    return 0


if __name__ == "__main__":
    sys.exit(main())

