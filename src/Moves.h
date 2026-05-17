/**
 * @file Moves.h
 * @brief Per-thread DDS move generation, move ordering, and trick-local replay state.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
 */

#ifndef DDS_MOVES_H
#define DDS_MOVES_H

#include <iostream>
#include <fstream>
#include <string>

#include "dds.h"
#include "../include/dll.h"

using namespace std;


/*
  Move-generation function identifiers recorded by the optional DDS_MOVES
  instrumentation.  The suffix naming convention is:

  - 0/1/2/3: relative hand within the current trick,
  - NT/TRUMP: whether a trump suit exists,
  - VOID/NOTVOID: whether the hand must follow suit.

  The values are used only for statistics/reporting; they do not affect search
  semantics.
*/
enum MGtype
{
  MG_NT0 = 0,
  MG_TRUMP0 = 1,
  MG_NT_VOID1 = 2,
  MG_TRUMP_VOID1 = 3,
  MG_NT_NOTVOID1 = 4,
  MG_TRUMP_NOTVOID1 = 5,
  MG_NT_VOID2 = 6,
  MG_TRUMP_VOID2 = 7,
  MG_NT_NOTVOID2 = 8,
  MG_TRUMP_NOTVOID2 = 9,
  MG_NT_VOID3 = 10,
  MG_TRUMP_VOID3 = 11,
  MG_COMB_NOTVOID3 = 12,
  MG_SIZE = 13
};


/*
  Compact summary of the current trick after four cards have been provisionally
  selected through MakeNext()/MakeSpecific().  ABsearch::Make3() uses this to
  resolve the completed trick without having to rescan all four played cards.
*/
struct trickDataType
{
  /* Number of cards played in each suit during the current trick. */
  int playCount[DDS_SUITS];
  /* Winning absolute rank after the fourth hand has been applied. */
  int bestRank;
  /* Winning suit after the fourth hand has been applied. */
  int bestSuit;
  /* Sequence bits associated with the winning card. */
  int bestSequence;
  /* Relative hand (0..3) that currently wins the trick. */
  int relWinner;
  /* Reserved convenience field for the next leader. */
  int nextLeadHand;
};


/*
  Moves owns all per-thread move-generation state for one search.  It has three
  responsibilities:

  1. enumerate legal candidate cards for each relative hand,
  2. weight and sort those candidates to improve alpha-beta cutoffs,
  3. maintain enough trick-local state for MakeNext() to advance the current
     trick incrementally without rebuilding everything from scratch.

  The search calls the class in this rough order:

  - Init()/Reinit() establish the current trick shell,
  - MoveGen0()/MoveGen123() populate and weight a move list,
  - MakeNext() iterates the sorted list while updating trick state,
  - GetTrickData() summarizes a completed trick for fourth-hand make/undo,
  - optional statistics/reporting methods inspect the recorded behavior.
*/
class Moves
{
  private:

    /*
      Scratch copies of the current move-generation context.  These are updated
      before entering the WeightAlloc* helpers so the helpers can stay compact
      and avoid repeatedly re-deriving the same values from the position.
    */
    int leadHand;
    int leadSuit;
    int currHand;
    int currSuit;
    int currTrick;
    int trump;
    int suit;
    int numMoves;
    int lastNumMoves;

    /*
      Per-trick transient state used while constructing and replaying moves.
      There are at most 13 remaining tricks, so the array is indexed directly by
      the current trick number.
    */
    struct trackType
    {
      /* Absolute hand that leads this trick. */
      int leadHand;
      /* Suit led by relative hand 0 once known. */
      int leadSuit;
      /* Suit chosen by each relative hand in the provisional trick. */
      int playSuits[DDS_HANDS];
      /* Rank chosen by each relative hand in the provisional trick. */
      int playRanks[DDS_HANDS];
      /* Cached fourth-hand summary derived from playSuits/playRanks/high. */
      trickDataType trickData;
      /* Current winning card after each relative hand has played. */
      extCard move[DDS_HANDS];
      /* Relative hand currently winning after 0..3 plays. */
      int high[DDS_HANDS];
      /* Lowest rank that still matters per hand/suit during MakeNext(). */
      int lowestWin[DDS_HANDS][DDS_SUITS];
      /* Bitmask of cards already removed from each suit at this trick depth. */
      int removedRanks[DDS_SUITS];
    };

    /* One track record per remaining trick count. */
    trackType track[13];
    /* Pointer to the active track[trick] entry while generating/playing moves. */
    trackType * trackp;

    /*
      Candidate move lists indexed by remaining trick count and relative hand.
      Each list is short (maximum 13 cards, 14 storage slots for convenience)
      and is sorted in descending weight order.
    */
    movePlyType moveList[13][DDS_HANDS];

    /* Pointer to the currently active move array within moveList. */
    moveType * mply;

    /* Last move-generation helper used for each trick/relative hand. */
    MGtype lastCall[13][DDS_HANDS];

    /* Human-readable names matching MGtype for reports. */
    string funcName[MG_SIZE];

    /* Aggregate counters for one statistics bucket. */
    struct moveStatType
    {
      /* Number of observations accumulated into this bucket. */
      int count;
      /* MGtype identifier for the bucket. */
      int findex;
      /* Sum of hit positions (how far into a list the chosen move was). */
      int sumHits;
      /* Sum of candidate-list lengths seen for this bucket. */
      int sumLengths;
    };

    /* Collection of per-generator statistics buckets. */
    struct moveStatsType
    {
      /* Number of populated entries in list[]. */
      int nfuncs;
      /* One bucket per generator family that was actually observed. */
      moveStatType list[MG_SIZE];
    };

    /* Per-trick/hand aggregate hit statistics. */
    moveStatType trickTable[13][DDS_HANDS];

    /* Per-trick/hand hit statistics restricted to the played suit. */
    moveStatType trickSuitTable[13][DDS_HANDS];

    /* Per-trick/hand breakdown by generator family. */
    moveStatsType trickDetailTable[13][DDS_HANDS];

    /* Per-trick/hand breakdown by generator family in the chosen suit only. */
    moveStatsType trickDetailSuitTable[13][DDS_HANDS];

    /* Whole-search breakdown by generator family. */
    moveStatsType trickFuncTable;

    /* Whole-search breakdown by generator family in the chosen suit only. */
    moveStatsType trickFuncSuitTable;


    /* Weight relative-hand-0 candidates in trump contracts. */
    void WeightAllocTrump0(
      const pos& tpos,
      const moveType& bestMove,
      const moveType& bestMoveTT,
      const relRanksType thrp_rel[]);

    /* Weight relative-hand-0 candidates in notrump contracts. */
    void WeightAllocNT0(
      const pos& tpos,
      const moveType& bestMove,
      const moveType& bestMoveTT,
      const relRanksType thrp_rel[]);

    /* Weight second-hand follow-suit candidates in trump contracts. */
    void WeightAllocTrumpNotvoid1( const pos& tpos);
    /* Weight second-hand follow-suit candidates in notrump contracts. */
    void WeightAllocNTNotvoid1(const pos& tpos);
    /* Weight second-hand discard/ruff candidates in trump contracts. */
    void WeightAllocTrumpVoid1(const pos& tpos);
    /* Weight second-hand discard candidates in notrump contracts. */
    void WeightAllocNTVoid1(const pos& tpos);
    /* Weight third-hand follow-suit candidates in trump contracts. */
    void WeightAllocTrumpNotvoid2(const pos& tpos);
    /* Weight third-hand follow-suit candidates in notrump contracts. */
    void WeightAllocNTNotvoid2(const pos& tpos);
    /* Weight third-hand discard/ruff candidates in trump contracts. */
    void WeightAllocTrumpVoid2(const pos& tpos);
    /* Weight third-hand discard candidates in notrump contracts. */
    void WeightAllocNTVoid2(const pos& tpos);
    /* Weight fourth-hand follow-suit candidates when a shared rule works for
       both trump and notrump contracts. */
    void WeightAllocCombinedNotvoid3(const pos& tpos);
    /* Weight fourth-hand discard/ruff candidates in trump contracts. */
    void WeightAllocTrumpVoid3(const pos& tpos);
    /* Weight fourth-hand discard candidates in notrump contracts. */
    void WeightAllocNTVoid3(const pos& tpos);

    /*
      Helper for third-hand notrump weighting.  Determines how many top honors a
      partner suit can cash and which candidate move first overtakes partner.
    */
    void GetTopNumber(
      const int ris,
      const int prank,
      int& topNumber,
      int& mno) const;

    /* Identify the cheapest candidate that forces out fourth hand's top honor. */
    int RankForcesAce(int cards4th) const;

    /* Signature of the hand/contract-specific weighting helpers. */
    typedef void (Moves::*WeightPtr)(const pos& tpos);
    /* Lookup table indexed by hand-relative/contract/voidness case. */
    WeightPtr WeightList[16];

    /* Compare a newly played card against the current winning card. */
    inline bool WinningMove(
      const moveType& mvp1,
      const extCard& mvp2,
      const int trump) const;

    /* Render one move list as formatted text. */
    string PrintMove(const movePlyType& mply) const;

    /* Dispatch to the currently selected small-N sorter implementation. */
    void SortMoves();

    /* Merge one observation into a move statistics table. */
    void UpdateStatsEntry(
      moveStatsType& stat,
      const int findex,
      const int hit,
      const int len) const;

    /* Format an average hit/length pair for reports. */
    string AverageString(const moveStatType& statp) const;

    /* Format the full per-generator statistics line used in reports. */
    string FullAverageString(const moveStatType& statp) const;

    /* Render a trick-by-trick statistics matrix. */
    string PrintTrickTable(const moveStatType tablep[][DDS_HANDS]) const;

    /* Render a generator-family statistics table. */
    string PrintFunctionTable(const moveStatsType& tablep) const;

  public:
    /* Construct an empty move-generation context and initialize report tables. */
    Moves();

    /* Trivial destructor; ownership stays entirely within the object. */
    ~Moves();

    /*
      Initialize per-trick state before the first relative hand of a trick is
      searched.  This sets the lead hand, computes removed-rank masks, and
      clears all move-list cursors for the search.
    */
    void Init(
      const int tricks,
      const int relStartHand,
      const int initialRanks[],
      const int initialSuits[],
      const unsigned short rankInSuit[DDS_HANDS][DDS_SUITS],
      const int trump,
      const int leadHand);

    /* Reuse an existing trick shell when only the leading hand changes. */
    void Reinit(
      const int tricks,
      const int leadHand);

    /*
      Generate, weight, and sort lead-hand candidates.  This path can lead any
      suit, so it scans all non-empty suits and then applies contract-specific
      weighting through WeightAllocTrump0() or WeightAllocNT0().
    */
    int MoveGen0(
      const int tricks,
      const pos& tpos,
      const moveType& bestMove,
      const moveType& bestMoveTT,
      const relRanksType thrp_rel[]);

    /*
      Generate, weight, and sort candidates for relative hands 1, 2, or 3.
      When the hand must follow suit it uses a single-suit fast path; otherwise
      it generates candidates from every legal suit and dispatches to the
      appropriate WeightAlloc* helper.
    */
    int MoveGen123(
      const int tricks,
      const int relHand,
      const pos& tpos);

    /* Return the number of candidates currently stored for a trick/hand pair. */
    int GetLength(
      const int trick,
      const int relHand) const;

    /*
      Inject a specific move as if MakeNext() had chosen it, updating the trick
      winner/high-card tracking and next-trick removed-rank masks.
    */
    void MakeSpecific(
      const moveType& mply,
      const int trick,
      const int relHand);

    /*
      Return the next candidate whose rank is still relevant according to
      winRanks, while updating the trick-local winner state.  One low losing
      move per suit is still allowed so the search can represent small-card
      continuations cheaply.
    */
    moveType const * MakeNext(
      const int trick,
      const int relHand,
      const unsigned short winRanks[DDS_SUITS]);

    /* Variant of MakeNext() that simply walks the pre-sorted list. */
    moveType const * MakeNextSimple(
      const int trick,
      const int relHand);

    /* Advance a move-list cursor manually. */
    void Step(
      const int tricks,
      const int relHand);

    /* Reset a move-list cursor to the first candidate. */
    void Rewind(
      const int tricks,
      const int relHand);

    /* Remove user-forbidden lead cards from a generated list. */
    void Purge(
      const int tricks,
      const int relHand,
      const moveType forbiddenMoves[]);

    /* Increase the weight of the move most recently returned by MakeNext(). */
    void Reward(
      const int trick,
      const int relHand);

    /* Summarize the currently assembled four-card trick for fourth-hand make. */
    const trickDataType& GetTrickData(const int tricks);

    /* Re-sort an already populated move list. */
    void Sort(
      const int tricks,
      const int relHand);

    /*
      Historical baseline sorter: a fixed compare-swap network for sizes up to
      12, falling back to insertion sort only outside the normal DDS range.
      The network is tuned for very short lists and preserves the exact ordering
      semantics expected by the existing search.
    */
    static void MergeSort(
      moveType * mply,
      int numMoves);


    /*
      Diagnostic alternative to MergeSort().  It first derives the exact output
      permutation using the same compare network on indices, then applies that
      permutation by cycle moves.  It exists for benchmark comparison, not as
      the default production path.
    */
    static void CycleSort(
      moveType * mply,
      int numMoves);

    /* Render one move list as formatted diagnostic text. */
    string PrintMoves(
      const int trick,
      const int relHand) const;

    /* Record how far into the list the accepted move was found. */
    void RegisterHit(
      const int tricks,
      const int relHand);

    /* Render the last provisional trick as a single text line. */
    string TrickToText(const int trick) const;

    /* Print aggregate hit-position statistics. */
    void PrintTrickStats(ofstream& fout) const;

    /* Print per-trick and per-generator breakdown statistics. */
    void PrintTrickDetails(ofstream& fout) const;

    /* Print whole-search generator-family statistics. */
    void PrintFunctionStats(ofstream& fout) const;
};

#endif
