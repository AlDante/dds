/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/


/*
   See TimerList.h for some description.
*/

// #include <sstream>

// #include "dds.h"
#include "TimerList.h"


TimerList::TimerList()
{
  TimerList::Reset();
}


TimerList::~TimerList()
{
}


void TimerList::Reset()
{
  timerGroups.resize(TIMER_NO_SIZE);

  timerGroups[TIMER_NO_AB].SetNames("AB");
  timerGroups[TIMER_NO_MAKE].SetNames("Make");
  timerGroups[TIMER_NO_UNDO].SetNames("Undo");
  timerGroups[TIMER_NO_EVALUATE].SetNames("Evaluate");
  timerGroups[TIMER_NO_NEXTMOVE].SetNames("NextMove");
  timerGroups[TIMER_NO_QT].SetNames("QuickTricks");
  timerGroups[TIMER_NO_LT].SetNames("LaterTricks");
  timerGroups[TIMER_NO_MOVEGEN].SetNames("MoveGen");
  timerGroups[TIMER_NO_LOOKUP].SetNames("Lookup");
  timerGroups[TIMER_NO_BUILD].SetNames("Build");
  timerGroups[TIMER_NO_AB_TERMINAL].SetNames("ABTerm");
  timerGroups[TIMER_NO_AB_CHILDLOOP].SetNames("ABChild");
  timerGroups[TIMER_NO_AB_CUTOFF].SetNames("ABCut");
  timerGroups[TIMER_NO_AB_RECURSE_SETUP].SetNames("ABRecurse");
  timerGroups[TIMER_NO_AB_NODE_SETUP].SetNames("ABNode");
  timerGroups[TIMER_NO_AB_LOOP_CONTROL].SetNames("ABLoop");
  timerGroups[TIMER_NO_AB_POST_CHILD].SetNames("ABPost");
  timerGroups[TIMER_NO_AB_TT_PREP].SetNames("ABTT");
  timerGroups[TIMER_NO_AB_SETUP].SetNames("ABSetup");
  timerGroups[TIMER_NO_AB_TERMINAL_CONTROL].SetNames("ABTermCtl");
  timerGroups[TIMER_NO_AB_ITERATION_CONTROL].SetNames("ABIterCtl");
  timerGroups[TIMER_NO_AB_STORE_PREP].SetNames("ABStore");
}


void TimerList::Start(
  const ABTimerType groupno,
  const int timerno)
{
  if (groupno >= TIMER_NO_SIZE)
    return;
  timerGroups[groupno].Start(timerno);
}


void TimerList::End(
  const ABTimerType groupno,
  const int timerno)
{
  if (groupno >= TIMER_NO_SIZE)
    return;
  timerGroups[groupno].End(timerno);
}


bool TimerList::Used() const
{
  for (unsigned g = 0; g < TIMER_NO_SIZE; g++)
  {
    if (timerGroups[g].Used())
      return true;
  }
  return false;
}


TimerListSummary TimerList::Summary() const
{
  TimerListSummary summary;

  TimerGroup ABGroup;
  ABGroup = timerGroups[TIMER_NO_AB];
  ABGroup.Differentiate();
  for (unsigned g = TIMER_NO_MAKE; g <= TIMER_NO_BUILD; g++)
    ABGroup -= timerGroups[g];

  summary.abUserMicros = ABGroup.UserTimeMicroseconds();
  summary.makeUserMicros = timerGroups[TIMER_NO_MAKE].UserTimeMicroseconds();
  summary.undoUserMicros = timerGroups[TIMER_NO_UNDO].UserTimeMicroseconds();
  summary.evaluateUserMicros = timerGroups[TIMER_NO_EVALUATE].UserTimeMicroseconds();
  summary.nextMoveUserMicros = timerGroups[TIMER_NO_NEXTMOVE].UserTimeMicroseconds();
  summary.quickTricksUserMicros = timerGroups[TIMER_NO_QT].UserTimeMicroseconds();
  summary.laterTricksUserMicros = timerGroups[TIMER_NO_LT].UserTimeMicroseconds();
  summary.moveGenUserMicros = timerGroups[TIMER_NO_MOVEGEN].UserTimeMicroseconds();
  summary.lookupUserMicros = timerGroups[TIMER_NO_LOOKUP].UserTimeMicroseconds();
  summary.buildUserMicros = timerGroups[TIMER_NO_BUILD].UserTimeMicroseconds();
  summary.abTerminalUserMicros = timerGroups[TIMER_NO_AB_TERMINAL].UserTimeMicroseconds();
  summary.abChildLoopUserMicros = timerGroups[TIMER_NO_AB_CHILDLOOP].UserTimeMicroseconds();
  summary.abCutoffUserMicros = timerGroups[TIMER_NO_AB_CUTOFF].UserTimeMicroseconds();
  summary.abRecurseSetupUserMicros = timerGroups[TIMER_NO_AB_RECURSE_SETUP].UserTimeMicroseconds();
  summary.abNodeSetupUserMicros = timerGroups[TIMER_NO_AB_NODE_SETUP].UserTimeMicroseconds();
  summary.abLoopControlUserMicros = timerGroups[TIMER_NO_AB_LOOP_CONTROL].UserTimeMicroseconds();
  summary.abPostChildUserMicros = timerGroups[TIMER_NO_AB_POST_CHILD].UserTimeMicroseconds();
  summary.abTTPrepUserMicros = timerGroups[TIMER_NO_AB_TT_PREP].UserTimeMicroseconds();
  summary.abSetupUserMicros = timerGroups[TIMER_NO_AB_SETUP].UserTimeMicroseconds();
  summary.abTerminalControlUserMicros = timerGroups[TIMER_NO_AB_TERMINAL_CONTROL].UserTimeMicroseconds();
  summary.abIterationControlUserMicros = timerGroups[TIMER_NO_AB_ITERATION_CONTROL].UserTimeMicroseconds();
  summary.abStorePrepUserMicros = timerGroups[TIMER_NO_AB_STORE_PREP].UserTimeMicroseconds();
  summary.abOtherUserMicros =
    summary.abUserMicros -
    summary.abTerminalUserMicros -
    summary.abSetupUserMicros -
    summary.abTerminalControlUserMicros -
    summary.abIterationControlUserMicros -
    summary.abStorePrepUserMicros;
  return summary;
}


void TimerList::PrintStats(ofstream& fout) const
{
  if (! TimerList::Used())
    return;

  // Approximate the exclusive times of each function.
  // The ABsearch*() functions are recursively nested,
  // so subtract out the one below.
  // The other ones are subtracted out based on knowledge
  // of the functions.

  TimerGroup ABGroup;
  ABGroup = timerGroups[0];
  ABGroup.Differentiate();
  for (unsigned g = TIMER_NO_MAKE; g <= TIMER_NO_BUILD; g++)
    ABGroup -= timerGroups[g];

  Timer ABTotal;
  ABGroup.SetNames("AB");
  ABGroup.Sum(ABTotal);
  ABTotal.SetName("Sum");

  Timer sumTotal = ABTotal;
  for (unsigned g = TIMER_NO_MAKE; g <= TIMER_NO_BUILD; g++)
  {
    Timer t;
    timerGroups[g].Sum(t);
    sumTotal += t;
  }

  fout << timerGroups[0].Header();
  fout << ABGroup.SumLine(sumTotal);
  for (unsigned g = TIMER_NO_MAKE; g <= TIMER_NO_BUILD; g++)
    fout << timerGroups[g].SumLine(sumTotal);
  fout << timerGroups[0].DashLine();
  fout << sumTotal.SumLine(sumTotal) << endl;

  if (ABGroup.Used())
  {
    fout << ABGroup.Header();
    fout << ABGroup.TimerLines(ABTotal);
    fout << ABGroup.DashLine();
    fout << ABTotal.SumLine(ABTotal) << endl;
  }

  Timer ABSubphaseTotal;
  for (unsigned g = TIMER_NO_AB_TERMINAL; g < TIMER_NO_SIZE; g++)
  {
    Timer t;
    timerGroups[g].Sum(t);
    ABSubphaseTotal += t;
  }

  if (ABSubphaseTotal.Used())
  {
    fout << timerGroups[0].Header();
    for (unsigned g = TIMER_NO_AB_TERMINAL; g < TIMER_NO_SIZE; g++)
      fout << timerGroups[g].SumLine(ABTotal);

    Timer ABOther = ABTotal;
    ABOther -= ABSubphaseTotal;
    fout << ABOther.SumLine(ABTotal, "ABOther");
    fout << timerGroups[0].DashLine();
    fout << ABTotal.SumLine(ABTotal, "ABTotal") << endl;
  }

#ifdef DDS_TIMING_DETAILS
  fout << timerGroups[0].DetailHeader();
  for (unsigned g = 0; g < TIMER_NO_SIZE; g++)
    fout << timerGroups[g].DetailLines();
  fout << endl;
#endif
}

