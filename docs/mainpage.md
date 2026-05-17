# DDS Documentation

DDS is a bridge double-dummy solver with a C-compatible public API in `include/dll.h`, a threshold-search solver core under `src/`, regression and benchmark infrastructure under `test/`, and historical narrative documentation under `doc/`.

This documentation set combines curated Markdown guides with generated API pages from selected headers.

## Contents

- [Architecture](architecture.md)
- [DDS code flow and algorithms](dds-code-flow.md)
- [Key data structures](data-structures.md)
- [API overview](api-overview.md)
- [Alpha-mu architecture and implementation](alpha-mu-architecture.md)
- [Architecture diagrams](architecture-diagrams.md)
- [Legacy DLL description guide](legacy-dll-description-guide.md)
- [C++ codebase audit recommendations](code-audit-recommendations.md)
- [Alpha-mu and DDS](alpha-mu.md)
- [Alpha-mu: bridge player's guide](alpha-mu-guide.md)
- [Alpha-mu algorithm and invariants](alpha-mu-invariants.md)
- [Alpha-mu data flow](alpha-mu-dataflow.md)
- [Alpha-mu benchmark baseline (M1 Max)](alpha-mu-benchmark-baseline.md)
- [Alpha-mu information-state contract](alpha-mu-information-state.md)
- [Apple Silicon P1.6 plan](apple-silicon-p1.6-plan.md)
- [Apple Silicon P1.6 PR plan](apple-silicon-p1.6-pr-plan.md)
- [Alpha-mu completion plan](alpha-mu-completion-plan.md)
- [Alpha-mu final acceptance checklist](alpha-mu-acceptance.md)
- [Alpha-mu future roadmap](alpha-mu-future-roadmap.md)
- [Alpha-mu multicore plan](alpha-mu-multicore-plan.md)
- [Alpha-mu test set](alpha-mu-test-set.md)
- [Profiling procedure](profiling.md)
- [Performance tracking](performance.md)
- [Performance log](performance-log.md)
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
- Alpha-mu public surface: `include/alpha_mu/*.h`
- Alpha-mu production engine: `src/alpha_mu_*.{h,cpp}`
- Tests and regression harnesses: `test/`

## Scope of this documentation

The generated site is intended to answer three questions:

1. **How DDS is structured today**.
2. **What its key public and internal interfaces are**.
3. **How alpha-mu can be added incrementally without destabilizing the solver**.

The historical files in `doc/` remain valuable and are included as source material rather than being replaced.
