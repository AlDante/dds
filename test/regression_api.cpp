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
  }

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

