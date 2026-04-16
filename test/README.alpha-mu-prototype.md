# Alpha-Mu Prototype Runner

This file documents the first minimal alpha-mu prototype added as a separate test component.

## Goal

The prototype is intentionally narrow.

It is meant to validate the core paper semantics before deeper optimization work:

- world masks,
- a first possible-world generator from simple constraints,
- a first seed-based hidden-seat world constructor driven by recorded play/current-trick card ownership plus constructor-local bidding card-location, suit-length, HCP, balanced-shape, and follow-suit-derived length pruning,
- staged possible-world filtering with explicit known-card, cannot-hold-card, bidding-style (including HCP and balanced-shape), explicit follow-suit implication, and play-history constraints,
- deterministic seed-based world downselection after staged filtering,
- per-world explanation traces showing why each candidate world is accepted or rejected,
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

## File

- `test/alpha_mu_prototype.cpp`

## What it checks

The runner performs these checks:

1. Pareto-front insertion and domination reduction
2. the paper's non-locality-style toy example
3. an early-cut toy example
4. useful-world maintenance at a Min node
5. world cuts for zero and single useful worlds
6. possible-world generation from seed-based hidden-seat construction plus staged known-card, cannot-hold-card, bidding-style including HCP/balanced-shape filters, constructor-local and staged follow-suit implications, play-history, deduplicated-world filtering, deterministic downselection, and per-world accept/reject explanations
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
make -f Makefiles/Makefile_Mac_clang alpha_mu_prototype
```

## Run

From `test/`:

```zsh
DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu_prototype
```

Target just the bridge DDS continuation regression bundle:

```zsh
DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu_prototype bridge_dds
```

## Scope limits

This is not yet a full bridge alpha-mu engine.

In particular, it does not yet include:

- full-scale possible-world generation from complete bidding or play histories,
- broad bridge-search horizons beyond the current small multi-trick prototype.

Useful-world maintenance, world cuts, empty-entry handling, optimistic impossible-world completion, deep alpha cuts, cut on win, DDS leaf parallelization, a staged constraint-based possible-world generator with explicit cannot-hold constraints plus HCP/balanced-shape bidding filters, explicit follow-suit implications derived from discard history, seed-based hidden-seat construction from partial-information worlds using recorded played-card ownership plus constructor-local bidding card-location, suit-length, HCP, balanced-shape, and follow-suit-derived length pruning, per-world world-generation explanation traces, play-history legality filtering, deduplication, and deterministic seed-based downselection, a first bridge move generator, multi-trick bridge search control with bridge-specific trick backup, bridge root reporting over a larger three-world continuation, DDS-backed bridge leaf evaluation, and a Pareto-front transposition table are now present in the prototype.

The targeted `bridge_dds` mode now checks:

- direct DDS bridge-leaf evaluation on a real hand-file world,
- one searched trick followed by DDS leaf handoff on that real world,
- direct DDS bridge-leaf evaluation on a controlled multi-world partial-trick state,
- a controlled two-world continuation that preserves sparse exact-score fronts after a deeper searched bridge continuation,
- and a controlled three-world continuation with one merged DDS-backed root branch and three split sparse branches after one searched trick.

The next planned non-performance step is extending this bridge-search control beyond the current first larger three-world continuation case to deeper mixed merge/split continuations and richer possible-world generation from more realistic histories.

## DDS vs alpha-mu comparison mode

For broad comparisons, use the dedicated runner from the repository root:

```zsh
python3 test/alpha_mu_dds_compare.py
```

For lower-level spot checks, the prototype also exposes exact one-world benchmark modes:

```zsh
DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu_prototype benchmark_dds hands/list100.txt 0
DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu_prototype benchmark_alpha hands/list100.txt 1 0
```

and a convenience combined report mode:

```zsh
DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu_prototype compare_dds hands/list100.txt 1 0
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

