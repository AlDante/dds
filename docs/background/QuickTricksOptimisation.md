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

## 9. QuickTricks-Specific Optimisations

`QuickTricks()` is called at every node in the search tree to detect early cutoffs. It is one of the hottest functions in DDS. The current implementation has significant performance issues related to code structure, parameter passing, branch density, and repeated patterns. The subsections below are ranked by estimated impact.

### 9.1 Collapse the Parameter Explosion with a Context Struct (10–20%)

The sub-functions (`QtricksLeadHandTrump`, `QtricksLeadHandNT`, `QuickTricksPartnerHandTrump`, `QuickTricksPartnerHandNT`) each take **13–16 parameters**. On ARM64, only 8 integer arguments go in registers — the rest spill to the stack. Every call to these functions generates cache-polluting stack writes and reads.

Collect the shared state into a single struct passed by pointer:

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

Each sub-function then takes only `QtricksContext& ctx` and `int& res`:

```cpp
int QtricksLeadHandTrump(QtricksContext& ctx, int& res);
```

**Benefits:**
- All parameters stay in one cache line (~80 bytes) instead of scattered across the stack
- The pointer to the context struct occupies a single register (x0)
- With LTO/inlining, the compiler can keep the struct in registers entirely
- Eliminates ~60 redundant parameter copies per `QuickTricks` call

### 9.2 Extract the Suit-Advancement Logic (5–10%)

The same "advance to next suit, skipping trump" pattern is copy-pasted **~15 times** throughout `QuickTricks`. Each instance is 5–10 lines of identical branching:

```cpp
suit++;
if ((trump != DDS_NOTRUMP) && (suit == trump))
    suit++;
```

And the trump-is-current-suit variant:

```cpp
if ((trump != DDS_NOTRUMP) && (trump == suit)) {
    if (trump == 0) suit = 1;
    else suit = 0;
} else {
    suit++;
    if ((trump != DDS_NOTRUMP) && (suit == trump))
        suit++;
}
```

Replace all instances with a single branchless inline function:

```cpp
inline int nextSuit(int suit, int trump) {
    // For non-trump iteration: skip past trump suit
    suit++;
    suit += (suit == trump);  // branchless skip
    return suit;
}

inline int nextSuitFromTrump(int suit, int trump) {
    // When current suit IS trump, start at 0 (or 1 if trump==0)
    return (trump == 0) ? 1 : 0;
}
```

Or better yet, precompute the suit iteration order once at the top of `QuickTricks`:

```cpp
int suitOrder[DDS_SUITS];
int numSuits = 0;
if (trump != DDS_NOTRUMP) {
    suitOrder[numSuits++] = trump;  // Trump first
    for (int s = 0; s < DDS_SUITS; s++)
        if (s != trump) suitOrder[numSuits++] = s;
} else {
    for (int s = 0; s < DDS_SUITS; s++)
        suitOrder[numSuits++] = s;
}

// Then iterate:
for (int i = 0; i < numSuits; i++) {
    int suit = suitOrder[i];
    // ...
}
```

This eliminates ~60 branches per call and converts the complex `do-while` with manual `suit` advancement into a clean `for` loop that the compiler can reason about.

### 9.3 Precompute Repeated Boolean Expressions (3–8%)

Several compound boolean expressions are evaluated repeatedly across the main loop and sub-functions:

```cpp
// Evaluated 10+ times per QuickTricks call:
(lhoTrumpRanks == 0) && (rhoTrumpRanks == 0)
(trump != DDS_NOTRUMP) && (trump != suit)
(trump != DDS_NOTRUMP) && (trump == suit)
```

Hoist these:

```cpp
const bool isTrumpGame = (trump != DDS_NOTRUMP);
const bool oppsHaveNoTrump = (lhoTrumpRanks == 0) && (rhoTrumpRanks == 0);

// In the loop:
const bool suitIsTrump = isTrumpGame && (suit == trump);
const bool suitIsNonTrump = isTrumpGame && (suit != trump);
```

The compiler *may* do this with LTO, but the deeply nested control flow and pointer aliasing in the current code often prevents it. Explicit hoisting guarantees it.

### 9.4 Replace `highestRank[]` Table Lookup with CLZ Intrinsic (2–5%)

The `highestRank[]` lookup table converts a bitmask of ranks to the highest set bit. On ARM64, this is a single instruction:

```cpp
inline int highestRankFast(unsigned short ranks) {
    // ranks is a bitmask where bit N = rank N
    // __builtin_clz operates on unsigned int (32-bit)
    return ranks ? (31 - __builtin_clz(ranks)) : 0;
}
```

This replaces a memory-dependent table lookup (potential L1 miss) with a single `CLZ` instruction (1 cycle, no memory access). The `bitMapRank[]` reverse lookup can similarly be replaced:

```cpp
inline unsigned short bitMapRankFast(int rank) {
    return static_cast<unsigned short>(1u << rank);
}
```

Both `highestRank[]` and `bitMapRank[]` are small tables that likely stay in L1, but eliminating the dependency on memory frees load-store bandwidth for the actual position data.

### 9.5 NEON for `ranks` Accumulation in Sub-Functions (2–5%)

The pattern in `QuickTricksPartnerHandTrump` and `QuickTricksPartnerHandNT`:

```cpp
unsigned short ranks = 0;
for (int h = 0; h < DDS_HANDS; h++)
    ranks |= tpos.rankInSuit[h][suit];
```

If `rankInSuit` is laid out as `[DDS_HANDS][DDS_SUITS]` (hand-major), this accesses 4 different rows at the same suit column — 4 scattered loads. Two options:

**Option A:** If the layout can be transposed to `[DDS_SUITS][DDS_HANDS]` (suit-major), all 4 hands are contiguous and can be OR-reduced in one NEON operation:

```cpp
// Assuming suit-major: rankInSuit[suit][0..3] are contiguous
uint16x4_t hands = vld1_u16(&tpos.rankInSuit[suit][0]);
unsigned short ranks = vget_lane_u16(
    vpmax_u16(vpmax_u16(hands, hands), vpmax_u16(hands, hands)), 0);
// Actually, horizontal OR via shifts:
uint16x4_t r = vorr_u16(hands, vext_u16(hands, hands, 2));
ranks = vget_lane_u16(vorr_u16(r, vext_u16(r, r, 1)), 0);
```

**Option B:** Keep the current layout but use scalar OR with explicit loads (no loop overhead):

```cpp
unsigned short ranks = tpos.rankInSuit[0][suit]
                     | tpos.rankInSuit[1][suit]
                     | tpos.rankInSuit[2][suit]
                     | tpos.rankInSuit[3][suit];
```

Option B is simpler and the compiler can schedule the 4 independent loads in parallel on M1's 4 load/store units.

### 9.6 Replace `res` Output Parameter with Return Enum (1–3%)

Every sub-function uses `int& res` as an output parameter with values 0, 1, 2. The caller then branches on it:

```cpp
qtricks = QtricksLeadHandTrump(..., res);
if (res == 1) return qtricks;
else if (res == 2) { suit++; ... continue; }
```

This creates a load-after-store dependency: the sub-function writes `res` to memory, the caller reads it back. On M1 this incurs a store-to-load forwarding penalty (~4 cycles).

Return a packed result instead:

```cpp
struct QtricksResult {
    int qtricks;
    int action;  // 0=continue suit, 1=cutoff, 2=next suit
};

inline QtricksResult QtricksLeadHandTrump(QtricksContext& ctx) {
    // ...
    return {qt, 1};  // returned in x0, x1 — no memory
}
```

Both fields are returned in registers (ARM64 returns small structs in x0/x1), eliminating the store-to-load forwarding penalty entirely.

### 9.7 Flatten `QuickTricksSecondHand` Early-Exit (1–2%)

`QuickTricksSecondHand` is called from `ABsearch` before move generation — it's on the absolute hottest path. The `for (int s = 0; s < DDS_SUITS; s++) tpos.winRanks[depth][s] = 0` at the top can be replaced with a NEON zero-store (see Section 2), and the early `return false` at line 1 (`depth == thrd.iniDepth`) should use `__builtin_expect`:

```cpp
if (__builtin_expect(depth == thrd.iniDepth, 0))
    return false;
```

---

## Expected Impact Summary

| Optimisation | Estimated Speedup |
|---|---|
| PGO + LTO | 10–20% |
| P-core pinning (8 threads) | 15–25% vs mixed scheduling |
| **QuickTricks** context struct (9.1) | 10–20% |
| **QuickTricks** suit-advance extraction (9.2) | 5–10% |
| Hot/cold split of `ThreadData` | 5–15% |
| SoA transposition table | 5–15% (probe-heavy workloads) |
| Stack-local `DepthLocal` scratch pad | 5–10% |
| NEON for 4-suit loops | 5–10% |
| **QuickTricks** precompute booleans (9.3) | 3–8% |
| `moveType` packing (4 bytes) | 3–8% |
| `__builtin_prefetch` | 2–5% |
| Cache-line alignment (128B) | 2–5% |
| **QuickTricks** CLZ intrinsic (9.4) | 2–5% |
| **QuickTricks** NEON ranks accumulation (9.5) | 2–5% |
| Branch hints | 1–3% |
| **QuickTricks** return struct (9.6) | 1–3% |
| **QuickTricks** SecondHand flatten (9.7) | 1–2% |

The **biggest wins** are PGO + LTO, ensuring all threads stay on P-cores, and collapsing the QuickTricks parameter explosion. The NEON vectorisation and suit-advancement extraction are clean, mechanical transformations with high confidence of payoff.
