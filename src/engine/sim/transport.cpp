// Independent transport behavior; evidence: docs/ferry_implementation.md.
#include "engine/sim/game_sim.hpp"
#include "engine/sim/sight.hpp"
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
void boat_state(Object& boat, int state) {
  boat.boat_state = state;
  const auto animation = state == 2 ? data::BoatPhase::Opening :
      state == 3 ? data::BoatPhase::Open : state == 4 ? data::BoatPhase::Closing :
      state == 6 ? data::BoatPhase::Turning : data::BoatPhase::Closed;
  boat.transport_phase = int(animation); boat.anim_tick = 0;
  boat.ferry_wait = state == 2 ? kFerryOpeningTicks : state == 4 ? kFerryClosingTicks :
      state == 6 ? kFerryTravelPause : 0;
}
void boat_mode(Object& boat, int mode) {
  if (mode == 3) boat_state(boat, 5);
  else if (mode == 0 && boat.boat_state == 1) boat_state(boat, 2);
  else if (mode == 1 && boat.boat_state == 3) boat_state(boat, 4);
  else if (mode == 2 && boat.boat_state == 1) boat_state(boat, 6);
}
int fresh_group(const World& w) {
  int64_t next = 0;
  for (const auto& o : w.objects) {
    next = std::max(next, int64_t(o.tactic) + 1);
    next = std::max(next, int64_t(o.ferry_destination) + 1);
  }
  return next <= std::numeric_limits<int32_t>::max() ? int(next) : -1;
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
    group = fresh_group(w);
    if (group < 0) return;
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
  if (boat.ferry_destination < 0) boat.ferry_destination = fresh_group(w);
  if (boat.ferry_destination < 0) return;
  // V: square tile rings, top row, bottom row, left column, right column.
  // G: bound the search and retry instead of hanging on an entirely blocked map.
  const int bx = tile(boat.cur_x), by = tile(boat.cur_y);
  const int limit = std::max(w.terrain.w, w.terrain.h) + 8;
  for (int radius = 1; radius <= limit && !boat.cargo.empty(); ++radius) {
    std::vector<std::pair<int,int>> cells;
    for (int x = bx-radius; x <= bx+radius; ++x) cells.emplace_back(x, by-radius);
    for (int x = bx-radius; x <= bx+radius; ++x) cells.emplace_back(x, by+radius);
    for (int y = by-radius; y <= by+radius; ++y) cells.emplace_back(bx-radius, y);
    for (int y = by-radius; y <= by+radius; ++y) cells.emplace_back(bx+radius, y);
    for (auto [tx, ty] : cells) {
      const Fixed x = tile_center_fixed(tx), y = tile_center_fixed(ty);
      for (auto it = boat.cargo.begin(); it != boat.cargo.end();) {
        auto* passenger = w.find_mut(*it);
        if (!passenger || !passenger->alive) { it = boat.cargo.erase(it); continue; }
        const auto& r = rules_->type(passenger->type);
        const auto cls = land_class(r);
        if (nav_.blocked(tile(x), tile(y), cls) || nav_.disc_hits({x,y}, r.radius_px * 256, cls) ||
            occupied(w, *passenger, x, y, *rules_)) { ++it; continue; }
        passenger->carrier = 0; passenger->removed = false; passenger->boarding = 0;
        passenger->tactic = boat.ferry_destination;
        if (boat.ferry_group_side >= 0) passenger->side = boat.ferry_group_side;
        passenger->attack_mode = AttackMode::Balanced;
        passenger->town = -1; passenger->defence = false;
        passenger->teleport(x,y);
        passenger->order = uint8_t(Order::Idle); passenger->target = 0; passenger->resume = 0;
        passenger->follow_tactic = -1;
        passenger->idle_attack = false;
        passenger->idle_anchor_x = passenger->cur_x / 256; passenger->idle_anchor_y = passenger->cur_y / 256;
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
  uint32_t last = 0;
  if (source != ObjectSource::Johnka)
    for (const auto& o : w.objects)
      if (o.alive && !o.removed && o.scenario == scenario && o.source == source) last = o.id;
  for (auto& o : w.objects) {
    if (source != ObjectSource::Johnka && o.id != last) continue;
    if (!o.alive || (o.removed && source != ObjectSource::Johnka) ||
        o.scenario != scenario || o.source != source) continue;
    found = true; o.vessel_enabled = enabled;
    if (source == ObjectSource::Johnka) {
      // V: disabling waits for the next turn; enabling starts a dormant patrol.
      if (enabled && !o.vessel_route) {
        o.removed = false; o.vessel_route = true; o.transport_phase = 1;
        o.teleport(tile_center_fixed(tile(o.cur_x)), tile_center_fixed(tile(o.cur_y)));
      }
      continue;
    }
    if (source == ObjectSource::Submarine) {
      if (!enabled || o.vessel_arrived) continue;
      std::erase_if(w.effects, [&](const Effect& e) { return e.anim == 27 && e.source == o.id && e.loop; });
      decoration(w, 27, 1, o.cur_x, o.cur_y, 0, o.side, o.id);
      o.vessel_arrived = true; o.removed = true;
    } else if (source == ObjectSource::Boat && enabled) {
      // V: activation overrides every phase and travels in the current direction.
      o.ferry_phase = 3; o.vessel_route = true;
      if (o.ferry_group_side < 0) o.ferry_group_side = o.side;
      boat_mode(o, 3);
    }
  }
  return found;
}

void GameSim::transport_tick(World& w) {
  for (auto& vessel : w.objects) {
    if (vessel.source != ObjectSource::Johnka || !vessel.alive || !vessel.vessel_route) continue;
    if (vessel.transport_phase == 1 && !vessel.vessel_enabled) {
      vessel.vessel_route = false; vessel.removed = true;
      vessel.target = 0; vessel.order = uint8_t(Order::Idle);
      continue;
    }
    vessel.transport_phase = 2;
    // V: five pixels on each axis per visual think, independent of terrain.
    // G: one visual think per engine tick; native scheduling needs a live run.
    const int step = (vessel.orientation ? -5 : 5) * 256;
    vessel.cur_x += step; vessel.cur_y += step;
    const int x = vessel.cur_x / 256, y = vessel.cur_y / 256;
    const int tx = x / kTilePx, ty = y / kTilePx;
    if (std::abs(tx * kTilePx + kTilePx / 2 - x) < 5 &&
        (tx < -3 || tx >= w.terrain.w + 3 || ty < -3 || ty >= w.terrain.h + 3)) {
      vessel.orientation ^= 1;
      vessel.transport_phase = 1;
    }
  }
  for (auto& boat : w.objects) {
    if (!is_ferry(boat)) continue;
    if (!boat.alive || boat.removed || tile(boat.cur_x) < -7 || tile(boat.cur_y) < -7 ||
        tile(boat.cur_x) >= w.terrain.w + 7 || tile(boat.cur_y) >= w.terrain.h + 7) {
      // A missing leader kills hidden members too and cancels every follower.
      const auto cargo = boat.cargo;
      for (auto id : cargo) script_kill(w, id);
      boat.cargo.clear();
      script_kill(w, boat.id); boat.boat_state = 0; boat.vessel_route = false;
      for (auto& follower : w.objects) if (follower.boarding == boat.id) script_stop(w, {follower.id});
      continue;
    }
    // Object animation/movement is independent of the tactic phase.
    if (boat.boat_state == 2 || boat.boat_state == 4 || boat.boat_state == 6) {
      const auto* inputs = rules_->sight();
      const int slot = data::boat_sequence(static_cast<data::BoatPhase>(boat.transport_phase), boat.orientation).slot;
      const bool resource = inputs && inputs->effects.anim(23).frame_count(slot) > 0;
      bool complete = false;
      if (resource) complete = inputs->sprites.boat_phase_complete(boat.transport_phase, boat.orientation, boat.anim_tick);
      else complete = boat.ferry_wait > 0 && --boat.ferry_wait == 0;
      if (complete) {
        if (boat.boat_state == 2) boat_state(boat, 3);
        else if (boat.boat_state == 4) boat_state(boat, 1);
        else {
          boat.orientation ^= 1;
          boat.ferry_direction = boat.orientation ? 1 : -1;
          boat_state(boat, 5);
        }
      }
    } else if (boat.boat_state == 5) {
      const int sign = boat.orientation ? 1 : -1;
      boat.ferry_direction = sign;
      boat.cur_x += sign * to_fixed(5); boat.cur_y -= sign * to_fixed(5);
      const int tx = tile(boat.cur_x), ty = tile(boat.cur_y);
      if (std::abs(tile_center_fixed(tx) - boat.cur_x) < to_fixed(5) &&
          !nav_.blocked(tx + sign, ty - sign, path::MoveClass::Small)) {
        boat.cur_x = tile_center_fixed(tx); boat.cur_y = tile_center_fixed(ty);
        boat_state(boat, 1);
      }
    }
    if (tile(boat.cur_x) < -7 || tile(boat.cur_y) < -7 ||
        tile(boat.cur_x) >= w.terrain.w + 7 || tile(boat.cur_y) >= w.terrain.h + 7) {
      script_kill(w, boat.id);
      for (auto& follower : w.objects) if (follower.boarding == boat.id) script_stop(w, {follower.id});
    }
  }
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
        boat->ferry_request = o.tactic;
        boat->ferry_destination = fresh_group(w);
        boat->ferry_group_side = o.side;
        boat->ferry_phase = 1; boat->vessel_route = true;
        boat_mode(*boat, 0);
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
    if (boat.ferry_request >= 0) {
      bool requester = false, followers = false;
      for (const auto& member : w.objects) {
        if (!member.alive || member.removed || member.boarding != boat.id) continue;
        followers = true;
        if (member.tactic == boat.ferry_request && member.side == boat.ferry_group_side) requester = true;
      }
      if (!requester) {
        boat.ferry_request = -1;
        if (boat.ferry_boarded == 0) {
          boat.ferry_group_side = -1;
          boat.ferry_phase = followers ? 0 : 5;
        }
      }
    }
    switch (boat.ferry_phase) {
      case 1: {
        if (boat.boat_state != 3) break;
        // V: departure is tested before this tick's loading scan.
        if (boat.ferry_boarded == kTransportCapacity || (boat.ferry_boarded && boat.ferry_request < 0)) {
          boat.ferry_request = -1;
          boat.ferry_phase = 2; boat_mode(boat, 1);
          break;
        }
        const int bx = tile(boat.cur_x), by = tile(boat.cur_y);
        for (int x = bx-1; x <= bx+1 && boat.ferry_boarded < int(kTransportCapacity); ++x)
          for (int y = by-1; y <= by+1 && boat.ferry_boarded < int(kTransportCapacity); ++y) {
            Object* occupant = nullptr;
            for (auto& candidate : w.objects) {
              if (!candidate.alive || candidate.removed || candidate.type < 0 || is_transport(candidate.type) ||
                  rules_->type(candidate.type).flier || tile(candidate.cur_x) != x || tile(candidate.cur_y) != y) continue;
              occupant = &candidate; break;
            }
            if (!occupant || boat.ferry_request < 0 || occupant->tactic != boat.ferry_request ||
                occupant->side != boat.ferry_group_side) continue;
            script_stop(w, {occupant->id});
            occupant->carrier = boat.id; occupant->removed = true; occupant->tactic = -1;
            boat.cargo.push_back(occupant->id); ++boat.ferry_boarded;
            if (occupant->mover >= 0) mover_->agents_mut()[size_t(occupant->mover)].enabled = false;
            auto erase_id = [&](auto& ids) { std::erase(ids, occupant->id); };
            erase_id(w.selection.ids);
            for (auto& side : w.side_state) erase_id(side.selected_ids);
          }
        break;
      }
      case 2:
        if (boat.boat_state == 1) { boat.ferry_phase = 3; boat_mode(boat, 2); }
        break;
      case 3:
        if (boat.boat_state == 1) boat_mode(boat, 0);
        else if (boat.boat_state == 3) {
          boat.vessel_arrived = true;
          unload(w, boat, tile(boat.cur_x), tile(boat.cur_y));
          if (boat.cargo.empty()) {
            boat.ferry_group_side = -1; boat.ferry_boarded = 0;
            boat.ferry_phase = 4; boat_mode(boat, 1);
          }
        }
        break;
      case 4:
        if (boat.boat_state == 1) { boat.ferry_phase = 5; boat_mode(boat, 2); }
        break;
      case 5:
        if (boat.boat_state == 1) { boat.ferry_phase = 0; boat.vessel_route = false; }
        else if (boat.boat_state == 3) boat_mode(boat, 1);
        break;
      default: break;
    }
  }
  for (auto& passenger : w.objects) if (passenger.carrier)
    if (const auto* boat = w.find(passenger.carrier)) passenger.teleport(boat->cur_x, boat->cur_y);
}
} // namespace aztec::engine
