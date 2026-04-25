# Alpha-Mu and DDS

## What alpha-mu means here

After reviewing the original paper and the optimization paper, there are **two different things** that need to be distinguished clearly:

1. **Alpha-mu proper**
   - an imperfect-information search for declarer play,
   - over a set of sampled possible worlds,
   - using outcome vectors and Pareto fronts,
   - with different backup rules at Max and Min nodes,
   - plus alpha-mu-specific cuts and transposition-table behavior.
2. **DDS-side support work inspired by alpha-mu**
   - improving how DDS answers exact-score threshold queries,
   - measuring and tuning repeated root probes,
   - preparing DDS to be a stronger leaf oracle or subroutine for a later alpha-mu engine.

Our current work is in the **second category**.

That means the current `src/SolverIF.cpp` work is valuable, but it should not be confused with a full implementation of the paper's algorithm.

## Why DDS is still a good fit

DDS already has important ingredients that make it a strong building block for alpha-mu experiments:

- threshold-style recursive search in the deep solver,
- explicit lower/upper bound storage in transposition-table entries,
- strong domain-specific pruning via `QuickTricks` and `LaterTricks`,
- separate root orchestration code that already performs repeated probes,
- battle-tested exact evaluation for perfect-information leaf states.

This means DDS is a natural **leaf evaluator and exact subsolver** for alpha-mu.

What DDS does **not** already contain is the central machinery from the papers:

- sets of possible worlds,
- outcome vectors over those worlds,
- Pareto-front maintenance,
- Max-node union and Min-node product/min combination,
- useful-world tracking,
- world cuts,
- cut-on-win,
- deep alpha cuts,
- a transposition table that stores Pareto fronts rather than scalar bounds.

## What should stay stable initially

The following components are high value and already deeply integrated:

- `src/Moves.cpp`
- `src/QuickTricks.cpp`
- `src/LaterTricks.cpp`
- the four specialized `ABsearch*()` functions
- TT storage semantics based on **remaining tricks from the node**

These should not be rewritten in the first phase.

## Where the DDS-side work should begin

The first practical insertion point for DDS-side support work is the **root exact-score logic** in `src/SolverIF.cpp`.

That code already maintains concepts such as:

- guess,
- lower bound,
- upper bound,
- repeated threshold probes.

A clean refactor can centralize that logic into a helper while keeping the existing recursive proof engine intact.

## Why not start inside `ABsearch0()`?

For DDS-side support work, changing `ABsearch*()` first would combine several risks at once:

- recursive control flow changes,
- TT interaction changes,
- move-order / proof interaction changes,
- benchmark and correctness uncertainty.

The current solver already has a strong threshold-search identity. The safest first step was therefore to improve **root policy**, not deep search semantics.

However, for **alpha-mu proper**, the papers point to a different insertion point: a new imperfect-information search layer built *around* DDS, not a quiet reinterpretation of the existing DDS recursion as if it were already alpha-mu.

## What has been implemented so far

The work completed so far should be viewed as **DDS groundwork**:

- a shared exact-score root helper in `src/SolverIF.cpp`,
- measurement-only instrumentation of root probing behavior,
- a benchmark runner for repeat solves and play analysis,
- a first repeat-solve guess-seeding experiment.

This is useful because any future alpha-mu engine will still depend on fast and measurable DDS evaluations.

It is not yet a paper-faithful alpha-mu search.

## Constraints for a correct DDS integration

A good DDS-side alpha-mu support path should preserve the following invariants:

1. **Public results must remain unchanged**.
2. **Move generation and pruning logic must remain valid**.
3. **TT bounds must keep their current meaning**.
4. **Repeated solves / play-analysis hint paths must still work**.
5. **Performance claims must be benchmark-backed**.

## Practical consequence for planning

The roadmap now has to split into two tracks:

1. **Continue modest DDS root-policy improvements** where they pay off.
2. **Start a separate alpha-mu implementation track** for the actual paper algorithm:
   - possible worlds,
   - vectors,
   - Pareto fronts,
   - Max/Min front operations,
   - alpha-mu cuts,
   - DDS leaf evaluation.

## Practical success criteria

A successful DDS-side support step will:

- reduce duplicated root probing code,
- keep all current regression tests passing,
- leave deep recursion untouched,
- provide a clean place for later experimentation with guess refinement and interval management.

A successful **first real alpha-mu step** will additionally:

- reproduce the paper's search semantics on a small, controlled implementation slice,
- run over multiple worlds rather than one perfect-information deal,
- maintain Pareto fronts correctly,
- demonstrate at least early/root-cut behavior before more aggressive optimizations are attempted.

