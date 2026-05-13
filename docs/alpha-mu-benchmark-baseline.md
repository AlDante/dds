# Alpha-Mu Benchmark Baseline for Apple M1 Max

## Purpose

This document freezes the canonical alpha-mu benchmark suite used for
performance evaluation on Apple M1 Max. It defines the reference workloads,
measurement methodology, and baseline numbers against which all future
optimization attempts must be compared.

This is the **S7.1** deliverable from the alpha-mu completion plan.

## Canonical benchmark workloads

### Primary iteration workload (depth 2)

Used for rapid A/B comparisons during optimization work.

- Hand file: `hands/list9.txt` (9 boards)
- Depth: 2
- Mode: serial, single-threaded (for CPU-time stability)
- Command:

```
cd test && DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu benchmark_alpha \
    ../hands/list9.txt 2 0 \
    --parallel serial --board-workers 1 --root-workers 1 --dds-thread-id 0
```

- Metric: **process CPU time** via `getrusage(RUSAGE_SELF)`
- Why serial: eliminates the ~20-30% run-to-run variance seen in board-parallel
  wall-clock measurements, as documented in the performance log

### Secondary throughput workload (depth 2, board-parallel)

Used to confirm that serial gains translate to parallel throughput.

- Hand file: `hands/list9.txt` (9 boards)
- Depth: 2
- Mode: board-parallel, 8 workers (saturates M1 Max P-cores)
- Command:

```
cd test && DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu benchmark_alpha \
    ../hands/list9.txt 2 0 \
    --parallel board --board-workers 8
```

- Or use: `make perf-bench`
- Metric: wall-clock time (subject to ~20-30% scheduling noise)

### Deep validation workload (depth 3)

Used for final validation of accepted changes, not for iteration.

- Hand file: `hands/list10.txt` (10 boards, skip board 2)
- Depth: 3
- Mode: serial or board-parallel
- Command:

```
cd test && DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu benchmark_alpha \
    ../hands/list10.txt 3 0 2 \
    --parallel serial --board-workers 1 --root-workers 1 --dds-thread-id 0
```

- Baseline: ~4674 s/board (pre-parallelisation, from 2026-04-18 entry)
- Purpose: too slow for iteration (~42000 s total) but important for
  confirming no deep behavioral regression

### PMU single-board workload

Used for hardware-counter profiling with `pmu_single_run`.

- Hand file: `hands/list9.txt`, board 1 only
- Depth: 2
- Mode: serial
- Provides: cycles, instructions, IPC, branch mispredictions, L1D cache misses

## Frozen baseline numbers

All numbers from the performance log on Apple M1 Max (macOS 26.4.x, arm64).

### Fastest measured code: commit `170e566`

This commit introduced the M1 Max-specific `ABsearch` build path and represents
the fastest measured DDS-side code before any data-structure experiments.

| Metric | Value |
| --- | ---: |
| CPU time per board (serial, list9 depth 2) | **23.39 s** |
| Wall time per board (board-parallel 10 workers) | **28.19 s** |
| Total wall time (board-parallel) | **53.89 s** |

### PMU baseline: commit `170e566`, board 1

| Counter | Value |
| --- | ---: |
| Wall/board | 32.75 s |
| CPU/board | 32.23 s |
| Cycles | 101.94 B |
| Instructions | 350.05 B |
| IPC | 3.43 |
| Branch Mispredictions | 1,235 M |
| L1D Miss Loads | 4.635 B |
| L1D Miss Stores | 2.866 B |

### Current HEAD (after performance recovery and toolchain retuning)

After reverting unsuccessful source-level optimization experiments, the current
code retains the packed `moveType` change (`int` to `short`) on top of
`170e566`. On Apple `arm64`, the standard release build now also retains an
aggressive M1 Max-targeted toolchain profile:

- `-O3 -flto -ffast-math -fstrict-aliasing -funroll-loops`
- `-fomit-frame-pointer -ffunction-sections -fdata-sections`
- `-mcpu=apple-m1 -mtune=apple-m1`
- link: `-Wl,-dead_strip -Wl,-dead_strip_dylibs`

| Metric | Value | Delta vs `170e566` |
| --- | ---: | ---: |
| Wall/board (PMU, board 1) | 32.70 s | -0.2% |
| CPU/board (PMU, board 1) | 32.45 s | +0.7% |
| Instructions | 346.33 B | **-1.1%** |
| L1D Miss Stores | 2.836 B | **-1.0%** |

The packed `moveType` is performance-neutral on wall/CPU time but reduces both
instruction count and store misses, confirming the smaller struct size saves
memory bandwidth.

This retained set is also the closed Stage-5 / preserved-fallback endpoint for
the current Apple-Silicon `P1.6` plan: the standard Apple `arm64` build is now
the M1 Max-optimised release build, while non-Apple and non-`arm64` builds
still retain their own compile path.

## Measurement methodology

### For A/B comparisons

1. Use the **serial-mode CPU-time** primary workload (not board-parallel wall time)
2. Rebuild `src/build/libdds.so` from the candidate code
3. Rebuild `test/build/alpha_mu` against the same library
4. Run the serial benchmark command above
5. Record CPU time from the `getrusage`-based `cpu_seconds` output
6. Compare against the frozen baseline

### For PMU profiling

1. Build `test/build/pmu_single_run` (or use `test/run_pmu_ladder_v2.sh`)
2. Run on `list9.txt` board 1, depth 2
3. Record all 6 counters: cycles, instructions, branch mispredictions,
   L1D miss loads, L1D miss stores, plus derived IPC
4. Compare deterministic counters (instructions, branch mispredictions)
   first — these are stable across runs
5. Compare cycles and wall time second — these have run-to-run noise

### Acceptance criteria for optimization changes

1. Serial CPU time per board must improve (not just be within noise)
2. PMU instruction count should not increase
3. `mismatches=0` on the benchmark workload
4. `make perf-check` must pass (dtest, regression_api, alpha_mu serial)
5. Results must be recorded in `docs/performance-log.md`

## Lessons learned from the Stage 7 optimization program

### What helped

| Change | Impact | Status |
| --- | --- | --- |
| M1 Max-specific `ABsearch` path (`170e566`) | **-20.7%** wall time vs portable baseline | **Kept** |
| Packed `moveType` (`int` to `short`) | -1.1% instructions, -1.0% store misses, neutral wall time | **Kept** |
| P-core QoS pinning (`pthread_set_qos_class_self_np`) | ~4-5% wall time under contention | **Kept** |

### What did not help (reverted)

| Change | Impact | Decision |
| --- | --- | --- |
| Hot/cold `ThreadData` split (8.1) | +2.2% cycles | Reverted |
| `pos` hot-field reorder (8.5) | +0.9% cycles | Reverted |
| `DepthLocal` shadow state (8.3) | +10.6% cycles (combined with others) | Reverted |
| NEON intrinsics for winRanks | Neutral (within noise of predecessor) | Reverted |
| `__builtin_clz` for `highestRank` | +5.8% cycles | Reverted |
| QuickTricks context-struct refactor | +2.6% instructions | Reverted |

### Key insight

**IPC is flat** across all variants (3.36-3.43). The M1 Max is not stalling on
cache misses or branch mispredictions — it is simply executing more instructions
when the compiler generates less efficient code under LTO. The correct
optimization strategy on this chip for this workload is to **minimize
instruction count**, not to optimize memory layout.

### Measurement insight

Board-parallel wall-clock benchmarks have ~20-30% run-to-run variance and
masked multiple regressions for weeks. All future A/B comparisons must use
**serial-mode CPU time** via `getrusage`.

## PGO evaluation

PGO was evaluated using the standard clang `fprofile-instr-generate` /
`fprofile-instr-use` pipeline with the canonical depth-2 list9 benchmark
as both training and evaluation workload.

Result: PGO trailed the non-PGO M1 Max build by ~2.9%. The M1 Max-specific
`ABsearch` path already achieves better code generation than PGO can discover
from training data alone.

PGO remains available via `make macos_pgo_generate` / `make pgo_merge` /
`make macos_pgo_use` but is not part of the default release build.

## M1 Max-specific code paths

The retained Apple-specific release behaviour now has two parts:

1. the `DDS_TARGET_APPLE_M1_MAX` conditional in `src/ABsearch_m1max.cpp`,
   auto-selected by the Makefile on Apple `arm64` hosts via `M1_MAX_BUILD=1`,
2. the default Apple `arm64` release toolchain profile described above.

No alpha-mu-specific M1 Max code paths were justified by measurement. The
alpha-mu hot path is dominated by DDS leaf evaluation, so DDS-side optimization
(the M1 Max `ABsearch` variant plus the retained Apple release tuning) provides
the primary benefit automatically.

## References

- Full benchmark history: `docs/performance-log.md`
- Profiling procedure: `docs/profiling.md`
- PMU ladder chart: `docs/pmu-ladder.svg`
- Performance trend graph: `docs/performance-log.svg`

