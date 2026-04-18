# Optimising DDS Alpha-Beta Search for Apple M1 Max

Target: Apple M1 Max — 8 performance cores, 2 efficiency cores, 128-byte cache lines, ARM NEON SIMD.

---

## 1. Branch Prediction — Reorder the Hot Path

The M1's branch predictor is excellent but it cannot predict application-level semantics. Use `__builtin_expect` to hint the compiler on the two critical branches:

```cpp
if (__builtin_expect(res, 0))  // QuickTricks rarely succeeds
    return success;

if (__builtin_expect(value == success, 0))  // cut-offs are rarer than continuations
    goto ABexit;
```

> **Note:** Profile first — if cut-offs are actually *frequent* (good move ordering), flip the hint to `1`.

---

## 2. Eliminate the 4-Suit Loops with NEON/SIMD

Every `for (int ss = 0; ss < DDS_SUITS; ss++)` iterates over exactly **4 suits** — a perfect fit for a single 128-bit NEON operation:

```cpp
#include <arm_neon.h>

// Zero: lowestWin[depth][0..3] = 0
vst1q_s32(&thrp->lowestWin[depth][0], vdupq_n_s32(0));

// Zero: winRanks[depth][0..3] = 0
vst1q_s32(&posPoint->winRanks[depth][0], vdupq_n_s32(0));

// Copy: winRanks[depth] = winRanks[depth-1]  (at cut-off)
vst1q_s32(&posPoint->winRanks[depth][0],
           vld1q_s32(&posPoint->winRanks[depth - 1][0]));

// OR-accumulate: winRanks[depth] |= winRanks[depth-1]
int32x4_t cur  = vld1q_s32(&posPoint->winRanks[depth][0]);
int32x4_t prev = vld1q_s32(&posPoint->winRanks[depth - 1][0]);
vst1q_s32(&posPoint->winRanks[depth][0], vorrq_s32(cur, prev));
```

This replaces 4 scalar loads + stores with a single vector operation each time. Deep in the search tree, these loops execute millions of times.

---

## 3. Cache-Line Alignment for Hot Structs

The M1 Max has **128-byte cache lines** (not 64 like Intel). Align the hot structures:

```cpp
struct alignas(128) pos { ... };
struct alignas(128) ThreadData { ... };
```

Ensure `winRanks[depth]` arrays are contiguous (4 × `int` = 16 bytes fits in one cache line, but the `depth` stride matters — avoid striding across cache-line boundaries between consecutive depths).

---

## 4. Prefetch the Next Depth Level

The M1 has a hardware prefetcher but it cannot predict tree-recursive access patterns. Before calling `Make1`:

```cpp
__builtin_prefetch(&posPoint->winRanks[depth - 1], 1, 3);  // write, high locality
__builtin_prefetch(&thrp->lowestWin[depth - 1], 1, 3);
```

---

## 5. Pin Threads to Performance Cores

DDS supports multi-threading, but the key for M1 Max is keeping all search threads on **P-cores only** (cores 0–7). The E-cores (8–9) have smaller caches and lower throughput — they would hurt search performance.

```cpp
#include <pthread.h>
pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
```

Use `QOS_CLASS_USER_INTERACTIVE` or `QOS_CLASS_USER_INITIATED` for all 8 search threads. This tells the macOS scheduler to prefer performance cores.

---

## 6. Profile-Guided Optimisation (PGO)

The single biggest win for alpha-beta on M1:

```bash
# Pass 1: instrument
clang++ -O3 -mcpu=apple-m1 -fprofile-generate -o dds_profile ...
./dds_profile < benchmark_deals.txt

# Pass 2: optimise with profile data
clang++ -O3 -mcpu=apple-m1 -fprofile-use=default.profdata -o dds_fast ...
```

PGO lets the compiler see actual branch frequencies and lay out the hot path linearly in memory. This is typically worth **10–20%** on tree-search code.

---

## 7. Compiler Flags

```bash
-O3 -mcpu=apple-m1 -flto -fomit-frame-pointer -DNDEBUG
```

`-flto` (link-time optimisation) is critical — it allows the compiler to inline `ABsearch2`, `Make1`, `Undo2`, and `QuickTricksSecondHand` across translation units, eliminating call overhead deep in the tree.

---

## 8. Data Structure Changes for Cache Coherency

The most impactful optimisation on M1 Max is often not algorithmic — it is ensuring that data accessed together lives together in memory. The M1 Max cache hierarchy is:

- **L1 data cache:** 128 KB per P-core, 4-cycle latency
- **L2 cache:** 48 MB shared, ~15-cycle latency
- **Cache line:** 128 bytes (double Intel's 64 bytes — misses are cheaper per byte but waste more on partial use)

The subsections below are ranked by estimated impact, highest first.

### 8.1 Split `ThreadData` into Hot and Cold (5–15%)

`ThreadData` likely contains both per-node hot data (move generator state, `lowestWin`, `bestMove`, `nodeTypeStore`) and cold data (configuration, statistics, forbidden moves). On M1 Max, a single `ThreadData` that spans multiple cache lines means every recursive call may pull in irrelevant data.

```cpp
struct ThreadDataHot {
    int lowestWin[49][DDS_SUITS];    // 784 bytes
    int nodeTypeStore[4];             // 16 bytes
    moveType bestMove[49];            // per-depth best move
    MoveList moves;                   // move generator
    int trump;                        // accessed every node
    int iniDepth;
    // Total: aim for ≤ 1–2 KB to stay in L1
};

struct ThreadDataCold {
    moveType forbiddenMoves[...];
    ABStats ABStats;
    unsigned long long nodes;
    // ... profiling, setup, etc.
};

struct alignas(128) ThreadData {
    ThreadDataHot hot;
    // padding to cache-line boundary if needed
    ThreadDataCold cold;
};
```

This ensures the recursive inner loop almost never touches a cache line belonging to `cold`.

### 8.2 Use Structure-of-Arrays for the Transposition Table (5–15%)

If DDS uses a transposition table (hash table for positions), the typical layout is:

```cpp
struct TTEntry {
    uint64_t key;       // 8 bytes — checked every probe
    int16_t value;      // 2 bytes — read on hit
    int16_t depth;      // 2 bytes — read on hit
    uint32_t winRanks;  // 4 bytes — read on hit
    // ... other fields
};
```

On M1 Max with 128-byte cache lines, a probe loads the entire entry plus its neighbours. If the table is large (millions of entries), most probes are **misses** — the key doesn't match and the rest of the entry is wasted bandwidth. Split into structure-of-arrays:

```cpp
struct TranspositionTable {
    uint64_t* keys;      // Contiguous keys — probe checks only these
    TTPayload* payloads; // Loaded only on a key match
};
```

This doubles the effective cache utilisation on probes because each 128-byte cache line now holds 16 keys instead of ~8 full entries.

### 8.3 Depth-Local Scratch Pad (5–10%)

Rather than indexing into large arrays with `[depth]` subscripts (which may stride across many cache lines as depth changes), consider a per-depth "scratch pad" struct that is passed down the recursion:

```cpp
struct DepthLocal {
    int winRanks[DDS_SUITS];   // 16 bytes
    int lowestWin[DDS_SUITS];  // 16 bytes
    moveType bestMove;          // 4–8 bytes
    int nodeType;               // 4 bytes
};
// ~40–48 bytes — fits in one cache line
```

Allocated on the stack (which is always in L1 on M1), this eliminates indexed array access into `ThreadData` entirely. The recursive call passes a pointer to the parent's `DepthLocal` so `winRanks` can be OR-accumulated upward.

```cpp
bool ABsearch1(pos* posPoint, int target, int depth,
               ThreadDataHot* thrp, DepthLocal* dl) {
    DepthLocal childDL;
    // ...
    value = ABsearch2(posPoint, target, depth - 1, thrp, &childDL);
    // OR-accumulate from child
    vst1q_s32(dl->winRanks,
              vorrq_s32(vld1q_s32(dl->winRanks),
                        vld1q_s32(childDL.winRanks)));
}
```

Stack-allocated `DepthLocal` structs are virtually guaranteed to be in L1 cache since the M1's 128 KB L1 can hold the entire recursion stack (49 depths × 48 bytes = ~2.4 KB).

### 8.4 Pack `moveType` to Minimise Per-Move Cache Footprint (3–8%)

If `moveType` is larger than 8 bytes, consider packing it. Each move in the move list is touched during `MakeNext`, `Make1`, and `Undo2`. A smaller move means more moves fit in a cache line:

```cpp
struct moveType {
    uint8_t suit;       // 0–3
    uint8_t rank;       // 2–14
    uint8_t sequence;   // move ordering score
    uint8_t padding;
};
// 4 bytes → 32 moves per 128-byte cache line
```

Compare with a typical 16-byte `moveType` (only 8 per cache line). This directly affects the inner loop throughput since `MakeNext` scans the move list sequentially.

### 8.5 Co-locate Hot Fields in `pos` (2–5%)

The current layout is typically:

```cpp
int winRanks[49][DDS_SUITS];   // winRanks[depth][suit]
int lowestWin[49][DDS_SUITS];  // lowestWin[depth][suit]
```

This is already depth-major, which is correct — each `winRanks[depth]` is 16 bytes (4 × `int`), fitting within a single cache line. **Do not transpose to suit-major.** However, ensure these arrays are not separated by hundreds of bytes of cold data inside `pos` / `ThreadData`. Move them adjacent to each other:

```cpp
struct pos {
    // HOT — accessed every node
    int winRanks[49][DDS_SUITS];
    int first[49];
    // ... other hot fields ...

    // COLD — accessed rarely
    // ... setup data, deal info, etc. ...
};
```

---

## Expected Impact Summary

| Optimisation | Estimated Speedup |
|---|---|
| NEON for 4-suit loops | 5–10% |
| PGO + LTO | 10–20% |
| P-core pinning (8 threads) | 15–25% vs mixed scheduling |
| `__builtin_prefetch` | 2–5% |
| Cache-line alignment (128B) | 2–5% |
| Branch hints | 1–3% |
| Hot/cold split of `ThreadData` | 5–15% |
| `moveType` packing (4 bytes) | 3–8% |
| SoA transposition table | 5–15% (probe-heavy workloads) |
| Stack-local `DepthLocal` scratch pad | 5–10% |

The **biggest wins** are PGO + LTO and ensuring all threads stay on P-cores. The NEON vectorisation is a clean, mechanical transformation that eliminates the most-executed loops in the function.
