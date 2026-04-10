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

## Working definition of alpha-mu in DDS

In this codebase, alpha-mu should be treated first as a **root interval / threshold search policy** rather than a wholesale replacement of the recursive proof engine.

DDS already knows how to answer questions of the form:

- can the side to move force at least `target` tricks from this node?

The alpha-mu opportunity is therefore to improve how the root chooses, tightens, and reuses score intervals when it wants an exact answer.

This is not only a raw-speed project. Performance matters, but the point is to get a better real-world evaluation strategy for repeated solves, play analysis, and exact-score discovery while preserving DDS correctness.

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

## Phase 2 — measure the current root policy properly

### Goal

Establish a trustworthy before/after baseline for further root-policy work.

### Scope

Stay at the root layer and add only measurement support or analysis that does not alter solver semantics.

### Likely work items

- capture root probe counts per exact-score solve,
- record starting guess quality versus final exact score,
- separate first-solve and repeated-solve behavior,
- benchmark solve, repeat-solve, and play-analysis workloads separately,
- document representative benchmark sets and how to rerun them.

### Out of scope

- no changes to `ABsearch*()` semantics,
- no TT redesign,
- no pruning rewrites.

## Phase 3 — root-policy experiments

### Goal

Try small alpha-mu-inspired improvements at the root while preserving the current recursive proof engine.

### Candidate directions

- better initial guess selection from prior exact scores or hints,
- more disciplined interval tightening,
- clearer handling of directional hints in analysis paths,
- reuse of information already available from similar-deal or repeat-solve contexts.

### Success condition

At least one root-only experiment should show value on representative workloads without harming correctness or maintainability.

## Phase 4 — optional cleanup of remaining root duplication

If Phase 3 produces a useful root-policy abstraction, the next cleanup candidate is the `solutions == 3` exact-card enumeration path in `src/SolverIF.cpp`.

That work is still root-local and can remain compatible with the current deep search behavior.

## Phase 5 — deeper search work only if justified

Only after the root-policy path has been measured and exploited should the project consider deeper changes.

Possible later topics include:

- interval-aware behavior deeper in the recursive search,
- tighter interaction between root policy and stored TT bounds,
- better proof-order control inside `ABsearch*()`.

These are explicitly deferred because they combine correctness, pruning, and TT risks.

## Recommended immediate next move

The next practical step is **not** to redesign `ABsearch*()`.

It is to:

1. rerun the current regression baseline,
2. add lightweight root-policy measurement,
3. benchmark real workloads,
4. choose one small root-only alpha-mu experiment from the resulting data.
