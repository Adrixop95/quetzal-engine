// Clone-only simulation options. Zero bits preserve the default production policy.
#pragma once
#include <cstdint>
#include <string>

namespace aztec::engine {
struct Modifications {
  bool parallel_construction = false;
  uint32_t bits() const { return parallel_construction ? 1u : 0u; }
  static bool valid(uint32_t bits) { return (bits & ~1u) == 0; }
  static Modifications from_bits(uint32_t bits) { return {bool(bits & 1u)}; }
  bool set(const std::string& option) {
    if (option == "parallel_construction=on") parallel_construction = true;
    else if (option == "parallel_construction=off") parallel_construction = false;
    else return false;
    return true;
  }
};
}  // namespace aztec::engine
