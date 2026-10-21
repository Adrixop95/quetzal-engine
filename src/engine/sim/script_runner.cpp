#include "engine/sim/script_runner.hpp"

#include <algorithm>

#include "core/text.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/world/type_info.hpp"

namespace aztec::engine {

using namespace aztec::script;

bool is_town_building(int t) {
  static const int th[] = {10, 11, 19, 24, 37, 38, 47, 63, 64, 71, 76};
  for (int v : th) if (v == t) return true;
  return false;
}

bool ScriptRunner::load(const std::string& path) {
  Script s;
  if (!load_script(path, s)) return false;
  set(std::move(s));
  return true;
}

void ScriptRunner::set(Script s) { script_ = std::move(s); }

void ScriptRunner::init(const World&) {
  enabled_.clear();
  timers_.clear();
  tactics_.clear();
  local_time_ = 0;
  // Triggers that another trigger enables start disabled (initial state is not documented; see header).
  for (const Trigger& t : script_.triggers) enabled_[t.id] = true;
  for (const Trigger& t : script_.triggers)
    for (const Action& a : t.actions)
      if (a.kind == ActKind::TriggerEnable && a.id != t.id) enabled_[a.id] = false;
}

int ScriptRunner::count_units(const World& w, int side) const {
  int n = 0;
  for (const Object& o : w.objects) {
    if (o.removed || !o.alive || o.type < 0 || o.side != side) continue;
    const TypeInfo* ti = type_info(o.type);
    if (ti && !ti->is_structure()) n++;
  }
  return n;
}

int ScriptRunner::count_towns(const World& w, int side) const {
  int n = 0;
  for (const Object& o : w.objects)
    if (!o.removed && o.alive && o.side == side && is_town_building(o.type) && o.progress >= 1000) n++;
  return n;
}

std::vector<uint32_t> ScriptRunner::tactic_members(const World& w, int side, int tactic) const {
  std::vector<uint32_t> v;
  for (const Object& o : w.objects)
    if (!o.removed && o.alive && o.side == side && o.tactic == tactic && tactic >= 0) v.push_back(o.id);
  return v;
}

static bool cmp(Cmp c, long a, long b) {
  switch (c) {
    case Cmp::Eq: return a == b;
    case Cmp::Ne: return a != b;
    case Cmp::Lt: return a < b;
    case Cmp::Le: return a <= b;
    case Cmp::Gt: return a > b;
    case Cmp::Ge: return a >= b;
  }
  return false;
}

bool ScriptRunner::eval(const World& w, const Condition& c) const {
  int side = resolve_side(w, c.side);
  auto in_rect = [&](const Object& o) {
    int tx = (o.cur_x >> 8) / kTilePx, ty = (o.cur_y >> 8) / kTilePx;
    return tx >= c.rect[0] && tx <= c.rect[2] && ty >= c.rect[1] && ty <= c.rect[3];
  };
  switch (c.kind) {
    case CondKind::Always: return true;
    case CondKind::LocalTime:
    case CondKind::GlobalTime: return cmp(c.cmp, long(local_time_), c.value);
    case CondKind::Timer: {
      auto it = timers_.find(c.id);
      if (it == timers_.end()) return cmp(c.cmp, 0, c.value);
      if (!it->second.exists) return false;
      return cmp(c.cmp, it->second.value, c.value);
    }
    case CondKind::SideUnits: return cmp(c.cmp, count_units(w, side), c.value);
    case CondKind::SideTowns: return cmp(c.cmp, count_towns(w, side), c.value);
    case CondKind::SideEnemies: {
      int n = 0;
      for (const Object& o : w.objects) {
        if (o.removed || !o.alive || o.type < 0 || !w.hostile(side, o.side)) continue;
        const TypeInfo* ti = type_info(o.type);
        if (ti && (!ti->is_structure() || is_town_building(o.type))) n++;
      }
      return cmp(c.cmp, n, c.value);
    }
    case CondKind::SideTactics: {
      std::vector<int> seen;
      for (const Object& o : w.objects)
        if (!o.removed && o.alive && o.side == side && o.tactic >= 0 && std::find(seen.begin(), seen.end(), o.tactic) == seen.end()) seen.push_back(o.tactic);
      return cmp(c.cmp, long(seen.size()), c.value);
    }
    case CondKind::SideMoney:
      return cmp(c.cmp, side >= 0 && side < int(w.side_state.size()) ? w.side_state[size_t(side)].money : 0, c.value);
    case CondKind::SideUnitsInRect: {
      int n = 0;
      for (const Object& o : w.objects) {
        if (o.removed || !o.alive || o.type < 0 || o.side != side) continue;
        const TypeInfo* ti = type_info(o.type);
        if (ti && !ti->is_structure() && in_rect(o)) n++;
      }
      return cmp(c.cmp, n, c.value);
    }
    case CondKind::SideUnitsOfUGIInRect:
    case CondKind::SideUnitsOfId: {
      int n = 0;
      for (const Object& o : w.objects)
        if (!o.removed && o.alive && o.side == side && o.type == c.ugi && (c.kind == CondKind::SideUnitsOfId || in_rect(o))) n++;
      return cmp(c.cmp, n, c.value);
    }
    case CondKind::TacticValid:
    case CondKind::TacticAttacked:
    case CondKind::TacticIdle:
    case CondKind::TacticLocked: {
      std::vector<uint32_t> m = tactic_members(w, side, c.id);
      long v = 0;
      if (c.kind == CondKind::TacticValid) v = m.empty() ? 0 : 1;
      else if (c.kind == CondKind::TacticAttacked) {
        v = -1;
        for (uint32_t id : m) if (w.find(id)->last_hit_by) { v = long(w.find(id)->last_hit_by); break; }
      } else if (c.kind == CondKind::TacticIdle) {
        v = 1;
        for (uint32_t id : m) if (w.find(id)->order != uint8_t(Order::Idle)) v = 0;
      } else {
        auto it = tactics_.find({side, c.id});
        v = it != tactics_.end() && it->second.locked ? 1 : 0;
      }
      return cmp(c.cmp, v, c.value);
    }
    case CondKind::DefenceValid: {
      long v = 0;
      for (const Object& o : w.objects)
        if (!o.removed && o.alive && o.side == side && o.scenario == c.id && o.type >= 0 && type_info(o.type) && type_info(o.type)->kind == 7) v = 1;
      return cmp(c.cmp, v, c.value);
    }
    case CondKind::IsFriendly: case CondKind::IsNeutral: case CondKind::IsEnemy: {
      int other = resolve_side(w, c.id);
      if (side < 0 || side >= 8 || other < 0 || other >= 8) return false;
      uint8_t want = c.kind == CondKind::IsFriendly ? kFriend : c.kind == CondKind::IsNeutral ? kNeutral : kEnemy;
      return cmp(c.cmp, w.relation[side][other] == want ? 1 : 0, c.value);
    }
  }
  return false;
}

void ScriptRunner::run(GameSim& sim, World& w, const Action& a) {
  int side = resolve_side(w, a.side);
  switch (a.kind) {
    case ActKind::TriggerEnable: enabled_[a.id] = true; break;
    case ActKind::TriggerDisable: enabled_[a.id] = false; break;
    case ActKind::TimerSet: timers_[a.id] = {true, a.a, true}; break;
    case ActKind::TimerStart: { Timer& t = timers_[a.id]; t.running = true; t.exists = true; break; }
    case ActKind::TimerStop: timers_[a.id].running = false; break;
    case ActKind::TimerKill: timers_[a.id].exists = false; break;
    case ActKind::CenterView:
      w.center_view = true; w.center_x = a.a * kTilePx + kTilePx / 2; w.center_y = a.b * kTilePx + kTilePx / 2; w.center_serial++;
      break;
    case ActKind::SideCameraActivate: {
      for (const SideDef& sd : w.mission.sides)
        if (sd.index == side && a.a >= 0 && a.a < int(sd.cameras.size())) {
          w.center_view = true;
          w.center_x = sd.cameras[size_t(a.a)][0] * kTilePx + kTilePx / 2;
          w.center_y = sd.cameras[size_t(a.a)][1] * kTilePx + kTilePx / 2;
          w.center_serial++;
        }
      break;
    }
    case ActKind::PrintString: w.messages.push_back({cp1250_to_utf8(a.text), 12 * 6}); break;
    case ActKind::PrintStringR: break;  // Russian variant: the English one is shown
    case ActKind::SideVictory: case ActKind::SideDefeat: {
      uint8_t r = a.kind == ActKind::SideVictory ? kVictory : kDefeat;
      if (side >= 0 && side < 8 && w.side_outcome[side] == kRunning) w.side_outcome[side] = r;
      if (side == w.local_side && w.outcome == kRunning) w.outcome = r;
      break;
    }
    case ActKind::SideAddMoney: if (side >= 0 && side < int(w.side_state.size())) w.side_state[size_t(side)].money += a.a; break;
    case ActKind::SideTakeMoney: if (side >= 0 && side < int(w.side_state.size())) w.side_state[size_t(side)].money = std::max(0, w.side_state[size_t(side)].money - a.a); break;
    case ActKind::SideSetFriendlyTo: case ActKind::SideSetNeutralTo: case ActKind::SideSetEnemyTo: {
      int other = resolve_side(w, a.a);
      if (side < 0 || side >= 8 || other < 0 || other >= 8 || side == other) break;
      uint8_t r = a.kind == ActKind::SideSetFriendlyTo ? kFriend : a.kind == ActKind::SideSetNeutralTo ? kNeutral : kEnemy;
      w.relation[side][other] = r;   // one-directional, like the original's per-side masks
      break;
    }
    case ActKind::TacticAttackTown: {
      const Object* town = nullptr;
      for (const Object& o : w.objects)
        if (!o.removed && o.alive && is_town_building(o.type) && o.scenario == a.a) town = &o;
      if (!town) break;
      sim.script_order(w, tactic_members(w, side, a.id), town->cur_x >> 8, town->cur_y >> 8, true);
      tactics_[{side, a.id}].path = -1;
      break;
    }
    case ActKind::TacticMoveTo:
      sim.script_order(w, tactic_members(w, side, a.id), a.a * kTilePx + kTilePx / 2, a.b * kTilePx + kTilePx / 2, true);
      tactics_[{side, a.id}].path = -1;
      break;
    case ActKind::TacticJoinTactic:
      for (uint32_t id : tactic_members(w, side, a.id)) w.find_mut(id)->tactic = a.a;
      break;
    case ActKind::TacticSetPath: case ActKind::TacticSetInvertedPath: case ActKind::TacticSetCycledPath: case ActKind::TacticSetCycledInvertedPath: {
      TacticState& t = tactics_[{side, a.id}];
      t.path = a.a; t.path_index = 0;
      t.cycled = a.kind == ActKind::TacticSetCycledPath || a.kind == ActKind::TacticSetCycledInvertedPath;
      t.inverted = a.kind == ActKind::TacticSetInvertedPath || a.kind == ActKind::TacticSetCycledInvertedPath;
      break;
    }
    case ActKind::TacticLock: tactics_[{side, a.id}].locked = true; break;
    case ActKind::TacticUnlock: tactics_[{side, a.id}].locked = false; break;
    case ActKind::TacticKill:
      for (uint32_t id : tactic_members(w, side, a.id)) sim.script_kill(w, id);
      break;
    case ActKind::TacticGiftToSide: {
      int to = resolve_side(w, a.a);
      for (uint32_t id : tactic_members(w, side, a.id)) {
        Object* o = w.find_mut(id);
        o->side = to;
        o->order = uint8_t(Order::Idle);
        o->target = 0;
      }
      break;
    }
    default:  // boat / johnka / submarine activation, build and move commands, camera deactivate: not implemented yet
      unsupported_++;
      break;
  }
}

void ScriptRunner::follow_paths(GameSim& sim, World& w) {
  for (auto& kv : tactics_) {
    TacticState& t = kv.second;
    if (t.path < 0 || t.path >= int(w.mission.paths.size()) || w.mission.paths[size_t(t.path)].empty()) continue;
    const auto& pts = w.mission.paths[size_t(t.path)];
    std::vector<uint32_t> m = tactic_members(w, kv.first.first, kv.first.second);
    if (m.empty()) continue;
    bool idle = true;
    for (uint32_t id : m) if (w.find(id)->order != uint8_t(Order::Idle)) idle = false;
    if (!idle) continue;
    int n = int(pts.size());
    int idx = t.path_index;
    if (idx >= n) {
      if (!t.cycled) { t.path = -1; continue; }
      idx = 0;
    }
    int k = t.inverted ? n - 1 - idx : idx;
    sim.script_order(w, m, pts[size_t(k)].first * kTilePx + kTilePx / 2, pts[size_t(k)].second * kTilePx + kTilePx / 2, true);
    t.path_index = idx + 1;
  }
}

uint64_t ScriptRunner::state_hash() const {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
  mix(local_time_);
  for (const auto& e : enabled_) { mix(uint64_t(e.first)); mix(e.second); }
  for (const auto& t : timers_) { mix(uint64_t(t.first)); mix(uint64_t(t.second.value)); mix(t.second.running * 2u + t.second.exists); }
  for (const auto& t : tactics_) { mix(uint64_t(t.first.first * 1000 + t.first.second)); mix(t.second.locked); mix(uint64_t(t.second.path + 1)); mix(uint64_t(t.second.path_index)); }
  return h;
}

void ScriptRunner::tick(GameSim& sim, World& w) {
  local_time_++;
  for (auto& kv : timers_)
    if (kv.second.exists && kv.second.running) kv.second.value++;
  if (timers_.find(0) == timers_.end()) timers_[0] = Timer{};   // implicit timer 0 runs from mission start
  for (const Trigger& t : script_.triggers) {
    auto it = enabled_.find(t.id);
    if (it != enabled_.end() && !it->second) continue;
    bool ok = true;
    for (const Condition& c : t.conditions) if (!eval(w, c)) { ok = false; break; }
    if (!ok) continue;
    for (const Action& a : t.actions) run(sim, w, a);
  }
  follow_paths(sim, w);
  for (Message& m : w.messages) m.ticks_left--;
  w.messages.erase(std::remove_if(w.messages.begin(), w.messages.end(), [](const Message& m) { return m.ticks_left <= 0; }), w.messages.end());
}

}  // namespace aztec::engine
