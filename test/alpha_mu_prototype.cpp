/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/

#include <stdexcept>
#include <string>

#include "alpha_mu_prototype_core.h"
#include "alpha_mu_prototype_tests.h"

using namespace std;
using namespace alpha_mu_prototype;

namespace
{
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
}


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
	  Check(argc >= 3,
		"benchmark_alpha mode requires a hand-file path argument");

	  const string handFile(argv[2]);
	  const int depth = ParseOptionalIntArgument(
		argc, argv, 3, 0, "benchmark_alpha depth");
	  const int maxBoards = ParseOptionalIntArgument(
		argc, argv, 4, 0, "benchmark_alpha max boards");
	  const string skipSpec = (argc >= 6 ? argv[5] : "");
	  const BenchmarkMethodSummary summary = BenchmarkAlphaMuExactBoards(
		handFile, depth, maxBoards, skipSpec);
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
  }

  RunDefaultTestSuite();
  return 0;
}

