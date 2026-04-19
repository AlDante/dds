/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/

#ifndef DDS_QUICKTRICKS_H
#define DDS_QUICKTRICKS_H

#include "dds.h"
#include "Memory.h"


/**
 * @brief Shared context for QuickTricks sub-functions (§9.1).
 *
 * Collects the 13-16 parameters that were previously passed individually
 * to QtricksLeadHandTrump, QtricksLeadHandNT, QuickTricksPartnerHandTrump,
 * and QuickTricksPartnerHandNT into a single cache-line-friendly struct
 * passed by reference.  On ARM64 this eliminates stack spills since only
 * a single pointer register is needed instead of 13+ register/stack slots.
 */
struct QtricksContext
{
  pos& tpos;
  const ThreadData& thrd;
  const int hand;
  const int depth;
  const int cutoff;
  const int trump;
  int suit;
  int qtricks;
  int countOwn;
  int countLho;
  int countRho;
  int countPart;
  int lhoTrumpRanks;
  int rhoTrumpRanks;
  int commSuit;
  int commRank;
  bool commPartner;
};


/**
 * @brief Return value from QuickTricks sub-functions (§9.6).
 *
 * Replaces the old int& res output parameter.  On ARM64 both fields
 * are returned in registers (x0, x1), eliminating the store-to-load
 * forwarding penalty of the old approach.
 */
struct QtricksResult
{
  int qtricks;  ///< Updated quick-trick count.
  int action;   ///< 0 = continue same suit, 1 = cutoff, 2 = next suit.
};


int QuickTricks(
  pos& tpos,
  const int hand,
  const int depth,
  const int target,
  const int trump,
  bool& result,
  const ThreadData& thrd);

bool QuickTricksSecondHand(
  pos& tpos,
  const int hand,
  const int depth,
  const int target,
  const int trump,
  const ThreadData& thrd);

#endif
