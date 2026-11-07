// Clone-only simulation options. Helpers are disabled by default.
#pragma once
#include <cstdint>
#include <string>

namespace aztec::engine {
struct Modifications {
  bool parallel_construction = false;
  bool relaxed_placement = false;
  // Zero selects completed matching producers; 1 preserves original serial training.
  int parallel_training = 1;
  uint32_t bits() const {
    const uint32_t training = parallel_training == 0 ? 8u : uint32_t(parallel_training - 1);
    return (parallel_construction ? 1u : 0u) | (relaxed_placement ? 2u : 0u) | (training << 2);
  }
  static bool valid(uint32_t bits) { return (bits >> 2) <= 8u; }
  static Modifications from_bits(uint32_t bits) {
    return {bool(bits & 1u), bool(bits & 2u), (bits >> 2) == 8u ? 0 : int(bits >> 2) + 1};
  }
  std::string training_value() const { return parallel_training == 0 ? "per building" : std::to_string(parallel_training); }
  bool set(const std::string& option) {
    if (option == "parallel_construction=on") parallel_construction = true;
    else if (option == "parallel_construction=off") parallel_construction = false;
    else if (option == "relaxed_placement=on") relaxed_placement = true;
    else if (option == "relaxed_placement=off") relaxed_placement = false;
    else if (option == "parallel_training=off") parallel_training = 1;
    else if (option == "parallel_training=per building" || option == "parallel_training=per-building") parallel_training = 0;
    else if (option.size() == 19 && option.compare(0, 18, "parallel_training=") == 0 &&
             option.back() >= '1' && option.back() <= '8') parallel_training = option.back() - '0';
    else return false;
    return true;
  }
};
}  // namespace aztec::engine
