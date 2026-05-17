/**
 * @file SolverIF.h
 * @brief Root DDS solve orchestration helpers used by the public API layer.
 *
 * These functions live above the recursive search core. They validate and stage
 * deals, seed thread-local state, drive repeated threshold probes, and convert
 * internal results into the public `futureTricks` format.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
 */

#ifndef DDS_SOLVERIF_H
#define DDS_SOLVERIF_H

#include "dds.h"
#include "Memory.h"


/** @brief Core implementation behind `SolveBoard()` and `SolveBoardPBN()`. */
int SolveBoardInternal(
  ThreadData * thrp,
  const deal& dl,
  const int target,
  const int solutions,
  const int mode,
  futureTricks * futp);

/** @brief Fast-path companion used when a new query reuses the same deal shell. */
int SolveSameBoard(
  ThreadData * thrp,
  const deal& dl,
  futureTricks * futp,
  const int hint);

/** @brief Re-analyse the position after applying one lead-card candidate. */
int AnalyseLaterBoard(
  ThreadData * thrp,
  const int leadHand,
  moveType const * move,
  const int hint,
  const int hintDir,
  futureTricks * futp);

#endif
