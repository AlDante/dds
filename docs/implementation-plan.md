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

## Phase 4 — first real alpha-mu implementation slice

### Goal

Implement a paper-faithful alpha-mu search layer as a **new layer around DDS**, not as a mutation of the existing `ABsearch*()` recursion.

### Scope

The first implementation slice should be intentionally narrow:

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

### Initial implementation status

The first minimal alpha-mu implementation now exists as a separate test component under `test/`.

It currently provides:

- world-mask handling,
- a first possible-world generator from simple bidding/play-style constraints over a candidate pool,
- a first bridge move generator over those possible worlds,
- a Pareto-front transposition table for exact alpha-mu search reuse,
- outcome vectors,
- Pareto-front reduction,
- Max-node union and Min-node product/min operations,
- toy-search validation of non-locality, early cut, and root cut,
- useful-world maintenance in the alpha-mu search,
- world cuts for zero and single useful worlds,
- empty-entry handling for sparse interior fronts,
- optimistic completion of impossible and useless worlds for cross-state comparison,
- deep alpha cuts against earlier Max ancestors,
- cut on win at Max nodes,
- a leaf-parallelized DDS leaf-evaluation path,
- a DDS-backed leaf-evaluation demo over `hands/alpha_mu_play.txt`.

It is intentionally not yet a full bridge alpha-mu engine.

## Phase 5 — alpha-mu optimizations from the second paper

Only after a correct alpha-mu implementation exists should the paper's optimization work begin.

The recommended order is:

1. maintain useful worlds,
2. world cuts,
3. cut on win,
4. empty-entry support for interior-node fronts,
5. deep alpha cuts,
6. only then consider leaf parallelization and low-level SIMD work.

The implementation now also includes:

- optimistic-value completion for impossible worlds during cross-state comparison,
- a first leaf-parallelized DDS leaf-evaluation path.

The first six optimization steps in that sequence are now present in implementation form, with low-level SIMD work still explicitly deferred until Pareto-front costs are measured.

## Phase 6 — optional DDS-side cleanup that remains worthwhile

This phase is now clearly separate from alpha-mu proper.

Possible items:

- `solutions == 3` root-path cleanup in `src/SolverIF.cpp`,
- better hint handling in play analysis,
- more precise benchmark reporting for repeat/hinted solves.

These may still be worthwhile, but they are no longer the primary algorithmic roadmap.

## Phase 7 — deeper integration only if justified

Only after the alpha-mu implementation track proves value should the project consider deeper integration with the existing solver core.

Possible later topics include:

- tighter DDS leaf reuse from alpha-mu,
- reuse of existing TT information where semantics genuinely align,
- selective borrowing of DDS move-ordering or pruning knowledge into alpha-mu,
- interval-aware behavior deeper in the perfect-information solver if benchmark data still justifies it.

These are explicitly deferred because they combine correctness, pruning, TT, and architectural risks.

## Phase 8 — realistic world generation and state construction

### Goal

Move from a curated world pool to realistic alpha-mu inputs derived from actual bridge information.

### Required work

1. Define an explicit alpha-mu-side representation for:
   - bidding constraints,
   - play-history constraints,
   - known cards,
   - suit-length / void / follow-suit implications,
   - optional world weights or sampling frequencies.
2. Extend the possible-world generator to support:
   - full partial-history filtering rather than only simple showcase constraints,
   - deterministic sampling with reproducible seeds,
   - deduplication / canonicalization of equivalent worlds,
   - rejection accounting so the generator can be debugged and benchmarked.
3. Add tests for:
   - worlds eliminated by bidding constraints,
   - worlds eliminated by play-history legality,
   - worlds merging under equivalent information,
   - reproducibility of generated world sets.

### Success criteria

- alpha-mu can build realistic world sets from nontrivial histories,
- generated worlds are reproducible and explainable,
- world-generation cost is measured separately from search cost.

## Phase 9 — broaden bridge search control into a real declarer-play searcher

### Goal

Extend the current small multi-trick controller into a true alpha-mu bridge search over realistic continuations.

### Required work

1. Support broader continuation horizons measured in Max moves.
2. Preserve correct state transitions for:
   - empty-trick states,
   - partial-trick states,
   - winner advancement,
   - next-trick lead changes,
   - legal-move elimination across worlds.
3. Add regression coverage for:
   - more than two surviving worlds,
   - mixed merge/split frontier shapes after continuation,
   - repeated sparse-front transitions across multiple tricks,
   - deeper searched continuations before DDS leaf handoff.
4. Add root result reporting for:
   - chosen move,
   - root front,
   - useful-world counts,
   - cut activity,
   - DDS leaf-evaluation count.

### Success criteria

- alpha-mu can search several Max moves deep on realistic bridge continuations,
- root move choice is stable on repeated runs,
- sparse-front behavior remains correct under deeper continuations.

## Phase 10 — complete the paper-motif validation set

### Goal

Ensure the implementation is judged on the imperfect-information motifs alpha-mu is supposed to address, not only on practical DDS-derived hands.

### Required work

1. Add dedicated controlled cases for:
   - strategy fusion,
   - non-locality,
   - discovery-play / information gain,
   - rare-bad-event avoidance.
2. For each motif, define:
   - the intended world family,
   - the expected root preference or front property,
   - the minimum horizon required to expose the behavior.
3. Extend `docs/alpha-mu-test-set.md` with:
   - explicit motif-to-fixture mapping,
   - success criteria for move choice and cut behavior,
   - notes about what is still synthetic versus repository-format.

### Success criteria

- the implementation passes deterministic motif-driven checks,
- alpha-mu-specific behavior is demonstrated on cases that DDS alone does not explain well.

## Phase 11 — separate alpha-mu internals into durable modules

### Goal

Keep the growing alpha-mu implementation understandable and maintainable before it graduates from a single narrow implementation file.

### Required work

1. Split the current implementation into reusable units for:
   - world representation and masks,
   - front operations,
   - bridge-state transition helpers,
   - DDS leaf adaptation,
   - search controller,
   - fixture definitions and test runners.
2. Define clear ownership boundaries between:
   - DDS-perfect-information evaluation,
   - alpha-mu world/state management,
   - alpha-mu search semantics,
   - reporting / instrumentation.
3. Preserve a dedicated alpha-mu runner while making the code easier to evolve.

### Success criteria

- alpha-mu code is no longer bottlenecked by one growing monolithic implementation file,
- new features can be added without destabilizing unrelated alpha-mu subsystems.

## Phase 12 — add full search instrumentation and benchmark accounting

### Goal

Make alpha-mu performance work evidence-driven before low-level tuning begins.

### Required work

Measure and report at least:

- generated world count,
- useful-world count by depth,
- Pareto-front sizes,
- dominance reduction counts,
- TT hit/miss counts,
- DDS leaf calls,
- cut counts by kind,
- elapsed time split by world generation, search, and DDS leaf work.

### Success criteria

- later optimization work can identify actual bottlenecks rather than guess,
- benchmark reports distinguish world-generation cost from search cost.

## Phase 13 — scale the implementation to a complete alpha-mu engine

### Goal

Close the gap between a validated implementation and a complete repository-supported alpha-mu engine.

### Required work

1. Define a stable alpha-mu entry point/API around the implementation layer.
2. Support real contract / declarer / leader input construction for alpha-mu searches.
3. Support configurable:
   - world count,
   - Max-move horizon,
   - sampling seed,
   - optional weighting / pruning policy.
4. Ensure deterministic reproducibility for debugging and regression.
5. Add documentation for invocation, expected outputs, and limits.

### Success criteria

- alpha-mu is runnable as a first-class repository component rather than only as an internal experiment,
- callers can request and reproduce imperfect-information searches with explicit configuration.

## Phase 14 — optimize only measured bottlenecks

### Goal

Improve performance after the algorithm is complete and benchmarked, without compromising correctness.

### Recommended order

1. front representation / reduction cost,
2. world canonicalization and hashing,
3. TT key design and reuse quality,
4. DDS leaf batching / parallelization policy,
5. only then SIMD or lower-level data-layout tuning if measurements justify it.

### Success criteria

- optimization work is benchmark-backed,
- correctness and move-choice stability remain unchanged.

## Phase 15 — integration decision point

### Goal

Decide how far alpha-mu should move beyond the current test-local layer.

### Decision questions

1. Should alpha-mu remain a separate experimental component?
2. Should a public or semi-public API be added?
3. Which DDS internals, if any, are safe to reuse more directly?
4. Is deeper solver-core integration justified by measured value?

### Guidance

Do not force integration for its own sake. Only integrate deeper if:

- the complete alpha-mu implementation is stable,
- the benchmark/test corpus is broad enough,
- the architectural benefit outweighs the correctness risk.

## Definition of complete alpha-mu implementation for this repository

The project should only describe alpha-mu as complete when all of the following are true:

1. realistic world generation exists from meaningful bidding/play histories,
2. bridge continuation search extends beyond the current small multi-trick showcase,
3. paper-motif fixtures for strategy fusion, non-locality, discovery play, and rare bad events are covered,
4. root results include usable move/front/cut reporting,
5. performance instrumentation exists for world generation, front work, cuts, TT reuse, and DDS leaf calls,
6. the implementation is organized into durable modules rather than one narrow implementation file,
7. the component can be run reproducibly as a first-class repository-supported alpha-mu engine,
8. performance tuning has been applied only where measurements justify it,
9. DDS baseline behavior remains unchanged and benchmark-backed throughout.

## Recommended immediate next move

The next practical step is still **not** to redesign `ABsearch*()`.

But it is also **no longer** just “another root-only tweak in `src/SolverIF.cpp`”.

The next implementation cycle should be:

1. document a paper-derived test set and success criteria,
2. build a minimal alpha-mu implementation slice around DDS leaf evaluation,
3. implement only the base search semantics plus early/root cut,
4. verify that implementation slice on small controlled cases,
5. then add the second paper's optimizations one by one.
