# Alpha-Mu Prototype Runner

This file documents the first minimal alpha-mu prototype added as a separate test component.

## Goal

The prototype is intentionally narrow.

It is meant to validate the core paper semantics before deeper optimization work:

- world masks,
- a first possible-world generator from simple constraints,
- outcome vectors,
- Pareto fronts,
- a Pareto-front transposition table for exact toy-search reuse,
- bridge move generation over possible worlds,
- multi-trick bridge search control over generated move trees,
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
6. possible-world generation from simple bidding-style and play-style constraints
7. bridge move generation over those possible worlds
8. bridge search control with trick completion, winner advancement, and multi-trick continuation
9. DDS-backed bridge leaf evaluation after searched bridge continuations, including a targeted multi-world sparse-front continuation case
10. empty-entry handling for interior fronts
11. optimistic completion of impossible worlds for cross-state comparison
12. a Pareto-front transposition table hit on a repeated exact subtree
13. deep alpha cuts against earlier Max ancestors
14. cut on win at a Max node
15. a root-cut toy example with iterative deepening
16. serial and parallel DDS leaf-evaluation over `hands/alpha_mu_play.txt`

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

Useful-world maintenance, world cuts, empty-entry handling, optimistic impossible-world completion, deep alpha cuts, cut on win, DDS leaf parallelization, a first constraint-based possible-world generator, a first bridge move generator, multi-trick bridge search control with bridge-specific trick backup, DDS-backed bridge leaf evaluation, and a Pareto-front transposition table are now present in the prototype.

The targeted `bridge_dds` mode now checks:

- direct DDS bridge-leaf evaluation on a real hand-file world,
- one searched trick followed by DDS leaf handoff on that real world,
- direct DDS bridge-leaf evaluation on a controlled multi-world partial-trick state,
- and a controlled two-world continuation that preserves sparse exact-score fronts after a deeper searched bridge continuation.

The next planned non-performance step is extending this bridge-search control from the current small multi-trick continuation model to larger bridge continuations and richer possible-world generation.

