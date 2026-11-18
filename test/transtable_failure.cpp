/**
 * Failure-injection tests for TransTableS / TransTableL.
 *
 * Every allocation inside MakeTT (and, for TransTableL, the first page
 * allocation in Lookup) is made to fail in turn. Each failure must surface as
 * std::bad_alloc, leave the object destructible without leaks or invalid
 * frees (run under ASan via `make asan-check`), and leave it usable so that a
 * retry succeeds.
 */

#include <cstdio>
#include <cstdlib>
#include <new>

#include "alloc_fail.h"
#include "../src/TransTableS.h"
#include "../src/TransTableL.h"


static int failures = 0;

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) \
    { \
      std::fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
      failures++; \
    } \
  } while (0)


static const int kHandLookup[DDS_SUITS][15] = {};

// Used only by the TT debug printers, which are linked in but not exercised.
unsigned char cardRank[16] = {};
unsigned char cardSuit[DDS_STRAINS] = {};


// Runs fn, returning true iff it throws std::bad_alloc.
template <typename F>
static bool ThrowsBadAlloc(F fn)
{
  try
  {
    fn();
  }
  catch (const std::bad_alloc&)
  {
    return true;
  }
  return false;
}


template <typename TT>
static void CheckMakeTTFailures(const char * name)
{
  // Count the allocations of a successful MakeTT.
  long total;
  {
    TT tt;
    tt.SetMemoryMaximum(160);
    ttalloc::Arm(-1);
    ttalloc::ResetCalls();
    tt.MakeTT();
    total = ttalloc::CallCount();
  }
  CHECK(total > 0, "MakeTT should allocate");

  for (long k = 0; k < total; k++)
  {
    TT tt;
    tt.SetMemoryMaximum(160);
    ttalloc::Arm(k);
    const bool thrown = ThrowsBadAlloc([&] { tt.MakeTT(); });
    ttalloc::Arm(-1);

    if (! thrown)
    {
      std::fprintf(stderr, "%s: allocation %ld of %ld did not throw\n",
        name, k, total);
      failures++;
      continue;
    }

    // The object must have rolled back and be usable again.
    bool retried = true;
    try
    {
      tt.MakeTT();
      tt.Init(kHandLookup);
    }
    catch (...)
    {
      retried = false;
    }
    CHECK(retried, "MakeTT should succeed on retry after a failure");
    CHECK(tt.MemoryInUse() > 0, "MemoryInUse after retry");
    // Destructor runs ReturnAllMemory.
  }
}


static void CheckLargeFirstPageFailures()
{
  // The first page needs a pool header (calloc) and a block list (malloc);
  // fail each in turn.
  for (long k = 0; k < 2; k++)
  {
    TransTableL tt;
    tt.MakeTT();
    tt.Init(kHandLookup);

    const unsigned short aggr[DDS_SUITS] = {0, 0, 0, 0};
    const int dist[DDS_SUITS] = {0, 0, 0, 0};
    bool lower = false;

    ttalloc::Arm(k);
    const bool thrown = ThrowsBadAlloc([&]
    {
      tt.Lookup(1, 0, aggr, dist, 0, lower);
    });
    ttalloc::Arm(-1);
    CHECK(thrown, "first page allocation failure should throw bad_alloc");

    // A retry must start from scratch and succeed (empty table: no hit).
    nodeCardsType const * hit = nullptr;
    bool ok = true;
    try
    {
      hit = tt.Lookup(1, 0, aggr, dist, 0, lower);
    }
    catch (...)
    {
      ok = false;
    }
    CHECK(ok, "Lookup should succeed after a failed page allocation");
    CHECK(hit == nullptr, "fresh table should not return a hit");
  }
}


int main()
{
  CheckMakeTTFailures<TransTableS>("TransTableS");
  CheckMakeTTFailures<TransTableL>("TransTableL");
  CheckLargeFirstPageFailures();

  if (failures)
  {
    std::fprintf(stderr, "transtable_failure: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("transtable_failure: all checks passed\n");
  return 0;
}
