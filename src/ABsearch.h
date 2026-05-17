/**
 * @file ABsearch.h
 * @brief Recursive threshold-search entry points for the DDS proof engine.
 *
 * DDS does not recurse through a single generic minimax routine. Instead it
 * specializes the search by relative hand within the current trick, which keeps
 * the inner loop compact and avoids repeated interpretation of trick position.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
 */

#ifndef DDS_ABSEARCH_H
#define DDS_ABSEARCH_H

#include "dds.h"
#include "Memory.h"


/** @brief Dispatch to the specialized `ABsearch0..3()` routine for the current node. */
bool ABsearch(
  pos * posPoint,
  const int target,
  const int depth,
  ThreadData * thrp);

/** @brief Search specialization for the lead hand of the current trick. */
bool ABsearch0(
  pos * posPoint,
  const int target,
  const int depth,
  ThreadData * thrp);

/** @brief Search specialization for second hand to play in the trick. */
bool ABsearch1(
  pos * posPoint,
  const int target,
  const int depth,
  ThreadData * thrp);

/** @brief Search specialization for third hand to play in the trick. */
bool ABsearch2(
  pos * posPoint,
  const int target,
  const int depth,
  ThreadData * thrp);

/** @brief Search specialization for fourth hand to play in the trick. */
bool ABsearch3(
  pos * posPoint,
  const int target,
  const int depth,
  ThreadData * thrp);

/** @brief Apply a lead-hand move to the recursive position state. */
void Make0(
  pos * posPoint,
  const int depth,
  moveType const * mply);

/** @brief Apply a second-hand move to the recursive position state. */
void Make1(
  pos * posPoint,
  const int depth,
  moveType const * mply);

/** @brief Apply a third-hand move to the recursive position state. */
void Make2(
  pos * posPoint,
  const int depth,
  moveType const * mply);

/** @brief Apply a fourth-hand move, resolve the trick, and advance the next leader. */
void Make3(
  pos * posPoint,
  unsigned short trickCards[DDS_SUITS],
  const int depth,
  moveType const * mply,
  ThreadData * thrp);

/** @brief Evaluate the current node when DDS can stop recursing exactly. */
evalType Evaluate(
  pos const * posPoint,
  const int trump,
  ThreadData const * thrp);

#endif
