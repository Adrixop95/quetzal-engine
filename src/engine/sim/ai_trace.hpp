// Semantic diagnostics only; no observer state participates in simulation hashes.
#pragma once
#include <array>
#include <cstdint>
#include <ostream>

namespace aztec::engine {
struct AiEvent {
  const char* kind = "";
  int side = -1, town = -1;
  // Placement is in tiles; -1 means this event has no placement.
  int x = -1, y = -1;
  std::array<int, 90> composition{};
};

inline void write_ai_event(std::ostream& out, uint64_t tick, const AiEvent& e) {
  out << "{\"tick\":" << tick << ",\"event\":\"" << e.kind << "\",\"side\":" << e.side
      << ",\"town\":" << e.town << ",\"x\":" << e.x << ",\"y\":" << e.y << ",\"composition\":[";
  bool first = true;
  for (size_t type = 0; type < e.composition.size(); ++type) if (e.composition[type]) {
    if (!first) out << ',';
    first = false;
    out << '[' << type << ',' << e.composition[type] << ']';
  }
  out << "]}\n";
}
}  // namespace aztec::engine
