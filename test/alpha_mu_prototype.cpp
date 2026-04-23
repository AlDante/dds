/**
 * @file alpha_mu_prototype.cpp
 * @brief Thin command-line runner for the split alpha-mu prototype.
 */

#include <stdexcept>
#include <string>

#include "alpha_mu_prototype_core.h"
#include "alpha_mu_prototype_tests.h"

using namespace std;
using namespace alpha_mu_prototype;

namespace
{
  bool IsLongOption(const char * text)
  {
	return text != NULL && strncmp(text, "--", 2) == 0;
  }
  /** @brief Parse an integer CLI argument with full-string validation. */
  bool TryParseIntArgument(
	const char * text,
	int& value)
  {
	try
	{
	  const string raw(text);
	  size_t used = 0;
	  value = stoi(raw, &used);
	  return used == raw.size();
	}
	catch (const invalid_argument&)
	{
	  return false;
	}
	catch (const out_of_range&)
	{
	  return false;
	}
  }
  /** @brief Read an optional integer CLI argument or fall back to a default. */
  int ParseOptionalIntArgument(
	const int argc,
	char ** argv,
	const int index,
	const int defaultValue,
	const string& description)
  {
	if (argc <= index)
	  return defaultValue;

	int value = defaultValue;
	Check(TryParseIntArgument(argv[index], value),
	  description + " should be a valid integer");
	return value;
  }
  AlphaMuBenchmarkOptions ParseBenchmarkAlphaOptions(
	const int argc,
	char ** argv)
  {
	Check(argc >= 3,
	  "benchmark_alpha mode requires a hand-file path argument");

	AlphaMuBenchmarkOptions options;
	options.handFile = argv[2];
	options.depth = ParseOptionalIntArgument(
	  argc, argv, 3, 0, "benchmark_alpha depth");
	options.maxBoards = ParseOptionalIntArgument(
	  argc, argv, 4, 0, "benchmark_alpha max boards");

	int index = 5;
	if (argc > index && ! IsLongOption(argv[index]))
	{
	  options.skipSpec = argv[index];
	  index++;
	}

	while (index < argc)
	{
	  const string flag(argv[index]);
	  Check(index + 1 < argc,
		string("benchmark_alpha option ") + flag +
		" requires a value");

	  if (flag == "--parallel")
	    options.parallelMode = ParseAlphaMuParallelModeName(argv[index + 1]);
	  else if (flag == "--board-workers")
	  {
	    Check(TryParseIntArgument(argv[index + 1], options.boardWorkers),
		  "benchmark_alpha board-workers should be a valid integer");
	  }
	  else if (flag == "--root-workers")
	  {
	    Check(TryParseIntArgument(argv[index + 1], options.rootWorkers),
		  "benchmark_alpha root-workers should be a valid integer");
	  }
	  else if (flag == "--dds-thread-id")
	  {
	    Check(TryParseIntArgument(argv[index + 1], options.ddsThreadId),
		  "benchmark_alpha dds-thread-id should be a valid integer");
	  }
	  else
	  {
	    Check(false,
		  string("benchmark_alpha does not recognize option ") + flag);
	  }

	  index += 2;
	}

	return NormalizeAlphaMuBenchmarkOptions(options);
	}
}


/**
 * @brief Dispatch to regression, benchmark, and comparison modes.
 *
 * The actual alpha-mu implementation lives in `alpha_mu_prototype_core.*`; this
 * file only validates and routes command-line arguments.
 */
int main(int argc, char ** argv)
{
  if (argc >= 2)
  {
	const string mode(argv[1]);
	if (mode == "benchmark_dds")
	{
	  Check(argc >= 3,
		"benchmark_dds mode requires a hand-file path argument");

	  const string handFile(argv[2]);
	  const int maxBoards = ParseOptionalIntArgument(
		argc, argv, 3, 0, "benchmark_dds max boards");
	  const string skipSpec = (argc >= 5 ? argv[4] : "");
	  const BenchmarkMethodSummary summary = BenchmarkDDSExactBoards(
		handFile, maxBoards, skipSpec);
	  ReportBenchmarkMethodSummary(summary);
	  PrintPrototypeStatus("DDS exact benchmark OK");
	  return 0;
	}
	if (mode == "benchmark_alpha")
	{
	  const AlphaMuBenchmarkOptions options =
		ParseBenchmarkAlphaOptions(argc, argv);
	  const BenchmarkMethodSummary summary =
		BenchmarkAlphaMuExactBoards(options);
	  ReportBenchmarkMethodSummary(summary);
	  PrintPrototypeStatus("alpha-mu exact benchmark OK");
	  return 0;
	}
	if (mode == "compare_dds")
	{
	  Check(argc >= 3,
		"compare_dds mode requires a hand-file path argument");

	  const string handFile(argv[2]);
	  const int maxDepth = ParseOptionalIntArgument(
		argc, argv, 3, 1, "compare_dds max depth");
	  const int maxBoards = ParseOptionalIntArgument(
		argc, argv, 4, 0, "compare_dds max boards");
	  const DDSVsAlphaMuComparison comparison = CompareDDSAndAlphaMu(
		handFile, maxDepth, maxBoards);
	  ReportDDSVsAlphaMuComparison(comparison);
	  PrintPrototypeStatus("DDS vs alpha-mu comparison OK");
	  return 0;
	}
	if (mode == "bridge_dds")
	{
	  RunBridgeDDSTestSuite();
	  return 0;
	}
	if (mode == "solve")
	{
	  Check(argc >= 4,
		"solve mode requires: <hand-file> <board-number> [depth] [max-worlds]");

	  const string handFile(argv[2]);
	  const int boardNumber = ParseOptionalIntArgument(
		argc, argv, 3, 1, "solve board number");
	  const int depth = ParseOptionalIntArgument(
		argc, argv, 4, 1, "solve depth");
	  const int maxWorlds = ParseOptionalIntArgument(
		argc, argv, 5, 50, "solve max worlds");

	  HandFileData data;
	  LoadHandFile(handFile, data);
	  Check(boardNumber >= 1 && boardNumber <= data.number,
		"board number out of range for the selected hand file");

	  const int idx = boardNumber - 1;
	  const dealPBN& deal = data.dealList[idx];
	  const playTracePBN& play = data.playList[idx];

	  // Determine declarer: the player before the opening leader
	  const int declarerSeat = (deal.first + 3) % 4;

	  const AlphaMuSolveResult result = SolveAlphaMu(
		deal, declarerSeat, play, depth,
		static_cast<unsigned>(maxWorlds));

	  ReportAlphaMuSolveResult(result);
	  PrintPrototypeStatus("alpha-mu solve OK");
	  return 0;
	}
	if (mode == "tt_bench")
	{
	  Check(argc >= 4,
		"tt_bench mode requires: <hand-file> <board-number> [depth] [max-worlds]");

	  const string handFile(argv[2]);
	  const int boardNumber = ParseOptionalIntArgument(
		argc, argv, 3, 1, "tt_bench board number");
	  const int maxDepth = ParseOptionalIntArgument(
		argc, argv, 4, 2, "tt_bench depth");
	  const int maxWorlds = ParseOptionalIntArgument(
		argc, argv, 5, 50, "tt_bench max worlds");

	  HandFileData data;
	  LoadHandFile(handFile, data);
	  Check(boardNumber >= 1 && boardNumber <= data.number,
		"board number out of range");

	  const int idx = boardNumber - 1;
	  const dealPBN& deal = data.dealList[idx];
	  const playTracePBN& play = data.playList[idx];
	  const int declarerSeat = (deal.first + 3) % 4;
	  const int trumpSuit = deal.trump;

	  const vector<PlayHistoryEvent> history =
		ParsePBNPlayHistory(play, deal.first, trumpSuit);
	  BridgeState state = MakeBridgeStateFromPartialInformation(
		deal, declarerSeat, history, static_cast<unsigned>(maxWorlds));

	  SetMaxThreads(0);
	  const SearchExecutionContext context;

	  cout << "TT Benchmark: " << state.possibleWorlds.PopCount()
		   << " worlds, board " << boardNumber << endl;

	  for (int d = 1; d <= maxDepth; d++)
	  {
		// Without TT
		auto t0 = chrono::steady_clock::now();
		ParetoFront f1 = SearchBridgeStateInternal(state, d, context);
		auto t1 = chrono::steady_clock::now();
		double noTT = chrono::duration<double>(t1 - t0).count();

		// With TT (cold)
		InitZobrist();
		BridgeTranspositionTable tt(1U << 18);
		BridgeTTStats stats;
		auto t2 = chrono::steady_clock::now();
		ParetoFront f2 = SearchBridgeStateWithTT(state, d, context, &tt, &stats);
		auto t3 = chrono::steady_clock::now();
		double withTTcold = chrono::duration<double>(t3 - t2).count();

		// With TT (warm)
		BridgeTTStats stats2;
		auto t4 = chrono::steady_clock::now();
		ParetoFront f3 = SearchBridgeStateWithTT(state, d, context, &tt, &stats2);
		auto t5 = chrono::steady_clock::now();
		double withTTwarm = chrono::duration<double>(t5 - t4).count();

		cout << "  depth=" << d
			 << "  no_tt=" << fixed << setprecision(4) << noTT << "s"
			 << "  tt_cold=" << withTTcold << "s"
			 << "  tt_warm=" << withTTwarm << "s"
			 << "  stores=" << stats.stores
			 << "  hits_cold=" << stats.hits
			 << "  hits_warm=" << stats2.hits
			 << "  vectors=" << f1.vectors.size()
			 << endl;
	  }
	  return 0;
	}
	if (mode == "partial")
	{
	  TestParsePlayHistoryValidation();
	  PrintPrototypeStatus("play history parse validation OK");
	  TestPartialInformationWorldGeneration();
	  PrintPrototypeStatus("partial-information world generation OK");
	  TestFollowSuitNarrowingInPartialInformation();
	  PrintPrototypeStatus("follow-suit narrowing in partial information OK");
	  TestEndToEndSolveAlphaMu();
	  PrintPrototypeStatus("end-to-end alpha-mu solve OK");
	  return 0;
	}
  }

  RunDefaultTestSuite();
  return 0;
}

