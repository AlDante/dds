/**
 * @file TransTable.h
 * @brief Abstract DDS transposition-table interface and stored bound payload.
 *
 * DDS caches perfect-information search results as lower/upper bounds plus a
 * best-move hint. The search talks only to this abstract interface; concrete
 * storage layouts live in `TransTableS.*` and `TransTableL.*`.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
 */

#ifndef DDS_TRANSTABLE_H
#define DDS_TRANSTABLE_H

#include <iostream>
#include <fstream>
#include <string>

#include "dds.h"

using namespace std;


/** @brief Reasons why a TT backend may discard or rebuild cached state. */
enum TTresetReason
{
  TT_RESET_UNKNOWN = 0,
  TT_RESET_TOO_MANY_NODES = 1,
  TT_RESET_NEW_DEAL = 2,
  TT_RESET_NEW_TRUMP = 3,
  TT_RESET_MEMORY_EXHAUSTED = 4,
  TT_RESET_FREE_MEMORY = 5,
  TT_RESET_SIZE = 6
};

/**
 * @brief Compact DDS transposition-table payload.
 *
 * Unlike alpha-mu's exact-front cache, DDS stores scalar proof information for a
 * perfect-information node:
 * - `ubound` / `lbound` are trick bounds from the current node,
 * - `bestMoveSuit` / `bestMoveRank` are move-ordering hints,
 * - `leastWin` stores the lowest winning rank per suit.
 */
struct nodeCardsType // 8 bytes
{
  char ubound; // For N-S
  char lbound; // For N-S
  char bestMoveSuit;
  char bestMoveRank;
  char leastWin[DDS_SUITS];
};

#ifdef _MSC_VER
  // Disable warning for unused arguments.
  #pragma warning(push)
  #pragma warning(disable: 4100)
#endif

#ifdef __APPLE__
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

/**
 * @brief Abstract base class for DDS transposition-table backends.
 *
 * Search code uses this contract to probe/store bounds without knowing whether
 * the underlying table is the compact (`TransTableS`) or large (`TransTableL`)
 * implementation.
 */
class TransTable
{
  public:
    TransTable(){}

    virtual ~TransTable() {}

    /** @brief Initialize backend lookup tables for hand-signature encoding. */
    virtual void Init(const int handLookup[][15]) = 0;

    /** @brief Set the preferred memory target used by automatic sizing. */
    virtual void SetMemoryDefault(const int megabytes) = 0;

    /** @brief Set the hard upper memory cap the backend may allocate. */
    virtual void SetMemoryMaximum(const int megabytes) = 0;

    /** @brief Allocate and initialize the backing TT storage. */
    virtual void MakeTT() = 0;

    /** @brief Clear cached entries while preserving the configured TT shape. */
    virtual void ResetMemory(const TTresetReason reason) = 0;

    /** @brief Release all TT-owned memory. */
    virtual void ReturnAllMemory() = 0;

    /** @brief Report memory currently owned by the backend, in bytes/MB units used by the implementation. */
    virtual double MemoryInUse() const = 0;

    /**
     * @brief Probe the TT for a previously solved position.
     *
     * `lowerFlag` tells the caller whether the returned bound should be treated
     * as a lower-bound proof or an upper-bound proof for the current threshold.
     */
    virtual nodeCardsType const * Lookup(
      const int trick,
      const int hand,
      const unsigned short aggrTarget[],
      const int handDist[],
      const int limit,
      bool& lowerFlag) = 0;

    /** @brief Store a newly solved position and its bound metadata. */
    virtual void Add(
      const int trick,
      const int hand,
      const unsigned short aggrTarget[],
      const unsigned short winRanksArg[],
      const nodeCardsType& first,
      const bool flag) = 0;

    virtual void PrintSuits(
      ofstream& fout, 
      const int trick, 
      const int hand) const {}

    virtual void PrintAllSuits(ofstream& fout) const {}

    virtual void PrintSuitStats(
      ofstream& fout, 
      const int trick, 
      const int hand) const {}

    virtual void PrintAllSuitStats(ofstream& fout) const {}

    virtual void PrintSummarySuitStats(ofstream& fout) const {}

    virtual void PrintEntriesDist(
      ofstream& fout, 
      const int trick,
      const int hand,
      const int handDist[]) const {}

    virtual void PrintEntriesDistAndCards(
      ofstream& fout,
      const int trick,
      const int hand,
      const unsigned short aggrTarget[],
      const int handDist[]) const {}

    virtual void PrintEntries(
      ofstream& fout, 
      const int trick, 
      const int hand) const {}

    virtual void PrintAllEntries(ofstream& fout) const {}

    virtual void PrintEntryStats(
      ofstream& fout, 
      const int trick, 
      const int hand) const {}

    virtual void PrintAllEntryStats(ofstream& fout) const {}

    virtual void PrintSummaryEntryStats(ofstream& fout) const {}

    virtual void PrintPageSummary(ofstream& fout) const {}

    virtual void PrintNodeStats(ofstream& fout) const {}

    virtual void PrintResetStats(ofstream& fout) const {}
};

#ifdef _MSC_VER
  #pragma warning(pop)
#endif

#ifdef __APPLE__
  #pragma clang diagnostic pop
#endif

#ifdef __GNUC__
  #pragma GCC diagnostic pop
#endif

#endif
