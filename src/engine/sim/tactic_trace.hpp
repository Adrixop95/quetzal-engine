// Engine diagnostics only: native identities and engine order codes need an
// audited adapter before comparison with original-game observations.
#pragma once
#include <ostream>
#include "engine/sim/rules.hpp"
#include "engine/world/visibility.hpp"

namespace aztec::engine {
inline void write_tactic_snapshot(std::ostream& out, uint64_t tick, const World& w, const RulesData& rules) {
  auto ids = [&](const auto& values) {
    out << '[';
    bool first = true;
    for (auto id : values) { if (!first) out << ','; first = false; out << id; }
    out << ']';
  };
  out << "{\"tick\":" << tick << ",\"objects\":{";
  bool first = true;
  for (const auto& o : w.objects) {
    if (o.type < 0 || o.removed) continue;
    if (!first) out << ',';
    first = false;
    out << '"' << o.id << "\":{\"type\":" << o.type << ",\"side\":" << o.side
        << ",\"hp\":" << o.hp << ",\"hp_max\":" << o.hp_max << ",\"alive\":" << int(o.alive)
        << ",\"x\":" << o.cur_x << ",\"y\":" << o.cur_y << ",\"dest_x\":" << o.dest_x << ",\"dest_y\":" << o.dest_y
        << ",\"order\":" << int(o.order) << ",\"target\":" << o.target << ",\"tactic\":" << o.tactic
        << ",\"follow\":" << o.follow_tactic << ",\"mode\":" << int(o.attack_mode)
        << ",\"resume\":" << int(o.resume) << ",\"cooldown\":" << o.cooldown << ",\"visible\":[";
    for (size_t side = 0; side < w.side_state.size(); ++side) {
      if (side) out << ',';
      out << int(object_visible(w, int(side), o));
    }
    out << "]}";
  }
  out << "},\"sides\":[";
  for (size_t side = 0; side < w.side_state.size(); ++side) {
    if (side) out << ',';
    const auto& s = w.side_state[side];
    int units = 0, structures = 0;
    for (const auto& o : w.objects) if (o.alive && !o.removed && o.type >= 0 && o.side == int(side)) {
      if (rules.type(o.type).structure) ++structures;
      else ++units;
    }
    out << "{\"money\":" << s.money << ",\"units\":" << units << ",\"structures\":" << structures
        << ",\"selected_tactic\":" << s.selected_tactic << ",\"selected\":";
    ids(s.selected_ids);
    out << ",\"groups\":[";
    for (int g = 0; g < 10; ++g) { if (g) out << ','; ids(s.groups[g]); }
    out << "],\"queue\":[";
    bool first_item = true;
    for (const auto& q : s.queue) {
      if (!first_item) out << ',';
      first_item = false;
      out << '[' << q.type << ',' << q.town << ',' << q.remaining << ',' << q.total << ',' << q.paid
          << ',' << int(q.started) << ',' << int(q.paused) << ',' << q.object << ']';
    }
    out << "]}";
  }
  out << "]}\n";
}
}  // namespace aztec::engine
