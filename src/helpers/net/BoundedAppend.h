#pragma once

// appendf: printf-style append at out+len that never writes past out[n-1]. Returns the
// new length; once the text does not fit it returns n, and every later call returns n
// too, so a caller tests `len < n` once at the end instead of after every append.
// Replaces `len += snprintf(out + len, n - len, ...)`, where a len past n turns
// `n - len` into a huge size_t (CodeQL cpp/overflowing-snprintf).

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

static inline int appendf(char* out, size_t n, int len, const char* fmt, ...) {
  if (len < 0 || (size_t)len >= n) return (int)n;
  va_list ap;
  va_start(ap, fmt);
  int w = vsnprintf(out + len, n - (size_t)len, fmt, ap);
  va_end(ap);
  if (w < 0 || (size_t)len + (size_t)w >= n) return (int)n;
  return len + w;
}
