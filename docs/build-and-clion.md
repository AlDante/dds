# Build and CLion Guide

This page records the practical build workflow for DDS on macOS and the recommended CLion setup for working with the repository.

It complements:

- `INSTALL`, which defines the supported build surface and repository-wide build toggles,
- `profiling.md`, which focuses on profiling-friendly builds and hotspot inspection.

## Supported build system

The supported build system for the library, tests, examples, sanitizer lanes, and instrumentation lanes is the repository Makefiles.

`src/CMakeLists.txt` is intentionally **documentation/IDE-only**. It is useful for CLion indexing and documentation generation, but it is not the canonical way to build DDS.

## Current production build on this machine

The current machine is Apple `arm64`, so the Makefiles default to:

- `M1_MAX_BUILD=1`
- `PGO_MODE=none`
- `PROFILE_BUILD=0`

That means the normal production build enables:

- `DDS_TARGET_APPLE_M1_MAX`
- `DDS_THREADS_GCD`
- `DDS_THREADS_STL`

## Production compiler and linker flags

### Library build (`src/Makefile`)

The current production library build uses:

```text
-fPIC
-O3
-flto
-mtune=generic
-std=c++11
-Wshadow
-Wsign-conversion
-pedantic
-Wall
-Wextra
-Wcast-align
-Wcast-qual
-Wctor-dtor-privacy
-Wdisabled-optimization
-Winit-self
-Wmissing-declarations
-Wmissing-include-dirs
-Wcomment
-Wold-style-cast
-Woverloaded-virtual
-Wredundant-decls
-Wsign-promo
-Wstrict-overflow=1
-Wswitch-default
-Wundef
-Werror
-Wno-unused
-Wno-unknown-pragmas
-Wno-long-long
-Wno-format
-DDDS_TARGET_APPLE_M1_MAX
-DDDS_THREADS_GCD
-DDDS_THREADS_STL
```

The normal library link step uses:

```text
-shared
-fPIC
```

Outputs are written to:

```text
src/build/libdds.so
src/build/obj/
```

### Test and benchmark build (`test/Makefiles/Makefile_Mac_clang`)

The production test binaries use the same release optimization level and the same warning set:

```text
-O3
-flto
-mtune=generic
-std=c++11
-Wshadow
-Wsign-conversion
-pedantic
-Wall
-Wextra
-Wcast-align
-Wcast-qual
-Wctor-dtor-privacy
-Wdisabled-optimization
-Winit-self
-Wmissing-declarations
-Wmissing-include-dirs
-Wcomment
-Wold-style-cast
-Woverloaded-virtual
-Wredundant-decls
-Wsign-promo
-Wstrict-overflow=1
-Wswitch-default
-Wundef
-Werror
-Wno-unused
-Wno-unknown-pragmas
-Wno-long-long
-Wno-format
-DDDS_THREADS_GCD
-DDDS_THREADS_STL
-DDDS_TARGET_APPLE_M1_MAX
```

These binaries link against:

```text
../src/build/libdds.so
```

with runtime library lookup set to:

```text
@loader_path/../../src/build
```

Outputs are written to:

```text
test/build/
test/build/obj/
```

## Build variants

## Production / release

This is the normal optimized build.

- library output: `src/build/libdds.so`
- test output: `test/build/`
- optimization flags: `-O3 -flto`

## Profile / debug-like

DDS does not currently have a dedicated repository-supported `-O0 -g` debug lane.

The supported debugger-friendly build is the profiling build:

- optimization flags: `-O2 -g -fno-omit-frame-pointer`
- library output: `src/build-profile/libdds.so`
- test output: `test/build-profile/`

This is the recommended CLion configuration for stepping through code or taking Time Profiler captures.

## Performance-test builds

The performance-test tools usually use the normal release build.

Important binaries include:

- `test/build/alpha_mu`
- `test/build/moves_sort_benchmark`
- `test/build/regression_api`
- `test/build/dtest`

## Manual build commands

All commands below are run from the repository root:

```text
/Users/david/Documents/dev/CLionProjects/dds
```

### Production build

Build the release library and the main test binaries:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds
make perf-build
```

This builds:

- `src/build/libdds.so`
- `test/build/dtest`
- `test/build/regression_api`
- `test/build/alpha_mu`

### Explicit production build steps

If you prefer to build the library and test tools separately:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds
make -C src macos
make -C test -f Makefiles/Makefile_Mac_clang dtest regression_api alpha_mu moves_sort_benchmark
```

### Profile / debug-like build

Build the profiling-friendly binaries:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds
make profile
```

Equivalent explicit form:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds
make -C src macos_profile
make -C test -f Makefiles/Makefile_Mac_clang profile_binaries
```

### Portable-path comparison build

To force the non-Apple-specialized code path, build the library and test tools with the same override:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds
make -C src M1_MAX_BUILD=0 macos
make -C test -f Makefiles/Makefile_Mac_clang M1_MAX_BUILD=0 dtest regression_api alpha_mu moves_sort_benchmark
```

## Running the built binaries manually

The test tools link against the shared library in `src/build` or `src/build-profile`, so `DYLD_LIBRARY_PATH` must point at the matching library directory.

### Release binaries

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds/test
DYLD_LIBRARY_PATH=../src/build ./build/regression_api
DYLD_LIBRARY_PATH=../src/build ./build/dtest -f ../hands/list10.txt -s solve
DYLD_LIBRARY_PATH=../src/build ./build/moves_sort_benchmark
DYLD_LIBRARY_PATH=../src/build ./build/alpha_mu benchmark_alpha ../hands/list9.txt 2 0 --parallel board --board-workers 8
```

### Profile / debug-like binaries

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds/test
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/regression_api
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/dtest -f ../hands/list10.txt -s solve
DYLD_LIBRARY_PATH=../src/build-profile ./build-profile/alpha_mu benchmark_alpha ../hands/list9.txt 2 0 --parallel serial --board-workers 1 --root-workers 1 --dds-thread-id 0
```

## Recommended CLion setup

The most practical CLion setup is:

1. use the CMake project only for indexing, navigation, and IDE services,
2. use custom Run/Debug configurations that call the repository Makefiles for real builds,
3. launch the built binaries with the appropriate `DYLD_LIBRARY_PATH`.

## Step 1: open the project

Open:

```text
/Users/david/Documents/dev/CLionProjects/dds
```

CLion should detect `src/CMakeLists.txt`. Keep it enabled for indexing, but do not treat it as the authoritative DDS build.

## Step 2: keep a lightweight IDE CMake profile

Create or keep a simple CMake profile such as:

- `IDE`

Its purpose is only:

- code completion,
- navigation,
- inspections,
- access to the documentation target.

## Step 3: create build configurations that call the Makefiles

Create three shell-script or external-tool style configurations.

### Production build configuration

Name:

- `DDS Production Build`

Working directory:

```text
/Users/david/Documents/dev/CLionProjects/dds
```

Command:

```zsh
make perf-build
```

### Profile / debug-like build configuration

Name:

- `DDS Profile Build`

Working directory:

```text
/Users/david/Documents/dev/CLionProjects/dds
```

Command:

```zsh
make profile
```

### Performance-tools build configuration

Name:

- `DDS Perf Tools Build`

Working directory:

```text
/Users/david/Documents/dev/CLionProjects/dds
```

Command:

```zsh
make -C test -f Makefiles/Makefile_Mac_clang moves_sort_benchmark alpha_mu
```

## Step 4: create run/debug configurations for the important binaries

### `regression_api` (release)

- executable: `/Users/david/Documents/dev/CLionProjects/dds/test/build/regression_api`
- working directory: `/Users/david/Documents/dev/CLionProjects/dds/test`
- environment: `DYLD_LIBRARY_PATH=../src/build`
- program arguments: `../hands/list10.txt ../hands/thomas1.txt`

### `dtest` solve (release)

- executable: `/Users/david/Documents/dev/CLionProjects/dds/test/build/dtest`
- working directory: `/Users/david/Documents/dev/CLionProjects/dds/test`
- environment: `DYLD_LIBRARY_PATH=../src/build`
- program arguments: `-f ../hands/list10.txt -s solve`

### `moves_sort_benchmark` (release)

- executable: `/Users/david/Documents/dev/CLionProjects/dds/test/build/moves_sort_benchmark`
- working directory: `/Users/david/Documents/dev/CLionProjects/dds/test`
- environment: `DYLD_LIBRARY_PATH=../src/build`
- no arguments required

### `alpha_mu` (profile)

- executable: `/Users/david/Documents/dev/CLionProjects/dds/test/build-profile/alpha_mu`
- working directory: `/Users/david/Documents/dev/CLionProjects/dds/test`
- environment: `DYLD_LIBRARY_PATH=../src/build-profile`
- program arguments: `benchmark_alpha ../hands/list9.txt 2 0 --parallel serial --board-workers 1 --root-workers 1 --dds-thread-id 0`

## Step 5: use “Before launch” for supported builds

To keep CLion launches consistent with the supported Makefile builds, add a before-launch build step to each run configuration.

Recommended mapping:

- release runs: `make perf-build`
- profile runs: `make profile`
- performance experiment runs: `make -C test -f Makefiles/Makefile_Mac_clang moves_sort_benchmark alpha_mu`

This keeps CLion responsible for launching and debugging, while the repository Makefiles remain responsible for the actual build products.

## Practical recommendation

For day-to-day work, treat these as the three main workflows.

### Production

Build:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds
make perf-build
```

Run with:

```text
DYLD_LIBRARY_PATH=../src/build
```

### Profile / debug-like

Build:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds
make profile
```

Run with:

```text
DYLD_LIBRARY_PATH=../src/build-profile
```

### Performance experiments

Build:

```zsh
cd /Users/david/Documents/dev/CLionProjects/dds
make -C test -f Makefiles/Makefile_Mac_clang moves_sort_benchmark alpha_mu
```

Run from `test/` with the release library path.

## Notes

- The repository Makefiles were verified on this checkout for `perf-build`, `profile`, and the current `moves_sort_benchmark` / `alpha_mu` test-binary builds.
- If a true low-optimization debugger build becomes desirable later, it should be added as an explicit repository-supported Makefile lane rather than improvised only in CLion.

