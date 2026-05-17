# Key Data Structures

## Public DDS API structures (`include/dll.h`)

### Solve request and result types

- `deal`
  - Binary in-memory position description.
  - Stores trump, opening leader, a possible partial current trick, and remaining
    cards as rank bitmasks by seat and suit.

- `dealPBN`
  - PBN-string representation of the same logical state.
  - Preferred when the caller already works with textual bridge notation.

- `futureTricks`
  - Output of `SolveBoard*()`.
  - `cards` indicates the number of candidate cards returned.
  - Each populated slot carries suit, rank, equivalence information, and the
    resulting double-dummy trick score.

### Batched solve and table types

- `boards`, `boardsPBN`
  - Batch inputs for the `SolveAll*()` family.
  - Hold per-board target, solution count, and mode arrays alongside the deals.

- `solvedBoards`
  - Batch output parallel to `boards` / `boardsPBN`.

- `ddTableDeal`, `ddTableDealPBN`
  - One deal in the shape expected by the DD-table APIs.

- `ddTableResults`
  - `resTable[strain][declarer]` stores the maximum tricks attainable for each
    declarer and strain.

- `ddTableDeals`, `ddTableDealsPBN`, `ddTablesRes`
  - Batched companions for table workflows.

### Par and play-analysis types

- `parResults`
  - Text-oriented par score and contract strings by side-to-bid view.

- `parResultsDealer`
  - Dealer-sensitive par contract list.

- `contractType`, `parResultsMaster`, `parTextResults`
  - Structured par results and their text conversion formats.

- `playTraceBin`, `playTracePBN`
  - Played-card traces for the play-analysis APIs.

- `solvedPlay`, `solvedPlays`
  - Trick totals after each ply for one trace or a batch of traces.

- `DDSInfo`
  - Runtime/build metadata: platform, compiler, threading backend, thread count,
    and configured TT sizing.

## DDS internal solver structures (`src/dds.h`)

### `pos`

`pos` is the core recursive search object. It is the best place to understand
what state DDS actually carries during `ABsearch*()`.

Important fields:

- `rankInSuit[DDS_HANDS][DDS_SUITS]`
  - Remaining card bitmasks by seat and suit.
- `aggr[DDS_SUITS]`
  - Aggregated suit signatures used in TT lookup and rank reasoning.
- `length[DDS_HANDS][DDS_SUITS]`
  - Remaining length by seat and suit.
- `handDist[DDS_HANDS]`
  - Encoded distribution signature used by TT backends.
- `winRanks[50][DDS_SUITS]`
  - Rank masks of cards that still matter to the proof at each depth.
- `first[50]`
  - Leader for each ply.
- `move[50]`
  - Currently winning move in the trick at each ply.
- `tricksMAX`
  - Tricks already secured by the maximizing side.
- `winner[]`, `secondBest[]`
  - Best and second-best ranks in each suit.

### `moveType` and `movePlyType`

- `moveType`
  - Compact 64-bit move representation.
  - Holds suit, rank, sequence metadata, and move-order weight.
  - Packed for very short-list sorting in the inner loop.

- `movePlyType`
  - A small move list plus current cursor.
  - DDS uses these compact arrays rather than general containers because the
    branching factor is tiny and performance-sensitive.

### `evalType`

Terminal or exact evaluation bundle returned when DDS can settle the node without
further recursion.

### `relRanksType`

A precomputed relative-rank ownership table that helps `Moves` translate compact
aggregate suit patterns into seat-specific rank information efficiently.

## DDS per-thread state (`src/Memory.h`)

### `ThreadData`

`ThreadData` packages nearly everything mutable that a worker needs:

- the active `pos` (`lookAheadPos`),
- root/search bookkeeping such as `nodes` and `trickNodes`,
- move-order history (`bestMove`, `bestMoveTT`),
- proof helpers (`lowestWin`, `winners`),
- the move generator (`moves`),
- the configured TT backend (`transTable`),
- optional timing/debug/reporting outputs.

This thread-local boundary is one of DDS's core architectural assumptions.
Changes that cross it should be made cautiously.

### `Memory`

`Memory` owns the pool of `ThreadData` objects and resizes it to match the
configured threading/memory policy.

## DDS transposition-table structures (`src/TransTable.h`)

### `nodeCardsType`

This is the compact TT payload stored for a solved node:

- `ubound`
- `lbound`
- `bestMoveSuit`
- `bestMoveRank`
- `leastWin[DDS_SUITS]`

These are not arbitrary scores. They are proof-oriented bounds relative to the
current perfect-information node.

### `TransTable`

Abstract interface implemented by:

- `TransTableS`
- `TransTableL`

The search uses this abstraction so TT layout and sizing can vary independently
of recursive solver logic.

### `TransTableS` internals

- `posSearchTypeSmall`
  - BST node keyed by compacted suit-length signature.
- `winCardType`
  - Linked node for one winning-card pattern below a length signature.
- `ttAggrType`
  - Precomputed aggregate-rank and win-mask expansion for a 13-bit suit holding.

This backend is optimized for lower memory use and simpler incremental growth.

### `TransTableL` internals

- `distHashType`
  - Small hash bucket keyed by compact hand-distribution signature.
- `posSearchType`
  - One keyed distribution entry inside a bucket.
- `winMatchType`
  - Compact winning-card signature plus stored `nodeCardsType` payload.
- `winBlockType`
  - Fixed-size block of `winMatchType` entries.
- `poolType`
  - Page of allocatable TT blocks.
- `harvestedType`
  - Reclaimed blocks ready for reuse.

This backend is optimized for speed and cache reuse on larger memory budgets.

## DDS orchestration and scheduling structures

### `schedType` (`src/Scheduler.h`)

Returned by the scheduler for batch runs:

- `number`
  - next board index to process,
- `repeatOf`
  - earlier equivalent board to reuse, if any.

### `DDSInfo` (`include/dll.h`)

Although public, this structure is also operationally important to the internal
architecture because it summarizes the compiled threading and memory setup.

## Alpha-mu structures (`src/alpha_mu_core.h`)

### World and value structures

- `WorldMask`
  - Bitmask of active possible worlds.
  - Repository invariant: bridge search compacts to at most 64 active worlds.

- `OutcomeVector`
  - One score per world, plus validity information.
  - Used as the atomic value unit in front comparisons.

- `ParetoFront`
  - Non-dominated set of `OutcomeVector` values.
  - Max nodes union fronts.
  - Min nodes form pairwise per-world minima and Pareto-reduce.

### Search and TT structures

- `TTEntry`
  - Exact alpha-mu front payload for one stored key.

- `TranspositionTable`
  - Toy-search exact-front table keyed by node/depth/useful-world state.

- `BridgeSearchStats`
  - Counters for front operations, cuts, TT reuse, DDS leaf calls, and other
    optimization behavior.

- `SearchExecutionContext`
  - Explicit execution-mode bundle for DDS thread id, worker mode, benchmark
    reporting, and bridge-search controls.

### Bridge state and reporting structures

- `BridgeMove`
  - One bridge card in suit/rank form.

- `BridgeState`
  - Current alpha-mu continuation state: worlds, active mask, side to move,
    MAX-side trick total, trump, trick leader, and current partial trick.

- `BridgeChild`
  - One legal child move plus its successor state.

- `BridgeRootChildReport`, `BridgeRootReport`
  - Root-level per-move and aggregate reporting surfaces.

### World-generation structures

- `WorldConstraint`
  - Hard filter facts such as card ownership, voidness, suit length, or HCP.

- `WorldPlausibilityHint`
  - Soft ranking-only hints used for explanation and deterministic ordering.

- `BridgeInformationState`
  - Full partial-information package for constructing and filtering worlds.

- `HistoryDerivedWorldSpec`
  - Constructor-local seed for world generation from visible cards and hidden
    seat information.

- `WorldGenerationStats`, `HistoryDerivedConstructionStats`
  - Per-stage counters describing how many worlds survived each filter phase.

- `WorldExplanation`, `WorldGenerationExplanation`
  - Explanatory traces for accepted/rejected worlds.

- `DecisionWorldPipelineResult`
  - Shared handoff from world construction into bridge search.

## DDS vs alpha-mu: structural contrast

| Topic | DDS | Alpha-mu |
| --- | --- | --- |
| Searched state | One perfect-information deal | Set of possible worlds plus visible bridge state |
| Value type | Scalar trick bounds | Pareto front of outcome vectors |
| TT payload | `nodeCardsType` bounds + move hint | Exact front keyed by state/world information |
| Main pruning | Quick/later-trick proofs, move ordering, TT bounds | Useful worlds, optimistic completion, cut-on-win, deep alpha cuts |
| Leaf evaluator | Internal DDS exact search | DDS exact search called per world |

## Reading strategy

If you are trying to understand the code through the data structures first,
read in this order:

1. `include/dll.h`
2. `src/dds.h`
3. `src/Memory.h`
4. `src/TransTable.h`
5. `src/alpha_mu_core.h`
6. [DDS code flow](dds-code-flow.md)
7. [Alpha-mu architecture](alpha-mu-architecture.md)
