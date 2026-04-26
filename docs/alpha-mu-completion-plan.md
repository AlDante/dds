# Alpha-Mu Completion Plan

## Purpose

This document defines the route from the current alpha-mu implementation state to
an implementation that is:

- complete rather than toy-only,
- correct before fast,
- fast enough to be practically useful,
- tuned as far as justified on Apple M1 Max,
- engineered to a first-class repository standard,
- and documented clearly enough for an ordinary bridge player to follow the
  high-level algorithmic ideas.

It complements `docs/action-plan.md`, which remains the short-cycle execution
plan, by describing the **full end-state plan**.

## Non-negotiable requirements

Every stage in this plan must satisfy the following principles:

1. **Correctness first**
   - no optimization is accepted without regression coverage,
   - no pruning rule is accepted without a bridge-backed correctness argument,
   - debug-only assertions should be used liberally where they do not affect
     release performance.

2. **Paper faithfulness where claimed**
   - when the implementation claims to use an optimization from the papers, that
     optimization must exist in the real bridge-backed search, not only in the
     toy harness.

3. **Measured performance only**
   - no M1 Max optimization is kept without benchmark evidence,
   - every accepted speedup must be recorded in the performance documents.

4. **Repository-grade engineering**
   - durable module boundaries,
   - small safe commits,
   - readable names,
   - stable interfaces,
   - documentation that serves both developers and bridge analysts.

5. **Bridge-player readability**
   - every major algorithmic subsystem must have a plain-language explanation,
   - reports must explain not only what move was chosen, but why.

## Engineering and documentation are continuous obligations

`Stage 8` is a **closure and completeness stage**, not permission to defer
engineering quality or documentation until late in development.

The rules are therefore:

- every stage must preserve or improve module boundaries,
- every stage must preserve or improve code readability,
- every stage must add or refresh documentation when behavior changes,
- and every stage must leave behind regression-backed code that is safe for the
  next stage to build upon.

`Stage 8` exists to close the remaining gaps and prove that these standards have
been maintained throughout, not to introduce them for the first time.

## Current position

At a high level, the repository already has:

- the main alpha-mu front data structures,
- toy-search implementations of the paper semantics and many optimization-paper
  cuts,
- bridge-state search with DDS leaf evaluation,
- decision-point analysis entry points,
- partial-information world construction and filtering,
- initial Workstream 6 instrumentation,
- and a materially improved module split.

What is **not yet complete** is the transfer of the optimization-paper machinery
from the toy harness into the real bridge-backed alpha-mu search, together with
full instrumentation, realistic world modeling, and first-class repository
integration.

## Definition of complete alpha-mu

Alpha-mu is considered complete only when all of the following are true:

1. the real bridge search implements the optimization-paper semantics that are
   currently only fully present in the toy harness,
2. the decision-point runner constructs realistic worlds from meaningful bidding
   and play information,
3. world weighting / plausibility improves recommendation quality rather than
   merely decorating reports,
4. the engine handles practical bridge continuation families beyond the current
   showcase depth-2 slice,
5. the implementation is fully instrumented and benchmark-backed,
6. the code is organized into durable engine modules,
7. the documentation is complete for both developers and ordinary bridge
   players,
8. and all claims about correctness and performance are regression-backed.

## Immediate execution plan for Stage 0 and Stage 1

The next concrete execution cycle should be:

1. complete `S0.1` by publishing the optimization-paper feature matrix,
2. complete `S0.2` by documenting bridge-search invariants in both docs and
   code comments,
3. complete `S0.3` by adding the missing debug-only bridge-search / TT / front
   assertions,
4. start `S1.1` by extending the bridge-search context so later paper
   optimizations have an explicit place to live,
5. implement the first bridge-backed optimization-paper slice only after the new
   assertions and regression gates are in place,
6. validate after every slice with the focused `partial` and `bridge_dds`
   bundles and then the full `alpha_mu` suite.

The immediate execution order inside those stages is:

- `S0.1` feature matrix,
- `S0.2` invariant documentation,
- `S0.3` assertions and semantic-gate tests,
- `S1.1` bridge-search context scaffolding,
- then the first real bridge-backed optimization port in `S1.2`.

## Optimization-paper feature matrix (Stage 0 / S0.1)

| Feature | Toy search | Bridge search | Status | Notes |
| --- | --- | --- | --- | --- |
| World masks | Yes | Yes | Implemented in bridge search | Core set representation is shared |
| Outcome vectors | Yes | Yes | Implemented in bridge search | Shared front representation |
| Pareto fronts | Yes | Yes | Implemented in bridge search | Shared Max/Min front operations |
| Max-node union | Yes | Yes | Implemented in bridge search | Used directly in bridge search |
| Min-node product/min | Yes | Yes | Implemented in bridge search | Used directly in bridge search |
| Useful-world maintenance | Yes | Yes | Implemented in bridge search | Real bridge Min-node search now shrinks the live useful-world mask across child exploration |
| Zero-world cut | Yes | Yes | Implemented in bridge search | Bridge search now applies the cut against the effective useful-world mask |
| Single-world cut | Yes | Yes | Implemented in bridge search | Bridge search now collapses one useful world to an exact DDS-backed single-world front |
| Optimistic completion | Yes | Partial | Partial | Bridge search now has opt-in optimistic completion for ancestor-front comparison, but exact root-report integration is still pending |
| Early cut | Yes | Partial | Partial | Bridge search now has opt-in nearest-ancestor early cut with exact-only TT storage, but the exact root-report path does not yet opt in |
| Deep alpha cut | Yes | Partial | Partial | Bridge search now has opt-in deep alpha cuts against earlier ancestor Max fronts, but the exact root-report path does not yet opt in |
| Cut-on-win | Yes | Yes | Implemented in bridge search | Bridge Max-node search now stops once a child front wins in every useful world |
| Root cut | Yes | Partial | Partial | Bridge iterative deepening now applies a root-cut stop on stable root `mu`, but reporting remains intentionally conservative about deeper inexact root-child coverage |
| Exact-front TT reuse | Yes | Partial | Partial | Bridge TT now keys the Stage 1 world-cut slice by useful-world mask and refuses to store optimistic-cut fronts, but later ancestor-front slices still need tighter reuse semantics |
| DDS-backed leaf evaluation | N/A | Yes | Implemented in bridge search | Core real-engine leaf oracle |
| Bridge root reporting | N/A | Yes | Implemented in bridge search | Practical reporting is already present |

This matrix is the semantic baseline for the next implementation stage: `Stage 1`
must move the features currently marked **Toy only** or **Partial** into the real
bridge-backed search where appropriate.

## Bridge-search invariants and semantic gates (Stage 0 / S0.2)

The real bridge search currently depends on the following invariants.

### Invariants

1. **World-mask consistency**
   - `BridgeState.worlds.size()` must match `BridgeState.possibleWorlds.count`,
   - the active-world mask must never mention a world outside the compacted
     vector,
   - and any bridge search state must fit within the 64-world `WorldMask`
     representation.

2. **Partial-trick consistency**
   - `currentTrick` and `currentTrickPlayers` must remain aligned,
   - the partial trick must never contain more than one card per seat,
   - and `leadSuit`, `trickLeader`, and `playerToMove` must stay consistent with
     the current partial trick.

3. **Sparse-front validity**
   - every front returned from bridge search must use the same world count as the
     searched state,
   - every outcome vector stored in that front must use the same world count,
   - and every valid-world bit in those vectors must refer to an active world in
     the state.

4. **TT exactness assumptions**
   - the current bridge TT stores practical exact fronts for the simplified
     bridge search currently implemented,
   - later optimization-paper cuts must not reuse those entries under stronger
     assumptions without tightening the TT key/value semantics first.

5. **DDS leaf legality**
   - DDS leaf handoff must only occur on legal bridge states,
   - normalized remaining seat counts must agree across the four seats once the
     current partial trick is accounted for,
   - and the serialized `dealPBN` leaf state must faithfully reflect the bridge
     continuation state.

### Current semantic gate regressions

The following regressions currently act as Stage 0 semantic gates and must stay
green before and after every Stage 1 slice:

- `bridge move generation OK`
- `bridge search control OK`
- `bridge root reporting OK`
- `partial-information world generation OK`
- `follow-suit narrowing in partial information OK`
- `end-to-end alpha-mu solve OK`
- `decision-point comparison reporting OK`
- `explicit decision-point request API OK`
- `practical multi-world depth-2 continuation OK`
- `practical partial-trick depth-2 continuation OK`
- `bridge transposition table OK`
- and the debug-only `debug world-mask assertion regression OK`

These are the minimum semantic gates for `Stage 0 / S0.3` and all `Stage 1`
bridge-backed optimization ports.

---

## Stage 0 — semantic baseline and acceptance envelope

### Goal

Freeze the semantic target before adding more optimization logic.

### Why this stage exists

The repository now contains both toy alpha-mu semantics and bridge-backed search.
Before adding more paper optimizations, we need a precise statement of what the
real bridge engine is expected to do and which invariants must remain true.

### Work items

1. Write a feature matrix mapping each optimization-paper concept to one of:
   - implemented in toy search only,
   - implemented in bridge search,
   - partially implemented,
   - not implemented.
2. Document the exact bridge-search invariants:
   - world-mask correctness,
   - legal-world replay correctness,
   - sparse-front validity,
   - TT exactness assumptions,
   - DDS leaf handoff legality.
3. Identify all existing regressions that serve as semantic gates.
4. Add any missing debug-only assertions needed to make later refactors safer.

### Commit-sized slices

- `S0.1` Add the feature matrix to docs.
- `S0.2` Add bridge-search invariant notes to code/docs.
- `S0.3` Add missing debug assertions for bridge search / TT / front validity.

### Definition of done

This stage is done when:

- every optimization-paper feature is classified as toy-only, bridge-backed,
  partial, or missing,
- the real bridge search invariants are documented,
- and later stages can state precisely what they are porting into the real
  engine.

---

## Stage 1 — port optimization-paper search control into the real bridge engine

### Goal

Make the **real bridge-backed search** implement the optimization-paper control
rules, not just the toy harness.

### Required capabilities

The bridge search must support, where semantically safe:

- useful-world maintenance,
- zero-useful-world and single-useful-world cuts,
- optimistic completion of sparse fronts,
- early cut,
- deep alpha cut,
- cut-on-win,
- root cut in bridge iterative deepening,
- and exact-front reuse semantics compatible with those cuts.

### Work items

1. Introduce a bridge-search context that carries:
   - useful worlds,
   - upper Max fronts / ancestor fronts,
   - optimistic values where required,
   - exactness state for TT storage.
2. Port useful-world shrinking into real bridge Min-node search.
3. Port world cuts into real bridge search.
4. Port optimistic completion and comparison against ancestor Max fronts.
5. Port early cut and deep alpha cut into bridge search.
6. Port cut-on-win into bridge Max-node search.
7. Port root-cut logic into real bridge iterative deepening.
8. Tighten bridge TT storage rules so stored fronts are correct under the new
   cut semantics.

### Commit-sized slices

- `S1.1` Add bridge-search context for useful worlds and exactness.
- `S1.2` Add useful-world maintenance and bridge world cuts.
- `S1.3` Add optimistic completion and early cut.
- `S1.4` Add deep alpha cut.
- `S1.5` Add cut-on-win.
- `S1.6` Add bridge root-cut in iterative deepening.
- `S1.7` Tighten TT exact-storage semantics and document them.

### Definition of done

This stage is done when:

- each optimization-paper search control rule exists in the real bridge search,
- every such rule is covered by bridge-backed regressions,
- TT reuse remains correct under those rules,
- and the bridge search still matches current exact DDS baselines in one-world
  mode where applicable.

---

## Stage 2 — align the decision-point world pipeline with staged filtering

### Goal

Make the decision-point runner report **truthful staged world metrics** and use
one coherent world-construction pipeline.

### Why this stage matters

Today, the staged world filter exists, but the decision-point runner does not yet
use one fully unified reporting path from raw constructor worlds to final active
worlds.

### Work items

1. Refactor decision-point state assembly so the same world pipeline is used for:
   - constructor-local candidate generation,
   - staged world filtering,
   - deterministic downselection,
   - explanation generation,
   - and final bridge-state creation.
2. Ensure the same world IDs / serialized worlds survive across:
   - explanation reports,
   - active state worlds,
   - root reporting.
3. Report per-stage surviving counts in the decision runner.
4. Record those same counts in machine-readable benchmark logs.

### Commit-sized slices

- `S2.1` Unify candidate-world indexing across explanation and final state.
- `S2.2` Route decision-state assembly through one staged filter pipeline.
- `S2.3` Expose per-stage counts in `AlphaMuSolveResult`.
- `S2.4` Emit stage counts in benchmark / decision-point logs.

### Definition of done

This stage is done when:

- the decision-point runner can report candidate and surviving world counts after
  every filter stage,
- those numbers are truthful for the actual worlds handed to search,
- and regressions prove explanation ordering and final active-world ordering stay
  deterministic.

---

## Stage 3 — realistic information-state construction

### Goal

Move from curated partial-information examples toward realistic declarer
information states.

### Work items

1. Expand the information-state contract in docs and code comments.
2. Improve bidding-derived inference within the scoped external-auction model:
   - HCP ranges,
   - hand types,
   - suit-length min/max,
   - partnership constraints.
3. Improve play-derived inference from longer histories:
   - repeated follow-suit evidence,
   - discard implications,
   - ownership implications from earlier plays.
4. Support larger ambiguous defender pools while preserving deterministic capping.
5. Extend explanation traces so the user can understand world admission/rejection.

### Commit-sized slices

- `S3.1` Expand the information-state contract documentation.
- `S3.2` Add richer bidding regression cases.
- `S3.3` Add richer play-history narrowing.
- `S3.4` Support larger ambiguous pools with deterministic capping.
- `S3.5` Improve plain-language world explanations.

### Definition of done

This stage is done when:

- the engine routinely constructs realistic world sets from longer histories,
- explanation traces are stable and readable,
- and at least one real-board decision changes for a reason that can be traced
  to richer information rather than accidental sampling noise.

---

## Stage 4 — world plausibility and weighting

### Goal

Make alpha-mu reason not only over membership in the world set, but over how
plausible the surviving worlds are.

### Work items

1. Define the intended semantics of plausibility:
   - reporting-only,
   - tie-breaking,
   - or weighted decision policy.
2. Start with stable reporting and tie-breaking.
3. Add an experimental weighted decision mode guarded by explicit configuration.
4. Compare weighted versus unweighted recommendation quality on a curated set.

### Commit-sized slices

- `S4.1` Finalize weighting semantics in docs.
- `S4.2` Improve plausibility explanation output.
- `S4.3` Add explicit weighted decision mode.
- `S4.4` Add regression / analysis cases comparing weighted and unweighted recommendations.

### Definition of done

This stage is done when:

- plausibility is no longer just decorative,
- weighted behavior is explicit rather than silent,
- and recommendation changes caused by weighting are explainable and regression-backed.

---

## Stage 5 — broader and deeper practical bridge search

### Goal

Move beyond the current practical depth-2 slice to a search that matters on a
broader family of real decisions.

### Work items

1. Add more bridge-backed continuation motifs.
2. Extend mixed merge/split multi-trick cases.
3. Deepen practical real-board continuation coverage.
4. Improve move ordering where it helps measured bridge search.
5. Extend TT reuse only where correctness is preserved.

### Commit-sized slices

- `S5.1` Add more bridge-backed motif regressions.
- `S5.2` Add deeper mixed merge/split continuation regressions.
- `S5.3` Improve move ordering under instrumentation.
- `S5.4` Improve bridge TT reuse on measured workloads.
- `S5.5` Add practical depth-3 benchmark and regression coverage.

### Definition of done

This stage is done when:

- the engine covers more than the current narrow depth-2 practical family,
- bridge-backed regressions exercise the optimization-paper logic under real
  continuation search,
- and repeated runs remain deterministic under fixed seeds and options.

---

## Stage 6 — complete Workstream 6 instrumentation

### Goal

Make alpha-mu performance visible enough that optimization decisions are driven
by evidence rather than intuition.

### Work items

1. Complete world-pipeline metrics:
   - candidate counts,
   - per-stage surviving counts,
   - sampled-out counts.
2. Complete front metrics:
   - front sizes,
   - dominance reduction counts,
   - useful-world shrink counts.
3. Complete TT metrics:
   - probes,
   - hits,
   - stores,
   - exact/non-exact storage decisions.
4. Complete cut metrics:
   - cut counts by type in the real bridge search.
5. Complete timing splits:
   - world generation,
   - bridge search excluding DDS,
   - DDS leaves,
   - optionally reporting / explanation generation if nontrivial.
6. Emit these metrics both in human-readable decision output and machine-readable
   benchmark logs.

### Commit-sized slices

- `S6.1` Finish per-stage world metrics.
- `S6.2` Add front-size and useful-world metrics.
- `S6.3` Add TT exactness / reuse metrics.
- `S6.4` Add full cut breakdown for bridge search.
- `S6.5` Add complete phase timing to machine-readable logs.

### Definition of done

This stage is done when:

- the decision runner reports all major alpha-mu cost centers,
- benchmark logs can explain where time is spent,
- and every retained optimization can be judged against those metrics.

---

## Stage 7 — Apple M1 Max performance program

### Goal

Push the implementation as far as justified on Apple M1 Max **without**
sacrificing correctness or maintainability.

### Rules for this stage

- no change is accepted because it “looks faster”,
- every change must be benchmark-backed,
- every accepted change must survive correctness and regression tests,
- architecture-specific paths must remain isolated and documented.

### Work items

1. Establish the benchmark suite that matters for alpha-mu, not just DDS.
2. Profile realistic decision-point workloads on M1 Max.
3. Optimize only measured bottlenecks, likely including:
   - front operations,
   - world filtering,
   - bridge-state copying / mutation,
   - DDS leaf orchestration,
   - TT access patterns,
   - parallel scheduling overhead.
4. Evaluate M1 Max-specific code paths only where data supports them.
5. Use PGO and toolchain-supported optimization where reproducible.
6. Keep PMU / CPU-time / wall-time records in the documentation.

### Commit-sized slices

- `S7.1` Freeze the alpha-mu benchmark suite for M1 Max.
- `S7.2` Profile and document top alpha-mu bottlenecks.
- `S7.3` Optimize the hottest alpha-mu bottleneck.
- `S7.4` Re-measure and either keep or revert.
- `S7.5` Repeat for the next bottleneck.
- `S7.6` Add M1 Max-specific path only if generic code is measurably worse.
- `S7.7` Evaluate PGO build support for alpha-mu workloads.

### Definition of done

This stage is done when:

- alpha-mu performance on M1 Max is benchmark-backed and near the practical
  limit justified by the current algorithm,
- no retained optimization is unsupported by data,
- and the performance documentation clearly records what was tried, what helped,
  and what was reverted.

---

## Stage 8 — first-class engineering and documentation

### Goal

Make alpha-mu a maintainable, understandable, first-class repository component.

### Work items

1. Finish moving alpha-mu from a test-area engine path toward stable repository
   ownership boundaries.
2. Keep module boundaries crisp:
   - front semantics,
   - worlds,
   - bridge search,
   - reporting,
   - benchmarking,
   - CLI / API.
3. Add user-facing documentation for ordinary bridge players:
   - what alpha-mu is,
   - how it differs from DDS,
   - what “possible worlds” means,
   - what the root report means,
   - how to read a decision-point analysis.
4. Complete developer-facing documentation:
   - algorithm references,
   - invariants,
   - data-flow diagrams,
   - performance notes,
   - extension points.
5. Ensure generated documentation remains available and up to date.

### Commit-sized slices

- `S8.1` Tighten module ownership and file-level responsibilities.
- `S8.2` Add bridge-player guide to alpha-mu output and concepts.
- `S8.3` Complete developer algorithm/invariant docs.
- `S8.4` Add diagrams / data-flow documentation.
- `S8.5` Ensure docs generation covers the complete alpha-mu surface.

### Definition of done

This stage is done when:

- a developer can modify the engine without reverse-engineering a monolith,
- an ordinary bridge player can follow the meaning of the output,
- and the repository documentation describes both the algorithm and the actual
  implementation structure accurately.

---

## Stage 9 — final acceptance and release gate

### Goal

Prove that the implementation is complete, correct, efficient, and supportable.

### Acceptance checklist

1. **Correctness**
   - full regression suite green,
   - bridge-backed optimization-paper regressions green,
   - one-world parity against DDS maintained where required,
   - no unresolved semantic mismatches.
2. **Performance**
   - benchmark suite green,
   - instrumentation complete,
   - accepted optimizations benchmark-backed,
   - M1 Max documentation current.
3. **Documentation**
   - bridge-player guide complete,
   - developer docs complete,
   - generated docs usable.
4. **Engineering**
   - module boundaries stable,
   - no oversized catch-all implementation unit remaining,
   - CLI / API entry points stable.
5. **Usability**
   - a real decision point can be analyzed reproducibly,
   - the output is understandable and useful,
   - explanations connect move choice, worlds, and uncertainty.

### Commit-sized slices

- `S9.1` Add final acceptance checklist document and status matrix.
- `S9.2` Close remaining documentation gaps.
- `S9.3` Close remaining regression or benchmark gaps.
- `S9.4` Tag the first repository-grade alpha-mu release state.

### Definition of done

This stage is done when all of the following are true:

- the real bridge engine implements the optimization-paper search controls,
- realistic world construction and weighting are present,
- practical bridge-backed search goes beyond the current showcase slice,
- the implementation is fully instrumented and benchmark-backed,
- the code is durable and well documented,
- and alpha-mu is usable as a first-class repository feature rather than a toy,
  prototype, or isolated test harness.

## Recommended execution order

The safest practical order is:

1. `Stage 0` semantic baseline,
2. `Stage 1` bridge-search optimization-paper port,
3. `Stage 2` unified staged world pipeline,
4. `Stage 6` finish instrumentation,
5. `Stage 3` richer information-state construction,
6. `Stage 4` weighting / plausibility,
7. `Stage 5` broader practical bridge search,
8. `Stage 7` M1 Max performance program,
9. `Stage 8` first-class engineering and documentation,
10. `Stage 9` final acceptance.

That ordering keeps the implementation correct and measurable while the harder
bridge-search optimizations are being introduced.

