/**
 * @file SolveBoard.h
 * @brief Batch-solving helpers layered above the core single-board DDS solver.
 *
 * These functions are used by the multi-board APIs to schedule work, reuse exact
 * duplicates, and convert the public `boards` input into repeated calls to the
 * single-board solve path.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
 */

#ifndef DDS_SOLVEBOARD_H
#define DDS_SOLVEBOARD_H

#include <vector>

#include "dds.h"

using namespace std;


/** @brief Solve one board request on one worker. */
void SolveSingleCommon(
  const int thrId,
  const int bno);

/** @brief Copy a previously solved board result to exact duplicates. */
void CopySolveSingle(
  const vector<int>& crossrefs);

/** @brief Worker loop for chunked/batched solve requests. */
void SolveChunkCommon(
  const int thrId);

/** @brief Detect exact duplicate solve inputs for reuse within a batch. */
void DetectSolveDuplicates(
  const boards& bds,
  vector<int>& uniques,
  vector<int>& crossrefs);

#endif
