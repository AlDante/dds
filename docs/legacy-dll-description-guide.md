# Legacy DLL Description Guide

## Purpose

The repository still contains the historical DLL/API description in:

- `../doc/dll-description.md`

That file is valuable as an archival source, but it was authored in an older
HTML-heavy style that produces substantial Doxygen parsing noise in the modern
site build.

This page serves as the maintained bridge between the current generated docs and
that historical reference.

## What the historical file contains

The legacy DLL description documents:

- the exported DDS API families,
- argument lists for the public functions,
- structure layouts and encodings,
- operational notes about batching, transposition-table reuse, and threading,
- par-calculation behavior,
- play-analysis behavior,
- return codes,
- and the revision history of the DLL interface.

## How to read it today

For current work, use the maintained docs first:

1. `mainpage.md`
2. `api-overview.md`
3. `architecture.md`
4. `dds-code-flow.md`
5. `data-structures.md`
6. generated API pages for `include/dll.h`

Then use `../doc/dll-description.md` when you need historical wording, legacy API
context, or detailed examples from the original DLL-era documentation.

## Mapping from the historical file to the maintained docs

| Historical topic | Preferred maintained source |
| --- | --- |
| Public entry points | `api-overview.md` and `include/dll.h` |
| Public structs and encodings | `data-structures.md` and `include/dll.h` |
| Single-board solve semantics | `dds-code-flow.md`, `architecture.md`, `src/SolverIF.h` |
| Batch solve / chunk behavior | `dds-code-flow.md`, `src/SolveBoard.h`, `src/Scheduler.h` |
| DD table calculation | `dds-code-flow.md`, `src/CalcTables.h` |
| Par calculation | `dds-code-flow.md`, `src/Par.cpp`, `include/dll.h` |
| Play analysis | `dds-code-flow.md`, `src/PlayAnalyser.h` |
| Return codes | `include/dll.h` |
| Historical revision log | `../doc/dll-description.md` |

## Why this guide exists

The generated docs aim to be:

- navigable,
- warning-clean,
- cross-linked to current code,
- and maintainable as DDS and alpha-mu evolve.

The original `../doc/dll-description.md` remains important, but it is better
preserved as a raw historical artifact than as a heavily parsed generated-docs
page.

## Archival status

The original file remains part of the repository and should not be treated as
obsolete. It is simply no longer used directly as a generated Doxygen input.

