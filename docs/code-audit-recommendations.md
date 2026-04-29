# C++ Codebase Audit Recommendations

## Scope

This review covers the current C++ codebase across:

- public API definitions in `include/`, especially `include/dll.h`,
- the DDS solver core in `src/`, including solve orchestration, alpha-beta search,
  move generation, quick/later trick pruning, transposition tables, memory,
  threading, scheduling, PBN conversion, table calculation, and play analysis,
- C++ test and benchmark harnesses in `test/`, including the alpha-mu runner,
- C++ examples in `examples/`,
- and build/test configuration in the repository root, `src/`, `test/`, and docs.

The recommendations are intentionally practical. They prioritize changes that reduce
correctness risk, preserve the existing C ABI, improve performance evidence, and make
future alpha-mu / DDS integration easier to maintain.

## Executive summary

DDS has a compact, mature, performance-oriented core with useful regression and
benchmark coverage. The main improvement opportunity is not a rewrite. It is a
series of targeted hardening and design-boundary changes:

1. make public-input validation more explicit and more uniformly tested,
2. remove or isolate global mutable run state so concurrent API users are safe,
3. turn fragile macro/raw-pointer/threading constructs into small RAII or typed
   wrappers where the C ABI permits,
4. protect the most aggressive search shortcuts with dedicated differential tests,
5. unify the build surface so Makefile, CMake, examples, tests, and docs do not drift,
6. and move alpha-mu from `test/` incubation toward a first-class internal module once
   its public scope is settled.

## Priority 0: correctness and safety

### C0.1 Make all API-level input validation explicit and regression-backed

**Why:** `SolverIF.cpp` does good range/value checks after binary conversion, but the
PBN conversion path in `src/PBN.cpp` is intentionally permissive and returns success
for many malformed strings that are later diagnosed indirectly, if at all.

Observed examples:

- `ConvertFromPBN()` scans for a starting seat and then consumes up to 80 characters,
  but it does not itself enforce exactly four hands, exactly four suit fields per
  hand, valid separator placement, or 13 cards per hand.
- `BoardValueChecks()` catches duplicate cards and inconsistent remaining hand sizes,
  but the resulting error can be less actionable than a PBN-specific parse error.
- The alpha-mu PBN loader now has stricter full-deal validation; the core DDS PBN
  path should have comparable clarity.

**Recommendation:** add a strict PBN validation layer before or inside
`ConvertFromPBN()` and keep the existing permissive behavior only if compatibility
requires it. A low-risk path is to introduce a new helper, for example
`ValidateDealPBNText()`, and call it from public PBN entry points before conversion.
If compatibility with old callers is uncertain, add a compile-time or API-local
strictness flag and first use it in tests and internal tools.

**Tests to add:**

- wrong number of hands,
- wrong number of suit fields,
- illegal seat token,
- illegal rank,
- duplicate card,
- missing card / wrong hand count,
- extra separator,
- lower-case valid input,
- current-trick card also remaining in a hand,
- duplicate cards within the current trick.

### C0.2 Fix the public error-text mismatch for `RETURN_FIRST_WRONG`

`include/dll.h` documents:

```text
TEXT_FIRST_WRONG "First is not in 0 .. 2"
```

but `BoardRangeChecks()` accepts `dl.first` in `0..3`. The text should say `0 .. 3`.
This is small but user-visible, and it matters when adding stricter validation.

### C0.3 Add explicit legality checks for current trick consistency

`BoardRangeChecks()` verifies rank/suit ranges and contiguous current-trick entries.
`BoardValueChecks()` verifies card counts and that current-trick cards are not also in
remaining hands. It does not appear to fully validate every bridge-legality property
that can be inferred from the supplied remaining hands, especially:

- duplicate current-trick cards,
- whether a current-trick card could have belonged to the recorded player,
- and follow-suit legality for the current trick where enough information is present.

Some of these checks may be intentionally omitted for historical compatibility. If so,
document the contract precisely. Otherwise, add explicit validation and regression
coverage. The alpha-mu bridge layer already has useful debug invariants for partial
tricks; the core DDS API should have similarly clear user-facing checks.

### C0.4 Eliminate data races in global run error state

The core multi-board paths use global run-state objects:

- `param` in `src/SolveBoard.cpp`,
- `cparam` in `src/CalcTables.cpp`,
- `playparam` / `traceparam` in `src/PlayAnalyser.cpp`,
- plus global `memory`, `scheduler`, and `sysdep` collaborators.

Worker threads can write shared error fields such as `param.error`, `cparam.error`,
and trace error state without synchronization. In practice these writes may usually
store the same kind of terminal error, but they are still C++ data races.

**Recommendation:** change these error fields to `std::atomic<int>` or collect
per-thread/per-board results and reduce them after joining. Prefer deterministic error
selection, for example lowest board index first, to make failures reproducible.

### C0.5 Make top-level public API calls explicitly serialized or reentrant

The public C API exposes functions that operate through process-wide mutable state.
Even if the internal worker model is safe for one active DDS run at a time, two client
threads concurrently calling high-level API functions can interleave through shared
`param`, `cparam`, `scheduler`, `memory`, and `sysdep` state.

**Recommendation:** choose and document one of these contracts:

1. **Serialized API contract:** only one top-level DDS run may be active at a time.
   Enforce this with an internal mutex around public entry points.
2. **Reentrant context contract:** introduce an internal run context and eventually a
   context-based C++ API, while preserving the existing C ABI as a wrapper around a
   default context.

The serialized contract is the quickest correctness hardening. The context contract is
the better long-term design.

### C0.6 Reset or remove static thread-id state in parallel helper paths

`System::RunThreadsSTLIMPL()` and `System::RunThreadsPPLIMPL()` use static
`atomic<int> thrIdNext`. Because it is static, it is not reset per run. That can cause
thread-id allocation to drift across repeated calls. Those paths also update shared
`err` flags from parallel lambdas without atomic protection.

**Recommendation:** make run-local thread-id allocation state and use atomics for
shared error flags. Add a regression that calls the affected run mode repeatedly.

### C0.7 Check instrumentation builds as first-class builds

`CopySolveSingle()` and `CopyCalcSingle()` use `START_THREAD_TIMER(thrId)` even though
those functions do not accept a `thrId` parameter. With `DDS_SCHEDULER` disabled this
macro compiles away; with scheduler instrumentation enabled it expands to code that
appears likely to fail compilation.

**Recommendation:** add CI/build targets for instrumentation configurations such as
`DDS_SCHEDULER`, `DDS_TIMING`, `DDS_TT_STATS`, and the profiling build. If
`Copy*Single()` needs timing, pass an explicit thread id or remove those timer calls.

### C0.8 Replace process-exiting internals in alpha-mu with returnable errors

The alpha-mu test runner uses `Fail()` / `Check()` that write to `stderr` and call
`exit(1)`. That is acceptable for a CLI test binary but becomes problematic if
alpha-mu is promoted to a library or called from another application.

**Recommendation:** keep CLI-facing `Fail()` at the outermost layer, but refactor
internal alpha-mu APIs to return structured status objects or throw exceptions that the
CLI catches. This will make PBN recommendation and future UI/API work safer.

## Priority 1: performance

### P1.1 Keep AB search shortcuts under differential testing

The hot path in `ABsearch.cpp`, `QuickTricks.cpp`, `LaterTricks.cpp`, and `Moves.cpp`
is heavily optimized and contains historically delicate pruning logic. There are also
comments such as:

- `ABsearch.cpp`: `// Is 1 right here?!`
- `QuickTricks.cpp`: `// TODO: Is the fix to qt correct?`

These comments identify exactly where small logic changes can cause silent wrong
answers.

**Recommendation:** create targeted differential tests that compare optimized search
against a deliberately slower oracle on small endgame positions. For each quick/later
trick shortcut, include at least one regression where the shortcut fires and one nearby
position where it must not fire.

### P1.2 Add a repeatable sanitizer/debug correctness lane separate from release speed

Current release builds use aggressive optimization and warnings. Add a slower but
high-signal lane:

- AddressSanitizer / UndefinedBehaviorSanitizer for single-threaded tests,
- ThreadSanitizer for selected multi-threaded API calls,
- debug assertions enabled,
- and exact comparison against known hand files.

This is especially useful for global run state, raw arrays, TT memory management,
current-trick input validation, and alpha-mu world masks.

### P1.3 Profile before changing move generation or quick-trick logic

`Moves.cpp` is large and branch-heavy but deliberately allocation-free in the hot path.
Before changing its representation, keep using the existing profiling workflow and add
per-function counters around:

- `Moves::MoveGen0()`,
- `Moves::MoveGen123()`,
- `Moves::MakeNext()`,
- `QuickTricks*()`,
- `LaterTricks*()`,
- TT lookup/add,
- and root exact-score probing in `SearchExactScoreRoot()`.

The likely wins are move ordering, TT locality, and reducing repeated root probes, not
large structural rewrites.

### P1.4 Review transposition-table memory layout and replacement policy

The core TT design is specialized and compact. The alpha-mu bridge TT uses a small
fixed-probe table with replacement after four probes. Both can benefit from explicit
measurement:

- hit rate by depth,
- collision/replacement rate,
- exact-vs-bound reuse rate,
- cache-line behavior,
- memory pressure by configured thread count,
- and reset reasons.

**Recommendation:** expose a minimal always-available TT summary in benchmark output,
not only diagnostic macro builds. Use it to decide whether to tune capacity,
replacement, or key layout.

### P1.5 Avoid unnecessary allocation in alpha-mu recursive bridge search

Alpha-mu bridge search frequently constructs vectors and front objects:

- `ExpandBridgeChildren()` returns `vector<BridgeChild>`,
- each child owns a copied `BridgeState`,
- `ParetoFront::MaxMerge()` and `MinProduct()` create new fronts,
- DDS leaf evaluation loops over worlds serially.

This is fine for the current correctness-first implementation, but it will dominate as
world counts/depth grow.

**Recommendations:**

- reserve child vectors using the known legal-move upper bound,
- consider move-only or stack-backed child generation for hot recursion,
- add front object reuse or small-vector storage,
- measure dominance-reduction cost separately from DDS leaf cost,
- and parallelize DDS leaf evaluation only after the single-thread semantics are fully
  frozen.

### P1.6 Treat `DDS_TARGET_APPLE_M1_MAX` as a measured specialization, not a fork

The Makefiles select `DDS_TARGET_APPLE_M1_MAX` automatically on arm64. This is useful,
but architecture-specific search code such as `ABsearch_m1max.cpp` should remain under
strict parity tests against generic search.

**Recommendation:** every architecture-specialized build should run:

- generic-vs-specialized score parity,
- representative benchmark comparison,
- and a fallback generic build on the same machine.

## Priority 2: software design and maintainability

### D2.1 Preserve the C ABI but introduce internal C++ context objects

The public `include/dll.h` ABI is valuable and should remain stable. Internally,
however, the solver would benefit from explicit context objects for one solve run:

- input board span,
- output span,
- scheduler state,
- error state,
- memory/thread resources,
- duplicate/cross-reference results,
- and per-run timing/statistics.

This would make the current global `param` / `cparam` / `traceparam` pattern easier to
reason about and eventually make concurrent top-level calls safe.

### D2.2 Remove `using namespace std` from headers over time

Several internal headers, such as `src/TransTable.h`, `src/Memory.h`, `src/Moves.h`,
and alpha-mu headers, place `using namespace std` at header scope. This leaks names
into every including translation unit and makes future library integration harder.

**Recommendation:** remove header-level `using namespace std` incrementally. Start with
new or recently changed headers, then migrate older headers when touched for nearby
work.

### D2.3 Replace macros with typed constants or inline functions where practical

Examples:

- `handId(hand, relative)` should become an inline `constexpr` function with
  parenthesized arguments and return expression,
- timer macros should become no-op inline functions under disabled builds,
- `MAXNODE` / `MINNODE` should become an enum or scoped constants.

Do not do this in one broad formatting pass. Convert macros only when touching related
code, and keep performance-sensitive inlining explicit.

### D2.4 Convert raw owning pointers to RAII in non-ABI internals

`Memory` owns `ThreadData*` and `TransTable*`; `System::RunThreadsSTL()` uses
`vector<thread*>`. These are internal implementation details and can be modernized
without changing the public C ABI.

**Recommendation:** use `std::unique_ptr<ThreadData>`, `std::unique_ptr<TransTable>`,
and `std::vector<std::thread>`. This reduces leak/exception risk and simplifies early
returns.

### D2.5 Make abstract interfaces truly abstract

`TransTable` currently provides empty virtual implementations for most methods. This
can hide incomplete implementations and requires suppressing unused-parameter warnings.

**Recommendation:** make required operations pure virtual. If a no-op TT is useful,
introduce an explicit `NullTransTable` implementation.

### D2.6 Split alpha-mu incubation from test infrastructure

The alpha-mu implementation is currently under `test/` and has grown into a substantial
engine:

- `alpha_mu_core.h` is large and mixes public types, implementation helpers,
  benchmarks, reporting, and test-facing utilities,
- implementation files are split by subsystem but still expose many functions through
  one broad header,
- the CLI runner is a test binary rather than a supported tool target.

**Recommendation:** when the API stabilizes, move alpha-mu into a first-class internal
module, for example `src/alpha_mu/` or `tools/alpha_mu/`, with separate headers for:

- public request/result types,
- bridge state and DDS leaf adapter,
- world construction and information state,
- Pareto/front operations,
- CLI/reporting,
- and tests.

### D2.7 Unify Makefile and CMake behavior

The Makefiles currently drive the real library/test builds. `src/CMakeLists.txt` sets
C++17 and documentation support but does not appear to define the main library or test
executables in parity with the Makefiles. This creates build-system drift.

**Recommendation:** either:

1. make CMake a complete supported build with library, examples, tests, alpha-mu, and
   options matching the Makefiles, or
2. document that CMake is documentation/IDE-only and keep Makefile as canonical.

The first option is better for CI and portability.

### D2.8 Separate generated/build artifacts from source-controlled examples and tests

The workspace contains build directories and binaries under `src/`, `test/`, and
`examples/`. If any are tracked, remove them from version control. If they are ignored,
make the ignore policy explicit and keep examples source-only.

### D2.9 Add a small developer-facing invariant guide for the core DDS solver

Alpha-mu now has detailed invariant documentation. The classic DDS solver would benefit
from a similar concise guide covering:

- `deal` / `dealPBN` invariants,
- current-trick representation,
- `pos` depth indexing,
- `handRelFirst`, `first[]`, and `handId`,
- TT bound semantics,
- quick/later trick soundness assumptions,
- `futureTricks` result semantics by `solutions` and `mode`,
- and scheduler duplicate/cross-reference invariants.

This would make future correctness reviews much cheaper.

## Priority 3: tests and CI

### T3.1 Promote `regression_api` to the minimum public-API correctness gate

`test/regression_api.cpp` is valuable because it exercises the public C API against
known goldens, multiple `solutions` modes, table calculation, par, and play analysis.
Treat it as mandatory for every solver change.

### T3.2 Add malformed-input regression suites

Create negative tests for each public API family:

- `SolveBoard*`,
- `SolveAllBoards*`,
- `CalcDDtable*`,
- `CalcAllTables*`,
- `AnalysePlay*`,
- par APIs,
- and alpha-mu PBN/decision modes.

These tests should assert exact return codes and, where available, `ErrorMessage()`
text.

### T3.3 Add repeated-call and concurrent-call tests

Add tests that repeatedly call public APIs with changing thread counts, including:

- serial repeated solve,
- multi-board repeated solve,
- `SetMaxThreads()` changes between calls,
- `FreeMemory()` between calls,
- repeated table calculation,
- repeated play analysis,
- and intentionally concurrent top-level calls if the chosen contract is to support
  them.

If the chosen contract is serialized-only, test that the internal mutex enforces it.

### T3.4 Keep alpha-mu exact/partial-information scopes distinct in tests

The new `pbn_recommend` mode reports an exact full-information DDS recommendation line.
The `decision` mode is the partial-information alpha-mu decision workflow. Tests and
docs should continue to label these separately so users do not mistake exact DDS
continuation for a hidden-information alpha-mu principal variation.

## Suggested execution order

1. **Small correctness fixes:** fix `TEXT_FIRST_WRONG`; add duplicate current-trick and
   stricter PBN negative tests; add regression for scheduler instrumentation build.
2. **Thread-safety hardening:** make shared error fields atomic or per-thread; reset
   static thread-id allocation; make error flags atomic.
3. **Build/CI hygiene:** add sanitizer and instrumentation build targets; decide Make
   vs CMake ownership.
4. **Search shortcut protection:** build differential oracle tests for quick/later
   trick cases and AB root probing.
5. **Design migration:** introduce internal run context; remove header `using namespace
   std`; replace raw owning pointers with RAII.
6. **Alpha-mu modularization:** move implementation out of `test/` once its supported
   CLI/API boundary is settled.

## Non-goals

The following are not recommended as near-term work:

- rewriting the solver in modern C++ wholesale,
- replacing the C ABI,
- changing the optimized move generator without profiling evidence,
- making alpha-mu larger-world support before the current 64-world contract is
  abstracted,
- or merging exact DDS PBN recommendation semantics with partial-information alpha-mu
  decision semantics.

The best path is incremental hardening: preserve the solver's speed and API stability,
while making correctness assumptions explicit and mechanically tested.

