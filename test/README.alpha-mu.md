# Alpha-Mu Benchmark Runner

This runner executes the current alpha-mu measurement cycle against the DDS test harnesses.

It works with the compile-time-gated root instrumentation in `src/SolverIF.cpp` enabled by `DDS_ALPHA_MU_STATS`.

## Files

- `test/alpha_mu_benchmark.py`
- `test/alpha_mu_dds_compare.py`
- `test/run_alpha_mu_benchmark.py`
- `test/alpha_mu.cpp`
- `test/README.alpha-mu-solver.md`

## What the runner does

1. builds an instrumented DDS shared library,
2. ensures `test/build/regression_api` and `test/build/dtest` exist,
3. runs representative workloads,
4. captures raw logs,
5. parses `ALPHA_MU root ...` lines,
6. writes `summary.json` and `summary.md`,
7. restores a normal non-instrumented library build by default.

## Default workloads

- `regression_api` on `hands/list10.txt` and `hands/thomas1.txt`
- `dtest -f ../hands/list10.txt -s solve`
- `dtest -f ../hands/list100.txt -s solve`
- `play_analysis_benchmark`

In the current workload mix:

- `regression_api` is the main source of `SolveBoardInternal` and `SolveSameBoard` measurements,
- `play_analysis_benchmark` is the dedicated source of `AnalyseLaterBoard` measurements,
- the `dtest` runs are still valuable, but they primarily provide throughput timing for representative solve batches.

## Extra workloads

Use `--full` to add:

- `regression_api ../hands/thomas2.txt`
- `dtest -f ../hands/list1000.txt -s solve`

## Output directory

By default the runner writes to:

- `test/build/alpha_mu_stats/<timestamp>/`

## Usage

From the repository root:

```zsh
python3 test/alpha_mu_benchmark.py
```

Full run:

```zsh
python3 test/alpha_mu_benchmark.py --full
```

Compare exact DDS against exact one-world alpha-mu over several hand files:

```zsh
python3 test/alpha_mu_dds_compare.py
```

Focused spot check on one workload:

```zsh
python3 test/alpha_mu_dds_compare.py --workload list10 --max-depth 1
```

Heavier comparison run with a deeper searched alpha-mu prefix:

```zsh
python3 test/alpha_mu_dds_compare.py --full --max-depth 2
```

Keep the instrumented library instead of restoring the normal build:

```zsh
python3 test/alpha_mu_benchmark.py --no-restore
```

Custom output directory:

```zsh
python3 test/alpha_mu_benchmark.py --output-dir /tmp/dds-alpha-mu-run
```

## Notes

- The runner sets `DYLD_LIBRARY_PATH` so the test binaries resolve `../src/build/libdds.so` on macOS.
- If `AnalyseLaterBoard` does not appear in the summary, the next improvement should be to add a dedicated play-analysis workload.
- The workflow is measurement-only; it does not alter deep search semantics.

## Alpha-Mu solver

The repository also now contains a separate alpha-mu solver runner:

- build target: `alpha_mu`
- source: `test/alpha_mu.cpp`

Unlike the benchmark runner, the solver runner is not a DDS root-policy measurement tool. It is a separate semantics-oriented test component for Pareto fronts, toy alpha-mu search, and a DDS-backed leaf-evaluation demo.

The new `test/alpha_mu_dds_compare.py` runner sits between the two: it uses the solver's exact one-world bridge benchmark modes to compare original DDS solve speed against alpha-mu solve speed on the same boards, while also checking score agreement at each measured alpha-mu depth.

For longer single benchmark runs, `test/run_alpha_mu_benchmark.py` streams output live, preserves machine-readable `ALPHA_MU_BENCHMARK_PROGRESS ...`, `ALPHA_MU_BENCHMARK_CHECKPOINT ...`, and `ALPHA_MU_BENCHMARK_BOARD ...` lines in the log, and updates a `.status.json` sidecar so interrupted runs still leave partial progress behind, including the latest in-flight board snapshot, completed board numbers, and per-board timings.

Example depth-2 baseline run from the repository root:

```zsh
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list1.txt --depth 2 --checkpoint-seconds 30
```

Skip specific 1-based board numbers or ranges while keeping checkpointed partial progress:

```zsh
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list10.txt --depth 3 --skip-boards 2 --checkpoint-seconds 60 --heartbeat-seconds 60
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list100.txt --depth 2 --skip-boards 2,5-7 --checkpoint-seconds 30
```

For very long boards, `ALPHA_MU_BENCHMARK_PROGRESS ...` lines report the current board number, elapsed time on that board, recursive call count, DDS leaf-call count, tricks remaining, active worlds, current trick size, and player to move. The sidecar mirrors the latest such snapshot as `current_board_progress`.

