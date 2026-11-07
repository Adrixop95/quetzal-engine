// Independent transport policies. Evidence and fallback choices: docs/transport_rules.md.
#include "engine/sim/game_sim.hpp"
#include <algorithm>
#include <tuple>

namespace aztec::engine {
namespace {
int tile(Fixed p) { return (p >> 8) / kTilePx; }
path::MoveClass land_class(const TypeRules& r) {
  return r.large ? path::MoveClass::Large : path::MoveClass::Small;
}
bool occupied(const World& w, const Object& passenger, int x, int y, const RulesData& rules) {
  for (const auto& other : w.objects) {
    if (!other.alive || other.removed || other.id == passenger.id || other.type < 0) continue;
    const auto& r = rules.type(other.type);
    if (r.flier || is_transport(other.type)) continue;
    int64_t dx = (other.cur_x >> 8) - (x * kTilePx + kTilePx / 2);
    int64_t dy = (other.cur_y >> 8) - (y * kTilePx + kTilePx / 2);
    int radius = r.radius_px + rules.type(passenger.type).radius_px + 4;
    if (dx * dx + dy * dy < int64_t(radius) * radius) return true;
  }
  return false;
}
}

void GameSim::unload(World& w, Object& vessel, int tx, int ty) {
  int bx = tile(vessel.cur_x), by = tile(vessel.cur_y);
  if (std::abs(tx - bx) > 3 || std::abs(ty - by) > 3) return;
  while (!vessel.cargo.empty()) {
    auto* passenger = w.find_mut(vessel.cargo.front());
    if (!passenger || !passenger->alive) { vessel.cargo.erase(vessel.cargo.begin()); continue; }
    const auto& r = rules_->type(passenger->type);
    std::vector<std::tuple<int, int, int>> candidates;
    for (int y = by - 2; y <= by + 2; ++y) for (int x = bx - 2; x <= bx + 2; ++x) {
      if (nav_.blocked(x, y, land_class(r))) continue;
      if (nav_.disc_hits({tile_center_fixed(x), tile_center_fixed(y)}, r.radius_px * 256, land_class(r))) continue;
      if (!occupied(w, *passenger, x, y, *rules_)) candidates.emplace_back((x-tx)*(x-tx)+(y-ty)*(y-ty), y, x);
    }
    if (candidates.empty()) break; // preserve cargo and its order when the shore is full
    std::sort(candidates.begin(), candidates.end());
    const auto [distance, y, x] = candidates.front(); (void)distance;
    passenger->carrier = 0; passenger->removed = false; passenger->boarding = 0;
    passenger->teleport(tile_center_fixed(x), tile_center_fixed(y));
    passenger->order = uint8_t(Order::Idle); passenger->target = 0; passenger->resume = 0;
    if (passenger->mover < 0) attach_agent(*passenger);
    else {
      auto& agent = mover_->agents_mut()[size_t(passenger->mover)];
      agent.enabled = true; agent.pos = agent.prev_pos = {passenger->cur_x, passenger->cur_y};
    }
    vessel.cargo.erase(vessel.cargo.begin());
  }
}

bool GameSim::activate_vessel(World& w, int scenario, ObjectSource source, bool enabled) {
  bool found = false;
  for (auto& o : w.objects) {
    if (!o.alive || o.removed || o.scenario != scenario || o.source != source) continue;
    found = true;
    o.vessel_enabled = enabled;
    if (!enabled) { script_stop(w, {o.id}); o.vessel_route = false; continue; }
    // Repeated enable/activate is idempotent during a route or after arrival.
    if (o.vessel_route || o.vessel_arrived) continue;
    if (source == ObjectSource::Submarine) {
      // V: the original dispatch starts animation 27 at the configured point and retires its controller.
      decoration(w, 27, 1, o.cur_x, o.cur_y, 0, o.side, o.id);
      o.vessel_arrived = true; o.removed = true;
    } else if (source == ObjectSource::Boat) {
      script_stop(w, {o.id}); o.vessel_route = true; o.transport_phase = 0;
      if (o.mover >= 0) mover_->agents_mut()[size_t(o.mover)].enabled = false;
    }
  }
  return found;
}

void GameSim::transport_tick(World& w) {
  // Scripted off-map entry uses the mission landing point; player sailing uses the shared mover.
  for (auto& boat : w.objects) {
    if (!boat.alive || boat.removed || !boat.vessel_route) continue;
    path::Vec2 goal{tile_center_fixed(boat.landing_x), tile_center_fixed(boat.landing_y)};
    path::Vec2 delta{goal.x - boat.cur_x, goal.y - boat.cur_y};
    constexpr int step = 5 * 256; // G timing fallback, original ferry visual step is five pixels
    auto velocity = path::len2(delta) <= int64_t(step)*step ? delta : path::with_length(delta, step);
    boat.cur_x += velocity.x; boat.cur_y += velocity.y;
    if (boat.cur_x == goal.x && boat.cur_y == goal.y) {
      boat.vessel_route = false; boat.vessel_arrived = true; boat.transport_phase = 2;
      if (boat.mover >= 0) {
        auto& a = mover_->agents_mut()[size_t(boat.mover)];
        a.enabled = true; a.pos = a.prev_pos = goal;
      }
      unload(w, boat, boat.landing_x, boat.landing_y);
    }
  }
  // Object IDs define simultaneous boarding order, independent of selection order.
  for (auto& o : w.objects) {
    if (o.carrier) {
      if (auto* boat = w.find(o.carrier)) o.teleport(boat->cur_x, boat->cur_y);
      continue;
    }
    if (!o.boarding || !o.alive || o.removed) continue;
    auto* boat = w.find_mut(o.boarding);
    if (!boat || !boat->alive || boat->removed || boat->side != o.side || !boat->vessel_enabled ||
        boat->cargo.size() >= kTransportCapacity) { script_stop(w, {o.id}); o.boarding = 0; continue; }
    if (boat->vessel_route) continue;
    int bx = tile(boat->cur_x), by = tile(boat->cur_y);
    auto cls = land_class(rules_->type(o.type));
    int best = -1, gx = 0, gy = 0;
    for (int y = by - 1; y <= by + 1; ++y) for (int x = bx - 1; x <= bx + 1; ++x) {
      if (nav_.blocked(x, y, cls)) continue;
      int d = (x-tile(o.cur_x))*(x-tile(o.cur_x))+(y-tile(o.cur_y))*(y-tile(o.cur_y));
      if (best >= 0 && d >= best) continue;
      auto route = path::astar(nav_, cls, {tile(o.cur_x), tile(o.cur_y)}, {x,y});
      if (!route.found) continue;
      best = d; gx = x; gy = y;
    }
    if (best < 0) continue; // no reachable shore yet; never teleport to a distant ferry
    int64_t dx = o.cur_x - tile_center_fixed(gx), dy = o.cur_y - tile_center_fixed(gy);
    if (dx*dx + dy*dy <= int64_t(20*256)*(20*256)) {
      script_stop(w, {o.id}); o.boarding = 0;
      o.carrier = boat->id; o.removed = true; boat->cargo.push_back(o.id);
      if (o.mover >= 0) mover_->agents_mut()[size_t(o.mover)].enabled = false;
      w.selection.ids.erase(std::remove(w.selection.ids.begin(), w.selection.ids.end(), o.id), w.selection.ids.end());
    } else if (o.mover >= 0 && (o.order != uint8_t(Order::Move) ||
               o.dest_x != gx*kTilePx+kTilePx/2 || o.dest_y != gy*kTilePx+kTilePx/2)) {
      script_order(w, {o.id}, gx*kTilePx+kTilePx/2, gy*kTilePx+kTilePx/2, false);
    }
  }
}
} // namespace aztec::engine
