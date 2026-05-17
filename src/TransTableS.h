/**
 * @file TransTableS.h
 * @brief Compact-memory DDS transposition-table backend.
 *
 * `TransTableS` trades memory footprint for somewhat higher lookup/update cost.
 * It organizes entries through suit-length search nodes and linked win-card sets
 * rather than the larger block/page structure used by `TransTableL`.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
 */

#ifndef DDS_TRANSTABLES_H
#define DDS_TRANSTABLES_H

#include <vector>
#include <string>

#include "TransTable.h"

using namespace std;

/**
 * @brief Small-memory implementation of the DDS transposition-table interface.
 *
 * The key hierarchy is roughly:
 * - trick count and first hand,
 * - suit-length signature,
 * - aggregated winning-card order/mask information,
 * - compact bound payload.
 */
class TransTableS: public TransTable
{
  private:

    // Structures for the small memory option.

    /** @brief Linked node for one winning-card pattern under a length signature. */
    struct winCardType
    {
      int orderSet;
      int winMask;
      nodeCardsType * first;
      winCardType * prevWin;
      winCardType * nextWin;
      winCardType * next;
    };

    /** @brief BST node keyed by compacted suit-length signature. */
    struct posSearchTypeSmall
    {
      winCardType * posSearchPoint;
      long long suitLengths;
      posSearchTypeSmall * left;
      posSearchTypeSmall * right;
    };

    /** @brief Precomputed aggregate-rank and win-mask expansion for one 13-bit holding. */
    struct ttAggrType
    {
      int aggrRanks[DDS_SUITS];
      int winMask[DDS_SUITS];
    };

    /** @brief Reset counters grouped by reset cause. */
    struct statsResetsType
    {
      int noOfResets;
      int aggrResets[TT_RESET_SIZE];
    };


    long long aggrLenSets[14];
    statsResetsType statsResets;

    winCardType temp_win[5];
    int nodeSetSizeLimit;
    int winSetSizeLimit;
    unsigned long long maxmem;
    unsigned long long allocmem;
    unsigned long long summem;
    int wmem;
    int nmem;
    int maxIndex;
    int wcount;
    int ncount;
    bool clearTTflag;
    int windex;
    ttAggrType * aggp;

    posSearchTypeSmall * rootnp[14][DDS_HANDS];
    winCardType ** pw;
    nodeCardsType ** pn;
    posSearchTypeSmall ** pl[14][DDS_HANDS];
    nodeCardsType * nodeCards;
    winCardType * winCards;
    posSearchTypeSmall * posSearch[14][DDS_HANDS];
    int nodeSetSize; /* Index with range 0 to nodeSetSizeLimit */
    int winSetSize;  /* Index with range 0 to winSetSizeLimit */
    int lenSetInd[14][DDS_HANDS];
    int lcount[14][DDS_HANDS];

    vector<string> resetText;

    long long suitLengths[14];

    int TTInUse;

    void SetConstants();

    void Wipe();

    void InitTT();

    void AddWinSet();

    void AddNodeSet();

    void AddLenSet(
      const int trick, 
      const int firstHand);

    void BuildSOP(
      const unsigned short ourWinRanks[DDS_SUITS],
      const unsigned short aggr[DDS_SUITS],
      const nodeCardsType& first,
      const long long suitLengths,
      const int tricks,
      const int firstHand,
      const bool flag);

    nodeCardsType * BuildPath(
      const int winMask[],
      const int winOrderSet[],
      const int ubound,
      const int lbound,
      const char bestMoveSuit,
      const char bestMoveRank,
      posSearchTypeSmall * node,
      bool& result);

    struct posSearchTypeSmall * SearchLenAndInsert(
      posSearchTypeSmall * rootp,
      const long long key,
      const bool insertNode,
      const int trick,
      const int firstHand,
      bool& result);

    nodeCardsType * UpdateSOP(
      const int ubound,
      const int lbound,
      const char bestMoveSuit,
      const char bestMoveRank,
      nodeCardsType * nodep);

    nodeCardsType const * FindSOP(
      const int orderSet[],
      const int limit,
      winCardType * nodeP,
      bool& lowerFlag);

  public:

    TransTableS();

    ~TransTableS();

    void Init(const int handLookup[][15]) override;

    void SetMemoryDefault(const int megabytes) override;

    void SetMemoryMaximum(const int megabytes) override;

    void MakeTT() override;

    void ResetMemory(const TTresetReason reason) override;

    void ReturnAllMemory() override;

    double MemoryInUse() const override;

    nodeCardsType const * Lookup(
      const int trick,
      const int hand,
      const unsigned short aggrTarget[],
      const int handDist[],
      const int limit,
      bool& lowerFlag) override;

    void Add(
      const int trick,
      const int hand,
      const unsigned short aggrTarget[],
      const unsigned short winRanksArg[],
      const nodeCardsType& first,
      const bool flag) override;

    void PrintNodeStats(ofstream& fout) const override;

    void PrintResetStats(ofstream& fout) const override;
};

#endif
