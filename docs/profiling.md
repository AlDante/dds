# Profiling Procedure

This page records the current practical profiling workflow for DDS and the alpha-mu prototype on macOS.

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
- `test/build-profile/alpha_mu_prototype`

## Recommended first profiling target

For alpha-mu work, start with a single-board depth-3 run before trying a longer all-board benchmark.

From `test/`:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds/test
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/alpha_mu_prototype benchmark_alpha ../hands/list10.txt 3 1 2
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
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/alpha_mu_prototype benchmark_alpha ../hands/list10.txt 3 0 2
```

This means:

- depth `3`
- all boards in the file
- skip board `2`

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

Use these values when profiling the alpha-mu prototype.

- Executable:
  - `/Users/david/Documents/dev/CLionProjects/dds/test/build-profile/alpha_mu_prototype`
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
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/alpha_mu_prototype benchmark_alpha ../hands/list10.txt 3 1 2
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

