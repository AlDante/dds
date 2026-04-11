# Alpha-Mu Implementation Plan

## Current status

The groundwork completed before algorithmic changes includes:

- stronger regression comparisons in `test/compare.cpp`,
- a focused public-API regression harness in `test/regression_api.cpp`,
- a standalone timer regression harness in `test/timer_regression.cpp`,
- warning cleanup needed to build and test reliably on the current macOS toolchain,
- default macOS multi-threading through GCD + STL instead of Boost,
- cleaner legacy Makefile output layouts under `src/build/`, `test/build/`, and `examples/build/`.

That means alpha-mu work now proceeds against a materially better-tested and easier-to-measure baseline.

## Corrected framing after reading the papers

The original alpha-mu paper and the optimization paper make one thing clear:

- **alpha-mu proper** is an imperfect-information search over multiple possible worlds using outcome vectors and Pareto fronts,
- **our current work** is a DDS-side support track that improves exact-score root probing and benchmarking inside a single perfect-information world.

So the current `src/SolverIF.cpp` work is best understood as:

- valuable DDS groundwork,
- relevant to future alpha-mu leaf evaluation and reuse,
- but **not yet** a direct implementation of the paper's alpha-mu search.

That distinction matters because the next steps should no longer assume that incremental root-policy work inside DDS alone will eventually “turn into” full alpha-mu.

## What DDS can and cannot provide directly

DDS already provides important ingredients:

- threshold-style recursive proof search,
- exact perfect-information evaluation,
- lower/upper bound storage in the TT,
- efficient move generation and pruning,
- root-level repeated threshold probing.

But the papers require additional machinery that does not yet exist in this codebase:

- a representation of sampled possible worlds,
- outcome vectors over those worlds,
- Pareto fronts,
- Max-node front union and Min-node front product/min combination,
- useful-world maintenance,
- world cuts,
- cut-on-win,
- deep alpha cuts,
- a TT keyed to alpha-mu fronts and search depth / Max-move horizon.

## Phase 0 — completed preparation

The goal of the preparation phase was to ensure that changes to root search policy could be measured and verified.

Completed ingredients include:

- golden-data regression for solve / calc / play / par / dealer-par,
- API-consistency checks for tables, par APIs, conversion APIs, and exact-score probe behavior,
- platform build cleanup that makes iterative testing easier,
- documentation describing DDS structure and the intended alpha-mu insertion point.

## Phase 1 — completed root refactor

### Target

Refactor the duplicated root exact-score probing logic in `src/SolverIF.cpp` into a single helper.

### Completed implementation

The first implementation step has already been done:

- added a shared root exact-score helper, `SearchExactScoreRoot()`, in `src/SolverIF.cpp`,
- reused that helper from:
  - `SolveBoardInternal()` for `target == -1`,
  - `SolveSameBoard()`,
  - `AnalyseLaterBoard()`,
- preserved the existing deep recursive search functions,
- preserved pruning behavior in `src/QuickTricks.cpp` and `src/LaterTricks.cpp`,
- preserved TT meaning and storage semantics,
- deliberately left the `solutions == 3` enumeration path unchanged for now.

### Why this was the right first step

This extraction created a single, explicit place to evolve root interval handling without mixing that work with deeper semantic changes.

It also reduced duplication in exactly the code paths where alpha-mu-style experimentation is most natural.

### Verification status

The Phase 1 refactor has already compiled and passed regression checks, including focused public-API regression and representative solve runs.

Those checks should still be rerun whenever the next alpha-mu step lands, but the current baseline is considered verified.

## Constraints for the next steps

Any further alpha-mu work should preserve the following invariants until data shows that deeper changes are justified:

1. **Public results must remain unchanged**.
2. **Move generation and pruning logic must remain valid**.
3. **TT bounds must keep their current meaning**.
4. **Repeat-solve and play-analysis hint paths must continue to work**.
5. **Claims about improvement must be backed by benchmark data and representative workloads**.

## Evaluation criteria

Because alpha-mu is a different approach rather than just a micro-optimization exercise, success should be judged on more than elapsed time alone.

Important measures are:

- correctness against the existing regression suites,
- node counts and root probe counts,
- wall-clock time on representative workloads,
- behavior for repeated solves and hinted solves,
- behavior for play-analysis paths such as `AnalyseLaterBoard()`,
- stability of move selection and exact-score discovery in realistic usage, not only synthetic hot loops.

## Phase 2 — completed DDS-side measurement cycle

### Goal

Establish a trustworthy before/after baseline for DDS-side root-policy work.

### Status

This phase is effectively complete:

- root instrumentation exists in `src/SolverIF.cpp`,
- `test/alpha_mu_benchmark.py` automates the benchmark cycle,
- `test/play_analysis_benchmark.cpp` provides dedicated `AnalyseLaterBoard()` coverage,
- one measured repeat-solve guess-seeding experiment has already been run.

### Result

We now have enough evidence to say that the DDS-side groundwork is real and useful, but also that it should be treated as a **support track** rather than the core implementation of alpha-mu proper.

## Phase 3 — small DDS-side root-policy experiments

### Goal

Continue only the DDS-local experiments that clearly improve exact-score probing and are cheap to maintain.

### Candidate directions

- better initial guess selection from prior exact scores or hints,
- more disciplined interval tightening,
- clearer handling of directional hints in analysis paths,
- reuse of information already available from similar-deal or repeat-solve contexts.

### Status

The first such experiment already exists:

- `SolveSameBoard()` now biases the initial guess up by one trick,
- the benchmarked average probe count improved on the current repeat-solve workload.

### Guidance

Further work in this phase is optional and should be limited to obvious local wins such as:

- conditional repeat-solve biasing,
- an exact-hint fast path for `AnalyseLaterBoard()`,
- cleanup of remaining root duplication.

## Phase 4 — first real alpha-mu prototype

### Goal

Implement a paper-faithful alpha-mu search prototype as a **new layer around DDS**, not as a mutation of the existing `ABsearch*()` recursion.

### Scope

The first prototype should be intentionally narrow:

- fixed contract family,
- small number of worlds,
- limited Max-move horizon,
- DDS used as the leaf evaluator,
- correctness and semantics prioritized over speed.

### Required pieces

1. A representation of:
   - sampled worlds,
   - valid/useful-world masks,
   - outcome vectors,
   - Pareto fronts.
2. Core front operations:
   - dominance,
   - Max-node union + front reduction,
   - Min-node product/min + front reduction.
3. Search control:
   - horizon measured in Max moves,
   - root iterative deepening,
   - early cut,
   - root cut.
4. A simple DDS leaf adapter for evaluating a world.
5. A paper-derived benchmark and example set.

### Why this is now the recommended next implementation step

This is the first phase that actually moves the repository toward the algorithm described in the papers rather than just improving DDS exact-score probing.

### Initial prototype status

The first minimal prototype now exists as a separate test component under `test/`.

It currently provides:

- world-mask handling,
- outcome vectors,
- Pareto-front reduction,
- Max-node union and Min-node product/min operations,
- toy-search validation of non-locality, early cut, and root cut,
- useful-world maintenance in the prototype search,
- world cuts for zero and single useful worlds,
- empty-entry handling for sparse interior fronts,
- deep alpha cuts against earlier Max ancestors,
- cut on win at Max nodes,
- a leaf-parallelized DDS leaf-evaluation path,
- a DDS-backed leaf-evaluation demo over `hands/alpha_mu_play.txt`.

It is intentionally not yet a full bridge alpha-mu engine.

## Phase 5 — alpha-mu optimizations from the second paper

Only after a correct prototype exists should the paper's optimization work begin.

The recommended order is:

1. maintain useful worlds,
2. world cuts,
3. cut on win,
4. empty-entry support for interior-node fronts,
5. deep alpha cuts,
6. only then consider leaf parallelization and low-level SIMD work.

The prototype now also includes a first leaf-parallelized DDS leaf-evaluation path.

The first six optimization steps in that sequence are now present in prototype form, with low-level SIMD work still explicitly deferred until Pareto-front costs are measured.

## Phase 6 — optional DDS-side cleanup that remains worthwhile

This phase is now clearly separate from alpha-mu proper.

Possible items:

- `solutions == 3` root-path cleanup in `src/SolverIF.cpp`,
- better hint handling in play analysis,
- more precise benchmark reporting for repeat/hinted solves.

These may still be worthwhile, but they are no longer the primary algorithmic roadmap.

## Phase 7 — deeper integration only if justified

Only after the prototype track proves value should the project consider deeper integration with the existing solver core.

Possible later topics include:

- tighter DDS leaf reuse from alpha-mu,
- reuse of existing TT information where semantics genuinely align,
- selective borrowing of DDS move-ordering or pruning knowledge into the alpha-mu prototype,
- interval-aware behavior deeper in the perfect-information solver if benchmark data still justifies it.

These are explicitly deferred because they combine correctness, pruning, TT, and architectural risks.

## Recommended immediate next move

The next practical step is still **not** to redesign `ABsearch*()`.

But it is also **no longer** just “another root-only tweak in `src/SolverIF.cpp`”.

The next implementation cycle should be:

1. document a paper-derived test set and success criteria,
2. build a minimal alpha-mu prototype around DDS leaf evaluation,
3. implement only the base search semantics plus early/root cut,
4. verify the prototype on small controlled cases,
5. then add the second paper's optimizations one by one.
