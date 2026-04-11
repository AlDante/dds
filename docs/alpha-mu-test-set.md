# Alpha-Mu Test Set

## Purpose

This document defines a practical alpha-mu-oriented test set for this repository.

It is based on the two alpha-mu papers, but it uses the hand-file format already used by DDS test inputs in `hands/`.

That means these files are **practical repository-format test sets**, not literal transcriptions of every partial-endgame diagram from the papers.

## Why this test set exists

The papers emphasize that alpha-mu should be judged on more than raw double-dummy speed.

Important themes are:

- strategy fusion,
- non-locality,
- information gain / discovery play,
- repeated evaluation of related positions,
- stability on hard control positions,
- behavior under iterative deepening and cuts.

The existing DDS hand corpus is already useful for regression and throughput, but it was not organized specifically around alpha-mu goals.

The files below provide that organization.

## New alpha-mu hand files

### `hands/alpha_mu_play.txt`

A compact 3-hand family derived from the same example hands already used in `examples/hands.cpp` and in `test/play_analysis_benchmark.cpp`.

Use this file for:

- play-analysis-oriented checks,
- continuation-position reasoning,
- regression of the paper-derived "analysis after some play" family.

### `hands/alpha_mu_controls.txt`

A 2-hand control file built from `hands/thomas1.txt` and `hands/thomas2.txt`.

Use this file for:

- hard-position stability,
- correctness under difficult search conditions,
- checking that alpha-mu experiments do not destabilize extreme cases.

### `hands/alpha_mu_showcase.txt`

A 10-hand curated showcase set:

- the 3 play-analysis/example hands,
- 5 additional practical hands drawn from `hands/list10.txt`,
- the 2 hard Thomas controls.

Use this file for:

- compact before/after comparisons,
- smoke benchmarking,
- demonstrations of mixed alpha-mu-relevant families in one file.

## Family interpretation

The papers' examples fall into several conceptual families.

This repository test set maps them as follows.

### 1. Play-analysis / continuation family

Repository proxy:

- `hands/alpha_mu_play.txt`

Reason:

- these hands are already used by the play-analysis example code,
- they naturally exercise continuation-style exact evaluation,
- they are the closest existing repository-format assets to the paper's emphasis on reasoning after some play has already occurred.

### 2. Practical repeated-solve family

Repository proxy:

- the `list10`-derived portion of `hands/alpha_mu_showcase.txt`

Reason:

- these are representative real DDS hands,
- they are useful for measuring stable root behavior,
- they provide a compact family for comparing policy changes.

### 3. Hard controls

Repository proxy:

- `hands/alpha_mu_controls.txt`
- the final two records of `hands/alpha_mu_showcase.txt`

Reason:

- `thomas1` and `thomas2` are already recognized hard cases in this repository,
- they are good control positions when evaluating whether alpha-mu-related work introduces regressions.

## What is still missing

The papers also include small illustrative partial-endgame examples for:

- pure strategy-fusion failure,
- pure non-locality,
- discovery-play information gain,
- rare-bad-event avoidance.

Those examples are conceptually important, but they are not yet available in this repository as dedicated DDS hand-list records with full golden outputs.

A later follow-up should add hand-crafted repository-format files that target those motifs more directly.

## Recommended usage

For the current plan:

1. keep `test/alpha_mu_benchmark.py` as the DDS-side support baseline,
2. use `hands/alpha_mu_showcase.txt` for compact regression and demonstration,
3. use `hands/alpha_mu_play.txt` when validating play-analysis-related work,
4. use `hands/alpha_mu_controls.txt` when validating stability on difficult boards.

## Success criteria for future alpha-mu work

When a real alpha-mu prototype exists, evaluate it on these sets using:

- correctness,
- selected move stability,
- number of worlds,
- Max-move horizon,
- cut activity,
- elapsed time,
- behavior on hard controls.

