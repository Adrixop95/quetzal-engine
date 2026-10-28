// Simulation adapter for the pure town planner; generated commands are replayed by recomputation.
#include <algorithm>
#include "engine/ai/planner.hpp"
#include "engine/sim/game_sim.hpp"

namespace aztec::engine {
void GameSim::run_ai(World& w) {
  for (size_t ti = 0; ti < w.mission.towns.size(); ++ti) {
    const auto town = w.mission.towns[ti];
    const auto side = std::find_if(w.mission.sides.begin(), w.mission.sides.end(),
                                   [&](const auto& sd) { return sd.index == town.side; });
    if (side == w.mission.sides.end() || !side->only_comp || side->neutral ||
        town.side < 0 || town.side >= int(w.side_state.size())) continue;
    const auto state = w.ai_towns[ti];
    int speed = ai::build_speed(w, state, int(ti));
    bool hostile_all = false, friendly = false;
    for (const auto& sd : w.mission.sides) if (sd.index != town.side) {
      hostile_all = true;
      if (!w.hostile(town.side, sd.index)) friendly = true;
    }
    hostile_all &= !friendly;
    std::vector<ai::Offer> offers;
    bool needs_offers = state.phase == 2 || state.phase == 3 || state.phase == 5 || state.phase == 7 ||
                        state.phase == 14 || state.phase == 15 || state.phase == 18 || state.phase == 22 || state.phase == 23;
    for (const auto& q : w.side_state[town.side].queue) if (q.town == int(ti)) needs_offers = false;
    for (const auto& o : w.objects)
      if (o.alive && !o.removed && o.town == int(ti) && o.progress < 1000) needs_offers = false;
    const auto items = needs_offers ? menu(w, town.side, int(ti)) : std::vector<MenuItem>{};
    for (const auto& item : items) {
      // Reprice with this tick's strategist speed before the BuildMode command is applied.
      bool available = item.enabled || std::string(item.why_not) == "no gold";
      int price = production_value(rules_->type(item.type).price, town.type, speed, true, hostile_all, false);
      if (!available || w.side_state[town.side].money < price) continue;
      ai::Offer offer; offer.type = item.type; offer.price = price;
      offer.placeable = !item.construct;
      if (item.construct) {
        // G: search eight tiles around the town, using resource bytes for economic placement.
        // Exact reference placement and mine/farm rankings remain unresolved.
        int64_t best = -1;
        const auto& h = rules_->type(item.type).harvest;
        for (int radius = 0; radius <= 8; ++radius)
          for (int y = town.y - radius; y <= town.y + radius; ++y)
            for (int x = town.x - radius; x <= town.x + radius; ++x) {
              if (std::max(std::abs(x - town.x), std::abs(y - town.y)) != radius ||
                  !can_place(w, town.side, item.type, x, y)) continue;
              int64_t score = 0;
              if (h.terrain) for (int cell = 0; cell < 49; ++cell) if (h.occupancy[cell] == 2) {
                int rx = x + cell % 7 - 3, ry = y + cell / 7 - 3;
                uint16_t resource = w.resources.in(rx, ry) ? w.resources.at(rx, ry) : 0;
                score += int64_t(resource & 255) * h.rate_a + int64_t(resource >> 8) * h.rate_b;
              }
              if (score > best) { best = score; offer.x = x; offer.y = y; offer.placeable = true; }
            }
      }
      offers.push_back(offer);
    }
    auto decision = ai::step(w, *rules_, int(ti), state, offers);
    w.ai_towns[ti] = std::move(decision.next);
    for (const auto& c : decision.commands) apply(w, c);
  }
}
}  // namespace aztec::engine
