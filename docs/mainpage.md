# DDS Documentation

DDS is a bridge double-dummy solver with a C-compatible public API in `include/dll.h`, a threshold-search solver core under `src/`, regression and benchmark infrastructure under `test/`, and historical narrative documentation under `doc/`.

This documentation set combines curated Markdown guides with generated API pages from selected headers.

## Contents

- [Architecture](architecture.md)
- [Key data structures](data-structures.md)
- [API overview](api-overview.md)
- [Alpha-mu and DDS](alpha-mu.md)
- [Alpha-mu multicore plan](alpha-mu-multicore-plan.md)
- [Alpha-mu test set](alpha-mu-test-set.md)
- [Profiling procedure](profiling.md)
- [Implementation plan](implementation-plan.md)
- [Concrete action plan](action-plan.md)
- [Legacy and existing documentation](legacy-docs.md)

## Source map

- Public API: `include/dll.h`
- Core internal types: `src/dds.h`
- Root solve orchestration: `src/SolverIF.cpp`, `src/SolverIF.h`
- Recursive search: `src/ABsearch.cpp`
- Move generation: `src/Moves.cpp`, `src/Moves.h`
- Quick/later trick pruning: `src/QuickTricks.h`, `src/LaterTricks.h`
- Transposition tables: `src/TransTable.h`, `src/TransTableS.*`, `src/TransTableL.*`
- Per-thread memory and state: `src/Memory.h`
- Parallel execution and scheduling: `src/System.h`, `src/Scheduler.h`, `src/Init.cpp`
- Tests and regression harnesses: `test/`

## Scope of this documentation

The generated site is intended to answer three questions:

1. **How DDS is structured today**.
2. **What its key public and internal interfaces are**.
3. **How alpha-mu can be added incrementally without destabilizing the solver**.

The historical files in `doc/` remain valuable and are included as source material rather than being replaced.
