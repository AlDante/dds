# Alpha-Mu Benchmark Runner

This runner executes the current alpha-mu measurement cycle against the DDS test harnesses.

It works with the compile-time-gated root instrumentation in `src/SolverIF.cpp` enabled by `DDS_ALPHA_MU_STATS`.
The `ALPHA_MU root ...` prefix is historical and retained for parser/log compatibility, but the emitter itself is the shared DDS exact-root instrumentation used by `SolveBoardInternal`, `SolveSameBoard`, and `AnalyseLaterBoard`.

## Files

The benchmark/runner entry points remain under `test/`, while the production
alpha-mu engine modules now live under `src/`.

- `test/alpha_mu_benchmark.py`
- `test/alpha_mu_backend_compare.py`
- `test/alpha_mu_dds_compare.py`
- `test/run_alpha_mu_benchmark.py`
- `test/alpha_mu.cpp`
- `test/alpha_mu/api.h`
- `test/alpha_mu/bridge.h`
- `test/alpha_mu/tests.h`
- `test/README.alpha-mu-solver.md`

## What the runner does

1. builds an instrumented DDS shared library,
2. ensures `test/build/regression_api` and `test/build/dtest` exist,
3. runs representative workloads,
4. captures raw logs,
5. parses `ALPHA_MU root ...` lines,
6. aggregates the retained DDS root timing field `ab_us`, the retained coarse AB split `ab_frontend_us`, `ab_iteration_control_us`, and residual `ab_other_us`, and the exclusive per-function diagnostic split `ab_search_us`, `ab_search0_us`, `ab_search1_us`, `ab_search2_us`, and `ab_search3_us`,
7. writes `summary.json` and `summary.md`,
8. restores a normal non-instrumented library build by default.

## Default workloads

- `regression_api` on `hands/list10.txt` and `hands/thomas1.txt`
- `dtest -f ../hands/list10.txt -s solve`
- `dtest -f ../hands/list100.txt -s solve`
- `play_analysis_benchmark`

In the current workload mix:

- `regression_api` is the main source of `SolveBoardInternal` and `SolveSameBoard` measurements,
- `play_analysis_benchmark` is the dedicated source of `AnalyseLaterBoard` measurements,
- `alpha_mu_leaf_depth0` is the dedicated exact leaf workload for the same DDS solve path alpha-mu uses at depth 0,
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
- `ab_us` remains the full exclusive AB bucket; after pruning timers below the current 5% cutoff, the retained coarse split is `ab_frontend_us` (pre-loop setup and terminal/pruning work), `ab_iteration_control_us`, and the residual `ab_other_us`.
- The per-function diagnostic fields `ab_search_us`, `ab_search0_us`, `ab_search1_us`, `ab_search2_us`, and `ab_search3_us` are exclusive local `ABsearch*` timings that pause around recursive child calls. They are a complementary structural view of where local search-body time lands by function; because `ab_us` remains the historical differentiated AB estimator, do not expect these diagnostic fields to sum exactly to `ab_us`.
- If `AnalyseLaterBoard` does not appear in the summary, the next improvement should be to add a dedicated play-analysis workload.
- The workflow is measurement-only; it does not alter deep search semantics.

## Alpha-Mu solver

The repository also now contains a separate alpha-mu solver runner:

- build target: `alpha_mu`
- source: `test/alpha_mu.cpp`

Unlike the benchmark runner, the solver runner is not a DDS root-policy measurement tool. It is a separate semantics-oriented test component for Pareto fronts, toy alpha-mu search, and a DDS-backed leaf-evaluation demo.

The CLI intentionally keeps two reporting scopes separate:

- `pbn_recommend` is an exact full-information DDS continuation workflow and
  reports `ALPHA_MU_PBN_RECOMMEND`.
- `decision` is the partial-information alpha-mu decision workflow and reports
  `ALPHA_MU_DECISION`.

Tests should keep those scopes distinct so exact DDS continuation output is not
mistaken for a hidden-information alpha-mu recommendation.

The new `test/alpha_mu_dds_compare.py` runner sits between the two: it uses the solver's exact one-world bridge benchmark modes to compare original DDS solve speed against alpha-mu solve speed on the same boards, while also checking score agreement at each measured alpha-mu depth.

For longer single benchmark runs, `test/run_alpha_mu_benchmark.py` streams output live, preserves machine-readable `ALPHA_MU_BENCHMARK_PROGRESS ...`, `ALPHA_MU_BENCHMARK_CHECKPOINT ...`, and `ALPHA_MU_BENCHMARK_BOARD ...` lines in the log, and updates a `.status.json` sidecar so interrupted runs still leave partial progress behind, including the latest in-flight board snapshot, completed board numbers, and per-board timings.

Example depth-2 baseline run from the repository root:

```zsh
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list1.txt --depth 2 --checkpoint-seconds 30
```

Apple-Silicon board-parallel runs can now choose the board-worker backend
explicitly. The baseline portable executor remains `stl`, while the Apple-only
executor is `gcd`:

```zsh
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list10.txt --depth 3 --max-boards 0 --skip-boards 2 --parallel board --worker-backend stl --board-workers 4
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list10.txt --depth 3 --max-boards 0 --skip-boards 2 --parallel board --worker-backend gcd --board-workers 4
```

For a routine repeated comparison bundle with semantic-stability checks and a
summary report, use the dedicated backend-comparison runner:

```zsh
python3 test/alpha_mu_backend_compare.py --hand-file hands/list10.txt --depth 3 --parallel board --board-workers 4 --warmups 1 --repeats 5
```

This writes a timestamped bundle under:

- `test/build/alpha_mu_backend_compare/<timestamp>/`

including per-run logs, copied `.status.json` sidecars, `summary.json`, and
`summary.md`.

Skip specific 1-based board numbers or ranges while keeping checkpointed partial progress:

```zsh
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list10.txt --depth 3 --skip-boards 2 --checkpoint-seconds 60 --heartbeat-seconds 60
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list100.txt --depth 2 --skip-boards 2,5-7 --checkpoint-seconds 30
```

For very long boards, `ALPHA_MU_BENCHMARK_PROGRESS ...` lines report the current board number, elapsed time on that board, recursive call count, DDS leaf-call count, tricks remaining, active worlds, current trick size, and player to move. The sidecar mirrors the latest such snapshot as `current_board_progress`.

