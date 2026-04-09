# Alpha-Mu and DDS

## What alpha-mu means here

In the DDS context, alpha-mu should be understood as an **interval / threshold-oriented root search policy**.

Instead of rethinking the whole solver as a score-returning engine, the practical question is:

- DDS already knows how to answer "can this side force at least `target` tricks from here?"
- how can the root use that capability more efficiently when it wants the exact optimum?

That is why the recommended path starts in `src/SolverIF.cpp`.

## Why DDS is a good fit

DDS already has important ingredients that align with alpha-mu ideas:

- threshold-style recursive search in the deep solver,
- explicit lower/upper bound storage in transposition-table entries,
- strong domain-specific pruning via `QuickTricks` and `LaterTricks`,
- separate root orchestration code that already performs repeated probes.

This means alpha-mu integration can be evolutionary rather than revolutionary.

## What should stay stable initially

The following components are high value and already deeply integrated:

- `src/Moves.cpp`
- `src/QuickTricks.cpp`
- `src/LaterTricks.cpp`
- the four specialized `ABsearch*()` functions
- TT storage semantics based on **remaining tricks from the node**

These should not be rewritten in the first phase.

## Where alpha-mu should begin

The first practical insertion point is the **root exact-score logic** in `src/SolverIF.cpp`.

That code already maintains concepts such as:

- guess,
- lower bound,
- upper bound,
- repeated threshold probes.

A clean alpha-mu-style refactor can centralize that logic into a helper while keeping the existing recursive proof engine intact.

## Why not start inside `ABsearch0()`?

Because that would combine several risks at once:

- recursive control flow changes,
- TT interaction changes,
- move-order / proof interaction changes,
- benchmark and correctness uncertainty.

The project already has a strong threshold-search identity. The safest first step is to improve **root policy**, not deep search semantics.

## Constraints for a correct DDS integration

A good alpha-mu integration should preserve the following invariants:

1. **Public results must remain unchanged**.
2. **Move generation and pruning logic must remain valid**.
3. **TT bounds must keep their current meaning**.
4. **Repeated solves / play-analysis hint paths must still work**.
5. **Performance claims must be benchmark-backed**.

## Practical success criteria

A successful first alpha-mu step will:

- reduce duplicated root probing code,
- keep all current regression tests passing,
- leave deep recursion untouched,
- provide a clean place for later experimentation with guess refinement and interval management.
