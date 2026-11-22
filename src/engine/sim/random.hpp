// Explicit CRT-compatible state: each simulation owns the game thread's stream.
#pragma once
#include <cstdint>

namespace aztec::engine {
constexpr uint32_t msvc_rand(uint32_t& state) {
  state = state * 214013u + 2531011u;
  return (state >> 16) & 0x7fffu;
}
}  // namespace aztec::engine
