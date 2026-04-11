# Alpha-Mu Prototype Runner

This file documents the first minimal alpha-mu prototype added as a separate test component.

## Goal

The prototype is intentionally narrow.

It is meant to validate the core paper semantics before deeper optimization work:

- world masks,
- outcome vectors,
- Pareto fronts,
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
6. empty-entry handling for interior fronts
7. optimistic completion of impossible worlds for cross-state comparison
8. deep alpha cuts against earlier Max ancestors
9. cut on win at a Max node
10. a root-cut toy example with iterative deepening
11. serial and parallel DDS leaf-evaluation over `hands/alpha_mu_play.txt`

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

- possible-world generation from bidding or play constraints,
- bridge move generation inside the alpha-mu search itself,
- a Pareto-front transposition table.

Useful-world maintenance, world cuts, empty-entry handling, optimistic impossible-world completion, deep alpha cuts, cut on win, and DDS leaf parallelization are now present in the prototype.

The next planned non-performance step is possible-world generation from bidding or play constraints.

