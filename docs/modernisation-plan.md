# Code Modernisation Plan

Date: 2026-04-19

## Scope

This plan covers the DDS core library (`src/`) and the alpha-mu solver code (`test/alpha_mu_*`).

## Phase 1: Alpha-mu solver modernisation (low risk, high readability gain)

1. **Replace `NULL` with `nullptr`** throughout `alpha_mu_core.h/.cpp`
2. **Use `__builtin_popcountll`** in `WorldMask::PopCount()` instead of manual bit loop
3. **Use range-based for loops** where iterating over vectors with index-only access
4. **Add `[[nodiscard]]`** to pure query functions returning important values
5. **Use `auto` for iterator declarations** in TranspositionTable::Lookup

## Phase 2: DDS core documentation (no code change risk)

1. Add doxygen-style comments to `dds.h` structures (`pos`, `moveType`, etc.)
2. Add brief documentation to `ABsearch.h` function declarations
3. Add brief documentation to `QuickTricks.h` function declarations
4. Document `Memory.h` structures (`ThreadData`, `ThreadDataHot`, etc.)

## Phase 3: DDS core modernisation (moderate risk, tested via regression)

1. **Replace `assert.h` with `<cassert>`** in ABsearch.cpp
2. **Use `std::array` where appropriate** for fixed-size arrays in new code paths
3. **Remove `using namespace std`** from `Memory.h` header (leaks into all includers)

## Phase 4: Testing and validation

1. Build library and test binaries via `make perf-build`
2. Run correctness suite via `make perf-check`
3. Run performance benchmark via `make perf-bench`
4. Record results in `docs/performance-log.md`

## Constraints

- No heredoc in scripts or console commands
- No performance regressions (verify with benchmark)
- C++11 compatibility for DDS core (Makefile uses `-std=c++11`)
- C++17 available in CMake builds only
- Must not break existing concurrency model (GCD + STL threading)

