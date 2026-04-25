# Alpha-Mu Implementation Plan v2

> **Status note (2026-04-24):** This document is still useful as an engineering
> direction document, but parts of its starting-state description are now
> historical rather than current. Since it was written, the repository has
> gained a working `solve` / `decision` path, bridge-state TT support, richer
> root reporting, practical real-board depth-2 continuation regressions, and an
> initial split of the solver into
> `alpha_mu.cpp`, `alpha_mu_core.*`, and
> `alpha_mu_tests.*`. The immediate next work is now continued
> `Workstream 5` extraction while keeping those practical continuation and
> reporting regressions green. For the authoritative current status and immediate
> next work, use `docs/action-plan.md` and `docs/alpha-mu-roadmap.md`.

## Context

This plan replaces the incremental stage-based roadmap in `alpha-mu-roadmap.md` and the cycle-based plan in `action-plan.md` with a single, concrete engineering plan that works directly from the current codebase state toward a complete, testable, performant alpha-mu engine.

### What exists today

The current solver lives in four files under `test/` totalling ~9,500 lines:

| File | Lines | Role |
|------|------:|------|
| `alpha_mu_core.h` | 1,892 | All data structures and function declarations |
| `alpha_mu_core.cpp` | 4,046 | All implementation logic in one file |
| `alpha_mu_tests.cpp` | 3,358 | 48 regression tests |
| `alpha_mu.cpp` | 189 | CLI runner (test/benchmark/compare modes) |

The current solver already implements:

- **Core semantics**: WorldMask, OutcomeVector, ParetoFront, dominance, Max-union, Min-product/min
- **Paper cuts**: early cut, root cut, deep alpha cut, cut-on-win, world cuts, useful-world maintenance
- **Optimistic completion** of impossible/useless worlds
- **Pareto-front transposition table** for exact reuse
- **Bridge state**: legal-move generation, trick progression, winner advancement
- **DDS leaf evaluation**: serial and leaf-parallelized
- **World construction**: seed-based hidden-seat enumeration with constructor-local pruning (card-location, length, HCP, balanced shape, hand-type, partnership ranges)
- **Staged world filtering**: known-card, bidding, follow-suit, play-history replay, current-trick, deduplication, deterministic sampling
- **Partial-information entry point**: `MakeBridgeStateFromPartialInformation` with PBN play-history parsing, follow-suit void filtering
- **Benchmarking**: board-parallel, DDS-vs-alpha-mu comparison, PMU counters
- **48 passing regression tests**

### What does not yet exist

1. **No realistic end-to-end run**: nobody can point the engine at a deal and get back "play the jack of spades because it wins in 17 of 20 worlds"
2. **No search deeper than ~2 tricks** on real deals (combinatorial explosion)
3. **No bridge-state transposition table** (the existing TT is toy-tree only)
4. **No measured time budget breakdown** (world generation vs search vs DDS leaf)
5. **Monolithic code**: 4,000 lines of implementation in one file
6. **No public API**: alpha-mu is only callable from the test runner
7. **No paper-motif coverage**: strategy fusion, non-locality, discovery play, and rare-bad-event cases are not tested on bridge deals

### Design principles for the new plan

1. **Vertical slices over horizontal layers**. Each milestone delivers a working, testable capability end-to-end, not a horizontal layer that requires everything else to be useful.
2. **Performance is a first-class constraint**, not a later bolt-on. Every milestone includes a timing checkpoint. Architecture decisions are PMU-informed.
3. **Refactoring is interleaved with features**, not deferred to a late "cleanup stage". Each milestone leaves the code in better shape than it found it.
4. **DDS is a leaf oracle, not a host**. Alpha-mu logic lives in its own modules, calling DDS via a narrow interface. DDS internals are not modified.
5. **Measurability precedes optimisation**. No performance work without instrumentation showing where time goes.

---

## Milestone 1: End-to-end single-board alpha-mu run

**Goal**: A user can run `./build/alpha_mu solve <deal.txt> <board> <depth>` and get back a chosen move with a front summary, using real partial-information world generation and DDS leaf evaluation.

### Work

1. **Wire `MakeBridgeStateFromPartialInformation` into `SearchBridgeStateInternal`**. Currently these two pieces exist but are not connected. Add a `SolveAlphaMu(deal, declarer, playHistory, depth, maxWorlds)` function that:
   - constructs a partial-information state from the deal and play prefix,
   - calls `SearchBridgeState` with the requested depth,
   - returns the root front and chosen move.

2. **Add a `solve` CLI mode** that parses a hand file, selects a board, and calls `SolveAlphaMu`.

3. **Add a root-report formatter** that prints: chosen move, front vectors, surviving world count, depth searched.

4. **Add instrumentation counters**: DDS leaf calls, recursive search nodes, elapsed time split into world-generation and search phases.

5. **Regression test**: call `SolveAlphaMu` on the `alpha_mu_play.txt` board 1 at depth 1 with 10-trick play prefix. Assert: returns a valid move, front has >0 vectors, DDS leaf count >0, elapsed <10s.

### Refactoring in this milestone

- Extract `SolveAlphaMu` into a new file `alpha_mu_solve.cpp` / `.h`. This is the first step toward splitting the monolith.

### Performance checkpoint

- Record wall-clock time and DDS leaf count for the smoke-test board at depth 1.

### Completion signal

`./build/alpha_mu solve ../hands/alpha_mu_play.txt 1 1` prints a move recommendation with timing.

---

## Milestone 2: Bridge-state transposition table

**Goal**: Avoid re-searching identical positions reached by different move orders.

### Work

1. **Define a bridge-state hash** (Zobrist-style). Key components: remaining cards per seat per suit (as bitmask), player to move, tricks won, current trick cards. Hash must be cheap to compute incrementally via XOR as cards are played.

2. **Implement `BridgeTranspositionTable`** with exact-front storage, keyed by hash + useful-world mask. Store and probe before DDS leaf calls and before recursive expansion.

3. **Add TT hit/miss/store counters** to `SearchStats`.

4. **Regression**: verify that TT-enabled search produces identical fronts to TT-disabled search on existing test boards. Verify hit count > 0 on boards where move transpositions exist.

### Refactoring in this milestone

- Extract Pareto-front operations (`ParetoFront`, `OutcomeVector`, dominance, merge, product) into `alpha_mu_front.h` / `.cpp`.

### Performance checkpoint

- Compare depth-1 and depth-2 solve times with and without TT. Record hit rates.

### Completion signal

TT-backed search matches TT-free results exactly, with measured hit rate > 0 on at least one test board.

---

## Milestone 3: Deeper search with time budget

**Goal**: Search 3+ tricks deep on real boards within a configurable time budget.

### Work

1. **Iterative deepening by trick depth** with wall-clock cutoff. Start at depth 1, increment, stop when time budget is exhausted. Return the deepest completed front.

2. **Move ordering at Max nodes**: try moves that won in more worlds first (from the previous depth's front). This is the single most impactful search-speed improvement.

3. **Leaf-call batching**: when a Min node has expanded all children across all worlds, batch the DDS leaf calls and dispatch them in parallel using `SolveAllBoards` where possible.

4. **Add a `--time` flag** to the `solve` CLI mode.

5. **Regression**: depth-3 solve on `alpha_mu_play.txt` board 1 (after 6-trick prefix, 7 remaining cards per hand, manageable). Assert correctness and record time.

### Refactoring in this milestone

- Extract bridge-state logic (move generation, trick progression, state transitions) into `alpha_mu_bridge.h` / `.cpp`.
- Extract DDS leaf adapter into `alpha_mu_dds_leaf.h` / `.cpp`.

### Performance checkpoint

- Depth-1/2/3 timings on 3 boards. DDS leaf call count per depth. TT hit rate per depth.

### Completion signal

At least one real board searched to depth 3 with correct results in < 60 seconds.

---

## Milestone 4: Paper-motif validation

**Goal**: Demonstrate that alpha-mu makes better decisions than single-world DDS on imperfect-information hands.

### Work

1. **Strategy fusion fixture**: a deal where the best play depends on combining different strategies across worlds. DDS on any single world suggests the wrong move; alpha-mu suggests the right one.

2. **Non-locality fixture**: a deal where information gained from Min's response in one world informs the right play in another. Already tested in the toy search; now demonstrate on a real bridge deal.

3. **Discovery play fixture**: a deal where playing a non-obvious card reveals information (e.g., a defender's void) that improves subsequent play.

4. **Rare-bad-event avoidance**: a deal where a greedy play wins in most worlds but loses catastrophically in one; alpha-mu prefers the safer line.

5. Each fixture:
   - hand file in `hands/alpha_mu_motif_*.txt`,
   - regression test asserting the chosen move,
   - recorded search depth and world count,
   - documented explanation of why the move is correct.

### Refactoring in this milestone

- Extract world construction and filtering into `alpha_mu_worlds.h` / `.cpp`.

### Performance checkpoint

- Record solve times for each motif fixture.

### Completion signal

4 motif regressions passing, each with documented explanations.

---

## Milestone 5: Instrumentation and profiling

**Goal**: Know exactly where time goes and what to optimise.

### Work

1. **Per-phase timing**: world generation, constructor pruning, search, DDS leaf calls. Reported in the solve output.

2. **Per-depth counters**: nodes expanded, fronts merged, dominance reductions, TT probes/hits/stores, cuts by type (early, root, deep-alpha, cut-on-win, world-zero, world-single).

3. **PMU integration**: option to run the solve path under the existing `pmu_counters.h` infrastructure. Record IPC, branch mispredictions, L1D misses for the search phase.

4. **Benchmark suite**: `./build/alpha_mu benchmark_alpha ../hands/alpha_mu_showcase.txt 3 --parallel board --board-workers 8` with structured output parseable by `test/run_alpha_mu_benchmark.py`.

5. **Performance log entry** with baseline numbers for all showcase boards at depths 1-3.

### Refactoring in this milestone

- Extract search control (iterative deepening, TT management, cut logic) into `alpha_mu_search.h` / `.cpp`.
- The monolithic `alpha_mu_core.cpp` should now be split into ~5 focused files.

### Performance checkpoint

- Full instrumented profile of showcase boards at depth 3.

### Completion signal

`solve` output includes per-phase timing. Benchmark suite runs and produces parseable output.

---

## Milestone 6: Optimisation pass

**Goal**: Make depth-3 search practical on most boards and depth-4 reachable on some.

### Work (guided by Milestone 5 instrumentation)

Likely candidates (to be confirmed by measurement):

1. **DDS leaf batching with `SolveAllChunksPBN`** for worlds solved at the same position.
2. **Front compaction**: limit front size to K vectors (e.g., K=32) via aggressive dominance reduction.
3. **World sampling**: for boards with >20 candidate worlds, sample down to 20 deterministically before searching. Measure effect on move stability.
4. **Bridge-state TT with incremental Zobrist hashing**: avoid re-hashing the full state at every node.
5. **Outcome-vector representation**: switch from `vector<int>` to fixed-size `int[64]` or bitpacked representation when world count is small.
6. **Move-generation cache**: if the same set of legal moves appears at many nodes (common in partial-trick states), cache it.

### Performance checkpoint

- Before/after comparison on showcase boards at depth 3 and 4.
- PMU counters for the search phase before and after.

### Completion signal

Depth 3 in < 30s on most showcase boards. At least one board searchable to depth 4.

---

## Milestone 7: Public API and integration

**Goal**: Alpha-mu is a first-class repository feature with stable entry points.

### Work

1. **Public header** `include/alpha_mu.h` with:
   ```c
   int AlphaMuSolve(dealPBN *deal, playTracePBN *play,
                    int declarer, int maxDepth, int maxWorlds,
                    alphaMuResult *result);
   ```

2. **Result structure** containing: chosen move, front summary, world count, depth searched, elapsed time, DDS leaf count.

3. **Thread safety**: alpha-mu solve must be safe to call from multiple threads (each with its own state, using separate DDS thread slots).

4. **Documentation**: `docs/alpha-mu-user-guide.md` covering build, invocation, output interpretation, and limits.

5. **Example program**: `examples/AlphaMuSolve.cpp` showing a simple alpha-mu call.

6. **Regression**: the existing 48+ tests continue to pass. The public API returns correct results on the showcase boards.

### Performance checkpoint

- Public API overhead vs direct solver call (should be negligible).

### Completion signal

A new user can build DDS, include `alpha_mu.h`, and call `AlphaMuSolve` to get an imperfect-information move recommendation.

---

## Summary timeline

| Milestone | What | Key deliverable | Test count |
|-----------|------|-----------------|-----------|
| 1 | End-to-end solve | `solve` CLI mode, root report | ~50 |
| 2 | Bridge TT | Zobrist hash, exact-front TT | ~53 |
| 3 | Deeper search | Depth 3+, move ordering, time budget | ~57 |
| 4 | Paper motifs | 4 motif fixtures | ~61 |
| 5 | Instrumentation | Per-phase timing, PMU, benchmark suite | ~63 |
| 6 | Optimisation | Depth 3 in <30s, depth 4 reachable | ~63 |
| 7 | Public API | `alpha_mu.h`, docs, example | ~65 |

## Definition of done

Alpha-mu is complete when all of the following are true:

1. A user can call `AlphaMuSolve` and get back a move recommendation with timing and confidence information.
2. The engine searches at least 3 tricks deep on real boards within a practical time budget.
3. Paper-motif regressions (strategy fusion, non-locality, discovery play, rare-bad-event) pass.
4. Instrumentation shows where time goes (world generation, search, DDS leaf, TT).
5. Performance is benchmark-tracked and PMU-validated on Apple Silicon.
6. Code is split into focused modules (~5-6 files) with clear interfaces.
7. DDS baseline behavior is unchanged and benchmark-backed.
8. Documentation covers build, invocation, output, and limits.

## Highest-risk items

| Risk | Mitigation |
|------|-----------|
| Combinatorial explosion at depth 3+ | Move ordering + TT + world sampling + time budget |
| Pareto front size blowup | Front compaction with size cap |
| DDS leaf call cost dominates | Batching via `SolveAllChunksPBN` + TT to avoid repeated calls |
| World generation for early-play positions | Capped enumeration + deterministic sampling (already implemented) |
| Monolithic code hinders progress | Refactoring interleaved with each milestone |

