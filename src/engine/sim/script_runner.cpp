#include "engine/sim/script_runner.hpp"

#include <algorithm>
#include <bit>

#include "core/text.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/world/type_info.hpp"

namespace aztec::engine {

using namespace aztec::script;

// G: embarked living passengers retain script identity and ownership counts.
// Hidden cargo must not count as a dead protected unit or a defeated side.
static bool script_present(const Object& object) {
  return object.alive && (!object.removed || object.carrier != 0);
}

// V: runtime categories admit units, structures and vessels, but not cameras.
// Construction-class numbers in TypeInfo use a different namespace.
static bool script_rect_type(int type) {
  const auto* info = type_info(type);
  return info && info->kind != 11;
}

bool is_town_building(int t) {
  static const int th[] = {10, 11, 19, 24, 37, 38, 47, 63, 64, 71, 76};
  for (int v : th) if (v == t) return true;
  return false;
}

bool ScriptRunner::supports(ActKind kind) {
  switch (kind) {
    case ActKind::SideBuildCommand:
    case ActKind::SideMoveCommand:
      return false;
    case ActKind::BoatActivate:
    case ActKind::JohnkaEnable:
    case ActKind::JohnkaDisable:
    case ActKind::SubmarineGo:
    case ActKind::TriggerEnable:
    case ActKind::TriggerDisable:
    case ActKind::TimerSet:
    case ActKind::TimerStart:
    case ActKind::TimerStop:
    case ActKind::TimerKill:
    case ActKind::CenterView:
    case ActKind::PrintString:
    case ActKind::PrintStringR:
    case ActKind::SideVictory:
    case ActKind::SideDefeat:
    case ActKind::SideCameraActivate:
    case ActKind::SideCameraDeactivate:
    case ActKind::SideSetFriendlyTo:
    case ActKind::SideSetNeutralTo:
    case ActKind::SideSetEnemyTo:
    case ActKind::SideAddMoney:
    case ActKind::SideTakeMoney:
    case ActKind::TacticAttackTown:
    case ActKind::TacticMoveTo:
    case ActKind::TacticJoinTactic:
    case ActKind::TacticSetPath:
    case ActKind::TacticSetInvertedPath:
    case ActKind::TacticSetCycledPath:
    case ActKind::TacticSetCycledInvertedPath:
    case ActKind::TacticLock:
    case ActKind::TacticUnlock:
    case ActKind::TacticKill:
    case ActKind::TacticGiftToSide:
      return true;
  }
  return false;
}

bool ScriptRunner::load(const std::string& path) {
  Script s;
  if (!load_script(path, s)) return false;
  set(std::move(s));
  return true;
}

void ScriptRunner::set(Script s) {
  script_ = std::move(s);
  trigger_traces_.assign(script_.triggers.size(), {});
}

void ScriptRunner::init(const World& w) {
  enabled_.assign(script_.triggers.size(), true);
  latched_.assign(script_.triggers.size(), false);
  timers_.clear();
  tactics_.clear();
  local_time_ = (w.mission.start_time % 7200 + 7200) % 7200;
  elapsed_ticks_ = 0;
  unsupported_ = 0;
  outcome_origin_.reset();
  trigger_traces_.assign(script_.triggers.size(), {});
  // G: placed scenario groups start reserved; map Locked loading is not available.
  for (const auto& o : w.objects)
    if (script_present(o) && o.tactic >= 0) tactics_.try_emplace({o.side, o.tactic});
}

void ScriptRunner::set_enabled(int id, bool enabled) {
  for (size_t i = 0; i < script_.triggers.size(); ++i)
    if (script_.triggers[i].id == id) { enabled_[i] = enabled; return; }
}

bool ScriptRunner::trigger_enabled(int id) const {
  for (size_t i = 0; i < script_.triggers.size(); ++i)
    if (script_.triggers[i].id == id) return enabled_[i];
  return false;
}

std::vector<ScriptActionObservation> ScriptRunner::action_observations(const World& w) const {
  std::vector<ScriptActionObservation> result;
  for (size_t i = 0; i < script_.triggers.size(); ++i) {
    const auto& t = script_.triggers[i];
    std::vector<int> false_lines;
    for (const auto& c : t.conditions) if (!eval(w, c)) false_lines.push_back(c.line);
    for (size_t j = 0; j < t.actions.size(); ++j) {
      const auto& a = t.actions[j];
      result.push_back({i, j, t.id, a.line, a.kind, enabled_[i], trigger_traces_[i], false_lines});
    }
  }
  return result;
}

int ScriptRunner::count_units(const World& w, int side) const {
  int n = 0;
  for (const Object& o : w.objects) {
    if (!script_present(o) || o.type < 0 || o.side != side) continue;
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
  const auto state = tactics_.find({side, tactic});
  // V: only reserved plain tactics are addressable, with towns as the fallback.
  if (state == tactics_.end() || state->second.locked) {
    for (const Object& o : w.objects) {
      const bool town_group = std::any_of(w.mission.towns.begin(), w.mission.towns.end(), [&](const TownDef& t) {
        return t.side == side && t.index == o.town && t.scenario_id == tactic;
      });
      if (script_present(o) && o.side == side && !town_group && o.tactic == tactic && tactic >= 0)
        v.push_back(o.id);
    }
  }
  if (!v.empty()) return v;
  for (const auto& town : w.mission.towns) {
    if (town.side != side || town.scenario_id != tactic) continue;
    for (const Object& o : w.objects)
      if (script_present(o) && o.side == side && o.town == town.index) v.push_back(o.id);
    break;
  }
  return v;
}

static bool cmp(Cmp c, int64_t a, int64_t b) {
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

void ScriptRunner::notify_attack(World& w, const Object& target, int source_side) {
  // V: attacked stores the notifying nonfriendly side, independently of its lifetime.
  // G: the combat adapter checks source kind and tactic before notifying.
  if (target.tactic >= 0 && target.side >= 0 && target.side < 8 &&
      source_side >= 0 && source_side < 8 && target.side != source_side &&
      w.relation[target.side][source_side] != kFriend)
    w.tactic_attacked_by[{target.side, target.tactic}] = source_side;
}

bool ScriptRunner::eval(const World& w, const Condition& c) const {
  int side = resolve_side(w, c.side);
  auto in_rect = [&](const Object& o) {
    int tx = (o.cur_x >> 8) / kTilePx, ty = (o.cur_y >> 8) / kTilePx;
    return tx >= std::min(c.rect[0], c.rect[2]) && tx <= std::max(c.rect[0], c.rect[2]) &&
        ty >= std::min(c.rect[1], c.rect[3]) && ty <= std::max(c.rect[1], c.rect[3]);
  };
  switch (c.kind) {
    case CondKind::Always: return true;
    case CondKind::LocalTime: return cmp(c.cmp, local_time_, c.value);
    case CondKind::GlobalTime: return cmp(c.cmp, std::bit_cast<int32_t>(uint32_t(elapsed_ticks_)), c.value);
    case CondKind::Timer: {
      auto it = timers_.find(c.id);
      if (it == timers_.end()) return cmp(c.cmp, 0, c.value);
      return cmp(c.cmp, it->second.value, c.value);
    }
    case CondKind::SideUnits: return cmp(c.cmp, count_units(w, side), c.value);
    case CondKind::SideTowns: return cmp(c.cmp, count_towns(w, side), c.value);
    case CondKind::SideEnemies: {
      int n = 0;
      for (int other = 0; other < 8; ++other) {
        if (!w.hostile(side, other)) continue;
        const bool has_tactic = std::any_of(w.objects.begin(), w.objects.end(), [&](const Object& o) {
          const TypeInfo* ti = type_info(o.type);
          return script_present(o) && o.side == other &&
              (o.tactic != -1 || (ti && !ti->is_structure()));
        });
        if (has_tactic || count_towns(w, other) != 0) ++n;
      }
      return cmp(c.cmp, n, c.value);
    }
    case CondKind::SideTactics: {
      std::vector<int> seen;
      for (const Object& o : w.objects)
        if (!o.removed && o.alive && o.side == side && o.tactic != -1 && std::find(seen.begin(), seen.end(), o.tactic) == seen.end()) seen.push_back(o.tactic);
      return cmp(c.cmp, int64_t(seen.size()), c.value);
    }
    case CondKind::SideMoney:
      return cmp(c.cmp, side >= 0 && side < int(w.side_state.size()) ? w.side_state[size_t(side)].money : 0, c.value);
    case CondKind::SideUnitsInRect: {
      int n = 0;
      for (const Object& o : w.objects) {
        if (o.removed || !o.alive || o.type < 0 || o.side != side) continue;
        if (script_rect_type(o.type) && in_rect(o)) n++;
      }
      return cmp(c.cmp, n, c.value);
    }
    case CondKind::SideUnitsOfUGIInRect:
    case CondKind::SideUnitsOfId: {
      int n = 0;
      for (const Object& o : w.objects)
        if (script_present(o) && o.side == side && o.type == c.ugi &&
            (c.kind == CondKind::SideUnitsOfId || (!o.removed && script_rect_type(o.type) && in_rect(o)))) n++;
      return cmp(c.cmp, n, c.value);
    }
    case CondKind::TacticValid:
    case CondKind::TacticAttacked:
    case CondKind::TacticIdle:
    case CondKind::TacticLocked: {
      std::vector<uint32_t> m = tactic_members(w, side, c.id);
      // V: an absent tactic does not satisfy idle/attacked/locked comparisons.
      if (m.empty() && c.kind != CondKind::TacticValid) return false;
      int64_t v = 0;
      if (c.kind == CondKind::TacticValid) v = m.empty() ? 0 : 1;
      else if (c.kind == CondKind::TacticAttacked) {
        v = -1;
        const auto hit = w.tactic_attacked_by.find({side, c.id});
        if (!m.empty() && hit != w.tactic_attacked_by.end()) v = hit->second;
      } else if (c.kind == CondKind::TacticIdle) {
        v = 1;
        for (uint32_t id : m) if (w.find(id)->order != uint8_t(Order::Idle)) v = 0;
        // A path assigned earlier in this action pass is already work, even
        // before follow_paths submits its first movement order.
        const auto it = tactics_.find({side, c.id});
        if (it != tactics_.end() && it->second.path >= 0) v = 0;
      } else {
        auto it = tactics_.find({side, c.id});
        v = it != tactics_.end() && it->second.locked ? 1 : 0;
      }
      return cmp(c.cmp, v, c.value);
    }
    case CondKind::DefenceValid: {
      int64_t v = 0;
      for (const Object& o : w.objects)
        if (!o.removed && o.alive && o.side == side && o.defence && c.id >= 0 && o.scenario == c.id && o.type >= 0 && type_info(o.type) && type_info(o.type)->kind == 7) v = 1;
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
    case ActKind::BoatActivate:
    case ActKind::JohnkaEnable:
    case ActKind::JohnkaDisable:
    case ActKind::SubmarineGo: {
      ObjectSource source = a.kind == ActKind::BoatActivate ? ObjectSource::Boat :
          a.kind == ActKind::SubmarineGo ? ObjectSource::Submarine : ObjectSource::Johnka;
      if (!sim.activate_vessel(w, a.id, source, a.kind != ActKind::JohnkaDisable))
        w.messages.push_back({"Vessel action: no living vessel for scenario " + std::to_string(a.id), 72});
      break;
    }
    case ActKind::TriggerEnable: set_enabled(a.id, true); break;
    case ActKind::TriggerDisable: set_enabled(a.id, false); break;
    case ActKind::TimerSet: {
      auto& timer = timers_[a.id];
      // V: setting the counter does not clear an existing timer's stop flag.
      timer.value = a.a;
      break;
    }
    case ActKind::TimerStart: { timers_[a.id].running = true; break; }
    case ActKind::TimerStop: timers_[a.id].running = false; break;
    case ActKind::TimerKill: timers_.erase(a.id); break;
    case ActKind::CenterView:
      // V: centerView uses tile origins; the view camera clamps its viewport.
      w.center_view = true;
      w.center_x = std::clamp(a.a, 0, std::max(0, w.terrain.w - 1)) * kTilePx;
      w.center_y = std::clamp(a.b, 0, std::max(0, w.terrain.h - 1)) * kTilePx;
      w.center_serial++;
      break;
    case ActKind::SideCameraActivate: {
      for (const SideDef& sd : w.mission.sides)
        if (sd.index == side && a.a >= 0 && a.a < int(sd.cameras.size())) {
          const auto& camera = sd.cameras[size_t(a.a)];
          // V: repeated activation creates another source; deactivate addresses the latest.
          for (auto& previous : w.script_cameras)
            if (previous.side == side && previous.index == a.a) previous.latest = false;
          const int x = std::clamp(camera[0], 0, std::max(0, w.terrain.w - 1)) * kTilePx + kTilePx / 2;
          const int y = std::clamp(camera[1], 0, std::max(0, w.terrain.h - 1)) * kTilePx + kTilePx / 2;
          // V: camera size selects one of the eight vision masks. Widen before adding.
          const int mask = int(std::clamp(int64_t(camera[2]) + 59, int64_t(64), int64_t(71)));
          w.script_cameras.push_back({side, a.a, x, y, mask, a.b});
        }
      break;
    }
    case ActKind::SideCameraDeactivate:
      for (auto& camera : w.script_cameras)
        if (camera.side == side && camera.index == a.a && camera.latest) camera.active = false;
      break;
    case ActKind::PrintString: {
      // V: the reference ring retains at most fifteen prints, with four-second deadlines.
      if (std::count_if(w.messages.begin(), w.messages.end(), [](const Message& m) { return m.seconds_left >= 0; }) >= 15) {
        const auto oldest = std::find_if(w.messages.begin(), w.messages.end(), [](const Message& m) { return m.seconds_left >= 0; });
        w.messages.erase(oldest);
      }
      w.messages.push_back({cp1250_to_utf8(a.text), 0, 4});
      break;
    }
    case ActKind::PrintStringR: break;  // V: parsed and discarded in this executable revision.
    case ActKind::SideVictory: case ActKind::SideDefeat: {
      uint8_t r = a.kind == ActKind::SideVictory ? kVictory : kDefeat;
      if (side >= 0 && side < 8 && w.side_outcome[side] == kRunning) w.side_outcome[side] = r;
      break;
    }
    case ActKind::SideAddMoney:
    case ActKind::SideTakeMoney:
      if (side >= 0 && side < int(w.side_state.size())) {
        auto& money = w.side_state[size_t(side)].money;
        const uint32_t amount = uint32_t(a.a);
        const uint32_t result = a.kind == ActKind::SideAddMoney ? uint32_t(money) + amount : uint32_t(money) - amount;
        money = std::max(0, int(std::bit_cast<int32_t>(result)));
      }
      break;
    case ActKind::SideSetFriendlyTo: case ActKind::SideSetNeutralTo: case ActKind::SideSetEnemyTo: {
      int other = resolve_side(w, a.a);
      if (side < 0 || side >= 8 || other < 0 || other >= 8 || side == other) break;
      uint8_t r = a.kind == ActKind::SideSetFriendlyTo ? kFriend : a.kind == ActKind::SideSetNeutralTo ? kNeutral : kEnemy;
      const uint32_t bit = uint32_t(1) << other;
      if (r == kEnemy) w.side_state[size_t(side)].enemy_mask |= bit;
      else w.side_state[size_t(side)].enemy_mask &= ~bit;
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
      sim.script_order(w, tactic_members(w, side, a.id), std::clamp(a.a, 0, w.terrain.w - 1) * kTilePx + kTilePx / 2, std::clamp(a.b, 0, w.terrain.h - 1) * kTilePx + kTilePx / 2, true);
      tactics_[{side, a.id}].path = -1;
      break;
    case ActKind::TacticJoinTactic:
      if (!tactic_members(w, side, a.a).empty())
        for (uint32_t id : tactic_members(w, side, a.id)) w.find_mut(id)->tactic = a.a;
      break;
    case ActKind::TacticSetPath: case ActKind::TacticSetInvertedPath: case ActKind::TacticSetCycledPath: case ActKind::TacticSetCycledInvertedPath: {
      TacticState& t = tactics_[{side, a.id}];
      t.path = a.a; t.path_index = 0;
      t.cycled = a.kind == ActKind::TacticSetCycledPath || a.kind == ActKind::TacticSetCycledInvertedPath;
      t.inverted = a.kind == ActKind::TacticSetInvertedPath || a.kind == ActKind::TacticSetCycledInvertedPath;
      break;
    }
    case ActKind::TacticLock:
      if (!tactic_members(w, side, a.id).empty()) tactics_[{side, a.id}].locked = true;
      break;
    case ActKind::TacticUnlock: {
      auto members = tactic_members(w, side, a.id);
      if (members.empty()) break;
      tactics_[{side, a.id}].locked = false;
      break;
    }
    case ActKind::TacticKill:
      for (uint32_t id : tactic_members(w, side, a.id)) sim.script_kill(w, id);
      break;
    case ActKind::TacticGiftToSide: {
      int to = resolve_side(w, a.a);
      if (to < 0 || to >= int(w.side_state.size())) break;
      auto members = tactic_members(w, side, a.id);
      if (members.empty()) break;
      const Object* first = w.find(members.front());
      const bool town_group = std::any_of(w.mission.towns.begin(), w.mission.towns.end(), [&](const TownDef& t) {
        return t.side == side && t.index == first->town && t.scenario_id == a.id;
      });
      if (town_group) {
        for (size_t i = 0; i < w.mission.towns.size(); ++i)
          if (w.mission.towns[i].index == first->town && w.mission.towns[i].side == side) {
            sim.transfer_town(w, int(i), to);
            tactics_[{side, a.id}].path = -1;
            tactics_[{to, a.id}] = TacticState{};
            break;
          }
      } else {
        // Allocate an independent receiving group, avoiding all existing identities.
        int group = 0;
        for (;;) {
          const bool used = std::any_of(w.objects.begin(), w.objects.end(), [&](const Object& o) {
            return o.side == to && o.tactic == group;
          }) || std::any_of(w.mission.towns.begin(), w.mission.towns.end(), [&](const TownDef& t) {
            return t.side == to && t.scenario_id == group;
          });
          if (!used) break;
          ++group;
        }
        sim.transfer_objects(w, members, to);
        for (uint32_t id : members) {
          auto* member = w.find_mut(id);
          member->tactic = group;
          member->town = -1;
        }
        tactics_[{to, group}].locked = false;
      }
      break;
    }
    default: // build/move commands remain unsupported
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
    sim.script_order(w, m, std::clamp(pts[size_t(k)].first, 0, w.terrain.w - 1) * kTilePx + kTilePx / 2, std::clamp(pts[size_t(k)].second, 0, w.terrain.h - 1) * kTilePx + kTilePx / 2, true);
    t.path_index = idx + 1;
  }
}

uint64_t ScriptRunner::state_hash() const {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
  mix(script_.triggers.size());
  for (const auto& trigger : script_.triggers) {
    mix(trigger.id); mix(trigger.conditions.size());
    for (const auto& c : trigger.conditions) {
      mix(uint32_t(c.kind)); mix(c.side); mix(c.id); mix(c.ugi);
      for (int coordinate : c.rect) mix(coordinate);
      mix(uint32_t(c.cmp)); mix(c.value);
    }
    mix(trigger.actions.size());
    for (const auto& a : trigger.actions) {
      mix(uint32_t(a.kind)); mix(a.side); mix(a.id); mix(a.a); mix(a.b); mix(a.ugi);
      mix(a.args.size()); for (int arg : a.args) mix(arg);
      mix(a.text.size()); for (unsigned char byte : a.text) mix(byte);
    }
  }
  mix(local_time_);
  if (loaded()) mix(elapsed_ticks_);
  mix(enabled_.size());
  for (size_t i = 0; i < enabled_.size(); ++i) { mix(enabled_[i]); mix(latched_[i]); }
  mix(timers_.size());
  for (const auto& t : timers_) { mix(uint64_t(t.first)); mix(uint64_t(t.second.value)); mix(t.second.running); }
  mix(tactics_.size());
  for (const auto& t : tactics_) {
    mix(uint64_t(t.first.first)); mix(uint64_t(t.first.second));
    mix(t.second.locked); mix(uint64_t(t.second.path + 1)); mix(uint64_t(t.second.path_index));
    mix(t.second.cycled); mix(t.second.inverted);
  }
  return h;
}

void ScriptRunner::tick(GameSim& sim, World& w) {
  // G: age existing sources before this tick's actions; native creation/update phase needs measurement.
  for (auto& camera : w.script_cameras)
    if (camera.active && camera.ticks_left > 0 && --camera.ticks_left == 0) camera.active = false;
  w.script_cameras.erase(std::remove_if(w.script_cameras.begin(), w.script_cameras.end(),
      [](const ScriptCamera& camera) { return !camera.active && !camera.latest; }), w.script_cameras.end());
  local_time_ = (local_time_ + 1) % 7200;
  ++elapsed_ticks_;
  // G: existing timers advance before triggers; pool ordering still needs measurement.
  for (auto& kv : timers_)
    if (kv.second.running) kv.second.value = std::bit_cast<int32_t>(uint32_t(kv.second.value) + 1u);
  for (size_t i = 0; i < script_.triggers.size(); ++i) {
    const Trigger& t = script_.triggers[i];
    if (!enabled_[i]) continue;
    if (latched_[i]) {
      auto& trace = trigger_traces_[i];
      if (!trace.executions) trace.first_tick = elapsed_ticks_;
      ++trace.executions;
      trace.last_tick = elapsed_ticks_;
      for (const Action& a : t.actions) {
        const int outcome_side = w.script_side;
        const bool valid_side = outcome_side >= 0 && outcome_side < 8;
        const auto before = valid_side ? w.side_outcome[outcome_side] : uint8_t(kRunning);
        run(sim, w, a);
        if (valid_side && before == kRunning && w.side_outcome[outcome_side] != kRunning)
          outcome_origin_ = ScriptOutcomeOrigin{t.id, a.line, std::max(sim.tick_count(), w.tick)};
      }
    }
    // Even a self-disabling action completes this update and re-latches.
    bool ok = !t.conditions.empty();
    for (const Condition& c : t.conditions) {
      if (c.kind == CondKind::Timer) timers_.try_emplace(c.id);
      if (!eval(w, c)) { ok = false; break; }
    }
    latched_[i] = ok;
  }
  follow_paths(sim, w);
  for (Message& m : w.messages) if (m.seconds_left < 0) m.ticks_left--;
  w.messages.erase(std::remove_if(w.messages.begin(), w.messages.end(), [](const Message& m) { return m.seconds_left < 0 && m.ticks_left <= 0; }), w.messages.end());
}

}  // namespace aztec::engine
