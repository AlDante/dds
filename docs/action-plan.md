# Alpha-Mu Concrete Action Plan

## Purpose

This document turns the staged roadmap in `implementation-plan.md` into the next concrete execution cycle.

The first root refactor is already complete, so the next cycle should focus on **measurement first, then one root-only experiment**.

## Immediate objectives

1. Reconfirm the current correctness baseline.
2. Measure how the new shared root helper behaves on representative workloads.
3. Choose one small alpha-mu experiment that stays inside `src/SolverIF.cpp`.
4. Re-run correctness and benchmark checks after that experiment.

## Step 1 — re-run the baseline now

Re-run the existing checks before making another solver change:

- `test/build/regression_api`
- `test/build/dtest -f ../hands/list10.txt -s solve`
- at least one repeat-solve-oriented workload that exercises `SolveSameBoard()` indirectly
- at least one play-analysis workload that reaches `AnalyseLaterBoard()` if a practical harness is available

Record:

- pass/fail status,
- wall-clock time,
- any suspicious output drift,
- whether repeated solves show stable exact-score behavior.

## Step 2 — add lightweight root instrumentation

Add small, easy-to-remove instrumentation around `SearchExactScoreRoot()` in `src/SolverIF.cpp`.

The instrumentation should answer:

- how many root probes were needed,
- what the initial guess was,
- what the final exact score was,
- whether the starting hint was above or below the final score,
- which entry path was used:
  - `SolveBoardInternal()`,
  - `SolveSameBoard()`,
  - `AnalyseLaterBoard()`.

Prefer one of these styles:

- compile-time-gated counters, or
- optional timer/stat reporting that does not affect public results.

Do not change deep recursive semantics while adding this instrumentation.

## Step 3 — define a representative benchmark set

Use a benchmark set that reflects real DDS usage rather than only synthetic tight loops.

Recommended starter set:

- correctness smoke set:
  - `hands/list10.txt`
  - `hands/thomas1.txt`
  - `hands/thomas2.txt`
- larger throughput set:
  - `hands/list100.txt`
  - `hands/list1000.txt`
- repeated-position behavior:
  - workloads that naturally reuse the same or similar deals
- play-analysis behavior:
  - at least one sequence that exercises analysis after the opening lead

For each benchmark, capture:

- elapsed time,
- node count if available,
- root probe count,
- distribution of final exact scores,
- repeated-solve behavior.

## Step 4 — choose one root-only alpha-mu experiment

Only after Step 2 and Step 3 should the next solver change be selected.

The best candidates are:

1. **Improved initial guess seeding**
   - reuse prior exact scores or directional hints more deliberately.
2. **More disciplined interval management**
   - make lower/upper-bound updates clearer and easier to analyze.
3. **Hint-aware play-analysis probing**
   - improve how `hint` and `hintDir` seed the root interval in `AnalyseLaterBoard()`.

Selection rule:

- choose the change with the clearest expected benefit,
- keep it local to root orchestration,
- avoid mixing multiple experiments in one patch.

## Step 5 — keep the next code change small

For the next implementation patch:

- prefer editing only `src/SolverIF.cpp` unless a measurement helper clearly belongs elsewhere,
- avoid touching `src/ABsearch.cpp`, `src/QuickTricks.cpp`, `src/LaterTricks.cpp`, or TT storage code,
- preserve public API behavior,
- preserve `solutions == 3` behavior unless the chosen experiment directly requires root-local cleanup there.

## Step 6 — re-verify after the next change

After the next alpha-mu experiment lands, rerun:

- the public API regression suite,
- the solve regression workload,
- the benchmark set from this document,
- any targeted repeat-solve / play-analysis checks used during measurement.

The change is only a success if:

- correctness is unchanged,
- the data is understandable,
- the root-policy behavior improves on representative workloads,
- the code remains easier to evolve than before.

## Decision gate after this cycle

If the first measured root-only experiment is promising, the next follow-up should be one of:

- another root-policy refinement, or
- cleanup of the remaining `solutions == 3` root duplication.

If the results are noisy or neutral, keep measuring and refining at the root instead of moving deeper into `ABsearch*()`.

## Explicit non-goals for the next cycle

These should stay out of scope for now:

- redesigning the recursive search core,
- changing TT meaning,
- rewriting pruning logic,
- broad performance tuning unrelated to alpha-mu behavior.

## Practical summary

The next cycle should be:

1. baseline,
2. instrumentation,
3. representative benchmarks,
4. one root-only experiment,
5. full re-verification.

