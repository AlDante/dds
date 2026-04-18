#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEST_DIR = ROOT / "test"
BINARY = TEST_DIR / "build" / "alpha_mu_prototype"
DEFAULT_LOG = TEST_DIR / "build" / "alpha_mu_prototype_full_suite.log"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the full default alpha_mu_prototype suite and tee output to a log file."
    )
    parser.add_argument("--log-path", default=str(DEFAULT_LOG))
    parser.add_argument("--timeout-seconds", type=float, default=0.0)
    parser.add_argument("--no-build", action="store_true")
    args = parser.parse_args()

    log_path = Path(args.log_path).resolve()
    log_path.parent.mkdir(parents=True, exist_ok=True)

    if not args.no_build:
        build_cmd = ["make", "-f", "Makefiles/Makefile_Mac_clang", "alpha_mu_prototype"]
        build = subprocess.run(build_cmd, cwd=str(TEST_DIR), text=True)
        if build.returncode != 0:
            return build.returncode

    if not BINARY.exists():
        print(f"Missing binary: {BINARY}", file=sys.stderr)
        return 2

    env = os.environ.copy()
    env["DYLD_LIBRARY_PATH"] = str(ROOT / "src" / "build")

    start_wall = time.strftime("%Y-%m-%d %H:%M:%S")
    start = time.perf_counter()

    with log_path.open("w", encoding="utf-8") as handle:
        handle.write(f"started_at={start_wall}\n")
        handle.write(f"command={BINARY}\n\n")
        handle.flush()

        proc = subprocess.Popen(
            [str(BINARY)],
            cwd=str(TEST_DIR),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )

        assert proc.stdout is not None
        try:
            while True:
                line = proc.stdout.readline()
                if line == "" and proc.poll() is not None:
                    break
                if line:
                    sys.stdout.write(line)
                    sys.stdout.flush()
                    handle.write(line)
                    handle.flush()
                if args.timeout_seconds > 0.0 and (time.perf_counter() - start) > args.timeout_seconds:
                    proc.terminate()
                    try:
                        proc.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait()
                    break
        finally:
            returncode = proc.wait()
            elapsed = time.perf_counter() - start
            handle.write(f"\nfinished_at={time.strftime('%Y-%m-%d %H:%M:%S')}\n")
            handle.write(f"elapsed_seconds={elapsed:.6f}\n")
            handle.write(f"returncode={returncode}\n")
            handle.flush()

    print(f"Log path: {log_path}")
    return returncode


if __name__ == "__main__":
    raise SystemExit(main())

