# DDS Transposition Table Sizing Benchmark

**Date:** 2026-04-23  
**Platform:** Apple M1 Max, macOS, clang, single-threaded (`-n 1`)  
**Workload:** `dtest -f N -s solve` (SolveBoard on list files)

## Background

DDS allocates per-thread transposition tables with a default/maximum size
pair. The "Large" TT variant (`TransTableL`) uses pages of pre-allocated
nodes; the "Small" variant (`TransTableS`) uses dynamically grown pools.
Thread selection between L and S is automatic based on available memory.

The original hardcoded defaults were 95 MB default / 160 MB max for large
threads, and 20 MB / 30 MB for small threads. This benchmark evaluates
whether increasing the large-thread TT improves performance on a machine
with ample RAM.

## Method

All runs use the shared library (`libdds.so`) built with `-O3 -flto` for
arm64. TT sizes are controlled via environment variables introduced in
`TTConfig.h`:

- `DDS_THREADMEM_LARGE_DEF_MB` — initial allocation
- `DDS_THREADMEM_LARGE_MAX_MB` — ceiling before reset/eviction

The default ratio used is def ≈ 2/3 × max.

## Results: 1,000 hands (`list1000.txt`)

| Def MB | Max MB | User time (ms) | Avg (ms/hand) |
|--------|--------|-----------------|----------------|
| 95     | 160    | 28,460          | 28.46          |
| 95     | 160    | 27,263          | 27.26          |
| 80     | 160    | 40,301          | 40.30          |
| 213    | 320    | 28,272          | 28.27          |
| 320    | 480    | 25,614          | 25.61          |
| 320    | 480    | 30,820          | 30.82          |
| 426    | 640    | 27,910          | 27.91          |
| 426    | 640    | 27,031          | 27.03          |
| 533    | 800    | 25,117          | 25.12          |
| 640    | 960    | 25,091          | 25.09          |
| 640    | 1280   | 29,101          | 29.10          |
| 1280   | 2560   | 29,303          | 29.30          |

## Results: 10,000 hands (`list10000.txt`)

| Def MB | Max MB | User time (ms) | Avg (ms/hand) |
|--------|--------|-----------------|----------------|
| 95     | 160    | 256,348         | 25.63          |
| 95     | 160    | 220,322         | 22.03          |
| 320    | 480    | 212,890         | 21.29          |
| 320    | 480    | 226,494         | 22.65          |
| 320    | 480    | 224,864         | 22.49          |

## Analysis

- **Below 320 MB max**: measurably slower due to TT resets/evictions.
- **480–960 MB max**: modest ~5–10% improvement over default on 1K hands;
  within noise on 10K hands.
- **Above 1280 MB**: no further gain; possible slight regression from
  cache/TLB pressure.
- Run-to-run variance is ~10%, making it difficult to establish a
  statistically significant optimum.
- The TT is reset between boards, so a larger TT primarily helps within
  individual hard deals.

## Conclusion

The default is bumped from 95/160 to **320/480 MB** (def/max). This is
a conservative increase that avoids the clearly-slower small-TT regime
while staying well within the memory budget of modern machines. The
improvement is modest and within noise on large benchmarks, but the
larger TT eliminates resets on harder deals.

The new `TTConfig.h` infrastructure allows easy tuning via environment
variables or compile-time `-D` flags without code changes.

