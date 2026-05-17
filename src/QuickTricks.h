/**
 * @file QuickTricks.h
 * @brief Immediate winner/proof helpers used for fast DDS cutoffs.
 *
 * The quick-trick routines answer whether the side to move can cash enough
 * immediate winners to settle the threshold without deeper search.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
 */

#ifndef DDS_QUICKTRICKS_H
#define DDS_QUICKTRICKS_H

#include "dds.h"
#include "Memory.h"


/** @brief Count/prove immediately cashable tricks for the side to move. */
int QuickTricks(
  pos& tpos,
  const int hand,
  const int depth,
  const int target,
  const int trump,
  bool& result,
  const ThreadData& thrd);

/** @brief Specialized quick-trick probe for the second-hand-in-trick case. */
bool QuickTricksSecondHand(
  pos& tpos,
  const int hand,
  const int depth,
  const int target,
  const int trump,
  const ThreadData& thrd);

#endif
