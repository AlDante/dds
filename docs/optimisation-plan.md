# DDS Performance Optimisation Plan

_Generated 2026-04-19 from codebase review against `ABOptimisationHints.md` and `QuickTricksOptimisation.md`._

---

## Current State Summary

The profiling ladder (see `docs/performance-log.md`) established that:

1. The **`8.1` hot/cold `ThreadData` split** delivered a real `-14%` improvement.
2. The **`8.3` `DepthLocal` scratch-pad** experiment caused a `+35–57%` regression that has since been reverted; `posPoint->winRanks[depth]` is again the source of truth.
3. The **M1 Max `ABsearch` path** (branch hints, prefetch, inlined helpers) delivered `-20.7%` wall time on `list9` depth 2.
4. The **`8.4` packed `moveType`** and **`8.5` `pos` hot-field reorder** showed mixed/neutral results in isolation but were not harmful under the profiling build.
5. The dominant hot path remains `ABsearch* → MakeNext → Make/Undo → QuickTricks → SolveBoardInternal`.

---

## Prioritised Action Items

### Tier 1 — High confidence, moderate complexity, no thread-safety risk

| # | Optimisation | Est. gain | Complexity | Safety |
|---|---|---|---|---|
| 1 | **QuickTricks context struct (§9.1)** | 10–20% | Medium | Safe (stack-local) |
| 2 | **QuickTricks suit-advance extraction (§9.2)** | 5–10% | Low | Safe |
| 3 | **NEON 4-suit operations in `ABsearch_m1max.cpp` (§2)** | 5–10% | Low | Safe |
| 4 | **LTO (`-flto`) in release build** | 5–15% | Trivial (build flag) | Safe |

#### 1. QuickTricks context struct

**What:** The four sub-functions (`QtricksLeadHandTrump`, `QtricksLeadHandNT`, `QuickTricksPartnerHandTrump`, `QuickTricksPartnerHandNT`) each take 12–16 scalar parameters. On ARM64, arguments beyond x0–x7 spill to the stack. Collecting them into a single `QtricksContext` struct passed by pointer eliminates ~60 parameter copies per `QuickTricks` call and keeps all shared state in one cache line (~80 bytes).

**Implementation sketch:**

```cpp
struct QtricksContext {
  pos& tpos;
  const int hand;
  const int depth;
  const int cutoff;
  const int trump;
  int suit;
  int qtricks;
  int countOwn, countLho, countRho, countPart;
  int lhoTrumpRanks, rhoTrumpRanks;
  int commSuit, commRank;
  bool commPartner;
};
```

Each sub-function becomes `int QtricksLeadHandTrump(QtricksContext& ctx, int& res)`.

**Risk:** Purely mechanical refactor. All state remains stack-local within `QuickTricks`. Thread safety unchanged — each thread has its own stack frame. Only risk is introducing a typo during the large but repetitive rewrite across ~1200 lines.

**Validation:** The existing `dtest -f ../hands/list10.txt -s solve`, `regression_api`, and `play_analysis_benchmark` suites cover QuickTricks exhaustively.

#### 2. Suit-advance extraction

**What:** The identical "advance suit, skip trump" pattern appears ~15 times in `QuickTricks.cpp`. Replace with:

```cpp
inline int nextNonTrumpSuit(int suit, int trump) {
  suit++;
  suit += (suit == trump);  // branchless
  return suit;
}
```

And convert the `do { ... } while (suit < DDS_SUITS)` into a precomputed `suitOrder[4]` iteration.

**Risk:** None — pure code deduplication producing identical control flow. Validates with the same test suites.

#### 3. NEON for `ABsearch_m1max.cpp` helper functions

**What:** The current `DDSM1ZeroWinRanks`, `DDSM1CopyChildWinRanks`, `DDSM1OrChildWinRanks` use four scalar assignments. Replace with single NEON intrinsics:

```cpp
#include <arm_neon.h>

inline void DDSM1ZeroWinRanks(pos* p, int depth) {
  vst1_u32(reinterpret_cast<uint32_t*>(&p->winRanks[depth][0]),
           vdup_n_u32(0));  // 64-bit store covers 4 × uint16
}
```

Actually, `winRanks` is `unsigned short[50][4]` = 8 bytes per depth. A single `vst1_u16(vdup_n_u16(0))` zeroes all four in one instruction. For the OR-accumulate:

```cpp
uint16x4_t cur  = vld1_u16(&p->winRanks[depth][0]);
uint16x4_t prev = vld1_u16(&p->winRanks[depth - 1][0]);
vst1_u16(&p->winRanks[depth][0], vorr_u16(cur, prev));
```

**Risk:** None — same semantic, different encoding. Guarded by `#ifdef DDS_TARGET_APPLE_M1_MAX`.

#### 4. LTO in release builds

**What:** Add `-flto` to the release CMake configuration. This allows the compiler to inline `ABsearch*`, `Make*`, `Undo*`, and `QuickTricks*` across translation units.

**Risk:** Increases link time. No semantic change. Already standard practice for performance-critical C++ on Apple clang.

---

### Tier 2 — Moderate confidence, higher complexity, requires careful validation

| # | Optimisation | Est. gain | Complexity | Safety |
|---|---|---|---|---|
| 5 | **P-core pinning via QoS (§5)** | 10–25% multi-threaded | Trivial | Safe |
| 6 | **`highestRank[]` → CLZ intrinsic (§9.4)** | 2–5% | Low | Safe |
| 7 | **Precomputed booleans in QuickTricks (§9.3)** | 3–8% | Low | Safe |
| 8 | **Return struct for QuickTricks sub-functions (§9.6)** | 1–3% | Low | Safe |

#### 5. P-core pinning

**What:** At board-worker thread creation, call:

```cpp
pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
```

This biases macOS towards scheduling DDS search threads on the 8 performance cores rather than the 2 efficiency cores.

**Risk:** Purely advisory. If the system is under load, the scheduler may still migrate. No correctness impact.

**Implementation:** One-liner in the board-parallel worker thread entry point (alpha-mu prototype's `std::thread` lambda or GCD block).

#### 6. CLZ intrinsic

**What:** Replace `highestRank[ranks]` (32 KB lookup table) with:

```cpp
inline int highestRankFast(unsigned short ranks) {
  return ranks ? (31 - __builtin_clz(static_cast<unsigned>(ranks))) : 0;
}
```

Single cycle, no memory dependency. The table itself likely stays in L1, so the gain is modest but frees load-port bandwidth.

**Risk:** Must confirm the current `highestRank` table semantics match `31 - clz`. A quick audit shows `highestRank[bitMapRank[r]] == r` for all valid ranks, which is consistent with a MSB-extraction. Add a `static_assert` or startup check.

#### 7–8. Precomputed booleans and return struct

Low-risk mechanical changes. Implement as part of the QuickTricks context-struct refactor (item 1) since they touch the same code.

---

### Tier 3 — Speculative / deferred

| # | Optimisation | Est. gain | Complexity | Safety | Status |
|---|---|---|---|---|---|
| 9 | SoA transposition table (§8.2) | 5–15% | High | **High risk** (concurrency) | Deferred |
| 10 | `DepthLocal` scratch pad (§8.3) | 5–10% theoretical | High | Medium | **Failed** — reverted |
| 11 | Packed `moveType` to 4 bytes (§8.4) | 3–8% | Medium | Low | Mixed results |
| 12 | `pos` hot-field reorder (§8.5) | 2–5% | Low | Safe | Mixed results |

**Rationale for deferral:**

- **§8.2 (SoA TT):** The transposition table is accessed concurrently. Splitting keys and payloads introduces torn-read risk. DDS may already use Zobrist-based lockless hashing, which would need substantial rework. The profiling data shows the TT is not currently the primary bottleneck (ABsearch inner loop dominates), so the risk-to-reward ratio is unfavourable now.

- **§8.3 (`DepthLocal`):** Empirically caused a +35–57% regression. The hypothesis that stack-local state would stay in L1 was correct in theory, but the interaction with `QuickTricks` (which writes directly to `posPoint->winRanks[depth]`) and `MakeNext` (which reads `winRanks[depth]`) made the indirection through a shadow copy a net negative. The `pos::winRanks[depth]` array is already compact (8 bytes per depth level) and sequential depth access has good spatial locality.

- **§8.4 (packed `moveType`):** The current `moveType` is 8 bytes (`4 × short`), confirmed by `static_assert(sizeof(moveType) == 8)`. Packing to 4 bytes would require changing `short` fields to `uint8_t` and updating all ~30 call sites that read `.suit`, `.rank`, `.sequence`, `.weight`. The profiling ladder showed mixed results — likely because 8 bytes already gives 16 moves per 128-byte cache line, which is adequate for the typical 4–7 legal moves per position. Revisit only if move-generation becomes a clear bottleneck.

- **§8.5 (`pos` reorder):** Mixed/neutral in benchmarks. The current layout places `rankInSuit`, `aggr`, `length`, `handDist`, `winner`, `secondBest`, `first`, `move`, `winRanks` in order — all accessed per-node — so there is no obvious cold data interleaved. Leave as-is.

---

## Recommended Execution Order

```
Phase 1 (immediate, low risk):
  1. Add -flto to release build flags
  2. NEON intrinsics in ABsearch_m1max.cpp helpers
  3. P-core pinning (QoS) in worker threads

Phase 2 (QuickTricks refactor — one atomic commit):
  4. Introduce QtricksContext struct
  5. Convert sub-function signatures
  6. Extract suit-advance into inline helper
  7. Hoist precomputed booleans
  8. Replace highestRank[] with CLZ intrinsic
  9. Replace res output parameter with return struct

Phase 3 (validation):
  - Full regression suite after each phase
  - Repeat list9 depth-2 board-parallel benchmark
  - Compare against stage-1 baseline (50.107 s)
```

---

## Estimated Combined Impact

Conservative compound estimate (not simply additive due to overlapping hot paths):

| Phase | Estimated wall-time reduction |
|---|---|
| Phase 1 (LTO + NEON + QoS) | 10–20% |
| Phase 2 (QuickTricks refactor) | 10–20% additional |
| **Combined** | **~20–35%** vs current tree |

This would bring the `list9` depth-2 board-parallel benchmark from the current ~50 s (stage-1 baseline, profile build) to an estimated ~33–40 s, approaching the earlier M1 Max `ABsearch` result of 53.891 s (release build, which was already 20.7% faster than the portable baseline).

---

## Critical Review and Caveats

1. **The `DepthLocal` lesson:** Theoretical cache-locality arguments do not always translate to real speedups. Every change must be benchmarked on the actual workload before being kept. The profiling ladder methodology (serial single-board → serial all-board → board-parallel) has proven effective for isolating regressions.

2. **`ThreadDataHot` size concern:** The current `ThreadDataHot` contains `rel[8192]` which is **960 KB** — far too large for L1 (128 KB). This dwarfs all other hot fields. The `rel` table is read-only after setup and is accessed via `thrp->rel[ranks]` in move generation. Since it is constant per deal, it could potentially be shared across threads (read-only pointer) rather than duplicated per-thread. However, this is a deeper architectural change and should be investigated separately with profiling data showing `rel` access frequency.

3. **`moveType` is already 8 bytes:** The `static_assert` confirms the packing is already halfway to the 4-byte target. Given mixed benchmark results, this should not be pursued further without clear profiling evidence that `MakeNext` scanning is a bottleneck.

4. **QuickTricks is 1212 lines:** The refactor (items 1–2, 6–8) touches the majority of this file. It should be done as a single atomic commit with the full regression suite run immediately after. Incremental sub-commits risk introducing subtle bugs in the tightly coupled suit-iteration logic.

5. **PGO showed no benefit over M1 Max ABsearch path:** The earlier benchmark showed PGO trailed the non-PGO M1 Max build by 2.9%. This suggests the manual branch hints and prefetch are already achieving what PGO would provide. PGO should be revisited only after the QuickTricks refactor changes branch patterns.

---

## Summary Decision Matrix

| Optimisation | Do now? | Rationale |
|---|---|---|
| `-flto` | ✅ | Trivial, safe, enables cross-TU inlining |
| NEON in `ABsearch_m1max.cpp` | ✅ | Mechanical, safe, small but clean |
| P-core QoS pinning | ✅ | One-liner, safe, large multi-threaded win |
| QuickTricks context struct | ✅ | Highest estimated single-function gain |
| QuickTricks suit-advance extraction | ✅ | Deduplicates ~200 lines, eliminates branches |
| CLZ intrinsic | ✅ | Trivial, safe |
| Precomputed booleans | ✅ | Trivial, part of QT refactor |
| Return struct | ✅ | Trivial, part of QT refactor |
| SoA transposition table | ❌ | High concurrency risk, not the bottleneck |
| `DepthLocal` scratch pad | ❌ | Empirically harmful |
| Packed `moveType` to 4 bytes | ❌ | Mixed results, already 8 bytes |
| `pos` hot-field reorder | ❌ | Mixed results, layout already reasonable |

