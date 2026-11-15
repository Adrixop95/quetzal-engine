// Independently implemented shuttle policies; evidence: docs/ferry_shuttle.md.
#include "engine/sim/game_sim.hpp"
#include "engine/data/animation_slots.hpp"
#include <algorithm>
#include <limits>
#include <tuple>

namespace aztec::engine {
namespace {
int tile(Fixed p) {
  // Floor division matters for scenario entries outside the map.
  const int pixels = p / 256;
  return pixels >= 0 ? pixels / kTilePx : (pixels - kTilePx + 1) / kTilePx;
}
path::MoveClass land_class(const TypeRules& r) {
  return r.large ? path::MoveClass::Large : path::MoveClass::Small;
}
int anchor_distance(int x, int y) {
  const int major = std::max(std::abs(x), std::abs(y));
  const int minor = std::min(std::abs(x), std::abs(y));
  const int scaled = minor + minor / 2;
  return major + scaled / 4 + scaled / 64 - major / 128 - major / 32;
}
void phase(Object& boat, int controller, data::BoatPhase animation, int wait) {
  boat.ferry_phase = controller; boat.ferry_wait = wait;
  boat.transport_phase = int(animation); boat.anim_tick = 0;
  boat.vessel_route = controller != 0;
}
bool occupied(const World& w, const Object& passenger, Fixed x, Fixed y, const RulesData& rules) {
  for (const auto& other : w.objects) {
    if (!other.alive || other.removed || other.id == passenger.id || other.type < 0) continue;
    const auto& r = rules.type(other.type);
    if (r.flier || is_transport(other.type)) continue;
    const int64_t dx = (other.cur_x - x) / 256, dy = (other.cur_y - y) / 256;
    const int radius = r.radius_px + rules.type(passenger.type).radius_px + 4;
    if (dx * dx + dy * dy < int64_t(radius) * radius) return true;
  }
  return false;
}
}

void GameSim::ferry_follow(World& w, const std::vector<uint32_t>& ids, int side, Object& boat) {
  if (!boat.alive || boat.removed || !is_ferry(boat) || side < 0 || side >= 8 ||
      boat.side < 0 || boat.side >= 8 || (side != boat.side && w.relation[side][boat.side] != kFriend)) return;
  std::vector<uint32_t> members;
  int group = -1;
  bool same_group = true;
  for (auto id : ids) {
    const auto* o = w.find(id);
    if (!o || !o->alive || o->removed || o->side != side || o->type < 0 || is_transport(o->type)) continue;
    const auto& r = rules_->type(o->type);
    if (!(r.mobile || r.flier) || r.structure) continue;
    if (!members.empty() && o->tactic != group) same_group = false;
    if (members.empty()) group = o->tactic;
    if (std::find(members.begin(), members.end(), id) == members.end()) members.push_back(id);
  }
  if (members.empty()) return;
  // A town controller accepts the follow fields but does not execute field-group movement.
  for (auto id : members) if (w.find(id)->town >= 0) return;
  if (!same_group || group < 0) {
    int64_t next = 0;
    for (const auto& o : w.objects) next = std::max(next, int64_t(o.tactic) + 1);
    if (next > std::numeric_limits<int32_t>::max()) return;
    group = int(next);
    for (auto id : members) w.find_mut(id)->tactic = group;
  } else {
    // The order is issued to a tactic, including its unselected members.
    members.clear();
    for (const auto& o : w.objects)
      if (o.alive && !o.removed && o.side == side && o.tactic == group && !is_transport(o.type)) members.push_back(o.id);
  }
  for (auto id : members) {
    auto* o = w.find_mut(id);
    script_stop(w, {id});
    o->boarding = boat.id;
    o->order = uint8_t(Order::Follow);
  }
}

void GameSim::unload(World& w, Object& boat, int, int) {
  // Fitted first rings reproduce the measured 48..60 px shore spacing. Further
  // rings are bounded by the map; blocked passengers remain aboard for a retry.
  const int limit = (w.terrain.w + w.terrain.h + 2) * kTilePx;
  for (int radius = 48; radius <= limit && !boat.cargo.empty(); radius += radius == 48 ? 12 : 48) {
    const int a = radius * 3 / 5, b = radius * 4 / 5;
    const std::pair<int, int> offsets[] = {
      {0,-radius}, {a,-b}, {b,-a}, {radius,0}, {b,a}, {a,b},
      {0,radius}, {-a,b}, {-b,a}, {-radius,0}, {-b,-a}, {-a,-b}
    };
    for (auto [dx, dy] : offsets) {
      const Fixed x = boat.cur_x + to_fixed(dx), y = boat.cur_y + to_fixed(dy);
      for (auto it = boat.cargo.begin(); it != boat.cargo.end();) {
        auto* passenger = w.find_mut(*it);
        if (!passenger || !passenger->alive) { it = boat.cargo.erase(it); continue; }
        const auto& r = rules_->type(passenger->type);
        const auto cls = land_class(r);
        if (nav_.blocked(tile(x), tile(y), cls) || nav_.disc_hits({x,y}, r.radius_px * 256, cls) ||
            occupied(w, *passenger, x, y, *rules_)) { ++it; continue; }
        passenger->carrier = 0; passenger->removed = false; passenger->boarding = 0;
        passenger->tactic = boat.ferry_destination;
        passenger->teleport(x,y);
        passenger->order = uint8_t(Order::Idle); passenger->target = 0; passenger->resume = 0;
        passenger->follow_tactic = -1;
        if (passenger->mover < 0) attach_agent(*passenger);
        if (passenger->mover >= 0) {
          auto& agent = mover_->agents_mut()[size_t(passenger->mover)];
          agent.enabled = true; agent.pos = agent.prev_pos = {x,y};
        }
        boat.cargo.erase(it);
        break; // One occupant per placement.
      }
    }
  }
}

bool GameSim::activate_vessel(World& w, int scenario, ObjectSource source, bool enabled) {
  bool found = false;
  for (auto& o : w.objects) {
    if (!o.alive || o.removed || o.scenario != scenario || o.source != source) continue;
    found = true; o.vessel_enabled = enabled;
    if (source == ObjectSource::Submarine) {
      if (!enabled || o.vessel_arrived) continue;
      decoration(w, 27, 1, o.cur_x, o.cur_y, 0, o.side, o.id);
      o.vessel_arrived = true; o.removed = true;
    } else if (source == ObjectSource::Boat && enabled && o.ferry_phase == 0) {
      o.ferry_request = -1;
      o.ferry_destination = o.cargo.empty() ? -1 : w.find(o.cargo.front())->tactic;
      o.ferry_group_side = o.side;
      phase(o, 3, data::BoatPhase::Closed, kFerryTravelPause);
    } else if (source == ObjectSource::Johnka && !enabled) script_stop(w, {o.id});
  }
  return found;
}

void GameSim::transport_tick(World& w) {
  // Follow is generic: approach the allied target anchor, then ask a ready ferry.
  // Group anchors are currently centroids, a documented engine approximation.
  for (auto& o : w.objects) {
    if (!o.boarding || !o.alive || o.removed) continue;
    auto* boat = w.find_mut(o.boarding);
    if (!boat || !boat->alive || boat->removed || !is_ferry(*boat) ||
        (o.side != boat->side && w.relation[o.side][boat->side] != kFriend)) {
      script_stop(w, {o.id}); continue;
    }
    int64_t sx = 0, sy = 0, count = 0;
    bool airborne = true;
    for (const auto& member : w.objects) {
      if (!member.alive || member.removed || member.side != o.side || member.tactic != o.tactic) continue;
      sx += member.cur_x; sy += member.cur_y; ++count;
      airborne = airborne && rules_->type(member.type).flier;
    }
    if (!count) continue;
    if (anchor_distance(tile(Fixed(sx/count)) - tile(boat->cur_x),
                        tile(Fixed(sy/count)) - tile(boat->cur_y)) < ferry_reach(o.attack_mode)) {
      if (airborne) { script_stop(w, {o.id}); continue; }
      if (boat->ferry_phase == 0) {
        boat->ferry_request = boat->ferry_destination = o.tactic;
        boat->ferry_group_side = o.side;
        phase(*boat, 1, data::BoatPhase::Opening, kFerryOpeningTicks);
      }
    }
    // Resolve the anchor to a reachable land cell within the boarding block.
    // This is shore routing, independent of whether the group sent its request.
    if ((tick_ + o.id) % 6 != 0 && o.order != uint8_t(Order::Idle)) continue;
    int gx = tile(boat->cur_x), gy = tile(boat->cur_y), best = std::numeric_limits<int>::max();
    if (!rules_->type(o.type).flier) {
      bool found = false;
      const auto cls = land_class(rules_->type(o.type));
      for (int x = tile(boat->cur_x)-1; x <= tile(boat->cur_x)+1; ++x)
        for (int y = tile(boat->cur_y)-1; y <= tile(boat->cur_y)+1; ++y) {
          if (nav_.blocked(x,y,cls) || nav_.disc_hits({tile_center_fixed(x),tile_center_fixed(y)},
                                                   rules_->type(o.type).radius_px*256,cls)) continue;
          const int dx=x-tile(o.cur_x), dy=y-tile(o.cur_y), score=dx*dx+dy*dy;
          if (score >= best) continue;
          const auto route = path::astar(nav_,cls,{tile(o.cur_x),tile(o.cur_y)},{x,y});
          if (!route.found) continue;
          gx=x; gy=y; best=score; found=true;
        }
      if (!found) {
        // Follow still approaches the nearest reachable shore when no member
        // can enter the boarding block; the water destination is resolved by
        // the shared mover and never confers boarding eligibility.
        gx = tile(boat->cur_x); gy = tile(boat->cur_y);
      }
    }
    const int dx=gx*kTilePx+kTilePx/2, dy=gy*kTilePx+kTilePx/2;
    if (o.dest_x != dx || o.dest_y != dy || o.order == uint8_t(Order::Idle) ||
        (o.mover >= 0 && mover_->agents()[size_t(o.mover)].state != path::AgentState::Moving)) {
      script_order(w, {o.id}, dx, dy, false);
      o.order = uint8_t(Order::Follow);
    }
  }
  for (auto& boat : w.objects) {
    if (!is_ferry(boat) || !boat.alive || boat.removed) continue;
    if (tile(boat.cur_x) < -7 || tile(boat.cur_y) < -7 ||
        tile(boat.cur_x) >= w.terrain.w + 7 || tile(boat.cur_y) >= w.terrain.h + 7) {
      script_kill(w, boat.id); continue;
    }
    if (boat.ferry_phase == 0) continue;
    if (boat.ferry_wait > 0) { --boat.ferry_wait; continue; }
    switch (boat.ferry_phase) {
      case 1: {
        boat.transport_phase = int(data::BoatPhase::Open);
        // Occupancy grid semantics: x then y, at most one live ground object per
        // tile. Stable ID breaks overlaps in the engine's continuous mover (G).
        const int bx = tile(boat.cur_x), by = tile(boat.cur_y);
        for (int x = bx-1; x <= bx+1 && boat.cargo.size() < kTransportCapacity; ++x)
          for (int y = by-1; y <= by+1 && boat.cargo.size() < kTransportCapacity; ++y) {
            Object* occupant = nullptr;
            for (auto& candidate : w.objects) {
              if (!candidate.alive || candidate.removed || candidate.type < 0 || is_transport(candidate.type) ||
                  rules_->type(candidate.type).flier || tile(candidate.cur_x) != x || tile(candidate.cur_y) != y) continue;
              occupant = &candidate; break;
            }
            if (!occupant || occupant->tactic != boat.ferry_request || occupant->side != boat.ferry_group_side) continue;
            script_stop(w, {occupant->id});
            occupant->carrier = boat.id; occupant->removed = true;
            boat.cargo.push_back(occupant->id);
            if (occupant->mover >= 0) mover_->agents_mut()[size_t(occupant->mover)].enabled = false;
            auto erase_id = [&](auto& ids) { ids.erase(std::remove(ids.begin(), ids.end(), occupant->id), ids.end()); };
            erase_id(w.selection.ids);
            for (auto& side : w.side_state) erase_id(side.selected_ids);
          }
        bool remaining = false;
        for (const auto& member : w.objects)
          if (member.alive && !member.removed && member.side == boat.ferry_group_side && member.tactic == boat.ferry_request &&
              !rules_->type(member.type).flier && !is_transport(member.type)) remaining = true;
        if (boat.cargo.size() == kTransportCapacity || (!boat.cargo.empty() && !remaining)) {
          for (auto& member : w.objects) if (member.boarding == boat.id) script_stop(w, {member.id});
          boat.ferry_request = -1;
          phase(boat, 2, data::BoatPhase::Closing, kFerryClosingTicks);
        }
        break;
      }
      case 2: phase(boat, 3, data::BoatPhase::Closed, kFerryTravelPause); break;
      case 3:
      case 5: {
        const int sign = boat.ferry_direction;
        const Fixed nx = boat.cur_x + sign * to_fixed(5), ny = boat.cur_y - sign * to_fixed(5);
        const bool returning = boat.ferry_phase == 5;
        if (returning && boat.transport_phase == int(data::BoatPhase::Turning)) {
          boat.transport_phase = int(data::BoatPhase::Closed); boat.anim_tick = 0;
        }
        if (returning && std::abs(boat.cur_x - boat.ferry_dock_x) <= to_fixed(5) &&
                         std::abs(boat.cur_y - boat.ferry_dock_y) <= to_fixed(5)) {
          boat.cur_x = boat.ferry_dock_x; boat.cur_y = boat.ferry_dock_y;
          boat.orientation ^= 1; boat.ferry_direction = -boat.ferry_direction;
          boat.ferry_destination = boat.ferry_group_side = -1;
          phase(boat, 0, data::BoatPhase::Closed, 0);
        } else if (!nav_.in_bounds(tile(nx), tile(ny)) && !returning) {
          // Scenario entries may start outside the map. Preserve the diagonal
          // until entry, but never leave the map on an outbound trip.
          if (!nav_.in_bounds(tile(boat.cur_x), tile(boat.cur_y))) { boat.cur_x = nx; boat.cur_y = ny; }
          else phase(boat, 4, data::BoatPhase::Opening, kFerryOpeningTicks);
        } else if (!returning && nav_.blocked(tile(nx), tile(ny), path::MoveClass::Water)) {
          boat.vessel_arrived = true;
          phase(boat, 4, data::BoatPhase::Opening, kFerryOpeningTicks);
        } else if (returning && nav_.in_bounds(tile(nx), tile(ny)) &&
                   nav_.blocked(tile(nx), tile(ny), path::MoveClass::Water)) {
          // G: a newly obstructed return route waits and retries.
        } else { boat.cur_x = nx; boat.cur_y = ny; }
        break;
      }
      case 4:
        unload(w, boat, tile(boat.cur_x), tile(boat.cur_y));
        if (boat.cargo.empty()) {
          boat.orientation ^= 1; boat.ferry_direction = -boat.ferry_direction;
          phase(boat, 5, data::BoatPhase::Turning, kFerryTravelPause);
        }
        break;
      default: break;
    }
  }
  for (auto& passenger : w.objects) if (passenger.carrier)
    if (const auto* boat = w.find(passenger.carrier)) passenger.teleport(boat->cur_x, boat->cur_y);
}
} // namespace aztec::engine
