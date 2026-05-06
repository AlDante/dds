/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
 */

#include <cstring>

#include "TransTable.h"
#include "Moves.h"
#include "QuickTricks.h"
#include "LaterTricks.h"
#include "ABsearch.h"
#include "ABstats.h"
#include "TimerList.h"
#include "dump.h"
#include "debug.h"

#ifdef DDS_TARGET_APPLE_M1_MAX

void Undo0(
  pos * posPoint,
  const int depth,
  const moveType& mply,
  ThreadData const * thrp);

void Undo1(
  pos * posPoint,
  const int depth,
  const moveType& mply);

void Undo2(
  pos * posPoint,
  const int depth,
  const moveType& mply);

void Undo3(
  pos * posPoint,
  const int depth,
  const moveType& mply);

namespace
{
  const int DDSM1HandDelta[DDS_SUITS] = { 256, 16, 1, 0 };

  inline __attribute__((always_inline)) void DDSM1RestoreCard(
    pos * posPoint,
    const int hand,
    const moveType& mply)
  {
    const int suit = mply.suit;
    const unsigned short rankMask = bitMapRank[mply.rank];

    posPoint->rankInSuit[hand][suit] = static_cast<unsigned short>(
      posPoint->rankInSuit[hand][suit] | rankMask);
    posPoint->aggr[suit] = static_cast<unsigned short>(
      posPoint->aggr[suit] | rankMask);
    posPoint->handDist[hand] += DDSM1HandDelta[suit];
    posPoint->length[hand][suit]++;
  }

  inline __attribute__((always_inline)) void DDSM1Undo0(
    pos * posPoint,
    ThreadData * thrp,
    const int depth,
    const moveType& mply)
  {
    DDSM1RestoreCard(posPoint, handId(posPoint->first[depth], 3), mply);

    WinnersType const * wp = &thrp->winners[(depth + 3) >> 2];
    for (int n = 0; n < wp->number; n++)
    {
      const WinnerEntryType& entry = wp->winner[n];
      const int suit = entry.suit;
      posPoint->winner[suit].rank = entry.winnerRank;
      posPoint->winner[suit].hand = entry.winnerHand;
      posPoint->secondBest[suit].rank = entry.secondRank;
      posPoint->secondBest[suit].hand = entry.secondHand;
    }
  }

  inline __attribute__((always_inline)) void DDSM1Undo1(
    pos * posPoint,
    const int depth,
    const moveType& mply)
  {
    DDSM1RestoreCard(posPoint, posPoint->first[depth], mply);
  }

  inline __attribute__((always_inline)) void DDSM1Undo2(
    pos * posPoint,
    const int depth,
    const moveType& mply)
  {
    DDSM1RestoreCard(posPoint, handId(posPoint->first[depth], 1), mply);
  }

  inline __attribute__((always_inline)) void DDSM1Undo3(
    pos * posPoint,
    const int depth,
    const moveType& mply)
  {
    DDSM1RestoreCard(posPoint, handId(posPoint->first[depth], 2), mply);
  }

  inline bool DDSM1Unlikely(const bool value)
  {
    return __builtin_expect(value ? 1 : 0, 0);
  }

  inline void DDSM1ZeroLowestWin(
    ThreadData * thrp,
    const int depth)
  {
    thrp->lowestWin[depth][0] = 0;
    thrp->lowestWin[depth][1] = 0;
    thrp->lowestWin[depth][2] = 0;
    thrp->lowestWin[depth][3] = 0;
  }

  inline void DDSM1ZeroWinRanks(
    pos * posPoint,
    const int depth)
  {
    posPoint->winRanks[depth][0] = 0;
    posPoint->winRanks[depth][1] = 0;
    posPoint->winRanks[depth][2] = 0;
    posPoint->winRanks[depth][3] = 0;
  }

  inline void DDSM1CopyChildWinRanks(
    pos * posPoint,
    const int depth)
  {
    posPoint->winRanks[depth][0] = posPoint->winRanks[depth - 1][0];
    posPoint->winRanks[depth][1] = posPoint->winRanks[depth - 1][1];
    posPoint->winRanks[depth][2] = posPoint->winRanks[depth - 1][2];
    posPoint->winRanks[depth][3] = posPoint->winRanks[depth - 1][3];
  }

  inline void DDSM1OrChildWinRanks(
    pos * posPoint,
    const int depth)
  {
    posPoint->winRanks[depth][0] = static_cast<unsigned short>(
      posPoint->winRanks[depth][0] | posPoint->winRanks[depth - 1][0]);
    posPoint->winRanks[depth][1] = static_cast<unsigned short>(
      posPoint->winRanks[depth][1] | posPoint->winRanks[depth - 1][1]);
    posPoint->winRanks[depth][2] = static_cast<unsigned short>(
      posPoint->winRanks[depth][2] | posPoint->winRanks[depth - 1][2]);
    posPoint->winRanks[depth][3] = static_cast<unsigned short>(
      posPoint->winRanks[depth][3] | posPoint->winRanks[depth - 1][3]);
  }

  inline void DDSM1CopyMakeWinRanks(
    pos * posPoint,
    unsigned short makeWinRank[DDS_SUITS],
    const int depth)
  {
    posPoint->winRanks[depth][0] = static_cast<unsigned short>(
      posPoint->winRanks[depth - 1][0] | makeWinRank[0]);
    posPoint->winRanks[depth][1] = static_cast<unsigned short>(
      posPoint->winRanks[depth - 1][1] | makeWinRank[1]);
    posPoint->winRanks[depth][2] = static_cast<unsigned short>(
      posPoint->winRanks[depth - 1][2] | makeWinRank[2]);
    posPoint->winRanks[depth][3] = static_cast<unsigned short>(
      posPoint->winRanks[depth - 1][3] | makeWinRank[3]);
  }

  inline void DDSM1OrMakeWinRanks(
    pos * posPoint,
    unsigned short makeWinRank[DDS_SUITS],
    const int depth)
  {
    posPoint->winRanks[depth][0] = static_cast<unsigned short>(
      posPoint->winRanks[depth][0] |
      posPoint->winRanks[depth - 1][0] |
      makeWinRank[0]);
    posPoint->winRanks[depth][1] = static_cast<unsigned short>(
      posPoint->winRanks[depth][1] |
      posPoint->winRanks[depth - 1][1] |
      makeWinRank[1]);
    posPoint->winRanks[depth][2] = static_cast<unsigned short>(
      posPoint->winRanks[depth][2] |
      posPoint->winRanks[depth - 1][2] |
      makeWinRank[2]);
    posPoint->winRanks[depth][3] = static_cast<unsigned short>(
      posPoint->winRanks[depth][3] |
      posPoint->winRanks[depth - 1][3] |
      makeWinRank[3]);
  }

  inline void DDSM1PrefetchChild(
    pos * posPoint,
    ThreadData * thrp,
    const int childDepth)
  {
    __builtin_prefetch(&posPoint->winRanks[childDepth][0], 1, 3);
    __builtin_prefetch(&thrp->lowestWin[childDepth][0], 1, 3);
    __builtin_prefetch(&thrp->bestMove[childDepth], 1, 2);
    __builtin_prefetch(&thrp->bestMoveTT[childDepth], 1, 2);
  }
}


bool ABsearch(
  pos * posPoint,
  const int target,
  const int depth,
  ThreadData * thrp)
{
  int hand = posPoint->first[depth];
  int tricks = depth >> 2;
  bool success = (thrp->nodeTypeStore[hand] == MAXNODE ? true : false);
  bool value = ! success;

#ifdef DDS_TOP_LEVEL
  thrp->nodes++;
#endif

  TIMER_START(TIMER_NO_MOVEGEN, depth);
  DDSM1ZeroLowestWin(thrp, depth);

  thrp->moves.MoveGen0(
    tricks,
    * posPoint,
    thrp->bestMove[depth],
    thrp->bestMoveTT[depth],
    thrp->rel);
  thrp->moves.Purge(tricks, 0, thrp->forbiddenMoves);

  TIMER_END(TIMER_NO_MOVEGEN, depth);

  TIMER_START(TIMER_NO_AB_SETUP, depth);
  TIMER_START(TIMER_NO_AB_SETUP, depth);
  TIMER_START(TIMER_NO_AB_SETUP, depth);
  TIMER_START(TIMER_NO_AB_NODE_SETUP, depth);
  DDSM1ZeroWinRanks(posPoint, depth);
  TIMER_END(TIMER_NO_AB_NODE_SETUP, depth);
  TIMER_END(TIMER_NO_AB_SETUP, depth);
  TIMER_END(TIMER_NO_AB_SETUP, depth);
  TIMER_END(TIMER_NO_AB_SETUP, depth);

  while (1)
  {
    TIMER_START(TIMER_NO_MAKE, depth);
    moveType const * mply = thrp->moves.MakeNext(tricks, 0,
      posPoint->winRanks[depth]);
#ifdef DDS_AB_STATS
    thrp->ABStats.IncrNode(depth);
#endif
    TIMER_END(TIMER_NO_MAKE, depth);

    TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_LOOP_CONTROL, depth);
    if (mply == NULL)
    {
      TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);
      TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
      break;
    }
    TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);

    TIMER_START(TIMER_NO_AB_CHILDLOOP, depth);
    Make0(posPoint, depth, mply);
    TIMER_END(TIMER_NO_AB_CHILDLOOP, depth);

    TIMER_START(TIMER_NO_AB_RECURSE_SETUP, depth);
    DDSM1PrefetchChild(posPoint, thrp, depth - 1);
    TIMER_END(TIMER_NO_AB_RECURSE_SETUP, depth);
    TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB, depth - 1);
    value = ABsearch1(posPoint, target, depth - 1, thrp);
    TIMER_END(TIMER_NO_AB, depth - 1);

    TIMER_START(TIMER_NO_UNDO, depth);
    DDSM1Undo1(posPoint, depth, * mply);
    TIMER_END(TIMER_NO_UNDO, depth);

    TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_POST_CHILD, depth);
    bool cutoff = (value == success);
    TIMER_END(TIMER_NO_AB_POST_CHILD, depth);

    TIMER_START(TIMER_NO_AB_CUTOFF, depth);
    if (DDSM1Unlikely(cutoff))
    {
      DDSM1CopyChildWinRanks(posPoint, depth);

      thrp->bestMove[depth] = * mply;
#ifdef DDS_MOVES
      thrp->moves.RegisterHit(tricks, 0);
#endif
      TIMER_END(TIMER_NO_AB_CUTOFF, depth);
      goto ABexit;
    }
    TIMER_END(TIMER_NO_AB_CUTOFF, depth);

    TIMER_START(TIMER_NO_AB_CHILDLOOP, depth);
    DDSM1OrChildWinRanks(posPoint, depth);
    TIMER_END(TIMER_NO_AB_CHILDLOOP, depth);

    TIMER_START(TIMER_NO_NEXTMOVE, depth);
    TIMER_END(TIMER_NO_NEXTMOVE, depth);
    TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
  }

ABexit:
  TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
  TIMER_START(TIMER_NO_AB_LOOP_CONTROL, depth);
  AB_COUNT(AB_MOVE_LOOP, value, depth);
#ifdef DDS_AB_STATS
  thrp->ABStats.PrintStats(thrp->fileABstats.GetStream());
#endif
  TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);
  TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);

  return value;
}


bool ABsearch0(
  pos * posPoint,
  const int target,
  const int depth,
  ThreadData * thrp)
{
  int trump = thrp->trump;
  int hand = posPoint->first[depth];
  int tricks = depth >> 2;

#ifdef DDS_TOP_LEVEL
  thrp->nodes++;
#endif

  TIMER_START(TIMER_NO_AB_SETUP, depth);
  DDSM1ZeroWinRanks(posPoint, depth);
  TIMER_END(TIMER_NO_AB_SETUP, depth);

  if (depth >= 20)
  {
    TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_TT_PREP, depth);
    int limit;
    if (thrp->nodeTypeStore[0] == MAXNODE)
      limit = target - posPoint->tricksMAX - 1;
    else
      limit = tricks - (target - posPoint->tricksMAX - 1);
    TIMER_END(TIMER_NO_AB_TT_PREP, depth);
    TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);

    bool lowerFlag;
    TIMER_START(TIMER_NO_LOOKUP, depth);
    nodeCardsType const * cardsP =
      thrp->transTable->Lookup(
        tricks, hand, posPoint->aggr, posPoint->handDist,
        limit, lowerFlag);
    TIMER_END(TIMER_NO_LOOKUP, depth);

    if (cardsP)
    {
      TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
      TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
      TIMER_START(TIMER_NO_AB_TERMINAL, depth);
#ifdef DDS_AB_HITS
      DumpRetrieved(thrp->fileRetrieved.GetStream(),
        * posPoint, cardsP, target, depth);
#endif

      for (int ss = 0; ss < DDS_SUITS; ss++)
        posPoint->winRanks[depth][ss] =
          winRanks[ posPoint->aggr[ss] ]
          [ static_cast<int>(cardsP->leastWin[ss]) ];

      if (cardsP->bestMoveRank != 0)
      {
        thrp->bestMoveTT[depth].suit = cardsP->bestMoveSuit;
        thrp->bestMoveTT[depth].rank = cardsP->bestMoveRank;
      }

      TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
      TIMER_START(TIMER_NO_AB_TT_PREP, depth);
      bool scoreFlag =
        (thrp->nodeTypeStore[0] == MAXNODE ? lowerFlag : ! lowerFlag);
      TIMER_END(TIMER_NO_AB_TT_PREP, depth);
      TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);

      AB_COUNT(AB_MAIN_LOOKUP, scoreFlag, depth);
      TIMER_END(TIMER_NO_AB_TERMINAL, depth);
      return scoreFlag;
    }
  }

  TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
  if (posPoint->tricksMAX >= target)
  {
    TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_TERMINAL, depth);
    AB_COUNT(AB_TARGET_REACHED, true, depth);
    TIMER_END(TIMER_NO_AB_TERMINAL, depth);
    return true;
  }
  else if (posPoint->tricksMAX + tricks + 1 < target)
  {
    TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_TERMINAL, depth);
    AB_COUNT(AB_TARGET_REACHED, false, depth);
    TIMER_END(TIMER_NO_AB_TERMINAL, depth);
    return false;
  }
  else if (depth == 0)
  {
    TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    TIMER_START(TIMER_NO_EVALUATE, depth);
    evalType evalData = Evaluate(posPoint, trump, thrp);
    TIMER_END(TIMER_NO_EVALUATE, depth);

    bool value = (evalData.tricks >= target ? true : false);

    posPoint->winRanks[depth][0] = evalData.winRanks[0];
    posPoint->winRanks[depth][1] = evalData.winRanks[1];
    posPoint->winRanks[depth][2] = evalData.winRanks[2];
    posPoint->winRanks[depth][3] = evalData.winRanks[3];

    TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_TERMINAL, depth);
    AB_COUNT(AB_DEPTH_ZERO, value, depth);
    TIMER_END(TIMER_NO_AB_TERMINAL, depth);
    return value;
  }
  TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);

  bool res;
  TIMER_START(TIMER_NO_QT, depth);
  int qtricks = QuickTricks(* posPoint, hand, depth, target,
                            trump, res, * thrp);
  TIMER_END(TIMER_NO_QT, depth);

  if (thrp->nodeTypeStore[hand] == MAXNODE)
  {
    TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    if (DDSM1Unlikely(res))
    {
      TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
      TIMER_START(TIMER_NO_AB_TERMINAL, depth);
      AB_COUNT(AB_QUICKTRICKS, 1, depth);
      TIMER_END(TIMER_NO_AB_TERMINAL, depth);
      return (qtricks == 0 ? false : true);
    }
    TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);

    TIMER_START(TIMER_NO_LT, depth);
    res = LaterTricksMIN(* posPoint, hand, depth, target, trump, * thrp);
    TIMER_END(TIMER_NO_LT, depth);

    TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    if (! res)
    {
      TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
      TIMER_START(TIMER_NO_AB_TERMINAL, depth);
      AB_COUNT(AB_LATERTRICKS, true, depth);
      TIMER_END(TIMER_NO_AB_TERMINAL, depth);
      return false;
    }
    TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
  }
  else
  {
    TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    if (DDSM1Unlikely(res))
    {
      TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
      TIMER_START(TIMER_NO_AB_TERMINAL, depth);
      AB_COUNT(AB_QUICKTRICKS, false, depth);
      TIMER_END(TIMER_NO_AB_TERMINAL, depth);
      return (qtricks == 0 ? true : false);
    }
    TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);

    TIMER_START(TIMER_NO_LT, depth);
    res = LaterTricksMAX(* posPoint, hand, depth, target, trump, * thrp);
    TIMER_END(TIMER_NO_LT, depth);

    TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    if (res)
    {
      TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
      TIMER_START(TIMER_NO_AB_TERMINAL, depth);
      AB_COUNT(AB_LATERTRICKS, false, depth);
      TIMER_END(TIMER_NO_AB_TERMINAL, depth);
      return true;
    }
    TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
  }

  if (depth < 20)
  {
    TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_TT_PREP, depth);
    int limit;
    if (thrp->nodeTypeStore[0] == MAXNODE)
      limit = target - posPoint->tricksMAX - 1;
    else
      limit = tricks - (target - posPoint->tricksMAX - 1);
    TIMER_END(TIMER_NO_AB_TT_PREP, depth);
    TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);

    bool lowerFlag;
    TIMER_START(TIMER_NO_LOOKUP, depth);
    nodeCardsType const * cardsP =
      thrp->transTable->Lookup(
        tricks, hand, posPoint->aggr, posPoint->handDist,
        limit, lowerFlag);
    TIMER_END(TIMER_NO_LOOKUP, depth);

    if (cardsP)
    {
      TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
      TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
      TIMER_START(TIMER_NO_AB_TERMINAL, depth);
#ifdef DDS_AB_HITS
      DumpRetrieved(thrp->fileRetrieved.GetStream(),
        * posPoint, * cardsP, target, depth);
#endif

      for (int ss = 0; ss < DDS_SUITS; ss++)
        posPoint->winRanks[depth][ss] =
          winRanks[ posPoint->aggr[ss] ]
          [ static_cast<int>(cardsP->leastWin[ss]) ];

      if (cardsP->bestMoveRank != 0)
      {
        thrp->bestMoveTT[depth].suit = cardsP->bestMoveSuit;
        thrp->bestMoveTT[depth].rank = cardsP->bestMoveRank;
      }

      TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
      TIMER_START(TIMER_NO_AB_TT_PREP, depth);
      bool scoreFlag =
        (thrp->nodeTypeStore[0] == MAXNODE ? lowerFlag : ! lowerFlag);
      TIMER_END(TIMER_NO_AB_TT_PREP, depth);
      TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);

      AB_COUNT(AB_MAIN_LOOKUP, scoreFlag, depth);
      TIMER_END(TIMER_NO_AB_TERMINAL, depth);
      return scoreFlag;
    }
  }

  TIMER_START(TIMER_NO_AB_SETUP, depth);
  bool success = (thrp->nodeTypeStore[hand] == MAXNODE ? true : false);
  bool value = ! success;
  TIMER_END(TIMER_NO_AB_SETUP, depth);

  TIMER_START(TIMER_NO_MOVEGEN, depth);
  DDSM1ZeroLowestWin(thrp, depth);

  thrp->moves.MoveGen0(
    tricks,
    * posPoint,
    thrp->bestMove[depth],
    thrp->bestMoveTT[depth],
    thrp->rel);

  TIMER_END(TIMER_NO_MOVEGEN, depth);

  TIMER_START(TIMER_NO_AB_SETUP, depth);
  TIMER_START(TIMER_NO_AB_NODE_SETUP, depth);
  DDSM1ZeroWinRanks(posPoint, depth);
  TIMER_END(TIMER_NO_AB_NODE_SETUP, depth);
  TIMER_END(TIMER_NO_AB_SETUP, depth);

  while (1)
  {
    TIMER_START(TIMER_NO_MAKE, depth);
    moveType const * mply = thrp->moves.MakeNext(tricks, 0,
      posPoint->winRanks[depth]);
#ifdef DDS_AB_STATS
    thrp->ABStats.IncrNode(depth);
#endif
    TIMER_END(TIMER_NO_MAKE, depth);

    TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_LOOP_CONTROL, depth);
    if (mply == NULL)
    {
      TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);
      TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
      break;
    }
    TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);

    TIMER_START(TIMER_NO_AB_CHILDLOOP, depth);
    Make0(posPoint, depth, mply);
    TIMER_END(TIMER_NO_AB_CHILDLOOP, depth);

    TIMER_START(TIMER_NO_AB_RECURSE_SETUP, depth);
    DDSM1PrefetchChild(posPoint, thrp, depth - 1);
    TIMER_END(TIMER_NO_AB_RECURSE_SETUP, depth);
    TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB, depth - 1);
    value = ABsearch1(posPoint, target, depth - 1, thrp);
    TIMER_END(TIMER_NO_AB, depth - 1);

    TIMER_START(TIMER_NO_UNDO, depth);
    Undo1(posPoint, depth, * mply);
    TIMER_END(TIMER_NO_UNDO, depth);

    TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_POST_CHILD, depth);
    bool cutoff = (value == success);
    TIMER_END(TIMER_NO_AB_POST_CHILD, depth);

    TIMER_START(TIMER_NO_AB_CUTOFF, depth);
    if (DDSM1Unlikely(cutoff))
    {
      DDSM1CopyChildWinRanks(posPoint, depth);

      thrp->bestMove[depth] = * mply;
#ifdef DDS_MOVES
      thrp->moves.RegisterHit(tricks, 0);
#endif
      TIMER_END(TIMER_NO_AB_CUTOFF, depth);
      goto ABexit;
    }
    TIMER_END(TIMER_NO_AB_CUTOFF, depth);

    TIMER_START(TIMER_NO_AB_CHILDLOOP, depth);
    DDSM1OrChildWinRanks(posPoint, depth);
    TIMER_END(TIMER_NO_AB_CHILDLOOP, depth);

    TIMER_START(TIMER_NO_NEXTMOVE, depth);
    TIMER_END(TIMER_NO_NEXTMOVE, depth);
    TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
  }

ABexit:
  TIMER_START(TIMER_NO_AB_STORE_PREP, depth);
  TIMER_START(TIMER_NO_AB_TT_PREP, depth);
  nodeCardsType first;
  if (value)
  {
    if (thrp->nodeTypeStore[0] == MAXNODE)
    {
      first.ubound = static_cast<char>(tricks + 1);
      first.lbound = static_cast<char>(target - posPoint->tricksMAX);
    }
    else
    {
      first.ubound = static_cast<char>
                     (tricks + 1 - target + posPoint->tricksMAX);
      first.lbound = 0;
    }
  }
  else
  {
    if (thrp->nodeTypeStore[0] == MAXNODE)
    {
      first.ubound = static_cast<char>
                     (target - posPoint->tricksMAX - 1);
      first.lbound = 0;
    }
    else
    {
      first.ubound = static_cast<char>(tricks + 1);
      first.lbound = static_cast<char>
                     (tricks + 1 - target + posPoint->tricksMAX + 1);
    }
  }

  first.bestMoveSuit = static_cast<char>(thrp->bestMove[depth].suit);
  first.bestMoveRank = static_cast<char>(thrp->bestMove[depth].rank);

  bool flag =
    ((thrp->nodeTypeStore[hand] == MAXNODE && value) ||
     (thrp->nodeTypeStore[hand] == MINNODE && !value))
    ? true : false;
  TIMER_END(TIMER_NO_AB_TT_PREP, depth);
  TIMER_END(TIMER_NO_AB_STORE_PREP, depth);

  TIMER_START(TIMER_NO_BUILD, depth);
  thrp->transTable->Add(
    tricks,
    hand,
    posPoint->aggr,
    posPoint->winRanks[depth],
    first,
    flag);
  TIMER_END(TIMER_NO_BUILD, depth);

#ifdef DDS_AB_HITS
  DumpStored(thrp->fileStored.GetStream(),
    * posPoint, thrp->moves, first, target, depth);
#endif

  TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
  TIMER_START(TIMER_NO_AB_LOOP_CONTROL, depth);
  AB_COUNT(AB_MOVE_LOOP, value, depth);
  TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);
  TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
  return value;
}


bool ABsearch1(
  pos * posPoint,
  const int target,
  const int depth,
  ThreadData * thrp)
{
  int trump = thrp->trump;
  int hand = handId(posPoint->first[depth], 1);
  bool success = (thrp->nodeTypeStore[hand] == MAXNODE ? true : false);
  bool value = ! success;
  int tricks = (depth + 3) >> 2;

#ifdef DDS_TOP_LEVEL
  thrp->nodes++;
#endif

  TIMER_START(TIMER_NO_QT, depth);
  int res = QuickTricksSecondHand(* posPoint, hand, depth, target,
     trump, * thrp);
  TIMER_END(TIMER_NO_QT, depth);
  TIMER_START(TIMER_NO_AB_TERMINAL_CONTROL, depth);
  if (DDSM1Unlikely(res))
  {
    TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_TERMINAL, depth);
    AB_COUNT(AB_QUICKTRICKS_2ND, true, depth);
    TIMER_END(TIMER_NO_AB_TERMINAL, depth);
    return success;
  }
  TIMER_END(TIMER_NO_AB_TERMINAL_CONTROL, depth);

  TIMER_START(TIMER_NO_MOVEGEN, depth);
  DDSM1ZeroLowestWin(thrp, depth);

  thrp->moves.MoveGen123(tricks, 1, * posPoint);
  if (depth == thrp->iniDepth)
    thrp->moves.Purge(tricks, 1, thrp->forbiddenMoves);

  TIMER_END(TIMER_NO_MOVEGEN, depth);

  TIMER_START(TIMER_NO_AB_SETUP, depth);
  TIMER_START(TIMER_NO_AB_NODE_SETUP, depth);
  DDSM1ZeroWinRanks(posPoint, depth);
  TIMER_END(TIMER_NO_AB_NODE_SETUP, depth);
  TIMER_END(TIMER_NO_AB_SETUP, depth);

  while (1)
  {
    TIMER_START(TIMER_NO_MAKE, depth);
    moveType const * mply = thrp->moves.MakeNext(tricks, 1,
      posPoint->winRanks[depth]);
#ifdef DDS_AB_STATS
    thrp->ABStats.IncrNode(depth);
#endif
    TIMER_END(TIMER_NO_MAKE, depth);

    TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_LOOP_CONTROL, depth);
    if (mply == NULL)
    {
      TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);
      TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
      break;
    }
    TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);

    TIMER_START(TIMER_NO_AB_CHILDLOOP, depth);
    Make1(posPoint, depth, mply);
    TIMER_END(TIMER_NO_AB_CHILDLOOP, depth);

    TIMER_START(TIMER_NO_AB_RECURSE_SETUP, depth);
    DDSM1PrefetchChild(posPoint, thrp, depth - 1);
    TIMER_END(TIMER_NO_AB_RECURSE_SETUP, depth);
    TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB, depth - 1);
    value = ABsearch2(posPoint, target, depth - 1, thrp);
    TIMER_END(TIMER_NO_AB, depth - 1);

    TIMER_START(TIMER_NO_UNDO, depth);
    DDSM1Undo2(posPoint, depth, * mply);
    TIMER_END(TIMER_NO_UNDO, depth);

    TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_POST_CHILD, depth);
    bool cutoff = (value == success);
    TIMER_END(TIMER_NO_AB_POST_CHILD, depth);

    TIMER_START(TIMER_NO_AB_CUTOFF, depth);
    if (DDSM1Unlikely(cutoff))
    {
      DDSM1CopyChildWinRanks(posPoint, depth);

      thrp->bestMove[depth] = * mply;
#ifdef DDS_MOVES
      thrp->moves.RegisterHit(tricks, 1);
#endif
      TIMER_END(TIMER_NO_AB_CUTOFF, depth);
      goto ABexit;
    }
    TIMER_END(TIMER_NO_AB_CUTOFF, depth);

    TIMER_START(TIMER_NO_AB_CHILDLOOP, depth);
    DDSM1OrChildWinRanks(posPoint, depth);
    TIMER_END(TIMER_NO_AB_CHILDLOOP, depth);

    TIMER_START(TIMER_NO_NEXTMOVE, depth);
    TIMER_END(TIMER_NO_NEXTMOVE, depth);
    TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
  }

ABexit:
  TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
  TIMER_START(TIMER_NO_AB_LOOP_CONTROL, depth);
  AB_COUNT(AB_MOVE_LOOP, value, depth);
  TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);
  TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
  return value;
}


bool ABsearch2(
  pos * posPoint,
  const int target,
  const int depth,
  ThreadData * thrp)
{
  int hand = handId(posPoint->first[depth], 2);
  bool success = (thrp->nodeTypeStore[hand] == MAXNODE ? true : false);
  bool value = ! success;
  int tricks = (depth + 3) >> 2;

#ifdef DDS_TOP_LEVEL
  thrp->nodes++;
#endif

  TIMER_START(TIMER_NO_MOVEGEN, depth);
  DDSM1ZeroLowestWin(thrp, depth);

  thrp->moves.MoveGen123(tricks, 2, * posPoint);
  if (depth == thrp->iniDepth)
    thrp->moves.Purge(tricks, 2, thrp->forbiddenMoves);

  TIMER_END(TIMER_NO_MOVEGEN, depth);

  TIMER_START(TIMER_NO_AB_NODE_SETUP, depth);
  DDSM1ZeroWinRanks(posPoint, depth);
  TIMER_END(TIMER_NO_AB_NODE_SETUP, depth);

  while (1)
  {
    TIMER_START(TIMER_NO_MAKE, depth);
    moveType const * mply = thrp->moves.MakeNext(tricks, 2,
      posPoint->winRanks[depth]);

    TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_LOOP_CONTROL, depth);
    if (mply == NULL)
    {
      TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);
      TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
      break;
    }
    TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);

    TIMER_START(TIMER_NO_AB_CHILDLOOP, depth);
    Make2(posPoint, depth, mply);
    TIMER_END(TIMER_NO_AB_CHILDLOOP, depth);

#ifdef DDS_AB_STATS
    thrp->ABStats.IncrNode(depth);
#endif
    TIMER_END(TIMER_NO_MAKE, depth);
    TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);

    TIMER_START(TIMER_NO_AB_RECURSE_SETUP, depth);
    DDSM1PrefetchChild(posPoint, thrp, depth - 1);
    TIMER_END(TIMER_NO_AB_RECURSE_SETUP, depth);
    TIMER_START(TIMER_NO_AB, depth - 1);
    value = ABsearch3(posPoint, target, depth - 1, thrp);
    TIMER_END(TIMER_NO_AB, depth - 1);

    TIMER_START(TIMER_NO_UNDO, depth);
    DDSM1Undo3(posPoint, depth, * mply);
    TIMER_END(TIMER_NO_UNDO, depth);

    TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_POST_CHILD, depth);
    bool cutoff = (value == success);
    TIMER_END(TIMER_NO_AB_POST_CHILD, depth);

    TIMER_START(TIMER_NO_AB_CUTOFF, depth);
    if (DDSM1Unlikely(cutoff))
    {
      DDSM1CopyChildWinRanks(posPoint, depth);

      thrp->bestMove[depth] = * mply;
#ifdef DDS_MOVES
      thrp->moves.RegisterHit(tricks, 2);
#endif
      TIMER_END(TIMER_NO_AB_CUTOFF, depth);
      goto ABexit;
    }
    TIMER_END(TIMER_NO_AB_CUTOFF, depth);

    TIMER_START(TIMER_NO_AB_CHILDLOOP, depth);
    DDSM1OrChildWinRanks(posPoint, depth);
    TIMER_END(TIMER_NO_AB_CHILDLOOP, depth);

    TIMER_START(TIMER_NO_NEXTMOVE, depth);
    TIMER_END(TIMER_NO_NEXTMOVE, depth);
    TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
  }

ABexit:
  TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
  TIMER_START(TIMER_NO_AB_LOOP_CONTROL, depth);
  AB_COUNT(AB_MOVE_LOOP, value, depth);
  TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);
  TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
  return value;
}


bool ABsearch3(
  pos * posPoint,
  const int target,
  const int depth,
  ThreadData * thrp)
{
  unsigned short int makeWinRank[DDS_SUITS];

  int hand = handId(posPoint->first[depth], 3);
  bool success = (thrp->nodeTypeStore[hand] == MAXNODE ? true : false);
  bool value = ! success;

#ifdef DDS_TOP_LEVEL
  thrp->nodes++;
#endif

  TIMER_START(TIMER_NO_MOVEGEN, depth);
  DDSM1ZeroLowestWin(thrp, depth);
  int tricks = (depth + 3) >> 2;

  thrp->moves.MoveGen123(tricks, 3, * posPoint);
  if (depth == thrp->iniDepth)
    thrp->moves.Purge(tricks, 3, thrp->forbiddenMoves);

  TIMER_END(TIMER_NO_MOVEGEN, depth);

  TIMER_START(TIMER_NO_AB_NODE_SETUP, depth);
  DDSM1ZeroWinRanks(posPoint, depth);
  TIMER_END(TIMER_NO_AB_NODE_SETUP, depth);

  while (1)
  {
    TIMER_START(TIMER_NO_MAKE, depth);
    moveType const * mply = thrp->moves.MakeNext(tricks, 3,
      posPoint->winRanks[depth]);
#ifdef DDS_AB_STATS
    thrp->ABStats.IncrNode(depth);
#endif
    TIMER_END(TIMER_NO_MAKE, depth);

    TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_LOOP_CONTROL, depth);
    if (mply == NULL)
    {
      TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);
      TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
      break;
    }
    TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);

    TIMER_START(TIMER_NO_AB_CHILDLOOP, depth);
    Make3(posPoint, makeWinRank, depth, mply, thrp);
    TIMER_END(TIMER_NO_AB_CHILDLOOP, depth);

    TIMER_START(TIMER_NO_AB_RECURSE_SETUP, depth);
    thrp->trickNodes++;

    if (thrp->nodeTypeStore[posPoint->first[depth - 1]] == MAXNODE)
      posPoint->tricksMAX++;

    DDSM1PrefetchChild(posPoint, thrp, depth - 1);
    TIMER_END(TIMER_NO_AB_RECURSE_SETUP, depth);
    TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB, depth - 1);
    value = ABsearch0(posPoint, target, depth - 1, thrp);
    TIMER_END(TIMER_NO_AB, depth - 1);

    TIMER_START(TIMER_NO_UNDO, depth);
    DDSM1Undo0(posPoint, thrp, depth, * mply);

    if (thrp->nodeTypeStore[posPoint->first[depth - 1]] == MAXNODE)
      posPoint->tricksMAX--;

    TIMER_END(TIMER_NO_UNDO, depth);

    TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
    TIMER_START(TIMER_NO_AB_POST_CHILD, depth);
    bool cutoff = (value == success);
    TIMER_END(TIMER_NO_AB_POST_CHILD, depth);

    TIMER_START(TIMER_NO_AB_CUTOFF, depth);
    if (DDSM1Unlikely(cutoff))
    {
      DDSM1CopyMakeWinRanks(posPoint, makeWinRank, depth);

      thrp->bestMove[depth] = * mply;
#ifdef DDS_MOVES
      thrp->moves.RegisterHit(tricks, 3);
#endif
      TIMER_END(TIMER_NO_AB_CUTOFF, depth);
      goto ABexit;
    }
    TIMER_END(TIMER_NO_AB_CUTOFF, depth);

    TIMER_START(TIMER_NO_AB_CHILDLOOP, depth);
    DDSM1OrMakeWinRanks(posPoint, makeWinRank, depth);
    TIMER_END(TIMER_NO_AB_CHILDLOOP, depth);

    TIMER_START(TIMER_NO_NEXTMOVE, depth);
    TIMER_END(TIMER_NO_NEXTMOVE, depth);
    TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
  }

ABexit:
  TIMER_START(TIMER_NO_AB_ITERATION_CONTROL, depth);
  TIMER_START(TIMER_NO_AB_LOOP_CONTROL, depth);
  AB_COUNT(AB_MOVE_LOOP, value, depth);
  TIMER_END(TIMER_NO_AB_LOOP_CONTROL, depth);
  TIMER_END(TIMER_NO_AB_ITERATION_CONTROL, depth);
  return value;
}

#endif

