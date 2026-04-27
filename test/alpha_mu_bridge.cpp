/*
  alpha_mu bridge-state, legality, and search helpers

   Copyright © 2026 by David Jenkins
   All rights reserved.
*/

#include "alpha_mu_core.h"

namespace alpha_mu
{
  using namespace std;

#ifndef NDEBUG
  /** @brief Enforce the Stage 0 compact-world mask invariant for bridge search. */
  static void DebugCheckWorldMaskCapacity(
    const unsigned worldCount,
    const string& context)
  {
    ostringstream oss;
    oss << context
        << " requires at most 64 worlds because WorldMask is backed by one 64-bit word"
        << " (got " << worldCount << ")";
    Check(worldCount <= 64U, oss.str());
  }

  /** @brief Enforce Stage 0 consistency between the active-world mask and the world vector. */
  static void DebugCheckBridgeStateMaskConsistency(
    const BridgeState& state,
    const string& context)
  {
    DebugCheckWorldMaskCapacity(static_cast<unsigned>(state.worlds.size()), context);
    Check(state.possibleWorlds.count == state.worlds.size(),
      context + " should keep BridgeState.worlds and possibleWorlds.count in sync");
    if (state.possibleWorlds.count < 64U)
    {
      const unsigned long long mask =
        (state.possibleWorlds.count == 0U ? 0ULL :
          ((1ULL << state.possibleWorlds.count) - 1ULL));
      Check((state.possibleWorlds.bits & ~mask) == 0ULL,
        context + " should not set active-world bits outside the compacted world vector");
    }
  }

  /** @brief Enforce Stage 0 partial-trick and seat-order invariants for bridge search. */
  static void DebugCheckBridgeStateTrickConsistency(
    const BridgeState& state,
    const string& context)
  {
    Check(state.currentTrick.size() == state.currentTrickPlayers.size(),
      context + " should keep currentTrick and currentTrickPlayers aligned");
    Check(state.currentTrick.size() <= 3U,
      context + " should only represent partial tricks of size 0..3");
    Check(state.playerToMove >= 0 && state.playerToMove < 4,
      context + " should keep playerToMove within the four bridge seats");
    Check(state.trickLeader >= 0 && state.trickLeader < 4,
      context + " should keep trickLeader within the four bridge seats");
    Check(state.leadSuit >= -1 && state.leadSuit < 4,
      context + " should keep leadSuit in the bridge suit range or -1 when no partial trick exists");
    Check(state.trumpSuit >= -1 && state.trumpSuit < 4,
      context + " should keep trumpSuit in the bridge suit range or -1 for notrump");

    if (state.currentTrick.empty())
    {
      Check(state.leadSuit == -1,
        context + " should clear leadSuit when no partial trick is present");
      return;
    }

    Check(state.leadSuit == state.currentTrick[0].suit,
      context + " should keep leadSuit equal to the first card of the current partial trick");
    for (unsigned i = 0; i < state.currentTrickPlayers.size(); i++)
    {
      const int expectedPlayer = (state.trickLeader + static_cast<int>(i)) % 4;
      Check(state.currentTrickPlayers[i] == expectedPlayer,
        context + " should preserve seat order inside the current partial trick");
    }
    Check(state.playerToMove ==
          (state.trickLeader + static_cast<int>(state.currentTrick.size())) % 4,
      context + " should advance playerToMove by the number of cards already played into the current partial trick");
  }

  /** @brief Enforce Stage 0 sparse-front validity against the searched bridge state. */
  static void DebugCheckFrontForBridgeState(
    const ParetoFront& front,
    const BridgeState& state,
    const string& context)
  {
    Check(front.worldCount == state.possibleWorlds.count,
      context + " should return a front whose world count matches the searched bridge state");
    for (unsigned i = 0; i < front.vectors.size(); i++)
    {
      const OutcomeVector& vec = front.vectors[i];
      Check(vec.valid.count == state.possibleWorlds.count,
        context + " should keep every outcome vector aligned with the searched bridge state world count");
      for (unsigned w = 0; w < state.possibleWorlds.count; w++)
      {
        if (vec.valid.Has(w))
        {
          Check(state.possibleWorlds.Has(w),
            context + " should not report a valid world outside the active bridge-state mask");
        }
      }
    }
  }

  static void DebugCheckDDSLeafWorld(
    const BridgeState& state,
    const ParsedWorld& world,
    const unsigned worldIndex)
  {
    Check(state.currentTrick.size() == state.currentTrickPlayers.size(),
      "DDS leaf validation requires currentTrick and currentTrickPlayers to stay aligned");

    unsigned playedInCurrentTrick[4] = {0, 0, 0, 0};
    const unsigned currentTrickSize =
      static_cast<unsigned>(state.currentTrickPlayers.size());
    for (unsigned i = 0; i < currentTrickSize; i++)
    {
      const int player = state.currentTrickPlayers[i];
      Check(player >= 0 && player < 4,
        "DDS leaf validation requires every current-trick player to be a valid seat");
      playedInCurrentTrick[player]++;
      Check(playedInCurrentTrick[player] <= 1U,
        "DDS leaf validation requires a partial trick to contain at most one card per seat");
    }

    const unsigned reference =
      WorldSeatCardCount(world, 0) + playedInCurrentTrick[0];
    for (int seat = 0; seat < 4; seat++)
    {
      const unsigned normalized =
        WorldSeatCardCount(world, seat) + playedInCurrentTrick[seat];
      if (normalized != reference)
      {
        ostringstream oss;
        oss << "DDS leaf validation found inconsistent remaining hand sizes in world "
            << worldIndex
            << ": normalized seat counts were N="
            << (WorldSeatCardCount(world, SEAT_NORTH) + playedInCurrentTrick[SEAT_NORTH])
            << " E="
            << (WorldSeatCardCount(world, SEAT_EAST) + playedInCurrentTrick[SEAT_EAST])
            << " S="
            << (WorldSeatCardCount(world, SEAT_SOUTH) + playedInCurrentTrick[SEAT_SOUTH])
            << " W="
            << (WorldSeatCardCount(world, SEAT_WEST) + playedInCurrentTrick[SEAT_WEST])
            << " with current trick size " << state.currentTrick.size()
            << " and world " << SerializePBNWorld(world);
        Fail(oss.str());
      }
    }
  }
#endif

#ifndef NDEBUG
  static void DebugCheckBridgeSearchContext(
    const BridgeState& state,
    const SearchExecutionContext& context,
    const string& tag)
  {
    if (context.bridgeSearch.hasUsefulWorlds)
    {
      Check(context.bridgeSearch.usefulWorlds.count == state.possibleWorlds.count,
        tag + " should keep useful-world masks aligned with the bridge-state world count");
      Check((context.bridgeSearch.usefulWorlds.bits & ~state.possibleWorlds.bits) == 0ULL,
        tag + " should keep useful worlds within the currently active bridge-state worlds");
    }

    for (unsigned i = 0; i < context.bridgeSearch.upperMaxFronts.size(); i++)
    {
      Check(context.bridgeSearch.upperMaxFronts[i] != NULL,
        tag + " should not carry null ancestor Max-front pointers");
      Check(context.bridgeSearch.upperMaxFronts[i]->worldCount ==
            state.possibleWorlds.count,
        tag + " should keep ancestor Max fronts aligned with the searched bridge-state world count");
    }

    if (context.bridgeSearch.hasOptimisticValues)
    {
      Check(context.bridgeSearch.optimisticValues.valid.count ==
            state.possibleWorlds.count,
        tag + " should keep optimistic completion values aligned with the searched bridge-state world count");
      Check(context.bridgeSearch.optimisticValues.values.size() ==
            state.possibleWorlds.count,
        tag + " should keep optimistic completion storage aligned with the searched bridge-state world count");
    }
  }
#endif

  static WorldMask BridgeSearchUsefulWorlds(
    const BridgeState& state,
    const SearchExecutionContext& context)
  {
    if (! context.bridgeSearch.hasUsefulWorlds)
      return state.possibleWorlds;

    return context.bridgeSearch.usefulWorlds.Intersection(state.possibleWorlds);
  }

  static SearchExecutionContext WithBridgeSearchUsefulWorlds(
    const SearchExecutionContext& context,
    const WorldMask& usefulWorlds)
  {
    SearchExecutionContext childContext(context);
    childContext.bridgeSearch.hasUsefulWorlds = true;
    childContext.bridgeSearch.usefulWorlds = usefulWorlds;
    return childContext;
  }

  static SearchExecutionContext WithBridgeSearchUpperMaxFront(
    const SearchExecutionContext& context,
    const ParetoFront* upperMaxFront)
  {
    SearchExecutionContext childContext(context);
    if (childContext.bridgeSearch.enableAncestorCuts && upperMaxFront != NULL)
      childContext.bridgeSearch.upperMaxFronts.push_back(upperMaxFront);
    return childContext;
  }

  static OutcomeVector MakeBridgeOptimisticValues(
    const BridgeState& state,
    const SearchExecutionContext& context)
  {
    OutcomeVector optimistic(state.possibleWorlds.count);
    optimistic.valid = state.possibleWorlds;
    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      if (state.possibleWorlds.Has(i))
        optimistic.values[i] = state.maxTricksWon +
          RemainingTricksInWorld(state, state.worlds[i]);
    }

    if (context.bridgeSearch.hasOptimisticValues)
    {
      for (unsigned i = 0; i < optimistic.values.size(); i++)
      {
        if (context.bridgeSearch.optimisticValues.valid.Has(i))
          optimistic.values[i] = max(optimistic.values[i],
            context.bridgeSearch.optimisticValues.values[i]);
      }
    }

    return optimistic;
  }

  enum BridgeAlphaCutType
  {
    BRIDGE_ALPHA_CUT_NONE = 0,
    BRIDGE_ALPHA_CUT_EARLY = 1,
    BRIDGE_ALPHA_CUT_DEEP = 2
  };

  static BridgeAlphaCutType BridgeAlphaCutTypeForFront(
    const BridgeState& state,
    const WorldMask& usefulWorlds,
    const ParetoFront& front,
    const SearchExecutionContext& context)
  {
    if (! context.bridgeSearch.enableAncestorCuts ||
        context.bridgeSearch.upperMaxFronts.empty())
    {
      return BRIDGE_ALPHA_CUT_NONE;
    }

    const ParetoFront optimisticFront = front.CompleteOptimistically(
      usefulWorlds,
      MakeBridgeOptimisticValues(state, context));
    if (optimisticFront.ValidWorlds().PopCount() > front.ValidWorlds().PopCount())
      NoteOptimisticCompletion();

    if (context.bridgeSearch.upperMaxFronts.back()->DominatesFront(
          optimisticFront))
    {
      return BRIDGE_ALPHA_CUT_EARLY;
    }

    for (unsigned i = 0; i + 1U < context.bridgeSearch.upperMaxFronts.size(); i++)
    {
      if (context.bridgeSearch.upperMaxFronts[i]->DominatesFront(optimisticFront))
        return BRIDGE_ALPHA_CUT_DEEP;
    }

    return BRIDGE_ALPHA_CUT_NONE;
  }

  static bool ShouldBridgeAlphaCut(
    const BridgeState& state,
    const WorldMask& usefulWorlds,
    const ParetoFront& front,
    const SearchExecutionContext& context)
  {
    const BridgeAlphaCutType cutType = BridgeAlphaCutTypeForFront(state,
      usefulWorlds, front, context);
    if (cutType == BRIDGE_ALPHA_CUT_EARLY)
    {
      NoteEarlyAlphaCut();
      return true;
    }
    else if (cutType == BRIDGE_ALPHA_CUT_DEEP)
    {
      NoteDeepAlphaCut();
      return true;
    }

    return false;
  }

  static ParetoFront MakeBridgeZeroFront(const BridgeState& state)
  {
    ParetoFront front(state.possibleWorlds.count);
    OutcomeVector vec(state.possibleWorlds.count);
    vec.valid = state.possibleWorlds;
    front.Insert(vec);
    return front;
  }

  static bool CanUseBridgeSingleWorldCut(
    const BridgeState& state,
    const unsigned worldIndex)
  {
    return worldIndex < state.worlds.size() &&
      RemainingTricksInWorld(state, state.worlds[worldIndex]) >= 3;
  }

  dealPBN MakeDDSDealPBN(
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

  int RemainingTricksInWorld(
    const BridgeState& state,
    const ParsedWorld& world)
  {
    return static_cast<int>((WorldCardCount(world) + state.currentTrick.size()) / 4U);
  }

  bool BridgeMoveLess(
    const BridgeMove& left,
    const BridgeMove& right)
  {
    if (left.suit != right.suit)
      return left.suit < right.suit;
    return RankOrder(left.rank) < RankOrder(right.rank);
  }

  int SeatSide(const int seat)
  {
    return (seat == SEAT_NORTH || seat == SEAT_SOUTH ? 0 : 1);
  }

  unsigned WinningCardIndex(
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

  int TrickWinner(
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

  bool WorldHasLegalSuit(
    const ParsedWorld& world,
    const int player,
    const int leadSuit)
  {
    return leadSuit >= 0 && WorldSuitLength(world, player, leadSuit) > 0;
  }

  vector<BridgeMove> LegalMovesInWorld(
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

  bool ContainsMove(
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

  bool WorldCanPlayMove(
    const ParsedWorld& world,
    const int player,
    const int leadSuit,
    const BridgeMove& move)
  {
    return ContainsMove(LegalMovesInWorld(world, player, leadSuit), move);
  }

  void RemoveCardFromWorld(
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

  vector<BridgeMove> GenerateBridgeMoves(const BridgeState& state)
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

  BridgeState PlayBridgeMove(
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

  vector<BridgeChild> ExpandBridgeChildren(const BridgeState& state)
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

  ParetoFront MakeBridgeTerminalFront(const BridgeState& state)
  {
    NoteTerminalFront();
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

  ParetoFront MakeBridgeDDSLeafFront(
    const BridgeState& state,
    const SearchExecutionContext& context)
  {
#ifndef NDEBUG
    DebugCheckBridgeStateMaskConsistency(state,
      "MakeBridgeDDSLeafFront");
    DebugCheckBridgeStateTrickConsistency(state,
      "MakeBridgeDDSLeafFront");
#endif
    vector<unsigned> active;
    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      if (state.possibleWorlds.Has(i))
        active.push_back(i);
    }

    if (active.empty())
    {
      NoteDDSLeafCut();
      return MakeZeroFront(state.possibleWorlds.count);
    }

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
    {
      NoteDDSLeafCut();
      return MakeBridgeTerminalFront(state);
    }

#ifndef NDEBUG
    for (unsigned i = 0; i < active.size(); i++)
      DebugCheckDDSLeafWorld(state, state.worlds[active[i]], active[i]);
#endif

    ParetoFront front(state.possibleWorlds.count);
    OutcomeVector vec(state.possibleWorlds.count);
    vec.valid = state.possibleWorlds;

    for (unsigned i = 0; i < active.size(); i++)
    {
      const unsigned worldIndex = active[i];
      futureTricks fut;
      memset(&fut, 0, sizeof(fut));

      if (context.benchmarkProgress != NULL)
        context.benchmarkProgress->ddsLeafCalls++;

      const dealPBN deal = MakeDDSDealPBN(state, state.worlds[worldIndex]);
      const chrono::steady_clock::time_point ddsStart =
        chrono::steady_clock::now();
      const int ret = SolveBoardPBN(deal, -1, 1, 1, &fut,
        context.ddsThreadId);
      const double ddsElapsed = chrono::duration<double>(
        chrono::steady_clock::now() - ddsStart).count();
      if (context.benchmarkProgress != NULL)
        context.benchmarkProgress->ddsLeafSeconds += ddsElapsed;
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
#ifndef NDEBUG
    DebugCheckFrontForBridgeState(front, state,
      "MakeBridgeDDSLeafFront");
#endif
    return front;
  }

  ParetoFront MakeBridgeDDSLeafFront(const BridgeState& state)
  {
    return MakeBridgeDDSLeafFront(state, SearchExecutionContext());
  }

  int BridgeDepthCost(
    const BridgeState& state,
    const BridgeState& child)
  {
    return (state.currentTrick.size() == 3 && child.currentTrick.empty() ? 1 : 0);
  }

  static ParetoFront SearchBridgeStateInternalExact(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context,
    bool * exactComplete)
  {
#ifndef NDEBUG
    DebugCheckBridgeStateMaskConsistency(state,
      "SearchBridgeStateInternalExact");
    DebugCheckBridgeStateTrickConsistency(state,
      "SearchBridgeStateInternalExact");
    DebugCheckBridgeSearchContext(state, context,
      "SearchBridgeStateInternalExact");
#endif
    MaybeReportBenchmarkBoardProgress(state, tricksRemaining, context);

    const WorldMask usefulWorlds = BridgeSearchUsefulWorlds(state, context);

    if (usefulWorlds.Empty())
    {
      NoteEmptyWorldCut();
      if (exactComplete != NULL)
        *exactComplete = true;
      return MakeBridgeZeroFront(state);
    }

    if (usefulWorlds.PopCount() == 1U)
    {
      const unsigned world = SoleWorldIndex(usefulWorlds);
      if (CanUseBridgeSingleWorldCut(state, world))
      {
        const int value = ExactBridgeDDSScoreForWorld(state, world, context);
        if (exactComplete != NULL)
          *exactComplete = true;
        return MakeSingleWorldFront(state.possibleWorlds.count, world, value);
      }
    }

    if (tricksRemaining <= 0)
    {
      NoteDDSLeafCut();
      if (exactComplete != NULL)
        *exactComplete = true;
      return MakeBridgeDDSLeafFront(state, context);
    }

    const vector<BridgeChild> children = ExpandBridgeChildren(state);
    if (children.empty())
    {
      NoteNoMoveLeafCut();
      if (exactComplete != NULL)
        *exactComplete = true;
      return MakeBridgeDDSLeafFront(state, context);
    }

    if (SeatSide(state.playerToMove) == state.maxSide)
    {
      ParetoFront front(state.possibleWorlds.count);
      bool complete = true;
      for (unsigned i = 0; i < children.size(); i++)
      {
        const int nextDepth = tricksRemaining - BridgeDepthCost(state,
          children[i].state);
        SearchExecutionContext childContext = WithBridgeSearchUsefulWorlds(
          context,
          usefulWorlds.Intersection(children[i].state.possibleWorlds));
        childContext = WithBridgeSearchUpperMaxFront(childContext, &front);
        bool childComplete = false;
        const ParetoFront childFront = SearchBridgeStateInternalExact(
          children[i].state, nextDepth, childContext, &childComplete);
        front = ParetoFront::MaxMerge(front, childFront);
        complete = complete && childComplete;
        if (context.bridgeSearch.enableAncestorCuts &&
            i + 1U < children.size() &&
            childFront.WinsAll(usefulWorlds))
        {
          NoteCutOnWinCut();
          complete = false;
          break;
        }
      }
#ifndef NDEBUG
      DebugCheckFrontForBridgeState(front, state,
        "SearchBridgeStateInternalExact Max path");
#endif
      if (exactComplete != NULL)
        *exactComplete = complete;
      return front;
    }

    ParetoFront front(state.possibleWorlds.count);
    bool initialized = false;
    WorldMask currentUseful = usefulWorlds;
    bool complete = true;
    for (unsigned i = 0; i < children.size(); i++)
    {
      const int nextDepth = tricksRemaining - BridgeDepthCost(state,
        children[i].state);
      const SearchExecutionContext childContext =
        WithBridgeSearchUsefulWorlds(context,
          currentUseful.Intersection(children[i].state.possibleWorlds));
      bool childComplete = false;
      const ParetoFront childFront = SearchBridgeStateInternalExact(
        children[i].state,
        nextDepth,
        childContext,
        &childComplete);
      complete = complete && childComplete;
      if (! initialized)
      {
        front = childFront;
        initialized = true;
      }
      else
        front = ParetoFront::MinProduct(front, childFront);

      currentUseful = currentUseful.Intersection(front.UsefulWorlds());
      if (ShouldBridgeAlphaCut(state, currentUseful, front, context))
      {
        complete = false;
        break;
      }
    }
#ifndef NDEBUG
    DebugCheckFrontForBridgeState(front, state,
      "SearchBridgeStateInternalExact Min path");
#endif
    if (exactComplete != NULL)
      *exactComplete = complete;
    return front;
  }

  ParetoFront SearchBridgeStateInternal(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context)
  {
    bool exactComplete = false;
    return SearchBridgeStateInternalExact(state, tricksRemaining, context,
      &exactComplete);
  }

  ParetoFront SearchBridgeStateInternal(
    const BridgeState& state,
    const int tricksRemaining)
  {
    return SearchBridgeStateInternal(state, tricksRemaining,
      SearchExecutionContext());
  }

  ParetoFront SearchBridgeState(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context)
  {
    return SearchBridgeStateInternal(state, tricksRemaining, context);
  }

  ParetoFront SearchBridgeState(
    const BridgeState& state,
    const int tricksRemaining)
  {
    return SearchBridgeStateInternal(state, tricksRemaining,
      SearchExecutionContext());
  }

  static ParetoFront SearchBridgeStateWithTTExact(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context,
    BridgeTranspositionTable* tt,
    BridgeTTStats* ttStats,
    bool * exactComplete)
  {
#ifndef NDEBUG
    DebugCheckBridgeStateMaskConsistency(state,
      "SearchBridgeStateWithTTExact");
    DebugCheckBridgeStateTrickConsistency(state,
      "SearchBridgeStateWithTTExact");
    DebugCheckBridgeSearchContext(state, context,
      "SearchBridgeStateWithTTExact");
#endif
    MaybeReportBenchmarkBoardProgress(state, tricksRemaining, context);

    const WorldMask usefulWorlds = BridgeSearchUsefulWorlds(state, context);

    unsigned long long hash = 0;
    if (tt != NULL)
    {
      hash = HashBridgeState(state);
      if (ttStats != NULL)
        ttStats->probes++;

      const ParetoFront* cached = tt->Probe(hash, usefulWorlds.bits);
      if (cached != NULL)
      {
        if (ttStats != NULL)
          ttStats->hits++;
        NoteTTCut();
#ifndef NDEBUG
        DebugCheckFrontForBridgeState(*cached, state,
          "SearchBridgeStateWithTTExact cached TT front");
#endif
        if (exactComplete != NULL)
          *exactComplete = true;
        return *cached;
      }
    }

    if (usefulWorlds.Empty())
    {
      NoteEmptyWorldCut();
      const ParetoFront zeroFront = MakeBridgeZeroFront(state);
      if (exactComplete != NULL)
        *exactComplete = true;
      if (tt != NULL)
      {
        if (ttStats != NULL)
          ttStats->stores++;
        tt->Store(hash, usefulWorlds.bits, zeroFront);
      }
      return zeroFront;
    }

    if (usefulWorlds.PopCount() == 1U)
    {
      const unsigned world = SoleWorldIndex(usefulWorlds);
      if (CanUseBridgeSingleWorldCut(state, world))
      {
        const int value = ExactBridgeDDSScoreForWorld(state, world, context);
        const ParetoFront singleFront = MakeSingleWorldFront(
          state.possibleWorlds.count, world, value);
        if (exactComplete != NULL)
          *exactComplete = true;
        if (tt != NULL)
        {
          if (ttStats != NULL)
            ttStats->stores++;
          tt->Store(hash, usefulWorlds.bits, singleFront);
        }
        return singleFront;
      }
    }

    if (tricksRemaining <= 0)
    {
      NoteDDSLeafCut();
      if (exactComplete != NULL)
        *exactComplete = true;
      return MakeBridgeDDSLeafFront(state, context);
    }


    const vector<BridgeChild> children = ExpandBridgeChildren(state);
    if (children.empty())
    {
      NoteNoMoveLeafCut();
      if (exactComplete != NULL)
        *exactComplete = true;
      return MakeBridgeDDSLeafFront(state, context);
    }

    ParetoFront front(state.possibleWorlds.count);
    bool complete = true;

    if (SeatSide(state.playerToMove) == state.maxSide)
    {
      for (unsigned i = 0; i < children.size(); i++)
      {
        const int nextDepth = tricksRemaining - BridgeDepthCost(state,
          children[i].state);
        SearchExecutionContext childContext = WithBridgeSearchUsefulWorlds(
          context,
          usefulWorlds.Intersection(children[i].state.possibleWorlds));
        childContext = WithBridgeSearchUpperMaxFront(childContext, &front);
        bool childComplete = false;
        const ParetoFront childFront = SearchBridgeStateWithTTExact(
          children[i].state, nextDepth, childContext, tt, ttStats,
          &childComplete);
        front = ParetoFront::MaxMerge(front, childFront);
        complete = complete && childComplete;
        if (context.bridgeSearch.enableAncestorCuts &&
            i + 1U < children.size() &&
            childFront.WinsAll(usefulWorlds))
        {
          NoteCutOnWinCut();
          complete = false;
          break;
        }
      }
    }
    else
    {
      bool initialized = false;
      WorldMask currentUseful = usefulWorlds;
      for (unsigned i = 0; i < children.size(); i++)
      {
        const int nextDepth = tricksRemaining - BridgeDepthCost(state,
          children[i].state);
        const SearchExecutionContext childContext =
          WithBridgeSearchUsefulWorlds(context,
            currentUseful.Intersection(children[i].state.possibleWorlds));
        bool childComplete = false;
        const ParetoFront childFront = SearchBridgeStateWithTTExact(
          children[i].state, nextDepth, childContext, tt, ttStats,
          &childComplete);
        complete = complete && childComplete;
        if (! initialized)
        {
          front = childFront;
          initialized = true;
        }
        else
          front = ParetoFront::MinProduct(front, childFront);

        currentUseful = currentUseful.Intersection(front.UsefulWorlds());
        if (ShouldBridgeAlphaCut(state, currentUseful, front, context))
        {
          complete = false;
          break;
        }
      }
    }

    if (tt != NULL && complete)
    {
      if (ttStats != NULL)
        ttStats->stores++;
#ifndef NDEBUG
      DebugCheckFrontForBridgeState(front, state,
        "SearchBridgeStateWithTTExact stored TT front");
#endif
      tt->Store(hash, usefulWorlds.bits, front);
    }

#ifndef NDEBUG
    DebugCheckFrontForBridgeState(front, state,
      "SearchBridgeStateWithTTExact result");
#endif
    if (exactComplete != NULL)
      *exactComplete = complete;
    return front;
  }

  ParetoFront SearchBridgeStateWithTT(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context,
    BridgeTranspositionTable* tt,
    BridgeTTStats* ttStats)
  {
    bool exactComplete = false;
    return SearchBridgeStateWithTTExact(state, tricksRemaining, context, tt,
      ttStats, &exactComplete);
  }

  BridgeRootReport AnalyzeBridgeRoot(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context)
  {
    BridgeRootReport report(state.possibleWorlds.count);
    if (state.possibleWorlds.Empty())
      return report;

    const vector<BridgeChild> children = ExpandBridgeChildren(state);
    for (unsigned i = 0; i < children.size(); i++)
    {
      BridgeRootChildReport childReport(state.possibleWorlds.count);
      childReport.move = children[i].move;
      const int nextDepth = tricksRemaining - BridgeDepthCost(state,
        children[i].state);
      BenchmarkBoardProgressContext childProgress;
      childProgress.totalStart = chrono::steady_clock::now();
      childProgress.boardStart = childProgress.totalStart;
      childProgress.reportIntervalSeconds = 1.0e30;
      childProgress.nextReportSeconds = 1.0e30;
      SearchExecutionContext childContext(context);
      childContext.benchmarkProgress = &childProgress;
      childReport.front = SearchBridgeState(children[i].state, nextDepth,
        childContext);
#ifndef NDEBUG
      DebugCheckFrontForBridgeState(childReport.front, children[i].state,
        "AnalyzeBridgeRoot child front");
#endif
      childReport.validWorlds = childReport.front.ValidWorlds();
      childReport.usefulWorlds = childReport.front.UsefulWorlds();
      childReport.mu = childReport.front.Mu();
      childReport.searchNodes = static_cast<unsigned>(childProgress.recursiveCalls);
      childReport.ddsLeafCalls = static_cast<unsigned>(childProgress.ddsLeafCalls);
      childReport.ddsLeafSeconds = childProgress.ddsLeafSeconds;
      report.searchNodes += childReport.searchNodes;
      report.ddsLeafCalls += childReport.ddsLeafCalls;
      report.ddsLeafSeconds += childReport.ddsLeafSeconds;
      report.children.push_back(childReport);
      report.rootFront = ParetoFront::MaxMerge(report.rootFront,
        childReport.front);
    }

    report.validWorlds = report.rootFront.ValidWorlds();
    report.usefulWorlds = report.rootFront.UsefulWorlds();

    return report;
  }

  BridgeRootReport AnalyzeBridgeRoot(
    const BridgeState& state,
    const int tricksRemaining)
  {
    return AnalyzeBridgeRoot(state, tricksRemaining, SearchExecutionContext());
  }

  BridgeRootReport AnalyzeBridgeRootWithTT(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context,
    BridgeTranspositionTable* tt,
    BridgeTTStats* ttStats,
    const BridgeRootReport* previousReport)
  {
    BridgeRootReport report(state.possibleWorlds.count);
    if (state.possibleWorlds.Empty())
      return report;

    vector<BridgeChild> children = ExpandBridgeChildren(state);
    if (children.empty())
      return report;

    if (previousReport != NULL && ! previousReport->children.empty())
    {
      map<string, double> moveMu;
      for (unsigned i = 0; i < previousReport->children.size(); i++)
      {
        const BridgeMove& m = previousReport->children[i].move;
        const string key = SuitName(m.suit) + string(1, m.rank);
        moveMu[key] = previousReport->children[i].front.Mu();
      }

      for (unsigned i = 0; i < children.size(); i++)
      {
        for (unsigned j = i + 1; j < children.size(); j++)
        {
          const string ki = SuitName(children[i].move.suit) +
            string(1, children[i].move.rank);
          const string kj = SuitName(children[j].move.suit) +
            string(1, children[j].move.rank);
          const double mi = (moveMu.count(ki) ? moveMu[ki] : -1.0);
          const double mj = (moveMu.count(kj) ? moveMu[kj] : -1.0);
          if (mj > mi)
            swap(children[i], children[j]);
        }
      }
    }

    const double previousRootMu = (previousReport == NULL ? -1.0 :
      previousReport->rootFront.Mu());

    for (unsigned i = 0; i < children.size(); i++)
    {
      BridgeRootChildReport childReport(state.possibleWorlds.count);
      childReport.move = children[i].move;
      const int nextDepth = tricksRemaining - BridgeDepthCost(state,
        children[i].state);
      BenchmarkBoardProgressContext childProgress;
      childProgress.totalStart = chrono::steady_clock::now();
      childProgress.boardStart = childProgress.totalStart;
      childProgress.reportIntervalSeconds = 1.0e30;
      childProgress.nextReportSeconds = 1.0e30;
      SearchExecutionContext childContext(context);
      childContext.benchmarkProgress = &childProgress;
      childReport.front = SearchBridgeStateWithTT(
        children[i].state, nextDepth, childContext, tt, ttStats);
#ifndef NDEBUG
      DebugCheckFrontForBridgeState(childReport.front, children[i].state,
        "AnalyzeBridgeRootWithTT child front");
#endif
      childReport.validWorlds = childReport.front.ValidWorlds();
      childReport.usefulWorlds = childReport.front.UsefulWorlds();
      childReport.mu = childReport.front.Mu();
      childReport.searchNodes = static_cast<unsigned>(childProgress.recursiveCalls);
      childReport.ddsLeafCalls = static_cast<unsigned>(childProgress.ddsLeafCalls);
      childReport.ddsLeafSeconds = childProgress.ddsLeafSeconds;
      report.searchNodes += childReport.searchNodes;
      report.ddsLeafCalls += childReport.ddsLeafCalls;
      report.ddsLeafSeconds += childReport.ddsLeafSeconds;
      report.children.push_back(childReport);
      report.rootFront = ParetoFront::MaxMerge(report.rootFront,
        childReport.front);
      if (context.bridgeSearch.enableAncestorCuts && previousRootMu >= 0.0 &&
          fabs(report.rootFront.Mu() - previousRootMu) < 1e-9)
      {
        NoteRootCut();
        break;
      }
    }

    report.validWorlds = report.rootFront.ValidWorlds();
    report.usefulWorlds = report.rootFront.UsefulWorlds();

    return report;
  }

  int BestScore(const futureTricks& fut)
  {
    if (fut.cards <= 0)
      return 0;

    int best = fut.score[0];
    for (int i = 1; i < fut.cards; i++)
      best = max(best, fut.score[i]);
    return best;
  }

  void CheckDDS(const int ret, const string& tag)
  {
    if (ret == RETURN_NO_FAULT)
      return;

    char line[80];
    ErrorMessage(ret, line);
    ostringstream oss;
    oss << tag << " failed: " << line;
    Fail(oss.str());
  }

  int ExactBridgeDDSScoreForWorld(
    const BridgeState& state,
    const unsigned worldIndex,
    const SearchExecutionContext& context)
  {
    futureTricks fut;
    memset(&fut, 0, sizeof(fut));

    const dealPBN deal = MakeDDSDealPBN(state, state.worlds[worldIndex]);
    const int ret = SolveBoardPBN(deal, -1, 1, 1, &fut,
      context.ddsThreadId);
    CheckDDS(ret, "SolveBoardPBN exact bridge DDS score");

    const int best = BestScore(fut);
    const int tricksRemaining = RemainingTricksInWorld(state,
      state.worlds[worldIndex]);
    const int maxAdditional =
      (SeatSide(state.playerToMove) == state.maxSide ?
        best : tricksRemaining - best);
    return state.maxTricksWon + maxAdditional;
  }

  int ExactBridgeDDSScoreForWorld(
    const BridgeState& state,
    const unsigned worldIndex)
  {
    return ExactBridgeDDSScoreForWorld(state, worldIndex,
      SearchExecutionContext());
  }

  BridgeState MakeBridgeStateFromDDSDeal(
    const dealPBN& deal)
  {
    BridgeState state;
    state.worlds.push_back(ParsePBNWorld(deal.remainCards));
    state.possibleWorlds = WorldMask(1, 0x1ULL);
    state.trumpSuit = (deal.trump == 4 ? -1 : deal.trump);
    state.trickLeader = deal.first;

    for (int i = 0; i < 3; i++)
    {
      if (deal.currentTrickRank[i] == 0)
        break;

      state.currentTrick.push_back(BridgeMove(
        deal.currentTrickSuit[i],
        RankFromDDSValue(deal.currentTrickRank[i])));
      state.currentTrickPlayers.push_back((deal.first + i) % 4);
    }

    state.leadSuit = (state.currentTrick.empty() ? -1 :
      state.currentTrick[0].suit);
    state.playerToMove =
      (deal.first + static_cast<int>(state.currentTrick.size())) % 4;
    state.maxSide = SeatSide(state.playerToMove);
    state.maxTricksWon = 0;
    return state;
  }

  int SuitFromPlayChar(const char c)
  {
    switch (c)
    {
      case 'S': return SUIT_SPADES;
      case 'H': return SUIT_HEARTS;
      case 'D': return SUIT_DIAMONDS;
      case 'C': return SUIT_CLUBS;
      default:
        throw runtime_error("Unknown suit character in play string");
    }
  }

  vector<PlayHistoryEvent> ParsePBNPlayHistory(
    const playTracePBN& play,
    const int openingLeader,
    const int trumpSuit)
  {
    vector<PlayHistoryEvent> events;
    const string cards(play.cards);
    if (cards.empty() || play.number <= 0)
      return events;

    const unsigned requiredLength = static_cast<unsigned>(play.number) * 2U;
    if (cards.size() < requiredLength)
    {
      throw runtime_error(
        "play string too short for declared play.number ("
        + to_string(cards.size()) + " chars for "
        + to_string(play.number) + " cards, need "
        + to_string(requiredLength) + ")");
    }

    if (play.number > 52)
      throw runtime_error("play.number exceeds maximum of 52 cards");

    if (openingLeader < 0 || openingLeader > 3)
      throw runtime_error("opening leader must be a valid seat (0-3)");

    const string validRanks = "AKQJT98765432";

    int currentPlayer = openingLeader;
    int leadSuit = -1;
    unsigned cardsInTrick = 0;
    vector<BridgeMove> trickCards;
    vector<int> trickPlayers;

    for (int i = 0; i < play.number; i++)
    {
      const unsigned pos = static_cast<unsigned>(i) * 2U;

      const char suitChar = cards[pos];
      if (suitChar != 'S' && suitChar != 'H' &&
          suitChar != 'D' && suitChar != 'C')
      {
        throw runtime_error(
          "invalid suit character '" + string(1, suitChar)
          + "' at position " + to_string(pos) + " in play string");
      }

      const int suit = SuitFromPlayChar(suitChar);
      const char rank = cards[pos + 1];

      if (validRanks.find(rank) == string::npos)
      {
        throw runtime_error(
          "invalid rank character '" + string(1, rank)
          + "' at position " + to_string(pos + 1) + " in play string");
      }

      if (cardsInTrick == 0)
        leadSuit = suit;

      events.push_back(PlayHistoryEvent(currentPlayer, leadSuit,
        BridgeMove(suit, rank)));

      trickCards.push_back(BridgeMove(suit, rank));
      trickPlayers.push_back(currentPlayer);
      cardsInTrick++;

      if (cardsInTrick == 4)
      {
        const unsigned winIdx = WinningCardIndex(trickCards, leadSuit, trumpSuit);
        currentPlayer = trickPlayers[winIdx];
        trickCards.clear();
        trickPlayers.clear();
        cardsInTrick = 0;
        leadSuit = -1;
      }
      else
      {
        currentPlayer = (currentPlayer + 1) % 4;
      }
    }
    return events;
  }

  static vector<unsigned> SelectDeterministicWorldIndices(
    const vector<ParsedWorld>& worlds,
    const vector<unsigned>& candidates,
    const unsigned limit,
    const unsigned samplingSeed)
  {
    if (limit == 0 || candidates.size() <= limit)
      return candidates;

    vector<unsigned> ordered(candidates);
    sort(ordered.begin(), ordered.end(),
      [&](const unsigned left, const unsigned right)
      {
        const string leftKey = SerializePBNWorld(worlds[left]);
        const string rightKey = SerializePBNWorld(worlds[right]);
        if (leftKey != rightKey)
          return leftKey < rightKey;
        return left < right;
      });

    vector<unsigned> selected;
    const unsigned offset = samplingSeed % static_cast<unsigned>(ordered.size());
    for (unsigned i = 0; i < limit; i++)
      selected.push_back(ordered[(offset + i) % ordered.size()]);

    sort(selected.begin(), selected.end());
    return selected;
  }

  BridgeState MakeBridgeStateFromInformationState(
    const dealPBN& fullDeal,
    const int declarerSeat,
    const vector<PlayHistoryEvent>& playedCards,
    const BridgeInformationState& information,
    WorldGenerationStats* worldGenerationStats,
    DecisionWorldPipelineResult* decisionWorldPipeline)
  {
    const HistoryDerivedWorldSpec spec =
      BuildWorldSpecFromDeal(fullDeal, declarerSeat, playedCards);
    const HistoryDerivedConstructionResult constructed =
      ConstructCandidateWorldsFromHistory(spec, information);

    BridgeInformationState pipelineInformation(information);
    pipelineInformation.followSuitConstraints =
      CollectFollowSuitConstraints(information);
    pipelineInformation.deriveFollowSuitConstraints = false;
    pipelineInformation.playHistory.clear();
    pipelineInformation.currentTrickHistory.clear();
    const DecisionWorldPipelineResult pipeline =
      BuildDecisionWorldPipeline(constructed.worlds, pipelineInformation);
    if (worldGenerationStats != NULL)
      *worldGenerationStats = pipeline.stats;
    if (decisionWorldPipeline != NULL)
      *decisionWorldPipeline = pipeline;

    const vector<ParsedWorld>& worlds = pipeline.activeWorlds;

    WorldMask possibleMask;
    possibleMask.count = static_cast<unsigned>(worlds.size());
    possibleMask.bits = 0ULL;
    for (unsigned w = 0; w < worlds.size(); w++)
      possibleMask.bits |= (1ULL << w);

#ifndef NDEBUG
    DebugCheckWorldMaskCapacity(static_cast<unsigned>(worlds.size()),
      "MakeBridgeStateFromInformationState");
    Check(possibleMask.count == worlds.size(),
      "MakeBridgeStateFromInformationState should keep WorldMask count aligned with the compacted world vector");
    Check(possibleMask.PopCount() == worlds.size(),
      "MakeBridgeStateFromInformationState should leave every compacted world active initially");
#endif

    BridgeState state;
    state.worlds = worlds;
    state.possibleWorlds = possibleMask;

    const int trumpSuit = (fullDeal.trump == 4 ? -1 : fullDeal.trump);
    state.trumpSuit = trumpSuit;

    unsigned cardsInCurrentTrick = playedCards.size() % 4;
    if (cardsInCurrentTrick > 0)
    {
      unsigned trickStart = playedCards.size() - cardsInCurrentTrick;
      state.trickLeader = playedCards[trickStart].player;
      state.leadSuit = playedCards[trickStart].move.suit;
      for (unsigned i = trickStart; i < playedCards.size(); i++)
      {
        state.currentTrick.push_back(playedCards[i].move);
        state.currentTrickPlayers.push_back(playedCards[i].player);
      }
      state.playerToMove =
        (state.trickLeader + static_cast<int>(cardsInCurrentTrick)) % 4;
    }
    else if (! playedCards.empty())
    {
      unsigned lastTrickStart = playedCards.size() - 4;
      vector<BridgeMove> lastTrick;
      vector<int> lastPlayers;
      for (unsigned i = lastTrickStart; i < playedCards.size(); i++)
      {
        lastTrick.push_back(playedCards[i].move);
        lastPlayers.push_back(playedCards[i].player);
      }
      const unsigned winIdx = WinningCardIndex(lastTrick,
        playedCards[lastTrickStart].move.suit, trumpSuit);
      state.trickLeader = lastPlayers[winIdx];
      state.playerToMove = state.trickLeader;
      state.leadSuit = -1;
    }
    else
    {
      state.trickLeader = fullDeal.first;
      state.playerToMove = fullDeal.first;
      state.leadSuit = -1;
    }

    state.maxSide = SeatSide(state.playerToMove);
    state.maxTricksWon = 0;
#ifndef NDEBUG
    DebugCheckBridgeStateMaskConsistency(state,
      "MakeBridgeStateFromInformationState");
#endif
    return state;
  }

  BridgeState MakeBridgeStateFromPartialInformation(
    const dealPBN& fullDeal,
    const int declarerSeat,
    const vector<PlayHistoryEvent>& playedCards,
    const unsigned maxWorlds,
    const unsigned samplingSeed)
  {
    const BridgeInformationState info =
      BuildInformationStateFromPlay(fullDeal, declarerSeat, playedCards,
        maxWorlds, samplingSeed);
    return MakeBridgeStateFromInformationState(fullDeal, declarerSeat,
      playedCards, info);
  }
}

