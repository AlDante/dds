# DDS / alpha-mu Improvement Plan

Priorities, in order: **1. correctness, 2. performance, 3. readability,
idiom and documentation**. This plan complements
`code-audit-recommendations.md` (detailed rationale) and
`optimisation-plan.md` (measured performance outcomes). Items marked
**[done]** have been implemented; verify with `make check`.

## Status snapshot (review of 2026-11-17)

**Achieved**

- Phase 1 safety/correctness work is complete (see below).
- Apple Silicon: the `DDS_TARGET_APPLE_M1_MAX` `ABsearch` path, compact 8-byte
  `moveType`, and the aggressive release profile (`-O3 -flto -mcpu=apple-m1
  -funroll-loops -ffunction-sections -Wl,-dead_strip`, ~6% serial win,
  2026-05-12) are kept. TT default raised to 320/480 MB (`TTConfig.h`).
- Measurement infrastructure is strong: serial CPU-time and PMU ladders,
  `standard_performance.py`, `ab_*_us` phase timers, a frozen full alpha-mu
  baseline (`play_board3_depth2_w32` ≈ 22.4 s).
- Stage-5 micro-optimisation cycle is **closed**: of eight candidates
  (hot/cold split, `pos` reorder, `DepthLocal`, NEON helpers, CLZ,
  QuickTricks context struct …) only packed `moveType` survived. IPC is flat
  at ~3.4, so the M1 is not stalled; gains must come from *fewer
  instructions or fewer nodes*, not layout tweaks.
- Alpha-mu Stages 0–9 are closed; `alpha-mu-future-roadmap.md` holds deferred work.

**Not yet achieved (verified in the tree)**

- No CI: `.github/` contains only `copilot-instructions.md`.
- `using namespace std` is still in 17 headers.
- Files still over ~1,500 lines: `Moves.cpp` (2,928), `TransTableL.cpp`
  (1,847), `Par.cpp` (1,599), `alpha_mu_bridge.cpp` (1,532),
  `alpha_mu_worlds.cpp` (1,475); `alpha_mu_core.h` is 2,617 lines.
- Core is still `-std=c++11` (`src/Makefile`, `test/Makefiles/*`);
  `modernisation-plan.md` still states this as a constraint.
- Build artefacts (`src/build-*`, `test/build-*`, `cmake-build-debug`,
  `__pycache__`, `test/*stats*.txt`, `test/timer*.txt`) live in the source
  tree; ensure they are git-ignored.
- `build-cpp23/` and `build-cpp2c/` experiments exist but **no result is
  recorded** in `performance-log.md`. Record or delete them.

## Phase 1 — Safety and correctness

| Item | Status |
|---|---|
| `ConvertFromPBN` wrote outside `remainCards` for >4 hands/suits and could read past a short string | **[done]** `src/PBN.cpp` |
| `ThreadMgr::Occupy` resized `machineThreads` outside the mutex; `Release` was unchecked | **[done]** `src/ThreadMgr.cpp` |
| `paramType::error` and the `err` flags in the STL/PPL runners were written by workers without synchronisation | **[done]** now `std::atomic` |
| Global `mtx`/`mtxPrint` had external linkage | **[done]** now `static` |
| Library `exit(1)` calls (`Memory.cpp`, `Moves.cpp`, `TransTableL.cpp`, `TransTableS.cpp`) | **[done]** now throw `bad_alloc`/`out_of_range`/`logic_error`; `SolveBoard`, `AnalysePlayBin` and the table worker catch and return `RETURN_UNKNOWN_FAULT` |
| Serialise batch runs, `SetResources`, `SetThreading` and `FreeMemory` with `ddsApiMutex` (recursive) | **[done]** single-board `SolveBoard` calls stay lock-free |
| Bounded string writes in `Par.cpp`, `DealerPar.cpp` | **[done]**; also fixed `ConvertToDealerTextFormat`, which used `sizeof(pointer)` as the buffer size and truncated its output |
| Deterministic error selection (lowest board index) for batch runs | **[done]** `RecordRunError` in `dds.h` |
| Alpha-mu: process-exiting error paths | **[done]** none found in `src/alpha_mu_*` or `include/alpha_mu` |

Follow-ups completed (2026-11-17):

- `strcpy` is gone from `src/`. All fixed-size copies use the truncating,
  always-NUL-terminated helpers in `src/SafeCopy.h` (`Init.cpp`,
  `System.cpp`, `alpha_mu_bridge.cpp`, `alpha_mu_support.cpp`).
- `thrIdNext` is documented at both declarations in `System.cpp`: it must
  never be reset per run (ids are cached in `thread_local`).
- `TransTable*` exceptions are exercised by a failure-injection test
  (`test/transtable_failure`, hooks in `test/alloc_fail.h`) that fails every
  allocation in `MakeTT` and the first-page allocations in `Lookup`. It found
  and fixed real bugs: uninitialised members read by the destructor after a
  partial `MakeTT`, `TTInUse` left set after failure (retry crashed), a
  leaked `pl[k][h]` array, and a half-built pool in `GetNextCardBlock`.

## Phase 2 — Verification infrastructure

Progress (2026-11-17):

- **Coverage tooling [done]:** `make coverage-check` / `make coverage-report`
  build with clang source-based coverage and print per-file line coverage.
  It runs `regression_api ../hands/list1.txt` plus `transtable_failure`
  (`thomas1` is too slow under instrumentation).
- **Baseline: 62% lines, 52% functions, 61% branches** over `src/*.cpp`.
  Biggest deficits: `Par.cpp` 49%, `SolveBoard.cpp` 48% (batch de-duplication,
  chunk and limit paths), `ThreadMgr.cpp` 20%, `TransTable*.cpp` ~35-40%
  (mixed with the failure-test copies), `dump.cpp` 29%, `DealerPar.cpp` 61%,
  `Moves.cpp` 70%; timer/stat files are 0% because the timers are compiled out.
- **Batch tests [done]:** `CheckBatchVariants` in `regression_api.cpp` covers
  duplicate boards, `SolveAllChunks`/`SolveAllChunksPBN`, `RETURN_CHUNK_SIZE`
  and `RETURN_TOO_MANY_BOARDS`.
- **Still to do:** Par/DealerPar vulnerability sweeps, multi-thread
  `ThreadMgr` tests (run with `SetResources(0, n)`), and CI.
- **thomas1 investigation (resolved, not a regression):** one
  `CalcAllTables` on `thomas1.txt` takes ~126 s on one M1 Max core, with the
  May 2026 library (`build-cpp23`) and with the current one alike (131 s), and
  does not speed up with more threads (one deal, one dominant strain). The
  deal is cyclically symmetric, so the transposition table helps little. A
  single `SolveBoard` takes ~2.5 s. `regression_api` runs the table several
  times (par check, batch, repeated calls), so the full file costs 10+ min.
  Third-party claims of 1-3 s do not apply to this file. `make check` now
  uses `list10.txt` only (~10 s); `make check-slow` adds thomas1.
  `regression_api` prints per-phase timings to stderr. Verified:
  `regression_api ../hands/thomas1.txt` passes ("regression_api: OK") in
  **44 min 51 s** wall (44 min 38 s CPU) on an M1 Max, about 20 table
  computations at ~130 s each (the repeated-call check runs 3 thread settings
  x 3 repeats x 2 tables). Consider trimming repeats for slow hands.
1. Add malformed-input tests to `test/regression_api.cpp`: extra hands/suits,
   truncated strings **[partly done]**; illegal ranks, duplicate cards and
   `ConvertPlayFromPBN` cases remain.
2. Add CI (`.github/workflows`): Linux + macOS build, `make check`, and the
   ASan/UBSan/TSan targets that already exist in the root `Makefile`.
   Add `transtable_failure` to these lanes.
3. Differential tests of alpha-beta shortcuts (quick/later tricks, TT) against
   a pruning-disabled baseline on `hands/list10.txt`.
4. Repeated-call and concurrent-call tests (after the API mutex lands).
   Repeated calls exist; concurrent calls remain.
5. Fuzz `ConvertFromPBN` and `ConvertPlayFromPBN` under ASan.

## Phase 3 — Performance (benchmark-gated)

- Follow `alpha-mu-benchmark-baseline.md`; record every result in
  `performance-log.md`.
- Only target the measured hotspots: the straight-line body of `ABsearch*`
  (`ab_other_us`) and iteration control.
- Each change must pass `make perf-check` and show no regression in
  `make perf-bench`. Keep the generic (non-Apple) fallback.
- Candidates: PGO for non-Apple builds; TT sizing review
  (`tt-sizing-benchmark.md`); avoiding allocation in the alpha-mu recursion.

### Suggested priorities (highest expected payoff first)

1. **Correctness of the release flags.** `-ffast-math` is in the Apple
   release profile. DDS is almost entirely integer code, so it probably buys
   nothing, yet it permits reassociation/NaN assumptions in the alpha-mu
   weighting and par code. Bisect the 2026-05-12 bundle (`-mcpu`, `-ffast-math`,
   `-funroll-loops`, `-fstrict-aliasing`); keep only flags that measure
   a win, and drop `-ffast-math` unless it does. Add a bit-exact result
   comparison (`dtest` + `regression_api` on `list1000`) to the gate.
2. **Algorithmic work beats micro-tuning.** Seven of eight micro-experiments
   failed. Remaining measured hotspots are `ab_other_us` (60%) and iteration
   control (30%) inside `ABsearch*`; target *node count* (move ordering,
   TT replacement/hit rate, QuickTricks/LaterTricks cut rates) using the
   existing `DDS_TIMING`/TT statistics. Every node avoided is worth more than
   any layout change. Differential tests (Phase 2.3) make this safe.
3. **Alpha-mu: reduce DDS leaf work.** On the frozen baseline 73% of time is
   DDS leaves and 24% world generation. Ideas: memoise leaf results keyed on
   the rank-equivalence-normalised position; batch leaves through the
   scheduler with shared TTs per deal; generate worlds lazily/incrementally
   instead of enumerating 184,756 and sampling 32; then parallel tree search
   (roadmap §7).
4. **Threading on heterogeneous cores.** M1 Max has 8 P + 2 E cores. Default
   worker count to `hw.perflevel0.logicalcpu` (P-cores) and keep QoS
   `USER_INTERACTIVE`; E-core workers lengthen the critical path of
   board-parallel runs (the 20–30% wall-clock variance seen in the logs).
5. **PGO/LTO retest.** The only PGO data point (2026-04-18) was one
   workload, 2.9% behind non-PGO. Retest with multi-workload training
   (`list10`, `list1000`, alpha-mu), repeated medians, and ThinLTO vs full
   LTO. If it wins, use it for release packaging on both macOS and Linux.
6. **Gate every change on instructions + cycles**, not wall time
   (`pmu_single_run`); wall-clock noise has repeatedly hidden ±15% effects.

### Compiler, language-standard and architecture recommendations

| Option | Expected performance | Recommendation |
|---|---|---|
| C++11 → **C++20** (C++17 minimum) | Neutral; the standard does not change codegen | **Do it** for idiom: `std::array`, `constexpr` tables, `[[nodiscard]]`, `std::span`, `std::popcount`, structured bindings. Keep the C ABI in `dll.h`. Gate with the ladder, as LTO/inlining decisions have shifted before. Record the existing `build-cpp23` result first. |
| Newest Homebrew/upstream LLVM vs Apple clang 21 | Possibly a few %; inlining heuristics differ | Cheap to test: same flags, compare PMU ladder. Adopt only if it wins and stays reproducible. |
| `-mcpu=apple-m1` → `-mcpu=native` / newer Apple cores | Likely ≤ 2%; loses portability to M1 | Test for local builds only; ship `apple-m1` as the portable floor. |
| x86-64: `-march=x86-64-v3` (BMI2/AVX2/POPCNT) | Plausible gain for bit-mask code (`WorldMask`, suit masks) | Provide a v3 build with runtime dispatch or a separate artefact; keep the baseline build. |
| Linux arm64 (Graviton/Neoverse): `-mcpu=neoverse-n1` | Similar to Apple path | Add a generic AArch64 profile; the NEON helpers gave no win, so do not add more. |
| Linux PGO + BOLT | BOLT often 5–15% on branchy code | Linux-only experiment; no macOS equivalent. |
| GCC vs clang on Linux | Unknown | One-off comparison; keep clang as primary. |

A change of **architecture** (e.g. GPU, SIMD rewrite) is not justified: the
search is branchy and pointer-chasing with IPC already ≈ 3.4. The one
architectural lever with real upside is the algorithmic/threading work above.

## Phase 4 — Readability, idiom, documentation

1. Remove `using namespace std` from the 17 headers that have it.
2. Prefer RAII over raw owning pointers where the C ABI is not involved.
3. Split files over ~1,500 lines (`Moves.cpp`, `Par.cpp`, `TransTableL.cpp`,
   `alpha_mu_bridge.cpp`, `alpha_mu_worlds.cpp`, `alpha_mu_support.cpp`).
4. Drop `-Wno-unused`; add `clang-tidy` and `clang-format` configuration.
5. Finish Doxygen coverage of `include/dll.h` and core headers; add a short
   solver invariants guide.
6. Unify Makefile and CMake behaviour; untrack generated files such as
   `test/dump.txt`.

## Phase 5 — Suggested sequencing

1. Housekeeping (no risk): git-ignore build artefacts; record or delete
   `build-cpp23`/`build-cpp2c`; update `modernisation-plan.md` constraints.
2. CI + sanitiser lanes + malformed-input and differential tests (Phase 2)
   — this is the safety net for everything below.
3. Flag bisect (`-ffast-math` etc.) and LLVM/PGO retest (Phase 3 items 1, 5).
4. Move to C++20 behind the PMU gate; then Phase 4 cleanups
   (`using namespace std`, file splits) in small, separately benchmarked commits.
5. Algorithmic search work and alpha-mu leaf caching/lazy worlds (Phase 3
   items 2–3); P-core default threading (item 4).
6. Only then revisit deferred alpha-mu roadmap items (64+ worlds, parallel tree).

## Exit criteria

- `make check` and `make perf-check` green on macOS arm64 and Linux.
- Sanitiser lanes clean.
- No benchmark regression versus the recorded baseline.
- No `exit()` or unsynchronised shared state in library code.
