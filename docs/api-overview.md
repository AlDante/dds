# API Overview

## Public entry point families

All exported functions are declared in `include/dll.h`.

## Configuration and lifecycle

Use these functions to prepare and interrogate the library:

- `SetMaxThreads(int userThreads)`
- `SetThreading(int code)`
- `SetResources(int maxMemoryMB, int maxThreads)`
- `FreeMemory()`
- `GetDDSInfo(DDSInfo * info)`
- `ErrorMessage(int code, char line[80])`

### Typical startup pattern

1. Optionally call `SetThreading()` to choose a compiled backend.
2. Call `SetResources()` or `SetMaxThreads()` if you want to override auto-sizing.
3. Call solve / calc / play APIs.
4. Call `FreeMemory()` when releasing long-lived DDS state.

## Single-board solving

- `SolveBoard(...)`
- `SolveBoardPBN(...)`

These answer a single declarer/strain/position query and return `futureTricks`.

Typical use:

- choose a target and mode,
- call the single-board solve function,
- inspect the returned move list and associated scores.

## Double-dummy tables

- `CalcDDtable(...)`
- `CalcDDtablePBN(...)`
- `CalcAllTables(...)`
- `CalcAllTablesPBN(...)`

These compute the table of tricks for each declarer and strain. For many workflows, DDS table calculation is the natural precursor to par calculation.

## Batch solving

- `SolveAllBoards(...)`
- `SolveAllBoardsBin(...)`
- `SolveAllChunks(...)`
- `SolveAllChunksBin(...)`
- `SolveAllChunksPBN(...)`

These are the performance-oriented APIs for large corpora. They are the main entry points where the scheduler and multi-threading backends matter.

## Par calculation

- `Par(...)`
- `CalcPar(...)`
- `CalcParPBN(...)`
- `SidesPar(...)`
- `DealerPar(...)`
- `SidesParBin(...)`
- `DealerParBin(...)`
- `ConvertToSidesTextFormat(...)`
- `ConvertToDealerTextFormat(...)`

The recommended pattern is:

1. compute a DD table,
2. call `Par()` / `DealerPar()` or the binary-output variants,
3. convert to text only if presentation is needed.

## Play analysis

- `AnalysePlayBin(...)`
- `AnalysePlayPBN(...)`
- `AnalyseAllPlaysBin(...)`
- `AnalyseAllPlaysPBN(...)`

These APIs value played lines against double-dummy optimal play.

## Key public data structures

The most important public structures to know are:

- `deal`, `dealPBN`
- `futureTricks`
- `boards`, `boardsPBN`
- `ddTableResults`
- `parResults`, `parResultsDealer`, `parResultsMaster`
- `DDSInfo`

## Existing regression coverage

The maintained regression harnesses currently exercise:

- solve correctness against golden `FUT` records,
- table correctness against golden `TABLE` records,
- par and dealer-par correctness,
- play-analysis traces,
- public API consistency (`test/regression_api.cpp`),
- Timer formatting and arithmetic (`test/timer_regression.cpp`).

That regression coverage is the baseline against which alpha-mu changes should be measured.
