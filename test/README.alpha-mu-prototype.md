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
- one-trick bridge search control over generated move trees,
- Max-node union,
- Min-node product/min combination,
- early cut,
- root cut,
- empty-entry handling for interior fronts,
- optimistic values for impossible and useless worlds,
- deep alpha cuts,
- leaf parallelization of DDS-backed leaf evaluation,
- DDS-backed leaf evaluation on a small curated world set.

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
8. one-trick bridge search control with trick completion and winner advancement
9. empty-entry handling for interior fronts
10. optimistic completion of impossible worlds for cross-state comparison
11. a Pareto-front transposition table hit on a repeated exact subtree
12. deep alpha cuts against earlier Max ancestors
13. cut on win at a Max node
14. a root-cut toy example with iterative deepening
15. serial and parallel DDS leaf-evaluation over `hands/alpha_mu_play.txt`

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

## Scope limits

This is not yet a full bridge alpha-mu engine.

In particular, it does not yet include:

- full-scale possible-world generation from complete bidding or play histories,
- multi-trick bridge search control beyond the current one-trick prototype,
- DDS-backed bridge leaf evaluation at the end of those generated bridge continuations.

Useful-world maintenance, world cuts, empty-entry handling, optimistic impossible-world completion, deep alpha cuts, cut on win, DDS leaf parallelization, a first constraint-based possible-world generator, a first bridge move generator, one-trick bridge search control with bridge-specific trick backup, and a Pareto-front transposition table are now present in the prototype.

The next planned non-performance step is extending this bridge-search control from a single trick to multi-trick continuations with DDS-backed leaf evaluation.

