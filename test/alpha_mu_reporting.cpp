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

    string JoinUnsigned(const vector<unsigned>& values)
    {
      ostringstream oss;
      for (unsigned i = 0; i < values.size(); i++)
      {
        if (i != 0)
          oss << ",";
        oss << values[i];
      }
      return oss.str();
    }

    string FriendlyStageName(const string& stage)
    {
      if (stage == "known_cards")
        return "known cards";
      if (stage == "bidding")
        return "bidding";
      if (stage == "follow_suit")
        return "follow suit";
      if (stage == "play_history")
        return "play history";
      if (stage == "current_trick")
        return "current trick";
      if (stage == "deduplication")
        return "deduplication";
      if (stage == "sampling")
        return "sampling";
      if (stage == "decision_world_set")
        return "decision world set";
      if (stage == "constructor_length")
        return "constructor length";
      if (stage == "constructor_hcp")
        return "constructor HCP";
      if (stage == "constructor_balanced")
        return "constructor shape";
      return stage;
    }

    string PassedPathSummary(const WorldExplanation& world)
    {
      ostringstream oss;
      bool first = true;
      for (unsigned i = 0; i < world.steps.size(); i++)
      {
        if (! world.steps[i].passed)
          break;
        if (! first)
          oss << " -> ";
        oss << FriendlyStageName(world.steps[i].stage);
        first = false;
      }
      if (first)
        return "none";
      return oss.str();
    }

    unsigned CountChildFrontVectors(const BridgeRootReport& report)
    {
      unsigned total = 0;
      for (unsigned i = 0; i < report.children.size(); i++)
      {
        total += static_cast<unsigned>(report.children[i].front.vectors.size());
      }
      return total;
    }

    unsigned MaxChildFrontVectors(const BridgeRootReport& report)
    {
      unsigned best = 0;
      for (unsigned i = 0; i < report.children.size(); i++)
      {
        const unsigned childVectors = static_cast<unsigned>(
          report.children[i].front.vectors.size());
        if (childVectors > best)
          best = childVectors;
      }
      return best;
    }

    double SafeRatio(
      const unsigned long long numerator,
      const unsigned long long denominator)
    {
      return (denominator == 0ULL ? 0.0 :
        static_cast<double>(numerator) / static_cast<double>(denominator));
    }

    void AppendAlphaMuBenchmarkMetrics(
      ostream& os,
      const unsigned long long searchNodes,
      const unsigned long long ddsLeafCalls,
      const double ddsLeafSeconds,
      const double bridgeSearchSeconds)
    {
      os << " search_nodes=" << searchNodes
         << " dds_leaf_calls=" << ddsLeafCalls
         << " dds_leaf_seconds=" << ddsLeafSeconds
         << " bridge_search_seconds=" << bridgeSearchSeconds;
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
         << " mismatches=" << summary.mismatches;
    if (summary.method == "alpha_mu")
    {
      AppendAlphaMuBenchmarkMetrics(cout, summary.searchNodes,
        summary.ddsLeafCalls, summary.ddsLeafSeconds,
        summary.bridgeSearchSeconds);
    }
    cout
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
         << " mismatches=" << summary.mismatches;
    if (summary.method == "alpha_mu")
    {
      AppendAlphaMuBenchmarkMetrics(cout, summary.searchNodes,
        summary.ddsLeafCalls, summary.ddsLeafSeconds,
        summary.bridgeSearchSeconds);
    }
    cout
         << endl;
  }

  void ReportBenchmarkBoardTiming(
    const BenchmarkMethodSummary& summary,
    const unsigned boardNumber,
    const double boardElapsedSeconds,
    const double totalElapsedSeconds,
    const unsigned long long searchNodes,
    const unsigned long long ddsLeafCalls,
    const double ddsLeafSeconds,
    const double bridgeSearchSeconds)
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
         << " mismatches=" << summary.mismatches;
    if (summary.method == "alpha_mu")
    {
      AppendAlphaMuBenchmarkMetrics(cout, searchNodes, ddsLeafCalls,
        ddsLeafSeconds, bridgeSearchSeconds);
    }
    cout
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
    const unsigned childFrontVectorTotal =
      CountChildFrontVectors(result.rootReport);
    const unsigned childFrontVectorMax =
      MaxChildFrontVectors(result.rootReport);
    const unsigned long long ttProbeMisses =
      (result.ttProbes >= result.ttHits ? result.ttProbes - result.ttHits : 0ULL);
    const double ttHitRate = SafeRatio(result.ttHits, result.ttProbes);

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
    cout << "  World pipeline: known="
         << result.worldGenerationStats.afterKnownCardCount
         << ", bidding=" << result.worldGenerationStats.afterBiddingCount
         << ", follow-suit=" << result.worldGenerationStats.afterFollowSuitCount
         << ", history=" << result.worldGenerationStats.afterPlayHistoryCount
         << ", current-trick=" << result.worldGenerationStats.afterCurrentTrickCount
         << ", sampled=" << result.worldGenerationStats.afterSamplingCount
         << ", duplicates-removed=" << result.worldGenerationStats.duplicateWorldsRemoved
         << ", sampled-out=" << result.worldGenerationStats.sampledOutWorlds
         << endl;
    cout << "  Active raw world ids: "
         << JoinUnsigned(result.activeWorldIndices) << endl;
    cout << "  Front summary: root_vectors="
         << result.rootFront.vectors.size()
         << ", root_valid_worlds="
         << result.rootReport.validWorlds.PopCount()
         << ", root_useful_worlds="
         << result.rootReport.usefulWorlds.PopCount()
         << ", child_count=" << result.rootReport.children.size()
         << ", child_vectors_total=" << childFrontVectorTotal
         << ", child_vectors_max=" << childFrontVectorMax
         << endl;
    cout << "  TT: exact_stores=" << result.ttStores
         << ", exact_reuses=" << result.ttHits
         << ", probes=" << result.ttProbes
         << ", probe_misses=" << ttProbeMisses
         << ", collisions=" << result.ttCollisions
         << ", reuse_cuts=" << result.bridgeSearchStats.ttCuts
         << ", hit_rate=" << ttHitRate
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
      cout << "  Decision policy: "
           << AlphaMuDecisionPolicyName(result.appliedDecisionPolicy);
      if (result.appliedDecisionPolicy != result.requestedDecisionPolicy)
      {
        cout << " (requested "
             << AlphaMuDecisionPolicyName(result.requestedDecisionPolicy)
             << ", fell back because no surviving world carried positive plausibility weight)";
      }
      cout << endl;

      cout << "  Chosen move detail:";
      if (result.hasChosenMoveMu)
        cout << " mu=" << setprecision(4) << result.chosenMoveMu;
      if (result.hasChosenMoveWeightedScore)
        cout << " weighted=" << setprecision(4) << result.chosenMoveWeightedScore;
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
        cout << " | passed_path=" << PassedPathSummary(world);
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
           << "rejected at "
           << FriendlyStageName(result.worldExplanation.worlds[i].rejectionStage)
           << " because "
           << result.worldExplanation.worlds[i].rejectionReason
           << " | passed_path="
           << PassedPathSummary(result.worldExplanation.worlds[i])
           << endl;
      rejectedLines++;
    }

    cout << setprecision(3);
    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "ALPHA_MU_DECISION"
         << " raw_worlds=" << result.worldCount
         << " surviving_worlds=" << result.survivingWorldCount
         << " root_vectors=" << result.rootFront.vectors.size()
         << " root_valid_worlds=" << result.rootReport.validWorlds.PopCount()
         << " root_useful_worlds=" << result.rootReport.usefulWorlds.PopCount()
         << " child_count=" << result.rootReport.children.size()
         << " child_vectors_total=" << childFrontVectorTotal
         << " child_vectors_max=" << childFrontVectorMax
         << " tt_exact_stores=" << result.ttStores
         << " tt_exact_reuses=" << result.ttHits
         << " tt_probe_misses=" << ttProbeMisses
         << " tt_collisions=" << result.ttCollisions
         << " tt_reuse_cuts=" << result.bridgeSearchStats.ttCuts
         << " tt_hit_rate=" << ttHitRate
         << " cut_empty_world=" << result.bridgeSearchStats.emptyWorldCuts
         << " cut_tt=" << result.bridgeSearchStats.ttCuts
         << " cut_early_alpha=" << result.bridgeSearchStats.earlyAlphaCuts
         << " cut_deep_alpha=" << result.bridgeSearchStats.deepAlphaCuts
         << " cut_on_win=" << result.bridgeSearchStats.cutOnWinCuts
         << " cut_root=" << result.bridgeSearchStats.rootCuts
         << " cut_dds_leaf=" << result.bridgeSearchStats.ddsLeafCuts
         << " cut_no_move=" << result.bridgeSearchStats.noMoveLeafCuts
         << " terminal_fronts=" << result.bridgeSearchStats.terminalFronts
         << " world_gen_seconds=" << result.worldGenerationSeconds
         << " search_seconds=" << result.searchSeconds
         << " bridge_search_seconds=" << nonDDSSearchSeconds
         << " dds_leaf_seconds=" << result.ddsLeafSeconds
         << " total_seconds=" << result.totalSeconds
         << " after_known_cards=" << result.worldGenerationStats.afterKnownCardCount
         << " after_bidding=" << result.worldGenerationStats.afterBiddingCount
         << " after_follow_suit=" << result.worldGenerationStats.afterFollowSuitCount
         << " after_play_history=" << result.worldGenerationStats.afterPlayHistoryCount
         << " after_current_trick=" << result.worldGenerationStats.afterCurrentTrickCount
         << " after_sampling=" << result.worldGenerationStats.afterSamplingCount
         << " duplicates_removed=" << result.worldGenerationStats.duplicateWorldsRemoved
         << " sampled_out=" << result.worldGenerationStats.sampledOutWorlds
         << " active_world_ids=" << JoinUnsigned(result.activeWorldIndices)
         << " front_insert_attempts=" << result.bridgeSearchStats.frontInsertAttempts
         << " front_insert_accepts=" << result.bridgeSearchStats.frontAcceptedInserts
         << " front_dominated_rejects=" << result.bridgeSearchStats.frontDominatedRejects
         << " front_dominated_removed=" << result.bridgeSearchStats.frontDominatedRemoved
         << " max_merge_calls=" << result.bridgeSearchStats.maxMergeCalls
         << " min_product_calls=" << result.bridgeSearchStats.minProductCalls
         << " optimistic_completions=" << result.bridgeSearchStats.optimisticCompletions
         << " decision_policy=" << AlphaMuDecisionPolicyName(result.appliedDecisionPolicy)
         << " chosen_weighted=" << result.chosenMoveWeightedScore
         << endl;

    cout << setprecision(3);
  }
}

