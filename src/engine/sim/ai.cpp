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
    auto state = w.ai_towns[ti];
    // V: lost references become requests again; paid or queued work remains reserved.
    int outstanding = 0;
    for (size_t slot = 0; slot < state.plan.size(); ++slot) {
      auto& entry = state.plan[slot];
      const auto* unit = w.find(entry.object);
      if (entry.object && (!unit || !unit->alive || unit->removed || unit->side != town.side)) {
        entry.object = 0; entry.producer = -1;
      }
      if (!entry.object && entry.producer >= 0) {
        const bool queued = std::any_of(w.side_state[town.side].queue.begin(), w.side_state[town.side].queue.end(),
            [&](const auto& q) { return q.plan_town == int(ti) && q.plan_entry == int(slot) && q.plan_generation == state.plan_generation; });
        if (!queued) entry.producer = -1;
      }
      unit = w.find(entry.object);
      if (!unit || unit->progress < 1000) ++outstanding;
      if (unit) w.find_mut(unit->id)->tactic = -2 - int(ti);
    }
    if (!state.plan.empty()) state.remaining = outstanding;
    int speed = ai::build_speed(w, *rules_, state, int(ti));
    const bool hostile_all = w.enemy_all(town.side);
    std::vector<ai::Offer> offers;
    bool needs_offers = state.phase == 2 || state.phase == 3 || state.phase == 5 || state.phase == 7 ||
                        state.phase == 14 || state.phase == 15 || state.phase == 18 || state.phase == 22 || state.phase == 23;
    bool production_busy = false;
    for (const auto& q : w.side_state[town.side].queue) if (q.town == int(ti)) production_busy = true;
    for (const auto& o : w.objects)
      if (o.alive && !o.removed && o.town == town.index && o.side == town.side && o.progress < 1000) production_busy = true;
    if (production_busy) needs_offers = false;
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
    // V: progressing a plan can feed any idle side town in phases 0/2/14/22.
    // Requests are reserved without testing cash; payment belongs to production start.
    if (!production_busy && (state.phase == 2 || state.phase == 14 || state.phase == 22) && !decision.next.plan.empty()) {
      for (size_t producer = 0; producer < w.mission.towns.size(); ++producer) {
        const auto& td = w.mission.towns[producer];
        if (td.side != town.side) continue;
        const int phase = producer == ti ? decision.next.phase : w.ai_towns[producer].phase;
        if (phase != 0 && phase != 2 && phase != 14 && phase != 22) continue;
        bool busy = false;
        uint32_t hall = 0;
        for (const auto& o : w.objects) if (o.alive && !o.removed && o.side == td.side && o.town == td.index) {
          busy |= o.progress < 1000;
          if (o.town_hall) hall = o.id;
        }
        for (const auto& q : w.side_state[town.side].queue) busy |= q.town == int(producer);
        for (const auto& c : decision.commands) if (c.type == Pct::Build && producing_town(w, c.side, c.ids) == int(producer)) busy = true;
        if (busy || !hall) continue;
        const auto items = menu(w, town.side, int(producer));
        for (auto entry = decision.next.plan.rbegin(); entry != decision.next.plan.rend(); ++entry) {
          if (entry->object || entry->producer >= 0) continue;
          const auto item = std::find_if(items.begin(), items.end(), [&](const auto& m) {
            return m.type == entry->type && !m.construct && (m.enabled || std::string(m.why_not) == "no gold");
          });
          if (item == items.end()) continue;
          Command c; c.type = Pct::Build; c.side = uint8_t(town.side); c.a = entry->type; c.ids = {hall};
          decision.commands.push_back(c);
          entry->producer = int(producer);
          break;
        }
      }
    }
    // Keep production town separate from the temporary plan tactic.
    for (const auto& entry : state.plan) if (entry.object &&
        std::none_of(decision.next.plan.begin(), decision.next.plan.end(), [&](const auto& e) { return e.object == entry.object; })) {
      if (auto* o = w.find_mut(entry.object)) o->tactic = -1;
    }
    for (const auto& entry : decision.next.plan) if (auto* o = w.find_mut(entry.object)) o->tactic = -2 - int(ti);
    // With the global modification enabled, fill a training batch from the
    // planner's existing target. Preserve its composition and spending limits.
    if (modifications_.parallel_training != 1) {
      std::vector<Command> batch;
      for (const auto& c : decision.commands) {
        batch.push_back(c);
        if (c.type != Pct::Build || rules_->type(c.a).structure) continue;
        const int producer = producing_town(w, c.side, c.ids);
        if (producer < 0) continue;
        const int capacity = training_capacity(w, producer, training_producer(w, producer, c.a));
        int price = 0;
        for (const auto& offer : offers) if (offer.type == c.a) price = offer.price;
        const int affordable = price > 0 ? w.side_state[town.side].money / price : capacity;
        int unassigned = 0;
        for (const auto& entry : decision.next.plan)
          if (entry.type == c.a && !entry.object && entry.producer < 0) ++unassigned;
        const int count = std::max(1, std::min({capacity, affordable, unassigned + 1, kProductionQueueCapacity}));
        int added = 0;
        for (auto& entry : decision.next.plan) {
          if (added == count - 1) break;
          if (entry.type != c.a || entry.object || entry.producer >= 0) continue;
          entry.producer = producer;
          batch.push_back(c); ++added;
        }
      }
      decision.commands = std::move(batch);
    }
    w.ai_towns[ti] = std::move(decision.next);
    for (const auto& c : decision.commands) {
      const size_t queued = w.side_state[town.side].queue.size();
      apply(w, c);
      if (c.type == Pct::Build && w.side_state[town.side].queue.size() > queued) {
        auto& q = w.side_state[town.side].queue.back();
        const auto& plan = w.ai_towns[ti].plan;
        for (size_t slot = 0; slot < plan.size(); ++slot) {
          if (plan[slot].type != q.type || plan[slot].producer != q.town || plan[slot].object) continue;
          const bool reserved = std::any_of(w.side_state[town.side].queue.begin(), w.side_state[town.side].queue.end(),
              [&](const auto& prior) { return prior.plan_town == int(ti) && prior.plan_entry == int(slot) && prior.plan_generation == w.ai_towns[ti].plan_generation; });
          if (reserved) continue;
          q.plan_town = int(ti); q.plan_entry = int(slot); q.plan_generation = w.ai_towns[ti].plan_generation;
          break;
        }
      }
      if (!ai_observer_) continue;
      AiEvent event; event.side = town.side; event.town = int(ti);
      if (c.type == Pct::Build && w.side_state[town.side].queue.size() > queued) {
        event.kind = "build_order";
        const auto& q = w.side_state[town.side].queue.back();
        event.town = q.town; event.x = q.tile_x; event.y = q.tile_y;
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
