# DDS Architecture

## High-level view

DDS is organized as a layered solver library:

1. **Public API layer**
   - `include/dll.h`
   - Exposes C-compatible entry points and public data structures.

2. **Root orchestration layer**
   - `src/SolverIF.cpp`
   - `src/CalcTables.cpp`
   - `src/PlayAnalyser.cpp`
   - `src/DealerPar.cpp`, `src/Par.cpp`
   - Converts API requests into solver runs and result formatting.

3. **Search core**
   - `src/ABsearch.cpp`
   - `src/Moves.cpp`
   - `src/QuickTricks.cpp`
   - `src/LaterTricks.cpp`
   - Performs threshold-style recursive search with domain-specific pruning.

4. **State, memory, and caching**
   - `src/dds.h`
   - `src/Memory.h`
   - `src/TransTable.h`, `src/TransTableS.*`, `src/TransTableL.*`
   - Holds per-thread state, search position data, and transposition-table caches.

5. **Parallel execution and scheduling**
   - `src/System.h`, `src/System.cpp`
   - `src/Scheduler.h`, `src/Scheduler.cpp`
   - `src/Init.cpp`
   - Configures thread backends, allocates per-thread memory, and groups jobs for throughput.

6. **Tests and examples**
   - `test/`
   - `examples/`
   - Validate correctness and show public API usage.

## Solver flow for a single board

A typical `SolveBoard` / `SolveBoardPBN` request flows as follows:

1. Public caller fills a `deal` or `dealPBN` structure.
2. `SolveBoard` validates parameters and resolves the target thread context.
3. `SolveBoardInternal()` in `src/SolverIF.cpp`:
   - classifies the deal,
   - initializes `ThreadData`,
   - rebuilds deal tables when necessary,
   - runs root-level exact-score / threshold probing,
   - collects winning moves into `futureTricks`.
4. `ABsearch*()` in `src/ABsearch.cpp` recursively proves or disproves threshold targets.
5. `Moves`, `QuickTricks`, and `LaterTricks` prune the search and maintain move ordering.
6. `TransTable` implementations store and retrieve bounds for repeated positions.

## Why DDS is a useful alpha-mu building block

DDS is not structured as a plain score-returning minimax. Instead, much of the deep recursion already answers a boolean-style question:

- can the side to move force at least this many tricks from here?

That is the key reason the DDS-side support work started at the **root-level score-search policy** in `src/SolverIF.cpp` rather than replacing the recursive core wholesale.

However, the papers make clear that alpha-mu proper adds major machinery that DDS does not currently have:

- sampled possible worlds,
- outcome vectors,
- Pareto fronts,
- different Max/Min backup operators on those fronts,
- alpha-mu-specific cuts and TT behavior.

So DDS should currently be viewed as a **strong perfect-information engine that alpha-mu can call**, not as an alpha-mu implementation in disguise.

## Major subsystems

### Public interface

`include/dll.h` defines the stable exported API:

- single-board solve calls,
- batch solving and table-building calls,
- par calculation calls,
- play-analysis calls,
- configuration and diagnostics calls.

### Position and search state

`src/dds.h` contains the important internal search structures:

- `pos` — recursive search position,
- `moveType` / `movePlyType` — move generation and ordering state,
- `evalType`, `highCardType`, and support structs.

`src/Memory.h` defines `ThreadData`, which packages nearly all per-thread mutable solver state, including:

- current position buffers,
- transposition table handle,
- move generator,
- best-move history,
- timing / stats objects when enabled.

### Recursive search

`src/ABsearch.cpp` contains four specialized search entry points:

- `ABsearch`
- `ABsearch1`
- `ABsearch2`
- `ABsearch3`

These are specialized by the relative hand position inside the trick, avoiding a more generic but slower uniform recursion pattern.

### Pruning and proof helpers

DDS relies heavily on bridge-specific pruning helpers:

- `QuickTricks` identifies immediate forcing winners,
- `LaterTricksMAX` / `LaterTricksMIN` prove or refute targets deeper in the hand.

These are important to preserve during alpha-mu integration.

### Transposition tables

`src/TransTable.h` defines the abstract interface and `nodeCardsType`, which stores:

- lower bound,
- upper bound,
- best move,
- least-winning-rank metadata.

There are two concrete backends:

- `TransTableS` — smaller memory footprint,
- `TransTableL` — larger memory footprint.

### Parallel execution

`src/System.cpp` chooses and runs a threading backend. The macOS defaults now compile in:

- GCD (`DDS_THREADS_GCD`)
- STL threads (`DDS_THREADS_STL`)

`src/Scheduler.cpp` batches similar boards to improve throughput and reduce repeated work.

## Alpha-mu module ownership

The alpha-mu implementation currently lives under `test/` as a test-area engine
that calls DDS as a leaf oracle. The modules and their responsibilities are:

- focused wrapper headers under `test/alpha_mu/` now provide narrower module
  boundaries (`api.h`, `bridge.h`, `tests.h`) while the implementation remains
  in the historical incubation area;
- the umbrella `test/alpha_mu_core.h` still exists for compatibility, but new
  call sites should prefer the narrower wrappers where practical.

| File | Responsibility |
| --- | --- |
| `test/alpha_mu.cpp` | CLI entry point: command parsing, mode dispatch, benchmark runner |
| `test/alpha_mu_core.h` | Shared data structures: `WorldMask`, `OutcomeVector`, `ParetoFront`, `BridgeState`, `TranspositionTable`, `BridgeSearchStats`, `AlphaMuSolveResult`, and all callable entry points |
| `test/alpha_mu_core.cpp` | Bridge search engine: `SearchBridgeStateInternal`, `MakeBridgeDDSLeafFront`, TT storage/lookup, iterative deepening, root-cut logic, solve orchestration |
| `test/alpha_mu_front.cpp` | Front and toy-search helpers: `ParetoFront` insert/merge/product/min, `SearchToy`, optimistic completion, outcome-vector operations |
| `test/alpha_mu_worlds.cpp` | World construction and information-state helpers: `ConstructCandidateWorldsFromHistory`, `GeneratePossibleWorlds`, staged filtering pipeline, follow-suit inference, bidding narrowing |
| `test/alpha_mu_bridge.cpp` | Bridge-state transition and legality: `MakeBridgeState`, `ApplyBridgeMove`, `GenerateBridgeMoves`, DDS leaf handoff, bridge-state serialization |
| `test/alpha_mu_decision.cpp` | Decision-point analysis: `SolveDecisionPoint`, information-state assembly, world pipeline routing, comparison reporting |
| `test/alpha_mu_reporting.cpp` | Output formatting: decision report printing, root summary, per-world explanation, machine-readable log lines |
| `test/alpha_mu_support.cpp` | Configuration and utility: parallel-mode naming, executable path, option parsing support |
| `test/alpha_mu_tests.h` | Regression bundle declarations: `RunDefaultTestSuite`, `RunBridgeDDSTestSuite`, individual semantic-gate entry points |
| `test/alpha_mu_tests.cpp` | Regression bundle implementations: all focused and full-suite test cases |

For detailed algorithm references and invariants, see
[alpha-mu-invariants.md](alpha-mu-invariants.md). For the core DDS solver-side
invariant checklist, see [dds-invariants.md](dds-invariants.md). For the
data-flow diagram, see [alpha-mu-dataflow.md](alpha-mu-dataflow.md).

## Current architectural guidance for changes

When evolving DDS, the low-risk path is:

1. preserve public API compatibility,
2. preserve the bridge-specific pruning logic,
3. preserve transposition-table bound semantics,
4. improve root search policy first,
5. benchmark before changing the deep recursion.
