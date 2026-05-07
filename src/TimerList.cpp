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
  timerGroups[TIMER_NO_AB_FRONTEND].SetNames("ABFront");
  timerGroups[TIMER_NO_AB_ITERATION_CONTROL].SetNames("ABIterCtl");
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

  summary.abUserMicros = ABGroup.UserTimeMicroseconds();
  summary.abFrontendUserMicros = timerGroups[TIMER_NO_AB_FRONTEND].UserTimeMicroseconds();
  summary.abIterationControlUserMicros = timerGroups[TIMER_NO_AB_ITERATION_CONTROL].UserTimeMicroseconds();
  summary.abOtherUserMicros =
    summary.abUserMicros -
    summary.abFrontendUserMicros -
    summary.abIterationControlUserMicros;
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

  Timer ABTotal;
  ABGroup.SetNames("AB");
  ABGroup.Sum(ABTotal);
  ABTotal.SetName("Sum");

  fout << timerGroups[0].Header();
  fout << ABGroup.SumLine(ABTotal);
  fout << timerGroups[0].DashLine();
  fout << ABTotal.SumLine(ABTotal) << endl;

  if (ABGroup.Used())
  {
    fout << ABGroup.Header();
    fout << ABGroup.TimerLines(ABTotal);
    fout << ABGroup.DashLine();
    fout << ABTotal.SumLine(ABTotal) << endl;
  }

  Timer ABSubphaseTotal;
  for (unsigned g = TIMER_NO_AB_FRONTEND; g < TIMER_NO_SIZE; g++)
  {
    Timer t;
    timerGroups[g].Sum(t);
    ABSubphaseTotal += t;
  }

  if (ABSubphaseTotal.Used())
  {
    fout << timerGroups[0].Header();
    for (unsigned g = TIMER_NO_AB_FRONTEND; g < TIMER_NO_SIZE; g++)
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

