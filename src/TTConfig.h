/*
   DDS, a bridge double dummy solver.

   Transposition table size configuration.

   These defaults can be overridden at compile time via -D flags, e.g.:
     -DTHREADMEM_LARGE_DEF_MB=512 -DTHREADMEM_LARGE_MAX_MB=1024

   Or at runtime via environment variables:
     DDS_THREADMEM_LARGE_DEF_MB, DDS_THREADMEM_LARGE_MAX_MB,
     DDS_THREADMEM_SMALL_DEF_MB, DDS_THREADMEM_SMALL_MAX_MB
*/

#ifndef DDS_TTCONFIG_H
#define DDS_TTCONFIG_H

#include <cstdlib>

// Compile-time defaults (override with -D flags).
#ifndef THREADMEM_SMALL_DEF_MB
  #define THREADMEM_SMALL_DEF_MB 20
#endif

#ifndef THREADMEM_SMALL_MAX_MB
  #define THREADMEM_SMALL_MAX_MB 30
#endif

#ifndef THREADMEM_LARGE_DEF_MB
  #define THREADMEM_LARGE_DEF_MB 95
#endif

#ifndef THREADMEM_LARGE_MAX_MB
  #define THREADMEM_LARGE_MAX_MB 160
#endif

// Runtime configuration: environment variables override compile-time defaults.
inline int TTConfigValue(const char * envVar, int compiletimeDefault)
{
  const char * val = getenv(envVar);
  if (val)
  {
    int v = atoi(val);
    if (v > 0)
      return v;
  }
  return compiletimeDefault;
}

inline int TTSmallDefMB()
{
  return TTConfigValue("DDS_THREADMEM_SMALL_DEF_MB", THREADMEM_SMALL_DEF_MB);
}

inline int TTSmallMaxMB()
{
  return TTConfigValue("DDS_THREADMEM_SMALL_MAX_MB", THREADMEM_SMALL_MAX_MB);
}

inline int TTLargeDefMB()
{
  return TTConfigValue("DDS_THREADMEM_LARGE_DEF_MB", THREADMEM_LARGE_DEF_MB);
}

inline int TTLargeMaxMB()
{
  return TTConfigValue("DDS_THREADMEM_LARGE_MAX_MB", THREADMEM_LARGE_MAX_MB);
}

#endif

