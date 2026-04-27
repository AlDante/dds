# Alpha-Mu Algorithm and Invariants Reference

## Purpose

This document consolidates the algorithm references, code invariants, and
module-to-paper mappings for the alpha-mu implementation. It serves developers
who need to modify the engine, understand its correctness properties, or trace
implementation choices back to the published papers.

For the bridge-player-oriented explanation, see
[alpha-mu-guide.md](alpha-mu-guide.md).

## Paper references

The implementation follows two papers:

1. **The original alpha-mu paper** introduces the imperfect-information search
   model: sampled possible worlds, outcome vectors, Pareto fronts, Max-node
   union backup, Min-node product/min backup, and the mu decision criterion.

2. **The optimization paper** adds useful-world maintenance, zero-world and
   single-world cuts, optimistic completion, early cut, deep alpha cut,
   cut-on-win, root cut in iterative deepening, and exact-front TT reuse
   semantics.

## Module-to-concept mapping

| Module file | Paper concept | Responsibility |
| --- | --- | --- |
| `alpha_mu_core.h` | All shared types | Defines `WorldMask`, `OutcomeVector`, `ParetoFront`, `BridgeState`, `TranspositionTable`, `BridgeSearchStats`, `AlphaMuSolveResult`, and all bridge-search entry points |
| `alpha_mu_core.cpp` | Bridge search + TT + solve orchestration | `SearchBridgeStateInternal`, `MakeBridgeDDSLeafFront`, TT storage/lookup, iterative deepening, root-cut logic |
| `alpha_mu_front.cpp` | Fronts and toy search | `ParetoFront` insert/merge/product/min, `SearchToy`, optimistic completion, outcome-vector operations |
| `alpha_mu_worlds.cpp` | World construction | `ConstructCandidateWorldsFromHistory`, `GeneratePossibleWorlds`, staged filtering pipeline, follow-suit inference, bidding narrowing |
| `alpha_mu_bridge.cpp` | Bridge state and legality | `MakeBridgeState`, `ApplyBridgeMove`, `GenerateBridgeMoves`, bridge-state serialization, DDS leaf handoff |
| `alpha_mu_decision.cpp` | Decision-point analysis | `SolveDecisionPoint`, information-state assembly, world pipeline, comparison reporting |
| `alpha_mu_reporting.cpp` | Output formatting | Decision report printing, root summary, per-world explanation, machine-readable log lines |
| `alpha_mu_support.cpp` | CLI and configuration | Parallel-mode naming, executable path, option parsing support |
| `alpha_mu_tests.h/.cpp` | Regression bundles | `RunDefaultTestSuite`, `RunBridgeDDSTestSuite`, individual semantic-gate regressions |
| `alpha_mu.cpp` | CLI entry point | Command parsing, mode dispatch, benchmark runner |

## Core data structures

### `WorldMask`

A 64-bit bitmask representing a subset of possible worlds. Each bit position
corresponds to one world in the compacted world vector. All world-set operations
(union, intersection, count, iteration) operate on this mask.

**Invariant**: `WorldMask` is backed by a single `uint64_t`, so at most 64
worlds may be active in any bridge search state.

### `OutcomeVector`

A per-world outcome array with a validity mask. `values[i]` holds the number of
tricks declarer makes in world `i`. `valid` is a `WorldMask` indicating which
entries are meaningful.

**Invariant**: every set bit in `valid` must correspond to an active world in
the enclosing search state.

### `ParetoFront`

An ordered collection of non-dominated `OutcomeVector` entries. No entry in
the front is dominated by any other entry. Insertion tests dominance and removes
newly dominated entries.

**Invariant**: after every insert or merge operation, no pair of entries in the
front has one dominating the other on all valid worlds simultaneously.

### `BridgeState`

The full state for one bridge-backed alpha-mu search node: the set of worlds,
each world's card distribution, the current trick state, and the active-world
mask.

**Invariant**: `worlds.size()` must match `possibleWorlds.count`, and
`activeWorlds` must be a subset of the compacted world vector indices.

### `TranspositionTable`

Maps bridge-state keys (serialized position + useful-world mask) to stored
Pareto fronts.

**Invariant**: stored fronts are exact under the current search semantics.
Optimistic-cut fronts are never stored. Later ancestor-front slices must
tighten reuse semantics before being stored.

## Bridge-search invariants

These invariants are documented in the completion plan (Stage 0 / S0.2) and
enforced by debug-only assertions.

### 1. World-mask consistency

- `BridgeState.worlds.size()` == `BridgeState.possibleWorlds.count`
- The active-world mask never mentions a world outside the compacted vector
- Any bridge search state fits within the 64-world `WorldMask` representation

### 2. Partial-trick consistency

- `currentTrick` and `currentTrickPlayers` remain aligned
- The partial trick never contains more than one card per seat
- `leadSuit`, `trickLeader`, and `playerToMove` stay consistent with the
  current partial trick

### 3. Sparse-front validity

- Every front returned from bridge search uses the same world count as the
  searched state
- Every outcome vector in that front uses the same world count
- Every valid-world bit in those vectors refers to an active world

### 4. TT exactness assumptions

- The bridge TT stores practical exact fronts
- Later optimization-paper cuts must not reuse entries under stronger
  assumptions without tightening TT key/value semantics first

### 5. DDS leaf legality

- DDS leaf handoff only occurs on legal bridge states
- Normalized remaining seat counts agree across all four seats once the
  current partial trick is accounted for
- The serialized `dealPBN` leaf state faithfully reflects the bridge
  continuation state

## Optimization-paper feature status

| Feature | Bridge search | Notes |
| --- | --- | --- |
| World masks | Implemented | Core set representation |
| Outcome vectors | Implemented | Shared front representation |
| Pareto fronts | Implemented | Shared Max/Min front operations |
| Max-node union | Implemented | Used directly in bridge search |
| Min-node product/min | Implemented | Used directly in bridge search |
| Useful-world maintenance | Implemented | Min-node shrinks live useful-world mask |
| Zero-world cut | Implemented | Applied against effective useful-world mask |
| Single-world cut | Implemented | Collapses to exact DDS-backed single-world front |
| Optimistic completion | Partial | Opt-in for ancestor-front comparison; exact root-report integration pending |
| Early cut | Partial | Opt-in nearest-ancestor early cut with exact-only TT storage |
| Deep alpha cut | Partial | Opt-in against earlier ancestor Max fronts |
| Cut-on-win | Implemented | Max-node stops once a child front wins in every useful world |
| Root cut | Partial | Iterative deepening applies root-cut on stable root mu |
| Exact-front TT reuse | Partial | World-mask keyed; later slices need tighter semantics |
| DDS-backed leaf evaluation | Implemented | Core real-engine leaf oracle |
| Bridge root reporting | Implemented | Practical reporting present |

## Search-control flow

1. **Entry**: `SolveAlphaMu` or `SolveDecisionPoint` called with a bridge
   information state
2. **World construction**: `ConstructCandidateWorldsFromHistory` generates
   candidates, then `BuildDecisionWorldPipeline` applies staged filtering
3. **State assembly**: `MakeBridgeStateFromInformationState` compacts surviving
   worlds into a `BridgeState`
4. **Iterative deepening**: search runs at increasing depths, applying root-cut
   when the root mu stabilizes
5. **Bridge search**: `SearchBridgeStateInternal` recursively explores
   Max/Min nodes, applying front operations and cuts
6. **DDS leaf calls**: `MakeBridgeDDSLeafFront` converts leaf positions to
   `SolveBoardPBN` calls and constructs leaf fronts from exact DDS results
7. **TT interaction**: exact fronts are stored; on TT hit, the stored front
   is returned without re-searching
8. **Root reporting**: the root front yields per-child outcome vectors,
   mu values, and the recommended move

## Semantic-gate regressions

The following regressions must stay green before and after every change:

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
- `debug world-mask assertion regression OK`

## Extension points

When adding new features, the safest insertion points are:

1. **New world-construction constraints**: add to
   `ConstructCandidateWorldsFromHistory` for constructor-local pruning, or
   to the staged filter pipeline in `BuildDecisionWorldPipeline`
2. **New search optimizations**: add inside `SearchBridgeStateInternal` with
   debug-only assertions that the optimization preserves front correctness
3. **New reporting fields**: add to `AlphaMuSolveResult` and the reporting
   functions in `alpha_mu_reporting.cpp`
4. **New CLI modes**: add to the command parser in `alpha_mu.cpp`

## References

- [Alpha-mu completion plan](alpha-mu-completion-plan.md) — full stage plan
- [Information-state contract](alpha-mu-information-state.md) — world inputs
- [Architecture](architecture.md) — DDS solver structure
- [Benchmark baseline](alpha-mu-benchmark-baseline.md) — M1 Max performance

