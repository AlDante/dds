# Alpha-Mu Roadmap

This document captures the current practical roadmap from the existing alpha-mu
implementation state to a complete repository-grade engine for **post-mortem
declarer-play evaluation under imperfect information**.

The target is not a side experiment that merely demonstrates paper semantics.
The target is a maintained alpha-mu engine that can replay a real hand at a real
decision point, reconstruct what declarer could reasonably infer, and recommend
play lines that are more realistic than omniscient DDS advice.

## Mission

Alpha-mu should become the repository's imperfect-information companion to DDS:

- DDS remains the perfect-information leaf oracle,
- alpha-mu supplies the world model, front semantics, and decision policy under
  uncertainty,
- and the combined system supports post-mortem analysis of what declarer should
  have done given the auction and play to that point.

## Current status

The current implementation is best described as an **incubating engine path**.
It already contains a large fraction of the core alpha-mu semantics, but it is
not yet a complete post-mortem evaluator.

A reasonable practical estimate is that the repository is currently about
**50% complete overall** toward that end-state.

### What is already strong

The current implementation already includes:

- world masks,
- sparse outcome vectors,
- Pareto fronts,
- Max-node union and Min-node product/min semantics,
- useful-world maintenance,
- world cuts,
- cut-on-win,
- deep alpha cuts,
- optimistic completion of sparse worlds,
- deterministic world downselection,
- staged possible-world filtering,
- constructor-local hidden-seat world construction,
- per-world explanation traces for world acceptance and rejection,
- bridge move generation and trick progression,
- multi-trick continuation search,
- DDS-backed leaf evaluation,
- root reporting,
- debug-only invariants for world-mask and DDS-leaf legality,
- and DDS-versus-alpha-mu comparison tooling.

### What still blocks the full goal

The implementation is not complete because several crucial pieces are still
missing or immature:

- richer world construction from realistic bidding and play histories,
- world plausibility / weighting rather than only set membership,
- broader and deeper practical continuation search,
- a first-class decision-point post-mortem analysis workflow,
- broader bridge-backed coverage of the paper motifs,
- durable engine modules outside the current monolithic test-area implementation,
- and performance maturity for nontrivial analysis depths.

## Percent-complete estimate by subsystem

| Subsystem | Estimate | Notes |
| --- | ---: | --- |
| Core alpha-mu semantics | 85% | Front semantics and major paper cuts are already present |
| Information-state representation | 55% | Useful structure exists, but richer real-history modeling is still needed |
| World construction and filtering | 45% | Strong staged filtering exists, but realistic large-scale construction is still limited |
| World plausibility / weighting | 15% | A major missing differentiator versus pure set-based DDS comparison |
| Bridge move generation and state evolution | 70% | Practical and already regression-backed |
| Bridge continuation search | 45% | Works on meaningful cases, but practical depth and coverage are still limited |
| DDS-backed leaf integration | 80% | One of the strongest parts of the current implementation |
| Reporting / inspectability | 55% | Good debug/reporting foundation exists, but not analyst-grade output yet |
| Regression coverage | 65% | Strong semantic coverage, but not yet a full bridge motif corpus |
| Performance instrumentation | 55% | Good start, but phase-separated alpha-mu accounting is still incomplete |
| Durable engine architecture | 20% | Most logic still lives in the current test-area implementation |
| First-class repository integration | 15% | Stable public entry points and user-facing workflows are still to be built |

## Roadmap: 50% to 100%

| Stage | Target completion | Main goal | What changes |
| --- | ---: | --- | --- |
| Current state | 50% | Incubating engine path | Strong core semantics, partial-information search scaffolding, DDS leaf integration, initial bridge continuation and reporting |
| Stage 1 | 60% | Realistic information-state construction | Richer bidding/play inference, larger history-derived world sets, explicit explanation of surviving worlds |
| Stage 2 | 70% | Post-mortem decision-point evaluator | Stable single-decision analysis path with recommendation, alternatives, and explanation |
| Stage 3 | 80% | Broader and deeper practical search | Larger continuation families, better reuse, stronger root summaries, deeper bridge-backed regressions |
| Stage 4 | 88% | Durable engine architecture | Extract modules from the monolith, define stable interfaces, retain regression parity |
| Stage 5 | 95% | Measured usable engine | Mature instrumentation, benchmark-backed optimization, meaningful real workloads |
| Done | 100% | Complete alpha-mu engine | Realistic worlds, practical search, motif coverage, analyst-grade reporting, first-class repository support |

## Stage 1: 50% to 60%

### Goal

Make the information state realistic enough that alpha-mu's recommendations are
meaningfully different from DDS for the right reasons.

### Main work

- extend information-state modeling beyond the current known-card, simple
  bidding-range, and follow-suit constraints,
- improve history-derived world construction from longer and less hand-curated
  auction/play prefixes,
- support moderately larger ambiguous defender pools with reproducible capped
  construction,
- add stronger explanation traces for why worlds survive or are rejected,
- and introduce a first explicit notion of world plausibility or weighting.

### Deliverables

- richer information-state construction,
- benchmark-backed realistic world-pool generation,
- world explanations suitable for post-mortem inspection,
- and a first weighted or ranked world-selection policy.

### Completion signal

This stage is done when the engine can routinely construct plausible world sets
from richer bridge histories rather than mostly relying on hand-curated
fixtures.

## Stage 2: 60% to 70%

### Goal

Deliver a usable **decision-point post-mortem evaluator**.

### Main work

- add a stable alpha-mu entry point for analyzing one declarer decision,
- accept real deal / auction / play-prefix input,
- stop at a declarer turn and report the recommended move,
- compare alpha-mu's recommendation with the actual play and DDS's omniscient
  answer,
- and report the supporting world/front information clearly enough for a bridge
  analyst to inspect.

### Deliverables

- a stable runner or API for a single decision point,
- recommendation + alternative-line reporting,
- and an explanation of why the chosen move is preferred under uncertainty.

### Completion signal

This stage is done when the engine can analyze a real played decision and emit a
recommendation that is understandable without reading the internals.

## Stage 3: 70% to 80%

### Goal

Broaden bridge-search coverage enough that the decision-point evaluator is not
limited to narrow showcase continuations.

### Main work

- add more practical continuation families,
- deepen mixed merge/split bridge regressions,
- strengthen sparse-front behavior across multiple tricks,
- improve bridge-state reuse and caching where semantics allow it,
- and make root summaries richer: chosen move, front, cut activity,
  surviving/weighted worlds, DDS leaf cost.

### Deliverables

- broader continuation regressions,
- deeper bridge-backed searches,
- and stronger root reporting suitable for post-mortem review.

### Completion signal

This stage is done when the bridge-search layer covers a broad enough family of
realistic continuation patterns that the engine can analyze more than isolated
showcase cases.

## Stage 4: 80% to 88%

### Goal

Turn the current monolithic implementation into durable engine code.

### Main work

- extract reusable modules for information state, world generation, front
  operations, bridge-state transitions, DDS leaf handoff, search control, and
  reporting,
- preserve regression coverage during extraction,
- and define clear ownership boundaries between DDS-perfect-information work and
  alpha-mu imperfect-information work.

### Deliverables

- alpha-mu logic moved out of the current monolithic implementation,
- stable internal interfaces,
- and preserved regression parity.

### Completion signal

This stage is done when alpha-mu is maintainable engine code rather than a
single growing implementation file under `test/`.

## Stage 5: 88% to 95%

### Goal

Make alpha-mu practically usable on nontrivial post-mortem workloads.

### Main work

- instrument time spent in world generation, weighting, search, DDS leaves, and
  reuse,
- measure front sizes, dominance reductions, candidate-world narrowing, TT
  effectiveness, and cut activity,
- optimize only measured bottlenecks,
- and benchmark realistic decision-point workloads rather than only showcase
  microcases.

### Deliverables

- mature instrumentation,
- benchmark trend tracking,
- and at least a useful subset of real post-mortem analyses that complete in
  practical time.

### Completion signal

This stage is done when alpha-mu is not merely correct, but practically usable
for meaningful post-mortem work.

## Final stage: 95% to 100%

### Goal

Promote alpha-mu to a first-class supported repository component.

### A complete alpha-mu implementation should mean all of the following are true

1. realistic world generation exists from meaningful bidding and play histories,
2. world plausibility is modeled well enough to improve recommendation quality,
3. bridge continuation search extends well beyond the current showcase depth,
4. paper motifs are covered directly in bridge-backed regressions,
5. reporting is genuinely useful for post-mortem move/front/world inspection,
6. performance instrumentation explains where time goes,
7. the implementation is organized into durable modules,
8. alpha-mu is runnable reproducibly as a first-class repository feature,
9. and DDS baseline behavior remains benchmark-backed throughout.

## Highest-value next steps

If progress should be maximized from the current state, the best next steps are:

1. richer realistic information-state construction,
2. world plausibility / weighting,
3. a stable single-decision post-mortem analysis entry point,
4. broader bridge continuation families and deeper regressions,
5. extraction of alpha-mu components into durable modules,
6. stronger alpha-mu-specific instrumentation and benchmark visibility.

## Short version

Alpha-mu in this repository is currently best described as an **incubating
engine path at about 50% complete overall**.

The route to 100% is:

- realistic information-state construction,
- credible world weighting,
- decision-point post-mortem evaluation,
- broader and deeper bridge search,
- module extraction and first-class integration,
- and benchmark-backed performance maturity.

