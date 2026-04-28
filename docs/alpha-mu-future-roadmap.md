# Alpha-Mu Future Roadmap

## Purpose

This document records **deferred future enhancements** for alpha-mu now that the
current staged implementation plan is complete.

It is intentionally different from `alpha-mu-completion-plan.md` and
`action-plan.md`:

- it does **not** represent committed near-term work,
- it does **not** reopen any completed stage,
- and it exists so good ideas are not lost after the `Stage 9` closeout.

## Relationship to current alpha-mu docs

The current implementation status is documented elsewhere:

- `alpha-mu-completion-plan.md` records the completed staged program,
- `alpha-mu-acceptance.md` records the closed release gate,
- `action-plan.md` tracks concrete near-cycle execution work,
- and this file captures the **optional roadmap beyond that baseline**.

## Current baseline

As of the `v2.10.0` release state, alpha-mu is repository-grade at the current
planned scope:

- optimization-paper bridge-search controls are implemented in the real bridge
  search,
- decision-point world construction, reporting, and weighted policy support are
  present,
- practical bridge-backed continuation coverage is regression-backed,
- instrumentation and performance documentation are in place,
- and the Stage 9 acceptance gate is closed.

The items below are therefore **enhancements**, not missing completion work.

## Why these ideas are deferred

These ideas were deferred because the completed implementation already satisfies
its documented definition of done. The remaining opportunities are valuable, but
most of them require one or more of the following:

- broader product scope,
- additional bridge-modeling research,
- significant performance/architecture work,
- or new user-interface expectations beyond the current CLI/text workflow.

## Future enhancement themes

### 1. Richer auction-derived world construction

Highest-value modeling extension:

- infer constraints from auction sequences directly,
- encode common bidding-system expectations rather than only explicit overrides,
- support partnership inferences and competitive auctions more realistically,
- and preserve explanation output for every inferred fact.

### 2. Richer play-history inference

Improve the quality of world construction from the cardplay itself:

- stronger follow-suit and discard implications,
- better ownership inference from longer lines,
- handling of equivalent lines without explanation drift,
- and careful separation between legality-derived facts and softer heuristic
  suggestions.

### 3. Stronger plausibility and weighting models

Extend the current explicit weighted policy into a more realistic likelihood
layer:

- calibrated plausibility scores rather than simple hint sums,
- cumulative reweighting as more evidence arrives,
- benchmarking of weighted versus unweighted recommendations on curated sets,
- and richer reporting of why weighting changed a choice.

### 4. Support for more than 64 worlds

Remove the most obvious current representational cap:

- replace single-word `WorldMask` assumptions with a larger-capacity design,
- preserve fast paths for small compact world sets,
- revisit front, TT, and reporting structures under larger masks,
- and keep deterministic downselection only as a policy choice rather than a
  hard storage limit.

### 5. Deeper and broader practical search

Extend practical bridge-backed coverage beyond the current scoped depth and motif
set:

- more real-board depth-3 and depth-4 workloads,
- more mixed split/merge continuation families,
- stronger selective deepening policies where justified,
- and broader benchmark sets for real decision points.

### 6. Move ordering and TT refinement

Improve search efficiency without changing semantics:

- root-child ordering from cheap bridge-aware heuristics,
- continuation ordering from measured features rather than intuition,
- stricter or richer TT policies where they improve real workloads,
- and measured reuse strategies specific to sparse fronts.

### 7. Parallel alpha-mu tree search

Address the current single-threaded alpha-mu tree limitation:

- root-parallel search,
- deterministic subtree parallelism,
- better coordination with DDS leaf parallelism,
- and benchmark-driven concurrency tuning for real workloads.

### 8. Better analyst-facing explanations

Improve usefulness for bridge analysts:

- stronger "why not this move?" contrast output,
- world clustering rather than flat lists,
- sensitivity analysis for move recommendations,
- and summaries of which facts most influenced the final choice.

### 9. Stable public alpha-mu API and productization

Promote alpha-mu from repository-grade engine path to a more explicit product
surface:

- long-term stable programmatic interfaces,
- clearer machine-readable result formats,
- possible bindings or service-style interfaces,
- and cleaner separation from test-harness-only entry points.

### 10. Interactive UI / visual tooling

Improve accessibility beyond the current CLI:

- browser or desktop decision-point viewer,
- visual world-filter pipeline inspection,
- move-comparison dashboards,
- and integration with hand viewers / PBN workflows.

## Roadmap by horizon

### Near-term candidates

Best cost/benefit follow-on work if alpha-mu development resumes soon:

1. richer auction-derived inference,
2. richer play-history inference,
3. better plausibility modeling,
4. improved analyst explanations.

### Medium-term candidates

Likely to require focused engineering effort but still fit the current engine
shape:

1. move ordering and TT refinement,
2. deeper practical continuation coverage,
3. stable public alpha-mu result/API surfaces.

### Longer-horizon candidates

Likely to require larger refactors or broadened product scope:

1. 64+ world support,
2. parallel alpha-mu tree search,
3. interactive UI / visualization tooling.

## Dependencies and risks

### Modeling risk

Richer bidding/play inference can improve decisions substantially, but it can
also make explanations harder to trust if soft assumptions are mixed with hard
facts.

### Performance risk

Larger world sets, deeper search, and more expressive weighting can all expand
cost quickly. Instrumentation and benchmark discipline should remain mandatory.

### Architecture risk

Large changes such as multiword world masks or parallel alpha-mu search will put
pressure on currently stable front, TT, and reporting interfaces.

### Product-scope risk

API and UI expansion can create maintenance obligations outside the current
research/engine scope.

## Revisit triggers

This roadmap should be revisited if any of the following become true:

- real users want stronger bidding-system inference rather than manual
  constraint injection,
- decision quality is now bottlenecked more by world quality than by search
  depth,
- the 64-world cap becomes a repeated practical limitation,
- alpha-mu needs to analyze larger workloads or lower-latency interactive
  sessions,
- or a downstream consumer needs a stable public alpha-mu interface.

## Open questions

1. Which matters most for practical bridge analysis: better worlds or deeper
   search?
2. Should future plausibility remain explainable and explicit, or become more
   statistical if that improves recommendation quality?
3. Is the next major architecture step larger world sets or parallel search?
4. Should alpha-mu remain primarily a developer/analyst tool, or evolve toward a
   broader end-user product surface?

## Related documents

- `alpha-mu-completion-plan.md`
- `alpha-mu-acceptance.md`
- `alpha-mu-roadmap.md`
- `action-plan.md`
- `alpha-mu-information-state.md`
- `alpha-mu-benchmark-baseline.md`

