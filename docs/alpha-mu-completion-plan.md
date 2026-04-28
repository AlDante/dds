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

## Audited status snapshot (2026-04-28)

The repository is no longer at the pre-Stage-0 starting point described by the
original execution order. After auditing the docs, code, regression suites, and
current git state, the stage status is:

| Stage | Status | Notes |
| --- | --- | --- |
| 0 | **Complete** | Feature matrix, bridge-search invariants, debug assertions, and semantic-gate regressions are present. |
| 1 | **Complete** | The default decision-point / root-report path now enables the bridge-backed ancestor-cut and root-cut controls, the focused and full regression suites both cover that default path, and exact-only TT storage remains preserved for explicit conservative contexts. |
| 2 | **Complete** | One staged world pipeline now feeds explanation, compaction, reporting, and benchmark logs. |
| 3 | **Complete at current planned scope** | Richer information-state contract, bidding/play-derived narrowing, deterministic capping, explanation improvements, and a real-board recommendation change are all present. |
| 4 | **Complete at current planned scope** | Plausibility semantics are documented, reporting is richer, and the explicit root-only weighted policy is regression-backed. |
| 5 | **Complete at current planned scope** | Practical depth-2 and depth-3 bridge-backed continuation regressions are present and deterministic; follow-on move-order / TT work remains optional unless the scope is expanded again. |
| 6 | **Complete** | Decision logs and benchmark surfaces expose world-pipeline, front, TT, cut, and timing metrics. |
| 7 | **Complete** | The M1 Max benchmark/profile/measurement program is documented and closed. |
| 8 | **Complete** | Module ownership, user/developer docs, data-flow docs, and Doxygen coverage are in place. |
| 9 | **Open** | `S9.1`-`S9.3` are landed and Stage 1 is now closed, but `S9.4` release tagging has not yet been performed. |

## Current position

At a high level, the repository now has:

- the main alpha-mu front data structures,
- toy-search implementations of the paper semantics and many optimization-paper
  cuts,
- bridge-state search with DDS leaf evaluation,
- decision-point analysis entry points,
- partial-information world construction and filtering,
- complete Workstream 6 instrumentation,
- the Stage 7 M1 Max performance baseline and measurement log,
- the Stage 8 repository-grade documentation set,
- and a materially improved module split.

The remaining substantive implementation gap is now operational rather than
algorithmic: the default decision-point / root-report path has been reconciled
with the Stage 1 bridge-backed ancestor-cut/root-cut machinery, so the only
remaining plan item is the Stage 9 release-tag step.

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

## Current remaining execution plan

The original Stage 0 / Stage 1 bootstrapping sequence has already been carried
out. From the current audited repository state, the remaining work is:

1. perform the `Stage 9` release-tag step for the now-closed repository-grade
   implementation state.

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
| Optimistic completion | Yes | Yes | Implemented in bridge search | The default decision-point / root-report path now enables ancestor-front comparison, while explicit conservative contexts remain available for exact regressions |
| Early cut | Yes | Yes | Implemented in bridge search | The default bridge-backed decision path now uses nearest-ancestor early cut with exact-only TT storage |
| Deep alpha cut | Yes | Yes | Implemented in bridge search | The default bridge-backed decision path now applies deep alpha cuts against earlier ancestor Max fronts |
| Cut-on-win | Yes | Yes | Implemented in bridge search | Bridge Max-node search now stops once a child front wins in every useful world |
| Root cut | Yes | Yes | Implemented in bridge search | Bridge iterative deepening now applies root-cut on stable root `mu` in the default decision-point / root-report path |
| Exact-front TT reuse | Yes | Yes | Implemented in bridge search | Bridge TT keys exact entries by useful-world mask, refuses optimistic-cut fronts, and the default decision path now uses those exact-only reuse semantics |
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
- `bridge deep alpha cut OK`
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

### Status update (2026-04-28)

This stage is complete:

- the optimization-paper feature matrix exists and is kept current in this
  document,
- the bridge-search invariants are documented here and in
  `docs/alpha-mu-invariants.md`,
- the debug-only bridge-state / trick / front / context assertions are present
  in `test/alpha_mu_bridge.cpp`,
- and the semantic-gate regressions remain green in the full suite.

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

### Status update (2026-04-28)

This stage is now complete:

- `S1.1` and `S1.2` are part of the default bridge search: bridge-search
  context, useful-world maintenance, zero-world cut, and single-world cut.
- `S1.3` through `S1.6` are now also part of the default decision-point /
  root-report path: optimistic completion, early cut, deep alpha cut,
  cut-on-win, and root cut are all bridge-backed and regression-backed.
- `S1.7` is closed: TT storage refuses optimistic-cut fronts, keys exact entries
  by useful-world mask, and the default decision path now uses the same
  exact-only reuse semantics while explicit conservative contexts remain
  available for exact regression probes.

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

### Status update (2026-04-26)

This stage is now functionally landed at the currently planned scope:

- the decision-point runner, explanation path, and compacted bridge-search state
  now share one staged world pipeline,
- raw constructor-world IDs now survive into the final compacted search-world
  set,
- `AlphaMuSolveResult` now reports truthful per-stage counts from that shared
  pipeline,
- decision output now includes machine-readable stage-count summaries,
- and regressions now check both pipeline stability and the bridge-backed deep
  alpha cut in the focused `bridge_dds` bundle.

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

### Status update (2026-04-27)

The first `S3.1` slice is now in place at the contract/documentation level:

- the repository documentation now describes the lifecycle of
  `BridgeInformationState` from play-derived base state through override
  application, constructor pruning, staged filtering, and compacted
  bridge-search handoff,
- code comments now document the same field ownership and stage ordering at the
  API and implementation boundaries,
- and the current hard/soft boundary is explicitly recorded: plausibility is
  still reporting-only, while staged filtering and replay remain hard world-set
  semantics.

Also now present as the first narrow `S3.2` slice:

- the regression suite includes a combined full-hand bidding-profile case using
  exact club length, exact HCP, and balanced-shape constraints on the same
  history-derived fixture,
- that case verifies constructor-local pruning, later staged bidding filtering,
  and explanation accounting stay aligned under a richer scoped auction-side
  profile,
- and it broadens bidding-derived regression coverage without introducing any
  new auction interpretation semantics.

Also now present as the first narrow `S3.3` slice:

- legality-derived first-show-out evidence is carried into constructor-local
  pruning as an explicit void-suit fact,
- the regression suite now checks that this earlier pruning agrees with the
  later staged follow-suit replay on the same fixture,
- and the explanation paths now remain aligned: constructor pruning and staged
  filtering both describe the narrowing as a void-suit legality consequence,
  not as a heuristic inference.

Also now present as the first narrow `S3.5` slice:

- decision reporting now emits friendlier stage names and passed-path summaries
  for both surviving and rejected worlds,
- rejected worlds are now described in plain language as being rejected at a
  particular stage because of a particular hard fact,
- and regression coverage now checks that those richer explanation strings stay
  aligned with the structured explanation data rather than inventing new
  semantics in reporting.

Also now present as the remaining Stage 3 closeout evidence slice:

- the regression suite includes a real-board decision on board 2 of
  `hands/alpha_mu_play.txt` at a 36-card prefix where richer defender-spade
  information changes the recommended move,
- that case keeps sampling out of the explanation by staying below the world cap
  on both sides of the comparison,
- and it traces the change to constructor-local plus staged bidding narrowing
  rather than to accidental sampling noise.

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

### Status update (2026-04-27)

The first `S4.1` through `S4.3` slices are now in place at the current scoped
policy:

- the information-state contract now states explicitly that plausibility remains
  hard/soft separated from world admission and alpha-mu front semantics,
- decision reporting now exposes plausibility-driven world ranking in plain
  language,
- and the decision-point API now supports an explicit experimental root-only
  weighted choice policy that falls back to plain `mu` when plausibility carries
  no surviving-world signal.

Also now present as the `S4.4` comparison slice:

- the CLI decision runner now accepts explicit decision-policy and plausibility
  options,
- the regression suite includes a real-board board-2 prefix-36 case where a soft
  plausibility hint favoring a spadeless East leaves world membership unchanged
  but the explicit weighted policy changes the chosen move,
- and that change is locked to the explanation trace rather than being an
  unreported heuristic side effect.

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

### Status update (2026-04-28)

This stage is complete at the current planned scope:

- the regression suite includes practical real-board depth-2 continuation cases
  on board 1 of `hands/alpha_mu_play.txt` for both trick-boundary and
  partial-trick prefixes,
- the regression suite also includes the `S5.5` practical multi-world depth-3
  continuation case on board 1 of `hands/alpha_mu_play.txt`,
- those cases check repeated-run stability, deeper root-child coverage,
  DDS-leaf activity, TT storage, and root world-summary alignment,
- and the current Stage 5 definition of done is satisfied: the repository now
  exercises practical bridge-backed continuation search beyond the earlier
  narrow depth-2 showcase slice.

Additional move-order or TT experiments remain valid follow-on work only if the
scope is widened again; they are not currently blocking Stage 5 closure.

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

### Status update (2026-04-27)

The current machine-readable decision log now also exposes:

- front-insert attempt / accept / dominance counters,
- merge/product / optimistic-completion counters,
- and the explicit applied decision policy together with the chosen weighted
  score.

This keeps the recently added weighted root policy and front-churn metrics
visible to scripts as well as to the human-readable report.

Also now present in the benchmark surfaces:

- machine-readable benchmark board and summary lines expose aggregate search-node
  counts, DDS-leaf counts, DDS-leaf seconds, and bridge-search-only seconds,
- the live benchmark runner persists those metrics into its status JSON for
  post-processing,
- and regression coverage now locks the reporting format plus the depth-1 metric
  population path.

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

### Status update (2026-04-27)

This stage is now complete. The full M1 Max performance program has been
executed, documented, and closed:

- **S7.1** (benchmark suite): the canonical benchmark suite is frozen in
  `docs/alpha-mu-benchmark-baseline.md`, defining the primary serial CPU-time
  workload (`list9.txt` depth 2), the secondary board-parallel throughput
  workload, the deep validation workload (`list10.txt` depth 3), and the PMU
  single-board workload, along with acceptance criteria for future changes.

- **S7.2** (profiling): the sampled hot path was profiled and documented across
  multiple sessions in `docs/performance-log.md`. The dominant path is
  `SearchBridgeStateInternal` -> `MakeBridgeDDSLeafFront` -> `SolveBoardPBN` ->
  `ABsearch*`, with DDS leaf evaluation consuming 85-95% of total search time.
  Front operations, world filtering, and bridge-state copy are minor.

- **S7.3-S7.5** (optimization and measurement): multiple optimization
  candidates were evaluated using serial-mode CPU time and PMU counters, with
  results recorded in `docs/performance-log.md`. The M1 Max-specific `ABsearch`
  path delivered -20.7% wall time. Packed `moveType` reduced instructions by
  -1.1% and store misses by -1.0% while remaining wall-time-neutral. P-core
  QoS pinning delivered ~4-5% under contention. All other candidates (hot/cold
  ThreadData, pos reorder, DepthLocal, NEON intrinsics, CLZ intrinsic,
  QuickTricks context-struct) were reverted after measurement showed regressions
  or no improvement.

- **S7.6** (M1 Max-specific paths): the only justified M1 Max-specific path is
  `DDS_TARGET_APPLE_M1_MAX` in `src/ABsearch_m1max.cpp`, auto-selected on
  Apple arm64 via `M1_MAX_BUILD=1`. No alpha-mu-specific M1 Max paths were
  justified by measurement.

- **S7.7** (PGO evaluation): PGO trailed the non-PGO M1 Max build by ~2.9%
  and is not part of the default release build. The PGO pipeline remains
  available for experimentation.

Key insight: IPC is flat (3.36-3.43) across all variants. The M1 Max is not
memory-stalling — the correct strategy is to minimize instruction count. The
board-parallel wall-clock benchmarks that were used initially had ~20-30%
run-to-run variance and masked regressions; all future A/B comparisons must use
serial-mode CPU time via `getrusage`.

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

### Status update (2026-04-27)

This stage is now complete at the current planned scope:

- **S8.1** (module ownership): `docs/architecture.md` now includes a
  file-level responsibility table for all `test/alpha_mu_*.{h,cpp}` files,
  mapping each module to its ownership boundary. All alpha-mu source files have
  `@file` and `@brief` doxygen comments.

- **S8.2** (bridge-player guide): `docs/alpha-mu-guide.md` provides a
  plain-language explanation of alpha-mu for bridge players: what it is, how it
  differs from DDS, what possible worlds are, what the output means, how to
  read a decision-point analysis, current limitations, and a terminology
  reference.

- **S8.3** (developer algorithm/invariant docs):
  `docs/alpha-mu-invariants.md` consolidates the optimization-paper feature
  matrix, code invariants, module-to-paper-concept mapping, core data-structure
  contracts, search-control flow, semantic-gate regressions, and extension
  points into a standalone developer reference.

- **S8.4** (data-flow documentation): `docs/alpha-mu-dataflow.md` provides an
  ASCII-art pipeline diagram from input hand file through world construction,
  staged filtering, bridge search, DDS leaf calls, and decision reporting, with
  phase-timing annotations and module-ownership cross-references.

- **S8.5** (Doxygen coverage): `docs/Doxyfile` INPUT list now includes all
  alpha-mu source files: `alpha_mu_core.cpp`, `alpha_mu_decision.cpp`,
  `alpha_mu_reporting.cpp`, `alpha_mu_support.cpp`, and `alpha_mu_tests.cpp`
  in addition to the previously listed files. `docs/mainpage.md` links all
  new documentation.

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

### Status update (2026-04-28)

This stage is not yet complete:

- **S9.1** (acceptance checklist): `docs/alpha-mu-acceptance.md` contains the
  live acceptance status matrix, plus a stage completion summary and
  known-limitations section.

- **S9.2** (documentation gaps): all remaining `@file`/`@brief` doxygen
  comments have been added to every `test/alpha_mu_*.{h,cpp}` file; the
  Doxyfile INPUT list was already complete from Stage 8; `docs/mainpage.md`
  now links the acceptance checklist.

- **S9.3** (regression/benchmark gaps): the full regression suite
  (`./build/alpha_mu`) and the focused bridge-DDS suite
  (`./build/alpha_mu bridge_dds`) both pass green.

- **S9.4** (release tag): this operational step is still pending.

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

## Remaining work after this audit

Only one item remains on the completion plan's critical path:

1. **Close Stage 9 operationally** by creating the intended release tag for the
   now-closed implementation state.

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

