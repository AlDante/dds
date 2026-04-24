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
`test/alpha_mu_prototype_core.h`.

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

## Next intended use

The next practical step is to feed plausibility summaries into the future
single-decision post-mortem runner so it can report:

- surviving worlds,
- why they survived,
- and which surviving worlds look most plausible,

before any decision is made about letting plausibility influence move choice.

