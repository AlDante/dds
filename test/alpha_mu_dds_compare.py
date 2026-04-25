#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shlex
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

BENCHMARK_RE = re.compile(
    r"ALPHA_MU_BENCHMARK method=(?P<method>\S+) file=(?P<file>\S+) boards=(?P<boards>\d+) "
    r"depth=(?P<depth>\d+) total_seconds=(?P<total>[0-9]+(?:\.[0-9]+)?) "
    r"per_board_seconds=(?P<per_board>[0-9]+(?:\.[0-9]+)?) mismatches=(?P<mismatches>\d+)"
)

DEFAULT_WORKLOADS = [
    {"name": "alpha_mu_play", "hand_file": "hands/alpha_mu_play.txt", "max_boards": 0},
    {"name": "thomas1", "hand_file": "hands/thomas1.txt", "max_boards": 0},
    {"name": "thomas2", "hand_file": "hands/thomas2.txt", "max_boards": 0},
    {"name": "list10", "hand_file": "hands/list10.txt", "max_boards": 0},
    {"name": "list100", "hand_file": "hands/list100.txt", "max_boards": 0},
]

FULL_WORKLOADS = [
    {"name": "list1000", "hand_file": "hands/list1000.txt", "max_boards": 0},
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


def build_steps(root: Path) -> list[tuple[str, list[str], Path]]:
    return [
        ("build_library", ["make", "macos"], root / "src"),
        (
            "build_alpha_mu",
            ["make", "-f", "Makefiles/Makefile_Mac_clang", "alpha_mu"],
            root / "test",
        ),
    ]


def dds_command(workload: dict[str, Any]) -> list[str]:
    return [
        "./build/alpha_mu",
        "benchmark_dds",
        workload["hand_file"],
        str(int(workload.get("max_boards", 0))),
    ]


def alpha_command(workload: dict[str, Any], depth: int) -> list[str]:
    return [
        "./build/alpha_mu",
        "benchmark_alpha",
        workload["hand_file"],
        str(depth),
        str(int(workload.get("max_boards", 0))),
    ]


def parse_benchmark_output(output: str) -> dict[str, Any]:
    match = BENCHMARK_RE.search(output)
    if not match:
        raise RuntimeError("Benchmark output did not include the expected ALPHA_MU_BENCHMARK line")

    return {
        "method": match.group("method"),
        "hand_file": match.group("file"),
        "boards": int(match.group("boards")),
        "depth": int(match.group("depth")),
        "total_seconds": float(match.group("total")),
        "per_board_seconds": float(match.group("per_board")),
        "mismatches": int(match.group("mismatches")),
    }


def summarize_series(values: list[float]) -> dict[str, float]:
    return {
        "median": statistics.median(values),
        "mean": statistics.fmean(values),
        "min": min(values),
        "max": max(values),
    }


def summarize_workload(name: str, runs: list[dict[str, Any]]) -> dict[str, Any]:
    dds_totals = [run["dds"]["total_seconds"] for run in runs]
    dds_per_board = [run["dds"]["per_board_seconds"] for run in runs]
    depth_values: dict[int, dict[str, list[float]]] = {}
    for run in runs:
        for depth_entry in run["alpha"]:
            bucket = depth_values.setdefault(
                depth_entry["depth"],
                {"total_seconds": [], "per_board_seconds": [], "ratio_vs_dds": [], "mismatches": []},
            )
            bucket["total_seconds"].append(depth_entry["total_seconds"])
            bucket["per_board_seconds"].append(depth_entry["per_board_seconds"])
            bucket["ratio_vs_dds"].append(depth_entry["ratio_vs_dds"])
            bucket["mismatches"].append(float(depth_entry["mismatches"]))

    return {
        "name": name,
        "hand_file": runs[0]["dds"]["hand_file"],
        "boards": runs[0]["dds"]["boards"],
        "run_count": len(runs),
        "dds": {
            "total_seconds": summarize_series(dds_totals),
            "per_board_seconds": summarize_series(dds_per_board),
        },
        "depths": [
            {
                "depth": depth,
                "total_seconds": summarize_series(values["total_seconds"]),
                "per_board_seconds": summarize_series(values["per_board_seconds"]),
                "ratio_vs_dds": summarize_series(values["ratio_vs_dds"]),
                "mismatches": int(max(values["mismatches"])),
            }
            for depth, values in sorted(depth_values.items())
        ],
        "runs": runs,
    }


def markdown_summary(summary: dict[str, Any]) -> str:
    lines: list[str] = []
    lines.append("# DDS vs Alpha-Mu Comparison Summary")
    lines.append("")
    lines.append(f"- Generated: {summary['generated_at']}")
    lines.append(f"- Repository: `{summary['repository']}`")
    lines.append(f"- Platform: `{summary['platform']}`")
    lines.append(f"- Python: `{summary['python']}`")
    lines.append(f"- Max alpha-mu search depth: `{summary['max_depth']}`")
    lines.append(f"- Warmup runs per workload: `{summary['warmups']}`")
    lines.append(f"- Measured repeats per workload: `{summary['repeats']}`")
    lines.append("")
    lines.append("## Build steps")
    lines.append("")
    for step in summary["build_steps"]:
        lines.append(
            f"- `{step['name']}`: `{step['command']}` → rc `{step['returncode']}` in `{step['elapsed_seconds']:.3f}` s"
        )
    lines.append("")
    lines.append("## Workload medians")
    lines.append("")
    lines.append("| Workload | Boards | Method | Median total (s) | Median per board (ms) | Ratio vs DDS | Mismatches |")
    lines.append("| --- | ---: | --- | ---: | ---: | ---: | ---: |")
    for workload in summary["workloads"]:
        lines.append(
            f"| `{workload['name']}` | {workload['boards']} | `dds` | "
            f"{workload['dds']['total_seconds']['median']:.6f} | "
            f"{workload['dds']['per_board_seconds']['median'] * 1000.0:.3f} | 1.000 | 0 |"
        )
        for depth in workload["depths"]:
            lines.append(
                f"| `{workload['name']}` | {workload['boards']} | `alpha_mu_depth_{depth['depth']}` | "
                f"{depth['total_seconds']['median']:.6f} | "
                f"{depth['per_board_seconds']['median'] * 1000.0:.3f} | "
                f"{depth['ratio_vs_dds']['median']:.3f} | {depth['mismatches']} |"
            )
    lines.append("")
    lines.append("## Notes")
    lines.append("")
    lines.append("- This compares exact one-world DDS solves with exact one-world alpha-mu solves on the same boards.")
    lines.append("- DDS and alpha-mu are benchmarked in separate process invocations to avoid cross-method cache reuse skew.")
    lines.append("- Alpha-mu depth `0` is a direct DDS-backed leaf benchmark with front-handling overhead.")
    lines.append("- Alpha-mu depths `>= 1` search that many full tricks before handing the remainder to DDS, while still checking for exact score agreement on every board.")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare original DDS solve speed against one-world alpha-mu solve speed."
    )
    parser.add_argument("--repeats", type=int, default=3, help="Measured repeats per workload.")
    parser.add_argument("--warmups", type=int, default=1, help="Unmeasured warmup runs per workload.")
    parser.add_argument("--max-depth", type=int, default=1, help="Maximum alpha-mu search depth to compare.")
    parser.add_argument("--full", action="store_true", help="Add heavier workloads such as hands/list1000.txt.")
    parser.add_argument(
        "--workload",
        action="append",
        default=[],
        help="Optional workload name filter. Repeat to keep more than one workload.",
    )
    parser.add_argument("--skip-build", action="store_true", help="Skip rebuilding the library and alpha_mu binary.")
    parser.add_argument(
        "--output-dir",
        default="",
        help="Optional output directory. Defaults to test/build/alpha_mu_dds_compare/<timestamp>/.",
    )
    args = parser.parse_args()

    if args.repeats <= 0:
        raise SystemExit("--repeats must be positive")
    if args.warmups < 0:
        raise SystemExit("--warmups must be non-negative")
    if args.max_depth < 0:
        raise SystemExit("--max-depth must be non-negative")

    root = repo_root()
    output_dir = (
        Path(args.output_dir).resolve()
        if args.output_dir
        else root / "test" / "build" / "alpha_mu_dds_compare" / timestamp_tag()
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

    workloads = list(DEFAULT_WORKLOADS)
    if args.full:
        workloads.extend(FULL_WORKLOADS)
    if args.workload:
        allowed = set(args.workload)
        workloads = [workload for workload in workloads if workload["name"] in allowed]
        if not workloads:
            raise SystemExit("--workload did not match any known workload name")

    workload_summaries: list[dict[str, Any]] = []
    for workload_index, workload in enumerate(workloads, start=len(build_results) + 1):
        runs: list[dict[str, Any]] = []
        for warmup_number in range(1, args.warmups + 1):
            dds_warmup = run_command(dds_command(workload), root / "test", run_env)
            write_text(
                output_dir / f"{workload_index:02d}_{workload['name']}_dds_warmup{warmup_number}.log",
                dds_warmup["output"],
            )
            if dds_warmup["returncode"] != 0:
                print(dds_warmup["output"], end="")
                raise RuntimeError(
                    f"Workload '{workload['name']}' DDS warmup {warmup_number} failed with exit code {dds_warmup['returncode']}"
                )

            for depth in range(args.max_depth + 1):
                alpha_warmup = run_command(alpha_command(workload, depth), root / "test", run_env)
                write_text(
                    output_dir / f"{workload_index:02d}_{workload['name']}_alpha_depth{depth}_warmup{warmup_number}.log",
                    alpha_warmup["output"],
                )
                if alpha_warmup["returncode"] != 0:
                    print(alpha_warmup["output"], end="")
                    raise RuntimeError(
                        f"Workload '{workload['name']}' alpha depth {depth} warmup {warmup_number} failed with exit code {alpha_warmup['returncode']}"
                    )

        for run_number in range(1, args.repeats + 1):
            dds_result = run_command(dds_command(workload), root / "test", run_env)
            write_text(output_dir / f"{workload_index:02d}_{workload['name']}_dds_run{run_number}.log", dds_result["output"])
            if dds_result["returncode"] != 0:
                print(dds_result["output"], end="")
                raise RuntimeError(
                    f"Workload '{workload['name']}' DDS run {run_number} failed with exit code {dds_result['returncode']}"
                )

            dds_parsed = parse_benchmark_output(dds_result["output"])
            alpha_runs = []
            for depth in range(args.max_depth + 1):
                alpha_result = run_command(alpha_command(workload, depth), root / "test", run_env)
                write_text(
                    output_dir / f"{workload_index:02d}_{workload['name']}_alpha_depth{depth}_run{run_number}.log",
                    alpha_result["output"],
                )
                if alpha_result["returncode"] != 0:
                    print(alpha_result["output"], end="")
                    raise RuntimeError(
                        f"Workload '{workload['name']}' alpha depth {depth} run {run_number} failed with exit code {alpha_result['returncode']}"
                    )

                alpha_parsed = parse_benchmark_output(alpha_result["output"])
                alpha_parsed["ratio_vs_dds"] = (
                    0.0
                    if dds_parsed["total_seconds"] <= 0.0
                    else alpha_parsed["total_seconds"] / dds_parsed["total_seconds"]
                )
                alpha_runs.append(alpha_parsed)

            runs.append({"run_number": run_number, "dds": dds_parsed, "alpha": alpha_runs})

        workload_summaries.append(summarize_workload(workload["name"], runs))

    summary = {
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "repository": str(root),
        "platform": platform.platform(),
        "python": sys.version.split()[0],
        "max_depth": args.max_depth,
        "warmups": args.warmups,
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

    print(f"Output directory: {output_dir}")
    for workload in workload_summaries:
        print(f"{workload['name']}: dds={workload['dds']['total_seconds']['median']:.6f}s")
        for depth in workload["depths"]:
            print(
                f"{workload['name']}: alpha_mu_depth_{depth['depth']}="
                f"{depth['total_seconds']['median']:.6f}s ratio={depth['ratio_vs_dds']['median']:.3f}"
            )

    return 0


if __name__ == "__main__":
    sys.exit(main())

