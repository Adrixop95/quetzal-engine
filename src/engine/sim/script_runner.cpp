#include "engine/sim/script_runner.hpp"

#include <algorithm>
#include <bit>
#include <limits>

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

// G: stable side-local numbers stand in for independently allocated controllers.
static int fresh_script_group(const World& w, int side) {
  for (int64_t group = 0; group <= std::numeric_limits<int>::max(); ++group) {
    const bool used = w.unit_tactics.contains({side, group}) || std::any_of(w.objects.begin(), w.objects.end(), [&](const Object& o) {
      return o.side == side && o.tactic == group;
    }) || std::any_of(w.mission.towns.begin(), w.mission.towns.end(), [&](const TownDef& town) {
      return town.side == side && town.scenario_id == group;
    });
    if (!used) return int(group);
  }
  return -1;
}

bool ScriptRunner::supports(ActKind kind) {
  switch (kind) {
    case ActKind::SideBuildCommand:
      return false;
    case ActKind::SideMoveCommand:
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
  local_time_ = std::clamp(w.mission.start_time, 0, 7199);
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
  // V: side +0x668 is the town-list length, independent of hall progress,
  // member count, scenario ID and diplomacy (0x417bb0, 0x446fe0).
  return int(std::count_if(w.mission.towns.begin(), w.mission.towns.end(),
      [side](const TownDef& town) { return town.side == side; }));
}

ScriptRunner::TacticLookup ScriptRunner::lookup_tactic(const World& w, int side, int tactic) const {
  TacticLookup result;
  const auto state = tactics_.find({side, tactic});
  // V: reserved living plain groups precede the side's town list.
  if (state == tactics_.end() || state->second.locked) {
    for (const auto& o : w.objects) {
      const bool town_member = std::any_of(w.mission.towns.begin(), w.mission.towns.end(),
          [&](const TownDef& town) { return town.side == side && town.index == o.town && town.scenario_id == o.tactic; });
      if (script_present(o) && o.side == side && !town_member && o.tactic == tactic && tactic >= 0)
        result.members.push_back(o.id);
    }
  }
  if (!result.members.empty()) return result;
  for (size_t i = 0; i < w.mission.towns.size(); ++i) {
    const auto& town = w.mission.towns[i];
    if (town.side != side || town.scenario_id != tactic) continue;
    result.town = int(i);
    for (const auto& o : w.objects)
      if (script_present(o) && o.side == side && o.town == town.index &&
          (o.tactic == town.scenario_id || o.town_hall)) result.members.push_back(o.id);
    break;
  }
  return result;
}

std::vector<uint32_t> ScriptRunner::tactic_members(const World& w, int side, int tactic) const {
  return lookup_tactic(w, side, tactic).members;
}

static bool tactic_busy(const World& w, const std::vector<uint32_t>& members) {
  for (auto id : members) {
    const auto* member = w.find(id);
    if (!member) continue;
    const auto group = w.unit_tactics.find(tactic_key(*member));
    if (group != w.unit_tactics.end() && !group->second.idle()) return true;
  }
  return false;
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
    // V: strict box overlap, with no extra tile on the maximum corner.
    // G: render_height supplies the native virtual height offset.
    const auto bounds = w.visibility_rules ? w.visibility_rules->type(o.type).combat_bounds : TypeRules{}.combat_bounds;
    const int64_t left = int64_t(std::min(c.rect[0], c.rect[2])) * kTilePx;
    const int64_t right = int64_t(std::max(c.rect[0], c.rect[2])) * kTilePx;
    const int64_t top = int64_t(std::min(c.rect[1], c.rect[3])) * kTilePx;
    const int64_t bottom = int64_t(std::max(c.rect[1], c.rect[3])) * kTilePx;
    const int64_t x = o.cur_x / 256, y = int64_t(o.cur_y / 256) - o.render_height;
    return left < right && top < bottom && x + bounds[0] < right && left < x + bounds[2] &&
        y + bounds[1] < bottom && top < y + bounds[3];
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
      const auto lookup = lookup_tactic(w, side, c.id);
      const auto& m = lookup.members;
      // V: an absent tactic does not satisfy idle/attacked/locked comparisons.
      if (m.empty() && lookup.town < 0 && c.kind != CondKind::TacticValid) return false;
      int64_t v = 0;
      if (c.kind == CondKind::TacticValid) v = m.empty() ? 0 : 1;
      else if (c.kind == CondKind::TacticAttacked) {
        v = -1;
        const auto hit = w.tactic_attacked_by.find({side, c.id});
        if (!m.empty() && hit != w.tactic_attacked_by.end()) v = hit->second;
      } else if (c.kind == CondKind::TacticIdle) {
        // V: script idle reads tactic movement state, independently of combat.
        v = tactic_busy(w, m) ? 0 : 1;
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
      // V: vessel lookup silently does nothing once no controller matches.
      sim.activate_vessel(w, a.id, source, a.kind != ActKind::JohnkaDisable);
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
      w.center_x = std::bit_cast<int32_t>(uint32_t(a.a) * uint32_t(kTilePx));
      w.center_y = std::bit_cast<int32_t>(uint32_t(a.b) * uint32_t(kTilePx));
      w.center_serial++;
      break;
    case ActKind::SideCameraActivate: {
      for (const SideDef& sd : w.mission.sides)
        if (sd.index == side && a.a >= 0 && a.a < int(sd.cameras.size())) {
          const auto& camera = sd.cameras[size_t(a.a)];
          // V: repeated activation creates another source; deactivate addresses the latest.
          for (auto& previous : w.script_cameras)
            if (previous.side == side && previous.index == a.a) previous.latest = false;
          const int x = std::bit_cast<int32_t>(uint32_t(camera[0]) * uint32_t(kTilePx) + kTilePx / 2);
          const int y = std::bit_cast<int32_t>(uint32_t(camera[1]) * uint32_t(kTilePx) + kTilePx / 2);
          // V: signed x86 addition followed by the eight-mask clamp.
          const int mask = std::clamp(std::bit_cast<int32_t>(uint32_t(camera[2]) + 59u), 64, 71);
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
    case ActKind::SideMoveCommand:
      // V: the native handler discards a read-only nearest-town lookup.
      break;
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
      const auto source = lookup_tactic(w, side, a.id);
      const auto& members = source.members;
      // V: the town move slot is a no-op, not an order to its recruits.
      if (source.town >= 0 || members.empty()) break;
      // V: first matching town controller, irrespective of alliance. Own towns
      // are ignored; a hall's object scenario ID is not the town scenario ID.
      const auto town = std::find_if(w.mission.towns.begin(), w.mission.towns.end(),
          [&](const TownDef& candidate) { return candidate.scenario_id == a.a; });
      if (town == w.mission.towns.end() || town->side == side) break;
      const size_t town_index = size_t(town - w.mission.towns.begin());
      const Object* hall = town_index < w.town_state.size() ? w.find(w.town_state[town_index].hall) : nullptr;
      // V: use the town's designated hall, not another hall-shaped member.
      if (!hall) break;
      const int x = (hall->cur_x / 256 / kTilePx) * kTilePx + kTilePx / 2;
      const int y = (hall->cur_y / 256 / kTilePx) * kTilePx + kTilePx / 2;
      sim.script_order(w, members, x, y, true);
      tactics_[{side, a.id}].path = -1;
      break;
    }
    case ActKind::TacticMoveTo: {
      const auto source = lookup_tactic(w, side, a.id);
      const auto& members = source.members;
      // V: the town move slot is a no-op, not an order to its recruits.
      if (source.town >= 0 || members.empty()) break;
      sim.script_order(w, members, std::clamp(a.a, 0, std::max(0, w.terrain.w - 1)) * kTilePx + kTilePx / 2,
          std::clamp(a.b, 0, std::max(0, w.terrain.h - 1)) * kTilePx + kTilePx / 2, true);
      tactics_[{side, a.id}].path = -1;
      break;
    }
    case ActKind::TacticJoinTactic: {
      auto source = lookup_tactic(w, side, a.id);
      auto destination = lookup_tactic(w, side, a.a);
      auto& target = destination.members;
      if (destination.town >= 0 && size_t(destination.town) < w.town_state.size()) {
        const auto hall = std::find(target.begin(), target.end(), w.town_state[size_t(destination.town)].hall);
        if (hall != target.end()) std::rotate(target.begin(), hall, hall + 1);
      }
      if (!source.members.empty() && !target.empty()) {
        if (source.town >= 0) {
          // V: the town script-join override recruits living non-buildings into
          // a new base group, then follows/merges that group (0x45c9d0).
          std::erase_if(source.members, [&](uint32_t id) {
            const auto* info = type_info(w.find(id)->type);
            return !info || info->is_structure();
          });
          if (source.members.empty()) break;
          const int group = fresh_script_group(w, side);
          if (group < 0) break;
          sim.script_stop(w, source.members);
          for (auto id : source.members) {
            auto& member = *w.find_mut(id);
            member.tactic = group; member.transient_tactic = 0; member.town = -1;
            member.attack_mode = AttackMode::Balanced;
          }
          tactics_[{side, group}] = TacticState{};
          tactics_[{side, group}].locked = false;
        }
        sim.script_join(w, source.members, target);
        tactics_[{side, a.id}].path = -1;
      }
      break;
    }
    case ActKind::TacticSetPath: case ActKind::TacticSetInvertedPath: case ActKind::TacticSetCycledPath: case ActKind::TacticSetCycledInvertedPath: {
      const auto source = lookup_tactic(w, side, a.id);
      if (source.members.empty()) break;
      sim.script_stop(w, source.members); // V: native path setters first cancel (+0xdc).
      // V: town path search returns false; appending waypoints cannot launch it.
      if (source.town >= 0) break;
      TacticState& t = tactics_[{side, a.id}];
      t.path = a.a; t.path_index = 0;
      // V: ordinary/inverted setters cancel the route but do not clear +0xe8.
      // Only the cycled variants explicitly set that persistent controller flag.
      t.cycled = t.cycled || a.kind == ActKind::TacticSetCycledPath || a.kind == ActKind::TacticSetCycledInvertedPath;
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
      const auto source = lookup_tactic(w, side, a.id);
      const auto& members = source.members;
      if (source.town >= 0) {
        sim.transfer_town(w, source.town, to);
        tactics_[{side, a.id}].path = -1;
        // A preexisting reserved receiving group with this ID keeps priority.
        tactics_.try_emplace({to, a.id});
      } else {
        if (members.empty()) break;
        const int group = fresh_script_group(w, to);
        if (group < 0) break;
        sim.script_gift_group(w, members, to, group, a.id);
        tactics_[{side, a.id}].path = -1;
        tactics_[{to, group}] = TacticState{};
        tactics_[{to, group}].locked = false;
      }
      break;
    }
    default: // General side build plans remain unsupported.
      unsupported_++;
      break;
  }
}

void ScriptRunner::follow_paths(GameSim& sim, World& w) {
  for (auto& kv : tactics_) {
    TacticState& t = kv.second;
    if (t.path < 0) continue;
    if (t.path >= int(w.mission.paths.size()) || w.mission.paths[size_t(t.path)].empty()) {
      // G: invalid native paths are fatal diagnostics; safely cancel in the engine.
      t.path = -1;
      continue;
    }
    const auto& pts = w.mission.paths[size_t(t.path)];
    const auto m = tactic_members(w, kv.first.first, kv.first.second);
    if (m.empty()) { t.path = -1; continue; }
    if (tactic_busy(w, m)) continue;
    const auto group = w.unit_tactics.find(tactic_key(*w.find(m.front())));
    if (group == w.unit_tactics.end()) continue;
    const int n = int(pts.size());
    // V: consume waypoints equal to the shared anchor, not a member position.
    // A stopped/failed route by itself does not complete a waypoint (0x453570).
    int skipped = 0;
    while (skipped < n) {
      if (t.path_index >= n) {
        if (!t.cycled) { t.path = -1; break; }
        t.path_index = 0;
      }
      const int k = t.inverted ? n - 1 - t.path_index : t.path_index;
      const auto [x, y] = pts[size_t(k)];
      if (group->second.x != x || group->second.y != y) break;
      ++t.path_index;
      ++skipped;
    }
    if (skipped == n || (!t.cycled && t.path_index >= n)) t.path = -1;
    if (t.path < 0) continue;
    if (t.path_index >= n) t.path_index = 0;
    const int k = t.inverted ? n - 1 - t.path_index : t.path_index;
    sim.script_order(w, m, std::clamp(pts[size_t(k)].first, 0, std::max(0, w.terrain.w - 1)) * kTilePx + kTilePx / 2,
        std::clamp(pts[size_t(k)].second, 0, std::max(0, w.terrain.h - 1)) * kTilePx + kTilePx / 2, true);
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
  local_time_ = (local_time_ + 1) % 7200;
  // V: lazy timers prepend to the shared pool, ahead of the loaded triggers.
  // A timer created during this pass first advances on the next update.
  for (auto& kv : timers_)
    if (kv.second.running) kv.second.value = std::bit_cast<int32_t>(uint32_t(kv.second.value) + 1u);
  for (size_t i = 0; i < script_.triggers.size(); ++i) {
    const Trigger& t = script_.triggers[i];
    if (!enabled_[i]) continue;
    if (latched_[i]) {
      auto& trace = trigger_traces_[i];
      if (!trace.executions) trace.first_tick = elapsed_ticks_ + 1;
      ++trace.executions;
      trace.last_tick = elapsed_ticks_ + 1;
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
  ++elapsed_ticks_; // V: single-player globalTime advances after trigger evaluation.
  // V: visual-object updates follow the script pool, including newly created cameras.
  for (auto& camera : w.script_cameras)
    if (camera.active && camera.ticks_left > 0 && --camera.ticks_left == 0) camera.active = false;
  w.script_cameras.erase(std::remove_if(w.script_cameras.begin(), w.script_cameras.end(),
      [](const ScriptCamera& camera) { return !camera.active && !camera.latest; }), w.script_cameras.end());
  follow_paths(sim, w);
  for (Message& m : w.messages) if (m.seconds_left < 0) m.ticks_left--;
  w.messages.erase(std::remove_if(w.messages.begin(), w.messages.end(), [](const Message& m) { return m.seconds_left < 0 && m.ticks_left <= 0; }), w.messages.end());
}

}  // namespace aztec::engine
