# Alpha-Mu Concrete Action Plan

## Purpose

This document turns the staged roadmap in `implementation-plan.md` and
`alpha-mu-roadmap.md` into the **next concrete execution cycle** toward a
complete alpha-mu engine for post-mortem declarer-play evaluation.

The immediate aim is not to demonstrate paper semantics in isolation. It is to
deliver the next vertical slice of a real decision-point evaluator that can take
an actual hand history, reconstruct a plausible information state, and recommend
a move under uncertainty.

## Immediate objectives

1. Preserve the current DDS-side benchmarked baseline.
2. Make the information state significantly more realistic.
3. Deliver a stable single-decision post-mortem alpha-mu analysis path.
4. Start extracting durable engine modules from the current monolithic
   implementation.
5. Add the instrumentation needed to judge recommendation quality and runtime
   cost separately.

## Deliverable for this cycle

At the end of this cycle, a user should be able to analyze a single declarer
decision point from a real hand and receive:

- the alpha-mu recommended move,
- alternative candidate moves,
- the surviving world summary,
- an explanation of why worlds were admitted or rejected,
- and a comparison point against DDS and the actual played card.

## Workstream 1 — preserve and measure the DDS baseline

Keep the DDS support workflow available and reproducible:

- `python3 test/alpha_mu_benchmark.py`
- `test/build/regression_api`
- `test/build/dtest -f ../hands/list10.txt -s solve`
- `test/build/play_analysis_benchmark`

Record:

- pass/fail status,
- wall-clock time,
- probe-count summaries by context,
- and whether repeated solves remain stable.

This is not the main algorithmic target, but it remains the performance and
correctness baseline for DDS as the leaf oracle.

## Workstream 2 — richer information-state construction

### Goal

Make alpha-mu's world model reflect what declarer could realistically infer at
the table.

### Tasks

1. **Formalize the information-state contract**
   - document the intended meaning of bidding constraints, play-history
     constraints, known cards, cannot-hold facts, follow-suit implications,
     suit-length ranges, HCP ranges, hand types, and partnership ranges,
   - decide which inferences are hard constraints versus soft plausibility
     inputs.

2. **Extend bidding-derived inference**
   - broaden the current scoped auction-side model,
   - add richer seat-level and partnership-level range construction where the
     auction meaning is clear enough to support deterministic regression cases.

3. **Extend play-derived inference**
   - preserve current follow-suit legality,
   - add richer history-derived narrowing from longer sequences,
   - add regression cases where later discard/play evidence materially narrows
     the world pool.

4. **Improve history-derived world construction**
   - support longer and larger ambiguous defender pools,
   - retain deterministic capping and explanation traces,
   - keep constructor-local pruning measurable.

5. **Introduce world plausibility hooks**
   - add a first explicit notion of world ranking or weighting,
   - keep the first version simple and regression-backed,
   - do not yet let weighting alter core front semantics silently; expose it in
     reporting first.

### Success tests

- new regressions starting from real-looking partial-information states rather
  than mostly curated toy pools,
- explanation traces for why worlds survive or are rejected,
- reproducible world construction on repeated runs,
- and at least one board where richer bidding/play information narrows the
  world set in a way that changes the reported recommendation.

## Workstream 3 — decision-point post-mortem runner

### Goal

Provide a stable alpha-mu entry point for analyzing a single real decision.

### Tasks

1. **Define the analysis entry point**
   Support input of:
   - deal,
   - declarer/leader/contract,
   - bidding-derived information,
   - play prefix,
   - search depth / Max-move horizon,
   - world-count budget,
   - deterministic seed.

2. **Add a decision-point runner mode**
   The runner should:
   - stop at a declarer turn,
   - build the information state,
   - generate and filter worlds,
   - search with alpha-mu,
   - and print the chosen move plus supporting information.

3. **Add comparison reporting**
   For the same decision point, report:
   - actual played move,
   - alpha-mu recommended move,
   - DDS omniscient move where appropriate,
   - and why alpha-mu differs if it does.

4. **Add analyst-grade output fields**
   Include:
   - surviving world count,
   - top candidate moves,
   - root front summary,
   - cut activity,
   - DDS leaf count,
   - time split between world generation and search.

### Success tests

- one stable command or API path that analyzes a real board at one decision
  point,
- repeated runs are deterministic under fixed seed and configuration,
- output is useful without reading the implementation.

## Workstream 4 — deeper practical continuation coverage

### Goal

Ensure the decision-point evaluator is supported by meaningful continuation
search rather than only narrow showcase trees.

### Tasks

1. add more realistic multi-world continuation regressions,
2. extend mixed merge/split bridge cases across more tricks,
3. verify partial-trick to next-trick transitions under deeper search,
4. strengthen root summaries for move/front/world/cut visibility,
5. continue to keep DDS only as the leaf oracle.

### Success tests

- more than two surviving worlds in practical bridge-backed continuation tests,
- deeper searched prefixes than the current showcase depth,
- stable move selection across repeated runs.

## Workstream 5 — extract durable modules while growing features

### Goal

Stop accumulating all alpha-mu growth in one monolithic implementation file.

### Tasks

1. extract information-state and world-construction code into a dedicated unit,
2. extract Pareto-front and outcome-vector logic into a dedicated unit,
3. extract bridge-state transition helpers into a dedicated unit,
4. keep the runner and tests working while code moves,
5. keep alpha-mu outside the core DDS recursion.

### Success tests

- no regression failures after extraction,
- cleaner ownership boundaries,
- easier addition of new world-generation and reporting features.

## Workstream 6 — instrumentation for the next optimization cycle

### Goal

Collect the evidence needed for later performance work.

### Tasks

Measure and report at least:

- candidate world count before and after constructor-local pruning,
- surviving world count after each staged filter,
- root front size,
- dominance reduction counts,
- TT probes/hits/stores,
- cut counts by type,
- DDS leaf calls,
- elapsed time split by world generation, search, and DDS leaves.

### Success tests

- the decision-point runner reports these metrics,
- benchmark logs can distinguish world-generation cost from search cost.

## Explicit non-goals for this cycle

These should stay out of scope for now:

- redesigning `ABsearch*()` to impersonate alpha-mu,
- pushing paper-level alpha-mu semantics into DDS recursion,
- broad low-level SIMD or Apple-Silicon tuning before alpha-mu bottlenecks are
  measured,
- forcing immediate public API stabilization before the decision-point runner is
  proven useful,
- treating set membership alone as sufficient if weighting/ranking turns out to
  be necessary for recommendation quality.

## Practical summary

The next cycle should be:

1. preserve the DDS baseline,
2. strengthen realistic information-state construction,
3. deliver a single-decision post-mortem alpha-mu runner,
4. broaden practical continuation coverage,
5. extract durable modules while growing the feature set,
6. instrument the engine for the next optimization cycle.

## Definition of done for this cycle

This cycle is done when all of the following are true:

1. alpha-mu can analyze at least one real declarer decision point end to end,
2. the world set is derived from richer bidding/play information than the
   current showcase baseline,
3. the runner reports recommendation, alternatives, world summary, and timing,
4. the implementation is measurably more modular than at the start of the
   cycle,
5. DDS baseline checks remain green and benchmark-backed.

