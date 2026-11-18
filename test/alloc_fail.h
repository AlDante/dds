/*
   Allocation failure injection for the transposition-table tests.

   Included before the TransTable*.cpp sources (see tt_alloc_s.cpp and
   tt_alloc_l.cpp) so that their malloc/calloc calls go through the hooks
   below. A failure can be armed for the n-th allocation (0-based); it fires
   once and then disarms itself.
*/

#ifndef DDS_TEST_ALLOC_FAIL_H
#define DDS_TEST_ALLOC_FAIL_H

// Pull in everything that may mention malloc/calloc before they are
// redefined as macros below.
#include <cstdlib>
#include <stdlib.h>
#include <cstring>
#include <cmath>
#include <math.h>
#include <new>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <atomic>
#include <mutex>

namespace ttalloc
{
  // Function-local statics give one shared instance across translation units.
  inline long& Countdown() { static long v = -1; return v; }
  inline long& Calls() { static long v = 0; return v; }

  /** Fail the n-th allocation from now on; a negative n disarms. */
  inline void Arm(const long n) { Countdown() = n; }
  inline void ResetCalls() { Calls() = 0; }
  inline long CallCount() { return Calls(); }

  inline bool ShouldFail()
  {
    Calls()++;
    long& c = Countdown();
    if (c < 0)
      return false;
    if (c == 0)
    {
      c = -1;
      return true;
    }
    c--;
    return false;
  }

  inline void * Malloc(const size_t n)
  {
    return ShouldFail() ? nullptr : std::malloc(n);
  }

  inline void * Calloc(const size_t n, const size_t m)
  {
    return ShouldFail() ? nullptr : std::calloc(n, m);
  }
}

#define malloc(n) ttalloc::Malloc(n)
#define calloc(n, m) ttalloc::Calloc(n, m)

#endif
