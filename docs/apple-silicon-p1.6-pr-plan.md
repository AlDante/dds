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

### Current readiness assessment (`2026-05-04`)

The repository is now at the point where `PR 5` can be evaluated explicitly, but
it is **not yet justified** by the current evidence.

- `PR 1` through `PR 4` are functionally in place for the current scoped plan.
- Apple worker-backend comparisons are now routine and reproducible via the
  dedicated backend comparison runner and `make apple-backend-compare`.
- The current `PR 4` slice added per-root DDS phase timing fields for the leaf
  contexts alpha-mu actually exercises: `ab_us`, `qt_us`, `lt_us`,
  `movegen_us`, `lookup_us`, `build_us`, and `undo_us`.
- The focused instrumented lane has been shortened so `make instrumented-check`
  now exercises only the explicit `hands/list10.txt` regression input and no
  longer pulls in the long-running `thomas1` / `thomas2` cases.
- The remaining `ALPHA_MU root` text in the instrumented output has now been
  traced to the single `ReportRootSearchStats()` emitter in `src/SolverIF.cpp`.
  That emitter is shared by the DDS exact-root search contexts
  `SolveBoardInternal`, `SolveSameBoard`, and `AnalyseLaterBoard`; the prefix is
  being kept intentionally for benchmark-parser and log-schema compatibility,
  not because a second legacy alpha-mu-only emitter still exists.
- The optimisation backlog is now clearer, and the next plausible DDS-side work
  items (`-flto`, NEON helper cleanup in `ABsearch_m1max.cpp`, worker QoS, and
  the `QuickTricks` refactor) all still fit **Bucket B**: Apple-specific or
  performance-oriented DDS work with the generic fallback preserved.

So the current state still supports the original policy: keep the generic path
alive, use the measurement-first tooling to locate the actual remaining leaf hot
spots, and do **not** open a real portability-sacrifice `PR 5` until a specific
`Bucket C` change is shown to beat the lower-cost alternatives.

### PR 5 checklist

- [x] `PR 1` through `PR 4` groundwork is in place.
- [x] Generic-vs-specialized parity checks remain part of the Apple-silicon plan.
- [x] Reproducible backend-comparison tooling exists for `stl` vs `gcd` alpha-mu runs.
- [x] Focused DDS leaf-path instrumentation exists for the exact alpha-mu leaf contexts.
- [x] The focused instrumented regression lane has been reduced to `hands/list10.txt`.
- [x] Finish tracing and either remove or explicitly justify the remaining legacy `ALPHA_MU root` emitter in the instrumented output.
- [ ] Re-run the focused instrumented lane and summarize the updated phase timings for the remaining hot leaf contexts.
- [ ] Confirm that the next material bottleneck is still inside `DDS`, not in alpha-mu board scheduling, front/world work, or reporting overhead.
- [ ] Name the exact proposed `Bucket C` portability tradeoff rather than a general class of possible optimisations.
- [ ] Quantify the expected gain of that exact tradeoff on the representative Apple workload.
- [ ] State the portability or maintainability cost being accepted.
- [ ] State which generic fallback, backend, or portable path would be weakened, bypassed, or left behind.
- [ ] Show that lower-cost `Bucket A` / `Bucket B` options were either exhausted, measured, or intentionally deferred with reasons.
- [ ] Preserve the existing correctness and parity gates for the chosen change (`regression_api`, `dtest`, `play_analysis_benchmark`, and backend semantic-stability checks where relevant).
- [ ] Record the final tradeoff decision plainly in the PR description and follow-up docs.

### Current recommendation

Do **not** treat `PR 5` as an implementation PR yet. Treat it as a gated
decision point. The next concrete work should still be:

1. finish the focused instrumentation cleanup,
2. collect the updated DDS leaf-path timing evidence,
3. and then start with the highest-confidence preserved-fallback work from
   `docs/optimisation-plan.md` before considering any true portability
   sacrifice.

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
`lookup_us`, `build_us`, and `undo_us` data instead of another speculative
micro-change.

