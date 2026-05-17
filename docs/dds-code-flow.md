# DDS Code Flow and Algorithms

## Purpose

This page documents how a DDS request moves through the codebase, which internal
algorithms do the work, and where the main data structures participate.

## End-to-end solve flow

A single-board solve request passes through these stages:

1. A caller fills `deal` or `dealPBN` from `include/dll.h`.
2. The public API entry point in the library validates the request.
3. `src/SolverIF.cpp` normalizes the root problem into DDS internal form.
4. A `ThreadData` slot from `src/Memory.h` is prepared or reused.
5. `src/Moves.cpp` and `src/ABsearch.cpp` drive the recursive proof search.
6. `src/QuickTricks.cpp` and `src/LaterTricks.cpp` try to settle thresholds early.
7. `src/TransTableS.*` or `src/TransTableL.*` cache bound information.
8. The winning moves are converted into `futureTricks` for the public caller.

## Root orchestration in `src/SolverIF.cpp`

`SolverIF.cpp` is the best place to understand the non-recursive control logic.
It is responsible for:

- validating targets and solution modes,
- reconstructing the current trick and root leader,
- preparing the per-thread `pos` state,
- choosing the root probing strategy,
- calling the recursive search repeatedly when needed, and
- formatting the best-card set in `futureTricks`.

The important architectural point is that DDS already behaves like a
threshold-search engine:

- the solver is often asked whether a side can force at least some target,
- root code may probe multiple thresholds,
- and TT entries store lower and upper bounds rather than only exact scores.

## Recursive search in `src/ABsearch.cpp`

DDS does not use one generic minimax function. Instead it specializes the
recursion by relative position in the current trick:

- `ABsearch0` — leader to the trick,
- `ABsearch1` — second hand,
- `ABsearch2` — third hand,
- `ABsearch3` — fourth hand.

That specialization matters because bridge legality and move-order heuristics
change materially across those four cases. The specialization avoids repeatedly
re-deriving:

- whether the hand must follow suit,
- which provisional card is currently winning,
- whether the hand is discarding or ruffing,
- and how the next trick leader is determined.

### Threshold semantics

The recursive question is not “what is the raw score of this node?” in the usual
textbook sense. Instead DDS often proves or disproves a target number of tricks.
That is why:

- `QuickTricks` and `LaterTricks` are phrased as proof helpers,
- `nodeCardsType` stores bounds,
- and root code can perform repeated probes to narrow the exact result.

## Move generation and ordering in `src/Moves.cpp`

`Moves` owns the per-thread move-generation engine. Its responsibilities are:

1. enumerate legal cards for a relative hand,
2. compress equivalent cards where possible,
3. assign heuristic weights,
4. sort the candidate list,
5. and update trick-local winner state as moves are consumed.

The move generator is performance-critical because the search touches it at every
node. The class therefore keeps compact trick-local arrays and short-list sort
paths rather than delegating to more generic containers.

## Bridge-specific pruning helpers

### `QuickTricks`

`QuickTricks` tries to prove that the side to move can cash enough immediate
winners without deeper search. It is the fastest strong cutoff in the solver.

### `LaterTricksMAX` / `LaterTricksMIN`

The later-trick helpers handle positions that are no longer trivial but still
admit domain-specific proofs. These routines are one of the reasons DDS is much
stronger than a naive alpha-beta implementation over the same game tree.

## Transposition-table semantics

DDS TT entries live behind `src/TransTable.h` and concrete backends in
`src/TransTableS.*` and `src/TransTableL.*`.

The key ideas are:

- the table is keyed by a compressed perfect-information state signature,
- entries store lower and upper trick bounds,
- entries also remember a best move and least-winning-rank metadata,
- and the search interprets the stored value relative to the current threshold.

This is a major architectural difference from alpha-mu:

- DDS TT = compact perfect-information bound cache,
- alpha-mu TT = exact-front cache over possible-world subsets.

## Batch scheduling and threading

For the multi-board APIs, DDS adds two more layers:

- `src/Scheduler.cpp` groups similar boards, identifies repeats, and assigns work,
- `src/System.cpp` selects a compiled threading backend and launches workers.

The scheduler is important because large workloads often contain structurally
similar deals. Solving those in a favorable order improves cache reuse and reduces
avoidable duplicated work.

## Batch solving in `src/SolveBoard.cpp`

The multi-board solve path is deliberately thin over the single-board engine.
Its job is to avoid recomputation rather than to invent a second search model.

Key responsibilities:

- validate batch size,
- register the workload with `Scheduler` and `System`,
- initialize result slots,
- run worker threads,
- copy results for exact duplicate boards when safe.

The duplicate reuse path is intentionally conservative. It only copies results
for positions that are known equivalent under the same declarer-to-play context.

## Double-dummy table calculation in `src/CalcTables.cpp`

Table calculation is not a separate search engine. Instead DDS:

1. solves one deal/strain for one declarer with `SolveBoard()`,
2. reuses the already prepared thread-local state with `SolveSameBoard()`,
3. rotates the leader/declarer across the four seats,
4. stores one trick result per seat into the table output.

That reuse is algorithmically important because the four declarer solves are
highly related.

## Play analysis in `src/PlayAnalyser.cpp`

The play-analysis path answers a different question than `SolveBoard()`: not
just the best continuation now, but how good the position was after each played
card in an actual line.

The algorithm is:

1. solve the current prefix,
2. apply the next played card,
3. update the current-trick winner and remaining-card state,
4. re-solve the continuation,
5. record the remaining-trick total after each prefix.

This makes `AnalysePlay*()` effectively a repeated incremental re-solve over the
trace rather than a single one-shot solve.

## Par calculation in `src/Par.cpp`

Par calculation consumes the finished double-dummy table and derives the bridge
contract(s) that neither side can improve upon through competitive bidding.

The implementation provides:

- `Par()` for text-oriented side views,
- `DealerPar()` / `DealerParBin()` for dealer-sensitive views,
- `SidesPar()` / `SidesParBin()` for side-to-bid summaries,
- conversion helpers that render compact machine-readable par data into text.

Architecturally, this means DDS separates:

- trick computation (`CalcDDtable*()`),
- from bidding-theoretic interpretation (`Par*()` functions).

## The two DDS TT backends

### `TransTableS`

The small backend minimizes memory. Conceptually it is organized by:

1. remaining trick count and leading hand,
2. compact suit-length signature,
3. winning-card pattern chain,
4. stored bound payload.

Use this backend when memory pressure matters more than absolute TT speed.

### `TransTableL`

The large backend maximizes lookup/update speed using more memory. It uses:

1. hashed hand-distribution buckets,
2. per-bucket distribution entries,
3. fixed-size blocks of compact winning-card signatures,
4. page-pool allocation and block harvesting.

The long opening comment in `src/TransTableL.cpp` is the canonical explanation of
its compact bit/mask encoding.

## Stable invariants when changing DDS

The following invariants are especially important:

1. public API results must remain unchanged,
2. `pos` and TT bound semantics must stay consistent,
3. move ordering may change performance but must not change correctness,
4. quick/later-trick proofs must remain aligned with the recursive threshold model,
5. benchmark claims should be checked on representative corpora.

## Key files to read together

For a practical code walk, read these in order:

1. `include/dll.h`
2. `src/SolverIF.h` and `src/SolverIF.cpp`
3. `src/dds.h`
4. `src/Memory.h`
5. `src/ABsearch.h` and `src/ABsearch.cpp`
6. `src/Moves.h` and `src/Moves.cpp`
7. `src/QuickTricks.h` / `src/LaterTricks.h`
8. `src/TransTable.h`, `src/TransTableS.*`, and `src/TransTableL.*`
9. `src/Scheduler.h` and `src/System.h`

For complementary architecture visuals, see [Architecture diagrams](architecture-diagrams.md).

