# Apple Silicon `P1.6` Plan for DDS and alpha-mu

## Purpose

This note turns audit item `P1.6` into a concrete engineering plan for this repository.

For the PR-sized execution sequence derived from this plan, see
`docs/apple-silicon-p1.6-pr-plan.md`.

It is written with three constraints in mind:

1. `DDS` is historically portable and should remain usable across multiple platforms where practical.
2. `alpha-mu` is an Apple-focused incubation project and is not currently planned as a supported non-Apple-Silicon product.
3. If `DDS` changes are eventually needed to improve `alpha-mu` performance, those changes are acceptable, but their portability cost should be made explicit.

The central rule is therefore:

- treat `DDS_TARGET_APPLE_M1_MAX` as a measured specialization, not a semantic fork,
- but allow Apple-specific evolution where `alpha-mu` performance genuinely depends on it.

## Current code review findings

### 1. The M1 Max search specialization is independent of the threading backend

The Apple-Silicon search specialization is selected by the build macro:

- `DDS_TARGET_APPLE_M1_MAX`

and is implemented in:

- `src/ABsearch_m1max.cpp`

with the generic fallback in:

- `src/ABsearch.cpp`

This specialization is **not** tied to STL threading. It is a separate compile-time search implementation choice.

### 2. DDS already supports multiple threading backends

The current `DDS` threading surface in `src/System.cpp` / `include/dll.h` supports these codes:

- `0` — none / basic
- `1` — Windows native
- `2` — OpenMP
- `3` — Grand Central Dispatch (`GCD`)
- `4` — Boost
- `5` — STL threads
- `6` — TBB
- `7` — STL implementation-parallel algorithms
- `8` — PPL implementation-parallel algorithms

The public API already exposes backend selection through:

- `SetThreading(int code)`

and reports the active backend through:

- `GetDDSInfo()`

### 3. On macOS, the supported Makefile builds already compile both GCD and STL support

The repository Makefiles for macOS define:

- `DDS_THREADS_GCD`
- `DDS_THREADS_STL`

That means Apple builds already contain both backends.

Because `System::Reset()` chooses the first available non-basic backend in numeric order, the current preferred backend on macOS is actually:

- `GCD` (code `3`)

not:

- `STL` (code `5`)

So the remembered rule that the Apple path was only enabled with STL threading is no longer accurate for the current tree.

### 4. The current alpha-mu board-parallel path does not use DDS thread scheduling

This point is easy to miss and matters for planning.

`alpha-mu` board-parallel benchmarking currently creates its own workers in:

- `test/alpha_mu_core.cpp`

using:

- `std::thread`

It then assigns explicit DDS thread slots to those workers and calls DDS single-board APIs with a chosen `thrId`.

So today:

- `alpha-mu` board scheduling is owned by `alpha-mu`, not by `DDS`,
- `DDS` multithreading backends matter mainly for `DDS` batch APIs such as `SolveAllBoards*()`,
- switching DDS backend preference to `GCD` does **not** automatically move alpha-mu board-parallel execution onto `GCD`.

This is the most important practical finding from the review.

## What `P1.6` means in this repository

For this repository, `P1.6` should mean all of the following:

1. the Apple-Silicon search path stays parity-tested against generic `DDS`,
2. Apple performance work for `alpha-mu` is allowed to be Apple-specific,
3. the first Apple concurrency improvements for `alpha-mu` should happen in `alpha-mu`'s own worker layer before destabilizing `DDS`,
4. any later `DDS` portability sacrifice must be deliberate, documented, and benchmark-justified.

In short:

- keep the `DDS` generic path alive as long as it is not the bottleneck,
- optimize `alpha-mu` aggressively in Apple-specific layers first,
- only pay `DDS` portability costs when a measured alpha-mu bottleneck leaves no cheaper route.

## Portability policy for future changes

To keep tradeoffs explicit, changes should be classified into the following buckets.

### Bucket A — Apple-specific alpha-mu only

Examples:

- adding a `GCD` worker backend for alpha-mu board-parallel execution,
- Apple-only QoS handling in alpha-mu worker code,
- Apple-only benchmark harness changes,
- Apple-only alpha-mu tuning flags.

**Portability cost to DDS:** none.

**Recommendation:** strongly preferred first.

### Bucket B — Apple-specific DDS optimization with generic fallback preserved

Examples:

- extending `DDS_TARGET_APPLE_M1_MAX` search specialization,
- adding Apple-only fast-path helpers guarded by `#ifdef __APPLE__` or `DDS_TARGET_APPLE_M1_MAX`,
- improving GCD behavior in `DDS` while keeping other backends compiling and working.

**Portability cost to DDS:** low to moderate.

**Recommendation:** acceptable when parity-tested and benchmark-justified.

### Bucket C — DDS interface or implementation changes that primarily serve alpha-mu and weaken portability

Examples:

- changing core `DDS` assumptions around scheduler or backend ownership to favor Apple-only execution,
- removing or neglecting generic paths because alpha-mu only needs Apple Silicon,
- introducing Apple-specific semantics into what is currently a portable core path.

**Portability cost to DDS:** high.

**Recommendation:** only after the lower-cost options have been exhausted and the measured alpha-mu benefit is material.

## Multithreading review and implications

## DDS backends today

`DDS` already has implementations for:

- basic serial execution,
- WinAPI,
- OpenMP,
- `GCD`,
- Boost,
- STL threads,
- TBB,
- STL execution-policy mode,
- PPL execution-policy mode.

For Apple work, the practically relevant backends are:

- `GCD`,
- STL threads,
- basic serial.

The `STLIMPL` and `PPLIMPL` variants remain special-purpose and are not the right starting point for performance-sensitive Apple work.

## Why a new GCD option still makes sense for alpha-mu

Although `DDS` already supports `GCD`, `alpha-mu` board-parallel execution currently does not use it.

So there is still a real missing feature:

- an Apple-specific `alpha-mu` board-worker backend that can use `GCD` instead of `std::thread`.

That work belongs naturally in `alpha-mu`, not in `DDS`, because `alpha-mu` currently owns the board-level work queue.

## Far-reaching ramifications of an alpha-mu GCD backend

Adding a GCD-backed alpha-mu worker mode is not a trivial mechanical swap. It affects:

- worker-to-DDS-thread-slot assignment,
- exception capture and propagation,
- progress-reporting order,
- deterministic summary generation,
- worker QoS and oversubscription behavior,
- test coverage for repeated runs,
- and the boundary between portable alpha-mu code and Apple-only execution code.

Those ramifications are real, but they are still **smaller** than re-architecting DDS around alpha-mu's worker model.

## Recommended staged plan

### Stage 0 — Document the current truth

Record and preserve these facts:

- `DDS_TARGET_APPLE_M1_MAX` is separate from backend selection,
- macOS builds already compile both `GCD` and STL support,
- current macOS preference in `DDS` is `GCD`,
- current board-parallel alpha-mu scheduling is `std::thread`-based and bypasses `DDS` backend selection.

**Portability impact:** none.

### Stage 1 — Keep parity checks for generic vs M1-specialized DDS

For Apple-Silicon performance work, always keep a same-machine comparison path:

- specialized `DDS` build (`M1_MAX_BUILD=1`),
- generic fallback build (`M1_MAX_BUILD=0`),
- score parity on correctness workloads,
- benchmark comparison on canonical Apple workloads.

This preserves the meaning of `P1.6`: the Apple path is an optimization, not a semantic fork.

**Portability impact:** none.

### Stage 2 — Add an alpha-mu worker-backend abstraction

Refactor alpha-mu board-parallel execution so the scheduling layer is explicit, for example:

- `serial`
- `stl`
- `gcd`

The existing behavior should remain the default-compatible path until the new backend is proven.

This is the right place to introduce Apple-specific GCD execution for alpha-mu.

**Portability impact:** none to DDS; low within alpha-mu.

### Stage 3 — Add an Apple-only alpha-mu GCD board scheduler

Implement a new alpha-mu board execution mode on Apple platforms that:

- uses `dispatch_apply` or an equivalent `GCD` work distribution model,
- preserves stable logical worker indices,
- maps those indices to explicit DDS thread ids,
- keeps result collation deterministic by board number,
- preserves serial fallback.

This should be benchmarked against the current `std::thread` board scheduler on the same Apple-Silicon hardware.

**Portability impact:** none to DDS.

### Stage 4 — Measure whether DDS backend selection matters for alpha-mu leaf work

Because alpha-mu currently calls single-board DDS APIs directly with explicit thread ids, changing the internal DDS backend may have little or no effect on alpha-mu board-parallel performance.

This stage should verify that assumption explicitly.

If measurements show that alpha-mu remains dominated by:

- board scheduling overhead outside DDS,
- DDS leaf search time inside `SolveBoardPBN()`,
- or alpha-mu front/world work,

then further DDS threading changes should wait.

**Portability impact:** none.

### Stage 5 — Only then consider Apple-specific DDS changes motivated by alpha-mu

If stages 1 to 4 show that alpha-mu still needs more speed and the next real bottleneck is inside DDS, acceptable next steps include:

- extending the Apple-Silicon DDS search specialization,
- improving Apple-only worker QoS or backend behavior,
- adding alpha-mu-oriented instrumentation hooks in DDS,
- or adding guarded Apple fast paths in the DDS leaf path.

These changes should remain behind generic fallbacks where reasonably possible.

**Portability impact:** low to moderate.

### Stage 6 — Explicit escalation path if DDS portability must be sacrificed

If a measured alpha-mu bottleneck still cannot be solved without altering DDS in ways that materially reduce portability, the repository should say so plainly.

The decision rule should be:

1. state the expected alpha-mu gain,
2. state what portability is being given up,
3. state which generic fallback or backend is being weakened or removed,
4. preserve correctness parity where possible even if performance parity is abandoned.

This is the point where the project would be choosing `alpha-mu` performance over DDS portability.

That choice is allowed by project goals, but it should be treated as a conscious architectural step, not as accidental drift.

## Recommendation on immediate next work

The recommended immediate `P1.6` plan is:

1. keep `DDS` generic-vs-specialized parity and benchmark comparison as a permanent Apple-Silicon gate,
2. do **not** start by rewriting DDS threading,
3. add an explicit alpha-mu worker-backend abstraction,
4. prototype an Apple-only GCD backend for alpha-mu board-parallel work,
5. benchmark `std::thread` vs `GCD` in alpha-mu before making further DDS portability tradeoffs,
6. only if the measured bottleneck remains inside DDS should further Apple-specific DDS work proceed.

## Signals to watch for during implementation

The following outcomes should be treated as explicit warning signals.

### Yellow flags

- the alpha-mu GCD backend improves wall time but breaks deterministic reporting,
- worker-to-DDS-thread-id mapping becomes fragile,
- repeated runs show timing wins only within normal board-parallel noise,
- the GCD path forces alpha-mu code to become harder to read or test.

### Red flags

- the M1-specialized DDS path diverges in score from generic DDS,
- alpha-mu changes require deleting or neglecting the generic DDS path,
- a DDS Apple-only optimization becomes semantically required for correctness,
- portability loss is proposed without a measured alpha-mu win.

## Definition of done for this plan

`P1.6` should be considered well-implemented when:

- Apple-Silicon DDS specialization remains parity-tested against generic DDS,
- the docs explicitly describe the current backend reality on macOS,
- alpha-mu has an explicit place to host Apple-specific board scheduling,
- Apple `GCD` has been evaluated where it actually matters for alpha-mu,
- and any future portability sacrifice in DDS is recorded as an intentional tradeoff rather than an accidental side effect.

