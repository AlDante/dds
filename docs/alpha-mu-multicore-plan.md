# Alpha-Mu Multicore Implementation Plan

This document records the staged plan for making the `alpha_mu` use multiple cores without destabilizing DDS correctness.

## Objective

Use all available cores for alpha-mu workloads while preserving:

- exact DDS-vs-alpha-mu score agreement,
- deterministic benchmark summaries,
- parseable progress logging,
- and a permanent serial fallback path.

## Current state

The current exact alpha-mu benchmark path is serial:

- `BenchmarkAlphaMuExactBoards()` iterates over boards one at a time,
- `SearchBridgeStateInternal()` performs a serial recursive search,
- DDS leaf evaluation uses `SolveBoardPBN()` one world at a time,
- DDS multithreading exists primarily behind batch APIs such as `SolveAllBoards()`.

That means the current solver does not naturally use all cores for alpha-mu benchmark runs.

## Parallelisation recommendation

The recommended rollout is:

1. add explicit execution-context plumbing,
2. parallelize across boards first,
3. harden benchmark logging and status reporting,
4. then consider optional root-level parallelism for single-board latency.

This is the lowest-risk path because board-level work items are coarse-grained and independent.

## PR-sized milestones

### PR 1 — Execution-context refactor with no behavior change

Goal:

- remove reliance on implicit global benchmark progress state,
- stop hardcoding DDS thread slot `0` in concurrency-sensitive bridge search helpers,
- add normalized benchmark options and future concurrency-policy plumbing,
- preserve serial behavior by default.

Key files:

- `src/alpha_mu_core.h`
- `src/alpha_mu_core.cpp`
- `test/alpha_mu.cpp`
- `test/alpha_mu_tests.cpp`

Key regressions:

- explicit execution-context bridge search parity,
- benchmark option normalization,
- benchmark overload parity.

### PR 2 — Board-parallel benchmark execution

Goal:

- parallelize `BenchmarkAlphaMuExactBoards()` across boards,
- keep each board search internally serial,
- assign stable DDS thread slots per worker,
- collate results deterministically by board number.

Key files:

- `src/alpha_mu_core.h`
- `src/alpha_mu_core.cpp`
- `test/alpha_mu.cpp`
- `test/run_alpha_mu_benchmark.py`
- `test/alpha_mu_tests.cpp`

Acceptance criteria:

- `mismatches=0` in both serial and board-parallel runs,
- deterministic summaries across worker counts,
- visible multicore throughput improvement on multi-board workloads.

### PR 3 — Logging, runner, and documentation hardening

Goal:

- make multicore benchmark runs observable and parseable,
- record parallel mode and worker counts in logs and status sidecars,
- document serial baseline and board-parallel benchmark commands.

Key files:

- `test/run_alpha_mu_benchmark.py`
- `test/alpha_mu_core.cpp`
- `docs/profiling.md`
- `docs/performance.md`

### PR 4 — Determinism and stress-test expansion

Goal:

- prove correctness is stable under repeated multicore execution,
- verify parity across worker counts,
- run larger stress workloads to flush out races or bad shutdown behavior.

Key workloads:

- `hands/list10.txt`
- `hands/list100.txt`
- optionally `hands/list1000.txt` for stress only.

### PR 5 — Performance validation checkpoint

Goal:

- benchmark 1/2/4/8/all-core throughput,
- quantify speedup and efficiency,
- decide whether board-parallel execution is sufficient.

Primary metrics:

- elapsed seconds,
- boards per second,
- speedup vs one worker,
- efficiency.

### PR 6 — Optional root-level parallelism

Goal:

- improve single-board latency by parallelizing only root children,
- keep deeper recursion serial,
- keep this mode opt-in and separate from board-parallel execution initially.

This PR should only proceed if profiling shows that single-board latency remains the primary bottleneck after board-parallel throughput is delivered.

## Regression strategy

At each milestone, rerun:

- `alpha_mu` default suite,
- `alpha_mu bridge_dds`,
- `regression_api`,
- `dtest -f ../hands/list10.txt -s solve`,
- `dtest -f ../hands/list100.txt -s solve`.

For multicore milestones, add:

- serial vs parallel alpha-mu exact-score parity,
- repeated deterministic runs,
- worker-count sweeps at 1/2/4/8/all.

## Parallelisation review checklist

Every PR should be reviewed against these questions:

1. Is correctness exactly preserved?
2. Is all shared mutable state explicit or synchronized?
3. Are results deterministic independent of scheduling?
4. Is the added concurrency actually useful and not oversubscribed?

## Recommended stopping point

If board-parallel alpha-mu produces strong multicore throughput gains with exact-score parity, stop after PR 5 and keep root parallelism as a later optional follow-up.

