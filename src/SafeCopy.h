/*
   DDS, a bridge double dummy solver.

   Bounded string copies into the fixed-size char buffers of the C API.
*/

#ifndef DDS_SAFECOPY_H
#define DDS_SAFECOPY_H

#include <cstddef>
#include <cstring>
#include <string>


/**
 * Copies `len` characters of `src` into `dst` and always NUL-terminates.
 * At most `cap - 1` characters are copied, so the result is truncated
 * rather than overflowing. Does nothing if `cap` is 0.
 */
inline void CopyTruncated(
  char * dst,
  const std::size_t cap,
  const char * src,
  const std::size_t len)
{
  if (cap == 0)
    return;

  const std::size_t n = (len < cap - 1 ? len : cap - 1);
  if (n > 0)
    std::memcpy(dst, src, n);
  dst[n] = '\0';
}


/** Bounded copy of a C string into a buffer of known capacity. */
inline void CopyTruncated(
  char * dst,
  const std::size_t cap,
  const char * src)
{
  CopyTruncated(dst, cap, src, std::strlen(src));
}


/** Bounded copy of a std::string into a fixed-size array. */
template <std::size_t N>
inline void CopyTruncated(char (&dst)[N], const std::string& src)
{
  CopyTruncated(dst, N, src.data(), src.size());
}

#endif
