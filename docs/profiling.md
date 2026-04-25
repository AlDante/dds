# Profiling Procedure

This page records the current practical profiling workflow for DDS and the alpha-mu solver on macOS.

It is deliberately separate from `performance.md`:

- `performance.md` covers benchmark history and the standardized post-change benchmark suite,
- this page covers **how to build profiling-friendly binaries and inspect hotspots**.

## Goal

Produce a profiling build that:

- keeps symbols,
- keeps frame pointers,
- avoids LTO,
- writes outputs to separate directories,
- and does not disturb the normal optimized release build.

## Profiling build targets

From the repository root, build profiling-friendly binaries with:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds
make profile
```

This target delegates to:

- `src/Makefile` target `macos_profile`
- `test/Makefiles/Makefile_Mac_clang` target `profile_binaries`

Clean profiling outputs with:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds
make profile-clean
```

## Build toggles related to profiling and comparison runs

The Mac Makefiles now expose two important build toggles that matter when you
compare hotspot captures or benchmark results.

### `M1_MAX_BUILD`

- `src/Makefile` and `test/Makefiles/Makefile_Mac_clang` default to `M1_MAX_BUILD=1` on Apple `arm64`
- the same Makefiles default to `M1_MAX_BUILD=0` on other hosts
- when enabled, the build adds `DDS_TARGET_APPLE_M1_MAX` and selects the separate M1 Max-specific `ABsearch*` implementation

To force the portable path for an apples-to-apples comparison run, build both
the library and test binaries with the same override:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds/src
make M1_MAX_BUILD=0 macos
cd /Users/david/Documents/dev/CLionProjects/dds/test
make -f Makefiles/Makefile_Mac_clang M1_MAX_BUILD=0 alpha_mu
```

The same override also works with `PROFILE_BUILD=1` and `PGO_MODE=...` when
you want portable versus M1 Max-specific comparison data under those build
variants.

### `PGO_MODE`

The Mac Makefiles accept three modes:

- `PGO_MODE=none` — normal release build
- `PGO_MODE=generate` — instrumented build that emits `.profraw`
- `PGO_MODE=use` — optimized build that consumes merged profile data

The dedicated targets are:

- `src/Makefile`: `macos_pgo_generate`, `macos_pgo_use`, `pgo_merge`
- `test/Makefiles/Makefile_Mac_clang`: `pgo_generate_binaries`, `pgo_use_binaries`

Default output locations:

- instrumented library: `src/build-pgo-generate/libdds.so`
- instrumented test binaries: `test/build-pgo-generate/`
- merged profile data: `src/build-pgo-generate/pgo-data/default.profdata`
- profile-using library: `src/build-pgo-use/libdds.so`
- profile-using test binaries: `test/build-pgo-use/`

Typical PGO workflow:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds/src
make macos_pgo_generate
cd /Users/david/Documents/dev/CLionProjects/dds/test
make -f Makefiles/Makefile_Mac_clang pgo_generate_binaries
export DYLD_LIBRARY_PATH=../src/build-pgo-generate
export LLVM_PROFILE_FILE=../src/build-pgo-generate/pgo-data/alpha_mu_%p.profraw
./build-pgo-generate/alpha_mu benchmark_alpha ../hands/list9.txt 2 0 --parallel board --board-workers 10
cd /Users/david/Documents/dev/CLionProjects/dds/src
make pgo_merge
make macos_pgo_use
cd /Users/david/Documents/dev/CLionProjects/dds/test
make -f Makefiles/Makefile_Mac_clang pgo_use_binaries
```

If you override `PGO_PROFILE_DIR` or `PGO_PROFILE_DATA`, keep the same values
for both the library and the test-binary builds.

## Profiling build characteristics

The profiling targets currently use:

- `-O2`
- `-g`
- `-fno-omit-frame-pointer`

and intentionally avoid `-flto`.

The resulting outputs are written to separate directories:

- shared library: `src/build-profile/libdds.so`
- test binaries: `test/build-profile/`

Important profiled binaries include:

- `test/build-profile/dtest`
- `test/build-profile/regression_api`
- `test/build-profile/play_analysis_benchmark`
- `test/build-profile/alpha_mu`

## Recommended first profiling target

For alpha-mu work, start with a single-board depth-3 run before trying a longer all-board benchmark.

From `test/`:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds/test
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/alpha_mu benchmark_alpha ../hands/list10.txt 3 1 2
```

Arguments:

1. `benchmark_alpha`
2. hand file `../hands/list10.txt`
3. alpha-mu depth `3`
4. max boards `1`
5. skip board `2`

This is usually long enough to expose hot paths while remaining much easier to inspect than a full long run.

## Profiling the longer depth-3 alpha-mu run

To profile the larger depth-3 run directly:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds/test
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/alpha_mu benchmark_alpha ../hands/list10.txt 3 0 2
```

This means:

- depth `3`
- all boards in the file
- skip board `2`

For board-parallel throughput profiling on the same workload, use explicit benchmark options:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds/test
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/alpha_mu benchmark_alpha ../hands/list10.txt 3 0 2 --parallel board --board-workers 4
```

The machine-readable benchmark output now records the reported `parallel`, `board_workers`, `root_workers`, `dds_thread_id`, and `configured_board_workers` fields on progress, per-board, checkpoint, and summary lines. In board-parallel mode, recursive progress lines are intentionally suppressed so worker output does not interleave unpredictably.

## Profiling DDS solving instead of alpha-mu

For core DDS solve-path profiling:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds/test
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/dtest -f ../hands/list100.txt -s solve
```

For play-analysis profiling:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds/test
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/play_analysis_benchmark
```

## Instruments Time Profiler procedure

`Instruments` Time Profiler is the recommended first profiler on macOS for this codebase.

### Setup

Use these values when profiling the alpha-mu solver.

- Executable:
  - `/Users/david/Documents/dev/CLionProjects/dds/test/build-profile/alpha_mu`
- Working directory:
  - `/Users/david/Documents/dev/CLionProjects/dds/test`
- Environment:
  - `DYLD_LIBRARY_PATH=/Users/david/Documents/dev/CLionProjects/dds/src/build-profile`
- Arguments for the first focused run:
  - `benchmark_alpha ../hands/list10.txt 3 1 2`

### Suggested workflow

1. Open `Instruments`.
2. Choose `Time Profiler`.
3. Set the executable, working directory, environment, and arguments listed above.
4. Record the run.
5. Let it complete, or stop once you have a stable sample set.
6. Inspect the hottest self time and hottest subtree time.

### Useful call-tree toggles

In the Time Profiler call tree, the most useful first toggles are usually:

- `Invert Call Tree`
- `Hide System Libraries`
- `Separate by Thread` when thread structure matters
- `Flatten Recursion` when recursive stacks dominate the view

## Quick CLI spot-check with `sample`

For a quick first look without using the full Instruments UI:

1. start the profiled workload,
2. find its PID,
3. run `sample`.

Example:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds/test
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/alpha_mu benchmark_alpha ../hands/list10.txt 3 1 2
sample <PID> 10 -file /tmp/alpha_mu_depth3_sample.txt
```

This is useful for answering a fast question such as:

- whether time is mostly in DDS or alpha-mu,
- whether symbols are readable,
- whether the run is stuck in one unexpectedly hot loop.

## Hotspot checklist for the first capture

Use this checklist when reading the first profile.

### Build sanity

- [ ] Are you profiling `build-profile`, not the normal `build` outputs?
- [ ] Is `DYLD_LIBRARY_PATH` pointing at `src/build-profile`?
- [ ] Do stack traces contain readable DDS and alpha-mu symbols?
- [ ] Are you profiling a workload long enough to produce stable samples?

### Top-level attribution

- [ ] Is most time in DDS perfect-information solving?
- [ ] Is most time in alpha-mu bridge continuation search?
- [ ] Is a surprising amount of time spent in world generation or explanation logic?
- [ ] Is startup, file parsing, or harness code dominating the capture unexpectedly?

### DDS-side hotspots to look for

- [ ] `SolveBoardPBN`
- [ ] `SolveBoard` / `SolveBoardInternal`
- [ ] `ABsearch*`
- [ ] `QuickTricks`
- [ ] `LaterTricks`
- [ ] move generation and ordering
- [ ] transposition-table lookup/store paths

### Alpha-mu-side hotspots to look for

- [ ] `SearchBridgeStateInternal`
- [ ] `MakeBridgeDDSLeafFront`
- [ ] `SearchToy` when using toy-semantic fixtures
- [ ] `ParetoFront` operations
- [ ] `OutcomeVector` operations
- [ ] `GeneratePossibleWorlds`
- [ ] `ConstructCandidateWorldsFromHistory`

### Interpretation questions

- [ ] Is the bottleneck leaf evaluation, front manipulation, or world supply?
- [ ] Is the hottest function hot because of self time or because of callees beneath it?
- [ ] Is recursion depth causing the cost, or just the number of repeated leaf calls?
- [ ] Is profiling noise coming from very small workloads rather than real solver cost?
- [ ] Would a one-board run and an all-board run tell the same performance story?

## Practical default recommendation

For alpha-mu profiling, the usual order should be:

1. `make profile`
2. profile `benchmark_alpha ../hands/list10.txt 3 1 2`
3. confirm symbols and top hotspots are readable
4. then profile `benchmark_alpha ../hands/list10.txt 3 0 2` if a longer run is needed

## Relationship to the benchmark suite

For repeatable benchmark comparisons after code changes, keep using:

```zsh
python3 test/standard_performance.py
```

For profiling, use the dedicated `build-profile` targets described on this page instead.

