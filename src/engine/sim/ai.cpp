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
    if (side == w.mission.sides.end() || !side->only_comp ||
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
      if (o.alive && !o.removed && o.town == town.index && o.side == town.side && o.progress < 1000) needs_offers = false;
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
                  !automatic_site(w, town.side, item.type, x, y, int(ti))) continue;
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
    // With the global modification enabled, fill a training batch from the
    // planner's existing target. Preserve its composition and spending limits.
    if (modifications_.parallel_training != 1) {
      std::vector<Command> batch;
      for (const auto& c : decision.commands) {
        batch.push_back(c);
        if (c.type != Pct::Build || rules_->type(c.a).structure) continue;
        const int capacity = training_capacity(w, int(ti), training_producer(w, int(ti), c.a));
        int price = 0;
        for (const auto& offer : offers) if (offer.type == c.a) price = offer.price;
        const int affordable = price > 0 ? w.side_state[town.side].money / price : capacity;
        const int count = std::max(1, std::min({capacity, affordable, decision.next.remaining + 1, kProductionQueueCapacity}));
        for (int i = 1; i < count; ++i) batch.push_back(c);
        decision.next.remaining -= count - 1;
      }
      decision.commands = std::move(batch);
    }
    w.ai_towns[ti] = std::move(decision.next);
    for (const auto& c : decision.commands) {
      const size_t queued = w.side_state[town.side].queue.size();
      apply(w, c);
      if (!ai_observer_) continue;
      AiEvent event; event.side = town.side; event.town = int(ti);
      if (c.type == Pct::Build && w.side_state[town.side].queue.size() > queued) {
        event.kind = "build_order";
        const auto& q = w.side_state[town.side].queue.back();
        event.x = q.tile_x; event.y = q.tile_y;
        event.composition.at(size_t(q.type)) = 1;
      } else if (c.type == Pct::Attack || c.type == Pct::AttackMove) {
        event.kind = c.type == Pct::Attack ? "defence" : "launch";
        for (uint32_t id : c.ids) {
          const auto* o = w.find(id);
          if (!o || !o->alive || o->removed || o->side != c.side || o->type < 0 || o->type >= 90) continue;
          const bool accepted = c.type == Pct::Attack
              ? o->order == uint8_t(Order::Attack) && o->target == uint32_t(c.a)
              : o->order == uint8_t(Order::AttackMove) && o->dest_x == c.a && o->dest_y == c.b;
          if (accepted) ++event.composition[size_t(o->type)];
        }
        if (std::all_of(event.composition.begin(), event.composition.end(), [](int n) { return n == 0; })) continue;
      } else continue;
      ai_observer_(tick_, event);
    }
  }
}
}  // namespace aztec::engine
