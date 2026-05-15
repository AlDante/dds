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

DEFAULT_CASES = [
    {
        "name": "play_board1_depth2_w32",
        "hand_file": "../hands/alpha_mu_play.txt",
        "board": 1,
        "depth": 2,
        "max_worlds": 32,
    },
    {
        "name": "play_board3_depth2_w32",
        "hand_file": "../hands/alpha_mu_play.txt",
        "board": 3,
        "depth": 2,
        "max_worlds": 32,
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


def git_is_dirty(root: Path) -> bool:
    return git_text(root, "status", "--short") not in ("", "unknown")


def build_steps(root: Path) -> list[tuple[str, list[str], Path]]:
    return [
        ("build_library", ["make", "macos"], root / "src"),
        (
            "build_alpha_mu",
            ["make", "-f", "Makefiles/Makefile_Mac_clang", "alpha_mu"],
            root / "test",
        ),
    ]


def parse_decision_line(output: str) -> dict[str, Any]:
    for line in output.splitlines():
        if not line.startswith("ALPHA_MU_DECISION "):
            continue
        fields: dict[str, str] = {}
        for token in line.split()[1:]:
            if "=" not in token:
                continue
            key, value = token.split("=", 1)
            fields[key] = value
        return {
            "raw_worlds": int(fields["raw_worlds"]),
            "surviving_worlds": int(fields["surviving_worlds"]),
            "root_vectors": int(fields["root_vectors"]),
            "root_valid_worlds": int(fields["root_valid_worlds"]),
            "root_useful_worlds": int(fields["root_useful_worlds"]),
            "child_count": int(fields["child_count"]),
            "child_vectors_total": int(fields["child_vectors_total"]),
            "world_gen_seconds": float(fields["world_gen_seconds"]),
            "search_seconds": float(fields["search_seconds"]),
            "bridge_search_seconds": float(fields["bridge_search_seconds"]),
            "dds_leaf_seconds": float(fields["dds_leaf_seconds"]),
            "total_seconds": float(fields["total_seconds"]),
            "decision_policy": fields["decision_policy"],
            "chosen_weighted": float(fields["chosen_weighted"]),
        }
    raise ValueError("Expected an ALPHA_MU_DECISION line in alpha_mu solve output")


def summarize_case(case: dict[str, Any], runs: list[dict[str, Any]]) -> dict[str, Any]:
    totals = [run["metrics"]["total_seconds"] for run in runs]
    world_gen = [run["metrics"]["world_gen_seconds"] for run in runs]
    search = [run["metrics"]["search_seconds"] for run in runs]
    bridge = [run["metrics"]["bridge_search_seconds"] for run in runs]
    dds = [run["metrics"]["dds_leaf_seconds"] for run in runs]
    first_metrics = runs[0]["metrics"]
    return {
        "name": case["name"],
        "hand_file": case["hand_file"],
        "board": case["board"],
        "depth": case["depth"],
        "max_worlds": case["max_worlds"],
        "median_total_seconds": statistics.median(totals),
        "mean_total_seconds": statistics.fmean(totals),
        "min_total_seconds": min(totals),
        "max_total_seconds": max(totals),
        "median_world_gen_seconds": statistics.median(world_gen),
        "median_search_seconds": statistics.median(search),
        "median_bridge_search_seconds": statistics.median(bridge),
        "median_dds_leaf_seconds": statistics.median(dds),
        "raw_worlds": first_metrics["raw_worlds"],
        "surviving_worlds": first_metrics["surviving_worlds"],
        "root_vectors": first_metrics["root_vectors"],
        "root_valid_worlds": first_metrics["root_valid_worlds"],
        "root_useful_worlds": first_metrics["root_useful_worlds"],
        "child_count": first_metrics["child_count"],
        "child_vectors_total": first_metrics["child_vectors_total"],
        "decision_policy": first_metrics["decision_policy"],
        "chosen_weighted": first_metrics["chosen_weighted"],
        "runs": runs,
    }


def markdown_summary(summary: dict[str, Any]) -> str:
    lines: list[str] = []
    lines.append("# Full Alpha-Mu Benchmark Summary")
    lines.append("")
    lines.append(f"- Generated: {summary['generated_at']}")
    lines.append(f"- Repository: `{summary['repository']}`")
    lines.append(f"- Commit: `{summary['git_commit']}`")
    lines.append(f"- Dirty tree: `{summary['git_dirty']}`")
    lines.append(f"- Platform: `{summary['platform']}`")
    lines.append(f"- Python: `{summary['python']}`")
    lines.append(f"- Repeats per case: `{summary['repeats']}`")
    lines.append("")
    lines.append("## Build steps")
    lines.append("")
    for step in summary["build_steps"]:
        lines.append(
            f"- `{step['name']}`: `{step['command']}` → rc `{step['returncode']}` in `{step['elapsed_seconds']:.3f}` s"
        )
    lines.append("")
    lines.append("## Frozen case results")
    lines.append("")
    lines.append(
        "| Case | Median total (s) | Mean total (s) | Min (s) | Max (s) | "
        "Median world gen (s) | Median search (s) | Median DDS leaves (s) | "
        "Raw worlds | Surviving worlds | Root vectors |"
    )
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for case in summary["cases"]:
        lines.append(
            f"| `{case['name']}` | {case['median_total_seconds']:.6f} | {case['mean_total_seconds']:.6f} | "
            f"{case['min_total_seconds']:.6f} | {case['max_total_seconds']:.6f} | "
            f"{case['median_world_gen_seconds']:.6f} | {case['median_search_seconds']:.6f} | "
            f"{case['median_dds_leaf_seconds']:.6f} | {case['raw_worlds']} | {case['surviving_worlds']} | {case['root_vectors']} |"
        )
    lines.append("")
    lines.append("## Frozen command set")
    lines.append("")
    for case in summary["cases"]:
        command = [
            "./build/alpha_mu",
            "solve",
            case["hand_file"],
            str(case["board"]),
            str(case["depth"]),
            str(case["max_worlds"]),
        ]
        lines.append(f"- `{case['name']}`: `{shlex.join(command)}`")
    lines.append("")
    lines.append("## Notes")
    lines.append("")
    lines.append("- This suite measures the real alpha-mu `solve` path, not the exact-leaf-only `benchmark_alpha` path.")
    lines.append("- `play_board1_depth2_w32` is the fast continuation sanity case; `play_board3_depth2_w32` is the heavier multi-world reference case.")
    lines.append("- Future algorithmic work on full alpha-mu should compare against these medians before changing the frozen suite.")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the frozen full alpha-mu benchmark suite.")
    parser.add_argument("--repeats", type=int, default=3, help="Measured repeats per case.")
    parser.add_argument(
        "--output-dir",
        default="",
        help="Optional output directory. Defaults to test/build/full_alpha_mu_runs/<timestamp>/.",
    )
    parser.add_argument("--skip-build", action="store_true", help="Skip rebuilding alpha_mu before benchmarking.")
    args = parser.parse_args()

    if args.repeats <= 0:
        raise SystemExit("--repeats must be positive")

    root = repo_root()
    output_dir = (
        Path(args.output_dir).resolve()
        if args.output_dir
        else root / "test" / "build" / "full_alpha_mu_runs" / timestamp_tag()
    )
    output_dir.mkdir(parents=True, exist_ok=True)

    test_dir = root / "test"
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

    case_summaries: list[dict[str, Any]] = []
    for case_index, case in enumerate(DEFAULT_CASES, start=len(build_results) + 1):
        runs: list[dict[str, Any]] = []
        command = [
            "./build/alpha_mu",
            "solve",
            case["hand_file"],
            str(case["board"]),
            str(case["depth"]),
            str(case["max_worlds"]),
        ]
        for run_number in range(1, args.repeats + 1):
            result = run_command(command, test_dir, run_env)
            log_name = f"{case_index:02d}_{case['name']}_run{run_number}.log"
            write_text(output_dir / log_name, result["output"])
            if result["returncode"] != 0:
                print(result["output"], end="")
                raise RuntimeError(
                    f"Case '{case['name']}' failed on run {run_number} with exit code {result['returncode']}"
                )
            metrics = parse_decision_line(result["output"])
            runs.append(
                {
                    "run": run_number,
                    "returncode": result["returncode"],
                    "elapsed_seconds": result["elapsed_seconds"],
                    "metrics": metrics,
                }
            )
        case_summaries.append(summarize_case(case, runs))

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
        "cases": case_summaries,
    }

    write_text(output_dir / "summary.json", json.dumps(summary, indent=2, sort_keys=True))
    write_text(output_dir / "summary.md", markdown_summary(summary))

    print(f"Output directory: {output_dir}")
    for case in case_summaries:
        print(
            f"{case['name']}: median={case['median_total_seconds']:.6f}s, "
            f"min={case['min_total_seconds']:.6f}s, max={case['max_total_seconds']:.6f}s"
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())

