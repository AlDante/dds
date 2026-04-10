# Key Data Structures

## Public API structures (`include/dll.h`)

### Deals and solve results

- `deal`
  - Binary in-memory representation of a position.
  - Holds trump, leader, current trick cards, and remaining cards by hand/suit.

- `dealPBN`
  - PBN-string representation of a position.

- `futureTricks`
  - Result structure returned by solve functions.
  - Contains candidate cards and the trick score associated with each move.

### Batch solve and table structures

- `boards`, `boardsPBN`
  - Arrays of board requests for batch solve APIs.

- `solvedBoards`
  - Array of `futureTricks` results.

- `ddTableDeal`, `ddTableDealPBN`
  - Input for double-dummy table calculation.

- `ddTableResults`, `ddTablesRes`
  - Per-deal and batched table results.

### Par and play-analysis structures

- `parResults`
  - Par score and contracts by side-to-bid view.

- `parResultsDealer`
  - Dealer-sensitive par representation.

- `parResultsMaster`, `parTextResults`
  - Binary and text conversion support for par output.

- `playTraceBin`, `playTracePBN`, `solvedPlay`, `solvedPlays`
  - Inputs and outputs for double-dummy play analysis.

### Runtime and configuration structures

- `DDSInfo`
  - Describes the compiled/runtime configuration:
    - version,
    - platform,
    - compiler,
    - threading backend,
    - thread count,
    - memory/thread sizing.

## Internal solver structures (`src/dds.h`)

### `pos`

`pos` is the core recursive position object used during search.

Important fields include:

- `rankInSuit[DDS_HANDS][DDS_SUITS]`
  - Remaining card bitsets by hand and suit.
- `aggr[DDS_SUITS]`
  - Aggregated suit holdings used for TT lookup and quick-rank logic.
- `length[DDS_HANDS][DDS_SUITS]`
  - Per-hand suit lengths.
- `handDist[DDS_HANDS]`
  - Encoded hand-distribution signature used in TT lookup.
- `winRanks[50][DDS_SUITS]`
  - Winning-rank masks by depth.
- `first[50]`
  - Leader for each ply.
- `move[50]`
  - Current winning move by ply.
- `tricksMAX`
  - Tricks already secured by the MAX side.

This structure is the best place to understand what recursive search state DDS actually carries.

### `moveType`

Represents one candidate card:

- suit,
- rank,
- sequence information,
- move-ordering weight.

### `relRanksType`

Per-thread helper used by move generation and equivalence handling.

## Per-thread state (`src/Memory.h`)

### `ThreadData`

`ThreadData` packages the mutable data that must stay thread-local during solving.

Key members:

- `lookAheadPos`
  - The active `pos` object.
- `transTable`
  - Pointer to the configured TT backend.
- `moves`
  - Move generation and ordering engine.
- `lowestWin`, `winners`
  - Proof/pruning helpers.
- `bestMove`, `bestMoveTT`
  - Root and TT-guided move-order information.
- `nodes`, `trickNodes`
  - Search counters.
- `rel`
  - Large precomputed equivalence data.

The alpha-mu work should avoid destabilizing `ThreadData` unless a benchmark justifies deeper redesign.

## Transposition-table structures (`src/TransTable.h`)

### `nodeCardsType`

This is the essential stored TT payload:

- `ubound`
- `lbound`
- `bestMoveSuit`
- `bestMoveRank`
- `leastWin[DDS_SUITS]`

For alpha-mu planning, the important point is that DDS already stores **bounds**, not just exact values.

### `TransTable`

Abstract interface implemented by:

- `TransTableS`
- `TransTableL`

This abstraction isolates the search from TT storage layout.

## Scheduling structures (`src/Scheduler.h`)

### `schedType`

Returned by the scheduler when a worker thread requests its next hand:

- `number`
- `repeatOf`

### Scheduler internals

The scheduler tracks grouped hands, predicted costs, repeats, and thread assignment state. This matters for throughput and should be kept conceptually separate from alpha-mu search-policy work.
