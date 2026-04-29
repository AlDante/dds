/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <algorithm>

#include "../include/dll.h"
#include "parse.h"
#include "compare.h"
#include "print.h"

using namespace std;


static bool FileExists(const string& fname)
{
  ifstream fin(fname.c_str());
  return fin.good();
}


static string ResolvePath(const string& fname)
{
  if (FileExists(fname))
    return fname;

  if (FileExists("../" + fname))
    return "../" + fname;

  return fname;
}


static void PrintDDSerror(const int code)
{
  char line[80];
  ErrorMessage(code, line);
  cerr << "DDS error " << code << ": " << line << "\n";
}


static void Fail(const string& msg)
{
  cerr << "regression_api: " << msg << "\n";
  exit(1);
}


static void CheckReturn(
  const string& tag,
  const int ret)
{
  if (ret == RETURN_NO_FAULT)
    return;

  cerr << "Call failed: " << tag << "\n";
  PrintDDSerror(ret);
  exit(1);
}


static int BestScore(const futureTricks& fut)
{
  if (fut.cards <= 0)
    return 0;

  int best = fut.score[0];
  for (int i = 1; i < fut.cards; i++)
    best = max(best, fut.score[i]);

  return best;
}


static void CheckSolveGolden(
  const dealPBN& dl,
  const futureTricks& expected)
{
  futureTricks fut;
  memset(&fut, 0, sizeof(fut));

  const int ret = SolveBoardPBN(dl, -1, 3, 1, &fut, 0);
  CheckReturn("SolveBoardPBN(target=-1, solutions=3, mode=1)", ret);

  if (! compare_FUT(fut, expected))
  {
    cerr << "Golden FUT mismatch\n";
    cerr << "Computed:\n";
    print_FUT(fut);
    cerr << "\nExpected:\n";
    print_FUT(expected);
    cerr << "\n";
    Fail("SolveBoardPBN(target=-1, solutions=3) != golden FUT");
  }
}


static void CheckSolveConsistency(
  const dealPBN& dl,
  const futureTricks& expected)
{
  futureTricks bestOne;
  futureTricks exact;
  futureTricks tooHigh;

  memset(&bestOne, 0, sizeof(bestOne));
  memset(&exact, 0, sizeof(exact));
  memset(&tooHigh, 0, sizeof(tooHigh));

  int ret = SolveBoardPBN(dl, -1, 1, 1, &bestOne, 0);
  CheckReturn("SolveBoardPBN(target=-1, solutions=1, mode=1)", ret);

  if (bestOne.cards <= 0)
    Fail("SolveBoardPBN(target=-1, solutions=1) returned no cards");

  const int optimum = bestOne.score[0];
  const int goldenBest = BestScore(expected);

  if (optimum != goldenBest)
  {
    cerr << "Computed optimum: " << optimum << "\n";
    cerr << "Golden optimum:   " << goldenBest << "\n";
    Fail("optimum score from SolveBoardPBN(target=-1, solutions=1) does not match golden FUT");
  }

  if (optimum > 0)
  {
    ret = SolveBoardPBN(dl, optimum, 2, 1, &exact, 0);
    CheckReturn("SolveBoardPBN(target=optimum, solutions=2, mode=1)", ret);

    if (exact.cards <= 0)
      Fail("SolveBoardPBN(target=optimum, solutions=2) returned no cards");

    for (int i = 0; i < exact.cards; i++)
    {
      if (exact.score[i] != optimum)
      {
        cerr << "Card " << i << " returned score " << exact.score[i] << ", expected " << optimum << "\n";
        Fail("SolveBoardPBN(target=optimum, solutions=2) returned a non-optimum score");
      }
    }

    if (optimum < 13)
    {
      ret = SolveBoardPBN(dl, optimum + 1, 1, 1, &tooHigh, 0);
      CheckReturn("SolveBoardPBN(target=optimum+1, solutions=1, mode=1)", ret);

      if (tooHigh.cards != 0)
      {
        cerr << "Unexpectedly found cards for target " << (optimum + 1) << "\n";
        print_FUT(tooHigh);
        Fail("SolveBoardPBN(target=optimum+1) unexpectedly succeeded");
      }
    }
  }
  else
  {
    ret = SolveBoardPBN(dl, 1, 1, 1, &tooHigh, 0);
    CheckReturn("SolveBoardPBN(target=1, solutions=1, mode=1)", ret);

    if (tooHigh.cards != 0)
    {
      cerr << "Unexpectedly found cards for target 1 when optimum is 0\n";
      print_FUT(tooHigh);
      Fail("SolveBoardPBN(target=1) unexpectedly succeeded when optimum is 0");
    }
  }
}


static void CheckParConsistency(
  const dealPBN& dl,
  const int dealer,
  const int vul,
  const ddTableResults& expectedTable,
  const parResults& expectedPar,
  const parResultsDealer& expectedDealerPar)
{
  ddTableDealPBN tableDealPBN;
  ddTableResults table;
  parResults parFromTable;
  parResults parFromCalc;
  parResultsDealer dealerPar;
  parResultsMaster dealerBin;
  parResultsMaster sidesBin[2];
  parTextResults sidesText;
  char dealerText[128];

  memset(&tableDealPBN, 0, sizeof(tableDealPBN));
  memset(&table, 0, sizeof(table));
  memset(&parFromTable, 0, sizeof(parFromTable));
  memset(&parFromCalc, 0, sizeof(parFromCalc));
  memset(&dealerPar, 0, sizeof(dealerPar));
  memset(&dealerBin, 0, sizeof(dealerBin));
  memset(&sidesBin, 0, sizeof(sidesBin));
  memset(&sidesText, 0, sizeof(sidesText));
  memset(dealerText, 0, sizeof(dealerText));

  strcpy(tableDealPBN.cards, dl.remainCards);

  int ret = CalcDDtablePBN(tableDealPBN, &table);
  CheckReturn("CalcDDtablePBN", ret);

  if (! compare_TABLE(table, expectedTable))
  {
    cerr << "TABLE mismatch\nComputed:\n";
    print_TABLE(table);
    cerr << "\nExpected:\n";
    print_TABLE(expectedTable);
    cerr << "\n";
    Fail("CalcDDtablePBN != golden TABLE");
  }

  ret = Par(&table, &parFromTable, vul);
  CheckReturn("Par", ret);

  if (! compare_PAR(parFromTable, expectedPar))
  {
    cerr << "PAR mismatch\nComputed:\n";
    print_PAR(parFromTable);
    cerr << "\nExpected:\n";
    print_PAR(expectedPar);
    cerr << "\n";
    Fail("Par != golden PAR");
  }

  ret = CalcParPBN(tableDealPBN, &table, vul, &parFromCalc);
  CheckReturn("CalcParPBN", ret);

  if (! compare_PAR(parFromCalc, expectedPar))
  {
    cerr << "CalcParPBN mismatch\nComputed:\n";
    print_PAR(parFromCalc);
    cerr << "\nExpected:\n";
    print_PAR(expectedPar);
    cerr << "\n";
    Fail("CalcParPBN != golden PAR");
  }

  ret = DealerPar(&table, &dealerPar, dealer, vul);
  CheckReturn("DealerPar", ret);

  if (! compare_DEALERPAR(dealerPar, expectedDealerPar))
  {
    cerr << "DealerPar mismatch\nComputed:\n";
    print_DEALERPAR(dealerPar);
    cerr << "\nExpected:\n";
    print_DEALERPAR(expectedDealerPar);
    cerr << "\n";
    Fail("DealerPar != golden PAR2");
  }

  ret = DealerParBin(&table, &dealerBin, dealer, vul);
  CheckReturn("DealerParBin", ret);

  if (dealerBin.score != dealerPar.score)
    Fail("DealerParBin.score != DealerPar.score");

  if (dealerBin.number != dealerPar.number)
    Fail("DealerParBin.number != DealerPar.number");

  ret = ConvertToDealerTextFormat(&dealerBin, dealerText);
  CheckReturn("ConvertToDealerTextFormat", ret);

  if (dealerText[0] == '\0')
    Fail("ConvertToDealerTextFormat returned empty text");

  ret = SidesParBin(&table, sidesBin, vul);
  CheckReturn("SidesParBin", ret);

  ret = ConvertToSidesTextFormat(sidesBin, &sidesText);
  CheckReturn("ConvertToSidesTextFormat", ret);

  if (sidesText.parText[0][0] == '\0')
    Fail("ConvertToSidesTextFormat returned empty NS text");
}


static void ExpectReturnAndText(
  const string& tag,
  const int ret,
  const int expectedRet,
  const string& expectedText)
{
  char line[80];
  memset(line, 0, sizeof(line));
  ErrorMessage(ret, line);

  if (ret != expectedRet)
  {
    cerr << "Unexpected return code for " << tag << "\n";
    cerr << "Expected: " << expectedRet << "\n";
    cerr << "Actual:   " << ret << "\n";
    cerr << "Message:  " << line << "\n";
    Fail(tag + " returned the wrong error code");
  }

  if (expectedText != line)
  {
    cerr << "Unexpected error text for " << tag << "\n";
    cerr << "Expected: " << expectedText << "\n";
    cerr << "Actual:   " << line << "\n";
    Fail(tag + " returned the wrong error text");
  }
}


static void CheckPlayGolden(
  const dealPBN& dl,
  const playTracePBN& play,
  const solvedPlay& expected)
{
  if (play.number <= 0)
    return;

  solvedPlay solved;
  memset(&solved, 0, sizeof(solved));

  const int ret = AnalysePlayPBN(dl, play, &solved, 0);
  CheckReturn("AnalysePlayPBN", ret);

  if (! compare_TRACE(solved, expected))
  {
    cerr << "Play trace mismatch\n";
    cerr << "Play:\n";
    print_PLAY(play);
    cerr << "\nComputed:\n";
    print_TRACE(solved);
    cerr << "\nExpected:\n";
    print_TRACE(expected);
    cerr << "\n";
    Fail("AnalysePlayPBN != golden TRACE");
  }
}


static void CheckSolveAllBoardsBatch(
  const dealPBN * deals,
  const futureTricks * expected,
  const int count)
{
  boardsPBN boards;
  solvedBoards solved;
  memset(&boards, 0, sizeof(boards));
  memset(&solved, 0, sizeof(solved));

  boards.noOfBoards = count;
  for (int i = 0; i < count; i++)
  {
    boards.deals[i] = deals[i];
    boards.target[i] = -1;
    boards.solutions[i] = 3;
    boards.mode[i] = 1;
  }

  const int ret = SolveAllBoards(&boards, &solved);
  CheckReturn("SolveAllBoards", ret);

  if (solved.noOfBoards != count)
    Fail("SolveAllBoards returned the wrong solved.noOfBoards");

  for (int i = 0; i < count; i++)
  {
    if (! compare_FUT(solved.solvedBoard[i], expected[i]))
    {
      cerr << "SolveAllBoards mismatch on board " << i << "\n";
      cerr << "Computed:\n";
      print_FUT(solved.solvedBoard[i]);
      cerr << "\nExpected:\n";
      print_FUT(expected[i]);
      cerr << "\n";
      Fail("SolveAllBoards != golden FUT batch");
    }
  }
}


static void CheckCalcAllTablesBatch(
  const dealPBN * deals,
  const ddTableResults * expectedTables,
  const parResults * expectedPars,
  const int count)
{
  ddTableDealsPBN tableDeals;
  ddTablesRes results;
  allParResults pars;
  int trumpFilter[DDS_STRAINS] = {0, 0, 0, 0, 0};
  memset(&tableDeals, 0, sizeof(tableDeals));
  memset(&results, 0, sizeof(results));
  memset(&pars, 0, sizeof(pars));

  tableDeals.noOfTables = count;
  for (int i = 0; i < count; i++)
    strcpy(tableDeals.deals[i].cards, deals[i].remainCards);

  const int ret = CalcAllTablesPBN(&tableDeals, 0, trumpFilter, &results, &pars);
  CheckReturn("CalcAllTablesPBN", ret);

  for (int i = 0; i < count; i++)
  {
    if (! compare_TABLE(results.results[i], expectedTables[i]))
    {
      cerr << "CalcAllTablesPBN TABLE mismatch on board " << i << "\n";
      cerr << "Computed:\n";
      print_TABLE(results.results[i]);
      cerr << "\nExpected:\n";
      print_TABLE(expectedTables[i]);
      cerr << "\n";
      Fail("CalcAllTablesPBN != golden TABLE batch");
    }
  }

  (void) expectedPars;
}


static void CheckAnalyseAllPlaysBatch(
  const vector<dealPBN>& deals,
  const vector<playTracePBN>& plays,
  const vector<solvedPlay>& expected)
{
  boardsPBN boards;
  playTracesPBN traces;
  solvedPlays solved;
  memset(&boards, 0, sizeof(boards));
  memset(&traces, 0, sizeof(traces));
  memset(&solved, 0, sizeof(solved));

  boards.noOfBoards = static_cast<int>(deals.size());
  traces.noOfBoards = static_cast<int>(plays.size());
  for (size_t i = 0; i < deals.size(); i++)
  {
    boards.deals[i] = deals[i];
    traces.plays[i] = plays[i];
  }

  const int ret = AnalyseAllPlaysPBN(&boards, &traces, &solved, 1);
  CheckReturn("AnalyseAllPlaysPBN", ret);

  if (solved.noOfBoards != static_cast<int>(expected.size()))
    Fail("AnalyseAllPlaysPBN returned the wrong number of solved traces");

  for (size_t i = 0; i < expected.size(); i++)
  {
    if (! compare_TRACE(solved.solved[i], expected[i]))
    {
      cerr << "AnalyseAllPlaysPBN mismatch on board " << i << "\n";
      cerr << "Computed:\n";
      print_TRACE(solved.solved[i]);
      cerr << "\nExpected:\n";
      print_TRACE(expected[i]);
      cerr << "\n";
      Fail("AnalyseAllPlaysPBN != golden TRACE batch");
    }
  }
}


static void CheckMalformedInputs(const dealPBN& validDeal)
{
  futureTricks fut;
  solvedBoards solvedBoardsBatch;
  solvedPlay solvedTrace;
  ddTableResults table;
  parResults par;
  ddTablesRes tableBatch;
  allParResults parBatch;

  memset(&fut, 0, sizeof(fut));
  memset(&solvedBoardsBatch, 0, sizeof(solvedBoardsBatch));
  memset(&solvedTrace, 0, sizeof(solvedTrace));
  memset(&table, 0, sizeof(table));
  memset(&par, 0, sizeof(par));
  memset(&tableBatch, 0, sizeof(tableBatch));
  memset(&parBatch, 0, sizeof(parBatch));

  dealPBN badFirst = validDeal;
  badFirst.first = 4;
  ExpectReturnAndText(
    "SolveBoardPBN invalid first",
    SolveBoardPBN(badFirst, -1, 3, 1, &fut, 0),
    RETURN_FIRST_WRONG,
    TEXT_FIRST_WRONG);

  boardsPBN tooManyBoards;
  memset(&tooManyBoards, 0, sizeof(tooManyBoards));
  tooManyBoards.noOfBoards = MAXNOOFBOARDS + 1;
  ExpectReturnAndText(
    "SolveAllBoards too many boards",
    SolveAllBoards(&tooManyBoards, &solvedBoardsBatch),
    RETURN_TOO_MANY_BOARDS,
    TEXT_TOO_MANY_BOARDS);

  ddTableDealPBN badTableDeal;
  memset(&badTableDeal, 0, sizeof(badTableDeal));
  strcpy(badTableDeal.cards, "X:AKQJ.T987.654.32 .... .... ....");
  ExpectReturnAndText(
    "CalcDDtablePBN malformed PBN",
    CalcDDtablePBN(badTableDeal, &table),
    RETURN_PBN_FAULT,
    TEXT_PBN_FAULT);

  ddTableDealsPBN badTableBatch;
  int trumpFilter[DDS_STRAINS] = {0, 0, 0, 0, 0};
  memset(&badTableBatch, 0, sizeof(badTableBatch));
  badTableBatch.noOfTables = 1;
  strcpy(badTableBatch.deals[0].cards, badTableDeal.cards);
  ExpectReturnAndText(
    "CalcAllTablesPBN malformed PBN",
    CalcAllTablesPBN(&badTableBatch, 0, trumpFilter, &tableBatch, &parBatch),
    RETURN_PBN_FAULT,
    TEXT_PBN_FAULT);

  ExpectReturnAndText(
    "CalcParPBN malformed PBN",
    CalcParPBN(badTableDeal, &table, 0, &par),
    RETURN_PBN_FAULT,
    TEXT_PBN_FAULT);

  playTracePBN badPlay;
  memset(&badPlay, 0, sizeof(badPlay));
  badPlay.number = 1;
  strcpy(badPlay.cards, "S1");
  ExpectReturnAndText(
    "AnalysePlayPBN malformed play string",
    AnalysePlayPBN(validDeal, badPlay, &solvedTrace, 0),
    RETURN_PLAY_FAULT,
    TEXT_PLAY_FAULT);

  boardsPBN tooManyPlayBoards;
  playTracesPBN tooManyPlays;
  solvedPlays solvedPlaysBatch;
  memset(&tooManyPlayBoards, 0, sizeof(tooManyPlayBoards));
  memset(&tooManyPlays, 0, sizeof(tooManyPlays));
  memset(&solvedPlaysBatch, 0, sizeof(solvedPlaysBatch));
  tooManyPlayBoards.noOfBoards = MAXNOOFBOARDS + 1;
  tooManyPlays.noOfBoards = MAXNOOFBOARDS + 1;
  ExpectReturnAndText(
    "AnalyseAllPlaysPBN too many boards",
    AnalyseAllPlaysPBN(&tooManyPlayBoards, &tooManyPlays, &solvedPlaysBatch, 1),
    RETURN_TOO_MANY_BOARDS,
    TEXT_TOO_MANY_BOARDS);
}


static void CheckRepeatedPublicCalls(
  const int dealer,
  const int vul,
  const dealPBN& solveDeal,
  const futureTricks& solveExpected,
  const ddTableResults& tableExpected,
  const parResults& parExpected,
  const parResultsDealer& dealerParExpected,
  const dealPBN * playDeal,
  const playTracePBN * play,
  const solvedPlay * trace,
  const dealPBN * batchDeals,
  const futureTricks * batchFuts,
  const ddTableResults * batchTables,
  const parResults * batchPars,
  const int batchCount,
  const vector<dealPBN>& playBatchDeals,
  const vector<playTracePBN>& playBatch,
  const vector<solvedPlay>& traceBatch)
{
  DDSInfo info;
  memset(&info, 0, sizeof(info));
  GetDDSInfo(&info);

  vector<int> threadSettings;
  threadSettings.push_back(1);
  threadSettings.push_back(0);
  if (info.noOfThreads > 1)
    threadSettings.push_back(min(2, info.noOfThreads));

  for (size_t setting = 0; setting < threadSettings.size(); setting++)
  {
    SetMaxThreads(threadSettings[setting]);
    for (int repeat = 0; repeat < 3; repeat++)
    {
      CheckSolveGolden(solveDeal, solveExpected);
      CheckSolveConsistency(solveDeal, solveExpected);
      CheckParConsistency(solveDeal, dealer, vul, tableExpected,
        parExpected, dealerParExpected);

      if (playDeal != NULL && play != NULL && trace != NULL)
        CheckPlayGolden(* playDeal, * play, * trace);

      CheckSolveAllBoardsBatch(batchDeals, batchFuts, batchCount);
      CheckCalcAllTablesBatch(batchDeals, batchTables, batchPars, batchCount);

      if (! playBatch.empty())
        CheckAnalyseAllPlaysBatch(playBatchDeals, playBatch, traceBatch);
    }
  }

  FreeMemory();
  SetResources(0, 1);
  CheckSolveGolden(solveDeal, solveExpected);
  CheckParConsistency(solveDeal, dealer, vul, tableExpected,
    parExpected, dealerParExpected);
  if (playDeal != NULL && play != NULL && trace != NULL)
    CheckPlayGolden(* playDeal, * play, * trace);
  CheckSolveAllBoardsBatch(batchDeals, batchFuts, batchCount);
  CheckCalcAllTablesBatch(batchDeals, batchTables, batchPars, batchCount);
  if (! playBatch.empty())
    CheckAnalyseAllPlaysBatch(playBatchDeals, playBatch, traceBatch);
}


static void CheckHandFile(const string& fname)
{
  int number = 0;
  bool GIBmode = false;
  int * dealer_list = NULL;
  int * vul_list = NULL;
  dealPBN * deal_list = NULL;
  futureTricks * fut_list = NULL;
  ddTableResults * table_list = NULL;
  parResults * par_list = NULL;
  parResultsDealer * dealerpar_list = NULL;
  playTracePBN * play_list = NULL;
  solvedPlay * trace_list = NULL;

  if (! read_file(fname, number, GIBmode,
      &dealer_list,
      &vul_list,
      &deal_list,
      &fut_list,
      &table_list,
      &par_list,
      &dealerpar_list,
      &play_list,
      &trace_list))
  {
    Fail("read_file failed for " + fname);
  }

  cerr << "Checking " << fname << " (" << number << " hands)\n";

  const int batchCount = min(number, 4);
  vector<dealPBN> playBatchDeals;
  vector<playTracePBN> playBatch;
  vector<solvedPlay> traceBatch;

  for (int i = 0; i < number; i++)
  {
    CheckParConsistency(
      deal_list[i],
      dealer_list[i],
      vul_list[i],
      table_list[i],
      par_list[i],
      dealerpar_list[i]);

    CheckSolveGolden(deal_list[i], fut_list[i]);
    CheckSolveConsistency(deal_list[i], fut_list[i]);

    if (play_list[i].number > 0)
    {
      CheckPlayGolden(deal_list[i], play_list[i], trace_list[i]);
      if (playBatch.size() < 3U)
      {
        playBatchDeals.push_back(deal_list[i]);
        playBatch.push_back(play_list[i]);
        traceBatch.push_back(trace_list[i]);
      }
    }
  }

  CheckSolveAllBoardsBatch(deal_list, fut_list, batchCount);
  CheckCalcAllTablesBatch(deal_list, table_list, par_list, batchCount);

  if (! playBatch.empty())
    CheckAnalyseAllPlaysBatch(playBatchDeals, playBatch, traceBatch);

  CheckMalformedInputs(deal_list[0]);
  CheckRepeatedPublicCalls(
    dealer_list[0],
    vul_list[0],
    deal_list[0],
    fut_list[0],
    table_list[0],
    par_list[0],
    dealerpar_list[0],
    (playBatch.empty() ? NULL : &playBatchDeals[0]),
    (playBatch.empty() ? NULL : &playBatch[0]),
    (playBatch.empty() ? NULL : &traceBatch[0]),
    deal_list,
    fut_list,
    table_list,
    par_list,
    batchCount,
    playBatchDeals,
    playBatch,
    traceBatch);

  free(dealer_list);
  free(vul_list);
  free(deal_list);
  free(fut_list);
  free(table_list);
  free(par_list);
  free(dealerpar_list);
  free(play_list);
  free(trace_list);
}


int main(int argc, char * argv[])
{
  set_constants();

  // Keep the test deterministic and simple.
  SetResources(0, 1);

  vector<string> files;

  if (argc > 1)
  {
    for (int i = 1; i < argc; i++)
      files.push_back(argv[i]);
  }
  else
  {
    files.push_back(ResolvePath("hands/list10.txt"));
    files.push_back(ResolvePath("hands/thomas1.txt"));
    files.push_back(ResolvePath("hands/thomas2.txt"));
  }

  for (size_t i = 0; i < files.size(); i++)
    CheckHandFile(files[i]);

  FreeMemory();

  cout << "regression_api: OK\n";
  return 0;
}

