// Clone-only simulation options. Helpers are disabled by default.
#pragma once
#include <cstdint>
#include <string>

namespace aztec::engine {
struct Modifications {
  bool parallel_construction = false;
  bool relaxed_placement = false;
  bool unit_unstick = false;
  uint32_t bits() const { return (parallel_construction ? 1u : 0u) | (relaxed_placement ? 2u : 0u) | (unit_unstick ? 4u : 0u); }
  static bool valid(uint32_t bits) { return (bits & ~7u) == 0; }
  static Modifications from_bits(uint32_t bits) { return {bool(bits & 1u), bool(bits & 2u), bool(bits & 4u)}; }
  bool set(const std::string& option) {
    if (option == "parallel_construction=on") parallel_construction = true;
    else if (option == "parallel_construction=off") parallel_construction = false;
    else if (option == "relaxed_placement=on") relaxed_placement = true;
    else if (option == "relaxed_placement=off") relaxed_placement = false;
    else if (option == "unit_unstick=on") unit_unstick = true;
    else if (option == "unit_unstick=off") unit_unstick = false;
    else return false;
    return true;
  }
};
}  // namespace aztec::engine
