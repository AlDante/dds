/**
 * @file Memory.h
 * @brief Per-thread DDS solver state and the top-level thread-memory manager.
 *
 * DDS keeps almost all mutable search state thread-local. `ThreadData` is the
 * package searched by one worker at a time, while `Memory` owns the pool of
 * `ThreadData` instances sized according to the configured threading mode.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
 */

#ifndef DDS_MEMORY_H
#define DDS_MEMORY_H

#include <string>
#include <vector>

#include "TransTable.h"
#include "TransTableS.h"
#include "TransTableL.h"

#include "Moves.h"
#include "File.h"
#include "debug.h"

#ifdef DDS_AB_STATS
  #include "ABstats.h"
#endif

#ifdef DDS_TIMING
  #include "TimerList.h"
#endif

/** @brief Requested TT footprint class for newly allocated worker memories. */
enum TTmemory
{
  DDS_TT_SMALL = 0,
  DDS_TT_LARGE = 1
};

/** @brief Winning and runner-up rank information for one suit. */
struct WinnerEntryType
{
  int suit;
  int winnerRank;
  int winnerHand;
  int secondRank;
  int secondHand;
};

/** @brief Bundle of immediate winning-card descriptors for the current node. */
struct WinnersType
{
  int number;
  WinnerEntryType winner[4];
};


/**
 * @brief Thread-local mutable state for one DDS worker.
 *
 * This struct intentionally groups together the objects that must not be shared
 * across concurrent searches:
 * - the active recursive position,
 * - TT handle,
 * - move generator,
 * - pruning and best-move helpers,
 * - search counters, and
 * - optional debug/timing/reporting sinks.
 */
struct ThreadData
{
  int nodeTypeStore[DDS_HANDS];
  int iniDepth;
  bool val;

  unsigned short int suit[DDS_HANDS][DDS_SUITS];
  int trump;

  pos lookAheadPos; // Recursive alpha-beta data
  bool analysisFlag;
  unsigned short int lowestWin[50][DDS_SUITS];
  WinnersType winners[13];
  moveType forbiddenMoves[14];
  moveType bestMove[50];
  moveType bestMoveTT[50];

  double memUsed;
  int nodes;
  int trickNodes;

  // Constant for a given hand.
  // 960 KB
  relRanksType rel[8192];

  TransTable * transTable;

  Moves moves;

#ifdef DDS_TOP_LEVEL
  File fileTopLevel;
#endif

#ifdef DDS_AB_STATS
  ABstats ABStats;
  File fileABstats;
#endif

#ifdef DDS_AB_HITS
  File fileRetrieved;
  File fileStored;
#endif

#ifdef DDS_TT_STATS
  File fileTTstats;
#endif 

#ifdef DDS_TIMING
  TimerList timerList;
  File fileTimerList;
#endif

#ifdef DDS_MOVES
  File fileMoves;
#endif

};


/**
 * @brief Owner of all allocated `ThreadData` objects.
 *
 * The `System` and API layers use this class to resize the worker pool, fetch a
 * thread slot by id, and release all per-thread resources at shutdown.
 */
class Memory
{
  private:

    std::vector<ThreadData *> memory;

    std::vector<std::string> threadSizes;

  public:

    Memory();

    ~Memory();

    /** @brief Release one worker slot back to the idle pool after a top-level run. */
    void ReturnThread(const unsigned thrId);

    /** @brief Resize the worker pool and choose the requested TT footprint class. */
    void Resize(
      const unsigned n,
      const TTmemory flag,
      const int memDefault_MB,
      const int memMaximum_MB);

    /** @brief Return the number of currently allocated worker slots. */
    unsigned NumThreads() const;

    /** @brief Fetch the mutable thread-local state block for one worker id. */
    ThreadData * GetPtr(const unsigned thrId);

    /** @brief Report the currently allocated footprint of one worker in megabytes. */
    double MemoryInUseMB(const unsigned thrId) const;

    /** @brief Return the textual TT footprint label for one worker slot. */
    std::string ThreadSize(const unsigned thrId) const;
};

#endif
