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
      for (unsigned i = 0; i < values.size(); i++)
      {
        if (valid.Has(i) && ! useful.Has(i))
          result.values[i] = 0;
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
      for (unsigned i = 0; i < vectors.size(); i++)
      {
        for (unsigned w = 0; w < vectors[i].values.size(); w++)
        {
          if (vectors[i].valid.Has(w) && vectors[i].values[w] > 0)
            useful.bits |= (1ULL << w);
        }
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
    vector<const ToyNode *> children;

    ToyNode(
      const string& nameArg,
      const ToyNodeType typeArg,
      const unsigned worldCount) :
      name(nameArg),
      type(typeArg),
      leafFront(worldCount),
      children()
    {
    }
  };


  struct SearchStats
  {
    int nodesVisited;
    int earlyCuts;
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
    vec.valid = WorldMask::All(n);
    for (unsigned i = 0; i < n; i++)
    {
      if (text[i] == '0' || text[i] == '1')
        vec.values[i] = text[i] - '0';
      else
        throw runtime_error("MakeBinaryOutcome expects only 0/1 text");
    }
    return vec;
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
    vec.valid = WorldMask::All(worldCount);
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
      for (unsigned i = 0; i < node.children.size(); i++)
      {
        const int value = EvaluateSingleWorld(
          * node.children[i],
          maxMoves,
          world,
          stats);
        best = min(best, value);
      }
      return best;
    }

    int best = 0;
    for (unsigned i = 0; i < node.children.size(); i++)
    {
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
    const ParetoFront * upperFront,
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
          currentUseful,
          NULL,
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

        if (upperFront != NULL && upperFront->DominatesFront(mini))
        {
          stats.earlyCuts++;
          break;
        }
      }

      return mini;
    }

    ParetoFront front(node.leafFront.worldCount);
    for (unsigned i = 0; i < node.children.size(); i++)
    {
      const ParetoFront f = SearchToy(
        * node.children[i],
        maxMoves - 1,
        usefulWorlds,
        &front,
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
        NULL,
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
    d.children.push_back(&leaf100);
    d.children.push_back(&leaf011);

    ToyNode e("e", TOY_MAX, 3);
    e.children.push_back(&leaf000a);
    e.children.push_back(&leaf100);

    ToyNode b("b", TOY_MIN, 3);
    b.children.push_back(&d);
    b.children.push_back(&e);

    ToyNode f("f", TOY_MAX, 3);
    f.children.push_back(&leaf000b);

    ToyNode c("c", TOY_MIN, 3);
    c.children.push_back(&f);

    ToyNode a("a", TOY_MAX, 3);
    a.children.push_back(&b);
    a.children.push_back(&c);

    SearchStats stats;
    bool rootCutTriggered = false;
    const ParetoFront front = SearchToy(a, 2, WorldMask::All(3), NULL, true,
      -1.0, stats,
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
    candidateMin.children.push_back(&cutLeaf1);
    candidateMin.children.push_back(&shouldNotVisit);

    ToyNode root("root", TOY_MAX, 3);
    root.children.push_back(&bestLeaf);
    root.children.push_back(&candidateMin);

    SearchStats stats;
    bool rootCutTriggered = false;
    const ParetoFront front = SearchToy(root, 2, WorldMask::All(3), NULL,
      true, -1.0, stats,
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
    candidateMin.children.push_back(&minFirst);
    candidateMin.children.push_back(&minSecond);

    SearchStats stats;
    bool rootCutTriggered = false;
    const ParetoFront front = SearchToy(candidateMin, 2, WorldMask::All(3),
      NULL, false, -1.0, stats, rootCutTriggered);

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
    root.children.push_back(&stableBest);
    root.children.push_back(&skippedByRootCut);

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
      NULL,
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
    root.children.push_back(&left);
    root.children.push_back(&right);

    SearchStats singleStats;
    const WorldMask onlyWorld1(3, 1ULL << 1);
    const ParetoFront singleFront = SearchToy(
      root,
      1,
      onlyWorld1,
      NULL,
      true,
      -1.0,
      singleStats,
      rootCutTriggered);

    Check(singleStats.worldCutsSingle == 1,
      "single useful world should trigger a single-world cut");
    Check(singleStats.leafWorldEvaluations == 2,
      "single-world cut should evaluate only one world through the collapsed search");
    Check(FrontContains(singleFront, MakeBinaryOutcome("010")),
      "single-world cut should return the exact one-world result embedded in a full vector");
  }


  static void TestCutOnWin()
  {
    ToyNode winningMove("winningMove", TOY_LEAF, 3);
    winningMove.leafFront = MakeFront(3, vector<string>(1, "111"));

    ToyNode skippedSibling("skippedSibling", TOY_LEAF, 3);
    skippedSibling.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode root("root", TOY_MAX, 3);
    root.children.push_back(&winningMove);
    root.children.push_back(&skippedSibling);

    SearchStats stats;
    bool rootCutTriggered = false;
    const ParetoFront front = SearchToy(root, 1, WorldMask::All(3), NULL,
      true, -1.0, stats, rootCutTriggered);

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
    HandFileData data;
    LoadHandFile("hands/alpha_mu_play.txt", data);

    Check(data.number == 3,
      "alpha_mu_play.txt should provide three DDS worlds for the leaf demo");

    const int target = 4;
    OutcomeVector leaf(static_cast<unsigned>(data.number));
    leaf.valid = WorldMask::All(static_cast<unsigned>(data.number));

    SetResources(0, 1);

    for (int i = 0; i < data.number; i++)
    {
      futureTricks fut;
      memset(&fut, 0, sizeof(fut));

      const int ret = SolveBoardPBN(data.dealList[i], -1, 1, 1, &fut, 0);
      CheckDDS(ret, "SolveBoardPBN DDS leaf demo");

      const int best = BestScore(fut);
      const int goldenBest = BestScore(data.futList[i]);

      Check(best == goldenBest,
        "DDS leaf demo optimum should match the golden FUT optimum");

      leaf.values[static_cast<unsigned>(i)] = (best >= target ? 1 : 0);
    }

    Check(leaf.ToString() == "[1 1 0]",
      "DDS leaf demo should yield the expected threshold vector [1 1 0]");
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

  TestCutOnWin();
  cout << "alpha_mu_prototype: cut on win OK\n";

  TestRootCutExample();
  cout << "alpha_mu_prototype: root cut toy search OK\n";

  TestDDSLeafDemo();
  cout << "alpha_mu_prototype: DDS leaf demo OK\n";

  cout << "alpha_mu_prototype: all checks passed\n";
  return 0;
}

