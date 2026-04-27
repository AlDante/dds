/*
  alpha_mu decision-point solve helpers

   Copyright © 2026 by David Jenkins
   All rights reserved.
*/

#include "alpha_mu_core.h"

namespace alpha_mu
{
  using namespace std;

  namespace
  {
    ParsedWorld BuildActualRemainingWorld(
      const dealPBN& deal,
      const vector<PlayHistoryEvent>& playedCards)
    {
      ParsedWorld world = ParsePBNWorld(deal.remainCards);
      for (unsigned i = 0; i < playedCards.size(); i++)
      {
        Check(WorldHasCard(world, playedCards[i].player, playedCards[i].move.suit,
              playedCards[i].move.rank),
          "actual decision-point world reconstruction should only remove cards held by their recorded owner");
        RemoveCardFromWorld(world, playedCards[i].player, playedCards[i].move);
      }
      return world;
    }

    BridgeState MakeSingleWorldDecisionState(
      const dealPBN& deal,
      const vector<PlayHistoryEvent>& playedCards)
    {
      BridgeState state;
      state.worlds.push_back(BuildActualRemainingWorld(deal, playedCards));
      state.possibleWorlds = WorldMask(1, 0x1ULL);
      state.trumpSuit = (deal.trump == 4 ? -1 : deal.trump);

      const unsigned cardsInCurrentTrick =
        static_cast<unsigned>(playedCards.size() % 4);
      if (cardsInCurrentTrick > 0)
      {
        const unsigned trickStart =
          static_cast<unsigned>(playedCards.size()) - cardsInCurrentTrick;
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
        const unsigned lastTrickStart =
          static_cast<unsigned>(playedCards.size()) - 4U;
        vector<BridgeMove> lastTrick;
        vector<int> lastPlayers;
        for (unsigned i = lastTrickStart; i < playedCards.size(); i++)
        {
          lastTrick.push_back(playedCards[i].move);
          lastPlayers.push_back(playedCards[i].player);
        }
        const unsigned winIdx = WinningCardIndex(lastTrick,
          playedCards[lastTrickStart].move.suit, state.trumpSuit);
        state.trickLeader = lastPlayers[winIdx];
        state.playerToMove = state.trickLeader;
        state.leadSuit = -1;
      }
      else
      {
        state.trickLeader = deal.first;
        state.playerToMove = deal.first;
        state.leadSuit = -1;
      }

      state.maxSide = SeatSide(state.playerToMove);
      state.maxTricksWon = 0;
      return state;
    }

    const BridgeRootChildReport * FindRootChildReport(
      const BridgeRootReport& report,
      const BridgeMove& move)
    {
      for (unsigned i = 0; i < report.children.size(); i++)
      {
        if (report.children[i].move == move)
          return &report.children[i];
      }
      return NULL;
    }

    void PopulateDecisionWorldExplanation(
      AlphaMuSolveResult& result,
      const DecisionWorldPipelineResult& pipeline)
    {
      result.worldExplanation = pipeline.explanation;
      result.activeWorldIndices = pipeline.activeWorldIndices;
      result.activeWorldSerializations.clear();
      for (unsigned i = 0; i < pipeline.activeWorlds.size(); i++)
      {
        result.activeWorldSerializations.push_back(
          SerializePBNWorld(pipeline.activeWorlds[i]));
      }

      set<unsigned> activeIndexSet(pipeline.activeWorldIndices.begin(),
        pipeline.activeWorldIndices.end());
      for (unsigned i = 0; i < result.worldExplanation.worlds.size(); i++)
      {
        WorldExplanation& world = result.worldExplanation.worlds[i];
        world.accepted = (activeIndexSet.find(world.worldIndex) !=
          activeIndexSet.end());
        AddWorldExplanationStep(world, "decision_world_set", world.accepted,
          (world.accepted ?
            "survived into the compacted decision-point world set searched by alpha-mu" :
            "not active in the compacted decision-point world set searched by alpha-mu"));
      }
    }
  }

  AlphaMuSolveResult SolveAlphaMu(
    const dealPBN& deal,
    const int declarerSeat,
    const playTracePBN& play,
    const int depth,
    const unsigned maxWorlds,
    const double timeBudgetSeconds,
    const int prefixCards,
    const unsigned samplingSeed)
  {
    const int trumpSuit = (deal.trump == 4 ? -1 : deal.trump);
    AlphaMuDecisionPointRequest request;
    request.deal = deal;
    request.declarerSeat = declarerSeat;
    request.contractLevel = 0;
    request.playHistory = ParsePBNPlayHistory(play, deal.first, trumpSuit);
    request.depth = depth;
    request.maxWorlds = maxWorlds;
    request.timeBudgetSeconds = timeBudgetSeconds;
    request.prefixCards = prefixCards;
    request.samplingSeed = samplingSeed;
    return SolveAlphaMuDecisionPoint(request);
  }

  AlphaMuSolveResult SolveAlphaMuDecisionPoint(
    const AlphaMuDecisionPointRequest& request)
  {
    AlphaMuSolveResult result;

    const chrono::steady_clock::time_point totalStart =
      chrono::steady_clock::now();
    const chrono::steady_clock::time_point worldGenStart =
      chrono::steady_clock::now();

    result.declarerSeat = request.declarerSeat;
    result.leaderSeat = request.deal.first;
    result.contractLevel = request.contractLevel;
    result.contractTrumpSuit = (request.deal.trump == 4 ? -1 : request.deal.trump);

    const vector<PlayHistoryEvent>& fullHistory = request.playHistory;
    result.fullPlayLength = static_cast<unsigned>(fullHistory.size());
    if (request.prefixCards >= 0)
    {
      Check(static_cast<unsigned>(request.prefixCards) <= fullHistory.size(),
        "solve decision-point prefix should not exceed the available play history length");
      result.prefixPlayLength = static_cast<unsigned>(request.prefixCards);
    }
    else
      result.prefixPlayLength = static_cast<unsigned>(fullHistory.size());

    const vector<PlayHistoryEvent>::difference_type prefixSize =
      static_cast<vector<PlayHistoryEvent>::difference_type>(
        result.prefixPlayLength);
    vector<PlayHistoryEvent> history(fullHistory.begin(),
      fullHistory.begin() + prefixSize);

    if (result.prefixPlayLength < fullHistory.size())
    {
      result.hasActualPlayedMove = true;
      result.actualPlayedBy = fullHistory[result.prefixPlayLength].player;
      result.actualPlayedMove = fullHistory[result.prefixPlayLength].move;
    }

    const HistoryDerivedWorldSpec spec =
      BuildWorldSpecFromDeal(request.deal, request.declarerSeat, history);
    const BridgeInformationState information = ApplyInformationOverrides(
      BuildInformationStateFromPlay(request.deal, request.declarerSeat, history,
        request.maxWorlds, request.samplingSeed),
      request.informationOverrides,
      request.maxWorlds,
      request.samplingSeed);
    for (unsigned i = 0; i < information.biddingConstraints.size(); i++)
      result.biddingConstraintTexts.push_back(
        ConstraintToString(information.biddingConstraints[i]));
    const HistoryDerivedConstructionExplanation constructorExplanation =
      ExplainHistoryDerivedConstruction(spec, information);
    result.constructorStats = constructorExplanation.stats;

    DecisionWorldPipelineResult decisionWorldPipeline;
    const BridgeState state = MakeBridgeStateFromInformationState(
      request.deal, request.declarerSeat, history, information,
      &result.worldGenerationStats, &decisionWorldPipeline);
    PopulateDecisionWorldExplanation(result, decisionWorldPipeline);
    result.playerToMove = state.playerToMove;
    result.decisionOnDeclarerSide =
      (SeatSide(state.playerToMove) == SeatSide(request.declarerSeat));
    if (result.hasActualPlayedMove)
    {
      Check(result.actualPlayedBy == state.playerToMove,
        "solve decision-point reporting should align the next recorded play with the computed player-to-move");
    }

    const chrono::steady_clock::time_point worldGenEnd =
      chrono::steady_clock::now();
    result.worldGenerationSeconds =
      chrono::duration<double>(worldGenEnd - worldGenStart).count();

    result.worldCount = result.worldGenerationStats.candidateWorldCount;
    result.survivingWorldCount = static_cast<unsigned>(
      decisionWorldPipeline.activeWorldIndices.size());

    if (result.survivingWorldCount == 0)
    {
      result.totalSeconds =
        chrono::duration<double>(chrono::steady_clock::now() - totalStart).count();
      return result;
    }

    const chrono::steady_clock::time_point searchStart =
      chrono::steady_clock::now();

    unsigned maxCards = 0;
    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      if (state.possibleWorlds.Has(i))
      {
        const unsigned c = WorldCardCount(state.worlds[i]);
        if (c > maxCards)
          maxCards = c;
      }
    }
    const int tricksRemaining = static_cast<int>(
      (maxCards + state.currentTrick.size()) / 4U);
    const int maxSearchDepth =
      (request.depth <= 0 || request.depth > tricksRemaining)
      ? tricksRemaining : request.depth;

    SetMaxThreads(0);
    InitZobrist();
    BridgeTranspositionTable tt(1U << 20);
    BridgeTTStats ttStats;
    BenchmarkBoardProgressContext progress;
    progress.totalStart = searchStart;
    progress.boardStart = searchStart;
    progress.reportIntervalSeconds = 1.0e30;
    progress.nextReportSeconds = 1.0e30;
    SearchExecutionContext context;
    context.benchmarkProgress = &progress;
    SetActiveBridgeSearchStats(&result.bridgeSearchStats);

    BridgeRootReport bestReport(state.possibleWorlds.count);
    int bestDepth = 0;

    if (request.timeBudgetSeconds > 0.0 && maxSearchDepth > 1)
    {
      const BridgeRootReport* prevReport = NULL;
      for (int d = 1; d <= maxSearchDepth; d++)
      {
        const double elapsed =
          chrono::duration<double>(chrono::steady_clock::now() - searchStart).count();
        if (d > 1 && elapsed > request.timeBudgetSeconds)
          break;

        tt.Clear();
        BridgeTTStats iterStats;
        const BridgeRootReport report = AnalyzeBridgeRootWithTT(
          state, d, context, &tt, &iterStats, prevReport);

        ttStats.probes += iterStats.probes;
        ttStats.hits += iterStats.hits;
        ttStats.stores += iterStats.stores;

        bestReport = report;
        bestDepth = d;
        prevReport = &bestReport;

        const double iterElapsed =
          chrono::duration<double>(chrono::steady_clock::now() - searchStart).count();
        cout << "  ID depth=" << d
             << " mu=" << fixed << setprecision(4) << report.rootFront.Mu()
             << " vectors=" << report.rootFront.vectors.size()
             << " tt_hits=" << iterStats.hits
             << " tt_stores=" << iterStats.stores
             << " elapsed=" << setprecision(3) << iterElapsed << "s"
             << endl;
      }
    }
    else
    {
      bestReport = AnalyzeBridgeRootWithTT(
        state, maxSearchDepth, context, &tt, &ttStats);
      bestDepth = maxSearchDepth;
    }
    SetActiveBridgeSearchStats(NULL);

    const chrono::steady_clock::time_point searchEnd =
      chrono::steady_clock::now();
    result.searchSeconds =
      chrono::duration<double>(searchEnd - searchStart).count();

    result.rootReport = bestReport;
    result.rootFront = bestReport.rootFront;
    result.depthSearched = bestDepth;
    result.valid = ! bestReport.rootFront.vectors.empty();
    result.ddsLeafSeconds = (progress.ddsLeafSeconds > 0.0 ?
      progress.ddsLeafSeconds : bestReport.ddsLeafSeconds);
    result.ttProbes = ttStats.probes;
    result.ttHits = ttStats.hits;
    result.ttStores = ttStats.stores;
    result.searchNodes = (progress.recursiveCalls == 0ULL ?
      bestReport.searchNodes : static_cast<unsigned>(progress.recursiveCalls));
    result.ddsLeafCalls = (progress.ddsLeafCalls == 0ULL ?
      bestReport.ddsLeafCalls : static_cast<unsigned>(progress.ddsLeafCalls));

    bool haveChosenChild = false;
    unsigned bestChildIndex = 0;
    if (! bestReport.children.empty())
    {
      double bestMu = -1.0;
      for (unsigned i = 0; i < bestReport.children.size(); i++)
      {
        const double mu = bestReport.children[i].front.Mu();
        if (! haveChosenChild || mu > bestMu ||
            (mu == bestMu &&
             BridgeMoveLess(bestReport.children[i].move,
               bestReport.children[bestChildIndex].move)))
        {
          haveChosenChild = true;
          bestMu = mu;
          bestChildIndex = i;
        }
      }
      result.chosenMove = bestReport.children[bestChildIndex].move;
      result.hasChosenMoveMu = true;
      result.chosenMoveMu = bestReport.children[bestChildIndex].front.Mu();
    }

    if (result.hasActualPlayedMove)
    {
      const BridgeRootChildReport * actualReport = FindRootChildReport(bestReport,
        result.actualPlayedMove);
      if (actualReport != NULL)
      {
        result.hasActualMoveMu = true;
        result.actualMoveMu = actualReport->front.Mu();
      }
    }

    const BridgeState actualState = MakeSingleWorldDecisionState(request.deal,
      history);
    const vector<BridgeChild> actualChildren = ExpandBridgeChildren(actualState);
    for (unsigned i = 0; i < actualChildren.size(); i++)
    {
      const int score = ExactBridgeDDSScoreForWorld(actualChildren[i].state, 0,
        context);
      if (! result.hasDDSBestMove || score > result.ddsBestScore ||
          (score == result.ddsBestScore &&
           BridgeMoveLess(actualChildren[i].move, result.ddsBestMove)))
      {
        result.hasDDSBestMove = true;
        result.ddsBestMove = actualChildren[i].move;
        result.ddsBestScore = score;
      }
      if (result.valid && actualChildren[i].move == result.chosenMove)
      {
        result.hasChosenMoveDDSScore = true;
        result.chosenMoveDDSScore = score;
      }
      if (result.hasActualPlayedMove &&
          actualChildren[i].move == result.actualPlayedMove)
      {
        result.hasActualMoveDDSScore = true;
        result.actualMoveDDSScore = score;
      }
    }

    result.totalSeconds =
      chrono::duration<double>(chrono::steady_clock::now() - totalStart).count();
    return result;
  }
}

