# DDS Solver Invariants

This guide collects the classic DDS solver invariants that changes should preserve.

## Deal inputs

- `deal.trump` is `0..4`, with `4` meaning notrump.
- `deal.first` is `0..3` for North, East, South, West.
- `currentTrickRank[k] == 0` means slot `k` is unused.
- Current-trick slots are contiguous from index `0`.
- A card present in the current trick must not also remain in any hand.
- For a complete deal, each seat has the same normalized card count after adding already-played current-trick cards.

## Search depth and seat indexing

- `iniDepth` is the number of cards below the root ply, so `cardCount = iniDepth + 4`.
- `trick = (iniDepth + 3) >> 2` is the number of whole tricks remaining.
- `handRelFirst = (48 - iniDepth) % 4` is the number of cards already played in the current trick.
- `handId(first, relative)` maps the leader seat plus a relative offset back to the absolute seat id.
- `lookAheadPos.first[depth]` tracks the trick leader for that ply depth.
- `tricksMAX` always counts tricks already secured by the MAX side.

## Relative seat helpers

- `partner[seat]`, `lho[seat]`, and `rho[seat]` are absolute-seat lookup tables.
- `MAXNODE` and `MINNODE` are assigned per absolute seat according to the side currently maximizing the returned trick count.
- `handRelFirst` values `0..3` select the specialized `ABsearch*()` entry points.

## Winning-rank metadata

- `winRanks[depth][suit]` is a bitmask of ranks that are immediately winning at the current ply depth.
- `winner[suit]` and `secondBest[suit]` must stay synchronized with the current `rankInSuit` / `length` position representation.
- `lowestWin[depth][suit]` tracks the least rank that still proves the target.

## Transposition-table semantics

- TT entries store lower and upper bounds for the North/South viewpoint.
- Retrieved bounds must only be reused when the stored aggregate target and hand distribution match the current node semantics.
- A reset reason should explain why cached entries were discarded: new deal, new trump, too many nodes, explicit free, or memory exhaustion.

## Quick-trick and later-trick pruning

- `QuickTricks()` may only claim tricks forced immediately from the current position.
- `LaterTricksMAX()` and `LaterTricksMIN()` are proof shortcuts, not heuristic guesses: each return value must remain score-sound.
- Any change near those routines should be checked against the endgame oracle regressions in `test/alpha_mu_tests.cpp` and the public API gate in `test/regression_api.cpp`.

## Public result semantics

- `solutions == 3` returns all legal cards with their exact optimum scores.
- `solutions == 2` returns all cards achieving the requested target or optimum.
- `solutions == 1` returns a single optimum-achieving card.
- `mode == 0` may short-circuit when only one legal move exists.
- `futureTricks.score[i]` is interpreted from the side-to-move perspective at the supplied root state.

## Batch solver expectations

- `SolveAllBoards*()` preserves board order.
- `CalcAllTables*()` preserves table order.
- `AnalyseAllPlays*()` preserves board/play pairing and board order.
- `regression_api` is the minimum correctness gate for solver changes, especially before performance work.
