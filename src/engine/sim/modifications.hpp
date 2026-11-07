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
  // Keep training in bits 2–5; recovery has its own bit 6.
  bool unit_unstick = false;
  uint32_t bits() const {
    const uint32_t training = parallel_training == 0 ? 8u : uint32_t(parallel_training - 1);
    return (parallel_construction ? 1u : 0u) | (relaxed_placement ? 2u : 0u) | (training << 2) | (unit_unstick ? 64u : 0u);
  }
  static bool valid(uint32_t bits) { return ((bits & ~64u) >> 2) <= 8u; }
  static Modifications from_bits(uint32_t bits) {
    const uint32_t training = (bits & ~64u) >> 2;
    return {bool(bits & 1u), bool(bits & 2u), training == 8u ? 0 : int(training) + 1, bool(bits & 64u)};
  }
  std::string training_value() const { return parallel_training == 0 ? "per building" : std::to_string(parallel_training); }
  bool set(const std::string& option) {
    if (option == "parallel_construction=on") parallel_construction = true;
    else if (option == "parallel_construction=off") parallel_construction = false;
    else if (option == "relaxed_placement=on") relaxed_placement = true;
    else if (option == "relaxed_placement=off") relaxed_placement = false;
    else if (option == "unit_unstick=on") unit_unstick = true;
    else if (option == "unit_unstick=off") unit_unstick = false;
    else if (option == "parallel_training=off") parallel_training = 1;
    else if (option == "parallel_training=per building" || option == "parallel_training=per-building") parallel_training = 0;
    else if (option.size() == 19 && option.compare(0, 18, "parallel_training=") == 0 &&
             option.back() >= '1' && option.back() <= '8') parallel_training = option.back() - '0';
    else return false;
    return true;
  }
};
}  // namespace aztec::engine
