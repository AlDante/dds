/**
 * @file alpha_mu_prototype_core.h
 * @brief Shared data structures and callable entry points for the alpha-mu prototype.
 *
 * The prototype follows the two-paper alpha-mu line described in
 * `docs/alpha-mu.md` and `docs/action-plan.md`. The first paper contributes the
 * imperfect-information search model over sampled possible worlds using outcome
 * vectors, Pareto fronts, Max-node union, and Min-node product/min backup. The
 * later optimization paper contributes useful-world maintenance, world cuts,
 * cut-on-win, optimistic completion of impossible worlds, deep alpha cuts, and
 * related implementation refinements.
 *
 * This header intentionally exposes both the paper-faithful toy search and the
 * bridge-specific scaffolding that feeds DDS perfect-information leaves into the
 * same front-based search semantics.
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

  /**
   * @brief Bit-mask representation of the currently relevant possible worlds.
   *
   * Alpha-mu operations repeatedly need to talk about subsets of the sampled
   * worlds: worlds still consistent with the history, worlds still useful at a
   * Min node, worlds reachable by a child move, and so on. This small helper is
   * the core set representation used throughout the prototype.
   */
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

    /** @brief Construct a mask containing all world indices in the range. */
    static WorldMask All(const unsigned countArg)
    {
      return WorldMask(
        countArg,
        (countArg >= 64 ? ~0ULL :
         (countArg == 0 ? 0ULL : ((1ULL << countArg) - 1ULL))));
    }

    /** @brief Construct an empty mask with the given world capacity. */
    static WorldMask None(const unsigned countArg)
    {
      return WorldMask(countArg, 0ULL);
    }

    bool operator==(const WorldMask& other) const
    {
      return count == other.count && bits == other.bits;
    }

    /** @brief Return whether a world index is currently enabled in the mask. */
    bool Has(const unsigned index) const
    {
      return index < count && ((bits & (1ULL << index)) != 0ULL);
    }

    /** @brief Count the number of active worlds represented by this mask. */
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

    /** @brief Return the set-theoretic union of two world masks. */
    WorldMask Union(const WorldMask& other) const
    {
      return WorldMask(count, bits | other.bits);
    }

    /** @brief Return the set-theoretic intersection of two world masks. */
    WorldMask Intersection(const WorldMask& other) const
    {
      return WorldMask(count, bits & other.bits);
    }

    /** @brief Return whether no worlds survive in the mask. */
    bool Empty() const
    {
      return bits == 0ULL;
    }

    /** @brief Render the active world indices in a compact debug form. */
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


  /**
   * @brief Score vector over the sampled worlds.
   *
   * In the original alpha-mu formulation each entry stores the score obtained in
   * one possible world. The `valid` mask distinguishes worlds that have an exact
   * value in the vector from worlds that are currently impossible, pruned, or
   * only present through optimistic completion.
   */
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

    /**
     * @brief Return whether this vector weakly dominates another vector.
     *
     * Dominance is only defined for vectors over the same valid-world set.
     */
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

    /** @brief Return the mean score over the currently valid worlds. */
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

    /**
     * @brief Compute the Min-node product/min combination with another vector.
     *
     * This is the per-world minimum used by the alpha-mu Min backup rule before
     * Pareto reduction is applied at the front level.
     */
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

    /**
     * @brief Zero out worlds that are no longer useful at a Min node.
     *
     * This models the optimization-paper idea that some worlds no longer affect
     * the Min backup once the current front already proves them irrelevant.
     */
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

    /**
     * @brief Fill missing worlds with optimistic values for dominance checks.
     *
     * This corresponds to the optimization-paper discussion of comparing sparse
     * fronts by giving impossible or unexpanded worlds optimistic completions.
     */
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

    /** @brief Render the vector as debug text with `x` for invalid worlds. */
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


  /**
   * @brief Non-dominated set of outcome vectors.
   *
   * Pareto-front maintenance is the central data structure in alpha-mu. Max
   * nodes union child fronts and then drop dominated vectors; Min nodes form the
   * product of child fronts under per-world minimum and then again reduce by
   * dominance.
   */
  struct ParetoFront
  {
    unsigned worldCount;
    vector<OutcomeVector> vectors;

    explicit ParetoFront(const unsigned worldCountArg = 0) :
      worldCount(worldCountArg),
      vectors()
    {
    }

    /** @brief Insert a vector and discard any newly dominated incumbents. */
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

    /** @brief Return whether every vector in @p other is dominated here. */
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

    /**
     * @brief Return the best mean score over the front.
     *
     * The prototype uses this as the root `mu` value for iterative deepening and
     * the root-cut optimization.
     */
    double Mu() const
    {
      double best = 0.0;
      for (unsigned i = 0; i < vectors.size(); i++)
        best = max(best, vectors[i].Mean());
      return best;
    }

    /** @brief Perform the alpha-mu Max-node union/merge operation. */
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

    /** @brief Perform the alpha-mu Min-node product/min operation. */
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

    /**
     * @brief Compute the useful worlds still capable of affecting Min backup.
     *
     * This implements the optimization-paper notion of useful worlds.
     */
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

    /** @brief Return the union of worlds mentioned by any vector in the front. */
    WorldMask ValidWorlds() const
    {
      WorldMask valid = WorldMask::None(worldCount);
      for (unsigned i = 0; i < vectors.size(); i++)
        valid = valid.Union(vectors[i].valid);
      return valid;
    }

    /// @brief Apply useful-world reduction to every vector in the front.
    ParetoFront RestrictToUseful(const WorldMask& useful) const
    {
      ParetoFront result(worldCount);
      for (unsigned i = 0; i < vectors.size(); i++)
        result.Insert(vectors[i].RestrictToUseful(useful));
      return result;
    }

    /// @brief Optimistically complete every vector for sparse-front comparison.
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

    /**
     * @brief Return whether some vector wins strictly in every useful world.
     *
     * This is the condition used by the optimization-paper cut-on-win rule.
     */
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

    /** @brief Render the front as a set of outcome vectors. */
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


  /** @brief Node role in the small paper-faithful toy search harness. */
  enum ToyNodeType
  {
    TOY_LEAF = 0,
    TOY_MAX = 1,
    TOY_MIN = 2
  };


  /**
   * @brief Minimal game tree node used to exercise paper semantics in isolation.
   *
   * The toy tree lets the prototype test non-locality, early cut, cut-on-win,
   * deep alpha cuts, transposition-table behavior, and root cut without bridge
   * move-generation noise.
   */
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


  /** @brief Counters for the paper-level search mechanics exercised in a run. */
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


  /** @brief Exact transposition-table payload for a previously solved front. */
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


  /**
   * @brief Toy-search transposition table keyed by node, depth, and useful worlds.
   *
   * Only exact fronts are stored, mirroring the paper's preference to avoid
   * mixing partial front information into reuse.
   */
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


  /** @brief Result of iterative deepening in Max-move horizon. */
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


  /**
   * @brief Owning wrapper for parsed DDS hand-file arrays.
   *
   * The benchmark and comparison modes use the legacy C parsing helpers from the
   * test harness; this wrapper gives them a single RAII-managed lifetime.
   */
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


  /** @brief Canonical seat numbering used throughout the prototype. */
  enum Seat
  {
    SEAT_NORTH = 0,
    SEAT_EAST = 1,
    SEAT_SOUTH = 2,
    SEAT_WEST = 3
  };


  /** @brief Canonical suit numbering used throughout the prototype. */
  enum SuitIndex
  {
    SUIT_SPADES = 0,
    SUIT_HEARTS = 1,
    SUIT_DIAMONDS = 2,
    SUIT_CLUBS = 3
  };


  /** @brief Supported world-constraint families for world construction/filtering. */
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


  /**
   * @brief Auction-side hand-shape categories accepted by the scoped interface.
   *
   * These are consumed as already-derived bidding facts; alpha-mu does not try
   * to interpret the auction itself.
   */
  enum HandType
  {
    HAND_TYPE_BALANCED = 0,
    HAND_TYPE_ONE_SUITER = 1,
    HAND_TYPE_TWO_SUITER = 2,
    HAND_TYPE_THREE_SUITER = 3
  };


  /** @brief Simple PBN-style four-hand world representation. */
  struct ParsedWorld
  {
    string suits[4][4];
  };


  /** @brief Single bridge card play in suit/rank form. */
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


  /**
   * @brief Bridge continuation state searched by the prototype.
   *
   * This wraps the current partial trick, side to move, already won Max-side
   * tricks, trump, and the surviving possible worlds that remain legal after the
   * line of play followed so far.
   */
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


  /** @brief A legal bridge child move paired with the resulting continuation state. */
  struct BridgeChild
  {
    BridgeMove move;
    BridgeState state;
  };


  /** @brief Root-level report for one candidate bridge move. */
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


  /** @brief Full root report containing all child fronts and the merged root front. */
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


  /** @brief Recorded play-history fact used to constrain legal worlds. */
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


  /** @brief Stage-by-stage counts for staged possible-world filtering. */
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


  /**
   * @brief One externally derived fact about a world.
   *
   * These constraints are the boundary between the alpha-mu world generator and
   * other analysis components such as auction interpretation.
   */
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


  /**
   * @brief Partial-information package used to generate or explain worlds.
   *
   * The prototype separates known cards, bidding facts, follow-suit facts, play
   * history, and deterministic sampling controls so each stage can be tested and
   * explained independently.
   */
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


  /** @brief One hidden card plus the seats it may still legally belong to. */
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


  /**
   * @brief Seed specification for constructor-local world building from history.
   *
   * This is a repository-specific extension around the paper algorithm: it turns
   * a partial-information bridge state into a candidate world pool that alpha-mu
   * can search over.
   */
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


  /** @brief Candidate worlds produced by history-derived construction. */
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


  /** @brief Stage counters for constructor-local history-derived pruning. */
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


  /** @brief One explanatory acceptance/rejection step for a candidate world. */
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


  /** @brief Full staged explanation for one candidate world. */
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


  /** @brief Explanation bundle for the staged world-filter pipeline. */
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


  /** @brief Explanation bundle for constructor-local history-derived pruning. */
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


  /** @brief Result of replaying a history sequence inside one candidate world. */
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


  /** @brief Scores obtained from DDS leaf evaluation over one or more worlds. */
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


  /** @brief Per-depth timing and mismatch summary in DDS-vs-alpha-mu comparison mode. */
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


  /** @brief Aggregate comparison summary across DDS and alpha-mu exact modes. */
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


  /** @brief Requested concurrency strategy for alpha-mu benchmark/search entry points. */
  enum AlphaMuParallelMode
  {
    ALPHA_MU_PARALLEL_SERIAL = 0,
    ALPHA_MU_PARALLEL_BOARD = 1,
    ALPHA_MU_PARALLEL_ROOT = 2
  };


  /** @brief Summary for one exact benchmark method over a hand-file workload. */
  struct BenchmarkMethodSummary
  {
    string method;
    string handFile;
    unsigned boardsTested;
    int depth;
    AlphaMuParallelMode parallelMode;
    int boardWorkers;
    int rootWorkers;
    int ddsThreadId;
    int configuredBoardWorkers;
    double elapsedSeconds;
    unsigned mismatches;
    vector<double> perBoardSeconds;

    BenchmarkMethodSummary() :
      method(),
      handFile(),
      boardsTested(0),
      depth(0),
      parallelMode(ALPHA_MU_PARALLEL_SERIAL),
      boardWorkers(1),
      rootWorkers(1),
      ddsThreadId(0),
      configuredBoardWorkers(1),
      elapsedSeconds(0.0),
      mismatches(0),
      perBoardSeconds()
    {
    }
  };


  /** @brief Mutable progress-reporting state for long-running benchmark jobs. */
  struct BenchmarkBoardProgressContext
  {
    string method;
    string handFile;
    unsigned boardNumber;
    unsigned totalBoards;
    int depth;
    AlphaMuParallelMode parallelMode;
    int boardWorkers;
    int rootWorkers;
    int ddsThreadId;
    int configuredBoardWorkers;
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
      parallelMode(ALPHA_MU_PARALLEL_SERIAL),
      boardWorkers(1),
      rootWorkers(1),
      ddsThreadId(0),
      configuredBoardWorkers(1),
      reportIntervalSeconds(0.0),
      totalStart(),
      boardStart(),
      nextReportSeconds(0.0),
      recursiveCalls(0ULL),
      ddsLeafCalls(0ULL)
    {
    }
  };



  /** @brief Explicit execution context carried through bridge search and DDS leaf calls. */
  struct SearchExecutionContext
  {
    int ddsThreadId;
    AlphaMuParallelMode parallelMode;
    int boardWorkers;
    int rootWorkers;
    BenchmarkBoardProgressContext * benchmarkProgress;

    SearchExecutionContext() :
      ddsThreadId(0),
      parallelMode(ALPHA_MU_PARALLEL_SERIAL),
      boardWorkers(1),
      rootWorkers(1),
      benchmarkProgress(NULL)
    {
    }
  };


  /** @brief Benchmark-mode options normalized before the actual workload starts. */
  struct AlphaMuBenchmarkOptions
  {
    string handFile;
    int depth;
    int maxBoards;
    string skipSpec;
    AlphaMuParallelMode parallelMode;
    int boardWorkers;
    int rootWorkers;
    int ddsThreadId;

    AlphaMuBenchmarkOptions() :
      handFile(),
      depth(0),
      maxBoards(0),
      skipSpec(),
      parallelMode(ALPHA_MU_PARALLEL_SERIAL),
      boardWorkers(1),
      rootWorkers(1),
      ddsThreadId(0)
    {
    }
  };

  /** @brief Prefix used by prototype status and failure messages. */
  extern const char kPrototypeMessagePrefix[];
  /** @brief Default hand-file used by the alpha-mu DDS leaf regressions. */
  extern const char kAlphaMuPlayHandFile[];

  /** @brief Render a parallel-mode enum in command-line and log-friendly text. */
  string AlphaMuParallelModeName(const AlphaMuParallelMode mode);
  /** @brief Parse a command-line parallel-mode token. */
  AlphaMuParallelMode ParseAlphaMuParallelModeName(const string& text);
  /** @brief Construct an explicit search execution context. */
  SearchExecutionContext MakeSearchExecutionContext( const int ddsThreadId, BenchmarkBoardProgressContext * benchmarkProgress, const AlphaMuParallelMode parallelMode = ALPHA_MU_PARALLEL_SERIAL, const int boardWorkers = 1, const int rootWorkers = 1);
  /** @brief Clamp benchmark execution options into a future-proof serial-safe baseline. */
  AlphaMuBenchmarkOptions NormalizeAlphaMuBenchmarkOptions( const AlphaMuBenchmarkOptions& options);

  /** @brief Abort the current run with a prototype-prefixed fatal message. */
  void Fail(const string& msg);
  /** @brief Assert a prototype invariant and fail with context if it is violated. */
  void Check(const bool condition, const string& msg);
  /** @brief Exact equality check for two outcome vectors. */
  bool SameOutcome( const OutcomeVector& left, const OutcomeVector& right);
  /** @brief Return whether a front contains a vector exactly, not merely by dominance. */
  bool FrontContains( const ParetoFront& front, const OutcomeVector& target);
  /** @brief Convert PBN seat letters to the prototype seat indices. */
  int SeatIndex(const char seat);
  /** @brief Split a string with optional retention of empty fields. */
  vector<string> SplitString( const string& text, const char delimiter, const bool keepEmpty);
  /** @brief Parse a compact PBN world string into the internal world representation. */
  ParsedWorld ParsePBNWorld(const string& pbn);
  /** @brief Return whether a world places a given card at a given seat. */
  bool WorldHasCard( const ParsedWorld& world, const int player, const int suit, const char rank);
  /** @brief Return the length of one suit in one hand. */
  int WorldSuitLength( const ParsedWorld& world, const int player, const int suit);
  /** @brief Convert a rank character to standard high-card-point value. */
  int HonorPointValue(const char rank);
  /** @brief Sum the high-card points held by one seat in one world. */
  int WorldHighCardPoints( const ParsedWorld& world, const int player);
  /** @brief Render a hand-type enum value in auction-facing terminology. */
  string HandTypeName(const int handType);
  /** @brief Classify a full 13-card length pattern against the supported hand types. */
  bool LengthsMatchHandType( const int lengths[4], const int totalCards, const int handType);
  /** @brief Return whether a complete world hand satisfies the given hand-type class. */
  bool WorldHasHandType( const ParsedWorld& world, const int player, const int handType);
  /** @brief Compatibility helper for the balanced-hand special case. */
  bool WorldHasBalancedShape( const ParsedWorld& world, const int player);
  /** @brief Normalize legacy balanced constraints and explicit hand-type constraints. */
  int ConstraintHandType(const WorldConstraint& constraint);
  /** @brief Map rank text into ascending rank order for comparisons and sorting. */
  int RankOrder(const char rank);
  /** @brief Convert rank text to DDS numeric rank encoding. */
  int RankValue(const char rank);
  /** @brief Convert DDS numeric rank encoding back to rank text. */
  char RankFromDDSValue(const int value);
  /** @brief Count all remaining cards still present in a world. */
  unsigned WorldCardCount(const ParsedWorld& world);
  /** @brief Count the cards still present in one seat of a world. */
  unsigned WorldSeatCardCount( const ParsedWorld& world, const int player);
  /** @brief Serialize the internal world representation back to compact PBN text. */
  string SerializePBNWorld(const ParsedWorld& world);
  /** @brief Human-readable seat name for diagnostics and explanations. */
  string SeatName(const int seat);
  /** @brief Human-readable suit name for diagnostics and explanations. */
  string SuitName(const int suit);
  /** @brief Return the partner seat in a bridge partnership. */
  int PartnerSeat(const int seat);
  /** @brief Human-readable partnership label anchored at one seat. */
  string PartnershipName(const int seat);
  /** @brief Human-readable card name for diagnostics and explanations. */
  string CardName(const BridgeMove& move);
  /** @brief Check whether a world satisfies one explicit constraint. */
  bool WorldMatchesConstraint( const ParsedWorld& world, const WorldConstraint& constraint);
  /** @brief Remove a played card from the specified seat in a world. */
  void RemoveCardFromWorld( ParsedWorld& world, const int player, const BridgeMove& move);
  /** @brief Render a constraint as explanatory text. */
  string ConstraintToString(const WorldConstraint& constraint);
  /** @brief Return the first failing constraint as user-facing explanatory text. */
  string FirstConstraintFailureReason( const ParsedWorld& world, const vector<WorldConstraint>& constraints);
  /** @brief Replay a complete prior history against a world and explain failure. */
  HistoryCheckResult CheckWorldReplayHistory( const ParsedWorld& world, const vector<PlayHistoryEvent>& history, const string& stageName);
  /** @brief Replay a partial current trick after an already accepted prior history. */
  HistoryCheckResult CheckWorldReplayHistoryAfterHistory( const ParsedWorld& world, const vector<PlayHistoryEvent>& priorHistory, const vector<PlayHistoryEvent>& history, const string& stageName);
  /** @brief Derive follow-suit impossibility constraints from observed discards. */
  void AppendDerivedFollowSuitConstraints( const vector<PlayHistoryEvent>& history, int playedCount[4][4], vector<WorldConstraint>& constraints);
  /** @brief Gather explicit and derived follow-suit constraints for filtering. */
  vector<WorldConstraint> CollectFollowSuitConstraints( const BridgeInformationState& information);
  /** @brief Select bidding constraints that are safe for constructor-local pruning. */
  vector<WorldConstraint> CollectConstructorConstraints( const BridgeInformationState& information);
  /** @brief Append one explanation step to a world's trace. */
  void AddWorldExplanationStep( WorldExplanation& explanation, const string& stage, const bool passed, const string& detail);
  unsigned SeatBit(const int seat);
  unsigned SeatMask(const vector<int>& seats);
  bool VectorContainsSeat( const vector<int>& seats, const int seat);
  /** @brief Return whether a constructor-local constraint refers to hidden seats. */
  bool ConstructorConstraintTouchesHiddenSeats( const vector<int>& hiddenSeats, const WorldConstraint& constraint);
  string CardKey(const BridgeMove& move);
  void SortSuitCards(string& cards);
  /** @brief Canonicalize suit-card order to stabilize deduplication and explanations. */
  void CanonicalizeWorld(ParsedWorld& world);
  /** @brief Record cards already played, keyed by card name and owner seat. */
  void CollectPlayedCards( const BridgeInformationState& information, map<string, int>& playedBySeat);
  unsigned CountSeatChoices(const unsigned allowedSeatsMask);
  /** @brief Find the current owner seat of a card in a world, or `-1` if absent. */
  int FindCardSeatInWorld( const ParsedWorld& world, const int suit, const char rank);
  /** @brief Extract explicitly unspecified hidden cards from the visible seed world. */
  void CollectSeedHiddenCards( const HistoryDerivedWorldSpec& spec, HistoryDerivedConstructionResult& result, const unsigned hiddenSeatMask, int targetCounts[4]);
  /** @brief Infer hidden cards from the full-deck complement of visible seed hands. */
  void CollectInferredHiddenCardsFromVisibleHands( const HistoryDerivedWorldSpec& spec, HistoryDerivedConstructionResult& result, const unsigned hiddenSeatMask, int targetCounts[4], int finalSeatCounts[4]);
  /** @brief Pin played cards to their known owning hidden seats before enumeration. */
  void ApplyConstructionPlayedCardOwnership( const vector<int>& hiddenSeats, const BridgeInformationState& information, vector<HiddenCardCandidate>& hiddenCards);
  /** @brief Sort hidden-card candidates for stable world construction and reporting. */
  void SortHiddenCardCandidates( vector<HiddenCardCandidate>& hiddenCards);
  /** @brief Sort constructed worlds canonically for deterministic downstream behavior. */
  void SortConstructedWorlds( vector<ParsedWorld>& worlds);
  /** @brief Prepare the constructor-local state used by history-derived world building. */
  void PrepareHistoryDerivedConstruction( const HistoryDerivedWorldSpec& spec, const BridgeInformationState& information, HistoryDerivedConstructionResult& result, vector<WorldConstraint>& constructorConstraints, int targetCounts[4], int finalSeatCounts[4]);
  /** @brief Restrict hidden-card seat choices using explicit card-location constraints. */
  void ApplyConstructionCardLocationConstraints( const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints, vector<HiddenCardCandidate>& hiddenCards);
  int PotentialRemainingSuitCardsForSeat( const vector<HiddenCardCandidate>& hiddenCards, const unsigned nextIndex, const int seat, const int suit);
  int PotentialRemainingHighCardPointsForSeat( const vector<HiddenCardCandidate>& hiddenCards, const unsigned nextIndex, const int seat);
  /** @brief Check whether unfinished assignments can still satisfy length constraints. */
  bool ConstructorLengthConstraintsPossible( const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints, const ParsedWorld& current, const vector<HiddenCardCandidate>& hiddenCards, const unsigned nextIndex);
  /** @brief Check whether unfinished assignments can still satisfy HCP constraints. */
  bool ConstructorHCPConstraintsPossible( const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints, const ParsedWorld& current, const vector<HiddenCardCandidate>& hiddenCards, const unsigned nextIndex);
  /** @brief Conservative balanced/hand-type feasibility test for partial hidden hands. */
  bool PartialSeatCanStillReachBalancedShape( const ParsedWorld& current, const vector<HiddenCardCandidate>& hiddenCards, const unsigned nextIndex, const int seat, const int targetCount, const int handType);
  /** @brief Check whether unfinished assignments can still satisfy shape constraints. */
  bool ConstructorBalancedConstraintsPossible( const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints, const ParsedWorld& current, const vector<HiddenCardCandidate>& hiddenCards, const int finalSeatCounts[4], const unsigned nextIndex);
  /** @brief Combined constructor-local feasibility check across bidding-derived facts. */
  bool ConstructorBiddingConstraintsPossible( const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints, const ParsedWorld& current, const vector<HiddenCardCandidate>& hiddenCards, const int finalSeatCounts[4], const unsigned nextIndex);
  /** @brief Check whether one hidden card can still be assigned to one seat. */
  bool CanAssignHiddenCard( const HiddenCardCandidate& candidate, const int seat, const int targetCounts[4], const int assignedCounts[4]);
  /** @brief Enumerate history-derived worlds with constructor-local pruning enabled. */
  void ConstructHistoryDerivedWorldsRec( const vector<int>& hiddenSeats, const vector<HiddenCardCandidate>& hiddenCards, const vector<WorldConstraint>& constructorConstraints, const int targetCounts[4], const int finalSeatCounts[4], int assignedCounts[4], ParsedWorld& current, const unsigned index, vector<ParsedWorld>& worlds);
  /** @brief Enumerate the raw assignment space without constructor-local pruning. */
  void EnumerateHistoryDerivedWorldsRec( const vector<int>& hiddenSeats, const vector<HiddenCardCandidate>& hiddenCards, const int targetCounts[4], int assignedCounts[4], ParsedWorld& current, const unsigned index, vector<ParsedWorld>& worlds);
  /** @brief Materialize all raw worlds implied by the visible seed and hidden cards. */
  vector<ParsedWorld> EnumerateHistoryDerivedWorlds( const vector<int>& hiddenSeats, const vector<HiddenCardCandidate>& hiddenCards, const int targetCounts[4], const ParsedWorld& visibleSeedWorld);
  bool WorldMatchesConstructorLengthConstraints( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  bool WorldMatchesConstructorHCPConstraints( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  bool WorldMatchesConstructorBalancedConstraints( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  string FirstConstructorLengthFailureReason( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  string FirstConstructorHCPFailureReason( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  string FirstConstructorBalancedFailureReason( const ParsedWorld& world, const vector<int>& hiddenSeats, const vector<WorldConstraint>& constraints);
  /**
   * @brief Build candidate worlds from a partial-information bridge history.
   *
   * This is the repository-specific bridge front-end that feeds the alpha-mu
   * search layer: start from a visible seed state, enumerate hidden-card
   * assignments, and prune early using only constructor-safe constraints.
   */
  HistoryDerivedConstructionResult ConstructCandidateWorldsFromHistory( const HistoryDerivedWorldSpec& spec, const BridgeInformationState& information);
  /** @brief Explain each constructor-local pruning stage used during world building. */
  HistoryDerivedConstructionExplanation ExplainHistoryDerivedConstruction( const HistoryDerivedWorldSpec& spec, const BridgeInformationState& information);
  /** @brief Convert a bridge continuation state into the DDS `dealPBN` leaf format. */
  dealPBN MakeDDSDealPBN( const BridgeState& state, const ParsedWorld& world);
  /** @brief Count the number of unresolved tricks remaining in one world. */
  int RemainingTricksInWorld( const BridgeState& state, const ParsedWorld& world);
  bool BridgeMoveLess( const BridgeMove& left, const BridgeMove& right);
  int SeatSide(const int seat);
  unsigned WinningCardIndex( const vector<BridgeMove>& trick, const int leadSuit, const int trumpSuit);
  int TrickWinner( const BridgeState& state);
  bool WorldHasLegalSuit( const ParsedWorld& world, const int player, const int leadSuit);
  vector<BridgeMove> LegalMovesInWorld( const ParsedWorld& world, const int player, const int leadSuit);
  bool ContainsMove( const vector<BridgeMove>& moves, const BridgeMove& move);
  bool WorldCanPlayMove( const ParsedWorld& world, const int player, const int leadSuit, const BridgeMove& move);
  /** @brief Generate the union of legal moves across all surviving worlds. */
  vector<BridgeMove> GenerateBridgeMoves(const BridgeState& state);
  /** @brief Apply one move, eliminating worlds where that move was illegal. */
  BridgeState PlayBridgeMove( const BridgeState& state, const BridgeMove& move);
  /** @brief Expand the bridge state into legal move/state children. */
  vector<BridgeChild> ExpandBridgeChildren(const BridgeState& state);
  /** @brief Construct the all-zero front used for empty or impossible world sets. */
  ParetoFront MakeZeroFront(const unsigned worldCount);
  int BestScore(const futureTricks& fut);
  void CheckDDS(const int ret, const string& tag);
  void MaybeReportBenchmarkBoardProgress( const BridgeState& state, const int tricksRemaining, const SearchExecutionContext& context);
  /** @brief Terminal bridge front when no further DDS solve is required. */
  ParetoFront MakeBridgeTerminalFront(const BridgeState& state);
  /**
   * @brief Evaluate all active worlds exactly with DDS and wrap them as one front.
   *
   * DDS acts as the perfect-information oracle beneath the alpha-mu layer.
   */
  ParetoFront MakeBridgeDDSLeafFront(const BridgeState& state, const SearchExecutionContext& context);
  /** @brief Compatibility wrapper using the default serial search execution context. */
  ParetoFront MakeBridgeDDSLeafFront(const BridgeState& state);
  /** @brief Charge one unit of alpha-mu depth when a full trick has just completed. */
  int BridgeDepthCost( const BridgeState& state, const BridgeState& child);
  /**
   * @brief Search a bridge continuation using alpha-mu Max/Min front semantics.
   *
   * This is the bridge-specific analogue of the toy search, but without the more
   * aggressive paper optimizations; it focuses on validating front propagation
   * and DDS leaf handoff over realistic card play.
   */
  ParetoFront SearchBridgeStateInternal( const BridgeState& state, const int tricksRemaining, const SearchExecutionContext& context);
  /** @brief Compatibility wrapper using the default serial search execution context. */
  ParetoFront SearchBridgeStateInternal( const BridgeState& state, const int tricksRemaining);
  /** @brief Public bridge-search wrapper for multi-trick continuation analysis. */
  ParetoFront SearchBridgeState( const BridgeState& state, const int tricksRemaining, const SearchExecutionContext& context);
  /** @brief Compatibility wrapper using the default serial search execution context. */
  ParetoFront SearchBridgeState( const BridgeState& state, const int tricksRemaining);
  /** @brief Analyze every legal root move and report its child front summary. */
  BridgeRootReport AnalyzeBridgeRoot( const BridgeState& state, const int tricksRemaining, const SearchExecutionContext& context);
  /** @brief Compatibility wrapper using the default serial search execution context. */
  BridgeRootReport AnalyzeBridgeRoot( const BridgeState& state, const int tricksRemaining);
  /** @brief Check a world against all supplied constraints. */
  bool WorldMatchesAllConstraints( const ParsedWorld& world, const vector<WorldConstraint>& constraints);
  /** @brief Filter a candidate world mask by explicit structural constraints. */
  WorldMask FilterWorldsByConstraints( const vector<ParsedWorld>& worlds, const WorldMask& candidates, const vector<WorldConstraint>& constraints);
  /** @brief Check whether a world can replay an observed history legally. */
  bool WorldCanReplayHistory( const ParsedWorld& world, const vector<PlayHistoryEvent>& history);
  /** @brief Filter candidates by replaying complete prior-trick history. */
  WorldMask FilterWorldsByHistory( const vector<ParsedWorld>& worlds, const WorldMask& candidates, const vector<PlayHistoryEvent>& history);
  /** @brief Filter candidates by replaying the current partial trick after prior history. */
  WorldMask FilterWorldsByHistoryAfterHistory( const vector<ParsedWorld>& worlds, const WorldMask& candidates, const vector<PlayHistoryEvent>& priorHistory, const vector<PlayHistoryEvent>& history);
  /** @brief Keep only one canonical representative of each equivalent world. */
  WorldMask DeduplicateWorldMask( const vector<ParsedWorld>& worlds, const WorldMask& candidates, unsigned& duplicatesRemoved);
  /** @brief Downselect worlds reproducibly after canonical sorting. */
  WorldMask SampleWorldMaskDeterministically( const vector<ParsedWorld>& worlds, const WorldMask& candidates, const unsigned sampleLimit, const unsigned samplingSeed, unsigned& sampledOutWorlds);
  /**
   * @brief Run the staged world-generation pipeline.
   *
   * This is the bridge-specific world supplier for alpha-mu: known cards,
   * bidding-derived facts, follow-suit implications, play history, current-trick
   * replay, deduplication, and deterministic sampling are applied in order.
   */
  WorldMask GeneratePossibleWorlds( const vector<ParsedWorld>& worlds, const BridgeInformationState& information, WorldGenerationStats* stats);
  /** @brief Produce an explanation trace for every stage of possible-world filtering. */
  WorldGenerationExplanation ExplainPossibleWorldGeneration( const vector<ParsedWorld>& worlds, const BridgeInformationState& information);
  /** @brief Convenience overload for simple constraint-only world filtering. */
  WorldMask GeneratePossibleWorlds( const vector<ParsedWorld>& worlds, const vector<WorldConstraint>& constraints);
  /** @brief Build a binary toy outcome vector from `0`, `1`, and `x` text. */
  OutcomeVector MakeBinaryOutcome( const string& text);
  /** @brief Add a toy child reachable only in the supplied world subset. */
  void AddChild( ToyNode& parent, const ToyNode& child, const WorldMask& worlds);
  /** @brief Add a toy child reachable in all worlds. */
  void AddChild( ToyNode& parent, const ToyNode& child);
  /** @brief Construct the exact-reuse key for the toy transposition table. */
  string MakeTTKey( const ToyNode& node, const int maxMoves, const WorldMask& usefulWorlds);
  /** @brief Build a front from compact textual binary outcomes. */
  ParetoFront MakeFront( const unsigned worldCount, const vector<string>& outcomes);
  /** @brief Resolve a hand-file path relative to either the current or parent directory. */
  string ResolvePath(const string& candidate);
  /** @brief Construct an exact front for a single surviving world. */
  ParetoFront MakeSingleWorldFront( const unsigned worldCount, const unsigned world, const int value);
  /** @brief Return the sole surviving world index, or fail if the mask is empty. */
  unsigned SoleWorldIndex(const WorldMask& mask);
  /** @brief Evaluate the best value assigned to one world by a front. */
  int EvaluateLeafWorld( const ParetoFront& front, const unsigned world);
  /** @brief Solve the toy tree exactly for one specific world. */
  int EvaluateSingleWorld( const ToyNode& node, const int maxMoves, const unsigned world, SearchStats& stats);
  /**
   * @brief Run the paper-faithful alpha-mu recursion on the toy tree.
   *
   * This function contains the main algorithmic ideas from the papers: Max-node
   * front union, Min-node product/min, useful-world maintenance, optimistic
   * completion, early cut, deep alpha cut, cut-on-win, root cut, and exact-only
   * transposition-table storage.
   */
  ParetoFront SearchToy( const ToyNode& node, const int maxMoves, const WorldMask& usefulWorlds, const vector<const ParetoFront *>& upperMaxFronts, const OutcomeVector& optimisticValues, TranspositionTable& tt, const bool isRoot, const double previousRootMu, SearchStats& stats, bool& rootCutTriggered, bool& exactComplete);
  /** @brief Iteratively deepen the toy search in number of Max moves. */
  IterativeResult RunIterativeDeepening( const ToyNode& root, const int maxDepth);
  /** @brief Exact DDS score for one bridge world from the current continuation state. */
  int ExactBridgeDDSScoreForWorld( const BridgeState& state, const unsigned worldIndex, const SearchExecutionContext& context);
  /** @brief Compatibility wrapper using the default serial search execution context. */
  int ExactBridgeDDSScoreForWorld( const BridgeState& state, const unsigned worldIndex);
  /** @brief Parse a DDS hand file into owned arrays for benchmark or comparison modes. */
  void LoadHandFile( const string& fname, HandFileData& data);
  /** @brief Solve one parsed hand-file board exactly with DDS. */
  int SolveDDSLeafWorld( const HandFileData& data, const int index, const int thrId);
  /** @brief Convert one parsed DDS deal into the prototype bridge-state wrapper. */
  BridgeState MakeBridgeStateFromDDSDeal( const dealPBN& deal);
  /** @brief Extract the single-world score expected at a given alpha-mu depth. */
  int SingleWorldFrontScore( const ParetoFront& front, const int depth, const int boardIndex);
  /** @brief Normalize the board-count limit used by benchmarks. */
  unsigned BoardsToBenchmark( const HandFileData& data, const int maxBoards);
  /** @brief Parse a comma/range board-skip specification used by benchmarks. */
  set<unsigned> ParseSkippedBoardNumbers( const string& skipSpec, const unsigned availableBoards);
  /** @brief Select the benchmark board numbers after limits and skips are applied. */
  vector<unsigned> SelectBenchmarkBoardNumbers( const HandFileData& data, const int maxBoards, const string& skipSpec);
  double BenchmarkCheckpointIntervalSeconds();
  void ReportBenchmarkCheckpoint( const BenchmarkMethodSummary& summary, const unsigned completedBoards, const double elapsedSeconds);
  double BenchmarkProgressIntervalSeconds();
  void ReportBenchmarkBoardTiming( const BenchmarkMethodSummary& summary, const unsigned boardNumber, const double boardElapsedSeconds, const double totalElapsedSeconds);
  /** @brief Benchmark exact DDS solves over a chosen hand-file workload. */
  BenchmarkMethodSummary BenchmarkDDSExactBoards( const string& handFile, const int maxBoards, const string& skipSpec);
  /** @brief Benchmark one-world exact alpha-mu solves over a chosen hand-file workload. */
  BenchmarkMethodSummary BenchmarkAlphaMuExactBoards( const AlphaMuBenchmarkOptions& options);
  /** @brief Compatibility wrapper preserving the legacy positional benchmark API. */
  BenchmarkMethodSummary BenchmarkAlphaMuExactBoards( const string& handFile, const int depth, const int maxBoards, const string& skipSpec);
  /** @brief Print a machine-readable benchmark summary line for one method. */
  void ReportBenchmarkMethodSummary( const BenchmarkMethodSummary& summary);
  /** @brief Compare exact DDS and exact alpha-mu results over multiple depths. */
  DDSVsAlphaMuComparison CompareDDSAndAlphaMu( const string& handFile, const int maxDepth, const int maxBoards);
  /** @brief Print a machine-readable DDS-vs-alpha-mu comparison summary. */
  void ReportDDSVsAlphaMuComparison( const DDSVsAlphaMuComparison& summary);
  /** @brief Verify that DDS leaf best scores match the hand-file goldens. */
  void CheckDDSLeafBestScores( const HandFileData& data, const vector<int>& bestScores);
  /** @brief Evaluate a hand file with serial DDS threshold probes. */
  DDSLeafEvalResult EvaluateDDSLeafThresholdSerial( const HandFileData& data, const int target);
  /** @brief Evaluate a hand file with parallel DDS threshold probes. */
  DDSLeafEvalResult EvaluateDDSLeafThresholdParallel( const HandFileData& data, const int target, const int requestedThreads);

  /** @brief Print a successful prototype status line with the standard prefix. */
  void PrintPrototypeStatus(const string& msg);
}

#endif
