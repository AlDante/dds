/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/

/*
   TimerList consists of a number of groups, one for each piece
   of the code being timed (ABsearch etc).

   Each group corresponds to something that should be timed at
   multiple AB depths, i.e. cards played. The first card of a
   new game is number 48, and the last card is number 0.

   The AB timer is special, as the AB functions are recursive
   and so their timing includes not only the other functions they
   contain, but also their own recursive calls at lower depths.
   The AB timer group must be the first one.

   The object calculates an approximation to exclusive function
   times, so it is a "poor man's profiler".

   For AB, first the times at depth-1 are subtracted out, and then
   the times for all calls at the same depth are subtracted out.
   This still leaves the overhead of the timing itself. As an
   approximation, there is one timing overhead left for each
   function, and it is on the order of the execution time of
   Evaluate(), which is a very fast function.

   TIMER_START and TIMER_END are macros for bracketing code
   to be timed, so

   TIMER_START(TIMER_NO_AB, depth);
   ABsearch(...);
   TIMER_END(TIMER_NO_AB, depth);

   This avoids the tedious #ifdef's at every place of a timer.
 */

#ifndef DDS_TIMERLIST_H
#define DDS_TIMERLIST_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "TimerGroup.h"
#include "debug.h"

using namespace std;


#ifdef DDS_TIMING
  #define TIMER_START(g, a) thrp->timerList.Start(g, a)
  #define TIMER_END(g, a) thrp->timerList.End(g, a)
#else
  #define TIMER_START(g, a) 1
  #define TIMER_END(g, a) 1
#endif

enum ABTimerType
{
  TIMER_NO_AB = 0,
  TIMER_NO_MAKE = 1,
  TIMER_NO_UNDO = 2,
  TIMER_NO_EVALUATE = 3,
  TIMER_NO_NEXTMOVE = 4,
  TIMER_NO_QT = 5,
  TIMER_NO_LT = 6,
  TIMER_NO_MOVEGEN = 7,
  TIMER_NO_LOOKUP = 8,
  TIMER_NO_BUILD = 9,
  TIMER_NO_AB_TERMINAL = 10,
  TIMER_NO_AB_CHILDLOOP = 11,
  TIMER_NO_AB_CUTOFF = 12,
  TIMER_NO_AB_RECURSE_SETUP = 13,
  TIMER_NO_AB_NODE_SETUP = 14,
  TIMER_NO_AB_LOOP_CONTROL = 15,
  TIMER_NO_AB_POST_CHILD = 16,
  TIMER_NO_AB_TT_PREP = 17,
  TIMER_NO_AB_SETUP = 18,
  TIMER_NO_AB_TERMINAL_CONTROL = 19,
  TIMER_NO_AB_ITERATION_CONTROL = 20,
  TIMER_NO_AB_STORE_PREP = 21,
  TIMER_NO_SIZE = 22
};


struct TimerListSummary
{
  long abUserMicros;
  long makeUserMicros;
  long undoUserMicros;
  long evaluateUserMicros;
  long nextMoveUserMicros;
  long quickTricksUserMicros;
  long laterTricksUserMicros;
  long moveGenUserMicros;
  long lookupUserMicros;
  long buildUserMicros;
  long abTerminalUserMicros;
  long abChildLoopUserMicros;
  long abCutoffUserMicros;
  long abRecurseSetupUserMicros;
  long abNodeSetupUserMicros;
  long abLoopControlUserMicros;
  long abPostChildUserMicros;
  long abTTPrepUserMicros;
  long abSetupUserMicros;
  long abTerminalControlUserMicros;
  long abIterationControlUserMicros;
  long abStorePrepUserMicros;
  long abOtherUserMicros;

  TimerListSummary() :
    abUserMicros(0),
    makeUserMicros(0),
    undoUserMicros(0),
    evaluateUserMicros(0),
    nextMoveUserMicros(0),
    quickTricksUserMicros(0),
    laterTricksUserMicros(0),
    moveGenUserMicros(0),
    lookupUserMicros(0),
    buildUserMicros(0),
    abTerminalUserMicros(0),
    abChildLoopUserMicros(0),
    abCutoffUserMicros(0),
    abRecurseSetupUserMicros(0),
    abNodeSetupUserMicros(0),
    abLoopControlUserMicros(0),
    abPostChildUserMicros(0),
    abTTPrepUserMicros(0),
    abSetupUserMicros(0),
    abTerminalControlUserMicros(0),
    abIterationControlUserMicros(0),
    abStorePrepUserMicros(0),
    abOtherUserMicros(0)
  {
  }

  TimerListSummary operator-(const TimerListSummary& other) const
  {
    TimerListSummary delta;
    delta.abUserMicros = abUserMicros - other.abUserMicros;
    delta.makeUserMicros = makeUserMicros - other.makeUserMicros;
    delta.undoUserMicros = undoUserMicros - other.undoUserMicros;
    delta.evaluateUserMicros = evaluateUserMicros - other.evaluateUserMicros;
    delta.nextMoveUserMicros = nextMoveUserMicros - other.nextMoveUserMicros;
    delta.quickTricksUserMicros = quickTricksUserMicros - other.quickTricksUserMicros;
    delta.laterTricksUserMicros = laterTricksUserMicros - other.laterTricksUserMicros;
    delta.moveGenUserMicros = moveGenUserMicros - other.moveGenUserMicros;
    delta.lookupUserMicros = lookupUserMicros - other.lookupUserMicros;
    delta.buildUserMicros = buildUserMicros - other.buildUserMicros;
    delta.abTerminalUserMicros = abTerminalUserMicros - other.abTerminalUserMicros;
    delta.abChildLoopUserMicros = abChildLoopUserMicros - other.abChildLoopUserMicros;
    delta.abCutoffUserMicros = abCutoffUserMicros - other.abCutoffUserMicros;
    delta.abRecurseSetupUserMicros = abRecurseSetupUserMicros - other.abRecurseSetupUserMicros;
    delta.abNodeSetupUserMicros = abNodeSetupUserMicros - other.abNodeSetupUserMicros;
    delta.abLoopControlUserMicros = abLoopControlUserMicros - other.abLoopControlUserMicros;
    delta.abPostChildUserMicros = abPostChildUserMicros - other.abPostChildUserMicros;
    delta.abTTPrepUserMicros = abTTPrepUserMicros - other.abTTPrepUserMicros;
    delta.abSetupUserMicros = abSetupUserMicros - other.abSetupUserMicros;
    delta.abTerminalControlUserMicros = abTerminalControlUserMicros - other.abTerminalControlUserMicros;
    delta.abIterationControlUserMicros = abIterationControlUserMicros - other.abIterationControlUserMicros;
    delta.abStorePrepUserMicros = abStorePrepUserMicros - other.abStorePrepUserMicros;
    delta.abOtherUserMicros = abOtherUserMicros - other.abOtherUserMicros;
    return delta;
  }
};


class TimerList
{
  private:

    vector<TimerGroup> timerGroups;

  public:
    TimerList();

    ~TimerList();

    void Reset();

    void Start(
      const ABTimerType groupno,
      const int timerno);

    void End(
      const ABTimerType groupno,
      const int timerno);

    bool Used() const;

    TimerListSummary Summary() const;

    void PrintStats(ofstream& fout) const;
};

#endif
