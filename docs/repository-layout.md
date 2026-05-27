# Repository Layout and Directory Guide

This page documents what the main DDS repository directories currently contain and how they relate to the supported build and documentation workflows.

## At a glance

| Path | What it contains | Notes |
| --- | --- | --- |
| `README.md` | Top-level project overview and release summary | Start here for a quick orientation |
| `INSTALL` | Supported build, install, test, profiling, sanitizer, and PGO workflow | Canonical build instructions |
| `Makefile` | Repository-level convenience targets | Wraps common docs/build/check flows |
| `ChangeLog` | Release notes and version history | Human-maintained changelog |
| `LICENSE` | Apache 2.0 license text | Distribution license |
| `.git/` | Git repository metadata | Version-control internals |
| `.github/` | Repository automation metadata | Currently only an empty `copilot-instructions.md` placeholder |
| `.idea/` | Local CLion/IntelliJ project metadata | Editor configuration for this checkout |
| `build/` | Top-level CMake/Doxygen-generated files | IDE/docs support only, not the supported production build surface |
| `doc/` | Historical documentation archive | Older DLL/API descriptions, algorithm notes, reports, and exported assets |
| `docs/` | Maintained documentation set | Markdown guides, Doxygen config, research notes, and generated HTML |
| `examples/` | Minimal sample programs using DDS APIs | Includes example-specific Makefiles and local build outputs |
| `hands/` | Input datasets for tests and benchmarks | Small curated sets plus larger historical corpora |
| `include/` | Public headers | Main C API plus alpha-mu wrapper headers |
| `src/` | DDS library implementation | Core solver code, alpha-mu engine code, platform Makefiles, and build outputs |
| `test/` | Regression tools, benchmarks, support scripts, and test-only shims | Main correctness and performance harness area |

## Top-level directories

### `.git/`

The Git metadata directory for the checkout. It stores version-control state and history for the repository and is not part of the DDS source or documentation surface.

### `.github/`

Repository metadata for GitHub-side tooling. In this checkout it currently contains only `copilot-instructions.md`, and that file is empty.

### `.idea/`

Local CLion/IntelliJ project metadata for this checkout. The current contents include files such as `misc.xml`, `workspace.xml`, and the module file `dds2.iml`.

### `build/`

A generated top-level CMake/Doxygen workspace. It contains files such as `CMakeCache.txt`, `CMakeFiles/`, and generated Makefiles. Treat it as an IDE/documentation artifact rather than the supported way to build DDS itself.

The supported production builds write into directory trees under `src/`, `test/`, `examples/`, and `docs/` instead.

### `doc/`

Historical documentation archive. This directory preserves legacy source material such as:

- DLL/API descriptions in Markdown, HTML, PDF, RTF, and MHT form,
- algorithm notes,
- performance and benchmarking reports,
- older office-document exports,
- archived HTML asset folders such as `DLL-dds_x-Dateien/`.

Use `docs/legacy-docs.md` when you want a curated entry point into this older material.

### `docs/`

The maintained documentation entry point. It combines current Markdown guides with a Doxygen configuration.

Important contents include:

- `mainpage.md` — generated-site landing page,
- architecture, data-structure, API, profiling, performance, and planning guides,
- alpha-mu design, roadmap, benchmarking, and acceptance documents,
- `README.md` — how to build the docs,
- `Doxyfile` and `Makefile` — documentation build config,
- `background/` — research papers and optimisation notes used as reference material,
- `build/` — generated documentation output, including `build/html/` and Doxygen logs.

### `examples/`

Small standalone sample programs showing how to call individual DDS library functions.

Notable contents:

- example sources such as `SolveBoard.cpp`, `CalcDDtable.cpp`, `AnalysePlayPBN.cpp`, and `DealerPar.cpp`,
- `hands.cpp` / `hands.h` helper code shared by the examples,
- `README` with quick build instructions,
- `Makefiles/` with platform-specific example build files,
- `build/` for generated example binaries and objects,
- `libdds.so`, a local shared-library copy used by example builds in some workflows.

### `hands/`

Input data files used by regression tests, benchmarks, and example runs.

The directory includes:

- `list*.txt` subsets for quick and medium-sized runs,
- `masterDD.txt` as the large historical superset,
- `largest.txt`, `thomas1.txt`, and `thomas2.txt` for especially hard cases,
- `sol*.txt` pre-solved table data,
- `alpha_mu_play.txt`, `alpha_mu_showcase.txt`, and `alpha_mu_controls.txt` for curated alpha-mu-focused scenarios,
- `README` describing the provenance and intended use of the datasets.

### `include/`

Public header surface for DDS.

Current structure:

- `dll.h` — the main public C-compatible DDS API,
- `portab.h` — portability definitions used by consumers that want them,
- `alpha_mu/` — maintained public alpha-mu wrappers:
  - `api.h`
  - `bridge.h`
  - `core.h`

### `src/`

The DDS library implementation directory and the main solver-engine codebase.

It contains:

- core DDS search/orchestration code such as `SolverIF.*`, `ABsearch.*`, `Moves.*`, `QuickTricks.*`, `LaterTricks.*`, `Memory.*`, `Scheduler.*`, and `ThreadMgr.*`,
- transposition-table and timing/statistics support files,
- `dds.cpp` / `dds.h` and related public-interface glue,
- alpha-mu production engine files such as `alpha_mu_core.cpp`, `alpha_mu_bridge.cpp`, `alpha_mu_worlds.cpp`, `alpha_mu_decision.cpp`, and reporting/support modules,
- platform-specific library Makefiles under `Makefiles/`,
- `CMakeLists.txt` for IDE indexing and documentation support,
- multiple generated build directories.

Common generated output directories currently present here include:

- `build/` — normal release outputs,
- `build-profile/` — profiling-friendly outputs,
- `build-asan/` — AddressSanitizer lane,
- `build-cpp23/`, `build-cpp2c/` — alternate compiler/standard experiments,
- `build-instrumented/` — compile-time instrumentation lane,
- `build-m1-generic/` — Apple portability comparison lane,
- `cmake-build-debug/` — IDE-generated CMake output.

### `test/`

The main test, regression, benchmark, and tooling area.

It currently contains:

- regression and correctness binaries such as `dtest.cpp`, `regression_api.cpp`, and `compare.cpp`,
- alpha-mu regression and benchmark programs such as `alpha_mu.cpp`, `alpha_mu_tests.cpp`, `play_analysis_benchmark.cpp`, and `moves_sort_benchmark.cpp`,
- support code for parsing, argument handling, reporting, and timing,
- Python scripts for alpha-mu benchmarking, backend comparison, performance-log generation, and full-suite runs,
- shell runners such as `run_cpu_benchmark_ladder.sh` and the `run_pmu_ladder*.sh` family,
- `README.alpha-mu.md` and `README.alpha-mu-solver.md` for alpha-mu-specific guidance,
- `alpha_mu/` containing test-local alpha-mu headers and compatibility shims,
- `Makefiles/` with platform-specific test build files,
- generated build directories matching the library variants (`build/`, `build-profile/`, `build-asan/`, `build-cpp23/`, `build-cpp2c/`, `build-instrumented/`, `build-m1-generic/`, `build-std11-check/`),
- captured timing/statistics artifacts such as `timer*.txt`, `movestats*.txt`, and `TTstats*.txt`.

## Notable nested directories

### `docs/background/`

Background reading and optimisation research referenced by the maintained docs. The current contents include papers, SIMD notes, optimisation technique write-ups, and extracted arXiv material.

### `docs/build/`

Generated documentation output and logs. At the moment it contains the generated `html/` site plus `doxygen-run.log` and `doxygen-clean.log`.

### `examples/Makefiles/`

Platform-specific example build definitions copied or referenced when building the example programs manually.

### `include/alpha_mu/`

Maintained alpha-mu public wrapper headers. Prefer these over the older test-area compatibility include paths.

### `src/Makefiles/`

Canonical platform-specific library build definitions and dependency manifests:

- Mac static/shared variants,
- Linux static/shared variants,
- Windows (`Visual`, `mingw`, `cygwin`) variants,
- dependency source lists such as `sources.txt`, `depends_o.txt`, and `depends_obj.txt`.

### `test/Makefiles/`

Platform-specific test-binary build definitions and dependency manifests. This is the supported surface for building regression tools and benchmarks.

### `test/alpha_mu/`

A narrow compatibility layer for test-only alpha-mu includes. Its `README.md` explains that the public wrappers now live under `include/alpha_mu/`, while this directory keeps older include paths and remaining test-only entry points working.

## Generated-output conventions

DDS currently uses different generated-output roots depending on what you build:

- documentation: `docs/build/`
- library release build: `src/build/`
- library profiling build: `src/build-profile/`
- library sanitizer/instrumented variants: `src/build-*`
- test release build: `test/build/`
- test profiling build: `test/build-profile/`
- test sanitizer/instrumented variants: `test/build-*`
- examples: `examples/build/`
- IDE/CMake scratch: top-level `build/` and sometimes `src/cmake-build-debug/`

This is why the repository can contain several `build*` directories at once: they serve different tools and should not be conflated.

## Practical navigation order

If you are new to the repository, the most useful order is usually:

1. `README.md`
2. `INSTALL`
3. `docs/mainpage.md`
4. `docs/build-and-clion.md`
5. `docs/architecture.md`
6. `include/dll.h`
7. `src/`
8. `test/`
9. `doc/` for historical context


