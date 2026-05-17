/**
 * @file LaterTricks.h
 * @brief Deeper bridge-specific proof helpers used by DDS pruning.
 *
 * These routines reason about whether the MAX or MIN side can still secure the
 * remaining target once the position is beyond the immediate quick-trick cases.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
 */

#ifndef DDS_LATERTRICKS_H
#define DDS_LATERTRICKS_H

#include "dds.h"
#include "Memory.h"


/** @brief Prove/refute a target from the minimizing side's perspective. */
bool LaterTricksMIN(
  pos& tpos,
  const int hand,
  const int depth,
  const int target,
  const int trump,
  const ThreadData& thrd);

/** @brief Prove/refute a target from the maximizing side's perspective. */
bool LaterTricksMAX(
  pos& tpos,
  const int hand,
  const int depth,
  const int target,
  const int trump,
  const ThreadData& thrd);

#endif
