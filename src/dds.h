/**
 * @file dds.h
 * @brief Core internal DDS search-state types and global lookup tables.
 *
 * This header is the center of the perfect-information solver's in-memory model.
 * It defines the recursive position object, move records, rank-lookup tables,
 * and a handful of enums shared across search, move generation, scheduling, and
 * orchestration.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
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


// Thread memory configuration moved to TTConfig.h
// Override at compile time (-D flags) or runtime (env vars).
#include "TTConfig.h"

/** @brief Logical search side at a DDS node. */
enum NodeType
{
  MINNODE = 0,
  MAXNODE = 1
};

#define SIMILARDEALLIMIT 5
#define SIMILARMAXWINNODES 700000

#define DDS_NOTRUMP 4

/* "hand" is leading hand, "relative" is hand relative leading
hand.
The handId macro implementation follows a solution
by Thomas Andrews.
All hand identities are given as
0=NORTH, 1=EAST, 2=SOUTH, 3=WEST. */

/**
 * @brief Convert a leader-relative hand index back to the absolute seat id.
 *
 * DDS specializes heavily by relative seat within the current trick, so this
 * helper appears throughout move generation and search.
 */
inline constexpr int handId(
  const int hand,
  const int relative)
{
  return (hand + relative) & 3;
}


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


/**
 * @brief Precomputed decomposition of a 13-bit suit holding into rank runs.
 *
 * DDS uses these groups to reason quickly about touching honors, sequences, and
 * equivalent-card compression during move generation.
 */
struct moveGroupType
{
  // There are at most 7 groups of bit "runs" in a 13-bit vector
  int lastGroup;
  int rank[7];
  int sequence[7];
  int fullseq[7];
  int gap[7];
};

extern moveGroupType groupData[8192];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"

// Problematic struct code here...

/**
 * @brief Compact representation of one candidate card plus ordering metadata.
 *
 * The union layout preserves a packed 64-bit representation so specialized sort
 * and compare paths can operate on one machine word.
 */
// moveType is defined as a union to allow use of M1 compare and sort (CAS) instructions
union moveType {
  struct {
    short suit;
    short rank;
    short sequence; // Whether or not this move is the first in a sequence
    short weight;   // Signed weight used at sorting. Most significant 16 bits on little-endian M1 Max
  };
  int64_t raw;
};
#pragma clang diagnostic pop


static_assert(sizeof(moveType) == 8, "moveType should remain compact");

/** @brief Small move list and cursor for one trick depth and relative hand. */
struct movePlyType
{
  moveType move[14];
  int current;
  int last;
};

/** @brief Winning-card descriptor used while resolving a trick. */
struct highCardType
{
  int rank;
  int hand;
};


/**
 * @brief Recursive DDS position state.
 *
 * `pos` is the internal state object threaded through `ABsearch*()`.
 * It contains:
 * - remaining card bitmasks and suit lengths,
 * - aggregate signatures used for TT lookup,
 * - trick-local winner/leader state for each ply, and
 * - the number of tricks already secured by the MAX side.
 */
struct pos
{
  unsigned short int rankInSuit[DDS_HANDS][DDS_SUITS];
  unsigned short int aggr[DDS_SUITS];
  unsigned char length[DDS_HANDS][DDS_SUITS];
  int handDist[DDS_HANDS];

  unsigned short int winRanks[50][DDS_SUITS];
  /* Cards that win by rank, firstindex is depth. */
  int first[50];
  /* Hand that leads the trick for each ply */
  moveType move[50];
  /* Presently winning move */
  int handRelFirst;
  /* The current hand, relative first hand */
  int tricksMAX;
  /* Aggregated tricks won by MAX */
  highCardType winner[DDS_SUITS];
  /* Winning rank of trick. */
  highCardType secondBest[DDS_SUITS];
  /* Second best rank. */
};


/** @brief Static/terminal evaluation result returned by DDS proof helpers. */
struct evalType
{
  int tricks;
  unsigned short int winRanks[DDS_SUITS];
};

/** @brief Simple suit/rank pair used by utility code. */
struct card
{
  int suit;
  int rank;
};

/** @brief Suit/rank pair extended with sequence metadata. */
struct extCard
{
  int suit;
  int rank;
  int sequence;
};

/** @brief Absolute rank ownership record used in relative-rank tables. */
struct absRankType // 2 bytes
{
  char rank;
  signed char hand;
};

/**
 * @brief Relative-rank lookup table for one aggregate 13-bit suit holding.
 *
 * This lets the move generator convert between compact aggregate rank patterns
 * and seat-specific ownership without rebuilding the mapping repeatedly.
 */
struct relRanksType // 120 bytes
{
  absRankType absRank[15][DDS_SUITS];
};

/** @brief Shared batch-run parameter block used by worker backends. */
struct paramType
{
  int noOfBoards;
  boards * bop;
  solvedBoards * solvedp;
  int error;
};

/** @brief High-level operation category used by the scheduler and threading layer. */
enum RunMode
{
  DDS_RUN_SOLVE = 0,
  DDS_RUN_CALC = 1,
  DDS_RUN_TRACE = 2,
  DDS_RUN_SIZE = 3
};

#endif
