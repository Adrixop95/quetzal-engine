// Internal budgets for untrusted files, independent of their declared sizes.
#pragma once
#include <cstddef>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
namespace aztec::detail {
constexpr size_t kFileBytes = 256 * 1024 * 1024;
constexpr size_t kTextBytes = 1024 * 1024;
constexpr size_t kPixels = 4 * 1024 * 1024;
constexpr size_t kCells = 16 * 1024 * 1024;
constexpr size_t kEntries = 16384;
inline int integer(const char* s, int fallback = 0) {
  errno = 0;
  char* end;
  long v = std::strtol(s, &end, 10);
  return end == s || errno == ERANGE || v < INT_MIN || v > INT_MAX ? fallback : int(v);
}
}
