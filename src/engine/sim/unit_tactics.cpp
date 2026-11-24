#include "engine/sim/game_sim.hpp"

#include <algorithm>
#include <limits>
#include <set>

#include "engine/world/visibility.hpp"

namespace aztec::engine {
namespace {
int tile(Fixed value) { return (value >> 8) / kTilePx; }
bool present(const Object& o) { return o.alive && !o.removed && o.type >= 0 && !o.carrier; }
bool nonfriendly(const World& w, int side, int other) {
  return side >= 0 && side < 8 && other >= 0 && other < 8 && side != other && w.relation[side][other] != kFriend;
}
void list_enemy(UnitTactic& t, TacticKey enemy) {
  if (std::find(t.enemies.begin(), t.enemies.end(), enemy) == t.enemies.end()) t.enemies.push_back(enemy);
}
std::vector<uint32_t> connected_members(const World& w, const UnitTactic& t, const RulesData& rules) {
  std::vector<uint32_t> connected;
  for (auto id : t.members) {
    const auto& m = *w.find(id);
    const int dx = (m.cur_x >> 8) - (t.x * kTilePx + 24);
    const int dy = (m.cur_y >> 8) - (t.y * kTilePx + 24);
    const int limit = rules.type(m.type).flier ? 192 : 120;
    if (std::abs(dx) <= limit && std::abs(dy) <= limit) connected.push_back(id);
  }
  for (size_t i = 0; i < connected.size(); ++i) {
    const auto& a = *w.find(connected[i]);
    for (auto id : t.members) {
      if (std::find(connected.begin(), connected.end(), id) != connected.end()) continue;
      const auto& b = *w.find(id);
      if (std::abs((a.cur_x >> 8) - (b.cur_x >> 8)) <= 120 &&
          std::abs((a.cur_y >> 8) - (b.cur_y >> 8)) <= 120) connected.push_back(id);
    }
  }
  return connected;
}
int fresh_tactic(const World& w) {
  int64_t next = 0;
  for (const auto& o : w.objects) next = std::max(next, int64_t(o.tactic) + 1);
  return next <= std::numeric_limits<int32_t>::max() ? int(next) : -1;
}
}

void GameSim::refresh_tactics(World& w) {
  for (auto& [key, t] : w.unit_tactics) { (void)key; t.members.clear(); }
  for (const auto& o : w.objects) {
    if (!present(o)) continue;
    auto [it, created] = w.unit_tactics.try_emplace(tactic_key(o));
    auto& t = it->second;
    if (created) {
      t.x = tile(o.cur_x); t.y = tile(o.cur_y);
      t.random = uint32_t(o.id * 2654435761u) | 1u;
    }
    t.members.push_back(o.id);
  }
  std::set<TacticKey> parents;
  for (const auto& [key, t] : w.unit_tactics) {
    (void)key;
    if ((t.lost || t.joining) && !t.members.empty()) parents.insert(t.parent);
  }
  // Keep empty parents until their merging followers can be absorbed.
  for (auto it = w.unit_tactics.begin(); it != w.unit_tactics.end();) {
    if (it->second.members.empty() && !parents.count(it->first)) it = w.unit_tactics.erase(it);
    else ++it;
  }
}

void GameSim::path_tactic(UnitTactic& t, const Object& leader, int x, int y) {
  const auto previous = t.state;
  t.dest_x = x; t.dest_y = y; t.node = 0; t.wait_ticks = 0; t.path_ticks = 0;
  t.path.clear();
  if (leader.mover < 0) { t.state = GroupState::Stopped; return; }
  auto cls = mover_->agents()[size_t(leader.mover)].cls;
  // G: use the independent A* route for tile steps. WaveSearch permits blocked
  // endpoints and diagonal corner crossings that order_step cannot execute.
  auto route = path::legacy_astar(nav_, cls, {t.x, t.y}, {x, y});
  if (!route.found) ++mover_->agents_mut()[size_t(leader.mover)].path_failures;
  for (size_t i = 1; i < route.tiles.size(); ++i) t.path.emplace_back(route.tiles[i].x, route.tiles[i].y);
  // V: a failed path stops a moving controller; other states are preserved.
  t.state = t.path.empty() ? (previous == GroupState::Moving ? GroupState::Stopped : previous) : GroupState::Moving;
}

void GameSim::move_tactic(World& w, const std::vector<uint32_t>& ids, int x, int y) {
  // A selection of otherwise independent unnumbered units becomes one tactic.
  // Script/AI numbered tactics retain their public identities.
  if (!ids.empty()) {
    bool independent = true, same = true;
    const auto* first = w.find(ids.front());
    for (auto id : ids) {
      const auto* m = w.find(id);
      independent &= m && first && m->side == first->side && m->tactic == -1;
      same &= m && first && tactic_key(*m) == tactic_key(*first);
    }
    bool subset = false;
    if (first && same) for (const auto& member : w.objects) {
      if (present(member) && tactic_key(member) == tactic_key(*first) && member.mover >= 0 &&
          std::find(ids.begin(), ids.end(), member.id) == ids.end()) subset = true;
    }
    if (first && same) for (const auto& [key, team] : w.unit_tactics) {
      (void)key;
      if (team.lost && team.parent == tactic_key(*first))
        for (auto id : team.members) if (std::find(ids.begin(), ids.end(), id) == ids.end()) subset = true;
    }
    if (independent && (!same || subset)) {
      const auto identity = -(int64_t(1) << 40) - int64_t(w.next_unit_tactic++);
      for (auto id : ids) w.find_mut(id)->transient_tactic = identity;
    }
  }
  refresh_tactics(w);
  std::set<TacticKey> keys;
  for (auto id : ids) if (auto* o = w.find(id)) keys.insert(tactic_key(*o));
  for (const auto& key : keys) {
    auto it = w.unit_tactics.find(key);
    if (it == w.unit_tactics.end()) continue;
    auto& t = it->second;
    t.enemies.clear(); t.lost = false; t.joining = false;
    const Object* leader = nullptr;
    for (auto id : t.members) {
      auto& member = *w.find_mut(id);
      if (member.mover < 0) continue;
      // Use the strictest land class represented by the group for its shared path.
      if (!leader || mover_->agents()[size_t(member.mover)].cls == path::MoveClass::Large) leader = &member;
      member.target = 0; member.idle_attack = false; member.return_to_tactic = true;
      member.dest_x = x; member.dest_y = y;
      if (member.order != uint8_t(Order::AttackMove) && member.order != uint8_t(Order::Follow)) member.order = uint8_t(Order::Move);
      mover_->stop(uint32_t(member.mover));
    }
    if (leader) path_tactic(t, *leader, x / kTilePx, y / kTilePx);
  }
}

void GameSim::merge_tactic(World& w, TacticKey source, TacticKey target) {
  auto from = w.unit_tactics.find(source), to = w.unit_tactics.find(target);
  if (source == target || from == w.unit_tactics.end() || to == w.unit_tactics.end()) return;
  auto& parent = to->second;
  if (parent.members.empty()) { parent.x = from->second.x; parent.y = from->second.y; }
  const int town = parent.members.empty() ? -1 : w.find(parent.members.front())->town;
  for (auto id : from->second.members) {
    auto& member = *w.find_mut(id);
    member.tactic = target.second < -int64_t(UINT32_MAX) ? -1 : int32_t(target.second);
    if (member.tactic == -1) member.transient_tactic = target.second;
    if (from->second.joining) member.town = town;
    member.order = uint8_t(parent.moving() ? Order::Move : Order::Idle);
    member.return_to_tactic = true;
  }
  parent.members.insert(parent.members.end(), from->second.members.begin(), from->second.members.end());
  // V: followers of an absorbed group follow its receiving controller.
  for (auto& [key, follower] : w.unit_tactics) {
    (void)key;
    if ((follower.lost || follower.joining) && follower.parent == source) follower.parent = target;
  }
  if (target.second >= 0 && target.second <= std::numeric_limits<int32_t>::max())
    for (auto& member : w.objects)
      if (member.side == source.first && member.follow_tactic >= 0 && member.follow_tactic == source.second)
        member.follow_tactic = int32_t(target.second);
  w.unit_tactics.erase(from);
}

void GameSim::script_join(World& w, const std::vector<uint32_t>& source, const std::vector<uint32_t>& target) {
  if (source.empty() || target.empty()) return;
  refresh_tactics(w);
  const auto* first = w.find(source.front());
  const auto* last = w.find(target.front());
  if (!first || !last) return;
  const auto from = tactic_key(*first), to = tactic_key(*last);
  auto origin = w.unit_tactics.find(from), receiver = w.unit_tactics.find(to);
  if (from == to || from.first != to.first || origin == w.unit_tactics.end() || receiver == w.unit_tactics.end()) return;
  // Independent identity guard: never create a cyclic merge chain.
  std::set<TacticKey> visited{from};
  for (auto next = receiver; next != w.unit_tactics.end();) {
    if (!visited.insert(next->first).second) return;
    if (!next->second.joining && !next->second.lost) break;
    next = w.unit_tactics.find(next->second.parent);
  }
  auto& group = origin->second;
  const auto& destination = receiver->second;
  const auto& leader = *w.find(source.front());
  group.enemies.clear(); group.joining = true; group.lost = false; group.parent = to;
  if (tactic_norm(group.x - destination.x, group.y - destination.y) < tactic_reach(leader.attack_mode)) {
    merge_tactic(w, from, to);
    refresh_tactics(w);
    return;
  }
  // V: retain the source controller until it reaches the target's strict reach.
  for (auto id : source) {
    auto& member = *w.find_mut(id);
    member.target = 0; member.resume = 0; member.idle_attack = false;
    member.order = uint8_t(Order::Follow); member.follow_tactic = -1;
    member.return_to_tactic = true;
  }
  path_tactic(group, leader, destination.x, destination.y);
}

void GameSim::member_step(World& w, Object& o, int x, int y, bool combat) {
  if (o.mover < 0 || o.boarding || o.release_target) return;
  const auto& a = mover_->agents()[size_t(o.mover)];
  if (!a.enabled || a.state == path::AgentState::Moving) return;
  const auto it = w.unit_tactics.find(tactic_key(o));
  if (it == w.unit_tactics.end()) return;
  const auto& t = it->second;
  if (t.state == GroupState::Inactive) return; // V: no member-step callback in state 0.
  const int sx = tile(o.cur_x), sy = tile(o.cur_y);
  if (combat && tactic_norm(sx - t.x, sy - t.y) > tactic_reach(o.attack_mode)) {
    x = t.x; y = t.y; combat = false;
  }
  if (sx == x && sy == y) return;
  auto fellow_at = [&](int tx, int ty) {
    for (auto id : t.members) {
      if (id == o.id) continue;
      const auto& other = *w.find(id);
      if (tile(other.cur_x) == tx && tile(other.cur_y) == ty) return true;
      if (other.mover >= 0) {
        const auto& agent = mover_->agents()[size_t(other.mover)];
        if (agent.tile_step && agent.state == path::AgentState::Moving &&
            tile(agent.step_goal.x) == tx && tile(agent.step_goal.y) == ty) return true;
      }
    }
    return false;
  };
  if (!combat) {
    auto route = path::legacy_astar(nav_, a.cls, {sx, sy}, {x, y});
    if (route.tiles.size() > 1) {
      const auto next = route.tiles[1];
      if (!fellow_at(next.x, next.y) && mover_->order_step(uint32_t(o.mover), next)) return;
      if (!t.moving() && !t.lost && std::abs(sx - x) <= 2 && std::abs(sy - y) <= 2) return;
      // G: route around occupied fellow tiles when the direct member step is
      // blocked. This substitutes for the reference's short packed own path.
      auto occupied = nav_;
      auto block = [&](int tx, int ty) {
        if (occupied.in_bounds(tx, ty) && (tx != sx || ty != sy)) occupied.add_block(tx, ty, 1, 1, 1);
      };
      for (auto id : t.members) {
        if (id == o.id) continue;
        const auto& other = *w.find(id);
        block(tile(other.cur_x), tile(other.cur_y));
        if (other.mover >= 0) {
          const auto& agent = mover_->agents()[size_t(other.mover)];
          if (agent.tile_step && agent.state == path::AgentState::Moving) block(tile(agent.step_goal.x), tile(agent.step_goal.y));
        }
      }
      for (const auto& other : w.objects)
        if (present(other) && nonfriendly(w, o.side, other.side)) block(tile(other.cur_x), tile(other.cur_y));
      auto detour = path::legacy_astar(occupied, a.cls, {sx, sy}, {x, y});
      if (detour.tiles.size() > 1) mover_->order_step(uint32_t(o.mover), detour.tiles[1]);
    }
    return;
  }
  const int dx = x - sx, dy = y - sy;
  // V: quantize the anchor direction with an axis cutoff of 2.5 times the minor.
  int vx = (dx > 0) - (dx < 0), vy = (dy > 0) - (dy < 0);
  if (std::abs(dx) * 2 >= std::abs(dy) * 5) vy = 0;
  else if (std::abs(dy) * 2 >= std::abs(dx) * 5) vx = 0;
  constexpr int ox[] = {1, 1, 0, -1, -1, -1, 0, 1};
  constexpr int oy[] = {0, 1, 1, 1, 0, -1, -1, -1};
  int direction = 0;
  for (int d = 0; d < 8; ++d) if (ox[d] == vx && oy[d] == vy) direction = d;
  constexpr int rotations[] = {0, 1, -1, 2, -2, 3, -3, 4};
  const int attempts = combat ? 8 : 3;
  for (int i = 0; i < attempts; ++i) {
    const int d = (direction + rotations[i] + 8) % 8;
    const int nx = sx + ox[d], ny = sy + oy[d];
    if (combat && tactic_norm(nx - t.x, ny - t.y) > tactic_reach(o.attack_mode)) continue;
    if (fellow_at(nx, ny)) return;
    if (mover_->order_step(uint32_t(o.mover), {nx, ny})) return;
  }

}

void GameSim::think_tactics(World& w) {
  refresh_tactics(w);
  std::vector<std::pair<TacticKey, std::vector<uint32_t>>> detach;
  std::vector<TacticKey> merge;
  for (auto& [key, t] : w.unit_tactics) {
    if (t.members.empty()) continue;
    Object& leader = *w.find_mut(t.members.front());
    const int reach = tactic_reach(leader.attack_mode);
    const auto connected = connected_members(w, t, *rules_);
    if (t.lost || t.joining) {
      auto parent = w.unit_tactics.find(t.parent);
      if (parent == w.unit_tactics.end()) { t.lost = false; t.joining = false; }
      else if (parent->second.members.empty() || tactic_norm(t.x - parent->second.x, t.y - parent->second.y) < reach) {
        merge.push_back(key); continue;
      } else if (!t.moving() || ++t.path_ticks > 36) {
        path_tactic(t, leader, parent->second.x, parent->second.y);
      }
    }
    if (t.scan_ticks > 0) --t.scan_ticks;
    if (t.scan_ticks == 0) {
      t.random ^= t.random << 13; t.random ^= t.random >> 17; t.random ^= t.random << 5;
      t.scan_ticks = 15 + int(t.random % 8);
      int minx = t.x, maxx = t.x, miny = t.y, maxy = t.y;
      for (auto id : t.members) {
        const auto& m = *w.find(id);
        minx = std::min(minx, tile(m.cur_x)); maxx = std::max(maxx, tile(m.cur_x));
        miny = std::min(miny, tile(m.cur_y)); maxy = std::max(maxy, tile(m.cur_y));
      }
      bool sighting = false;
      for (const auto& enemy : w.objects) {
        if (!present(enemy) || !nonfriendly(w, key.first, enemy.side) || !object_visible(w, key.first, enemy)) continue;
        const int ex = enemy.cur_x >> 8, ey = enemy.cur_y >> 8;
        if (ex < (minx - 6) * kTilePx || ex >= (maxx + 8) * kTilePx ||
            ey < (miny - 6) * kTilePx || ey >= (maxy + 8) * kTilePx) continue;
        sighting = true;
        if (leader.attack_mode != AttackMode::Defensive && rules_->type(enemy.type).kind != 7)
          list_enemy(t, tactic_key(enemy));
      }
      if (!sighting && !t.urgent && !t.enemies.empty()) {
        t.enemies.clear();
        for (auto id : t.members) {
          auto& m = *w.find_mut(id);
          if (m.idle_attack) finish_attack(m);
        }
      }
      std::vector<uint32_t> stragglers;
      for (auto id : t.members) {
        auto& m = *w.find_mut(id);
        if (rules_->type(m.type).flier && m.idle_attack) {
          const auto* target = w.find(m.target);
          if (target && tactic_norm(tile(target->cur_x) - t.x, tile(target->cur_y) - t.y) > reach) finish_attack(m);
        }
        if (!t.lost && m.mover >= 0 && !m.boarding &&
            std::find(connected.begin(), connected.end(), id) == connected.end() &&
            tactic_norm(tile(m.cur_x) - t.x, tile(m.cur_y) - t.y) > reach) stragglers.push_back(id);
      }
      if (!stragglers.empty()) detach.emplace_back(key, std::move(stragglers));
    }
    t.urgent = false;
    if (t.state == GroupState::Waiting) {
      // G: independent agents supply readiness until native member flags exist.
      const bool ready = std::all_of(t.members.begin(), t.members.end(), [&](uint32_t id) {
        const auto& member = *w.find(id);
        return member.mover < 0 || mover_->agents()[size_t(member.mover)].state != path::AgentState::Moving;
      });
      if (ready) t.state = GroupState::Moving;
    }
    if (!t.moving()) continue;
    bool in_place = true, at_anchor = false, waiting_team = false;
    for (auto id : t.members) {
      const auto& m = *w.find(id);
      // G: connected members can keep the formation ready while taking a step;
      // only disconnected own-path movement holds up the anchor.
      if (m.mover >= 0 && mover_->agents()[size_t(m.mover)].state == path::AgentState::Moving &&
          std::find(connected.begin(), connected.end(), id) == connected.end()) in_place = false;
      if (tile(m.cur_x) == t.x && tile(m.cur_y) == t.y) at_anchor = true;
    }
    for (const auto& [other, follower] : w.unit_tactics) {
      (void)other;
      waiting_team |= follower.lost && follower.parent == key;
    }
    ++t.wait_ticks;
    if (((!in_place || waiting_team || !at_anchor) && t.wait_ticks < 20)) continue;
    t.wait_ticks = 0;
    if (t.node < t.path.size()) {
      const auto [x, y] = t.path[t.node++];
      // Revalidate nodes against new buildings before advancing the anchor.
      if (leader.mover >= 0 && nav_.blocked(x, y, mover_->agents()[size_t(leader.mover)].cls)) {
        path_tactic(t, leader, t.dest_x, t.dest_y); continue;
      }
      t.x = x; t.y = y;
    } else {
      t.state = GroupState::Stopped;
    }
  }
  for (const auto& [parent, members] : detach) {
    const int id = fresh_tactic(w);
    if (id < 0) continue;
    auto& lost = w.unit_tactics[{parent.first, id}];
    const auto& first = *w.find(members.front());
    lost.x = tile(first.cur_x); lost.y = tile(first.cur_y);
    lost.lost = true; lost.parent = parent; lost.random = first.id | 1u;
    for (auto member : members) {
      auto& m = *w.find_mut(member);
      m.tactic = id; m.target = 0; m.resume = 0; m.idle_attack = false;
      m.order = uint8_t(Order::Follow); m.return_to_tactic = true;
      if (m.mover >= 0) mover_->stop(uint32_t(m.mover));
    }
  }
  for (auto key : merge) merge_tactic(w, key, w.unit_tactics.at(key).parent);
  refresh_tactics(w);
  for (const auto& [key, t] : w.unit_tactics) {
    (void)key;
    for (auto id : t.members) {
      auto& m = *w.find_mut(id);
      m.idle_anchor_x = t.x * kTilePx + 24; m.idle_anchor_y = t.y * kTilePx + 24;
    }
  }
}
} // namespace aztec::engine
