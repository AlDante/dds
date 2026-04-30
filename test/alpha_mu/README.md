# Alpha-Mu module boundary

This directory provides focused wrapper headers for the alpha-mu engine while the
implementation still lives in the historical `test/` incubation area.

## Current focused entry points

- `api.h` — supported solve, benchmark, exact-PBN, and reporting surface
- `bridge.h` — bridge-state, move-generation, and DDS-leaf search surface
- `tests.h` — regression-bundle entry points

The wrappers are intentionally incremental. They keep the existing build stable
while separating alpha-mu consumption from the broader test harness plumbing and
large umbrella headers.
