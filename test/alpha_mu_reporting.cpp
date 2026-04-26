/*
  alpha_mu reporting helpers

   Copyright © 2026 by David Jenkins
   All rights reserved.
*/

#include "alpha_mu_core.h"

namespace alpha_mu
{
  using namespace std;

  namespace
  {
    string ContractTrumpName(const int trumpSuit)
    {
      if (trumpSuit < 0)
        return "NT";
      if (trumpSuit == SUIT_SPADES)
        return "S";
      if (trumpSuit == SUIT_HEARTS)
        return "H";
      if (trumpSuit == SUIT_DIAMONDS)
        return "D";
      return "C";
    }

    void PrintMoveSummary(const BridgeMove& move)
    {
      cout << SuitName(move.suit) << " " << move.rank;
    }
  }

  void ReportBenchmarkMethodSummary(
    const BenchmarkMethodSummary& summary)
  {
    double sumPerBoard = 0.0;
    for (unsigned i = 0; i < summary.perBoardSeconds.size(); i++)
      sumPerBoard += summary.perBoardSeconds[i];

    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "ALPHA_MU_BENCHMARK method=" << summary.method
         << " file=" << summary.handFile
         << " boards=" << summary.boardsTested
         << " depth=" << summary.depth
         << " parallel=" << AlphaMuParallelModeName(summary.parallelMode)
         << " board_workers=" << summary.boardWorkers
         << " root_workers=" << summary.rootWorkers
         << " dds_thread_id=" << summary.ddsThreadId
         << " configured_board_workers=" << summary.configuredBoardWorkers
         << " total_seconds=" << summary.elapsedSeconds
         << " per_board_seconds="
         << (summary.perBoardSeconds.empty() ?
             (summary.boardsTested == 0 ? 0.0 :
               summary.elapsedSeconds / static_cast<double>(summary.boardsTested)) :
             sumPerBoard / static_cast<double>(summary.perBoardSeconds.size()))
         << " mismatches=" << summary.mismatches
         << "\n";
    Check(summary.mismatches == 0,
      "benchmark mode should preserve the exact golden FUT score on every tested board");
  }

  void ReportBenchmarkCheckpoint(
    const BenchmarkMethodSummary& summary,
    const unsigned completedBoards,
    const double elapsedSeconds)
  {
    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "ALPHA_MU_BENCHMARK_CHECKPOINT method=" << summary.method
         << " file=" << summary.handFile
         << " completed_boards=" << completedBoards
         << " total_boards=" << summary.boardsTested
         << " depth=" << summary.depth
         << " parallel=" << AlphaMuParallelModeName(summary.parallelMode)
         << " board_workers=" << summary.boardWorkers
         << " root_workers=" << summary.rootWorkers
         << " dds_thread_id=" << summary.ddsThreadId
         << " configured_board_workers=" << summary.configuredBoardWorkers
         << " elapsed_seconds=" << elapsedSeconds
         << " mismatches=" << summary.mismatches
         << endl;
  }

  void ReportBenchmarkBoardTiming(
    const BenchmarkMethodSummary& summary,
    const unsigned boardNumber,
    const double boardElapsedSeconds,
    const double totalElapsedSeconds)
  {
    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "ALPHA_MU_BENCHMARK_BOARD method=" << summary.method
         << " file=" << summary.handFile
         << " board=" << boardNumber
         << " total_boards=" << summary.boardsTested
         << " depth=" << summary.depth
         << " parallel=" << AlphaMuParallelModeName(summary.parallelMode)
         << " board_workers=" << summary.boardWorkers
         << " root_workers=" << summary.rootWorkers
         << " dds_thread_id=" << summary.ddsThreadId
         << " configured_board_workers=" << summary.configuredBoardWorkers
         << " board_seconds=" << boardElapsedSeconds
         << " elapsed_seconds=" << totalElapsedSeconds
         << " mismatches=" << summary.mismatches
         << endl;
  }

  void ReportDDSVsAlphaMuComparison(
    const DDSVsAlphaMuComparison& summary)
  {
    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "ALPHA_MU_COMPARE file=" << summary.handFile
         << " boards=" << summary.boardsTested
         << " max_depth="
         << (summary.alphaMuDepths.empty() ? 0 : summary.alphaMuDepths.back().depth)
         << "\n";
    cout << "ALPHA_MU_COMPARE dds total_seconds=" << summary.ddsElapsedSeconds
         << " per_board_seconds="
         << (summary.boardsTested == 0 ? 0.0 :
             summary.ddsElapsedSeconds / static_cast<double>(summary.boardsTested))
         << "\n";

    for (unsigned i = 0; i < summary.alphaMuDepths.size(); i++)
    {
      const DDSVsAlphaMuDepthTiming& depthTiming = summary.alphaMuDepths[i];
      const double ratio =
        (summary.ddsElapsedSeconds <= 0.0 ? 0.0 :
          depthTiming.elapsedSeconds / summary.ddsElapsedSeconds);
      cout << "ALPHA_MU_COMPARE depth=" << depthTiming.depth
           << " total_seconds=" << depthTiming.elapsedSeconds
           << " per_board_seconds="
           << (summary.boardsTested == 0 ? 0.0 :
               depthTiming.elapsedSeconds /
                 static_cast<double>(summary.boardsTested))
           << " ratio_vs_dds=" << ratio
           << " mismatches=" << depthTiming.mismatches
           << "\n";
      Check(depthTiming.mismatches == 0,
        "DDS comparison should preserve the exact single-world score for every alpha-mu depth tested");
    }
  }

  void PrintAlphaMuStatus(const string& msg)
  {
    cout << kAlphaMuMessagePrefix << msg << "\n";
  }

  void ReportAlphaMuSolveResult(const AlphaMuSolveResult& result)
  {
    cout.setf(ios::fixed);
    cout << setprecision(3);

    cout << "=== Alpha-Mu Solve Result ===" << endl;

    if (! result.valid)
    {
      cout << "  Status: no valid result (no surviving worlds or empty front)" << endl;
      cout << "  Total time: " << result.totalSeconds << "s" << endl;
      return;
    }

    cout << "  Chosen move: ";
    PrintMoveSummary(result.chosenMove);
    cout << endl;
    cout << "  Contract: ";
    if (result.contractLevel > 0)
      cout << result.contractLevel;
    else
      cout << "?";
    cout << ContractTrumpName(result.contractTrumpSuit)
         << " by " << SeatName(result.declarerSeat)
         << " (leader=" << SeatName(result.leaderSeat) << ")" << endl;
    cout << "  Decision point: player=" << SeatName(result.playerToMove)
         << ", side="
         << (result.decisionOnDeclarerSide ? "declarer" : "defender")
         << ", play prefix=" << result.prefixPlayLength
         << "/" << result.fullPlayLength << " cards" << endl;
    cout << "  Depth searched: " << result.depthSearched << " tricks" << endl;
    cout << "  Worlds: " << result.survivingWorldCount
         << " surviving / " << result.worldCount << " raw" << endl;
    cout << "  Root front: " << result.rootFront.vectors.size()
         << " vectors, mu=" << setprecision(4) << result.rootFront.Mu()
         << ", valid_worlds=" << result.rootReport.validWorlds.PopCount()
         << ", useful_worlds=" << result.rootReport.usefulWorlds.PopCount()
         << endl;

    cout << setprecision(3);
    const double nonDDSSearchSeconds =
      max(0.0, result.searchSeconds - result.ddsLeafSeconds);
    cout << "  Timing: " << result.totalSeconds << "s total ("
         << result.worldGenerationSeconds << "s world-gen, "
         << result.searchSeconds << "s search = "
         << nonDDSSearchSeconds << "s bridge-search + "
         << result.ddsLeafSeconds << "s DDS leaves)" << endl;
    cout << "  Constructor pruning: raw="
         << result.constructorStats.rawAssignmentCount
         << ", ownership=" << result.constructorStats.afterOwnershipCount
         << ", card-location="
         << result.constructorStats.afterCardLocationCount
         << ", length=" << result.constructorStats.afterConstructorLengthCount
         << ", HCP=" << result.constructorStats.afterConstructorHCPCount
         << ", shape=" << result.constructorStats.afterConstructorBalancedCount
         << ", final=" << result.constructorStats.finalWorldCount
         << endl;
    cout << "  TT: " << result.ttStores << " stores, "
         << result.ttHits << " hits / " << result.ttProbes << " probes"
         << endl;
    cout << "  Search activity: nodes=" << result.searchNodes
         << ", DDS leaf calls=" << result.ddsLeafCalls << endl;
    cout << "  Frontier activity: insert-attempts="
         << result.bridgeSearchStats.frontInsertAttempts
         << ", accepted=" << result.bridgeSearchStats.frontAcceptedInserts
         << ", rejected-dominated=" << result.bridgeSearchStats.frontDominatedRejects
         << ", dominated-removed=" << result.bridgeSearchStats.frontDominatedRemoved
         << ", max-merges=" << result.bridgeSearchStats.maxMergeCalls
         << ", min-products=" << result.bridgeSearchStats.minProductCalls
         << ", optimistic-completions="
         << result.bridgeSearchStats.optimisticCompletions
         << endl;
    cout << "  Cut activity: empty-world="
         << result.bridgeSearchStats.emptyWorldCuts
         << ", TT=" << result.bridgeSearchStats.ttCuts
         << ", early-alpha=" << result.bridgeSearchStats.earlyAlphaCuts
         << ", deep-alpha=" << result.bridgeSearchStats.deepAlphaCuts
         << ", cut-on-win=" << result.bridgeSearchStats.cutOnWinCuts
         << ", root=" << result.bridgeSearchStats.rootCuts
         << ", DDS-leaf=" << result.bridgeSearchStats.ddsLeafCuts
         << ", no-move=" << result.bridgeSearchStats.noMoveLeafCuts
         << ", terminal-fronts=" << result.bridgeSearchStats.terminalFronts
         << endl;

    if (! result.worldExplanation.appliedFollowSuitConstraints.empty())
    {
      cout << "  Derived follow-suit constraints:" << endl;
      for (unsigned i = 0; i < result.worldExplanation.appliedFollowSuitConstraints.size(); i++)
      {
        cout << "    - "
             << ConstraintToString(
                  result.worldExplanation.appliedFollowSuitConstraints[i])
             << endl;
      }
    }

    if (! result.biddingConstraintTexts.empty())
    {
      cout << "  Bidding-derived constraints:" << endl;
      for (unsigned i = 0; i < result.biddingConstraintTexts.size(); i++)
        cout << "    - " << result.biddingConstraintTexts[i] << endl;
    }

    if (result.hasActualPlayedMove)
    {
      cout << "  Actual played move: " << SeatName(result.actualPlayedBy) << " ";
      PrintMoveSummary(result.actualPlayedMove);
      if (result.hasActualMoveMu)
        cout << " (alpha-mu mu=" << setprecision(4) << result.actualMoveMu << ")";
      if (result.hasActualMoveDDSScore)
        cout << " (DDS=" << setprecision(3) << result.actualMoveDDSScore << ")";
      cout << endl;
    }

    if (result.hasDDSBestMove)
    {
      cout << "  DDS omniscient move: ";
      PrintMoveSummary(result.ddsBestMove);
      cout << " (DDS=" << setprecision(3) << result.ddsBestScore << ")"
           << endl;
    }

    if (result.hasChosenMoveMu || result.hasChosenMoveDDSScore)
    {
      cout << "  Chosen move detail:";
      if (result.hasChosenMoveMu)
        cout << " mu=" << setprecision(4) << result.chosenMoveMu;
      if (result.hasChosenMoveDDSScore)
        cout << " DDS=" << setprecision(3) << result.chosenMoveDDSScore;
      cout << endl;
    }

    if (result.hasActualPlayedMove &&
        ! (result.actualPlayedMove == result.chosenMove) &&
        result.hasChosenMoveMu && result.hasActualMoveMu)
    {
      cout << "  Alpha-mu vs actual: chose a move with higher root mu by "
           << setprecision(4)
           << (result.chosenMoveMu - result.actualMoveMu) << endl;
    }

    if (result.hasDDSBestMove &&
        ! (result.ddsBestMove == result.chosenMove) &&
        result.hasChosenMoveDDSScore)
    {
      cout << "  Alpha-mu vs DDS: chosen move trails the omniscient line by "
           << setprecision(3)
           << (result.ddsBestScore - result.chosenMoveDDSScore)
           << " tricks on the actual world" << endl;
    }

    if (! result.rootReport.children.empty())
    {
      cout << "  Candidate moves:" << endl;
      for (unsigned i = 0; i < result.rootReport.children.size(); i++)
      {
        const BridgeRootChildReport& child = result.rootReport.children[i];
        cout << "    ";
        PrintMoveSummary(child.move);
        cout << ": mu=" << setprecision(4) << child.mu
             << ", vectors=" << child.front.vectors.size()
             << ", valid_worlds=" << child.validWorlds.PopCount()
             << ", useful_worlds=" << child.usefulWorlds.PopCount()
             << ", nodes=" << child.searchNodes
             << ", dds_leaf_calls=" << child.ddsLeafCalls
             << endl;
      }
    }

    if (! result.worldExplanation.plausibilityRankedWorldIndices.empty())
    {
      cout << "  Top surviving worlds by plausibility:" << endl;
      const unsigned worldLines = min(static_cast<unsigned>(3),
        static_cast<unsigned>(result.worldExplanation.plausibilityRankedWorldIndices.size()));
      for (unsigned i = 0; i < worldLines; i++)
      {
        const unsigned worldIndex =
          result.worldExplanation.plausibilityRankedWorldIndices[i];
        const WorldExplanation& world = result.worldExplanation.worlds[worldIndex];
        cout << "    [" << worldIndex << "] score="
             << world.plausibilityScore << "/" << world.plausibilityMaxScore
             << " " << world.serializedWorld;
        if (! world.satisfiedPlausibilityHints.empty())
          cout << " | matched=" << world.satisfiedPlausibilityHints[0];
        cout << endl;
      }
    }

    unsigned rejectedLines = 0;
    for (unsigned i = 0; i < result.worldExplanation.worlds.size() && rejectedLines < 3; i++)
    {
      if (result.worldExplanation.worlds[i].accepted)
        continue;

      if (rejectedLines == 0)
        cout << "  Sample rejected worlds:" << endl;

      cout << "    [" << result.worldExplanation.worlds[i].worldIndex << "] "
           << result.worldExplanation.worlds[i].rejectionStage << ": "
           << result.worldExplanation.worlds[i].rejectionReason << endl;
      rejectedLines++;
    }

    cout << setprecision(3);
  }
}

