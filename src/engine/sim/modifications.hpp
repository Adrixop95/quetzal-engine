// Clone-only simulation options. Zero bits preserve the original production policy.
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace aztec::engine {
enum class PlayProfile { Faithful, Enhanced };
struct Modifications {
  bool parallel_construction = false;
  bool relaxed_placement = false;
  // Zero selects completed matching producers; one preserves serial training.
  int parallel_training = 1;
  // Keep training in bits 2–5; recovery has its own bit 6.
  bool unit_unstick = false;
  PlayProfile profile = PlayProfile::Faithful;
  uint8_t overrides = 0; // preference metadata, excluded from simulation hashes
  static constexpr std::array<const char*, 4> names = {
    "parallel_construction", "parallel_training", "relaxed_placement", "unit_unstick"};

  static Modifications faithful() { return {}; }
  static Modifications enhanced() {
    Modifications m; m.parallel_construction = m.relaxed_placement = m.unit_unstick = true;
    m.parallel_training = 3; m.profile = PlayProfile::Enhanced; return m;
  }
  const char* profile_name() const { return profile == PlayProfile::Faithful ? "faithful" : "enhanced"; }
  bool set_profile(const std::string& name) {
    if (name == "faithful") *this = faithful();
    else if (name == "enhanced") *this = enhanced();
    else return false;
    return true;
  }
  unsigned value(size_t i) const {
    switch (i) {
      case 0: return parallel_construction;
      case 1: return parallel_training;
      case 2: return relaxed_placement;
      case 3: return unit_unstick;
      default: return 0;
    }
  }
  std::string setting(size_t i) const {
    return std::string(names[i]) + "=" + (i == 1 ? (parallel_training == 1 ? "off" : training_value()) : value(i) ? "on" : "off");
  }
  std::string training_value() const { return parallel_training == 0 ? "per building" : std::to_string(parallel_training); }
  uint32_t bits() const {
    const uint32_t training = parallel_training == 0 ? 8u : uint32_t(parallel_training - 1);
    return uint32_t(parallel_construction) | (uint32_t(relaxed_placement) << 1) |
           (training << 2) | (uint32_t(unit_unstick) << 6);
  }
  static bool valid(uint32_t bits) { return (bits & ~0x7fu) == 0 && ((bits & 0x3cu) >> 2) <= 8; }
  static Modifications from_bits(uint32_t bits) {
    Modifications m;
    m.parallel_construction = bits & 1u; m.relaxed_placement = bits & 2u; m.unit_unstick = bits & 64u;
    const auto training = (bits & 0x3cu) >> 2;
    m.parallel_training = training == 8u ? 0 : int(training) + 1; m.overrides = 15; return m;
  }
  bool set(const std::string& option) {
    for (size_t i = 0; i < names.size(); ++i) {
      const std::string prefix = std::string(names[i]) + "=";
      if (option.rfind(prefix, 0) != 0) continue;
      const auto text = option.substr(prefix.size());
      unsigned v = 0;
      if (text == "on") v = i == 1 ? 3 : 1;
      else if (text == "off") v = i == 1 ? 1 : 0;
      else if (i == 1 && (text == "per building" || text == "per-building")) v = 0;
      else {
        if (i != 1 || text.size() != 1) return false;
        for (char c : text) { if (c < '0' || c > '9') return false; v = v * 10 + unsigned(c - '0'); }
        if (v < 1 || v > 8) return false;
      }
      switch (i) {
        case 0: parallel_construction = v; break;
        case 1: parallel_training = int(v); break;
        case 2: relaxed_placement = v; break;
        case 3: unit_unstick = v; break;
      }
      overrides |= uint8_t(1u << i); return true;
    }
    return false;
  }
};
}  // namespace aztec::engine
