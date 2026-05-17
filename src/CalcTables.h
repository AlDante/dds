/**
 * @file CalcTables.h
 * @brief Batch helpers for double-dummy table calculation across all declarers.
 *
 * The table-calculation path reuses the single-board DDS solve engine but wraps
 * it in logic that solves the same deal/strain combination for all four
 * declarers, plus duplicate detection and chunk scheduling for batch workloads.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
 */

#ifndef DDS_CALCTABLES_H
#define DDS_CALCTABLES_H

#include <vector>

#include "dds.h"

using namespace std;


/** @brief Solve one table-work item for all four declarers on one worker. */
void CalcSingleCommon(
  const int thrID,
  const int bno);

/** @brief Copy one previously computed table result to duplicate boards. */
void CopyCalcSingle(
  const vector<int>& crossrefs);

/** @brief Worker loop for chunked/batched double-dummy table computation. */
void CalcChunkCommon(
  const int thrId);

/** @brief Detect batch entries whose table results can be reused exactly. */
void DetectCalcDuplicates(
  const boards& bds,
  vector<int>& uniques,
  vector<int>& crossrefs);

#endif
