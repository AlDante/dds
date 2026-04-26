/*
  alpha_mu, an alpha-mu bridge solver

   Copyright © 2026 by David Jenkins
   All rights reserved.
*/

#include "alpha_mu_core.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace alpha_mu
{
  using namespace std;

  namespace
  {
    thread_local BridgeSearchStats * gActiveBridgeSearchStats = NULL;
  }

  const char kAlphaMuMessagePrefix[] = "alpha_mu: ";
  const char kAlphaMuPlayHandFile[] = "hands/alpha_mu_play.txt";

  void SetActiveBridgeSearchStats(BridgeSearchStats* stats)
  {
    gActiveBridgeSearchStats = stats;
  }

  void NoteFrontInsertAttempt()
  {
    if (gActiveBridgeSearchStats != NULL)
      gActiveBridgeSearchStats->frontInsertAttempts++;
  }

  void NoteFrontInsertRejectedByDominance()
  {
    if (gActiveBridgeSearchStats != NULL)
      gActiveBridgeSearchStats->frontDominatedRejects++;
  }

  void NoteFrontInsertAccepted(const unsigned removedCount)
  {
    if (gActiveBridgeSearchStats != NULL)
    {
      gActiveBridgeSearchStats->frontAcceptedInserts++;
      gActiveBridgeSearchStats->frontDominatedRemoved += removedCount;
    }
  }

  void NoteMaxMergeCall()
  {
    if (gActiveBridgeSearchStats != NULL)
      gActiveBridgeSearchStats->maxMergeCalls++;
  }

  void NoteMinProductCall()
  {
    if (gActiveBridgeSearchStats != NULL)
      gActiveBridgeSearchStats->minProductCalls++;
  }

  void NoteEmptyWorldCut()
  {
    if (gActiveBridgeSearchStats != NULL)
      gActiveBridgeSearchStats->emptyWorldCuts++;
  }

  void NoteTTCut()
  {
    if (gActiveBridgeSearchStats != NULL)
      gActiveBridgeSearchStats->ttCuts++;
  }

  void NoteDDSLeafCut()
  {
    if (gActiveBridgeSearchStats != NULL)
      gActiveBridgeSearchStats->ddsLeafCuts++;
  }

  void NoteNoMoveLeafCut()
  {
    if (gActiveBridgeSearchStats != NULL)
      gActiveBridgeSearchStats->noMoveLeafCuts++;
  }

  void NoteTerminalFront()
  {
    if (gActiveBridgeSearchStats != NULL)
      gActiveBridgeSearchStats->terminalFronts++;
  }

  // ========================================================================
  // Zobrist hashing and bridge transposition table implementation
  // ========================================================================

  ZobristTable gZobrist;
  static bool gZobristInitialized = false;

  void InitZobrist()
  {
    if (gZobristInitialized)
      return;
    gZobrist.Init();
    gZobristInitialized = true;
  }

  unsigned long long HashBridgeState(const BridgeState& state)
  {
    InitZobrist();
    unsigned long long h = 0;

    // Hash player to move
    h ^= gZobrist.playerToMove[state.playerToMove & 3];

    // Hash max tricks won
    if (state.maxTricksWon >= 0 &&
        state.maxTricksWon < static_cast<int>(ZobristTable::MAX_TRICKS))
      h ^= gZobrist.maxTricksWon[state.maxTricksWon];

    // Hash trump suit (+1 so notrump=-1 maps to index 0)
    const int trumpIdx = state.trumpSuit + 1;
    if (trumpIdx >= 0 && trumpIdx < 5)
      h ^= gZobrist.trumpSuit[trumpIdx];

    // Hash current trick cards
    for (unsigned t = 0; t < state.currentTrick.size(); t++)
    {
      const int ri = RankCharToIndex(state.currentTrick[t].rank);
      if (ri >= 0)
        h ^= gZobrist.currentTrickCard[t][state.currentTrick[t].suit][ri];
    }

    // Hash world mask
    for (unsigned w = 0; w < state.possibleWorlds.count && w < 64; w++)
    {
      if (state.possibleWorlds.Has(w))
        h ^= gZobrist.worldMaskBit[w];
    }

    // Hash card holdings per world
    for (unsigned w = 0; w < state.worlds.size(); w++)
    {
      if (! state.possibleWorlds.Has(w))
        continue;
      if (w >= ZobristTable::MAX_WORLDS)
        break;

      for (unsigned seat = 0; seat < 4; seat++)
      {
        for (unsigned suit = 0; suit < 4; suit++)
        {
          const string& cards = state.worlds[w].suits[seat][suit];
          for (unsigned c = 0; c < cards.size(); c++)
          {
            const int ri = RankCharToIndex(cards[c]);
            if (ri >= 0)
              h ^= gZobrist.card[w][seat][suit][ri];
          }
        }
      }
    }

    return h;
  }

  BridgeTranspositionTable::BridgeTranspositionTable(unsigned capacityHint)
  {
    // Round up to power of 2
    unsigned cap = 1;
    while (cap < capacityHint)
      cap <<= 1;
    capacity = cap;
    mask = cap - 1;
    stored = 0;
    table.resize(cap);
  }

  void BridgeTranspositionTable::Clear()
  {
    for (unsigned i = 0; i < capacity; i++)
      table[i].occupied = false;
    stored = 0;
  }

  const ParetoFront* BridgeTranspositionTable::Probe(
      unsigned long long hash,
      unsigned long long worldMaskBits) const
  {
    unsigned idx = static_cast<unsigned>(hash) & mask;
    // Linear probing with limited search
    for (unsigned i = 0; i < 4; i++)
    {
      const unsigned slot = (idx + i) & mask;
      const BridgeTTEntry& e = table[slot];
      if (! e.occupied)
        return NULL;
      if (e.hash == hash && e.worldMaskBits == worldMaskBits)
        return &e.front;
    }
    return NULL;
  }

  void BridgeTranspositionTable::Store(
      unsigned long long hash,
      unsigned long long worldMaskBits,
      const ParetoFront& front)
  {
    unsigned idx = static_cast<unsigned>(hash) & mask;
    // Linear probing: find empty or matching slot within 4 steps
    for (unsigned i = 0; i < 4; i++)
    {
      const unsigned slot = (idx + i) & mask;
      BridgeTTEntry& e = table[slot];
      if (! e.occupied)
      {
        e.hash = hash;
        e.worldMaskBits = worldMaskBits;
        e.front = front;
        e.occupied = true;
        stored++;
        return;
      }
      if (e.hash == hash && e.worldMaskBits == worldMaskBits)
      {
        e.front = front;
        return;
      }
    }
    // All 4 slots occupied — replace the first one
    const unsigned slot = idx;
    table[slot].hash = hash;
    table[slot].worldMaskBits = worldMaskBits;
    table[slot].front = front;
  }


void Fail(const string& msg)
  {
    cerr << kAlphaMuMessagePrefix << msg << "\n";
    exit(1);
  }
void Check(const bool condition, const string& msg)
  {
    if (! condition)
      Fail(msg);
  }

  // Bridge-state assembly/search helpers now live in alpha_mu_bridge.cpp.
  // World-construction/information-state helpers live in alpha_mu_worlds.cpp.
  // Front/outcome-vector and toy-search helpers live in alpha_mu_front.cpp.

unsigned BoardsToBenchmark(
    const HandFileData& data,
    const int maxBoards)
  {
    return static_cast<unsigned>(
      (maxBoards > 0 ? min(data.number, maxBoards) : data.number));
  }
set<unsigned> ParseSkippedBoardNumbers(
    const string& skipSpec,
    const unsigned availableBoards)
  {
    set<unsigned> skipped;
    if (skipSpec.empty())
      return skipped;

    const vector<string> parts = SplitString(skipSpec, ',', false);
    for (unsigned i = 0; i < parts.size(); i++)
    {
      const string token = parts[i];
      const size_t dash = token.find('-');
      if (dash == string::npos)
      {
        const int boardNumber = atoi(token.c_str());
        Check(boardNumber > 0,
          "benchmark skip-board spec should use positive 1-based board numbers");
        Check(static_cast<unsigned>(boardNumber) <= availableBoards,
          "benchmark skip-board spec should not reference a board beyond the selected hand-file range");
        skipped.insert(static_cast<unsigned>(boardNumber));
        continue;
      }

      const int startBoard = atoi(token.substr(0, dash).c_str());
      const int endBoard = atoi(token.substr(dash + 1).c_str());
      Check(startBoard > 0 && endBoard > 0 && startBoard <= endBoard,
        "benchmark skip-board ranges should be positive increasing 1-based ranges");
      Check(static_cast<unsigned>(endBoard) <= availableBoards,
        "benchmark skip-board range should not extend beyond the selected hand-file range");
      for (int boardNumber = startBoard; boardNumber <= endBoard; boardNumber++)
        skipped.insert(static_cast<unsigned>(boardNumber));
    }

    return skipped;
  }
vector<unsigned> SelectBenchmarkBoardNumbers(
    const HandFileData& data,
    const int maxBoards,
    const string& skipSpec)
  {
    const unsigned candidateBoards = BoardsToBenchmark(data, maxBoards);
    const set<unsigned> skipped = ParseSkippedBoardNumbers(skipSpec, candidateBoards);
    vector<unsigned> selectedBoards;
    for (unsigned boardNumber = 1; boardNumber <= candidateBoards; boardNumber++)
    {
      if (skipped.find(boardNumber) == skipped.end())
        selectedBoards.push_back(boardNumber);
    }
    return selectedBoards;
  }

  double BenchmarkProgressIntervalSeconds();

  namespace
  {
    struct AlphaMuBenchmarkBoardResult
    {
      unsigned boardNumber;
      double boardElapsedSeconds;
      double completionElapsedSeconds;
      unsigned mismatches;

      AlphaMuBenchmarkBoardResult() :
        boardNumber(0),
        boardElapsedSeconds(0.0),
        completionElapsedSeconds(0.0),
        mismatches(0)
      {
      }
    };

    int ClampDDSBenchmarkThreadId(
      const int requestedThreadId,
      const int configuredThreads)
    {
      if (configuredThreads <= 1)
        return 0;

      return min(max(0, requestedThreadId), configuredThreads - 1);
    }

    unsigned DetermineAlphaMuBoardWorkerCount(
      const AlphaMuBenchmarkOptions& options,
      const unsigned boardsToTest,
      int& baseThreadId)
    {
      if (options.parallelMode != ALPHA_MU_PARALLEL_BOARD || boardsToTest <= 1U)
      {
        baseThreadId = max(0, options.ddsThreadId);
        return 1U;
      }

      SetMaxThreads(max(1, options.boardWorkers));

      DDSInfo info;
      memset(&info, 0, sizeof(info));
      GetDDSInfo(&info);

      const int configuredThreads = max(1, info.noOfThreads);
      baseThreadId = ClampDDSBenchmarkThreadId(options.ddsThreadId,
        configuredThreads);
      const unsigned availableWorkers = static_cast<unsigned>(
        max(1, configuredThreads - baseThreadId));
      const unsigned requestedWorkers = static_cast<unsigned>(
        max(1, options.boardWorkers));
      return min(boardsToTest, min(requestedWorkers, availableWorkers));
    }

    AlphaMuBenchmarkBoardResult RunAlphaMuBenchmarkBoard(
      const HandFileData& data,
      const string& resolvedHandFile,
      const unsigned totalBoards,
      const unsigned boardNumber,
      const AlphaMuBenchmarkOptions& options,
      const int configuredBoardWorkers,
      const int ddsThreadId,
      const chrono::steady_clock::time_point benchmarkStart,
      const bool enableProgress)
    {
      const unsigned boardIndex = boardNumber - 1U;
      const chrono::steady_clock::time_point boardStart =
        chrono::steady_clock::now();
      const BridgeState state = MakeBridgeStateFromDDSDeal(data.dealList[boardIndex]);

      BenchmarkBoardProgressContext progress;
      if (enableProgress)
      {
        progress.method = "alpha_mu";
        progress.handFile = resolvedHandFile;
        progress.boardNumber = boardNumber;
        progress.totalBoards = totalBoards;
        progress.depth = options.depth;
        progress.parallelMode = options.parallelMode;
        progress.boardWorkers = options.boardWorkers;
        progress.rootWorkers = options.rootWorkers;
        progress.ddsThreadId = ddsThreadId;
        progress.configuredBoardWorkers = configuredBoardWorkers;
        progress.reportIntervalSeconds = BenchmarkProgressIntervalSeconds();
        progress.totalStart = benchmarkStart;
        progress.boardStart = boardStart;
        progress.nextReportSeconds = progress.reportIntervalSeconds;
      }

      const SearchExecutionContext searchContext = MakeSearchExecutionContext(
        ddsThreadId,
        (enableProgress && progress.reportIntervalSeconds > 0.0 ? &progress : NULL),
        options.parallelMode,
        options.boardWorkers,
        options.rootWorkers);

      const ParetoFront front = SearchBridgeState(state, options.depth,
        searchContext);
      const int alphaScore = SingleWorldFrontScore(front, options.depth,
        static_cast<int>(boardIndex));

      AlphaMuBenchmarkBoardResult result;
      result.boardNumber = boardNumber;
      result.boardElapsedSeconds = chrono::duration<double>(
        chrono::steady_clock::now() - boardStart).count();
      result.completionElapsedSeconds = chrono::duration<double>(
        chrono::steady_clock::now() - benchmarkStart).count();
      result.mismatches = static_cast<unsigned>(
        alphaScore != BestScore(data.futList[boardIndex]));
      return result;
    }
  }

double BenchmarkCheckpointIntervalSeconds();
void ReportBenchmarkCheckpoint(
    const BenchmarkMethodSummary& summary,
    const unsigned completedBoards,
    const double elapsedSeconds);
double BenchmarkProgressIntervalSeconds();
void ReportBenchmarkBoardTiming(
    const BenchmarkMethodSummary& summary,
    const unsigned boardNumber,
    const double boardElapsedSeconds,
    const double totalElapsedSeconds);
BenchmarkMethodSummary BenchmarkDDSExactBoards(
    const string& handFile,
    const int maxBoards,
    const string& skipSpec)
  {
    HandFileData data;
    LoadHandFile(handFile, data);
    Check(data.number > 0,
      "DDS benchmark requires at least one board in the selected hand file");

    BenchmarkMethodSummary summary;
    summary.method = "dds";
    summary.handFile = ResolvePath(handFile);
    summary.parallelMode = ALPHA_MU_PARALLEL_SERIAL;
    summary.boardWorkers = 1;
    summary.rootWorkers = 1;
    summary.ddsThreadId = 0;
    summary.configuredBoardWorkers = 1;
    const vector<unsigned> boardNumbers = SelectBenchmarkBoardNumbers(data, maxBoards,
      skipSpec);
    summary.boardsTested = static_cast<unsigned>(boardNumbers.size());
    Check(summary.boardsTested > 0,
      "DDS benchmark selected zero boards to test");

    SetMaxThreads(0);
    const double checkpointSeconds = BenchmarkCheckpointIntervalSeconds();
    const chrono::steady_clock::time_point start = chrono::steady_clock::now();
    double lastCheckpoint = 0.0;
    for (unsigned i = 0; i < boardNumbers.size(); i++)
    {
      const unsigned boardNumber = boardNumbers[i];
      const chrono::steady_clock::time_point boardStart =
        chrono::steady_clock::now();
      const unsigned boardIndex = boardNumber - 1U;
      const int score = SolveDDSLeafWorld(data, static_cast<int>(boardIndex), 0);
      if (score != BestScore(data.futList[boardIndex]))
        summary.mismatches++;

      const double boardElapsed = chrono::duration<double>(
        chrono::steady_clock::now() - boardStart).count();
      summary.perBoardSeconds.push_back(boardElapsed);
      const double elapsed = chrono::duration<double>(
        chrono::steady_clock::now() - start).count();
      ReportBenchmarkBoardTiming(summary, boardNumber, boardElapsed, elapsed);

      if (checkpointSeconds > 0.0)
      {
        if ((elapsed - lastCheckpoint >= checkpointSeconds) ||
            (i + 1 == boardNumbers.size()))
        {
          ReportBenchmarkCheckpoint(summary, i + 1, elapsed);
          lastCheckpoint = elapsed;
        }
      }
    }
    const chrono::steady_clock::time_point end = chrono::steady_clock::now();
    summary.elapsedSeconds = chrono::duration<double>(end - start).count();
    return summary;
  }
BenchmarkMethodSummary BenchmarkAlphaMuExactBoards(
    const AlphaMuBenchmarkOptions& rawOptions)
  {
    const AlphaMuBenchmarkOptions options =
      NormalizeAlphaMuBenchmarkOptions(rawOptions);
    Check(options.depth >= 0,
      "alpha-mu benchmark depth should be non-negative");

    HandFileData data;
    LoadHandFile(options.handFile, data);
    Check(data.number > 0,
      "alpha-mu benchmark requires at least one board in the selected hand file");

    BenchmarkMethodSummary summary;
    summary.method = "alpha_mu";
    summary.handFile = ResolvePath(options.handFile);
    summary.parallelMode = options.parallelMode;
    summary.boardWorkers = options.boardWorkers;
    summary.rootWorkers = options.rootWorkers;
    const vector<unsigned> boardNumbers = SelectBenchmarkBoardNumbers(data,
      options.maxBoards, options.skipSpec);
    summary.boardsTested = static_cast<unsigned>(boardNumbers.size());
    summary.depth = options.depth;
    Check(summary.boardsTested > 0,
      "alpha-mu benchmark selected zero boards to test");

    int baseThreadId = 0;
    const unsigned boardWorkerCount = DetermineAlphaMuBoardWorkerCount(options,
      summary.boardsTested, baseThreadId);
    if (boardWorkerCount <= 1U)
      SetMaxThreads(0);

    DDSInfo info;
    memset(&info, 0, sizeof(info));
    GetDDSInfo(&info);
    baseThreadId = ClampDDSBenchmarkThreadId(baseThreadId,
      max(1, info.noOfThreads));
    summary.ddsThreadId = baseThreadId;
    summary.configuredBoardWorkers = static_cast<int>(boardWorkerCount);

    const double checkpointSeconds = BenchmarkCheckpointIntervalSeconds();
    const chrono::steady_clock::time_point start = chrono::steady_clock::now();
    double lastCheckpoint = 0.0;

    if (boardWorkerCount <= 1U)
    {
      for (unsigned i = 0; i < boardNumbers.size(); i++)
      {
        const AlphaMuBenchmarkBoardResult result = RunAlphaMuBenchmarkBoard(data,
          summary.handFile, summary.boardsTested, boardNumbers[i], options,
          summary.configuredBoardWorkers,
          baseThreadId, start, true);
        summary.mismatches += result.mismatches;
        summary.perBoardSeconds.push_back(result.boardElapsedSeconds);
        ReportBenchmarkBoardTiming(summary, result.boardNumber,
          result.boardElapsedSeconds, result.completionElapsedSeconds);

        if (checkpointSeconds > 0.0)
        {
          if ((result.completionElapsedSeconds - lastCheckpoint >= checkpointSeconds) ||
              (i + 1 == boardNumbers.size()))
          {
            ReportBenchmarkCheckpoint(summary, i + 1,
              result.completionElapsedSeconds);
            lastCheckpoint = result.completionElapsedSeconds;
          }
        }
      }
    }
    else
    {
      vector<AlphaMuBenchmarkBoardResult> results(boardNumbers.size());
      atomic<unsigned> nextBoardIndex(0U);
      exception_ptr workerFailure;
      mutex workerFailureMutex;

      const auto worker = [&](const unsigned workerIndex)
      {
        try
        {
          while (true)
          {
            const unsigned index = nextBoardIndex.fetch_add(1U);
            if (index >= boardNumbers.size())
              break;

            results[index] = RunAlphaMuBenchmarkBoard(data, summary.handFile,
              summary.boardsTested, boardNumbers[index], options,
              summary.configuredBoardWorkers,
              baseThreadId + static_cast<int>(workerIndex), start, false);
          }
        }
        catch (...)
        {
          lock_guard<mutex> lock(workerFailureMutex);
          if (workerFailure == NULL)
            workerFailure = current_exception();
        }
      };

      vector<thread> workers;
      workers.reserve(boardWorkerCount);
      for (unsigned workerIndex = 0; workerIndex < boardWorkerCount; workerIndex++)
        workers.push_back(thread(worker, workerIndex));

      for (unsigned i = 0; i < workers.size(); i++)
        workers[i].join();

      if (workerFailure != NULL)
        rethrow_exception(workerFailure);

      double reportedElapsedSeconds = 0.0;
      for (unsigned i = 0; i < results.size(); i++)
      {
        summary.mismatches += results[i].mismatches;
        summary.perBoardSeconds.push_back(results[i].boardElapsedSeconds);
        reportedElapsedSeconds = max(reportedElapsedSeconds,
          results[i].completionElapsedSeconds);
        ReportBenchmarkBoardTiming(summary, results[i].boardNumber,
          results[i].boardElapsedSeconds, reportedElapsedSeconds);

        if (checkpointSeconds > 0.0)
        {
          if ((reportedElapsedSeconds - lastCheckpoint >= checkpointSeconds) ||
              (i + 1 == results.size()))
          {
            ReportBenchmarkCheckpoint(summary, i + 1, reportedElapsedSeconds);
            lastCheckpoint = reportedElapsedSeconds;
          }
        }
      }
    }

    const chrono::steady_clock::time_point end = chrono::steady_clock::now();
    summary.elapsedSeconds = chrono::duration<double>(end - start).count();
    return summary;
  }
BenchmarkMethodSummary BenchmarkAlphaMuExactBoards(
    const string& handFile,
    const int depth,
    const int maxBoards,
    const string& skipSpec)
  {
    AlphaMuBenchmarkOptions options;
    options.handFile = handFile;
    options.depth = depth;
    options.maxBoards = maxBoards;
    options.skipSpec = skipSpec;
    return BenchmarkAlphaMuExactBoards(options);
  }
double BenchmarkCheckpointIntervalSeconds()
  {
    const char * value = getenv("DDS_ALPHA_MU_BENCHMARK_CHECKPOINT_SECONDS");
    if (value == NULL || *value == '\0')
      return 0.0;

    const double parsed = atof(value);
    return (parsed > 0.0 ? parsed : 0.0);
  }
double BenchmarkProgressIntervalSeconds()
  {
    const char * value = getenv("DDS_ALPHA_MU_BENCHMARK_PROGRESS_SECONDS");
    if (value != NULL && *value != '\0')
    {
      const double parsed = atof(value);
      if (parsed > 0.0)
        return parsed;
    }

    return BenchmarkCheckpointIntervalSeconds();
  }
void MaybeReportBenchmarkBoardProgress(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context)
  {
    BenchmarkBoardProgressContext * progress = context.benchmarkProgress;
    if (progress == NULL || progress->reportIntervalSeconds <= 0.0)
    {
      return;
    }

    progress->recursiveCalls++;
    if ((progress->recursiveCalls & 0x3FFULL) != 0ULL)
      return;

    const double boardElapsed = chrono::duration<double>(
      chrono::steady_clock::now() - progress->boardStart).count();
    if (boardElapsed < progress->nextReportSeconds)
      return;

    const double totalElapsed = chrono::duration<double>(
      chrono::steady_clock::now() - progress->totalStart).count();

    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "ALPHA_MU_BENCHMARK_PROGRESS method="
         << progress->method
         << " file=" << progress->handFile
         << " board=" << progress->boardNumber
         << " total_boards=" << progress->totalBoards
         << " depth=" << progress->depth
         << " parallel=" << AlphaMuParallelModeName(progress->parallelMode)
         << " board_workers=" << progress->boardWorkers
         << " root_workers=" << progress->rootWorkers
         << " dds_thread_id=" << progress->ddsThreadId
         << " configured_board_workers=" << progress->configuredBoardWorkers
         << " board_elapsed_seconds=" << boardElapsed
         << " elapsed_seconds=" << totalElapsed
         << " recursive_calls=" << progress->recursiveCalls
         << " dds_leaf_calls=" << progress->ddsLeafCalls
         << " tricks_remaining=" << tricksRemaining
         << " active_worlds=" << state.possibleWorlds.PopCount()
         << " current_trick_size=" << state.currentTrick.size()
         << " player=" << state.playerToMove
         << endl;

    do
    {
      progress->nextReportSeconds += progress->reportIntervalSeconds;
    }
    while (boardElapsed >= progress->nextReportSeconds);
  }
DDSVsAlphaMuComparison CompareDDSAndAlphaMu(
    const string& handFile,
    const int maxDepth,
    const int maxBoards)
  {
    Check(maxDepth >= 0,
      "DDS comparison max depth should be non-negative");

    HandFileData data;
    LoadHandFile(handFile, data);
    Check(data.number > 0,
      "DDS comparison requires at least one board in the selected hand file");

    const unsigned boardsToTest = BoardsToBenchmark(data, maxBoards);
    Check(boardsToTest > 0,
      "DDS comparison selected zero boards to test");

    DDSVsAlphaMuComparison summary;
    summary.handFile = ResolvePath(handFile);
    summary.boardsTested = boardsToTest;

    SetMaxThreads(0);
    const chrono::steady_clock::time_point ddsStart =
      chrono::steady_clock::now();
    vector<int> ddsScores(boardsToTest, 0);
    for (unsigned i = 0; i < boardsToTest; i++)
      ddsScores[i] = SolveDDSLeafWorld(data, static_cast<int>(i), 0);
    const chrono::steady_clock::time_point ddsEnd =
      chrono::steady_clock::now();

    summary.ddsElapsedSeconds = chrono::duration<double>(ddsEnd - ddsStart).
      count();

    for (int depth = 0; depth <= maxDepth; depth++)
    {
      DDSVsAlphaMuDepthTiming depthTiming;
      depthTiming.depth = depth;

      const chrono::steady_clock::time_point alphaStart =
        chrono::steady_clock::now();
      for (unsigned i = 0; i < boardsToTest; i++)
      {
        const BridgeState state = MakeBridgeStateFromDDSDeal(data.dealList[i]);
        const ParetoFront front = SearchBridgeState(state, depth);
        const int alphaScore = SingleWorldFrontScore(front, depth,
          static_cast<int>(i));
        if (alphaScore != ddsScores[i])
          depthTiming.mismatches++;
      }
      const chrono::steady_clock::time_point alphaEnd =
        chrono::steady_clock::now();

      depthTiming.elapsedSeconds = chrono::duration<double>(
        alphaEnd - alphaStart).count();
      summary.alphaMuDepths.push_back(depthTiming);
    }

    return summary;
  }
void LoadHandFile(
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
int SolveDDSLeafWorld(
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
void CheckDDSLeafBestScores(
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
DDSLeafEvalResult EvaluateDDSLeafThresholdSerial(
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
DDSLeafEvalResult EvaluateDDSLeafThresholdParallel(
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
}
