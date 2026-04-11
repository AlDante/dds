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
- DDS-backed leaf evaluation on a small curated world set.

## File

- `test/alpha_mu_prototype.cpp`

## What it checks

The runner performs five checks:

1. Pareto-front insertion and domination reduction
2. the paper's non-locality-style toy example
3. an early-cut toy example
4. a root-cut toy example with iterative deepening
5. a DDS leaf-evaluation demo over `hands/alpha_mu_play.txt`

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
- useful-world maintenance,
- world cuts,
- cut on win,
- deep alpha cuts,
- a Pareto-front transposition table.

Those remain future implementation steps.

