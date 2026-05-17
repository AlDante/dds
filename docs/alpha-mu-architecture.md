# Alpha-Mu Architecture and Implementation

## Scope

This page explains the repository's alpha-mu implementation as it exists today:

- a front-based imperfect-information search over sampled possible worlds,
- a bridge-specific world-construction pipeline,
- DDS-backed perfect-information leaf evaluation,
- and reporting, comparison, and benchmark tooling around that engine.

## Public surface and module map

The supported include surface is:

- `include/alpha_mu/core.h`
- `include/alpha_mu/api.h`
- `include/alpha_mu/bridge.h`

Those wrappers expose the production implementation in `src/alpha_mu_*.{h,cpp}`.
The test area keeps the CLI runner and regression harnesses, not the production
engine itself.

## Core data model

### World representation

`WorldMask`
: a compact 64-bit set of active world indices.

`ParsedWorld`
: a concrete bridge deal in suit-string form.

`BridgeState`
: the searched bridge continuation state, combining:
- remaining cards per world,
- currently possible worlds,
- side to move,
- MAX-side trick count,
- trump and trick-leader metadata,
- current partial trick.

### Value representation

`OutcomeVector`
: one score per world, with a validity mask.

`ParetoFront`
: a non-dominated set of `OutcomeVector` values.

This is the algorithmic heart of alpha-mu. The solver does not back up a single
scalar score. It backs up a front of incomparable world-score vectors.

### Constraint and world-pipeline representation

`WorldConstraint`
: hard legality/filter facts.

`WorldPlausibilityHint`
: soft ranking-only signals used for explanation and deterministic ordering.

`BridgeInformationState`
: the full partial-information package used to generate and filter candidate
worlds before search.

`DecisionWorldPipelineResult`
: the shared handoff from world generation to the bridge search.

## Module responsibilities

| File | Responsibility |
| --- | --- |
| `src/alpha_mu_core.h` | Shared types, public entry points, and algorithm contracts |
| `src/alpha_mu_core.cpp` | Search orchestration, iterative deepening, TT use, root cuts |
| `src/alpha_mu_front.cpp` | Pareto-front operations and paper-level toy search helpers |
| `src/alpha_mu_worlds.cpp` | Candidate-world construction, filtering, constraint replay, explanations |
| `src/alpha_mu_bridge.cpp` | Bridge-state transitions, legal moves, DDS leaf evaluation |
| `src/alpha_mu_decision.cpp` | Decision-point assembly and root solve dispatch |
| `src/alpha_mu_reporting.cpp` | Human-readable and machine-readable reporting |
| `src/alpha_mu_support.cpp` | Configuration, option parsing, utility helpers |
| `test/alpha_mu.cpp` | CLI runner and benchmark command surface |
| `test/alpha_mu_tests.*` | Regression bundle and semantic gate tests |

## Search semantics

### Max nodes

At Max nodes, child fronts are unioned and then reduced by dominance.
In code this is `ParetoFront::MaxMerge()`.

### Min nodes

At Min nodes, child fronts are combined by pairwise per-world minima and then
Pareto-reduced. In code this is `ParetoFront::MinProduct()`.

### Useful worlds

The optimization-paper idea of useful worlds is implemented through
`ParetoFront::UsefulWorlds()` and the related `RestrictToUseful()` path. This
allows the solver to discard worlds that can no longer influence the Min backup.

### Optimistic completion

Sparse fronts can be compared by optimistically filling in missing worlds. That
behavior is represented by `OutcomeVector::CompleteOptimistically()` and the
corresponding front-level helper.

### Cut families

The implementation tracks several cut families explicitly in
`BridgeSearchStats`:

- early alpha cuts,
- deep alpha cuts,
- cut-on-win cuts,
- root cuts,
- empty-world cuts,
- TT cuts,
- DDS-leaf cuts.

## World-construction pipeline

The bridge-specific front end is the largest repository-specific extension around
paper alpha-mu. The pipeline works in stages:

1. build a visible seed world,
2. identify hidden cards and allowed hidden seats,
3. enumerate or prune hidden-card assignments,
4. apply known-card constraints,
5. apply bidding constraints,
6. derive and apply follow-suit constraints,
7. replay completed-trick history,
8. replay the current partial trick,
9. optionally deduplicate and rank worlds,
10. deterministically downselect to the 64-world search cap.

That staged design exists for three reasons:

- correctness: each filter family has a clear contract,
- explainability: the code can show why a world survived or failed,
- performance: cheap constructor-local pruning happens before expensive replay.

## DDS leaf integration

Alpha-mu does not replace DDS. It calls DDS as an exact evaluator for
perfect-information leaves.

The handoff is:

1. one active world is materialized into a DDS-compatible deal state,
2. DDS solves that perfect-information position,
3. the returned score is written into an `OutcomeVector`,
4. the vector is inserted into a front for the bridge-backed alpha-mu search.

This division of labor is central to the current architecture:

- DDS remains the exact perfect-information solver,
- alpha-mu adds imperfect-information world reasoning above it.

## Transposition table

The alpha-mu TT differs from DDS in both key and payload semantics.

DDS stores bounds keyed by a compressed perfect-information state.
Alpha-mu instead stores exact fronts keyed by the bridge state plus active-world
information. The key idea is that the same visible card position can have a
meaningfully different value if the surviving useful worlds differ.

## Reporting and inspection

The implementation exposes several explanation-oriented structures:

- `WorldGenerationStats`
- `WorldGenerationExplanation`
- `WorldExplanation`
- `BridgeRootReport`
- `BridgeRootChildReport`
- `DDSVsAlphaMuComparison`

These make the solver easier to reason about than a pure performance-oriented
prototype because the caller can inspect:

- which worlds survived,
- which constraints rejected worlds,
- which root move produced which front,
- and how much work was spent in DDS leaves versus alpha-mu search.

## Relationship to DDS architecture

The clean mental model is:

- DDS answers exact perfect-information bridge questions quickly,
- alpha-mu wraps DDS in an imperfect-information layer,
- world generation and front backup are alpha-mu-specific,
- deep card-play evaluation remains DDS's job.

## Recommended reading order

1. `include/alpha_mu/core.h`
2. `src/alpha_mu_core.h`
3. `src/alpha_mu_front.cpp`
4. `src/alpha_mu_worlds.cpp`
5. `src/alpha_mu_bridge.cpp`
6. `src/alpha_mu_core.cpp`
7. `src/alpha_mu_decision.cpp`
8. `src/alpha_mu_reporting.cpp`
9. `test/alpha_mu.cpp`
10. `test/alpha_mu_tests.cpp`

For visual call-flow and ownership diagrams, see [Architecture diagrams](architecture-diagrams.md).

