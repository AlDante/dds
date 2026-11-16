# DDS / alpha-mu Improvement Plan

Priorities, in order: **1. correctness, 2. performance, 3. readability,
idiom and documentation**. This plan complements
`code-audit-recommendations.md` (detailed rationale) and
`optimisation-plan.md` (measured performance outcomes). Items marked
**[done]** have been implemented; verify with `make check`.

## Phase 1 — Safety and correctness

| Item | Status |
|---|---|
| `ConvertFromPBN` wrote outside `remainCards` for >4 hands/suits and could read past a short string | **[done]** `src/PBN.cpp` |
| `ThreadMgr::Occupy` resized `machineThreads` outside the mutex; `Release` was unchecked | **[done]** `src/ThreadMgr.cpp` |
| `paramType::error` and the `err` flags in the STL/PPL runners were written by workers without synchronisation | **[done]** now `std::atomic` |
| Global `mtx`/`mtxPrint` had external linkage | **[done]** now `static` |
| Replace library `exit(1)` calls (`Memory.cpp`, `Moves.cpp`, `TransTableL.cpp`, `TransTableS.cpp`) with error returns | open |
| Serialise top-level API entry points (global `param`, `cparam`, `playparam`, `sysdep`, `memory`, `scheduler`) with one mutex, and document the contract | open |
| Replace `strcat`/`strcpy` in `Par.cpp`, `DealerPar.cpp` with bounded writes | open |
| Deterministic error selection (lowest board index) for batch runs | open |
| Alpha-mu: replace process-exiting error paths with returnable errors | open |

Note: `thrIdNext` in `System.cpp` must **not** be reset per run. Thread ids
are cached in `thread_local` storage, so resetting would hand two threads the
same id. Document this next to the declaration.

## Phase 2 — Verification infrastructure

1. Add malformed-input tests to `test/regression_api.cpp`: extra hands/suits,
   truncated strings, illegal ranks, duplicate cards.
2. Add CI (`.github/workflows`): Linux + macOS build, `make check`, and the
   ASan/UBSan/TSan targets that already exist in the root `Makefile`.
3. Differential tests of alpha-beta shortcuts (quick/later tricks, TT) against
   a pruning-disabled baseline on `hands/list10.txt`.
4. Repeated-call and concurrent-call tests (after the API mutex lands).
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

## Exit criteria

- `make check` and `make perf-check` green on macOS arm64 and Linux.
- Sanitiser lanes clean.
- No benchmark regression versus the recorded baseline.
- No `exit()` or unsynchronised shared state in library code.
