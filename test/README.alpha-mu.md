# Alpha-Mu Benchmark Runner

This runner executes the current alpha-mu measurement cycle against the DDS test harnesses.

It works with the compile-time-gated root instrumentation in `src/SolverIF.cpp` enabled by `DDS_ALPHA_MU_STATS`.

## Files

- `test/alpha_mu_benchmark.py`

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

