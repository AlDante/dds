# DDS Documentation

This directory contains the maintained documentation entry point for the DDS codebase.

It combines:

- curated Markdown guides for architecture and implementation work,
- generated Doxygen pages for selected public and internal headers,
- links to the historical material already present in `../doc/`.

## Prerequisite

Install `doxygen` and ensure it is available on your `PATH`.

## Build

From the repository root:

```sh
make docs
```

Or from inside this directory:

```sh
make html
```

The generated HTML site will be written to:

```text
build/html/index.html
```

For DDS library and test-binary build variants, including production builds,
CLion setup, `M1_MAX_BUILD`, profiling outputs, and `PGO_MODE=generate/use`,
see `build-and-clion.md`, the repository-level `INSTALL` guide, and
`profiling.md`.

## Clean

```sh
make -C docs clean
```

## Contents

The generated site includes:

- `mainpage.md` — landing page
- `repository-layout.md` — repository-level directory guide and generated-output map
- `architecture.md` — high-level DDS architecture
- `dds-code-flow.md` — detailed DDS request flow, recursive search roles, and algorithm notes
- `dds-invariants.md` — concise solver-side invariant guide for core DDS changes
- `api-overview.md` — key public APIs and usage patterns
- `data-structures.md` — important internal and public data structures
- `alpha-mu-architecture.md` — production alpha-mu code layout, search semantics, and world pipeline
- `architecture-diagrams.md` — D2, Mermaid, PlantUML, and GraphViz architecture views
- `legacy-dll-description-guide.md` — maintained bridge to the historical DLL/API description in `../doc/dll-description.md`
- `code-audit-recommendations.md` — recommended C++ correctness, performance, and software-design improvements from the repository-wide code review
- `alpha-mu.md` — alpha-mu background and DDS integration notes
- `alpha-mu-information-state.md` — current contract for hard constraints, derived follow-suit facts, and reporting-only plausibility hints in alpha-mu world construction
- `apple-silicon-p1.6-plan.md` — Apple-Silicon specialization and threading plan for balancing DDS portability against alpha-mu performance needs
- `apple-silicon-p1.6-pr-plan.md` — PR-sized execution sequence for the Apple-Silicon P1.6 work, starting with the alpha-mu worker-backend abstraction
- `alpha-mu-multicore-plan.md` — staged plan for multicore alpha-mu implementation and regression validation
- `alpha-mu-roadmap.md` — current completion estimate and staged roadmap from the current engine-incubation state to a full post-mortem alpha-mu evaluator
- `alpha-mu-future-roadmap.md` — deferred post-Stage-9 roadmap for optional future alpha-mu enhancements beyond the completed repository scope
- `alpha-mu-test-set.md` — paper-derived alpha-mu test families and hand sets
- `build-and-clion.md` — supported Makefile build flags, manual build commands, and recommended CLion configuration
- `profiling.md` — profiling build targets, Instruments workflow, and hotspot checklist
- `implementation-plan.md` — staged implementation roadmap
- `action-plan.md` — concrete next-cycle execution checklist for the next engine-building iteration
- `performance.md` — historical performance notes and the standardized post-change benchmark workflow
- `performance-log.md` — append-only record of standardized benchmark runs
- `legacy-docs.md` — guide to the historical documentation in `../doc/`

Selected existing Markdown documentation from the repository is also included in the Doxygen input set.

## Build-system ownership

- The repository Makefiles are the supported build surface for the DDS library,
  tests, examples, sanitizer lanes, and instrumentation lanes.
- `src/CMakeLists.txt` is intentionally documentation/IDE-only and should not be
  treated as the canonical library/test build.

