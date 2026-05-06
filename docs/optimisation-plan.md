# DDS Performance Optimisation Plan

_Generated 2026-04-19 from codebase review against `ABOptimisationHints.md` and `QuickTricksOptimisation.md`._

---

## Current State Summary

The performance log and clean-slate rebuild history (see
`docs/performance-log.md`) now establish that:

1. The **existing M1 Max `ABsearch` path** remains the principal kept DDS-side
   win.
2. The earlier **`8.3` `DepthLocal` scratch-pad** experiment was strongly
   regressive and has been reverted; `posPoint->winRanks[depth]` is again the
   source of truth.
3. The broader cache-layout / refactor hypotheses from the earlier Stage-5 cycle
   did **not** survive the later release/PMU and clean-slate validation pass.
4. The retained incremental result from that cycle is the compact
   **`moveType`** layout (`sizeof(moveType) == 8`), which is neutral on wall/CPU
   time but lowers instruction count and store misses.
5. The dominant hot path remains
   `ABsearch* → MakeNext → Make/Undo → QuickTricks → SolveBoardInternal`.
6. The refreshed `2026-05-05` focused `DDS_ALPHA_MU_STATS` lane closed Stage 4 /
   `PR 4`: across `2991` emitted root lines, aggregate DDS phase share was
   `77.90% ab_us`, `8.09% undo_us`, `6.24% movegen_us`, and `3.40% qt_us`;
   `SolveBoardInternal` and `SolveSameBoard` accounted for `99.77%` of measured
   phase time while `AnalyseLaterBoard` remained negligible.
7. The corrected `2026-05-06` residual-`ab_us` split, rerun after the timing
   cleanup pass, showed that the coarse top-level AB partition is now useful:
   `ab_iteration_control_us` accounts for `30.53%` of `ab_us`, while
   `ab_other_us` remains the largest single bucket at `60.74%` overall,
   `58.18%` in `SolveBoardInternal`, and `66.02%` in `SolveSameBoard`.

The focused `DDS_ALPHA_MU_STATS` lane also clarified one instrumentation concern:
the remaining `ALPHA_MU root ...` text is not evidence of a second legacy
alpha-mu-only emitter. It is the historical prefix emitted by the single shared
exact-root reporting helper in `src/SolverIF.cpp`, which is exercised by the
instrumented DDS contexts `SolveBoardInternal`, `SolveSameBoard`, and
`AnalyseLaterBoard`. Read together with the later clean-slate recovery entries,
that evidence closes the current preserved-fallback Stage-5 cycle and leaves no
presently justified `PR 5` portability sacrifice.

---

## Stage 5 outcome ledger

Stage 5 is now closed for the current preserved-fallback cycle. The table below
replaces the earlier speculative priority list with the measured outcome of each
candidate.

| Candidate | Outcome | Current status | Evidence anchor |
|---|---|---|---|
| Existing `DDS_TARGET_APPLE_M1_MAX` `ABsearch` path | Clear win | **Kept** | `docs/performance-log.md` 2026-04-18 M1 Max `ABsearch` follow-up |
| Compact `moveType` (`sizeof(moveType) == 8`) | Neutral wall/CPU, lower instructions/store misses | **Kept** | `docs/performance-log.md` 2026-04-20 clean-slate rebuild |
| `ThreadData` hot/cold split (`8.1`) | Regressive under release/PMU measurement | **Reverted / not carried forward** | `docs/performance-log.md` PMU ladder + clean-slate recovery |
| `pos` hot-field reorder (`8.5`) | Mixed to regressive | **Reverted / not carried forward** | `docs/performance-log.md` staged ladder + PMU ladder |
| `DepthLocal` scratch state (`8.3`) | Strong regression | **Reverted / not carried forward** | `docs/performance-log.md` staged ladder + PMU ladder |
| NEON helper rewrite in `ABsearch_m1max.cpp` | No retained win after recovery | **Not kept** | `docs/performance-log.md` Phase 1 + current HEAD summary |
| `highestRank[]` → CLZ | Regressive | **Reverted / not carried forward** | `docs/performance-log.md` Phase 2 + PMU ladder |
| `QuickTricks` context-struct / suit-advance refactor | Regressive in clean-slate rebuild | **Reverted / not carried forward** | `docs/performance-log.md` 2026-04-20 clean-slate rebuild |

## Current accepted endpoint

For the current Apple-Silicon `P1.6` scoped plan, the retained DDS-side result
is deliberately narrow:

- keep the existing Apple-specific `ABsearch` specialization,
- keep the compact 8-byte `moveType` layout,
- preserve the generic fallback path,
- and leave the other tested Stage-5 candidates out of the tree.

That means there is no unfinished Stage-5 optimisation carry-over at the moment.
Any future `Bucket B` idea should be treated as a **new hypothesis**, not as a
backlog item that still deserves a default implementation pass.

## Re-open criteria for future DDS work

Only re-open DDS optimisation work if all of the following are true:

1. the new idea has a concrete mechanism tied to a still-measured hotspot,
2. it is benchmarked first with the serial CPU-time and PMU methodology frozen
   in `docs/alpha-mu-benchmark-baseline.md`,
3. it preserves the generic fallback unless and until a separate Stage 6 /
   `PR 5` tradeoff is explicitly approved,
4. it passes the existing correctness gates (`regression_api`, `dtest`,
   `play_analysis_benchmark`, and the alpha-mu semantic checks where relevant).

For AB-specific follow-on instrumentation, this now means new work should focus
first on the still-dominant straight-line AB body represented by `ab_other_us`,
with `ab_iteration_control_us` as the other newly material coarse hotspot, not
on finer subdivision of already-small helper-boundary buckets.

## Notes carried forward

- `-flto` is already part of the release build and is no longer an open action
  item.
- The central lesson from the Stage-5 cycle is that instruction-count growth on
  Apple clang/LTO was a better predictor than cache-locality theory for this
  workload.
- The performance log remains the authoritative chronological record; this file
  now serves as the concise Stage-5 conclusion rather than as a speculative task
  queue.
