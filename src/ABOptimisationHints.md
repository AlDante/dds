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

## Expected Impact Summary

| Optimisation | Estimated Speedup |
|---|---|
| NEON for 4-suit loops | 5–10% |
| PGO + LTO | 10–20% |
| P-core pinning (8 threads) | 15–25% vs mixed scheduling |
| `__builtin_prefetch` | 2–5% |
| Cache-line alignment (128B) | 2–5% |
| Branch hints | 1–3% |

The **biggest wins** are PGO + LTO and ensuring all threads stay on P-cores. The NEON vectorisation is a clean, mechanical transformation that eliminates the most-executed loops in the function.
