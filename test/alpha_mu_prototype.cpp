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
    const bool isRoot,
    const double previousRootMu,
    SearchStats& stats,
    bool& rootCutTriggered)
  {
    stats.nodesVisited++;
    stats.visitOrder.push_back(node.name);

    if (usefulWorlds.Empty())
    {
      stats.worldCutsZero++;
      return MakeZeroFront(node.leafFront.worldCount);
    }

    if (usefulWorlds.PopCount() == 1)
    {
      const unsigned world = SoleWorldIndex(usefulWorlds);
      const int value = EvaluateSingleWorld(node, maxMoves, world, stats);
      stats.worldCutsSingle++;
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
      return node.leafFront.RestrictToUseful(usefulWorlds);
    }

    if (node.type == TOY_MIN)
    {
      ParetoFront mini(node.leafFront.worldCount);
      bool initialized = false;
      WorldMask currentUseful = usefulWorlds;

      for (unsigned i = 0; i < node.children.size(); i++)
      {
        const ParetoFront f = SearchToy(
          * node.children[i],
          maxMoves,
          currentUseful.Intersection(node.childWorlds[i]),
          upperMaxFronts,
          nodeOptimistic,
          false,
          previousRootMu,
          stats,
          rootCutTriggered);

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
          break;
        }

        for (unsigned j = 0; j + 1 < upperMaxFronts.size(); j++)
        {
          if (upperMaxFronts[j]->DominatesFront(optimisticMini))
          {
            stats.deepAlphaCuts++;
            return mini;
          }
        }
      }

      return mini;
    }

    ParetoFront front(node.leafFront.worldCount);
    vector<const ParetoFront *> childUpperMaxFronts(upperMaxFronts);
    childUpperMaxFronts.push_back(&front);
    for (unsigned i = 0; i < node.children.size(); i++)
    {
      const WorldMask childWorlds = usefulWorlds.Intersection(node.childWorlds[i]);
      const ParetoFront f = SearchToy(
        * node.children[i],
        maxMoves - 1,
        childWorlds,
        childUpperMaxFronts,
        nodeOptimistic,
        false,
        previousRootMu,
        stats,
        rootCutTriggered);

      front = ParetoFront::MaxMerge(front, f);

      if (f.WinsAll(usefulWorlds))
      {
        stats.cutOnWinCuts++;
        break;
      }

      if (isRoot && previousRootMu >= 0.0 &&
          fabs(front.Mu() - previousRootMu) < 1e-9)
      {
        stats.rootCuts++;
        rootCutTriggered = true;
        break;
      }
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
      const ParetoFront front = SearchToy(
        root,
        depth,
        WorldMask::All(root.leafFront.worldCount),
        vector<const ParetoFront *>(),
        OutcomeVector(root.leafFront.worldCount),
        true,
        previousMu,
        stats,
        rootCutTriggered);

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
    const ParetoFront front = SearchToy(a, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), true, -1.0, stats,
      rootCutTriggered);

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
    const ParetoFront front = SearchToy(root, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), true, -1.0, stats,
      rootCutTriggered);

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
    const ParetoFront front = SearchToy(candidateMin, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), false, -1.0, stats,
      rootCutTriggered);

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
    const ParetoFront zeroFront = SearchToy(
      zeroLeaf,
      1,
      WorldMask::None(3),
      vector<const ParetoFront *>(),
      OutcomeVector(3),
      false,
      -1.0,
      zeroStats,
      rootCutTriggered);

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
      true,
      -1.0,
      singleStats,
      rootCutTriggered);

    Check(singleStats.worldCutsSingle == 1,
      "single useful world should trigger a single-world cut");
    Check(singleStats.leafWorldEvaluations == 2,
      "single-world cut should evaluate only one world through the collapsed search");
    Check(FrontContains(singleFront, MakeBinaryOutcome("x1x")),
      "single-world cut should return the exact one-world result as a sparse vector");
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
    const ParetoFront front = SearchToy(root, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), true, -1.0, stats,
      rootCutTriggered);

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
    const ParetoFront front = SearchToy(root, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), true, -1.0, stats,
      rootCutTriggered);

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
    const ParetoFront front = SearchToy(root, 3, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), true, -1.0, stats,
      rootCutTriggered);

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
    const ParetoFront front = SearchToy(root, 1, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), true, -1.0, stats,
      rootCutTriggered);

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


int main()
{
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

