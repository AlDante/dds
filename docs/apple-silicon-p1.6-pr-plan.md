# Apple Silicon `P1.6` PR Plan

## Purpose

This document turns `docs/apple-silicon-p1.6-plan.md` into a PR-sized execution sequence.

The guiding rule is:

- keep `DDS` generic-vs-specialized parity intact,
- keep `alpha-mu` free to add Apple-specific execution layers,
- and defer any real `DDS` portability sacrifice until measurement shows that the alpha-mu bottleneck is truly inside `DDS`.

## PR 1 — Explicit alpha-mu worker-backend abstraction

### Goal

Make the board-worker backend an explicit alpha-mu concept without changing the default behavior.

### Scope

- add `AlphaMuWorkerBackend` to alpha-mu benchmark/context/reporting types,
- add render/parse helpers for machine-readable logs and CLI options,
- keep the current `std::thread` worker loop as the `stl` backend,
- route board-parallel benchmark execution through a backend dispatcher,
- expose `--worker-backend` in the benchmark CLI and runner,
- update benchmark logs/status output to include `worker_backend`.

### Non-goals

- no `GCD` executor yet,
- no change to default runtime behavior,
- no change to DDS threading selection,
- no changes to alpha-mu root-parallel execution,
- no changes to DDS search semantics.

### Acceptance criteria

- existing alpha-mu serial and board-parallel runs still work,
- default board-parallel execution remains `std::thread`-based,
- machine-readable benchmark outputs include `worker_backend=...`,
- regressions cover backend parsing/rendering and benchmark reporting.

## PR 2 — Apple-only alpha-mu `GCD` board scheduler

### Goal

Add a new Apple-only board-worker backend for alpha-mu using Grand Central Dispatch.

### Scope

- implement `ALPHA_MU_WORKER_BACKEND_GCD` for board-parallel benchmarks on Apple,
- preserve deterministic logical worker indexing,
- preserve explicit worker-to-DDS-thread-id mapping,
- preserve deterministic result collation by board number,
- keep the `stl` backend available as the baseline and fallback.

### Risks

- exception transport across worker callbacks,
- progress log ordering,
- oversubscription if worker count and DDS thread count drift,
- subtle performance regressions despite lower scheduling overhead.

### Acceptance criteria

- `gcd` backend works on Apple without changing exact scores,
- repeated runs remain deterministic at the result level,
- benchmark output stays parseable,
- `stl` vs `gcd` comparison is documented on the target machine.

## PR 3 — Benchmark and instrumentation gate for Apple worker backends

### Goal

Make worker-backend comparisons routine and reproducible.

### Scope

- add benchmark workflows that compare `stl` vs `gcd` for alpha-mu board-parallel runs,
- capture worker-backend choice in summary/status artifacts,
- add repeated-run checks to detect flaky backend behavior,
- document the recommended comparison procedure.

### Acceptance criteria

- backend comparisons are easy to run repeatedly,
- output bundles record the selected backend explicitly,
- worker-backend wins are evaluated against serial CPU-time and deterministic correctness gates.

## PR 4 — Apple-specific DDS improvements with generic fallback preserved

### Goal

Pursue deeper Apple-only DDS work only if alpha-mu measurement shows that board scheduling is no longer the main limiter.

### Candidate scope

- improved Apple worker QoS in DDS,
- additional Apple-only instrumentation hooks for alpha-mu leaf workloads,
- guarded extensions to `DDS_TARGET_APPLE_M1_MAX`,
- Apple-specific tuning in DDS leaf-entry hot paths.

### Gate

This PR should proceed only if measurement shows that the remaining alpha-mu bottleneck is substantially inside DDS rather than in alpha-mu board scheduling or alpha-mu bridge/front logic.

## PR 5 — Explicit portability tradeoff decision, if needed

### Goal

If alpha-mu still requires more speed and no lower-cost option remains, make any DDS portability sacrifice deliberate and documented.

### Requirements

Every such PR must state:

1. the measured alpha-mu bottleneck,
2. the expected performance gain,
3. the portability or maintainability cost being accepted,
4. the generic fallback or backend impact,
5. and the correctness/parity checks that still remain in force.

## Recommended order

1. PR 1 — explicit alpha-mu worker-backend abstraction
2. PR 2 — Apple-only alpha-mu `GCD` backend
3. PR 3 — backend benchmark/instrumentation gate
4. PR 4 — Apple-specific DDS tuning, if measurement requires it
5. PR 5 — explicit DDS portability tradeoff, only if unavoidable

## Current status

This repository change set implements:

- **PR 1**
- **PR 2**
- **PR 3**
- **PR 4** (measurement-first slice)

The repository now includes a dedicated backend-comparison runner and Makefile
entry point for repeated `stl` vs `gcd` alpha-mu benchmark comparisons with
semantic-stability checks. The current PR4 slice adds per-root DDS phase timing
export for the exact leaf contexts alpha-mu actually uses, so future Apple DDS
tuning can be driven by measured `ab_us`, `qt_us`, `lt_us`, `movegen_us`,
`lookup_us`, and `build_us` data instead of another speculative micro-change.

