# Alpha-Mu Solver Runner

This file documents the standalone alpha-mu solver runner added as a separate test component.

## Goal

The current runner is intentionally narrow.

It is meant to validate the core paper semantics before deeper optimization work:

- world masks,
- a first possible-world generator from simple constraints, including a first scoped auction-side contract where an external analysis component supplies HCP ranges, hand types, and minimum/maximum suit lengths, plus first partnership suit-length range and partnership HCP-range bidding constraints,
- a first seed-based hidden-seat world constructor driven by recorded play/current-trick card ownership plus constructor-local explicit known-card and bidding card-location pruning, suit-length, partnership suit-length range, partnership HCP-range, HCP, balanced-shape, and follow-suit-derived length pruning, including partially specified visible-hand seeds whose missing hidden cards are inferred from the full-deck complement, moderately larger two-defender pools that can then be deterministically downselected, and longer multi-trick visible-seed histories whose broader pools are narrowed by play-derived evidence before sampling,
- staged possible-world filtering with explicit known-card, cannot-hold-card, bidding-style (including seat-level HCP ranges, hand types, minimum/maximum suit lengths, partnership HCP-range, and partnership suit-length ranges), explicit follow-suit implication, and play-history constraints,
- deterministic seed-based world downselection after staged filtering,
- per-world explanation traces showing why each candidate world is accepted or rejected,
- constructor-local world-construction accounting and explanation traces showing how raw assignment pools are narrowed at card-location, length, HCP, and balanced checkpoints before later staged filtering,
- outcome vectors,
- Pareto fronts,
- a Pareto-front transposition table for exact toy-search reuse,
- bridge move generation over possible worlds,
- multi-trick bridge search control over generated move trees,
- bridge root reporting for move-front and surviving-world summaries,
- Max-node union,
- Min-node product/min combination,
- early cut,
- root cut,
- empty-entry handling for interior fronts,
- optimistic values for impossible and useless worlds,
- deep alpha cuts,
- leaf parallelization of DDS-backed leaf evaluation,
- DDS-backed leaf evaluation on a small curated world set,
- DDS-backed bridge leaf evaluation at the end of generated bridge continuations.

## Files

- `test/alpha_mu.cpp` — thin CLI runner / mode dispatcher
- `test/alpha_mu_core.h` — shared types and callable alpha-mu API
- `test/alpha_mu_front.cpp` — Pareto-front/outcome-vector helpers and the paper-faithful toy alpha-mu search harness
- `test/alpha_mu_worlds.cpp` — information-state, world-construction, and world-filtering helpers
- `test/alpha_mu_bridge.cpp` — bridge-state assembly, play-history parsing, move legality/transition, DDS-leaf handoff, and bridge search/reporting helpers
- `test/alpha_mu_core.cpp` — TT, benchmark, and DDS-side shared core support
- `test/alpha_mu_decision.cpp` — decision-point solve assembly and DDS/actual-play comparison helpers
- `test/alpha_mu_reporting.cpp` — benchmark, comparison, and solve-result formatting/reporting helpers
- `test/alpha_mu_tests.cpp` — regression suite and grouped test runners

## What it checks

The runner performs these checks:

1. Pareto-front insertion and domination reduction
2. the paper's non-locality-style toy example
3. an early-cut toy example
4. useful-world maintenance at a Min node
5. world cuts for zero and single useful worlds
6. possible-world generation from seed-based hidden-seat construction plus constructor-local known-card/cannot-hold-card and bidding-style pruning, staged known-card, cannot-hold-card, bidding-style including seat-level HCP ranges, hand types, minimum/maximum suit lengths, partnership suit-length ranges, and partnership HCP-range filters, constructor-local and staged follow-suit implications, play-history, deduplicated-world filtering, deterministic downselection, and per-world accept/reject explanations
7. bridge move generation over those possible worlds
8. bridge search control with trick completion, winner advancement, and multi-trick continuation
9. bridge root reporting over a larger three-world, two-trick continuation with mixed merge/split front behavior
10. DDS-backed bridge leaf evaluation after searched bridge continuations, including a targeted multi-world sparse-front continuation case
11. empty-entry handling for interior fronts
12. optimistic completion of impossible worlds for cross-state comparison
13. a Pareto-front transposition table hit on a repeated exact subtree
14. deep alpha cuts against earlier Max ancestors
15. cut on win at a Max node
16. a root-cut toy example with iterative deepening
17. serial and parallel DDS leaf-evaluation over `hands/alpha_mu_play.txt`

## Build

From `test/`:

```zsh
make -f Makefiles/Makefile_Mac_clang alpha_mu
```

## Run

From `test/`:

```zsh
DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu
```

Target just the bridge DDS continuation regression bundle:

```zsh
DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu bridge_dds
```

## Scope limits

This is not yet a full bridge alpha-mu engine.

In particular, it does not yet include:

- full-scale possible-world generation from complete bidding or play histories,
- broad bridge-search horizons beyond the current small multi-trick engine slice.

Useful-world maintenance, world cuts, empty-entry handling, optimistic impossible-world completion, deep alpha cuts, cut on win, DDS leaf parallelization, a staged constraint-based possible-world generator with explicit cannot-hold constraints plus a first scoped auction-side contract for seat-level HCP ranges, hand types, minimum/maximum suit lengths, partnership suit-length ranges, and partnership HCP-range bidding filters, explicit follow-suit implications derived from discard history, seed-based hidden-seat construction from partial-information worlds using recorded played-card ownership plus constructor-local explicit known-card and bidding card-location pruning, suit-length, hand-type, partnership suit-length range, partnership HCP-range, seat-level HCP, balanced-shape, and follow-suit-derived length pruning, partially specified visible-hand seeds whose hidden-card pool is inferred from the full-deck complement, moderately larger two-defender visible-seed pools with deterministic downselection after staged filtering, longer multi-trick visible-seed histories whose broader pools are narrowed by play-derived follow-suit evidence before any sampling stage, constructor-local world-construction accounting plus explanation traces for card-location, length, HCP, and balanced narrowing, per-world world-generation explanation traces, play-history legality filtering, deduplication, and deterministic seed-based downselection, a first bridge move generator, multi-trick bridge search control with bridge-specific trick backup, bridge root reporting over a larger three-world continuation, DDS-backed bridge leaf evaluation, and a Pareto-front transposition table are now present in the solver.

The targeted `bridge_dds` mode now checks:

- direct DDS bridge-leaf evaluation on a real hand-file world,
- one searched trick followed by DDS leaf handoff on that real world,
- direct DDS bridge-leaf evaluation on a controlled multi-world partial-trick state,
- a controlled two-world continuation that preserves sparse exact-score fronts after a deeper searched bridge continuation,
- and a controlled three-world continuation with one merged DDS-backed root branch and three split sparse branches after one searched trick.

The next planned non-performance step is extending this bridge-search control beyond the current first larger three-world continuation case to deeper mixed merge/split continuations and from these richer visible-seed histories toward broader realistic possible-world generation, while continuing the `Workstream 5` split of reporting and world/state helpers out of the oversized core implementation unit.

## DDS vs alpha-mu comparison mode

For broad comparisons, use the dedicated runner from the repository root:

```zsh
python3 test/alpha_mu_dds_compare.py
```

For lower-level spot checks, the solver also exposes exact one-world benchmark modes:

```zsh
DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu benchmark_dds hands/list100.txt 0
DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu benchmark_alpha hands/list100.txt 1 0
```

and a convenience combined report mode:

```zsh
DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu compare_dds hands/list100.txt 1 0
```

Arguments for `benchmark_dds` are:

1. hand-file path,
2. maximum boards to test (`0` means all boards in the file).

Arguments for `benchmark_alpha` are:

1. hand-file path,
2. maximum alpha-mu search depth,
3. maximum boards to test (`0` means all boards in the file).

The low-level modes and the runner:

- times direct DDS solves over the selected boards,
- times one-world alpha-mu solves over the same boards at depths `0..max_depth`,
- checks that every alpha-mu depth preserves the exact DDS score on every board,
- validate against the hand-file FUT goldens,
- and print machine-readable `ALPHA_MU_BENCHMARK ...` or `ALPHA_MU_COMPARE ...` lines for higher-level comparison runners.

