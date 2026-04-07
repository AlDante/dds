/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/


#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../src/Timer.h"

using namespace std;


namespace
{

volatile unsigned long long timerRegressionSink = 1ULL;

struct DetailFields
{
  string name;
  int count;
  long userCum;
  double avgUser;
  double systTotal;
  double avgSyst;
};

struct SumFields
{
  string name;
  int count;
  long userCum;
  string avgUserToken;
  string pctUserToken;
  string systTotalToken;
  string avgSystToken;
  string pctSystToken;
  double avgUser;
  double pctUser;
  double systTotal;
  double avgSyst;
  double pctSyst;
};

}


static void Fail(const string& msg)
{
  cerr << "timer_regression: " << msg << "\n";
  exit(1);
}


static vector<string> Tokens(const string& line)
{
  istringstream iss(line);
  vector<string> tokens;
  string token;

  while (iss >> token)
    tokens.push_back(token);

  return tokens;
}


static string Trim(const string& text)
{
  const size_t first = text.find_first_not_of(" \t\n\r");
  if (first == string::npos)
    return "";

  const size_t last = text.find_last_not_of(" \t\n\r");
  return text.substr(first, last - first + 1);
}


static vector<string> SplitFixedFields(
  const string& line,
  initializer_list<size_t> widths)
{
  vector<string> fields;
  size_t pos = 0;

  for (const size_t width : widths)
  {
    fields.push_back(Trim(line.substr(pos, width)));
    pos += width;
  }

  return fields;
}


static void ExpectTokens(
  const string& line,
  initializer_list<string> expected)
{
  const vector<string> actual = Tokens(line);
  const vector<string> want(expected);

  if (actual != want)
  {
    cerr << "Expected tokens:";
    for (const auto& token : want)
      cerr << " [" << token << "]";
    cerr << "\nActual tokens:";
    for (const auto& token : actual)
      cerr << " [" << token << "]";
    cerr << "\nRaw line: " << line;
    Fail("formatted output mismatch");
  }
}


static void ExpectApprox(
  const double actual,
  const double expected,
  const double tolerance,
  const string& what)
{
  if (fabs(actual - expected) > tolerance)
  {
    ostringstream ss;
    ss << what << " mismatch: actual=" << actual <<
      " expected=" << expected << " tolerance=" << tolerance;
    Fail(ss.str());
  }
}


static void BurnCpuFor(const chrono::milliseconds dur)
{
  const auto start = Clock::now();
  while (Clock::now() - start < dur)
  {
    timerRegressionSink =
      timerRegressionSink * 2862933555777941757ULL + 3037000493ULL;
  }
}


static Timer MakeMeasuredTimer(
  const string& name,
  const int count,
  const chrono::milliseconds work,
  long long * externalClocks = nullptr)
{
  Timer timer;
  timer.SetName(name);

  if (externalClocks != nullptr)
    * externalClocks = 0;

  for (int i = 0; i < count; i++)
  {
    timer.Start();
    const clock_t c0 = clock();
    BurnCpuFor(work);
    const clock_t c1 = clock();
    timer.End();

    if (externalClocks != nullptr)
    {
      const long long diff =
        static_cast<long long>(c1) - static_cast<long long>(c0);
      if (diff > 0)
        * externalClocks += diff;
    }
  }

  return timer;
}


static DetailFields ParseDetailLine(const Timer& timer)
{
  const vector<string> tokens =
    SplitFixedFields(timer.DetailLine(), {15, 10, 11, 11, 11, 11});
  if (tokens.size() != 6)
    Fail("DetailLine should yield 6 tokens");

  DetailFields fields;
  fields.name = tokens[0];
  fields.count = stoi(tokens[1]);
  fields.userCum = stol(tokens[2]);
  fields.avgUser = stod(tokens[3]);
  fields.systTotal = stod(tokens[4]);
  fields.avgSyst = stod(tokens[5]);
  return fields;
}


static SumFields ParseSumLine(
  const Timer& timer,
  const Timer& divisor,
  const string& bname)
{
  const vector<string> tokens =
    SplitFixedFields(timer.SumLine(divisor, bname),
      {14, 9, 11, 7, 5, 11, 7, 5});
  if (tokens.size() != 8)
    Fail("SumLine should yield 8 tokens");

  SumFields fields;
  fields.name = tokens[0];
  fields.count = stoi(tokens[1]);
  fields.userCum = stol(tokens[2]);
  fields.avgUserToken = tokens[3];
  fields.pctUserToken = tokens[4];
  fields.systTotalToken = tokens[5];
  fields.avgSystToken = tokens[6];
  fields.pctSystToken = tokens[7];
  fields.avgUser = stod(tokens[3]);
  fields.pctUser = stod(tokens[4]);
  fields.systTotal = stod(tokens[5]);
  fields.avgSyst = stod(tokens[6]);
  fields.pctSyst = stod(tokens[7]);
  return fields;
}


static void TestResetAndZeroCountBranch()
{
  Timer timer = MakeMeasuredTimer("busy", 1, chrono::milliseconds(3));

  if (! timer.Used())
    Fail("timer should be used before reset");

  if (timer.UserTime() <= 0)
    Fail("timer should accumulate positive user time before reset");

  timer.Reset();

  if (timer.Used())
    Fail("Reset should clear Used state");

  if (timer.UserTime() != 0)
    Fail("Reset should clear accumulated user time");

  const Timer divisor = MakeMeasuredTimer("divisor", 2, chrono::milliseconds(3));
  ExpectTokens(
    timer.SumLine(divisor, "zero"),
    {"zero", "0", "0", "-", "-", "0", "-", "-"});
}


static void TestDetailLineAndClockConversion()
{
  long long externalClocks = 0;
  const int calls = 4;
  const Timer timer =
    MakeMeasuredTimer("detail", calls, chrono::milliseconds(3), &externalClocks);

  if (! timer.Used())
    Fail("Start/End should mark timer as used");

  if (timer.UserTime() <= 0)
    Fail("Start/End should accumulate positive user time");

  const DetailFields detail = ParseDetailLine(timer);
  if (detail.name != "detail")
    Fail("DetailLine should use the timer name");

  if (detail.count != calls)
    Fail("DetailLine should report the number of Start/End calls");

  if (detail.userCum != timer.UserTime())
    Fail("DetailLine user total should match UserTime()");

  ExpectApprox(
    detail.avgUser,
    detail.userCum / static_cast<double>(detail.count),
    0.01,
    "average user time");

  if (detail.systTotal <= 0.)
    Fail("DetailLine should report positive system time after CPU work");

  ExpectApprox(
    detail.avgSyst,
    detail.systTotal / static_cast<double>(detail.count),
    0.2,
    "average system time");

  const double externalMicros =
    1000000. * static_cast<double>(externalClocks) /
    static_cast<double>(CLOCKS_PER_SEC);

  if (externalMicros <= 0.)
    Fail("external clock measurement should be positive");

  ExpectApprox(
    detail.systTotal,
    externalMicros,
    std::max(2000.0, 0.25 * externalMicros),
    "system total conversion");
}


static void TestSumLineAndAddition()
{
  const Timer base = MakeMeasuredTimer("base", 4, chrono::milliseconds(3));
  Timer total = base;
  total += base;

  const DetailFields baseDetail = ParseDetailLine(base);
  const DetailFields totalDetail = ParseDetailLine(total);
  const SumFields sum = ParseSumLine(base, total, "alias");

  if (totalDetail.count != 2 * baseDetail.count)
    Fail("operator += should double the count when adding identical timers");

  if (totalDetail.userCum != 2 * baseDetail.userCum)
    Fail("operator += should double the user total when adding identical timers");

  if (sum.name != "alias")
    Fail("SumLine should prefer the supplied alias name");

  if (sum.count != baseDetail.count)
    Fail("SumLine should report the base timer count");

  if (sum.userCum != baseDetail.userCum)
    Fail("SumLine should report the base timer user total");

  ExpectApprox(sum.avgUser, baseDetail.avgUser, 0.01, "SumLine avg user time");
  ExpectApprox(sum.avgSyst, baseDetail.avgSyst, 0.01, "SumLine avg system time");

  if (sum.pctUserToken != "50.0")
    Fail("SumLine user percentage should be 50.0 for a doubled divisor");

  if (sum.pctSystToken != "50.0")
    Fail("SumLine system percentage should be 50.0 for a doubled divisor");

  const vector<string> namedTokens = Tokens(base.SumLine(total));
  if (namedTokens.empty() || namedTokens[0] != "base")
    Fail("SumLine without alias should use the timer name");
}


static void TestSubtractionBehavior()
{
  const Timer base = MakeMeasuredTimer("base", 3, chrono::milliseconds(3));
  Timer doubled = base;
  doubled += base;

  Timer reduced = doubled;
  reduced -= base;

  const DetailFields baseDetail = ParseDetailLine(base);
  const DetailFields doubledDetail = ParseDetailLine(doubled);
  const DetailFields reducedDetail = ParseDetailLine(reduced);
  const SumFields reducedSum = ParseSumLine(reduced, doubled, "reduced");

  if (reducedDetail.count != doubledDetail.count)
    Fail("operator -= should leave the count unchanged");

  if (reducedDetail.userCum != baseDetail.userCum)
    Fail("operator -= should subtract the user total");

  ExpectApprox(
    reducedDetail.avgUser,
    reducedDetail.userCum / static_cast<double>(reducedDetail.count),
    0.01,
    "reduced average user time");

  if (reducedSum.pctUserToken != "50.0")
    Fail("reduced SumLine user percentage should be 50.0 against doubled timer");

  if (reducedSum.pctSystToken != "50.0")
    Fail("reduced SumLine system percentage should be 50.0 against doubled timer");

  Timer zeroed = base;
  zeroed -= zeroed;
  const SumFields zeroedSum = ParseSumLine(zeroed, doubled, "zeroed");

  if (zeroedSum.userCum != 0)
    Fail("self-subtraction should zero the user total");

  if (zeroedSum.avgUserToken != "0.00" ||
      zeroedSum.pctUserToken != "0.0" ||
      zeroedSum.systTotalToken != "0" ||
      zeroedSum.avgSystToken != "0.00" ||
      zeroedSum.pctSystToken != "0.0")
  {
    Fail("self-subtraction should zero the formatted totals and percentages");
  }
}


int main()
{
  TestResetAndZeroCountBranch();
  TestDetailLineAndClockConversion();
  TestSumLineAndAddition();
  TestSubtractionBehavior();

  cout << "timer_regression: OK\n";
  return 0;
}

