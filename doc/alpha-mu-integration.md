# Alpha-Mu Integration: Recommended Next Steps

## Goal

Integrate the alpha-mu approach into DDS in a way that preserves correctness, limits risk, and makes performance changes measurable.

The key recommendation is to proceed in **stages**:

1. **Lock down current behavior with regression tests**.
2. **Refactor only the root score-search logic first** in `src/SolverIF.cpp`.
3. **Only then consider deeper changes** in `src/ABsearch.cpp` and the transposition-table code if benchmarks justify it.

---

## Summary Recommendation

DDS is already close to a threshold-search / interval-search design. Because of that, the safest path is **not** to replace the core search wholesale.

Instead:

- keep the existing move generation in `src/Moves.cpp`
- keep the existing quick-trick and later-trick pruning
- keep the four specialized recursive search functions
- start by improving the **root-level search policy**
- defer any deeper `ABsearch0` / TT redesign until tests and benchmarks are in place

---

## Phase 0: Regression Coverage Before Search Changes

### Why

Before touching root search or `ABsearch0`, we need a reliable baseline for:

- solver output
- exact-score behavior
- par/dealer-par behavior
- consistency between related public APIs

### Existing coverage already in place

The current harness under `test/` already provides useful golden-data regression coverage:

- `test/loop.cpp`
  - `loop_solve()` validates `SolveAllChunks()` against golden `FUT`
  - `loop_calc()` validates `CalcAllTablesPBN()` against golden `TABLE`
  - `loop_par()` validates `Par()` against golden `PAR`
  - `loop_dealerpar()` validates `DealerPar()` against golden `PAR2`
  - `loop_play()` validates `AnalyseAllPlaysPBN()` against golden `TRACE`
- `test/parse.cpp` parses the `hands/*.txt` corpus
- `hands/list*.txt`, `hands/thomas1.txt`, `hands/thomas2.txt` provide baseline datasets

### Recommended additions

#### 1. Strengthen current comparisons

Update `test/compare.cpp`:

- make `compare_DEALERPAR()` also compare `number`

This closes an obvious regression gap.

#### 2. Add a focused API-regression executable

Add a new test file, for example:

- `test/regression_api.cpp`

This test should reuse the existing parsed `hands/*.txt` corpus and verify:

- `CalcDDtablePBN()` matches golden `TABLE`
- `Par()` matches golden `PAR`
- `CalcParPBN()` matches golden `PAR`
- `DealerPar()` matches golden `PAR2`
- `DealerParBin()` succeeds
- `SidesParBin()` succeeds
- `ConvertToDealerTextFormat()` returns non-empty text
- `ConvertToSidesTextFormat()` returns non-empty text
- `SolveBoardPBN(target = -1, solutions = 1, mode = 1)` returns an optimum score
- `SolveBoardPBN(target = optimum, solutions = 2, mode = 1)` returns only optimum-scoring moves
- `SolveBoardPBN(target = optimum + 1, solutions = 1, mode = 1)` fails cleanly

#### 3. Run both small and difficult datasets

Use at least:

- `hands/list10.txt`
- `hands/thomas1.txt`
- `hands/thomas2.txt`

If runtime is acceptable, also run:

- `hands/list100.txt`
- `hands/largest.txt`

### Success criteria for Phase 0

Before starting alpha-mu work, all of the following should be true:

- existing golden tests still pass
- new API-consistency tests pass
- exact-score and threshold behavior are locked down
- no known mismatch remains between `Par()`, `CalcParPBN()`, `DealerPar()`, and their binary/text conversion paths

---

## Phase 1: Root-Level Alpha-Mu Refactoring Only

### Why start here

The highest-value, lowest-risk first change is in the root driver logic, not the deep recursion.

The repeated score-search loops currently live in:

- `src/SolverIF.cpp`
  - `SolveBoardInternal()`
  - `SolveSameBoard()`
  - `AnalyseLaterBoard()`

These already do repeated threshold probes using the existing boolean search.

### Recommended change

Extract the duplicated "guess / lower bound / upper bound" loops into a shared helper in `src/SolverIF.cpp`.

That helper should:

- accept the current root position and depth
- call the existing search through `AB_ptr_list` / `AB_ptr_trace_list`
- preserve the best root move found on successful probes
- centralize the exact-score search policy

### Scope

Touch only:

- `src/SolverIF.cpp`

Do **not** change yet:

- `src/ABsearch.cpp`
- `src/Moves.cpp`
- TT layout in `src/TransTable.h`
- quick-trick or later-trick logic

### Suggested implementation target

Introduce an internal helper for exact-score search at the root, conceptually like:

- `SearchExactScoreRoot(...)`

It should wrap the existing boolean threshold probe rather than replacing it.

### Success criteria for Phase 1

- all regression tests from Phase 0 still pass
- public outputs are unchanged
- duplicated root score-search logic is reduced
- performance is measured before/after on representative hands

---

## Phase 2: Benchmark and Decide Whether Deeper Work Is Worth It

### Measure before changing recursion

After Phase 1, benchmark:

- `hands/list100.txt`
- `hands/largest.txt`
- `hands/thomas1.txt`
- `hands/thomas2.txt`

Use the existing test/benchmark harness where possible.

### Questions to answer

- Did root-only refactoring change runtime materially?
- Did node counts change?
- Is there enough headroom left to justify deeper search work?
- Are there any correctness deltas under repeated solves or play analysis?

If the benefit is small, stop here.

---

## Phase 3: Deeper Alpha-Mu Work in `ABsearch0()` Only

### Why `ABsearch0()` first

If deeper work is justified, start in:

- `src/ABsearch.cpp`

Specifically:

- `ABsearch0()`

This is the best place because it already centralizes:

- transposition-table lookup/store
- quick-trick pruning
- later-trick pruning
- terminal evaluation

### Important rule

If deeper alpha-mu logic is introduced, transposition-table values should remain based on:

- **remaining tricks from the node**

not:

- absolute final trick totals

This preserves the current TT reuse assumptions.

### Files likely involved

- `src/ABsearch.cpp`
- `src/TransTable.h`
- `src/TransTableS.cpp`
- `src/TransTableL.cpp`

### Files that should stay stable at first

- `src/Moves.cpp`
- `src/QuickTricks.cpp`
- `src/LaterTricks.cpp`
- `src/Init.cpp`

### Success criteria for Phase 3

- all Phase 0 regressions still pass
- no public API output changes unless intentionally introduced
- TT hit behavior remains valid
- benchmark gains justify the added complexity

---

## Files Most Likely to Be Touched

### Test / regression work

- `test/compare.cpp`
- `test/loop.cpp`
- `test/parse.cpp` (only if needed)
- `test/regression_api.cpp` (new)
- possibly `test/Makefiles/own_sources.txt`

### Root search refactor

- `src/SolverIF.cpp`

### Deeper search changes only if justified

- `src/ABsearch.cpp`
- `src/TransTable.h`
- `src/TransTableS.cpp`
- `src/TransTableL.cpp`

---

## Risks to Avoid

### Do not do first

- do not rewrite DDS into a single generic negamax search
- do not replace `src/Moves.cpp` move ordering before coverage is stronger
- do not store absolute scores in the transposition table
- do not start by rewriting all four `ABsearch*` functions
- do not mix search-policy cleanup with deep TT redesign in one step

### Special caution in `Par.cpp`

If `src/Par.cpp` is touched further, keep regression coverage around:

- `Par()`
- `CalcParPBN()`
- `DealerPar()`
- `DealerParBin()`
- `SidesParBin()`
- `ConvertToDealerTextFormat()`
- `ConvertToSidesTextFormat()`

---

## Recommended Order of Work

1. strengthen existing comparisons in `test/compare.cpp`
2. add `test/regression_api.cpp`
3. run regressions on `hands/list10.txt`, `hands/thomas1.txt`, `hands/thomas2.txt`
4. extract root exact-score search helper in `src/SolverIF.cpp`
5. rerun all regressions
6. benchmark
7. only if justified, prototype deeper alpha-mu in `ABsearch0()`
8. benchmark again before widening scope

---

## Definition of Done for the Next Milestone

The next milestone should be considered complete when:

- regression coverage exists for solver/par consistency
- root score-search logic is centralized in `src/SolverIF.cpp`
- public solver outputs remain unchanged on baseline corpora
- performance has been measured on easy and hard hand sets
- there is a clear go/no-go decision for deeper `ABsearch0()` work

