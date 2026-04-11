# Alpha-Mu Concrete Action Plan

## Purpose

This document turns the staged roadmap in `implementation-plan.md` into the next concrete execution cycle.

After reading the papers, the practical conclusion is:

- the DDS root-policy work is useful groundwork,
- but the next real alpha-mu implementation step is to build a **paper-faithful prototype around DDS**, not just to keep tuning `src/SolverIF.cpp`.

## Immediate objectives

1. Preserve the current DDS-side benchmarked baseline.
2. Define a paper-derived alpha-mu test and benchmark set.
3. Implement a minimal alpha-mu prototype with correct search semantics.
4. Add only the first paper-level cuts before attempting further optimization.
5. Keep DDS-only root-policy tweaks as a secondary track.

## Step 1 — freeze the DDS support baseline

Keep the current DDS-side support workflow available and reproducible:

- `python3 test/alpha_mu_benchmark.py`
- `test/build/regression_api`
- `test/build/dtest -f ../hands/list10.txt -s solve`
- `test/build/play_analysis_benchmark`

Record:

- pass/fail status,
- wall-clock time,
- probe-count summaries by context,
- whether repeated solves show stable exact-score behavior.

This step is not the new algorithmic target. It is the baseline we should keep so later alpha-mu work can still rely on a measurable DDS oracle.

## Step 2 — define the paper-derived alpha-mu test set

Before writing the prototype, lock down the test material that reflects the two papers.

This repository now includes the first curated version of that material in `docs/alpha-mu-test-set.md` and the corresponding hand files under `hands/alpha_mu_*.txt`.

The first set should include:

1. **paper-motivated hand families**
   - strategy-fusion examples,
   - non-locality examples,
   - discovery-play style examples,
   - “rare bad event” examples.
2. **repository-controlled workloads**
   - the 3 play-analysis example hands from `examples/hands.cpp`,
   - a small repeat-solve family from `hands/list10.txt`,
   - control boards from `hands/thomas1.txt` and `hands/thomas2.txt`.
3. **success metrics**
   - correctness of front operations,
   - stability of chosen move,
   - number of worlds,
   - root depth / Max-move horizon,
   - elapsed time,
   - alpha-mu cut activity once cuts are added.

## Step 3 — implement the minimum viable alpha-mu prototype

Do this as a **new component**, not as a rewrite of `src/ABsearch.cpp`.

Recommended first scope:

- a fixed small number of worlds,
- a bounded number of Max moves,
- DDS used as the leaf evaluator,
- single-threaded,
- correctness-first data structures.

The first implementation pieces should be:

1. `WorldMask` or equivalent valid-world tracking,
2. `OutcomeVector`,
3. `ParetoFront`,
4. dominance tests,
5. Max-node front union and reduction,
6. Min-node front product/min and reduction,
7. root iterative deepening in number of Max moves,
8. DDS leaf evaluation adapter.

The first minimal version of this now exists in `test/alpha_mu_prototype.cpp` with a dedicated runner target in `test/Makefiles/Makefile_Mac_clang`.

## Step 4 — add only the first paper-level cuts

The prototype should first support:

- **early cut**,
- **root cut**.

Do **not** start with the full optimization set from the second paper.

Those later optimizations depend on already having correct:

- front semantics,
- useful-world tracking,
- recursive control flow.

## Step 5 — verify the prototype before optimizing it

The prototype should be considered valid only if it can:

- pass deterministic front-operation checks,
- behave correctly on the small paper-derived examples,
- produce stable results on repeated runs,
- use DDS only as a leaf evaluator rather than duplicating DDS semantics internally.

## Step 6 — add the optimization-paper features in order

After the prototype is correct, the next implementation steps should be:

1. maintaining useful worlds,
2. world cuts,
3. cut on win,
4. empty-entry handling for interior fronts,
5. deep alpha cuts,
6. leaf parallelization,
7. only then low-level SIMD experiments if Pareto filtering becomes a measured bottleneck.

The first of these optimization-paper steps is now present in the prototype: useful-world maintenance.

The second optimization-paper step is also now present in the prototype: world cuts.

The third optimization-paper step is also now present in the prototype: cut on win.

The fourth optimization-paper step is also now present in the prototype: empty-entry handling for sparse interior fronts.

The fifth optimization-paper step is also now present in the prototype: deep alpha cuts against earlier Max ancestors.

The sixth optimization-paper step is also now present in the prototype: leaf-parallelized DDS leaf evaluation.

The prototype also now includes optimistic completion of impossible worlds for cross-state comparison, following the later discussion in the optimization paper.

The prototype also now includes a first possible-world generator from simple bidding-style and play-style constraints over a candidate world pool.

The prototype also now includes a first bridge move generator over those possible worlds, including legal-move union and world elimination after a play.

## Step 7 — keep DDS-only work on a separate branch of the plan

DDS-side follow-up work is still reasonable, but it is now a separate support track.

Good candidates there remain:

- a conditional refinement of the `SolveSameBoard()` guess bias,
- an exact-hint fast path in `AnalyseLaterBoard()`,
- cleanup of the `solutions == 3` root duplication.

These should not displace the actual alpha-mu prototype work.

## Explicit non-goals for the next cycle

These should stay out of scope for now:

- rewriting `ABsearch*()` to impersonate alpha-mu,
- mixing paper-level alpha-mu semantics directly into DDS before a prototype exists,
- low-level SIMD work before Pareto-front costs are measured,
- broad performance tuning unrelated to a confirmed bottleneck.

## Practical summary

The next cycle should be:

1. preserve the DDS baseline,
2. lock the paper-derived test set,
3. implement the minimal alpha-mu prototype,
4. add early/root cut,
5. verify correctness,
6. add optimization-paper features one at a time,
7. keep DDS root-policy tuning as a secondary support stream.
