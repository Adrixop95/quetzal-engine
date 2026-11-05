// Engine-local diagnostics, excluded from simulation hashes and save contracts.
#pragma once
#include <cstdint>
#include <ostream>
#include "engine/world/world.hpp"

namespace aztec::engine {
inline void write_combat_event(std::ostream& out, uint64_t tick, const char* event,
                               const Effect& e, uint32_t victim, int before, int after, const World& world) {
  const auto* v = world.find(victim);
  const int relation = v && e.side >= 0 && e.side < 8 && v->side >= 0 && v->side < 8
      ? int(world.relation[e.side][v->side]) : -1;
  out << "{\"tick\":" << tick << ",\"event\":\"" << event << "\",\"id\":" << e.trace_id
      << ",\"source\":" << e.source << ",\"target\":" << e.target << ",\"side\":" << e.side
      << ",\"kind\":" << int(e.kind) << ",\"x\":" << e.x << ",\"y\":" << e.y
      << ",\"height\":" << e.height << ",\"altitude\":" << e.altitude
      << ",\"victim_x\":" << (v ? v->cur_x : 0) << ",\"victim_y\":" << (v ? v->cur_y : 0)
      << ",\"victim_type\":" << (v ? v->type : -1) << ",\"victim_side\":" << (v ? v->side : -1)
      << ",\"relation\":" << relation << ",\"victim\":" << victim << ",\"before\":" << before << ",\"after\":" << after << "}\n";
}
}  // namespace aztec::engine
