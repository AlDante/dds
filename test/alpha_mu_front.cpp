/*
  alpha_mu front and toy-search helpers

   Copyright © 2026 by David Jenkins
   All rights reserved.
*/

#include "alpha_mu_core.h"

namespace alpha_mu
{
  using namespace std;

OutcomeVector MakeBinaryOutcome(
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
void AddChild(
    ToyNode& parent,
    const ToyNode& child,
    const WorldMask& worlds)
  {
    parent.children.push_back(&child);
    parent.childWorlds.push_back(worlds);
  }
void AddChild(
    ToyNode& parent,
    const ToyNode& child)
  {
    AddChild(parent, child, WorldMask::All(parent.leafFront.worldCount));
  }
string MakeTTKey(
    const ToyNode& node,
    const int maxMoves,
    const WorldMask& usefulWorlds)
  {
    ostringstream oss;
    oss << node.name << "|" << maxMoves << "|" << usefulWorlds.count << "|"
        << usefulWorlds.bits;
    return oss.str();
  }
ParetoFront MakeFront(
    const unsigned worldCount,
    const vector<string>& outcomes)
  {
    ParetoFront front(worldCount);
    for (unsigned i = 0; i < outcomes.size(); i++)
      front.Insert(MakeBinaryOutcome(outcomes[i]));
    return front;
  }
string ResolvePath(const string& candidate)
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
ParetoFront MakeZeroFront(const unsigned worldCount)
  {
    ParetoFront front(worldCount);
    OutcomeVector vec(worldCount);
    vec.valid = WorldMask::All(worldCount);
    front.Insert(vec);
    return front;
  }
ParetoFront MakeSingleWorldFront(
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
unsigned SoleWorldIndex(const WorldMask& mask)
  {
    for (unsigned i = 0; i < mask.count; i++)
    {
      if (mask.Has(i))
        return i;
    }

    throw runtime_error("SoleWorldIndex called on empty mask");
  }
int EvaluateLeafWorld(
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
int EvaluateSingleWorld(
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
  /**
   * Execute the paper-faithful alpha-mu recursion on the toy tree.
   *
   * This function is the clearest statement of the algorithm in the repository:
   *
   * - Max nodes use union/merge of child fronts.
   * - Min nodes use product/min combination of child fronts.
   * - Useful worlds are maintained after each Min backup.
   * - Optimistic completion is used when comparing sparse intermediate fronts.
   * - Early cut checks the nearest Max ancestor.
   * - Deep alpha cut checks earlier Max ancestors.
   * - Cut-on-win stops a Max node once a child wins in all useful worlds.
   * - Root cut stops iterative deepening once the root `mu` value stabilizes.
   * - The transposition table stores only exact fronts.
   *
   * Together, those items correspond to the original alpha-mu paper plus the
   * later optimization paper discussed in `docs/alpha-mu.md`.
   */
ParetoFront SearchToy(
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
      /*
       * Optimization-paper path:
       *   1. combine children with MinProduct,
       *   2. shrink the useful-world set after each child,
       *   3. compare an optimistically completed sparse front against ancestor
       *      Max fronts for early/deep alpha cuts.
       */
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

    /*
     * Original-paper Max path plus optimization-paper cut-on-win/root-cut:
     * merge each child front into the running front, stop early if a child now
     * wins in every useful world, and at the root stop iterative deepening once
     * the reported `mu` value stops changing.
     */
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
  /**
   * Iteratively deepen the toy alpha-mu search in number of Max moves.
   *
   * The root `mu` value from the previous iteration feeds the root-cut test in
   * the next iteration, matching this implementation's interpretation of the later paper.
   */
IterativeResult RunIterativeDeepening(
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

int SingleWorldFrontScore(
    const ParetoFront& front,
    const int depth,
    const int boardIndex)
  {
    Check(front.worldCount == 1,
      "DDS comparison should only inspect single-world alpha-mu fronts");
    Check(front.vectors.size() == 1,
      "single-world alpha-mu search should collapse to one exact vector in DDS comparison mode");
    Check(front.vectors[0].valid == WorldMask(1, 0x1ULL),
      "single-world alpha-mu comparison front should remain valid in the only world");

    (void) depth;
    (void) boardIndex;
    return front.vectors[0].values[0];
  }
}
