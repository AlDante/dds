# Alpha-Mu Information-State Contract

## Purpose

This document defines the current repository-level contract for the
**information state** that feeds alpha-mu world construction.

The goal is to make it explicit which inputs are treated as:

- **hard constraints** that reject impossible worlds,
- **derived hard constraints** that follow deterministically from play,
- and **soft plausibility hints** that rank surviving worlds for reporting
  without changing the hard world set.

This is the contract behind `BridgeInformationState` in
`test/alpha_mu_core.h`.

## Current structure

The current information-state package contains these fields:

- `knownCardConstraints`
- `biddingConstraints`
- `followSuitConstraints`
- `plausibilityHints`
- `playHistory`
- `currentTrickHistory`
- `deriveFollowSuitConstraints`
- `deduplicateEquivalentWorlds`
- `sampleLimit`
- `samplingSeed`

## Lifecycle through the current solver

The current decision-point path consumes `BridgeInformationState` in five
distinct phases:

1. **Play-derived base state construction**
   - `BuildInformationStateFromPlay()` creates the default visible-card facts,
     play-history replay facts, current-trick replay facts, and deterministic
     sampling controls for one decision point.

2. **Explicit override application**
   - `ApplyInformationOverrides()` appends externally supplied hard constraints
     and plausibility hints onto that play-derived base state.
   - This is the boundary where auction analysis or other external inference may
     add deterministic facts without the alpha-mu solver needing to interpret an
     auction object itself.

3. **Constructor-local pruning**
   - `ConstructCandidateWorldsFromHistory()` uses only the subset of facts that
     are safe before the full staged pipeline, so it can reduce the candidate
     pool without changing the meaning of later filtering.

4. **Shared staged world pipeline**
   - `BuildDecisionWorldPipeline()` applies the same ordered filtering stages
     used by decision reporting and by compacted bridge-state assembly:
     known-card checks, bidding checks, follow-suit checks, complete-history
     legality replay, current-trick legality replay, optional deduplication, and
     deterministic downselection.

5. **Compacted search-state assembly**
   - `MakeBridgeStateFromInformationState()` takes only the worlds accepted by
     that pipeline, compacts them into the 64-world search representation, and
     passes them into bridge search.

This means the information-state contract is no longer only about candidate
generation. It also defines the exact facts that the decision runner reports,
the exact world identities that survive into search, and the exact stage counts
now emitted in `ALPHA_MU_DECISION`.

## Field-by-field contract

### `knownCardConstraints`

- hard constraints,
- safe for constructor-local pruning when they refer to hidden-seat feasibility,
- always applied again in the shared staged filter,
- and expected to reflect cards that are already visible or otherwise known with
  certainty.

### `biddingConstraints`

- hard constraints,
- appended by explicit override application,
- partially usable during constructor-local pruning when they imply hidden-seat
  feasibility,
- and always applied again in the staged filter as the authoritative hard
  auction-side boundary.

### `followSuitConstraints`

- hard constraints,
- may be supplied explicitly by callers,
- may also be augmented from deterministic legality-derived follow-suit
  inference when `deriveFollowSuitConstraints` is enabled,
- and are applied in the shared staged filter before full history replay.

### `plausibilityHints`

- soft inputs only,
- appended by override application,
- never used to reject worlds,
- never used to alter sampling membership,
- never used in TT semantics or alpha-mu front backup,
- and currently only used to explain and rank already-surviving worlds.

### `playHistory`

- hard legality replay for fully completed prior tricks,
- consumed after known-card / bidding / follow-suit filtering,
- and intended to represent only the already completed portion of the decision
  point history, not the currently open trick.

### `currentTrickHistory`

- hard legality replay for the currently open partial trick,
- consumed after completed-trick replay,
- and intended to preserve the exact decision-point handoff into bridge search.

### `deriveFollowSuitConstraints`

- controls whether deterministic legality-derived follow-suit implications are
  added from the recorded play,
- defaults to `true` in play-derived information states,
- and is turned off in the final compaction handoff once those explicit derived
  constraints have already been materialized.

### `deduplicateEquivalentWorlds`

- controls whether equivalent surviving worlds are collapsed after legality
  replay,
- must not change legality semantics,
- and exists to reduce redundant search work before deterministic sampling.

### `sampleLimit`

- is the deterministic downselection budget for the staged world pipeline,
- applies only after hard filtering and optional deduplication,
- and is distinct from the final 64-world hard cap required by `WorldMask`.

### `samplingSeed`

- determines the stable rotation used by deterministic downselection,
- must make repeated runs reproducible,
- and is therefore part of the information-state contract rather than only a
  reporting detail.

## Override semantics

`ApplyInformationOverrides()` currently has deliberately simple semantics:

- append `knownCardConstraints`,
- append `biddingConstraints`,
- append `followSuitConstraints`,
- append `plausibilityHints`,
- overwrite `deriveFollowSuitConstraints`,
- overwrite `deduplicateEquivalentWorlds`,
- overwrite `sampleLimit` from the explicit decision-point world budget,
- overwrite `samplingSeed` from the explicit decision-point seed,
- and **do not merge or replace** `playHistory` / `currentTrickHistory`.

This keeps the play-derived replay facts authoritative while still allowing
external components to add auction-side or analyst-supplied facts.

## Hard constraints

These inputs are currently treated as **hard**: a world that violates them is
removed from the candidate set.

### 1. Known-card constraints

`knownCardConstraints` represent card-location facts that are already known to
the analyst from visible cards or from explicit external inference.

Examples:

- declarer still holds a visible card,
- dummy still holds a visible card,
- a defender is known to hold a specific card,
- a defender is known **not** to hold a specific card.

These map to deterministic `WorldConstraint` checks such as:

- `HasCard`
- `NotHasCard`

### 2. Bidding constraints

`biddingConstraints` represent auction-derived facts that the current model is
willing to enforce as deterministic filters.

Examples currently supported:

- seat-level suit-length bounds,
- seat-level HCP bounds,
- balanced / hand-type requirements,
- partnership suit-length bounds,
- partnership HCP bounds.

These are only safe when the interpretation is strong enough to support
regression-backed deterministic tests.

### 3. Explicit follow-suit constraints

`followSuitConstraints` contains manually supplied hard implications from play.
These are applied in the same filtering pipeline as the other hard constraints.

### 4. Derived follow-suit constraints

If `deriveFollowSuitConstraints` is enabled, the implementation derives extra
hard constraints from observed discards and failures to follow suit.

Current meaning:

- if a player discards on a suit lead, then after accounting for any earlier
  cards they already played in that suit, the world is bounded so that the
  player cannot still hold additional unseen cards in the led suit.

These derived implications are hard because they follow from bridge legality,
not from stylistic inference.

### 5. Play-history legality

`playHistory` is replayed as a hard filter.

A surviving world must allow the recorded prior play to happen legally:

- the recorded player must hold the recorded card at the point it was played,
- follow-suit legality must be respected during replay.

### 6. Current-trick legality

`currentTrickHistory` is replayed after the prior history.

A surviving world must also be compatible with the already-started current
partial trick.

## Constructor-local hard pruning

`ConstructCandidateWorldsFromHistory()` is allowed to apply only the subset of
hard constraints that are safe before the full staged world-generation pass.

Current constructor-local pruning may use:

- hidden-seat played-card ownership,
- hidden-seat card-location constraints,
- hidden-seat suit-length feasibility,
- hidden-seat HCP feasibility,
- hidden-seat balanced / hand-type feasibility,
- partnership range feasibility when the hidden seats are involved,
- follow-suit implications that are already forced by the observed history.

This pruning is still required to preserve deterministic behavior and must not
change the meaning of the later full filtering stages.

## Ordered staged filtering contract

After constructor-local pruning, the shared staged world pipeline applies the
remaining checks in this order:

1. known-card constraints,
2. bidding constraints,
3. explicit plus derived follow-suit constraints,
4. completed-trick legality replay,
5. current-trick legality replay,
6. optional deduplication,
7. deterministic downselection.

That ordering matters for both the user-facing explanation trace and the staged
metrics recorded in `WorldGenerationStats` and `ALPHA_MU_DECISION`.

In particular:

- deduplication happens only after legality filtering,
- deterministic sampling happens only after deduplication,
- and the final search state must still be compacted to at most 64 worlds even
  if the earlier sample budget is larger.

## Soft plausibility hints

`plausibilityHints` are the first explicit **soft** layer.

Each hint contains:

- a `WorldConstraint`,
- a positive integer `weight`,
- and an optional human-readable `label`.

Current behavior:

- if a surviving world matches the hint, it gains that weight,
- if it does not match the hint, the world is **not rejected**,
- plausibility is used only for explanation and deterministic ranking of the
  already-surviving worlds.

This means plausibility is currently **reporting-only**.
It does **not**:

- alter `GeneratePossibleWorlds()`,
- alter constructor-local acceptance,
- alter alpha-mu front semantics,
- alter move choice silently.

## Deterministic ranking rule

When plausibility hints are present, accepted worlds are ranked by:

1. descending plausibility score,
2. canonical serialized world text,
3. original world index.

This keeps repeated runs reproducible.

## Hard versus soft summary

Use **hard constraints** when the input means:

- “this world is impossible if the inference is correct”.

Use **soft plausibility hints** when the input means:

- “this world is more or less likely, but still possible”.

Examples of likely hard inputs:

- visible remaining cards,
- proven follow-suit implications,
- exact card-showing evidence,
- deterministic auction ranges already covered by tests.

Examples of likely soft inputs:

- style-based opening preferences,
- lead tendencies,
- discard tendencies,
- marginal auction-shape preferences,
- ranking among multiple still-legal worlds that all satisfy the hard facts.

## Current limitations

The current contract is intentionally conservative.

Not yet modeled here as first-class inputs:

- a structured auction object with convention meaning,
- negative-inference strength from choice among equivalent plays,
- probability calibration from frequency data,
- direct use of plausibility in alpha-mu backup or move selection.

Also not yet first-class in the contract:

- explicit negative-inference semantics for card-choice alternatives that were
  available but not chosen,
- richer ownership implications from repeated later-play patterns,
- larger ambiguous defender pools beyond the currently regression-backed cases,
- and weighted use of plausibility in move choice.

## Next intended use

The next practical step is to feed plausibility summaries into the future
single-decision post-mortem runner so it can report:

- surviving worlds,
- why they survived,
- and which surviving worlds look most plausible,

before any decision is made about letting plausibility influence move choice.

