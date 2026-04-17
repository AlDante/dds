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

The prototype also now includes a first possible-world generator from simple bidding-style and play-style constraints over a candidate world pool, including first partnership suit-length range and partnership HCP-range bidding constraints.

The prototype also now includes an explicit follow-suit-implication stage derived from discard history, plus per-world explanations for why candidate worlds are accepted or rejected during generation.

The prototype also now includes a first seed-based hidden-seat world-construction step from partial-information states, including partially specified visible-hand seeds whose missing hidden cards are inferred from the full-deck complement, plus a first moderate-size ambiguous two-defender visible-seed pool with deterministic downselection after staged filtering and a longer multi-trick visible-seed history whose broader pool is narrowed by play-derived follow-suit evidence before sampling, with constructor-local explicit known-card and bidding card-location pruning plus suit-length and partnership suit-length range pruning before the later full filtering pipeline.

The prototype also now includes constructor-local accounting and explanation traces for that seed-based hidden-seat construction, including stable counts before ownership pinning, after ownership pinning, after constructor-local card-location pruning, and after later constructor-local length/HCP/balanced narrowing.

The prototype also now includes constructor-local MinHCP and MaxHCP pruning for those seed-based hidden-seat candidate worlds before the later full bidding filter, along with a first constructor-local partnership HCP-range pruning path when one partner is hidden and the other remains visible.

The prototype also now includes conservative constructor-local balanced-shape pruning for full hidden hands, while still deferring incomplete toy hidden-hand cases to the later full bidding filter.

The prototype also now includes a first bridge move generator over those possible worlds, including legal-move union and world elimination after a play.

The prototype also now includes a first one-trick bridge search controller over those generated move trees, including trick completion, winner advancement, and bridge-specific backup of sparse outcome vectors.

The prototype also now includes a first Pareto-front transposition table for exact repeated-subtree reuse in the toy alpha-mu search.

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

## Next concrete execution cycles from the current prototype

The prototype now already covers the minimum semantic core plus the first optimization-paper features.

That means the next execution cycles should no longer be framed as “build the first prototype”, but as “turn the existing prototype into a complete alpha-mu implementation”.

### Cycle A — richer world generation

Implement the next realistic-world step:

1. represent bidding constraints, play-history constraints, known cards, and follow-suit implications explicitly,
2. extend world generation from simple filtered candidate pools to realistic history-derived worlds,
3. make world sampling reproducible and measurable,
4. add regression cases that explain why each world is accepted or rejected,
5. extend post-lead construction from tiny East/West card swaps to longer post-lead histories with richer defender-side ambiguity,
6. grow from narrow hidden-card pools to moderately larger ambiguous defender pools before attempting full remaining East/West construction,
7. treat capped or sampled full remaining East/West construction as a later Stage-1 candidate once the smaller-history construction path is benchmark-backed.

### Cycle B — larger bridge continuations

Extend the bridge search controller from the current small continuation model to larger realistic continuations:

1. support more than two surviving worlds,
2. support multiple sparse-front shapes after continuation,
3. support mixed merge/split behavior across multiple tricks,
4. verify correct empty-trick and partial-trick transitions at larger horizons,
5. report root move, front, and useful-world counts,
6. prefer longer post-lead continuation families before attempting much broader world counts at the same search horizon.

### Cycle C — complete the missing paper motifs

Add controlled fixtures for the motifs not yet directly represented in repository-format material:

1. strategy fusion,
2. non-locality,
3. discovery-play / information gain,
4. rare-bad-event avoidance.

For each fixture, record:

- intended world family,
- expected root property or move preference,
- required horizon,
- whether the case is synthetic or file-backed.

### Cycle D — refactor the prototype into durable units

Before further growth, split the prototype into clearer pieces for:

1. world/state representation,
2. Pareto-front operations,
3. bridge-state transitions,
4. DDS leaf adaptation,
5. search control,
6. fixture definition and runner logic.

Keep the alpha-mu code outside the core DDS recursion while doing this.

### Cycle E — instrument alpha-mu for measured optimization

Add benchmark accounting for:

- world generation cost,
- world-construction candidate counts before and after constructor-local pruning,
- front sizes and dominance reductions,
- TT hit/miss counts,
- cut counts by type,
- DDS leaf-call counts,
- elapsed time split by stage,
- and checkpoint output for long-running timing jobs so partial progress survives interrupted runs.

Possible later optimization candidates after those measurements include bridge-state transposition reuse for the continuation search and, if that proves worthwhile, Zobrist-style state keying for the bridge alpha-mu state.

Do this before adding further performance work.

### Cycle F — graduate from prototype to complete engine

Define the point where alpha-mu stops being only a prototype runner and becomes a complete repository component:

1. a stable alpha-mu entry point,
2. configurable world count / horizon / seed,
3. reproducible imperfect-information searches,
4. documented inputs, outputs, and limits,
5. maintained regression/benchmark coverage.

## Practical definition of done for alpha-mu

Alpha-mu should only be treated as complete in this repository when all of the following are true:

1. realistic world generation from meaningful histories exists,
2. bridge continuation search extends well beyond the current showcase depth,
3. paper-motif fixtures are covered directly,
4. root reporting includes move/front/cut information,
5. performance instrumentation separates world generation, search, and DDS leaf cost,
6. the implementation is organized into durable components,
7. the engine is runnable reproducibly as a first-class repository-supported alpha-mu searcher,
8. DDS baseline behavior remains unchanged and benchmark-backed.

