/**
 * @file alpha_mu.cpp
 * @brief Thin command-line runner for the split alpha-mu solver.
 */

#include <cctype>
#include <stdexcept>
#include <string>

#include "alpha_mu_core.h"
#include "alpha_mu_tests.h"

using namespace std;
using namespace alpha_mu;

namespace
{
  string Uppercase(const string& text)
  {
	string result(text);
	for (unsigned i = 0; i < result.size(); i++)
	  result[i] = static_cast<char>(toupper(static_cast<unsigned char>(result[i])));
	return result;
  }
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
  /** @brief Parse a floating-point CLI argument with full-string validation. */
  bool TryParseDoubleArgument(
	const char * text,
	double& value)
  {
	char * end = NULL;
	value = strtod(text, &end);
	return end != NULL && * text != '\0' && * end == '\0';
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
  int ParseSeatToken(const string& text)
  {
	Check(text.size() == 1,
	  "seat tokens should be one of N, E, S, or W");
	const int seat = SeatIndex(Uppercase(text)[0]);
	Check(seat >= 0,
	  "seat tokens should be one of N, E, S, or W");
	return seat;
  }
  int ParseTrumpToken(const string& text)
  {
	const string upper = Uppercase(text);
	if (upper == "NT" || upper == "N")
	  return -1;
	Check(upper.size() == 1,
	  "trump tokens should be S, H, D, C, or NT");
	return SuitFromPlayChar(upper[0]);
  }
  AlphaMuDecisionPolicy ParseDecisionPolicyToken(const string& text)
  {
	const string upper = Uppercase(text);
	if (upper == "MU")
	  return ALPHA_MU_DECISION_POLICY_MU;
	if (upper == "WEIGHTED")
	  return ALPHA_MU_DECISION_POLICY_WEIGHTED;
	Check(false,
	  "decision policy should be one of mu or weighted");
	return ALPHA_MU_DECISION_POLICY_MU;
  }
  int ParseHandTypeToken(const string& text)
  {
	const string upper = Uppercase(text);
	if (upper == "BALANCED")
	  return HAND_TYPE_BALANCED;
	if (upper == "ONE_SUITER" || upper == "ONE-SUITER" || upper == "ONESUITER")
	  return HAND_TYPE_ONE_SUITER;
	if (upper == "TWO_SUITER" || upper == "TWO-SUITER" || upper == "TWOSUITER")
	  return HAND_TYPE_TWO_SUITER;
	if (upper == "THREE_SUITER" || upper == "THREE-SUITER" || upper == "THREESUITER")
	  return HAND_TYPE_THREE_SUITER;
	Check(false,
	  "hand-type tokens should be balanced, one_suiter, two_suiter, or three_suiter");
	return HAND_TYPE_BALANCED;
  }
  void ParseContractToken(
	const string& text,
	int& level,
	int& trumpSuit)
  {
	Check(! text.empty(),
	  "contract token should not be empty");
	unsigned split = 0;
	while (split < text.size() && isdigit(static_cast<unsigned char>(text[split])))
	  split++;
	Check(split > 0 && split < text.size(),
	  "contract token should look like 3NT, 4S, 5H, 6D, or 7C");
	Check(TryParseIntArgument(text.substr(0, split).c_str(), level) &&
	      level >= 1 && level <= 7,
	  "contract level should be an integer between 1 and 7");
	trumpSuit = ParseTrumpToken(text.substr(split));
  }
  WorldConstraint ParseConstraintToken(const string& text)
  {
	const vector<string> parts = SplitString(text, ':', false);
	Check(! parts.empty(),
	  "constraint token should not be empty");
	const string kind = Uppercase(parts[0]);
	if (kind == "HAS_CARD")
	{
	  Check(parts.size() == 4 && parts[3].size() == 1,
		"has_card constraint should look like has_card:E:S:Q");
	  return WorldConstraint::HasCard(ParseSeatToken(parts[1]),
		ParseTrumpToken(parts[2]), Uppercase(parts[3])[0]);
	}
	if (kind == "NOT_HAS_CARD")
	{
	  Check(parts.size() == 4 && parts[3].size() == 1,
		"not_has_card constraint should look like not_has_card:W:H:T");
	  return WorldConstraint::NotHasCard(ParseSeatToken(parts[1]),
		ParseTrumpToken(parts[2]), Uppercase(parts[3])[0]);
	}
	if (kind == "MIN_LENGTH" || kind == "MAX_LENGTH")
	{
	  int count = 0;
	  Check(parts.size() == 4 && TryParseIntArgument(parts[3].c_str(), count),
		"length constraint should look like min_length:E:S:5");
	  return (kind == "MIN_LENGTH" ?
		WorldConstraint::MinLength(ParseSeatToken(parts[1]),
		  ParseTrumpToken(parts[2]), count) :
		WorldConstraint::MaxLength(ParseSeatToken(parts[1]),
		  ParseTrumpToken(parts[2]), count));
	}
	if (kind == "MIN_HCP" || kind == "MAX_HCP")
	{
	  int count = 0;
	  Check(parts.size() == 3 && TryParseIntArgument(parts[2].c_str(), count),
		"HCP constraint should look like min_hcp:E:12");
	  return (kind == "MIN_HCP" ?
		WorldConstraint::MinHCP(ParseSeatToken(parts[1]), count) :
		WorldConstraint::MaxHCP(ParseSeatToken(parts[1]), count));
	}
	if (kind == "BALANCED")
	{
	  Check(parts.size() == 2,
		"balanced constraint should look like balanced:E");
	  return WorldConstraint::Balanced(ParseSeatToken(parts[1]));
	}
	if (kind == "HAND_TYPE")
	{
	  Check(parts.size() == 3,
		"hand_type constraint should look like hand_type:E:one_suiter");
	  return WorldConstraint::HandTypeConstraint(ParseSeatToken(parts[1]),
		ParseHandTypeToken(parts[2]));
	}
	if (kind == "PARTNERSHIP_MIN_LENGTH" || kind == "PARTNERSHIP_MAX_LENGTH")
	{
	  int count = 0;
	  Check(parts.size() == 4 && TryParseIntArgument(parts[3].c_str(), count),
		"partnership length constraint should look like partnership_min_length:E:H:8");
	  return (kind == "PARTNERSHIP_MIN_LENGTH" ?
		WorldConstraint::PartnershipMinLength(ParseSeatToken(parts[1]),
		  ParseTrumpToken(parts[2]), count) :
		WorldConstraint::PartnershipMaxLength(ParseSeatToken(parts[1]),
		  ParseTrumpToken(parts[2]), count));
	}
	if (kind == "PARTNERSHIP_MIN_HCP" || kind == "PARTNERSHIP_MAX_HCP")
	{
	  int count = 0;
	  Check(parts.size() == 3 && TryParseIntArgument(parts[2].c_str(), count),
		"partnership HCP constraint should look like partnership_min_hcp:E:20");
	  return (kind == "PARTNERSHIP_MIN_HCP" ?
		WorldConstraint::PartnershipMinHCP(ParseSeatToken(parts[1]), count) :
		WorldConstraint::PartnershipMaxHCP(ParseSeatToken(parts[1]), count));
	}
	Check(false,
	  string("unknown constraint token kind ") + parts[0]);
	return WorldConstraint();
  }
  WorldPlausibilityHint ParsePlausibilityHintToken(const string& text)
  {
	const vector<string> parts = SplitString(text, '|', true);
	Check(parts.size() >= 2 && parts.size() <= 3,
	  "plausibility hints should look like weight|constraint or weight|constraint|label");
	int weight = 0;
	Check(TryParseIntArgument(parts[0].c_str(), weight) && weight >= 0,
	  "plausibility hint weight should be a non-negative integer");
	const WorldConstraint constraint = ParseConstraintToken(parts[1]);
	const string label = (parts.size() == 3 && ! parts[2].empty() ?
	  parts[2] : ConstraintToString(constraint));
	return WorldPlausibilityHint::Prefer(constraint, weight, label);
  }
}


/**
 * @brief Dispatch to regression, benchmark, and comparison modes.
 *
 * The actual alpha-mu implementation lives in `alpha_mu_core.*`; this
 * file only validates and routes command-line arguments.
 */
int main(int argc, char ** argv)
{
  if (argc >= 1 && argv[0] != NULL)
	SetAlphaMuExecutablePath(argv[0]);

  if (argc >= 2)
  {
	const string mode(argv[1]);
	if (mode == "debug_assert_worldmask_capacity")
	{
	  RunDebugWorldMaskCapacityAssertionTrigger();
	  return 0;
	}
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
	  PrintAlphaMuStatus("DDS exact benchmark OK");
	  return 0;
	}
	if (mode == "benchmark_alpha")
	{
	  const AlphaMuBenchmarkOptions options =
		ParseBenchmarkAlphaOptions(argc, argv);
	  const BenchmarkMethodSummary summary =
		BenchmarkAlphaMuExactBoards(options);
	  ReportBenchmarkMethodSummary(summary);
	  PrintAlphaMuStatus("alpha-mu exact benchmark OK");
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
	  PrintAlphaMuStatus("DDS vs alpha-mu comparison OK");
	  return 0;
	}
	if (mode == "bridge_dds")
	{
	  RunBridgeDDSTestSuite();
	  return 0;
	}
	if (mode == "pbn_recommend")
	{
	  string filePath;

	  for (int a = 2; a < argc; a++)
	  {
		const string flag(argv[a]);
		Check(IsLongOption(argv[a]),
		  "pbn_recommend mode expects only long options");
		Check(a + 1 < argc,
		  string("pbn_recommend option ") + flag + " requires a value");
		const string value(argv[++a]);

		if (flag == "--file")
		  filePath = value;
		else
		  Check(false,
			string("pbn_recommend mode does not recognize option ") + flag);
	  }

	  Check(! filePath.empty(),
		"pbn_recommend mode requires --file");

	  const PBNBoardRecord board = LoadPBNBoardRecord(filePath);
	  const ExactPlayLineResult result = RecommendExactPlayLine(board);
	  ReportExactPlayLineResult(result);
	  PrintAlphaMuStatus("PBN exact recommendation OK");
	  return 0;
	}
	if (mode == "solve")
	{
	  Check(argc >= 4,
		"solve mode requires: <hand-file> <board-number> [depth] [max-worlds] [--time seconds] [--prefix-cards n] [--seed n]");

	  const string handFile(argv[2]);
	  const int boardNumber = ParseOptionalIntArgument(
		argc, argv, 3, 1, "solve board number");
	  const int depth = ParseOptionalIntArgument(
		argc, argv, 4, 1, "solve depth");
	  const int maxWorlds = ParseOptionalIntArgument(
		argc, argv, 5, 50, "solve max worlds");

	  double timeBudget = 0.0;
	  int prefixCards = -1;
	  unsigned samplingSeed = 42U;
	  for (int a = 4; a < argc - 1; a++)
	  {
		const string flag(argv[a]);
		if (flag == "--time")
		{
		  Check(TryParseDoubleArgument(argv[a + 1], timeBudget),
			"solve time budget should be a valid number");
		}
		else if (flag == "--prefix-cards")
		{
		  Check(TryParseIntArgument(argv[a + 1], prefixCards) && prefixCards >= 0,
			"solve prefix-cards should be a non-negative integer");
		}
		else if (flag == "--seed")
		{
		  int parsedSeed = 0;
		  Check(TryParseIntArgument(argv[a + 1], parsedSeed) && parsedSeed >= 0,
			"solve seed should be a non-negative integer");
		  samplingSeed = static_cast<unsigned>(parsedSeed);
		}
	  }

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
		static_cast<unsigned>(maxWorlds), timeBudget, prefixCards, samplingSeed);

	  ReportAlphaMuSolveResult(result);
	  PrintAlphaMuStatus("alpha-mu solve OK");
	  return 0;
	}
	if (mode == "decision")
	{
	  AlphaMuDecisionPointRequest request;
	  string dealPbn;
	  string playCards;
	  int explicitPlayCount = -1;
	  bool haveDeal = false;
	  bool haveLeader = false;
	  bool haveDeclarer = false;
	  bool haveContract = false;

	  for (int a = 2; a < argc; a++)
	  {
		const string flag(argv[a]);
		Check(IsLongOption(argv[a]),
		  "decision mode expects only long options");
		Check(a + 1 < argc,
		  string("decision option ") + flag + " requires a value");
		const string value(argv[++a]);

		if (flag == "--deal-pbn")
		{
		  dealPbn = value;
		  haveDeal = true;
		}
		else if (flag == "--leader")
		{
		  request.deal.first = ParseSeatToken(value);
		  haveLeader = true;
		}
		else if (flag == "--declarer")
		{
		  request.declarerSeat = ParseSeatToken(value);
		  haveDeclarer = true;
		}
		else if (flag == "--contract")
		{
		  int trumpSuit = -1;
		  ParseContractToken(value, request.contractLevel, trumpSuit);
		  request.deal.trump = (trumpSuit < 0 ? 4 : trumpSuit);
		  haveContract = true;
		}
		else if (flag == "--play-cards")
		  playCards = value;
		else if (flag == "--play-count")
		{
		  Check(TryParseIntArgument(value.c_str(), explicitPlayCount) &&
			  explicitPlayCount >= 0,
			"decision play-count should be a non-negative integer");
		}
		else if (flag == "--prefix-cards")
		{
		  Check(TryParseIntArgument(value.c_str(), request.prefixCards) &&
			  request.prefixCards >= 0,
			"decision prefix-cards should be a non-negative integer");
		}
		else if (flag == "--depth")
		{
		  Check(TryParseIntArgument(value.c_str(), request.depth),
			"decision depth should be a valid integer");
		}
		else if (flag == "--max-worlds")
		{
		  int parsed = 0;
		  Check(TryParseIntArgument(value.c_str(), parsed) && parsed >= 0,
			"decision max-worlds should be a non-negative integer");
		  request.maxWorlds = static_cast<unsigned>(parsed);
		}
		else if (flag == "--time")
		{
		  Check(TryParseDoubleArgument(value.c_str(), request.timeBudgetSeconds),
			"decision time budget should be a valid number");
		}
		else if (flag == "--seed")
		{
		  int parsed = 0;
		  Check(TryParseIntArgument(value.c_str(), parsed) && parsed >= 0,
			"decision seed should be a non-negative integer");
		  request.samplingSeed = static_cast<unsigned>(parsed);
		}
		else if (flag == "--decision-policy")
		  request.decisionPolicy = ParseDecisionPolicyToken(value);
		else if (flag == "--constraint")
		  request.informationOverrides.biddingConstraints.push_back(
			ParseConstraintToken(value));
		else if (flag == "--plausibility")
		  request.informationOverrides.plausibilityHints.push_back(
			ParsePlausibilityHintToken(value));
		else
		  Check(false,
			string("decision mode does not recognize option ") + flag);
	  }

	  Check(haveDeal,
		"decision mode requires --deal-pbn");
	  Check(haveLeader,
		"decision mode requires --leader");
	  Check(haveDeclarer,
		"decision mode requires --declarer");
	  Check(haveContract,
		"decision mode requires --contract");
	  Check(dealPbn.size() < sizeof(request.deal.remainCards),
		"decision deal-pbn should fit into dealPBN.remainCards");
	  strcpy(request.deal.remainCards, dealPbn.c_str());

	  playTracePBN play;
	  memset(&play, 0, sizeof(play));
	  if (! playCards.empty())
	  {
		Check(playCards.size() % 2 == 0,
		  "decision play-cards should consist of suit/rank pairs");
		const int inferredCount = static_cast<int>(playCards.size() / 2);
		play.number = (explicitPlayCount >= 0 ? explicitPlayCount : inferredCount);
		Check(play.number <= inferredCount,
		  "decision play-count should not exceed the supplied play-card pairs");
		Check(playCards.size() < sizeof(play.cards),
		  "decision play-cards should fit into playTracePBN.cards");
		strcpy(play.cards, playCards.c_str());
	  }
	  request.playHistory = ParsePBNPlayHistory(play, request.deal.first,
		(request.deal.trump == 4 ? -1 : request.deal.trump));

	  const AlphaMuSolveResult result = SolveAlphaMuDecisionPoint(request);
	  ReportAlphaMuSolveResult(result);
	  PrintAlphaMuStatus("alpha-mu decision OK");
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
	  PrintAlphaMuStatus("play history parse validation OK");
	  TestPartialInformationWorldGeneration();
	  PrintAlphaMuStatus("partial-information world generation OK");
	  TestFollowSuitNarrowingInPartialInformation();
	  PrintAlphaMuStatus("follow-suit narrowing in partial information OK");
	  TestEndToEndSolveAlphaMu();
	  PrintAlphaMuStatus("end-to-end alpha-mu solve OK");
	  return 0;
	}
  }

  RunDefaultTestSuite();
  return 0;
}

