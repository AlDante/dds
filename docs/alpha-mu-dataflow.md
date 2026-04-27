# Alpha-Mu Data Flow

## Purpose

This document shows the end-to-end data flow of an alpha-mu decision-point
analysis, from the input hand file through to the final reported recommendation.

## High-level pipeline

```
Hand file + play prefix
        |
        v
+---------------------------+
| Play-derived base state   |  BuildInformationStateFromPlay()
| (visible cards, history,  |
|  follow-suit facts)       |
+---------------------------+
        |
        v
+---------------------------+
| Override application      |  ApplyInformationOverrides()
| (bidding constraints,     |
|  plausibility hints)      |
+---------------------------+
        |
        v
+---------------------------+
| Constructor-local pruning |  ConstructCandidateWorldsFromHistory()
| (card ownership, suit     |
|  lengths, HCP, voids)     |
+---------------------------+
        |
        v
+---------------------------+
| Staged world pipeline     |  BuildDecisionWorldPipeline()
|                           |
|  1. Known-card checks     |
|  2. Bidding checks        |
|  3. Follow-suit checks    |
|  4. History replay        |
|  5. Current-trick replay  |
|  6. Deduplication         |
|  7. Downselection (cap)   |
+---------------------------+
        |
        v
+---------------------------+
| Compacted bridge state    |  MakeBridgeStateFromInformationState()
| (<=64 worlds, WorldMask)  |
+---------------------------+
        |
        v
+---------------------------+
| Iterative deepening       |  SolveAlphaMu()
|                           |
|  for depth = 1, 2, ...    |
|    SearchBridgeState()    |
|    root-cut check         |
+---------------------------+
        |
        v
+-----------------------------------+
| Bridge search (recursive)         |  SearchBridgeStateInternal()
|                                   |
|  Max node:                        |
|    for each legal move:           |
|      apply move                   |
|      recurse (Min node)           |
|      union child front into root  |
|      cut-on-win check             |
|      deep alpha cut check         |
|                                   |
|  Min node:                        |
|    for each legal move:           |
|      shrink useful worlds         |
|      zero-world cut check         |
|      apply move                   |
|      recurse (Max node)           |
|      product/min child front      |
|    single-world cut check         |
+-----------------------------------+
     |              |
     v              v
+-----------+  +----------------+
| TT lookup |  | DDS leaf call  |
| (hit =>   |  | SolveBoardPBN  |
|  return   |  | => leaf front  |
|  stored   |  +----------------+
|  front)   |
+-----------+
        |
        v
+---------------------------+
| Root front                |
| (per-child outcome        |
|  vectors, mu values)      |
+---------------------------+
        |
        v
+---------------------------+
| Decision reporting        |
|                           |
| - Recommended move        |
| - Alternative moves       |
| - Per-world outcomes      |
| - World summaries         |
| - Stage counts            |
| - Timing splits           |
| - Plausibility ranking    |
+---------------------------+
```

## Phase timing splits

The decision-point path collects timing for the following phases:

| Phase | What it measures | Reported as |
| --- | --- | --- |
| World generation | Constructor + staged pipeline | `world_gen_seconds` |
| Bridge search (total) | All recursive search time | `search_seconds` |
| DDS leaf calls | Time inside `SolveBoardPBN` | `dds_leaf_seconds` |
| Bridge search (excluding DDS) | Search control, front ops, state copy | `search_seconds - dds_leaf_seconds` |

On measured workloads, DDS leaf evaluation dominates: typically 85-95% of total
search time is spent inside DDS `ABsearch*` calls. Front operations, world
filtering, and bridge-state copy are minor.

## Key data transformations

### World construction

```
Raw deal + play history
    => ParsedWorld candidates (full 52-card distributions)
    => Constructor-local pruning (reject impossible candidates early)
    => Staged filtering (ordered hard checks)
    => Deduplicated survivors
    => Downselected to budget
    => Compacted to WorldMask-indexed vector
```

### Bridge search

```
BridgeState (worlds + trick state + active mask)
    => Per-move child states (apply move, update trick)
    => Recursive front computation
    => Front backup (union at Max, product/min at Min)
    => TT store (exact fronts only)
    => Root front with per-child summaries
```

### Decision output

```
Root front
    => Per-child mu values (average across worlds)
    => Recommended move (highest mu)
    => Comparison vs actual play and DDS omniscient
    => Stage-count summary from world pipeline
    => Cut/termination counters from search stats
    => Machine-readable log line
```

## Module ownership summary

| Phase | Primary module | Key function |
| --- | --- | --- |
| CLI dispatch | `alpha_mu.cpp` | `main` |
| Information state | `alpha_mu_worlds.cpp` | `BuildInformationStateFromPlay` |
| World construction | `alpha_mu_worlds.cpp` | `ConstructCandidateWorldsFromHistory` |
| Staged filtering | `alpha_mu_worlds.cpp` | `BuildDecisionWorldPipeline` |
| Bridge state | `alpha_mu_bridge.cpp` | `MakeBridgeState` |
| Bridge search | `alpha_mu_core.cpp` | `SearchBridgeStateInternal` |
| DDS leaf | `alpha_mu_core.cpp` | `MakeBridgeDDSLeafFront` |
| TT | `alpha_mu_core.cpp` | `TranspositionTable` |
| Front operations | `alpha_mu_front.cpp` | `ParetoFront::Insert`, `Merge`, `ProductMin` |
| Decision solve | `alpha_mu_decision.cpp` | `SolveDecisionPoint` |
| Reporting | `alpha_mu_reporting.cpp` | `PrintDecisionReport` |
| Regressions | `alpha_mu_tests.cpp` | `RunDefaultTestSuite` |

## References

- [Invariants reference](alpha-mu-invariants.md) — correctness properties
- [Bridge player guide](alpha-mu-guide.md) — non-technical explanation
- [Information-state contract](alpha-mu-information-state.md) — world inputs
- [Benchmark baseline](alpha-mu-benchmark-baseline.md) — M1 Max performance

