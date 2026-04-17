/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/

#ifndef DDS_TEST_ALPHA_MU_PROTOTYPE_CORE_H
#define DDS_TEST_ALPHA_MU_PROTOTYPE_CORE_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "parse.h"
#include "../include/dll.h"
namespace alpha_mu_prototype
{
  using namespace std;

  struct WorldMask
  {
    unsigned count;
    unsigned long long bits;

    WorldMask() : count(0), bits(0ULL) {}

    WorldMask(
      const unsigned countArg,
      const unsigned long long bitsArg) :
      count(countArg),
      bits(bitsArg)
    {
      if (count < 64)
      {
        const unsigned long long mask =
          (count == 0 ? 0ULL : ((1ULL << count) - 1ULL));
        bits &= mask;
      }
    }

    static WorldMask All(const unsigned countArg)
    {
      return WorldMask(
        countArg,
        (countArg >= 64 ? ~0ULL :
         (countArg == 0 ? 0ULL : ((1ULL << countArg) - 1ULL))));
    }

    static WorldMask None(const unsigned countArg)
    {
      return WorldMask(countArg, 0ULL);
    }

    bool operator==(const WorldMask& other) const
    {
      return count == other.count && bits == other.bits;
    }

    bool Has(const unsigned index) const
    {
      return index < count && ((bits & (1ULL << index)) != 0ULL);
    }

    unsigned PopCount() const
    {
      unsigned n = 0;
      unsigned long long copy = bits;
      while (copy != 0ULL)
      {
        n += static_cast<unsigned>(copy & 1ULL);
        copy >>= 1;
      }
      return n;
    }

    WorldMask Union(const WorldMask& other) const
    {
      return WorldMask(count, bits | other.bits);
    }

    WorldMask Intersection(const WorldMask& other) const
    {
      return WorldMask(count, bits & other.bits);
    }

    bool Empty() const
    {
      return bits == 0ULL;
    }

    string ToString() const
    {
      ostringstream oss;
      oss << "{";
      for (unsigned i = 0; i < count; i++)
      {
        if (i != 0)
          oss << ",";
        if (Has(i))
          oss << i;
      }
      oss << "}";
      return oss.str();
    }
  };


  struct OutcomeVector
  {
    WorldMask valid;
    vector<int> values;

    OutcomeVector() {}

    explicit OutcomeVector(const unsigned worldCount) :
      valid(WorldMask::None(worldCount)),
      values(worldCount, 0)
    {
    }

    OutcomeVector(
      const WorldMask& validArg,
      const vector<int>& valuesArg) :
      valid(validArg),
      values(valuesArg)
    {
    }

    bool Dominates(const OutcomeVector& other) const
    {
      if (! (valid == other.valid))
        return false;

      for (unsigned i = 0; i < values.size(); i++)
      {
        if (valid.Has(i) && values[i] < other.values[i])
          return false;
      }
      return true;
    }

    double Mean() const
    {
      const unsigned n = valid.PopCount();
      if (n == 0)
        return 0.0;

      int sum = 0;
      for (unsigned i = 0; i < values.size(); i++)
      {
        if (valid.Has(i))
          sum += values[i];
      }

      return static_cast<double>(sum) / static_cast<double>(n);
    }

    OutcomeVector MinWith(const OutcomeVector& other) const
    {
      if (values.size() != other.values.size())
        throw runtime_error("OutcomeVector size mismatch");

      OutcomeVector result(static_cast<unsigned>(values.size()));
      result.valid = valid.Union(other.valid);

      for (unsigned i = 0; i < values.size(); i++)
      {
        const bool here = valid.Has(i);
        const bool there = other.valid.Has(i);

        if (here && there)
          result.values[i] = min(values[i], other.values[i]);
        else if (here)
          result.values[i] = values[i];
        else if (there)
          result.values[i] = other.values[i];
      }

      return result;
    }

    OutcomeVector RestrictToUseful(const WorldMask& useful) const
    {
      OutcomeVector result(*this);
      result.valid = valid.Intersection(useful);
      for (unsigned i = 0; i < values.size(); i++)
      {
        if (valid.Has(i) && ! useful.Has(i))
          result.values[i] = 0;
      }
      return result;
    }

    OutcomeVector CompleteOptimistically(
      const WorldMask& useful,
      const OutcomeVector& optimistic) const
    {
      OutcomeVector result(*this);
      for (unsigned i = 0; i < values.size(); i++)
      {
        if (result.valid.Has(i))
          continue;

        if (! useful.Has(i))
        {
          result.valid.bits |= (1ULL << i);
          result.values[i] = 0;
        }
        else if (optimistic.valid.Has(i))
        {
          result.valid.bits |= (1ULL << i);
          result.values[i] = optimistic.values[i];
        }
      }

      return result;
    }

    string ToString() const
    {
      ostringstream oss;
      oss << "[";
      for (unsigned i = 0; i < values.size(); i++)
      {
        if (i != 0)
          oss << " ";
        if (valid.Has(i))
          oss << values[i];
        else
          oss << "x";
      }
      oss << "]";
      return oss.str();
    }
  };


  struct ParetoFront
  {
    unsigned worldCount;
    vector<OutcomeVector> vectors;

    explicit ParetoFront(const unsigned worldCountArg = 0) :
      worldCount(worldCountArg),
      vectors()
    {
    }

    void Insert(const OutcomeVector& candidate)
    {
      for (unsigned i = 0; i < vectors.size(); i++)
      {
        if (vectors[i].Dominates(candidate))
          return;
      }

      vector<OutcomeVector> kept;
      for (unsigned i = 0; i < vectors.size(); i++)
      {
        if (! candidate.Dominates(vectors[i]))
          kept.push_back(vectors[i]);
      }

      kept.push_back(candidate);
      vectors.swap(kept);
    }

    bool DominatesFront(const ParetoFront& other) const
    {
      for (unsigned i = 0; i < other.vectors.size(); i++)
      {
        bool found = false;
        for (unsigned j = 0; j < vectors.size(); j++)
        {
          if (vectors[j].Dominates(other.vectors[i]))
          {
            found = true;
            break;
          }
        }

        if (! found)
          return false;
      }
      return true;
    }

    double Mu() const
    {
      double best = 0.0;
      for (unsigned i = 0; i < vectors.size(); i++)
        best = max(best, vectors[i].Mean());
      return best;
    }

    static ParetoFront MaxMerge(
      const ParetoFront& left,
      const ParetoFront& right)
    {
      ParetoFront result(left.worldCount);
      for (unsigned i = 0; i < left.vectors.size(); i++)
        result.Insert(left.vectors[i]);
      for (unsigned i = 0; i < right.vectors.size(); i++)
        result.Insert(right.vectors[i]);
      return result;
    }

    static ParetoFront MinProduct(
      const ParetoFront& left,
      const ParetoFront& right)
    {
      ParetoFront result(left.worldCount);
      for (unsigned i = 0; i < left.vectors.size(); i++)
      {
        for (unsigned j = 0; j < right.vectors.size(); j++)
          result.Insert(left.vectors[i].MinWith(right.vectors[j]));
      }
      return result;
    }

    WorldMask UsefulWorlds() const
    {
      WorldMask useful = WorldMask::None(worldCount);
      for (unsigned w = 0; w < worldCount; w++)
      {
        bool keep = false;
        for (unsigned i = 0; i < vectors.size(); i++)
        {
          if (! vectors[i].valid.Has(w) || vectors[i].values[w] > 0)
          {
            keep = true;
            break;
          }
        }

        if (keep)
          useful.bits |= (1ULL << w);
      }
      return useful;
    }

    WorldMask ValidWorlds() const
    {
      WorldMask valid = WorldMask::None(worldCount);
      for (unsigned i = 0; i < vectors.size(); i++)
        valid = valid.Union(vectors[i].valid);
      return valid;
    }

    ParetoFront RestrictToUseful(const WorldMask& useful) const
    {
      ParetoFront result(worldCount);
      for (unsigned i = 0; i < vectors.size(); i++)
        result.Insert(vectors[i].RestrictToUseful(useful));
      return result;
    }

    ParetoFront CompleteOptimistically(
      const WorldMask& useful,
      const OutcomeVector& optimistic) const
    {
      ParetoFront result(worldCount);
      for (unsigned i = 0; i < vectors.size(); i++)
      {
        result.Insert(vectors[i].CompleteOptimistically(useful, optimistic));
      }
      return result;
    }

    bool WinsAll(const WorldMask& useful) const
    {
      for (unsigned i = 0; i < vectors.size(); i++)
      {
        bool wins = true;
        for (unsigned w = 0; w < useful.count; w++)
        {
          if (useful.Has(w) &&
              (! vectors[i].valid.Has(w) || vectors[i].values[w] <= 0))
          {
            wins = false;
            break;
          }
        }

        if (wins)
          return true;
      }

      return false;
    }

    string ToString() const
    {
      ostringstream oss;
      oss << "{";
      for (unsigned i = 0; i < vectors.size(); i++)
      {
        if (i != 0)
          oss << ", ";
        oss << vectors[i].ToString();
      }
      oss << "}";
      return oss.str();
    }
  };


  enum ToyNodeType
  {
    TOY_LEAF = 0,
    TOY_MAX = 1,
    TOY_MIN = 2
  };


  struct ToyNode
  {
    string name;
    ToyNodeType type;
    ParetoFront leafFront;
    OutcomeVector optimisticValues;
    vector<const ToyNode *> children;
    vector<WorldMask> childWorlds;

    ToyNode(
      const string& nameArg,
      const ToyNodeType typeArg,
      const unsigned worldCount) :
      name(nameArg),
      type(typeArg),
      leafFront(worldCount),
      optimisticValues(worldCount),
      children(),
      childWorlds()
    {
    }
  };


  struct SearchStats
  {
    int nodesVisited;
    int earlyCuts;
    int deepAlphaCuts;
    int optimisticCompletions;
    int ttHits;
    int ttStores;
    int rootCuts;
    int cutOnWinCuts;
    int usefulWorldUpdates;
    int leafWorldEvaluations;
    int worldCutsZero;
    int worldCutsSingle;
    vector<string> visitOrder;

    SearchStats() :
      nodesVisited(0),
      earlyCuts(0),
      deepAlphaCuts(0),
      optimisticCompletions(0),
      ttHits(0),
      ttStores(0),
      rootCuts(0),
      cutOnWinCuts(0),
      usefulWorldUpdates(0),
      leafWorldEvaluations(0),
      worldCutsZero(0),
      worldCutsSingle(0),
      visitOrder()
    {
    }
  };


  struct TTEntry
  {
    ParetoFront front;

    explicit TTEntry(const unsigned worldCount = 0) :
      front(worldCount)
    {
    }

    explicit TTEntry(const ParetoFront& frontArg) :
      front(frontArg)
    {
    }
  };


  struct TranspositionTable
  {
    map<string, TTEntry> entries;

    bool Lookup(const string& key, ParetoFront& front) const
    {
      map<string, TTEntry>::const_iterator it = entries.find(key);
      if (it == entries.end())
        return false;

      front = it->second.front;
      return true;
    }

    void Store(const string& key, const ParetoFront& front)
    {
      entries[key] = TTEntry(front);
    }
  };


  struct IterativeResult
  {
    ParetoFront front;
    int depthReached;
    bool rootCutTriggered;
    vector<SearchStats> statsPerDepth;

    explicit IterativeResult(const unsigned worldCount) :
      front(worldCount),
      depthReached(0),
      rootCutTriggered(false),
      statsPerDepth()
    {
    }
  };


  struct HandFileData
  {
    int number;
    bool GIBmode;
    int * dealerList;
    int * vulList;
    dealPBN * dealList;
    futureTricks * futList;
    ddTableResults * tableList;
    parResults * parList;
    parResultsDealer * dealerParList;
    playTracePBN * playList;
    solvedPlay * traceList;

    HandFileData() :
      number(0),
      GIBmode(false),
      dealerList(NULL),
      vulList(NULL),
      dealList(NULL),
      futList(NULL),
      tableList(NULL),
      parList(NULL),
      dealerParList(NULL),
      playList(NULL),
      traceList(NULL)
    {
    }

    HandFileData(const HandFileData&) = delete;
    HandFileData& operator=(const HandFileData&) = delete;

    HandFileData(HandFileData&& other) :
      number(other.number),
      GIBmode(other.GIBmode),
      dealerList(other.dealerList),
      vulList(other.vulList),
      dealList(other.dealList),
      futList(other.futList),
      tableList(other.tableList),
      parList(other.parList),
      dealerParList(other.dealerParList),
      playList(other.playList),
      traceList(other.traceList)
    {
      other.Reset();
    }

    HandFileData& operator=(HandFileData&& other)
    {
      if (this != &other)
      {
        Free();
        number = other.number;
        GIBmode = other.GIBmode;
        dealerList = other.dealerList;
        vulList = other.vulList;
        dealList = other.dealList;
        futList = other.futList;
        tableList = other.tableList;
        parList = other.parList;
        dealerParList = other.dealerParList;
        playList = other.playList;
        traceList = other.traceList;
        other.Reset();
      }
      return *this;
    }

    ~HandFileData()
    {
      Free();
    }

  private:
    void Reset()
    {
      number = 0;
      GIBmode = false;
      dealerList = NULL;
      vulList = NULL;
      dealList = NULL;
      futList = NULL;
      tableList = NULL;
      parList = NULL;
      dealerParList = NULL;
      playList = NULL;
      traceList = NULL;
    }

    void Free()
    {
      free(dealerList);
      free(vulList);
      free(dealList);
      free(futList);
      free(tableList);
      free(parList);
      free(dealerParList);
      free(playList);
      free(traceList);
      Reset();
    }
  };


  enum Seat
  {
    SEAT_NORTH = 0,
    SEAT_EAST = 1,
    SEAT_SOUTH = 2,
    SEAT_WEST = 3
  };


  enum SuitIndex
  {
    SUIT_SPADES = 0,
    SUIT_HEARTS = 1,
    SUIT_DIAMONDS = 2,
    SUIT_CLUBS = 3
  };


  enum ConstraintKind
  {
    CONSTRAINT_HAS_CARD = 0,
    CONSTRAINT_NOT_HAS_CARD = 1,
    CONSTRAINT_VOID_SUIT = 2,
    CONSTRAINT_MIN_LENGTH = 3,
    CONSTRAINT_MAX_LENGTH = 4,
    CONSTRAINT_MIN_HCP = 5,
    CONSTRAINT_MAX_HCP = 6,
    CONSTRAINT_BALANCED = 7,
    CONSTRAINT_PARTNERSHIP_MIN_LENGTH = 8,
    CONSTRAINT_PARTNERSHIP_MIN_HCP = 9,
    CONSTRAINT_PARTNERSHIP_MAX_HCP = 10,
    CONSTRAINT_PARTNERSHIP_MAX_LENGTH = 11,
    CONSTRAINT_HAND_TYPE = 12
  };


  enum HandType
  {
    HAND_TYPE_BALANCED = 0,
    HAND_TYPE_ONE_SUITER = 1,
    HAND_TYPE_TWO_SUITER = 2,
    HAND_TYPE_THREE_SUITER = 3
  };


  struct ParsedWorld
  {
    string suits[4][4];
  };


  struct BridgeMove
  {
    int suit;
    char rank;

    BridgeMove() : suit(0), rank('0') {}

    BridgeMove(
      const int suitArg,
      const char rankArg) :
      suit(suitArg),
      rank(rankArg)
    {
    }

    bool operator==(const BridgeMove& other) const
    {
      return suit == other.suit && rank == other.rank;
    }
  };


  struct BridgeState
  {
    vector<ParsedWorld> worlds;
    WorldMask possibleWorlds;
    int playerToMove;
    int maxSide;
    int maxTricksWon;
    int trumpSuit;
    int trickLeader;
    int leadSuit;
    vector<BridgeMove> currentTrick;
    vector<int> currentTrickPlayers;

    BridgeState() :
      worlds(),
      possibleWorlds(),
      playerToMove(0),
      maxSide(0),
      maxTricksWon(0),
      trumpSuit(-1),
      trickLeader(0),
      leadSuit(-1),
      currentTrick(),
      currentTrickPlayers()
    {
    }
  };


  struct BridgeChild
  {
    BridgeMove move;
    BridgeState state;
  };


  struct BridgeRootChildReport
  {
    BridgeMove move;
    ParetoFront front;
    WorldMask validWorlds;
    WorldMask usefulWorlds;

    explicit BridgeRootChildReport(const unsigned worldCount = 0) :
      move(),
      front(worldCount),
      validWorlds(WorldMask::None(worldCount)),
      usefulWorlds(WorldMask::None(worldCount))
    {
    }
  };


  struct BridgeRootReport
  {
    vector<BridgeRootChildReport> children;
    ParetoFront rootFront;

    explicit BridgeRootReport(const unsigned worldCount = 0) :
      children(),
      rootFront(worldCount)
    {
    }
  };


  struct PlayHistoryEvent
  {
    int player;
    int leadSuit;
    BridgeMove move;

    PlayHistoryEvent() :
      player(0),
      leadSuit(-1),
      move()
    {
    }

    PlayHistoryEvent(
      const int playerArg,
      const int leadSuitArg,
      const BridgeMove& moveArg) :
      player(playerArg),
      leadSuit(leadSuitArg),
      move(moveArg)
    {
    }
  };


  struct WorldGenerationStats
  {
    unsigned candidateWorldCount;
    unsigned afterKnownCardCount;
    unsigned afterBiddingCount;
    unsigned afterFollowSuitCount;
    unsigned afterPlayHistoryCount;
    unsigned afterCurrentTrickCount;
    unsigned afterSamplingCount;
    unsigned finalWorldCount;
    unsigned duplicateWorldsRemoved;
    unsigned sampledOutWorlds;

    WorldGenerationStats() :
      candidateWorldCount(0),
      afterKnownCardCount(0),
      afterBiddingCount(0),
      afterFollowSuitCount(0),
      afterPlayHistoryCount(0),
      afterCurrentTrickCount(0),
      afterSamplingCount(0),
      finalWorldCount(0),
      duplicateWorldsRemoved(0),
      sampledOutWorlds(0)
    {
    }
  };


  struct WorldConstraint
  {
    ConstraintKind kind;
    int player;
    int suit;
    char rank;
    int count;

    WorldConstraint() :
      kind(CONSTRAINT_HAS_CARD),
      player(0),
      suit(0),
      rank('0'),
      count(0)
    {
    }

    static WorldConstraint HasCard(
      const int playerArg,
      const int suitArg,
      const char rankArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_HAS_CARD;
      c.player = playerArg;
      c.suit = suitArg;
      c.rank = rankArg;
      return c;
    }

    static WorldConstraint VoidSuit(
      const int playerArg,
      const int suitArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_VOID_SUIT;
      c.player = playerArg;
      c.suit = suitArg;
      return c;
    }

    static WorldConstraint NotHasCard(
      const int playerArg,
      const int suitArg,
      const char rankArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_NOT_HAS_CARD;
      c.player = playerArg;
      c.suit = suitArg;
      c.rank = rankArg;
      return c;
    }

    static WorldConstraint MinLength(
      const int playerArg,
      const int suitArg,
      const int countArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_MIN_LENGTH;
      c.player = playerArg;
      c.suit = suitArg;
      c.count = countArg;
      return c;
    }

    static WorldConstraint MaxLength(
      const int playerArg,
      const int suitArg,
      const int countArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_MAX_LENGTH;
      c.player = playerArg;
      c.suit = suitArg;
      c.count = countArg;
      return c;
    }

    static WorldConstraint MinHCP(
      const int playerArg,
      const int countArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_MIN_HCP;
      c.player = playerArg;
      c.count = countArg;
      return c;
    }

    static WorldConstraint MaxHCP(
      const int playerArg,
      const int countArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_MAX_HCP;
      c.player = playerArg;
      c.count = countArg;
      return c;
    }

    static WorldConstraint Balanced(
      const int playerArg)
    {
      return HandTypeConstraint(playerArg, HAND_TYPE_BALANCED);
    }

    static WorldConstraint HandTypeConstraint(
      const int playerArg,
      const int handTypeArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_HAND_TYPE;
      c.player = playerArg;
      c.count = handTypeArg;
      return c;
    }

    static WorldConstraint PartnershipMinLength(
      const int playerArg,
      const int suitArg,
      const int countArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_PARTNERSHIP_MIN_LENGTH;
      c.player = playerArg;
      c.suit = suitArg;
      c.count = countArg;
      return c;
    }

    static WorldConstraint PartnershipMaxLength(
      const int playerArg,
      const int suitArg,
      const int countArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_PARTNERSHIP_MAX_LENGTH;
      c.player = playerArg;
      c.suit = suitArg;
      c.count = countArg;
      return c;
    }

    static WorldConstraint PartnershipMinHCP(
      const int playerArg,
      const int countArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_PARTNERSHIP_MIN_HCP;
      c.player = playerArg;
      c.count = countArg;
      return c;
    }

    static WorldConstraint PartnershipMaxHCP(
      const int playerArg,
      const int countArg)
    {
      WorldConstraint c;
      c.kind = CONSTRAINT_PARTNERSHIP_MAX_HCP;
      c.player = playerArg;
      c.count = countArg;
      return c;
    }
  };


  struct BridgeInformationState
  {
    vector<WorldConstraint> knownCardConstraints;
    vector<WorldConstraint> biddingConstraints;
    vector<WorldConstraint> followSuitConstraints;
    vector<PlayHistoryEvent> playHistory;
    vector<PlayHistoryEvent> currentTrickHistory;
    bool deriveFollowSuitConstraints;
    bool deduplicateEquivalentWorlds;
    unsigned sampleLimit;
    unsigned samplingSeed;

    BridgeInformationState() :
      knownCardConstraints(),
      biddingConstraints(),
      followSuitConstraints(),
      playHistory(),
      currentTrickHistory(),
      deriveFollowSuitConstraints(true),
      deduplicateEquivalentWorlds(false),
      sampleLimit(0),
      samplingSeed(0)
    {
    }
  };


  struct HiddenCardCandidate
  {
    BridgeMove card;
    unsigned allowedSeatsMask;

    HiddenCardCandidate() :
      card(),
      allowedSeatsMask(0U)
    {
    }
  };


  struct HistoryDerivedWorldSpec
  {
    ParsedWorld seedWorld;
    vector<int> hiddenSeats;
    bool inferHiddenCardsFromVisibleHands;

    HistoryDerivedWorldSpec() :
      seedWorld(),
      hiddenSeats(),
      inferHiddenCardsFromVisibleHands(false)
    {
    }
  };


  struct HistoryDerivedConstructionResult
  {
    ParsedWorld visibleSeedWorld;
    vector<HiddenCardCandidate> hiddenCards;
    vector<ParsedWorld> worlds;

    HistoryDerivedConstructionResult() :
      visibleSeedWorld(),
      hiddenCards(),
      worlds()
    {
    }
  };


  struct HistoryDerivedConstructionStats
  {
    unsigned rawAssignmentCount;
    unsigned afterOwnershipCount;
    unsigned afterCardLocationCount;
    unsigned afterConstructorLengthCount;
    unsigned afterConstructorHCPCount;
    unsigned afterConstructorBalancedCount;
    unsigned afterConstructorConstraintCount;
    unsigned finalWorldCount;

    HistoryDerivedConstructionStats() :
      rawAssignmentCount(0),
      afterOwnershipCount(0),
      afterCardLocationCount(0),
      afterConstructorLengthCount(0),
      afterConstructorHCPCount(0),
      afterConstructorBalancedCount(0),
      afterConstructorConstraintCount(0),
      finalWorldCount(0)
    {
    }
  };


  struct WorldExplanationStep
  {
    string stage;
    bool passed;
    string detail;

    WorldExplanationStep() :
      stage(),
      passed(false),
      detail()
    {
    }
  };


  struct WorldExplanation
  {
    unsigned worldIndex;
    string serializedWorld;
    bool accepted;
    string rejectionStage;
    string rejectionReason;
    vector<WorldExplanationStep> steps;

    WorldExplanation() :
      worldIndex(0),
      serializedWorld(),
      accepted(true),
      rejectionStage(),
      rejectionReason(),
      steps()
    {
    }
  };


  struct WorldGenerationExplanation
  {
    vector<WorldConstraint> appliedFollowSuitConstraints;
    WorldMask finalWorldMask;
    vector<WorldExplanation> worlds;

    WorldGenerationExplanation() :
      appliedFollowSuitConstraints(),
      finalWorldMask(),
      worlds()
    {
    }
  };


  struct HistoryDerivedConstructionExplanation
  {
    HistoryDerivedConstructionStats stats;
    vector<WorldConstraint> constructorConstraints;
    vector<WorldExplanation> worlds;

    HistoryDerivedConstructionExplanation() :
      stats(),
      constructorConstraints(),
      worlds()
    {
    }
  };


  struct HistoryCheckResult
  {
    bool ok;
    string reason;

    HistoryCheckResult() :
      ok(true),
      reason()
    {
    }
  };


  struct DDSLeafEvalResult
  {
    OutcomeVector leaf;
    vector<int> bestScores;
    int workerCount;

    explicit DDSLeafEvalResult(const unsigned worldCount = 0) :
      leaf(worldCount),
      bestScores(worldCount, 0),
      workerCount(0)
    {
    }
  };


  struct DDSVsAlphaMuDepthTiming
  {
    int depth;
    double elapsedSeconds;
    unsigned mismatches;

    DDSVsAlphaMuDepthTiming() :
      depth(0),
      elapsedSeconds(0.0),
      mismatches(0)
    {
    }
  };


  struct DDSVsAlphaMuComparison
  {
    string handFile;
    unsigned boardsTested;
    double ddsElapsedSeconds;
    vector<DDSVsAlphaMuDepthTiming> alphaMuDepths;

    DDSVsAlphaMuComparison() :
      handFile(),
      boardsTested(0),
      ddsElapsedSeconds(0.0),
      alphaMuDepths()
    {
    }
  };


  struct BenchmarkMethodSummary
  {
    string method;
    string handFile;
    unsigned boardsTested;
    int depth;
    double elapsedSeconds;
    unsigned mismatches;
    vector<double> perBoardSeconds;

    BenchmarkMethodSummary() :
      method(),
      handFile(),
      boardsTested(0),
      depth(0),
      elapsedSeconds(0.0),
      mismatches(0),
      perBoardSeconds()
    {
    }
  };


  struct BenchmarkBoardProgressContext
  {
    string method;
    string handFile;
    unsigned boardNumber;
    unsigned totalBoards;
    int depth;
    double reportIntervalSeconds;
    chrono::steady_clock::time_point totalStart;
    chrono::steady_clock::time_point boardStart;
    double nextReportSeconds;
    unsigned long long recursiveCalls;
    unsigned long long ddsLeafCalls;

    BenchmarkBoardProgressContext() :
      method(),
      handFile(),
      boardNumber(0),
      totalBoards(0),
      depth(0),
      reportIntervalSeconds(0.0),
      totalStart(),
      boardStart(),
      nextReportSeconds(0.0),
      recursiveCalls(0ULL),
      ddsLeafCalls(0ULL)
    {
    }
  };

  extern const char kPrototypeMessagePrefix[];
  extern const char kAlphaMuPlayHandFile[];

  void Fail(const string& msg);
  void Check(const bool condition, const string& msg);
  bool SameOutcome( const OutcomeVector& left, const OutcomeVector& right);
  bool FrontContains( const ParetoFront& front, const OutcomeVector& target);
  int SeatIndex(const char seat);
  vector<string> SplitString( const string& text, const char delimiter, const bool keepEmpty);
  ParsedWorld ParsePBNWorld(const string& pbn);
  bool WorldHasCard( const ParsedWorld& world, const int player, const int suit, const char rank);
  int WorldSuitLength( const ParsedWorld& world, const int player, const int suit);
  int HonorPointValue(const char rank);
  int WorldHighCardPoints( const ParsedWorld& world, const int player);
  string HandTypeName(const int handType);
  bool LengthsMatchHandType( const int lengths[4], const int totalCards, const int handType);
  bool WorldHasHandType( const ParsedWorld& world, const int player, const int handType);
  bool WorldHasBalancedShape( const ParsedWorld& world, const int player);
  int ConstraintHandType(const WorldConstraint& constraint);
  int RankOrder(const char rank);
  int RankValue(const char rank);
  char RankFromDDSValue(const int value);
  unsigned WorldCardCount(const ParsedWorld& world);
  unsigned WorldSeatCardCount( const ParsedWorld& world, const int player);
  string SerializePBNWorld(const ParsedWorld& world);
  string SeatName(const int seat);
  string SuitName(const int suit);
  int PartnerSeat(const int seat);
  string PartnershipName(const int seat);
  string CardName(const BridgeMove& move);
  bool WorldMatchesConstraint( const ParsedWorld& world, const WorldConstraint& constraint);
  void RemoveCardFromWorld( ParsedWorld& world, const int player, const BridgeMove& move);
  string ConstraintToString(const WorldConstraint& constraint);
  string FirstConstraintFailureReason( const ParsedWorld& world, const vector<WorldConstraint>& constraints);
  HistoryCheckResult CheckWorldReplayHistory( const ParsedWorld& world, const vector<PlayHistoryEvent>& history, const string& stageName);
  HistoryCheckResult CheckWorldReplayHistoryAfterHistory( const ParsedWorld& world, const vector<PlayHistoryEvent>& priorHistory, const vector<PlayHistoryEvent>& history, const string& stageName);
  void AppendDerivedFollowSuitConstraints( const vector<PlayHistoryEvent>& history, int playedCount[4][4], vector<WorldConstraint>& constraints);
  vector<WorldConstraint> CollectFollowSuitConstraints( const BridgeInformationState& information);
  vector<WorldConstraint> CollectConstructorConstraints( const BridgeInformationState& information);
  void AddWorldExplanationStep( WorldExplanation& explanation, const string& stage, const bool passed, const string& detail);
  unsigned SeatBit(const int seat);
  unsigned SeatMask(const vector<int>& seats);
  bool VectorContainsSeat( const vector<int>& seats, const int seat);
  bool ConstructorConstraintTouchesHiddenSeats( const vector<int>& hiddenSeats, const WorldConstraint& constraint);
  string CardKey(const BridgeMove& move);
  void SortSuitCards(string& cards);
  void CanonicalizeWorld(ParsedWorld& world);
  void CollectPlayedCards( const BridgeInformationState& information, map<string, int>& playedBySeat);
  unsigned CountSeatChoices(const unsigned allowedSeatsMask);
  int FindCardSeatInWorld( const ParsedWorld& world, const int suit, const char rank);
  void CollectSeedHiddenCards( const HistoryDerivedWorldSpec& spec, HistoryDerivedConstructionResult& result, const unsigned hiddenSeatMask, int targetCounts[4]);
  void CollectInferredHiddenCardsFromVisibleHands( const HistoryDerivedWorldSpec& spec, HistoryDerivedConstructionResult& result, const unsigned hiddenSeatMask, int targetCounts[4], int finalSeatCounts[4]);
  void ApplyConstructionPlayedCardOwnership( const vector<int>& hiddenSeats, const BridgeInformationState& information, vector<HiddenCardCandidate>& hiddenCards);
  void SortHiddenCardCandidates( vector<HiddenCardCandidate>& hiddenCards);
  void SortConstructedWorlds( vector<ParsedWorld>& worlds);
  void PrepareHistoryDerivedConstruction( const HistoryDerivedWorldSpec& spec, const BridgeInformationState& information, HistoryDerivedConstructionResult& result, vector<WorldConstraint>& constructorConstraints, int targetCounts[4], int finalSeatCounts[4]);
  void ApplyConstructionCardLocationConstraints( const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints, vector<HiddenCardCandidate>& hiddenCards);
  int PotentialRemainingSuitCardsForSeat( const vector<HiddenCardCandidate>& hiddenCards, const unsigned nextIndex, const int seat, const int suit);
  int PotentialRemainingHighCardPointsForSeat( const vector<HiddenCardCandidate>& hiddenCards, const unsigned nextIndex, const int seat);
  bool ConstructorLengthConstraintsPossible( const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints, const ParsedWorld& current, const vector<HiddenCardCandidate>& hiddenCards, const unsigned nextIndex);
  bool ConstructorHCPConstraintsPossible( const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints, const ParsedWorld& current, const vector<HiddenCardCandidate>& hiddenCards, const unsigned nextIndex);
  bool PartialSeatCanStillReachBalancedShape( const ParsedWorld& current, const vector<HiddenCardCandidate>& hiddenCards, const unsigned nextIndex, const int seat, const int targetCount, const int handType);
  bool ConstructorBalancedConstraintsPossible( const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints, const ParsedWorld& current, const vector<HiddenCardCandidate>& hiddenCards, const int finalSeatCounts[4], const unsigned nextIndex);
  bool ConstructorBiddingConstraintsPossible( const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints, const ParsedWorld& current, const vector<HiddenCardCandidate>& hiddenCards, const int finalSeatCounts[4], const unsigned nextIndex);
  bool CanAssignHiddenCard( const HiddenCardCandidate& candidate, const int seat, const int targetCounts[4], const int assignedCounts[4]);
  void ConstructHistoryDerivedWorldsRec( const vector<int>& hiddenSeats, const vector<HiddenCardCandidate>& hiddenCards, const vector<WorldConstraint>& constructorConstraints, const int targetCounts[4], const int finalSeatCounts[4], int assignedCounts[4], ParsedWorld& current, const unsigned index, vector<ParsedWorld>& worlds);
  void EnumerateHistoryDerivedWorldsRec( const vector<int>& hiddenSeats, const vector<HiddenCardCandidate>& hiddenCards, const int targetCounts[4], int assignedCounts[4], ParsedWorld& current, const unsigned index, vector<ParsedWorld>& worlds);
  vector<ParsedWorld> EnumerateHistoryDerivedWorlds( const vector<int>& hiddenSeats, const vector<HiddenCardCandidate>& hiddenCards, const int targetCounts[4], const ParsedWorld& visibleSeedWorld);
  bool WorldMatchesConstructorLengthConstraints( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  bool WorldMatchesConstructorHCPConstraints( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  bool WorldMatchesConstructorBalancedConstraints( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  string FirstConstructorLengthFailureReason( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  string FirstConstructorHCPFailureReason( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  string FirstConstructorBalancedFailureReason( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  HistoryDerivedConstructionResult ConstructCandidateWorldsFromHistory( const HistoryDerivedWorldSpec& spec, const BridgeInformationState& information);
  HistoryDerivedConstructionExplanation ExplainHistoryDerivedConstruction( const HistoryDerivedWorldSpec& spec, const BridgeInformationState& information);
  dealPBN MakeDDSDealPBN( const BridgeState& state, const ParsedWorld& world);
  int RemainingTricksInWorld( const BridgeState& state, const ParsedWorld& world);
  bool BridgeMoveLess( const BridgeMove& left, const BridgeMove& right);
  int SeatSide(const int seat);
  unsigned WinningCardIndex( const vector<BridgeMove>& trick, const int leadSuit, const int trumpSuit);
  int TrickWinner( const BridgeState& state);
  bool WorldHasLegalSuit( const ParsedWorld& world, const int player, const int leadSuit);
  vector<BridgeMove> LegalMovesInWorld( const ParsedWorld& world, const int player, const int leadSuit);
  bool ContainsMove( const vector<BridgeMove>& moves, const BridgeMove& move);
  bool WorldCanPlayMove( const ParsedWorld& world, const int player, const int leadSuit, const BridgeMove& move);
  vector<BridgeMove> GenerateBridgeMoves(const BridgeState& state);
  BridgeState PlayBridgeMove( const BridgeState& state, const BridgeMove& move);
  vector<BridgeChild> ExpandBridgeChildren(const BridgeState& state);
  ParetoFront MakeZeroFront(const unsigned worldCount);
  int BestScore(const futureTricks& fut);
  void CheckDDS(const int ret, const string& tag);
  void MaybeReportBenchmarkBoardProgress( const BridgeState& state, const int tricksRemaining);
  ParetoFront MakeBridgeTerminalFront(const BridgeState& state);
  ParetoFront MakeBridgeDDSLeafFront(const BridgeState& state);
  int BridgeDepthCost( const BridgeState& state, const BridgeState& child);
  ParetoFront SearchBridgeStateInternal( const BridgeState& state, const int tricksRemaining);
  ParetoFront SearchBridgeState( const BridgeState& state, const int tricksRemaining);
  BridgeRootReport AnalyzeBridgeRoot( const BridgeState& state, const int tricksRemaining);
  bool WorldMatchesAllConstraints( const ParsedWorld& world, const vector<WorldConstraint>& constraints);
  WorldMask FilterWorldsByConstraints( const vector<ParsedWorld>& worlds, const WorldMask& candidates, const vector<WorldConstraint>& constraints);
  bool WorldCanReplayHistory( const ParsedWorld& world, const vector<PlayHistoryEvent>& history);
  WorldMask FilterWorldsByHistory( const vector<ParsedWorld>& worlds, const WorldMask& candidates, const vector<PlayHistoryEvent>& history);
  WorldMask FilterWorldsByHistoryAfterHistory( const vector<ParsedWorld>& worlds, const WorldMask& candidates, const vector<PlayHistoryEvent>& priorHistory, const vector<PlayHistoryEvent>& history);
  WorldMask DeduplicateWorldMask( const vector<ParsedWorld>& worlds, const WorldMask& candidates, unsigned& duplicatesRemoved);
  WorldMask SampleWorldMaskDeterministically( const vector<ParsedWorld>& worlds, const WorldMask& candidates, const unsigned sampleLimit, const unsigned samplingSeed, unsigned& sampledOutWorlds);
  WorldMask GeneratePossibleWorlds( const vector<ParsedWorld>& worlds, const BridgeInformationState& information, WorldGenerationStats* stats);
  WorldGenerationExplanation ExplainPossibleWorldGeneration( const vector<ParsedWorld>& worlds, const BridgeInformationState& information);
  WorldMask GeneratePossibleWorlds( const vector<ParsedWorld>& worlds, const vector<WorldConstraint>& constraints);
  OutcomeVector MakeBinaryOutcome( const string& text);
  void AddChild( ToyNode& parent, const ToyNode& child, const WorldMask& worlds);
  void AddChild( ToyNode& parent, const ToyNode& child);
  string MakeTTKey( const ToyNode& node, const int maxMoves, const WorldMask& usefulWorlds);
  ParetoFront MakeFront( const unsigned worldCount, const vector<string>& outcomes);
  string ResolvePath(const string& candidate);
  ParetoFront MakeSingleWorldFront( const unsigned worldCount, const unsigned world, const int value);
  unsigned SoleWorldIndex(const WorldMask& mask);
  int EvaluateLeafWorld( const ParetoFront& front, const unsigned world);
  int EvaluateSingleWorld( const ToyNode& node, const int maxMoves, const unsigned world, SearchStats& stats);
  ParetoFront SearchToy( const ToyNode& node, const int maxMoves, const WorldMask& usefulWorlds, const vector<const ParetoFront *>& upperMaxFronts, const OutcomeVector& optimisticValues, TranspositionTable& tt, const bool isRoot, const double previousRootMu, SearchStats& stats, bool& rootCutTriggered, bool& exactComplete);
  IterativeResult RunIterativeDeepening( const ToyNode& root, const int maxDepth);
  int ExactBridgeDDSScoreForWorld( const BridgeState& state, const unsigned worldIndex);
  void LoadHandFile( const string& fname, HandFileData& data);
  int SolveDDSLeafWorld( const HandFileData& data, const int index, const int thrId);
  BridgeState MakeBridgeStateFromDDSDeal( const dealPBN& deal);
  int SingleWorldFrontScore( const ParetoFront& front, const int depth, const int boardIndex);
  unsigned BoardsToBenchmark( const HandFileData& data, const int maxBoards);
  set<unsigned> ParseSkippedBoardNumbers( const string& skipSpec, const unsigned availableBoards);
  vector<unsigned> SelectBenchmarkBoardNumbers( const HandFileData& data, const int maxBoards, const string& skipSpec);
  double BenchmarkCheckpointIntervalSeconds();
  void ReportBenchmarkCheckpoint( const BenchmarkMethodSummary& summary, const unsigned completedBoards, const double elapsedSeconds);
  double BenchmarkProgressIntervalSeconds();
  void ReportBenchmarkBoardTiming( const BenchmarkMethodSummary& summary, const unsigned boardNumber, const double boardElapsedSeconds, const double totalElapsedSeconds);
  BenchmarkMethodSummary BenchmarkDDSExactBoards( const string& handFile, const int maxBoards, const string& skipSpec);
  BenchmarkMethodSummary BenchmarkAlphaMuExactBoards( const string& handFile, const int depth, const int maxBoards, const string& skipSpec);
  void ReportBenchmarkMethodSummary( const BenchmarkMethodSummary& summary);
  DDSVsAlphaMuComparison CompareDDSAndAlphaMu( const string& handFile, const int maxDepth, const int maxBoards);
  void ReportDDSVsAlphaMuComparison( const DDSVsAlphaMuComparison& summary);
  void CheckDDSLeafBestScores( const HandFileData& data, const vector<int>& bestScores);
  DDSLeafEvalResult EvaluateDDSLeafThresholdSerial( const HandFileData& data, const int target);
  DDSLeafEvalResult EvaluateDDSLeafThresholdParallel( const HandFileData& data, const int target, const int requestedThreads);

  void PrintPrototypeStatus(const string& msg);
}

#endif
