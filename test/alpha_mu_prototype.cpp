/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "parse.h"
#include "../include/dll.h"

using namespace std;

namespace
{
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

    ~HandFileData()
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
    CONSTRAINT_VOID_SUIT = 1,
    CONSTRAINT_MIN_LENGTH = 2,
    CONSTRAINT_MAX_LENGTH = 3
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
    unsigned afterPlayHistoryCount;
    unsigned afterCurrentTrickCount;
    unsigned finalWorldCount;
    unsigned duplicateWorldsRemoved;

    WorldGenerationStats() :
      candidateWorldCount(0),
      afterKnownCardCount(0),
      afterBiddingCount(0),
      afterPlayHistoryCount(0),
      afterCurrentTrickCount(0),
      finalWorldCount(0),
      duplicateWorldsRemoved(0)
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
  };


  struct BridgeInformationState
  {
    vector<WorldConstraint> knownCardConstraints;
    vector<WorldConstraint> biddingConstraints;
    vector<PlayHistoryEvent> playHistory;
    vector<PlayHistoryEvent> currentTrickHistory;
    bool deduplicateEquivalentWorlds;

    BridgeInformationState() :
      knownCardConstraints(),
      biddingConstraints(),
      playHistory(),
      currentTrickHistory(),
      deduplicateEquivalentWorlds(false)
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


  static void Fail(const string& msg)
  {
    cerr << "alpha_mu_prototype: " << msg << "\n";
    exit(1);
  }


  static void Check(const bool condition, const string& msg)
  {
    if (! condition)
      Fail(msg);
  }


  static bool SameOutcome(
    const OutcomeVector& left,
    const OutcomeVector& right)
  {
    return left.valid == right.valid && left.values == right.values;
  }


  static bool FrontContains(
    const ParetoFront& front,
    const OutcomeVector& target)
  {
    for (unsigned i = 0; i < front.vectors.size(); i++)
    {
      if (SameOutcome(front.vectors[i], target))
        return true;
    }
    return false;
  }


  static int SeatIndex(const char seat)
  {
    switch (seat)
    {
      case 'N': return SEAT_NORTH;
      case 'E': return SEAT_EAST;
      case 'S': return SEAT_SOUTH;
      case 'W': return SEAT_WEST;
      default:
        throw runtime_error("Unknown seat in PBN world");
    }
  }


  static vector<string> SplitString(
    const string& text,
    const char delimiter,
    const bool keepEmpty)
  {
    vector<string> parts;
    string current;
    for (unsigned i = 0; i < text.size(); i++)
    {
      if (text[i] == delimiter)
      {
        if (keepEmpty || ! current.empty())
          parts.push_back(current);
        current.clear();
      }
      else
        current.push_back(text[i]);
    }

    if (keepEmpty || ! current.empty())
      parts.push_back(current);
    return parts;
  }


  static ParsedWorld ParsePBNWorld(const string& pbn)
  {
    const size_t colon = pbn.find(':');
    if (colon == string::npos || colon == 0)
      throw runtime_error("Bad PBN world string");

    const int startSeat = SeatIndex(pbn[0]);
    const vector<string> hands = SplitString(pbn.substr(colon + 1), ' ', false);
    if (hands.size() != 4)
      throw runtime_error("PBN world should contain four hands");

    ParsedWorld world;
    for (unsigned h = 0; h < 4; h++)
    {
      const vector<string> suits = SplitString(hands[h], '.', true);
      if (suits.size() != 4)
        throw runtime_error("PBN hand should contain four suits");

      const int seat = (startSeat + static_cast<int>(h)) % 4;
      for (unsigned s = 0; s < 4; s++)
        world.suits[seat][s] = suits[s];
    }

    return world;
  }


  static bool WorldHasCard(
    const ParsedWorld& world,
    const int player,
    const int suit,
    const char rank)
  {
    return world.suits[player][suit].find(rank) != string::npos;
  }


  static int WorldSuitLength(
    const ParsedWorld& world,
    const int player,
    const int suit)
  {
    return static_cast<int>(world.suits[player][suit].size());
  }


  static int RankOrder(const char rank)
  {
    const string order = "23456789TJQKA";
    const size_t pos = order.find(rank);
    if (pos == string::npos)
      throw runtime_error("Unknown card rank");
    return static_cast<int>(pos);
  }


  static int RankValue(const char rank)
  {
    return RankOrder(rank) + 2;
  }


  static unsigned WorldCardCount(const ParsedWorld& world)
  {
    unsigned count = 0;
    for (int player = 0; player < 4; player++)
    {
      for (int suit = 0; suit < 4; suit++)
        count += static_cast<unsigned>(world.suits[player][suit].size());
    }
    return count;
  }


  static string SerializePBNWorld(const ParsedWorld& world)
  {
    ostringstream oss;
    oss << "N:";

    for (int seat = 0; seat < 4; seat++)
    {
      if (seat != 0)
        oss << " ";

      for (int suit = 0; suit < 4; suit++)
      {
        if (suit != 0)
          oss << ".";
        oss << world.suits[seat][suit];
      }
    }

    return oss.str();
  }


  static dealPBN MakeDDSDealPBN(
    const BridgeState& state,
    const ParsedWorld& world)
  {
    dealPBN deal;
    memset(&deal, 0, sizeof(deal));

    deal.trump = (state.trumpSuit >= 0 ? state.trumpSuit : 4);
    deal.first = (state.currentTrick.empty() ? state.playerToMove : state.trickLeader);

    for (unsigned i = 0; i < state.currentTrick.size() && i < 3; i++)
    {
      deal.currentTrickSuit[i] = state.currentTrick[i].suit;
      deal.currentTrickRank[i] = RankValue(state.currentTrick[i].rank);
    }

    const string remain = SerializePBNWorld(world);
    Check(remain.size() < sizeof(deal.remainCards),
      "bridge DDS leaf serialization should fit into dealPBN.remainCards");
    strcpy(deal.remainCards, remain.c_str());
    return deal;
  }


  static int RemainingTricksInWorld(
    const BridgeState& state,
    const ParsedWorld& world)
  {
    return static_cast<int>((WorldCardCount(world) + state.currentTrick.size()) / 4U);
  }


  static bool BridgeMoveLess(
    const BridgeMove& left,
    const BridgeMove& right)
  {
    if (left.suit != right.suit)
      return left.suit < right.suit;
    return RankOrder(left.rank) < RankOrder(right.rank);
  }


  static int SeatSide(const int seat)
  {
    return (seat == SEAT_NORTH || seat == SEAT_SOUTH ? 0 : 1);
  }


  static unsigned WinningCardIndex(
    const vector<BridgeMove>& trick,
    const int leadSuit,
    const int trumpSuit)
  {
    if (trick.empty())
      throw runtime_error("WinningCardIndex called on empty trick");

    unsigned best = 0;
    for (unsigned i = 1; i < trick.size(); i++)
    {
      const bool bestTrump = (trumpSuit >= 0 && trick[best].suit == trumpSuit);
      const bool candTrump = (trumpSuit >= 0 && trick[i].suit == trumpSuit);

      if (candTrump && ! bestTrump)
      {
        best = i;
        continue;
      }
      if (bestTrump && ! candTrump)
        continue;

      const int winningSuit = (bestTrump || candTrump ? trumpSuit : leadSuit);
      if (trick[i].suit == winningSuit && trick[best].suit != winningSuit)
      {
        best = i;
        continue;
      }

      if (trick[i].suit == trick[best].suit &&
          RankOrder(trick[i].rank) > RankOrder(trick[best].rank))
      {
        best = i;
      }
    }

    return best;
  }


  static int TrickWinner(
    const BridgeState& state)
  {
    if (state.currentTrick.size() != 4 ||
        state.currentTrickPlayers.size() != 4)
    {
      throw runtime_error("TrickWinner requires a complete 4-card trick");
    }

    const unsigned best = WinningCardIndex(state.currentTrick, state.leadSuit,
      state.trumpSuit);
    return state.currentTrickPlayers[best];
  }


  static bool WorldHasLegalSuit(
    const ParsedWorld& world,
    const int player,
    const int leadSuit)
  {
    return leadSuit >= 0 && WorldSuitLength(world, player, leadSuit) > 0;
  }


  static vector<BridgeMove> LegalMovesInWorld(
    const ParsedWorld& world,
    const int player,
    const int leadSuit)
  {
    vector<BridgeMove> moves;

    if (WorldHasLegalSuit(world, player, leadSuit))
    {
      const string& cards = world.suits[player][leadSuit];
      for (unsigned i = 0; i < cards.size(); i++)
        moves.push_back(BridgeMove(leadSuit, cards[i]));
      return moves;
    }

    for (int suit = 0; suit < 4; suit++)
    {
      const string& cards = world.suits[player][suit];
      for (unsigned i = 0; i < cards.size(); i++)
        moves.push_back(BridgeMove(suit, cards[i]));
    }
    return moves;
  }


  static bool ContainsMove(
    const vector<BridgeMove>& moves,
    const BridgeMove& move)
  {
    for (unsigned i = 0; i < moves.size(); i++)
    {
      if (moves[i] == move)
        return true;
    }
    return false;
  }


  static bool WorldCanPlayMove(
    const ParsedWorld& world,
    const int player,
    const int leadSuit,
    const BridgeMove& move)
  {
    return ContainsMove(LegalMovesInWorld(world, player, leadSuit), move);
  }


  static void RemoveCardFromWorld(
    ParsedWorld& world,
    const int player,
    const BridgeMove& move)
  {
    string& cards = world.suits[player][move.suit];
    const size_t pos = cards.find(move.rank);
    if (pos == string::npos)
      throw runtime_error("Tried to remove a card not held in world");
    cards.erase(pos, 1);
  }


  static vector<BridgeMove> GenerateBridgeMoves(const BridgeState& state)
  {
    vector<BridgeMove> moves;
    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      if (! state.possibleWorlds.Has(i))
        continue;

      const vector<BridgeMove> localMoves = LegalMovesInWorld(
        state.worlds[i],
        state.playerToMove,
        state.leadSuit);
      for (unsigned j = 0; j < localMoves.size(); j++)
      {
        if (! ContainsMove(moves, localMoves[j]))
          moves.push_back(localMoves[j]);
      }
    }

    sort(moves.begin(), moves.end(), BridgeMoveLess);
    return moves;
  }


  static BridgeState PlayBridgeMove(
    const BridgeState& state,
    const BridgeMove& move)
  {
    BridgeState next(state);
    next.possibleWorlds = WorldMask::None(state.possibleWorlds.count);

    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      if (! state.possibleWorlds.Has(i))
        continue;

      if (! WorldCanPlayMove(state.worlds[i], state.playerToMove,
            state.leadSuit, move))
        continue;

      next.possibleWorlds.bits |= (1ULL << i);
      RemoveCardFromWorld(next.worlds[i], state.playerToMove, move);
    }

    if (state.currentTrick.empty())
      next.trickLeader = state.playerToMove;

    next.currentTrick.push_back(move);
    next.currentTrickPlayers.push_back(state.playerToMove);
    if (state.leadSuit < 0)
      next.leadSuit = move.suit;

    if (next.currentTrick.size() == 4)
    {
      const int winner = TrickWinner(next);
      if (SeatSide(winner) == next.maxSide)
        next.maxTricksWon++;
      next.currentTrick.clear();
      next.currentTrickPlayers.clear();
      next.leadSuit = -1;
      next.playerToMove = winner;
      next.trickLeader = winner;
    }
    else
      next.playerToMove = (state.playerToMove + 1) % 4;

    return next;
  }


  static vector<BridgeChild> ExpandBridgeChildren(const BridgeState& state)
  {
    vector<BridgeChild> children;
    const vector<BridgeMove> moves = GenerateBridgeMoves(state);
    for (unsigned i = 0; i < moves.size(); i++)
    {
      BridgeChild child;
      child.move = moves[i];
      child.state = PlayBridgeMove(state, moves[i]);
      children.push_back(child);
    }
    return children;
  }


  static ParetoFront MakeZeroFront(const unsigned worldCount);
  static int BestScore(const futureTricks& fut);
  static void CheckDDS(const int ret, const string& tag);


  static ParetoFront MakeBridgeTerminalFront(const BridgeState& state)
  {
    ParetoFront front(state.possibleWorlds.count);
    OutcomeVector vec(state.possibleWorlds.count);
    vec.valid = state.possibleWorlds;
    for (unsigned i = 0; i < vec.values.size(); i++)
    {
      if (vec.valid.Has(i))
        vec.values[i] = state.maxTricksWon;
    }
    front.Insert(vec);
    return front;
  }


  static ParetoFront MakeBridgeDDSLeafFront(const BridgeState& state)
  {
    vector<unsigned> active;
    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      if (state.possibleWorlds.Has(i))
        active.push_back(i);
    }

    if (active.empty())
      return MakeZeroFront(state.possibleWorlds.count);

    bool allFinished = state.currentTrick.empty();
    for (unsigned i = 0; i < active.size(); i++)
    {
      if (WorldCardCount(state.worlds[active[i]]) != 0)
      {
        allFinished = false;
        break;
      }
    }

    if (allFinished)
      return MakeBridgeTerminalFront(state);

    ParetoFront front(state.possibleWorlds.count);
    OutcomeVector vec(state.possibleWorlds.count);
    vec.valid = state.possibleWorlds;

    for (unsigned i = 0; i < active.size(); i++)
    {
      const unsigned worldIndex = active[i];
      futureTricks fut;
      memset(&fut, 0, sizeof(fut));

      const dealPBN deal = MakeDDSDealPBN(state, state.worlds[worldIndex]);
      const int ret = SolveBoardPBN(deal, -1, 1, 1, &fut, 0);
      ostringstream ddsTag;
      ddsTag << "SolveBoardPBN bridge DDS leaf"
             << " world=" << worldIndex
             << " first=" << deal.first
             << " trickSize=" << state.currentTrick.size()
             << " remain=\"" << deal.remainCards << "\"";
      CheckDDS(ret, ddsTag.str());

      const int best = BestScore(fut);
      const int tricksRemaining = RemainingTricksInWorld(state,
        state.worlds[worldIndex]);
      const int maxAdditional =
        (SeatSide(state.playerToMove) == state.maxSide ?
          best : tricksRemaining - best);
      vec.values[worldIndex] = state.maxTricksWon + maxAdditional;
    }

    front.Insert(vec);
    return front;
  }


  static int BridgeDepthCost(
    const BridgeState& state,
    const BridgeState& child)
  {
    return (state.currentTrick.size() == 3 && child.currentTrick.empty() ? 1 : 0);
  }


  static ParetoFront SearchBridgeState(
    const BridgeState& state,
    const int tricksRemaining)
  {
    if (state.possibleWorlds.Empty())
      return MakeZeroFront(state.possibleWorlds.count);

    if (tricksRemaining <= 0)
      return MakeBridgeDDSLeafFront(state);

    const vector<BridgeChild> children = ExpandBridgeChildren(state);
    if (children.empty())
      return MakeBridgeDDSLeafFront(state);

    if (SeatSide(state.playerToMove) == state.maxSide)
    {
      ParetoFront front(state.possibleWorlds.count);
      for (unsigned i = 0; i < children.size(); i++)
      {
        const int nextDepth = tricksRemaining - BridgeDepthCost(state,
          children[i].state);
        front = ParetoFront::MaxMerge(front,
          SearchBridgeState(children[i].state, nextDepth));
      }
      return front;
    }

    ParetoFront front(state.possibleWorlds.count);
    bool initialized = false;
    for (unsigned i = 0; i < children.size(); i++)
    {
      const int nextDepth = tricksRemaining - BridgeDepthCost(state,
        children[i].state);
      const ParetoFront childFront = SearchBridgeState(
        children[i].state,
        nextDepth);
      if (! initialized)
      {
        front = childFront;
        initialized = true;
      }
      else
        front = ParetoFront::MinProduct(front, childFront);
    }
    return front;
  }


  static bool WorldMatchesConstraint(
    const ParsedWorld& world,
    const WorldConstraint& constraint)
  {
    switch (constraint.kind)
    {
      case CONSTRAINT_HAS_CARD:
        return WorldHasCard(world, constraint.player, constraint.suit,
          constraint.rank);

      case CONSTRAINT_VOID_SUIT:
        return WorldSuitLength(world, constraint.player, constraint.suit) == 0;

      case CONSTRAINT_MIN_LENGTH:
        return WorldSuitLength(world, constraint.player, constraint.suit) >=
          constraint.count;

      case CONSTRAINT_MAX_LENGTH:
        return WorldSuitLength(world, constraint.player, constraint.suit) <=
          constraint.count;

      default:
        throw runtime_error("Unknown world constraint kind");
    }
  }


  static bool WorldMatchesAllConstraints(
    const ParsedWorld& world,
    const vector<WorldConstraint>& constraints)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      if (! WorldMatchesConstraint(world, constraints[i]))
        return false;
    }
    return true;
  }


  static WorldMask FilterWorldsByConstraints(
    const vector<ParsedWorld>& worlds,
    const WorldMask& candidates,
    const vector<WorldConstraint>& constraints)
  {
    if (constraints.empty())
      return candidates;

    WorldMask mask = WorldMask::None(candidates.count);
    for (unsigned i = 0; i < worlds.size(); i++)
    {
      if (! candidates.Has(i))
        continue;

      if (WorldMatchesAllConstraints(worlds[i], constraints))
        mask.bits |= (1ULL << i);
    }
    return mask;
  }


  static bool WorldCanReplayHistory(
    const ParsedWorld& world,
    const vector<PlayHistoryEvent>& history)
  {
    ParsedWorld replay(world);
    for (unsigned i = 0; i < history.size(); i++)
    {
      const PlayHistoryEvent& event = history[i];
      if (! WorldHasCard(replay, event.player, event.move.suit, event.move.rank))
        return false;

      if (event.leadSuit >= 0 &&
          event.move.suit != event.leadSuit &&
          WorldSuitLength(replay, event.player, event.leadSuit) > 0)
      {
        return false;
      }

      RemoveCardFromWorld(replay, event.player, event.move);
    }
    return true;
  }


  static WorldMask FilterWorldsByHistory(
    const vector<ParsedWorld>& worlds,
    const WorldMask& candidates,
    const vector<PlayHistoryEvent>& history)
  {
    if (history.empty())
      return candidates;

    WorldMask mask = WorldMask::None(candidates.count);
    for (unsigned i = 0; i < worlds.size(); i++)
    {
      if (! candidates.Has(i))
        continue;

      if (WorldCanReplayHistory(worlds[i], history))
        mask.bits |= (1ULL << i);
    }
    return mask;
  }


  static WorldMask DeduplicateWorldMask(
    const vector<ParsedWorld>& worlds,
    const WorldMask& candidates,
    unsigned& duplicatesRemoved)
  {
    map<string, unsigned> firstSeen;
    WorldMask deduped = WorldMask::None(candidates.count);
    duplicatesRemoved = 0;

    for (unsigned i = 0; i < worlds.size(); i++)
    {
      if (! candidates.Has(i))
        continue;

      const string key = SerializePBNWorld(worlds[i]);
      if (firstSeen.find(key) != firstSeen.end())
      {
        duplicatesRemoved++;
        continue;
      }

      firstSeen[key] = i;
      deduped.bits |= (1ULL << i);
    }

    return deduped;
  }


  static WorldMask GeneratePossibleWorlds(
    const vector<ParsedWorld>& worlds,
    const BridgeInformationState& information,
    WorldGenerationStats* stats)
  {
    WorldMask mask = WorldMask::All(static_cast<unsigned>(worlds.size()));
    if (stats != NULL)
      stats->candidateWorldCount = mask.PopCount();

    mask = FilterWorldsByConstraints(worlds, mask,
      information.knownCardConstraints);
    if (stats != NULL)
      stats->afterKnownCardCount = mask.PopCount();

    mask = FilterWorldsByConstraints(worlds, mask,
      information.biddingConstraints);
    if (stats != NULL)
      stats->afterBiddingCount = mask.PopCount();

    mask = FilterWorldsByHistory(worlds, mask, information.playHistory);
    if (stats != NULL)
      stats->afterPlayHistoryCount = mask.PopCount();

    mask = FilterWorldsByHistory(worlds, mask, information.currentTrickHistory);
    if (stats != NULL)
      stats->afterCurrentTrickCount = mask.PopCount();

    if (information.deduplicateEquivalentWorlds)
    {
      unsigned duplicatesRemoved = 0;
      mask = DeduplicateWorldMask(worlds, mask, duplicatesRemoved);
      if (stats != NULL)
        stats->duplicateWorldsRemoved = duplicatesRemoved;
    }

    if (stats != NULL)
      stats->finalWorldCount = mask.PopCount();
    return mask;
  }


  static WorldMask GeneratePossibleWorlds(
    const vector<ParsedWorld>& worlds,
    const vector<WorldConstraint>& constraints)
  {
    BridgeInformationState information;
    information.knownCardConstraints = constraints;
    return GeneratePossibleWorlds(worlds, information, NULL);
  }


  static OutcomeVector MakeBinaryOutcome(
    const string& text)
  {
    const unsigned n = static_cast<unsigned>(text.size());
    OutcomeVector vec(n);
    for (unsigned i = 0; i < n; i++)
    {
      if (text[i] == '0' || text[i] == '1')
      {
        vec.valid.bits |= (1ULL << i);
        vec.values[i] = text[i] - '0';
      }
      else if (text[i] == 'x' || text[i] == 'X' || text[i] == '?')
        continue;
      else
        throw runtime_error("MakeBinaryOutcome expects only 0/1/x text");
    }
    return vec;
  }


  static void AddChild(
    ToyNode& parent,
    const ToyNode& child,
    const WorldMask& worlds)
  {
    parent.children.push_back(&child);
    parent.childWorlds.push_back(worlds);
  }


  static void AddChild(
    ToyNode& parent,
    const ToyNode& child)
  {
    AddChild(parent, child, WorldMask::All(parent.leafFront.worldCount));
  }


  static string MakeTTKey(
    const ToyNode& node,
    const int maxMoves,
    const WorldMask& usefulWorlds)
  {
    ostringstream oss;
    oss << node.name << "|" << maxMoves << "|" << usefulWorlds.count << "|"
        << usefulWorlds.bits;
    return oss.str();
  }


  static ParetoFront MakeFront(
    const unsigned worldCount,
    const vector<string>& outcomes)
  {
    ParetoFront front(worldCount);
    for (unsigned i = 0; i < outcomes.size(); i++)
      front.Insert(MakeBinaryOutcome(outcomes[i]));
    return front;
  }


  static string ResolvePath(const string& candidate)
  {
    FILE * fp = fopen(candidate.c_str(), "r");
    if (fp != NULL)
    {
      fclose(fp);
      return candidate;
    }

    const string prefixed = "../" + candidate;
    fp = fopen(prefixed.c_str(), "r");
    if (fp != NULL)
    {
      fclose(fp);
      return prefixed;
    }

    return candidate;
  }


  static ParetoFront MakeZeroFront(const unsigned worldCount)
  {
    ParetoFront front(worldCount);
    OutcomeVector vec(worldCount);
    vec.valid = WorldMask::All(worldCount);
    front.Insert(vec);
    return front;
  }


  static ParetoFront MakeSingleWorldFront(
    const unsigned worldCount,
    const unsigned world,
    const int value)
  {
    ParetoFront front(worldCount);
    OutcomeVector vec(worldCount);
    vec.valid = WorldMask(worldCount, 1ULL << world);
    vec.values[world] = value;
    front.Insert(vec);
    return front;
  }


  static unsigned SoleWorldIndex(const WorldMask& mask)
  {
    for (unsigned i = 0; i < mask.count; i++)
    {
      if (mask.Has(i))
        return i;
    }

    throw runtime_error("SoleWorldIndex called on empty mask");
  }


  static int EvaluateLeafWorld(
    const ParetoFront& front,
    const unsigned world)
  {
    int best = 0;
    for (unsigned i = 0; i < front.vectors.size(); i++)
    {
      if (front.vectors[i].valid.Has(world))
        best = max(best, front.vectors[i].values[world]);
    }
    return best;
  }


  static int EvaluateSingleWorld(
    const ToyNode& node,
    const int maxMoves,
    const unsigned world,
    SearchStats& stats)
  {
    if (node.type == TOY_LEAF || maxMoves == 0)
    {
      stats.leafWorldEvaluations++;
      return EvaluateLeafWorld(node.leafFront, world);
    }

    if (node.type == TOY_MIN)
    {
      int best = 1;
      bool found = false;
      for (unsigned i = 0; i < node.children.size(); i++)
      {
        if (! node.childWorlds[i].Has(world))
          continue;

        const int value = EvaluateSingleWorld(
          * node.children[i],
          maxMoves,
          world,
          stats);
        found = true;
        best = min(best, value);
      }
      if (! found)
        return 0;
      return best;
    }

    int best = 0;
    for (unsigned i = 0; i < node.children.size(); i++)
    {
      if (! node.childWorlds[i].Has(world))
        continue;

      const int value = EvaluateSingleWorld(
        * node.children[i],
        maxMoves - 1,
        world,
        stats);
      best = max(best, value);
    }
    return best;
  }


  static ParetoFront SearchToy(
    const ToyNode& node,
    const int maxMoves,
    const WorldMask& usefulWorlds,
    const vector<const ParetoFront *>& upperMaxFronts,
    const OutcomeVector& optimisticValues,
    TranspositionTable& tt,
    const bool isRoot,
    const double previousRootMu,
    SearchStats& stats,
    bool& rootCutTriggered,
    bool& exactComplete)
  {
    stats.nodesVisited++;
    stats.visitOrder.push_back(node.name);

    const string ttKey = MakeTTKey(node, maxMoves, usefulWorlds);
    ParetoFront ttFront(node.leafFront.worldCount);
    if (tt.Lookup(ttKey, ttFront))
    {
      stats.ttHits++;
      exactComplete = true;
      return ttFront;
    }

    if (usefulWorlds.Empty())
    {
      stats.worldCutsZero++;
      exactComplete = true;
      return MakeZeroFront(node.leafFront.worldCount);
    }

    if (usefulWorlds.PopCount() == 1)
    {
      const unsigned world = SoleWorldIndex(usefulWorlds);
      const int value = EvaluateSingleWorld(node, maxMoves, world, stats);
      stats.worldCutsSingle++;
      exactComplete = true;
      return MakeSingleWorldFront(node.leafFront.worldCount, world, value);
    }

    OutcomeVector nodeOptimistic(optimisticValues);
    for (unsigned i = 0; i < nodeOptimistic.values.size(); i++)
    {
      if (node.optimisticValues.valid.Has(i))
      {
        nodeOptimistic.valid.bits |= (1ULL << i);
        nodeOptimistic.values[i] = node.optimisticValues.values[i];
      }
    }

    if (node.type == TOY_LEAF || maxMoves == 0)
    {
      const WorldMask evaluated = node.leafFront.ValidWorlds().Intersection(
        usefulWorlds);
      stats.leafWorldEvaluations += static_cast<int>(evaluated.PopCount());
      const ParetoFront front = node.leafFront.RestrictToUseful(usefulWorlds);
      tt.Store(ttKey, front);
      stats.ttStores++;
      exactComplete = true;
      return front;
    }

    if (node.type == TOY_MIN)
    {
      ParetoFront mini(node.leafFront.worldCount);
      bool initialized = false;
      bool complete = true;
      WorldMask currentUseful = usefulWorlds;

      for (unsigned i = 0; i < node.children.size(); i++)
      {
        bool childComplete = false;
        const ParetoFront f = SearchToy(
          * node.children[i],
          maxMoves,
          currentUseful.Intersection(node.childWorlds[i]),
          upperMaxFronts,
          nodeOptimistic,
          tt,
          false,
          previousRootMu,
          stats,
          rootCutTriggered,
          childComplete);
        complete = complete && childComplete;

        if (! initialized)
        {
          mini = f;
          initialized = true;
        }
        else
          mini = ParetoFront::MinProduct(mini, f);

        const WorldMask nextUseful = currentUseful.Intersection(
          mini.UsefulWorlds());
        if (! (nextUseful == currentUseful))
          stats.usefulWorldUpdates++;
        currentUseful = nextUseful;

        const ParetoFront optimisticMini = mini.CompleteOptimistically(
          currentUseful,
          nodeOptimistic);
        if (optimisticMini.ValidWorlds().PopCount() > mini.ValidWorlds().PopCount())
          stats.optimisticCompletions++;

        if (! upperMaxFronts.empty() &&
            upperMaxFronts.back()->DominatesFront(optimisticMini))
        {
          stats.earlyCuts++;
          complete = false;
          exactComplete = false;
          break;
        }

        for (unsigned j = 0; j + 1 < upperMaxFronts.size(); j++)
        {
          if (upperMaxFronts[j]->DominatesFront(optimisticMini))
          {
            stats.deepAlphaCuts++;
            complete = false;
            exactComplete = false;
            return mini;
          }
        }
      }

      exactComplete = complete;
      if (exactComplete)
      {
        tt.Store(ttKey, mini);
        stats.ttStores++;
      }
      return mini;
    }

    ParetoFront front(node.leafFront.worldCount);
    bool complete = true;
    vector<const ParetoFront *> childUpperMaxFronts(upperMaxFronts);
    childUpperMaxFronts.push_back(&front);
    for (unsigned i = 0; i < node.children.size(); i++)
    {
      const WorldMask childWorlds = usefulWorlds.Intersection(node.childWorlds[i]);
      bool childComplete = false;
      const ParetoFront f = SearchToy(
        * node.children[i],
        maxMoves - 1,
        childWorlds,
        childUpperMaxFronts,
        nodeOptimistic,
        tt,
        false,
        previousRootMu,
        stats,
        rootCutTriggered,
        childComplete);
      complete = complete && childComplete;

      front = ParetoFront::MaxMerge(front, f);

      if (f.WinsAll(usefulWorlds))
      {
        stats.cutOnWinCuts++;
        complete = false;
        exactComplete = false;
        break;
      }

      if (isRoot && previousRootMu >= 0.0 &&
          fabs(front.Mu() - previousRootMu) < 1e-9)
      {
        stats.rootCuts++;
        rootCutTriggered = true;
        complete = false;
        exactComplete = false;
        break;
      }
    }

    exactComplete = complete;
    if (exactComplete)
    {
      tt.Store(ttKey, front);
      stats.ttStores++;
    }
    return front;
  }


  static IterativeResult RunIterativeDeepening(
    const ToyNode& root,
    const int maxDepth)
  {
    IterativeResult result(root.leafFront.worldCount);
    double previousMu = -1.0;

    for (int depth = 1; depth <= maxDepth; depth++)
    {
      SearchStats stats;
      bool rootCutTriggered = false;
      bool exactComplete = false;
      TranspositionTable tt;
      const ParetoFront front = SearchToy(
        root,
        depth,
        WorldMask::All(root.leafFront.worldCount),
        vector<const ParetoFront *>(),
        OutcomeVector(root.leafFront.worldCount),
        tt,
        true,
        previousMu,
        stats,
        rootCutTriggered,
        exactComplete);

      result.front = front;
      result.depthReached = depth;
      result.rootCutTriggered = rootCutTriggered;
      result.statsPerDepth.push_back(stats);
      previousMu = front.Mu();

      if (rootCutTriggered)
        break;
    }

    return result;
  }


  static int BestScore(const futureTricks& fut)
  {
    if (fut.cards <= 0)
      return 0;

    int best = fut.score[0];
    for (int i = 1; i < fut.cards; i++)
      best = max(best, fut.score[i]);
    return best;
  }


  static void CheckDDS(const int ret, const string& tag)
  {
    if (ret == RETURN_NO_FAULT)
      return;

    char line[80];
    ErrorMessage(ret, line);
    ostringstream oss;
    oss << tag << " failed: " << line;
    Fail(oss.str());
  }


  static int ExactBridgeDDSScoreForWorld(
    const BridgeState& state,
    const unsigned worldIndex)
  {
    futureTricks fut;
    memset(&fut, 0, sizeof(fut));

    const dealPBN deal = MakeDDSDealPBN(state, state.worlds[worldIndex]);
    const int ret = SolveBoardPBN(deal, -1, 1, 1, &fut, 0);
    CheckDDS(ret, "SolveBoardPBN exact bridge DDS score");

    const int best = BestScore(fut);
    const int tricksRemaining = RemainingTricksInWorld(state,
      state.worlds[worldIndex]);
    const int maxAdditional =
      (SeatSide(state.playerToMove) == state.maxSide ?
        best : tricksRemaining - best);
    return state.maxTricksWon + maxAdditional;
  }


  static void LoadHandFile(
    const string& fname,
    HandFileData& data)
  {
    const string path = ResolvePath(fname);
    if (! read_file(path,
        data.number,
        data.GIBmode,
        &data.dealerList,
        &data.vulList,
        &data.dealList,
        &data.futList,
        &data.tableList,
        &data.parList,
        &data.dealerParList,
        &data.playList,
        &data.traceList))
    {
      Fail("read_file failed for " + path);
    }
  }


  static int SolveDDSLeafWorld(
    const HandFileData& data,
    const int index,
    const int thrId)
  {
    futureTricks fut;
    memset(&fut, 0, sizeof(fut));

    const int ret = SolveBoardPBN(data.dealList[index], -1, 1, 1, &fut, thrId);
    CheckDDS(ret, "SolveBoardPBN DDS leaf demo");

    return BestScore(fut);
  }


  static void CheckDDSLeafBestScores(
    const HandFileData& data,
    const vector<int>& bestScores)
  {
    Check(static_cast<int>(bestScores.size()) == data.number,
      "DDS leaf evaluation should return one score per world");

    for (int i = 0; i < data.number; i++)
    {
      const int goldenBest = BestScore(data.futList[i]);
      Check(bestScores[static_cast<unsigned>(i)] == goldenBest,
        "DDS leaf demo optimum should match the golden FUT optimum");
    }
  }


  static DDSLeafEvalResult EvaluateDDSLeafThresholdSerial(
    const HandFileData& data,
    const int target)
  {
    DDSLeafEvalResult result(static_cast<unsigned>(data.number));
    result.leaf.valid = WorldMask::All(static_cast<unsigned>(data.number));
    result.workerCount = 1;

    for (int i = 0; i < data.number; i++)
    {
      const int best = SolveDDSLeafWorld(data, i, 0);
      result.bestScores[static_cast<unsigned>(i)] = best;
      result.leaf.values[static_cast<unsigned>(i)] = (best >= target ? 1 : 0);
    }

    CheckDDSLeafBestScores(data, result.bestScores);
    return result;
  }


  static DDSLeafEvalResult EvaluateDDSLeafThresholdParallel(
    const HandFileData& data,
    const int target,
    const int requestedThreads)
  {
    DDSLeafEvalResult result(static_cast<unsigned>(data.number));
    result.leaf.valid = WorldMask::All(static_cast<unsigned>(data.number));

    DDSInfo info;
    memset(&info, 0, sizeof(info));
    GetDDSInfo(&info);

    (void) requestedThreads;
    Check(info.noOfThreads >= 1,
      "DDS should expose at least one configured thread for leaf parallelization");
    result.workerCount = max(1, min(data.number, info.noOfThreads));

    boardsPBN boards;
    memset(&boards, 0, sizeof(boards));
    boards.noOfBoards = data.number;

    for (int i = 0; i < data.number; i++)
    {
      boards.deals[i] = data.dealList[i];
      boards.target[i] = -1;
      boards.solutions[i] = 1;
      boards.mode[i] = 1;
    }

    solvedBoards solved;
    memset(&solved, 0, sizeof(solved));

    const int ret = SolveAllBoards(&boards, &solved);
    CheckDDS(ret, "SolveAllBoards DDS leaf demo");
    Check(solved.noOfBoards == data.number,
      "parallel DDS leaf evaluation should solve every requested world");

    for (int i = 0; i < data.number; i++)
    {
      const int best = BestScore(solved.solvedBoard[i]);
      result.bestScores[static_cast<unsigned>(i)] = best;
      result.leaf.values[static_cast<unsigned>(i)] = (best >= target ? 1 : 0);
    }

    CheckDDSLeafBestScores(data, result.bestScores);
    return result;
  }


  static void TestParetoInsert()
  {
    ParetoFront front(3);
    front.Insert(MakeBinaryOutcome("100"));
    front.Insert(MakeBinaryOutcome("011"));
    front.Insert(MakeBinaryOutcome("110"));

    Check(front.vectors.size() == 2,
      "Pareto insert should remove dominated vectors");
    Check(FrontContains(front, MakeBinaryOutcome("110")),
      "front should contain [1 1 0]");
    Check(FrontContains(front, MakeBinaryOutcome("011")),
      "front should contain [0 1 1]");
  }


  static void TestNonLocalityExample()
  {
    ToyNode leaf100("leaf100", TOY_LEAF, 3);
    leaf100.leafFront = MakeFront(3, vector<string>(1, "100"));

    ToyNode leaf011("leaf011", TOY_LEAF, 3);
    leaf011.leafFront = MakeFront(3, vector<string>(1, "011"));

    ToyNode leaf000a("leaf000a", TOY_LEAF, 3);
    leaf000a.leafFront = MakeFront(3, vector<string>(1, "000"));

    ToyNode leaf000b("leaf000b", TOY_LEAF, 3);
    leaf000b.leafFront = MakeFront(3, vector<string>(1, "000"));

    ToyNode d("d", TOY_MAX, 3);
    AddChild(d, leaf100);
    AddChild(d, leaf011);

    ToyNode e("e", TOY_MAX, 3);
    AddChild(e, leaf000a);
    AddChild(e, leaf100);

    ToyNode b("b", TOY_MIN, 3);
    AddChild(b, d);
    AddChild(b, e);

    ToyNode f("f", TOY_MAX, 3);
    AddChild(f, leaf000b);

    ToyNode c("c", TOY_MIN, 3);
    AddChild(c, f);

    ToyNode a("a", TOY_MAX, 3);
    AddChild(a, b);
    AddChild(a, c);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(a, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(front.vectors.size() == 1,
      "non-locality example should collapse to one best root vector");
    Check(FrontContains(front, MakeBinaryOutcome("100")),
      "non-locality example should prefer [1 0 0] at the root");
    Check(! rootCutTriggered,
      "non-locality example should not need a root cut");
  }


  static void TestEarlyCutExample()
  {
    ToyNode bestLeaf("bestLeaf", TOY_LEAF, 3);
    {
      vector<string> fronts;
      fronts.push_back("110");
      fronts.push_back("011");
      bestLeaf.leafFront = MakeFront(3, fronts);
    }

    ToyNode cutLeaf1("cutLeaf1", TOY_LEAF, 3);
    cutLeaf1.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode shouldNotVisit("shouldNotVisit", TOY_LEAF, 3);
    shouldNotVisit.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode candidateMin("candidateMin", TOY_MIN, 3);
    AddChild(candidateMin, cutLeaf1);
    AddChild(candidateMin, shouldNotVisit);

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, bestLeaf);
    AddChild(root, candidateMin);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(front.vectors.size() == 2,
      "early-cut example should preserve the first move's Pareto front");
    Check(stats.earlyCuts == 1,
      "early-cut example should trigger exactly one early cut");
    Check(find(stats.visitOrder.begin(), stats.visitOrder.end(),
      string("shouldNotVisit")) == stats.visitOrder.end(),
      "early cut should stop before visiting the dominated continuation");
    Check(! rootCutTriggered,
      "early-cut example should not trigger a root cut");
  }


  static void TestUsefulWorldMaintenance()
  {
    ToyNode minFirst("minFirst", TOY_LEAF, 3);
    minFirst.leafFront = MakeFront(3, vector<string>(1, "101"));

    ToyNode minSecond("minSecond", TOY_LEAF, 3);
    minSecond.leafFront = MakeFront(3, vector<string>(1, "010"));

    ToyNode candidateMin("candidateMin", TOY_MIN, 3);
    AddChild(candidateMin, minFirst);
    AddChild(candidateMin, minSecond);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(candidateMin, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, false, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(FrontContains(front, MakeBinaryOutcome("000")),
      "useful-world example should reduce the Min continuation to [0 0 0]");
    Check(stats.usefulWorldUpdates == 2,
      "useful-world example should record two useful-world updates at the Min node");
    Check(stats.leafWorldEvaluations == 5,
      "useful-world example should evaluate only 5 leaf worlds instead of 6");
    Check(! rootCutTriggered,
      "useful-world example should not trigger a root cut");
  }


  static void TestRootCutExample()
  {
    ToyNode stableBest("stableBest", TOY_LEAF, 3);
    stableBest.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode skippedByRootCut("skippedByRootCut", TOY_LEAF, 3);
    skippedByRootCut.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, stableBest);
    AddChild(root, skippedByRootCut);

    const IterativeResult result = RunIterativeDeepening(root, 2);

    Check(result.depthReached == 2,
      "iterative deepening should reach the second depth before cutting");
    Check(result.rootCutTriggered,
      "root-cut example should trigger a root cut");
    Check(result.statsPerDepth.size() == 2,
      "iterative deepening should record two depth passes");
    Check(result.statsPerDepth[1].rootCuts == 1,
      "second depth pass should contain one root cut");
    Check(find(result.statsPerDepth[1].visitOrder.begin(),
      result.statsPerDepth[1].visitOrder.end(),
      string("skippedByRootCut")) == result.statsPerDepth[1].visitOrder.end(),
      "root cut should stop before the second root move is searched");
  }


  static void TestWorldCuts()
  {
    ToyNode zeroLeaf("zeroLeaf", TOY_LEAF, 3);
    zeroLeaf.leafFront = MakeFront(3, vector<string>(1, "101"));

    SearchStats zeroStats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront zeroFront = SearchToy(
      zeroLeaf,
      1,
      WorldMask::None(3),
      vector<const ParetoFront *>(),
      OutcomeVector(3),
      tt,
      false,
      -1.0,
      zeroStats,
      rootCutTriggered,
      exactComplete);

    Check(zeroStats.worldCutsZero == 1,
      "empty useful-world mask should trigger a zero-world cut");
    Check(zeroStats.leafWorldEvaluations == 0,
      "zero-world cut should avoid all leaf evaluations");
    Check(FrontContains(zeroFront, MakeBinaryOutcome("000")),
      "zero-world cut should return the all-zero vector");

    ToyNode left("left", TOY_LEAF, 3);
    left.leafFront = MakeFront(3, vector<string>(1, "010"));

    ToyNode right("right", TOY_LEAF, 3);
    right.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, left);
    AddChild(root, right);

    SearchStats singleStats;
    const WorldMask onlyWorld1(3, 1ULL << 1);
    const ParetoFront singleFront = SearchToy(
      root,
      1,
      onlyWorld1,
      vector<const ParetoFront *>(),
      OutcomeVector(3),
      tt,
      true,
      -1.0,
      singleStats,
      rootCutTriggered,
      exactComplete);

    Check(singleStats.worldCutsSingle == 1,
      "single useful world should trigger a single-world cut");
    Check(singleStats.leafWorldEvaluations == 2,
      "single-world cut should evaluate only one world through the collapsed search");
    Check(FrontContains(singleFront, MakeBinaryOutcome("x1x")),
      "single-world cut should return the exact one-world result as a sparse vector");
  }


  static void TestParetoFrontTT()
  {
    ToyNode sharedLeaf("sharedLeaf", TOY_LEAF, 3);
    sharedLeaf.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, sharedLeaf);
    AddChild(root, sharedLeaf);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 1, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(exactComplete,
      "transposition-table example should complete the shared subtree exactly");
    Check(stats.ttHits == 1,
      "transposition-table example should record exactly one TT hit on the repeated subtree");
    Check(stats.leafWorldEvaluations == 3,
      "transposition-table example should evaluate the shared leaf only once");
    Check(stats.ttStores >= 2,
      "transposition-table example should store both subtree and root fronts");
    Check(tt.entries.size() >= 2,
      "transposition-table example should keep at least the shared leaf and root entries");
    Check(FrontContains(front, MakeBinaryOutcome("110")),
      "transposition-table example should preserve the repeated leaf outcome");
    Check(! rootCutTriggered,
      "transposition-table example should not trigger a root cut");
  }


  static void TestPossibleWorldGeneration()
  {
    vector<string> pbns;
    pbns.push_back(
      "N:AKQ2.JT9.AKQ.JT9 765.8765.JT9.876 JT98.AKQ.432.AKQ 43.432.8765.5432");
    pbns.push_back(
      "N:AKQ2.JT9.AKQ.JT9 7654.876.JT9.876 JT98.AKQ.432.AKQ 3.5432.8765.5432");
    pbns.push_back(
      "N:AKQ2.JT9.AKQ.JT9 76543.876.JT.876 JT98.AKQ.432.AKQ .5432.98765.5432");
    pbns.push_back(
      "N:AKQ2.JT9.AKQ.JT9 76543..JT987.876 JT98.AKQ.432.AKQ .8765432.65.5432");

    vector<ParsedWorld> worlds;
    for (unsigned i = 0; i < pbns.size(); i++)
      worlds.push_back(ParsePBNWorld(pbns[i]));

    Check(WorldHasCard(worlds[0], SEAT_NORTH, SUIT_SPADES, 'A'),
      "possible-world parser should preserve known declarer cards");
    Check(WorldSuitLength(worlds[3], SEAT_WEST, SUIT_SPADES) == 0,
      "possible-world parser should preserve empty suits");

    BridgeInformationState biddingInfo;
    biddingInfo.biddingConstraints.push_back(WorldConstraint::MinLength(
      SEAT_EAST, SUIT_SPADES, 5));
    WorldGenerationStats biddingStats;
    const WorldMask biddingMask = GeneratePossibleWorlds(worlds, biddingInfo,
      &biddingStats);
    Check(biddingMask == WorldMask(4, 0xCU),
      "bidding-style spade-length constraint should keep the last two worlds");
    Check(biddingStats.candidateWorldCount == 4,
      "world-generation stats should count the initial candidate pool");
    Check(biddingStats.afterBiddingCount == 2,
      "world-generation stats should record the post-bidding surviving worlds");

    BridgeInformationState playInfo;
    playInfo.knownCardConstraints.push_back(
      WorldConstraint::HasCard(SEAT_EAST, SUIT_DIAMONDS, '8'));
    playInfo.biddingConstraints.push_back(
      WorldConstraint::VoidSuit(SEAT_WEST, SUIT_SPADES));
    WorldGenerationStats playStats;
    const WorldMask playMask = GeneratePossibleWorlds(worlds, playInfo,
      &playStats);
    Check(playMask == WorldMask(4, 0x8U),
      "play-style void and card-location constraints should isolate the final world");
    Check(playStats.afterKnownCardCount == 1,
      "world-generation stats should record the known-card filter before later stages");
    Check(playStats.finalWorldCount == 1,
      "world-generation stats should record the final surviving world count");

    BridgeInformationState combinedInfo;
    combinedInfo.knownCardConstraints.push_back(
      WorldConstraint::HasCard(SEAT_EAST, SUIT_DIAMONDS, '8'));
    combinedInfo.biddingConstraints.push_back(
      WorldConstraint::MinLength(SEAT_EAST, SUIT_SPADES, 5));
    combinedInfo.biddingConstraints.push_back(
      WorldConstraint::MaxLength(SEAT_EAST, SUIT_HEARTS, 0));
    const WorldMask combinedMask = GeneratePossibleWorlds(worlds, combinedInfo,
      NULL);
    Check(combinedMask == WorldMask(4, 0x8U),
      "combined bidding/play constraints should identify a single possible world");

    vector<ParsedWorld> duplicateWorlds(worlds);
    duplicateWorlds.push_back(worlds[3]);
    BridgeInformationState dedupInfo;
    dedupInfo.biddingConstraints.push_back(WorldConstraint::MinLength(
      SEAT_EAST, SUIT_SPADES, 5));
    dedupInfo.deduplicateEquivalentWorlds = true;
    WorldGenerationStats dedupStats;
    const WorldMask dedupMask = GeneratePossibleWorlds(duplicateWorlds,
      dedupInfo, &dedupStats);
    Check(dedupStats.duplicateWorldsRemoved == 1,
      "world-generation should remove one duplicate world after staged filtering");
    Check(dedupMask == WorldMask(5, 0xCU),
      "world deduplication should keep the first equivalent surviving world and drop later duplicates");
  }


  static void TestPlayHistoryFiltering()
  {
    vector<ParsedWorld> worlds;
    worlds.push_back(ParsePBNWorld("N:A... K.Q.. 2... 3..."));
    worlds.push_back(ParsePBNWorld("N:A... .Q.. K... 3..."));
    worlds.push_back(ParsePBNWorld("N:A... .Q.. K... 3..."));

    BridgeInformationState info;
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_HEARTS, 'Q')));
    info.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    info.deduplicateEquivalentWorlds = true;

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(worlds, info, &stats);

    Check(mask == WorldMask(3, 0x2ULL),
      "play-history filtering should keep only the world where East could legally fail to follow spades and duplicate removal should keep the first equivalent survivor");
    Check(stats.afterPlayHistoryCount == 2,
      "play-history filtering should leave exactly the two equivalent legal worlds before deduplication");
    Check(stats.afterCurrentTrickCount == 2,
      "current-trick filtering should preserve worlds that can replay the current partial trick history");
    Check(stats.duplicateWorldsRemoved == 1,
      "play-history world generation should report duplicate removal after legality filtering");
    Check(stats.finalWorldCount == 1,
      "play-history world generation should finish with one deduplicated surviving world");
  }


  static void TestBridgeMoveGeneration()
  {
    BridgeState state;
    state.playerToMove = SEAT_EAST;
    state.leadSuit = SUIT_DIAMONDS;
    state.possibleWorlds = WorldMask(2, 0x3ULL);

    state.worlds.push_back(ParsePBNWorld(
      "N:AKQ2.JT9.AKQ.JT9 76543.876.JT.876 JT98.AKQ.432.AKQ .5432.98765.5432"));
    state.worlds.push_back(ParsePBNWorld(
      "N:AKQ2.JT9.AKQ.JT9 76543..JT987.876 JT98.AKQ.432.AKQ .8765432.65.5432"));

    const vector<BridgeMove> moves = GenerateBridgeMoves(state);
    Check(moves.size() == 5,
      "bridge move generation should union legal diamond plays across possible worlds");
    Check(moves[0] == BridgeMove(SUIT_DIAMONDS, '7'),
      "bridge move generation should include the lowest legal diamond from the union");
    Check(moves[4] == BridgeMove(SUIT_DIAMONDS, 'J'),
      "bridge move generation should include the highest legal diamond from the union");

    const BridgeState afterD8 = PlayBridgeMove(state,
      BridgeMove(SUIT_DIAMONDS, '8'));
    Check(afterD8.possibleWorlds == WorldMask(2, 0x2ULL),
      "playing D8 should eliminate the world where East could not legally play that diamond");
    Check(afterD8.playerToMove == SEAT_SOUTH,
      "bridge move generation should advance turn order after a play");
    Check(afterD8.leadSuit == SUIT_DIAMONDS,
      "bridge move generation should preserve the established lead suit within the trick");
    Check(! WorldHasCard(afterD8.worlds[1], SEAT_EAST, SUIT_DIAMONDS, '8'),
      "bridge move application should remove the played card from surviving worlds");

    const vector<BridgeChild> children = ExpandBridgeChildren(state);
    Check(children.size() == moves.size(),
      "bridge child expansion should create one child per legal bridge move");
    Check(children[1].move == BridgeMove(SUIT_DIAMONDS, '8'),
      "bridge child expansion should preserve move ordering");
    Check(children[1].state.possibleWorlds == WorldMask(2, 0x2ULL),
      "bridge child expansion should carry the filtered possible-world mask into the child state");
  }


  static void TestBridgeSearchControl()
  {
    BridgeState state;
    state.playerToMove = SEAT_NORTH;
    state.maxSide = 0;
    state.trumpSuit = -1;
    state.possibleWorlds = WorldMask(2, 0x3ULL);

    state.worlds.push_back(ParsePBNWorld("N:A... K... 2... 3..."));
    state.worlds.push_back(ParsePBNWorld("N:Q... K... A... 3..."));

    const vector<BridgeMove> moves = GenerateBridgeMoves(state);
    Check(moves.size() == 2,
      "bridge search control should offer the union of North's possible opening leads");
    Check(moves[0] == BridgeMove(SUIT_SPADES, 'Q'),
      "bridge search control should order opening leads by rank");
    Check(moves[1] == BridgeMove(SUIT_SPADES, 'A'),
      "bridge search control should include the alternative opening lead");

    BridgeState manual = PlayBridgeMove(state, BridgeMove(SUIT_SPADES, 'Q'));
    manual = PlayBridgeMove(manual, BridgeMove(SUIT_SPADES, 'K'));
    manual = PlayBridgeMove(manual, BridgeMove(SUIT_SPADES, 'A'));
    manual = PlayBridgeMove(manual, BridgeMove(SUIT_SPADES, '3'));

    Check(manual.currentTrick.empty(),
      "bridge search control should clear the trick after the fourth card");
    Check(manual.playerToMove == SEAT_SOUTH,
      "bridge search control should advance to the trick winner after completion");
    Check(manual.maxTricksWon == 1,
      "bridge search control should count a won trick for the Max side");

    const ParetoFront front = SearchBridgeState(state, 1);
    Check(front.vectors.size() == 2,
      "bridge search control should keep one sparse winning vector per viable opening lead");
    Check(FrontContains(front, MakeBinaryOutcome("1x")),
      "bridge search control should keep the lead that wins only in the first world");
    Check(FrontContains(front, MakeBinaryOutcome("x1")),
      "bridge search control should keep the lead that wins only in the second world");
  }


  static void TestBridgeMultiTrickDDSLeaf()
  {
    SetMaxThreads(0);

    HandFileData data;
    LoadHandFile("hands/alpha_mu_play.txt", data);
    Check(data.number >= 1,
      "alpha_mu_play.txt should provide at least one real DDS world for the multi-trick bridge test");

    const int handno = 0;
    BridgeState state;
    state.worlds.push_back(ParsePBNWorld(data.dealList[handno].remainCards));
    state.possibleWorlds = WorldMask(1, 0x1ULL);
    state.playerToMove = data.dealList[handno].first;
    state.maxSide = SeatSide(state.playerToMove);
    state.trumpSuit = (data.dealList[handno].trump == 4 ? -1 :
      data.dealList[handno].trump);
    state.trickLeader = state.playerToMove;
    state.leadSuit = -1;

    const vector<BridgeMove> moves = GenerateBridgeMoves(state);
    Check(! moves.empty(),
      "multi-trick bridge test should expose at least one legal opening move in the real DDS world");

    OutcomeVector optimum(1);
    optimum.valid = WorldMask(1, 0x1ULL);
    optimum.values[0] = BestScore(data.futList[handno]);

    const ParetoFront directLeaf = SearchBridgeState(state, 0);
    Check(directLeaf.vectors.size() == 1,
      "bridge DDS leaf evaluation should collapse to a single exact-score vector in a one-world state");
    Check(FrontContains(directLeaf, optimum),
      "bridge DDS leaf evaluation should match the golden FUT optimum on a real DDS world");

    const ParetoFront front = SearchBridgeState(state, 1);
    Check(front.vectors.size() == 1,
      "one full searched trick plus a DDS bridge leaf should still collapse to a single exact-score vector in a one-world state");
    Check(FrontContains(front, optimum),
      "one full searched trick plus a DDS bridge leaf should preserve the golden FUT optimum on a real DDS world");


    BridgeState multiLeafState;
    multiLeafState.playerToMove = SEAT_NORTH;
    multiLeafState.maxSide = 0;
    multiLeafState.trumpSuit = -1;
    multiLeafState.trickLeader = SEAT_NORTH;
    multiLeafState.leadSuit = -1;
    multiLeafState.possibleWorlds = WorldMask(2, 0x3ULL);
    multiLeafState.worlds.push_back(ParsePBNWorld("N:A... K... 2... 3..."));
    multiLeafState.worlds.push_back(ParsePBNWorld("N:Q... K... A... 3..."));

    OutcomeVector multiLeafExact(2);
    multiLeafExact.valid = WorldMask(2, 0x3ULL);
    multiLeafExact.values[0] = ExactBridgeDDSScoreForWorld(multiLeafState, 0);
    multiLeafExact.values[1] = ExactBridgeDDSScoreForWorld(multiLeafState, 1);
    Check(multiLeafExact.values[0] == 1 && multiLeafExact.values[1] == 1,
      "multi-world bridge DDS leaf should return the expected exact one-trick values before any searched continuation");

    const ParetoFront multiLeafFront = SearchBridgeState(multiLeafState, 0);
    Check(multiLeafFront.vectors.size() == 1,
      "multi-world bridge DDS leaf evaluation should collapse to one exact vector before any searched continuation");
    Check(FrontContains(multiLeafFront, multiLeafExact),
      "multi-world bridge DDS leaf evaluation should match the direct DDS exact trick counts across surviving worlds");

    BridgeState splitState;
    splitState.playerToMove = SEAT_NORTH;
    splitState.maxSide = 0;
    splitState.trumpSuit = -1;
    splitState.trickLeader = SEAT_EAST;
    splitState.leadSuit = SUIT_HEARTS;
    splitState.possibleWorlds = WorldMask(2, 0x3ULL);
    splitState.currentTrick.push_back(BridgeMove(SUIT_HEARTS, 'K'));
    splitState.currentTrick.push_back(BridgeMove(SUIT_HEARTS, 'T'));
    splitState.currentTrick.push_back(BridgeMove(SUIT_HEARTS, 'J'));
    splitState.currentTrickPlayers.push_back(SEAT_EAST);
    splitState.currentTrickPlayers.push_back(SEAT_SOUTH);
    splitState.currentTrickPlayers.push_back(SEAT_WEST);
    splitState.worlds.push_back(ParsePBNWorld("N:K2.A.. J3... A4... Q5..."));
    splitState.worlds.push_back(ParsePBNWorld("N:A2.Q.. Q3... K4... J5..."));

    const vector<BridgeMove> splitMoves = GenerateBridgeMoves(splitState);
    Check(splitMoves.size() == 2,
      "multi-world bridge DDS continuation should expose exactly the two world-distinguishing heart plays");
    Check(splitMoves[0] == BridgeMove(SUIT_HEARTS, 'Q'),
      "multi-world bridge DDS continuation should include the world-1 heart completion");
    Check(splitMoves[1] == BridgeMove(SUIT_HEARTS, 'A'),
      "multi-world bridge DDS continuation should include the world-0 heart completion");

    const ParetoFront splitDirectLeaf = SearchBridgeState(splitState, 0);
    OutcomeVector splitLeaf(2);
    splitLeaf.valid = WorldMask(2, 0x3ULL);
    splitLeaf.values[0] = 3;
    splitLeaf.values[1] = 2;
    Check(splitDirectLeaf.vectors.size() == 1,
      "partial-trick bridge DDS leaf evaluation should collapse to one exact vector before any searched continuation");
    Check(FrontContains(splitDirectLeaf, splitLeaf),
      "partial-trick bridge DDS leaf evaluation should match the exact multi-world continuation trick counts across surviving worlds");

    const ParetoFront splitContinuation = SearchBridgeState(splitState, 2);
    OutcomeVector world0Only(2);
    world0Only.valid = WorldMask(2, 0x1ULL);
    world0Only.values[0] = 3;
    OutcomeVector world1Only(2);
    world1Only.valid = WorldMask(2, 0x2ULL);
    world1Only.values[1] = 2;

    Check(splitContinuation.vectors.size() == 2,
      "multi-world bridge DDS continuation should keep two sparse exact-score vectors after the searched continuation");
    Check(FrontContains(splitContinuation, world0Only),
      "multi-world bridge DDS continuation should preserve the world-0-only exact continuation [3 x]");
    Check(FrontContains(splitContinuation, world1Only),
      "multi-world bridge DDS continuation should preserve the world-1-only exact continuation [x 2]");
  }


  static void TestEmptyEntryInteriorFronts()
  {
    ToyNode bestLeaf("bestLeaf", TOY_LEAF, 3);
    bestLeaf.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode partialA("partialA", TOY_LEAF, 3);
    partialA.leafFront = MakeFront(3, vector<string>(1, "010"));

    ToyNode partialB("partialB", TOY_LEAF, 3);
    partialB.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode shouldNotVisit("shouldNotVisit", TOY_LEAF, 3);
    shouldNotVisit.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode candidateMin("candidateMin", TOY_MIN, 3);
    AddChild(candidateMin, partialA, WorldMask(3, 0x6ULL));
    AddChild(candidateMin, partialB, WorldMask(3, 0x3ULL));
    AddChild(candidateMin, shouldNotVisit);

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, bestLeaf);
    AddChild(root, candidateMin);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(stats.earlyCuts == 1,
      "empty-entry example should trigger one early cut after the interior front is completed");
    Check(find(stats.visitOrder.begin(), stats.visitOrder.end(),
      string("shouldNotVisit")) == stats.visitOrder.end(),
      "empty-entry example should cut before visiting the remaining Min child");
    Check(FrontContains(front, MakeBinaryOutcome("110")),
      "empty-entry example should preserve the dominating root outcome");
    Check(MakeBinaryOutcome("x10").ToString() == "[x 1 0]",
      "empty-entry parsing should support sparse vectors");

    const ParetoFront sparseA = partialA.leafFront.RestrictToUseful(
      WorldMask(3, 0x6ULL));
    const ParetoFront sparseB = partialB.leafFront.RestrictToUseful(
      WorldMask(3, 0x3ULL));
    const ParetoFront combined = ParetoFront::MinProduct(sparseA, sparseB);
    Check(FrontContains(combined, MakeBinaryOutcome("110")),
      "empty-entry example should combine sparse child fronts into [1 1 0]");
    Check(FrontContains(partialA.leafFront.RestrictToUseful(WorldMask(3, 0x6ULL)),
      MakeBinaryOutcome("x10")),
      "restricting to child worlds should produce an empty entry in the skipped world");
  }


  static void TestOptimisticImpossibleWorlds()
  {
    ToyNode rootBest("rootBest", TOY_LEAF, 3);
    rootBest.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode impossibleReply("impossibleReply", TOY_LEAF, 3);
    impossibleReply.leafFront = MakeFront(3, vector<string>(1, "000"));

    ToyNode shouldNotVisit("shouldNotVisit", TOY_LEAF, 3);
    shouldNotVisit.leafFront = MakeFront(3, vector<string>(1, "111"));

    ToyNode candidateMin("candidateMin", TOY_MIN, 3);
    candidateMin.optimisticValues = MakeBinaryOutcome("011");
    AddChild(candidateMin, impossibleReply, WorldMask(3, 0x4ULL));
    AddChild(candidateMin, shouldNotVisit);

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, rootBest);
    AddChild(root, candidateMin);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    const ParetoFront sparseImpossible = impossibleReply.leafFront.RestrictToUseful(
      WorldMask(3, 0x4ULL));
    Check(FrontContains(sparseImpossible, MakeBinaryOutcome("xx0")),
      "optimistic example should first produce a sparse impossible-world vector [x x 0]");

    const ParetoFront optimisticImpossible = sparseImpossible.CompleteOptimistically(
      WorldMask::All(3), candidateMin.optimisticValues);
    Check(FrontContains(optimisticImpossible, MakeBinaryOutcome("010")),
      "optimistic example should complete [x x 0] to [0 1 0] using the closest known world values");

    Check(stats.optimisticCompletions >= 1,
      "optimistic example should record at least one optimistic completion");
    Check(stats.earlyCuts == 1,
      "optimistic example should trigger an early cut once the impossible world is completed optimistically");
    Check(find(stats.visitOrder.begin(), stats.visitOrder.end(),
      string("shouldNotVisit")) == stats.visitOrder.end(),
      "optimistic example should cut before visiting the remaining Min child");
    Check(FrontContains(front, MakeBinaryOutcome("110")),
      "optimistic example should preserve the dominating root outcome");
    Check(! rootCutTriggered,
      "optimistic example should not be reported as a root cut");
  }


  static void TestDeepAlphaCut()
  {
    ToyNode rootBest("rootBest", TOY_LEAF, 3);
    rootBest.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode innerBest("innerBest", TOY_LEAF, 3);
    innerBest.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode candidateFirst("candidateFirst", TOY_LEAF, 3);
    candidateFirst.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode skippedByDeepAlpha("skippedByDeepAlpha", TOY_LEAF, 3);
    skippedByDeepAlpha.leafFront = MakeFront(3, vector<string>(1, "111"));

    ToyNode deepMin("deepMin", TOY_MIN, 3);
    AddChild(deepMin, candidateFirst);
    AddChild(deepMin, skippedByDeepAlpha);

    ToyNode innerMax("innerMax", TOY_MAX, 3);
    AddChild(innerMax, innerBest);
    AddChild(innerMax, deepMin);

    ToyNode outerMin("outerMin", TOY_MIN, 3);
    AddChild(outerMin, innerMax);

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, rootBest);
    AddChild(root, outerMin);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 3, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(stats.deepAlphaCuts == 1,
      "deep-alpha example should trigger exactly one deep alpha cut");
    Check(stats.earlyCuts == 0,
      "deep-alpha example should cut via an ancestor Max front rather than the immediate one");
    Check(find(stats.visitOrder.begin(), stats.visitOrder.end(),
      string("skippedByDeepAlpha")) == stats.visitOrder.end(),
      "deep-alpha cut should stop before searching the remaining deep Min child");
    Check(FrontContains(front, MakeBinaryOutcome("110")),
      "deep-alpha example should preserve the dominating root outcome");
    Check(! rootCutTriggered,
      "deep-alpha example should not be reported as a root cut");
  }


  static void TestCutOnWin()
  {
    ToyNode winningMove("winningMove", TOY_LEAF, 3);
    winningMove.leafFront = MakeFront(3, vector<string>(1, "111"));

    ToyNode skippedSibling("skippedSibling", TOY_LEAF, 3);
    skippedSibling.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, winningMove);
    AddChild(root, skippedSibling);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 1, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(stats.cutOnWinCuts == 1,
      "cut-on-win example should trigger exactly one cut on win");
    Check(find(stats.visitOrder.begin(), stats.visitOrder.end(),
      string("skippedSibling")) == stats.visitOrder.end(),
      "cut on win should stop before searching the remaining sibling move");
    Check(FrontContains(front, MakeBinaryOutcome("111")),
      "cut-on-win example should keep the fully winning move");
    Check(! rootCutTriggered,
      "cut-on-win example should not be reported as a root cut");
  }


  static void TestDDSLeafDemo()
  {
    SetMaxThreads(0);

    HandFileData data;
    LoadHandFile("hands/alpha_mu_play.txt", data);

    Check(data.number == 3,
      "alpha_mu_play.txt should provide three DDS worlds for the leaf demo");

    const int target = 4;
    const DDSLeafEvalResult serial = EvaluateDDSLeafThresholdSerial(data, target);
    const DDSLeafEvalResult parallel = EvaluateDDSLeafThresholdParallel(
      data,
      target,
      data.number);

    Check(serial.leaf.ToString() == "[1 1 0]",
      "DDS leaf demo should yield the expected threshold vector [1 1 0]");
    Check(parallel.leaf.ToString() == "[1 1 0]",
      "parallel DDS leaf demo should yield the expected threshold vector [1 1 0]");
    Check(serial.leaf.ToString() == parallel.leaf.ToString(),
      "serial and parallel DDS leaf evaluation should return the same threshold vector");
    Check(serial.bestScores == parallel.bestScores,
      "serial and parallel DDS leaf evaluation should return the same world scores");
    Check(parallel.workerCount >= 1,
      "parallel DDS leaf evaluation should configure at least one worker");
    Check(parallel.workerCount <= data.number,
      "parallel DDS leaf evaluation should not configure more worker slots than worlds");
  }
}


int main(int argc, char ** argv)
{
  if (argc >= 2)
  {
    const string mode(argv[1]);
    if (mode == "bridge_dds")
    {
      TestBridgeMultiTrickDDSLeaf();
      cout << "alpha_mu_prototype: multi-trick bridge DDS leaf search OK\n";
      cout << "alpha_mu_prototype: all checks passed\n";
      return 0;
    }
  }

  TestParetoInsert();
  cout << "alpha_mu_prototype: Pareto insert test OK\n";

  TestNonLocalityExample();
  cout << "alpha_mu_prototype: non-locality toy search OK\n";

  TestEarlyCutExample();
  cout << "alpha_mu_prototype: early cut toy search OK\n";

  TestUsefulWorldMaintenance();
  cout << "alpha_mu_prototype: useful-world maintenance OK\n";

  TestWorldCuts();
  cout << "alpha_mu_prototype: world cuts OK\n";

  TestParetoFrontTT();
  cout << "alpha_mu_prototype: Pareto-front TT OK\n";

  TestPossibleWorldGeneration();
  cout << "alpha_mu_prototype: possible-world generation OK\n";

  TestPlayHistoryFiltering();
  cout << "alpha_mu_prototype: play-history filtering OK\n";

  TestBridgeMoveGeneration();
  cout << "alpha_mu_prototype: bridge move generation OK\n";

  TestBridgeSearchControl();
  cout << "alpha_mu_prototype: bridge search control OK\n";

  TestEmptyEntryInteriorFronts();
  cout << "alpha_mu_prototype: empty-entry interior fronts OK\n";

  TestOptimisticImpossibleWorlds();
  cout << "alpha_mu_prototype: optimistic impossible worlds OK\n";

  TestDeepAlphaCut();
  cout << "alpha_mu_prototype: deep alpha cuts OK\n";

  TestCutOnWin();
  cout << "alpha_mu_prototype: cut on win OK\n";

  TestRootCutExample();
  cout << "alpha_mu_prototype: root cut toy search OK\n";

  TestDDSLeafDemo();
  cout << "alpha_mu_prototype: DDS leaf demo and leaf parallelization OK\n";


  cout << "alpha_mu_prototype: all checks passed\n";
  return 0;
}

