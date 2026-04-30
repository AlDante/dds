# Performance Tracking

This page captures two things:

1. the earliest preserved performance history visible in this repository, and
2. the standardized post-change performance routine we now use after important code changes.

## Earliest preserved performance records

The oldest performance note currently preserved in-tree is in `ChangeLog` under **DDS 1.0.1**.

- `ChangeLog:718-728`
- It records that reusing transposition-table contents for subsequent searches gave a **decrease in search time**, usually slight and sometimes substantial.

That is the earliest performance-related record I found, but it is qualitative rather than numeric.

## Earliest quantified performance claim

The earliest explicit numeric claim currently preserved in-tree is in `ChangeLog` under **DDS 1.0.5**.

- `ChangeLog:661-668`
- It reports **about 25% speed improvement** from a search algorithm change.

## Early milestone entries from `ChangeLog`

| Release | Evidence | Notes |
| --- | --- | --- |
| `DDS 1.0.1` | search time decreased in most cases; substantial in a few | earliest preserved performance mention |
| `DDS 1.0.5` | about **25%** speed improvement | earliest quantified claim |
| `DDS 1.0.6` | about **10%** increased speed | quick-tricks improvement |
| `DDS 1.1.0` | about **2x** the speed of `1.0.6` | major TT and quick-tricks redesign |
| `DDS 1.1.2` | about **15%** improvement | move-ordering improvement |
| `DDS 1.1.3` | about **20%** total improvement | compiler plus algorithm improvements |
| `DDS 2.0.0` | single-thread speed about equal to `1.1.9`; two threads about **2x** single-thread | thread-safe `SolveBoard` and parallel use |
| `DDS 2.1.2` | about **10%** faster than `2.1.1` | quick-tricks and move-ordering changes |
| `DDS 2.2.0` | about **10-15%** faster than `2.1.2` | `SolveAllBoards` added |
| `DDS 2.8.0` | about **15%** faster than `2.7.0` | faster move generation and scheduler changes |
| `DDS 2.8.4` | lower-memory mode about **11-14% slower** | explicit speed/memory trade-off |

## Earliest dated benchmark artifact in git history

The earliest dated benchmark/performance artifact I found in the repository history is the addition of the historical document:

- commit `87e6d52`
- date `2014-11-25`
- message: `Updates to documentation directory, incl new Performance document`

That corresponds to:

- `doc/2014-11 Performance and Benchmarking.docx`
- `doc/2014-11 Performance and Benchmarking.pdf`

## Standardized post-change performance suite

After every **important** code change, run the standardized suite:

```zsh
python3 test/standard_performance.py
```

What it does:

- rebuilds the library with `make macos` in `src/`,
- rebuilds the key test binaries in `test/`,
- runs a fixed workload set several times,
- writes a timestamped bundle under `test/build/performance_runs/`,
- appends a summarized entry to `docs/performance-log.md`.

Default workloads:

1. `regression_api_smoke`
2. `dtest_solve_list10`
3. `dtest_solve_list100`
4. `play_analysis_benchmark`
5. `alpha_mu_default`
6. `alpha_mu_bridge_dds`

The default requested repeat count is `1` so the routine can be used after every important code change without becoming too disruptive.

To keep reported medians accurate to roughly `0.1 s` or better, all short workloads in the standardized suite are stabilized automatically with:

- one unmeasured warmup run,
- at least three measured repeats,
- enough measured repeats to accumulate at least `1.0 s` of measured wall time,
- and an automatic cap of `10` repeats unless the user explicitly requests more.

This covers:

- `dtest_solve_list10`
- `dtest_solve_list100`
- `play_analysis_benchmark`
- `alpha_mu_default`
- `alpha_mu_bridge_dds`

The goal is to suppress transient startup and scheduling noise without materially lengthening the standardized suite.

For a stronger comparison run, increase the requested repeat count explicitly, for example:

```zsh
python3 test/standard_performance.py --repeats 3
```

For deeper alpha-mu benchmark instrumentation, keep using the dedicated runner:

```zsh
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list10.txt --depth 3 --max-boards 0 --skip-boards 2
```

For DDS-side leaf-path instrumentation that now includes per-context root timing
fields such as `ab_us`, `qt_us`, `lt_us`, `movegen_us`, `lookup_us`, and
`build_us`, use:

```zsh
python3 test/alpha_mu_benchmark.py
```

For board-parallel throughput experiments, pass the parallel settings explicitly, for example:

```zsh
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list10.txt --depth 3 --max-boards 0 --skip-boards 2 --parallel board --board-workers 4
```

On Apple Silicon, the alpha-mu board scheduler can also be selected explicitly.
Use the existing `stl` backend as the baseline and compare it against the
Apple-only `gcd` backend on the same workload, for example:

```zsh
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list10.txt --depth 3 --max-boards 0 --skip-boards 2 --parallel board --worker-backend stl --board-workers 4
python3 test/run_alpha_mu_benchmark.py --hand-file hands/list10.txt --depth 3 --max-boards 0 --skip-boards 2 --parallel board --worker-backend gcd --board-workers 4
```

For a routine repeated comparison with semantic-stability checks and a summary
bundle, use the dedicated backend comparison runner:

```zsh
python3 test/alpha_mu_backend_compare.py --hand-file hands/list10.txt --depth 3 --parallel board --board-workers 4 --warmups 1 --repeats 5
```

or the convenience target:

```zsh
make apple-backend-compare
```

The comparison bundle is written to:

- `test/build/alpha_mu_backend_compare/<timestamp>/`

and records both backend summaries and any timing-noise warnings while still
failing hard on semantic drift between repeated runs or between `stl` and `gcd`.

## Recorded results

Routine standardized runs are recorded in:

- `docs/performance-log.md`

Raw per-run logs and machine-readable summaries are written to:

- `test/build/performance_runs/<timestamp>/`

For deeper alpha-mu benchmarks outside the standardized suite, see the recorded serial baseline in `docs/performance-log.md` dated `2026-04-18 08:46:32`, which captures the `hands/list10.txt` depth-3 pre-parallelisation run and its strong board-to-board timing skew. Board-parallel runs recorded through `test/run_alpha_mu_benchmark.py` also preserve the reported parallel mode and worker counts in their log and status outputs.

