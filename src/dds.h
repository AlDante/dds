/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/

#ifndef DDS_DDS_H
#define DDS_DDS_H

#include "../include/portab.h"
#include "../include/dll.h"


#if defined(DDS_MEMORY_LEAKS) && defined(_MSC_VER)
  #define DDS_MEMORY_LEAKS_WIN32
  #define _CRTDBG_MAP_ALLOC
  #include <crtdbg.h>
#endif


#define THREADMEM_SMALL_MAX_MB 30
#define THREADMEM_SMALL_DEF_MB 20
#define THREADMEM_LARGE_MAX_MB 160
#define THREADMEM_LARGE_DEF_MB 95

#define MAXNODE 1
#define MINNODE 0

#define SIMILARDEALLIMIT 5
#define SIMILARMAXWINNODES 700000

#define DDS_NOTRUMP 4

/* "hand" is leading hand, "relative" is hand relative leading
hand.
The handId macro implementation follows a solution
by Thomas Andrews.
All hand identities are given as
0=NORTH, 1=EAST, 2=SOUTH, 3=WEST. */

#define handId(hand, relative) (hand + relative) & 3


extern int lho[DDS_HANDS];
extern int rho[DDS_HANDS];
extern int partner[DDS_HANDS];

extern unsigned short int bitMapRank[16];

extern unsigned char cardRank[16];
extern unsigned char cardSuit[DDS_STRAINS];
extern unsigned char cardHand[DDS_HANDS];

// These five together take up 440 KB
extern int highestRank[8192];
extern int lowestRank[8192];
extern int counttable[8192];
extern char relRank[8192][15];
extern unsigned short int winRanks[8192][14];


/** @brief Grouped representation of consecutive rank runs within a 13-bit suit holding. */
struct moveGroupType
{
  /// Number of groups minus one (at most 7 groups in a 13-bit vector).
  int lastGroup;
  /// Highest rank in each group.
  int rank[7];
  /// Consecutive-rank sequence bitmap starting from each group's top rank.
  int sequence[7];
  /// Full sequence bitmap including cards below the gap.
  int fullseq[7];
  /// Gap (missing ranks) below each group.
  int gap[7];
};

extern moveGroupType groupData[8192];


/** @brief A single candidate move in suit/rank/sequence/weight form. */
struct moveType
{
  short suit;       ///< Suit index (0=spades .. 3=clubs).
  short rank;       ///< Rank as a bit position (2..14).
  short sequence;   ///< Non-zero if this move heads a sequence of equals.
  short weight;     ///< Sorting weight assigned by move-generation heuristics.
};

static_assert(sizeof(moveType) == 8, "moveType should remain compact");

struct movePlyType
{
  moveType move[14];
  int current;
  int last;
};

struct highCardType
{
  int rank;
  int hand;
};


/**
 * @brief Full search position state used by the alpha-beta engine.
 *
 * Contains the remaining cards for each hand/suit, aggregated suit holdings,
 * lengths, trick winners, and the search-tree bookkeeping arrays indexed by
 * depth (ply).
 */
struct pos
{
  unsigned short int rankInSuit[DDS_HANDS][DDS_SUITS]; ///< Remaining cards per hand/suit.
  unsigned short int aggr[DDS_SUITS];                  ///< OR of all hands' holdings per suit.
  unsigned char length[DDS_HANDS][DDS_SUITS];          ///< Card count per hand/suit.
  int handDist[DDS_HANDS];                             ///< Packed 4-suit length distribution.

  highCardType winner[DDS_SUITS];     ///< Current highest card in each suit.
  highCardType secondBest[DDS_SUITS]; ///< Second-highest card in each suit.
  int first[50];                      ///< Leading hand at each depth.
  moveType move[50];                  ///< Currently winning move at each depth.
  unsigned short int winRanks[50][DDS_SUITS]; ///< Winning rank-set by depth/suit.
  int tricksMAX;                      ///< Cumulative tricks won by the MAX side.
  int handRelFirst;                   ///< Current hand position relative to trick leader.
};


struct evalType
{
  int tricks;
  unsigned short int winRanks[DDS_SUITS];
};

struct card
{
  int suit;
  int rank;
};

struct extCard
{
  int suit;
  int rank;
  int sequence;
};

struct absRankType // 2 bytes
{
  char rank;
  signed char hand;
};

struct relRanksType // 120 bytes
{
  absRankType absRank[15][DDS_SUITS];
};

struct paramType
{
  int noOfBoards;
  boards * bop;
  solvedBoards * solvedp;
  int error;
};

enum RunMode
{
  DDS_RUN_SOLVE = 0,
  DDS_RUN_CALC = 1,
  DDS_RUN_TRACE = 2,
  DDS_RUN_SIZE = 3
};

#endif
