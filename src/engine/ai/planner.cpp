#include "engine/ai/planner.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include "engine/world/world.hpp"

namespace aztec::engine::ai {
namespace {
bool live(const Object& o) { return o.alive && !o.removed && o.type >= 0; }
bool near(const TownDef& a, const TownDef& b) { return std::abs(a.x - b.x) < 8 && std::abs(a.y - b.y) < 8; }
int count(const World& w, int town, int type) {
  int n = 0;
  for (const auto& o : w.objects) if (live(o) && o.town == town && o.type == type) ++n;
  return n;
}
int64_t income(const World& w, const TownState& s, int town) {
  int towns = 0, side = w.mission.towns[town].side;
  for (const auto& t : w.mission.towns) if (t.side == side) ++towns;
  // G: observed average harvest substitutes for the unresolved instantaneous estimate.
  return (s.harvested * 256 + int64_t(w.side_state[side].money / std::max(1, towns)) * 256) / int64_t(s.elapsed + 1);
}
}

bool visible(const World& w, int observer, uint32_t id) {
  const auto* o = w.find(id);
  if (!o || !live(*o)) return false;
  if (o->side == observer) return true;
  if (observer < 0 || observer >= int(w.visibility.sides.size())) return false;
  const auto& l = w.visibility.sides[observer];
  int x = (o->cur_x >> 8) / kTilePx, y = (o->cur_y >> 8) / kTilePx;
  return x >= 0 && y >= 0 && x < l.w && y < l.h &&
         size_t(y * l.w + x) < l.v.size() && l.v[size_t(y * l.w + x)] <= 24;
}

std::vector<Intelligence> intelligence(const World& w, const RulesData& rules, int observer) {
  std::vector<Intelligence> records;
  for (size_t ti = 0; ti < w.mission.towns.size(); ++ti) {
    const auto& t = w.mission.towns[ti];
    bool seen = false;
    for (const auto& o : w.objects)
      if (live(o) && o.town == int(ti) && visible(w, observer, o.id)) seen = true;
    if (!seen) continue;
    Intelligence record; record.town = int(ti);
    for (const auto& o : w.objects) {
      if (!live(o) || o.type >= 90 || o.town < 0 || o.town >= int(w.mission.towns.size())) continue;
      if (!near(t, w.mission.towns[o.town])) continue;
      if (t.side == observer) {
        if (o.side != observer || rules.type(o.type).kind != 2) continue;
      } else if (!visible(w, observer, o.id)) continue;
      ++record.counts[o.type];
    }
    records.push_back(record);
  }
  return records;
}

int development(const World& w, const RulesData& rules, int town) {
  int result = 0;
  for (const auto& o : w.objects)
    if (live(o) && o.town == town && rules.type(o.type).kind == 2) result += rules.type(o.type).dev_level;
  for (const auto& s : w.side_state) for (const auto& q : s.queue)
    if (q.town == town && rules.type(q.type).kind == 2) result += rules.type(q.type).dev_level;
  return std::clamp(result, 0, 9);
}

int build_speed(const World& w, const TownState& state, int town) {
  return income(w, state, town) < 2 * (state.spent * 256 / int64_t(state.elapsed + 1)) ? 1 : 2;
}

Decision step(const World& w, const RulesData& rules, int town, const TownState& state,
              const std::vector<Offer>& offers) {
  Decision d; d.next = state;
  if (town < 0 || town >= int(w.mission.towns.size())) return d;
  const auto& t = w.mission.towns[town];
  if (t.side < 0 || t.side >= int(w.side_state.size())) return d;
  auto& s = d.next;
  ++s.elapsed;
  if (s.science_timer >= 0 && s.science_timer < std::numeric_limits<int>::max()) ++s.science_timer;
  uint32_t anchor = 0;
  bool busy = false;
  int forces = 0, defences = 0;
  std::vector<uint32_t> available;
  const Object* attacker = nullptr;
  for (const auto& o : w.objects) {
    if (!live(o) || o.town != town || o.side != t.side) continue;
    const auto& r = rules.type(o.type);
    if (r.structure && !anchor) anchor = o.id;
    busy |= o.progress < 1000;
    if (r.kind == 7) ++defences; else ++forces;
    // G: autonomous groups contain only unassigned combat units. Scripts retain their tactics.
    if ((r.mobile || r.flier) && r.armed && o.tactic < 0 && o.order == 0) available.push_back(o.id);
    if (r.structure && o.last_hit_by) {
      const auto* a = w.find(o.last_hit_by);
      if (a && live(*a) && w.hostile(t.side, a->side) && visible(w, t.side, a->id) &&
          std::abs((a->cur_x >> 8) / kTilePx - t.x) < 8 &&
          std::abs((a->cur_y >> 8) / kTilePx - t.y) < 8 && (!attacker || a->id < attacker->id)) attacker = a;
    }
  }
  if (!anchor) return d;  // A destroyed town cannot issue purchases through a stale mission record.
  auto command = [&](Pct kind) { Command c; c.type = kind; c.side = uint8_t(t.side); return c; };
  int speed = build_speed(w, state, town);
  if (town >= int(w.town_speed.size()) || w.town_speed[town] != speed) {
    auto c = command(Pct::BuildMode); c.a = speed; c.ids = {anchor}; d.commands.push_back(c);
  }
  auto records = intelligence(w, rules, t.side);
  if (state.elapsed == 0 || s.elapsed % uint64_t((t.side + 1) * 1000) == 0) {
    s.known_towns.clear();
    for (const auto& r : records) s.known_towns.push_back(r.town);
  }
  auto enemy = [&](int ti) {
    return ti >= 0 && ti < int(w.mission.towns.size()) && w.mission.towns[ti].type != 0 &&
           w.hostile(t.side, w.mission.towns[ti].side) &&
           std::find(s.known_towns.begin(), s.known_towns.end(), ti) != s.known_towns.end() &&
           std::any_of(w.objects.begin(), w.objects.end(), [&](const auto& o) { return live(o) && o.town == ti; });
  };
  if (!enemy(s.target_town)) {
    s.target_town = -1;
    for (int ti : s.known_towns) if (enemy(ti)) { s.target_town = ti; break; }
  }
  if (attacker && s.last_alert != attacker->id) {
    // G: redirect eligible autonomous town troops, including moving troops, on a new alert.
    auto c = command(Pct::Attack); c.a = int(attacker->id);
    for (const auto& o : w.objects)
      if (live(o) && o.town == town && o.side == t.side && o.tactic < 0 &&
          (rules.type(o.type).mobile || rules.type(o.type).flier) && rules.type(o.type).armed &&
          rules.attack(o.type, attacker->type).damage > 0) c.ids.push_back(o.id);
    if (!c.ids.empty()) d.commands.push_back(c);
    s.last_alert = attacker->id; s.phase = 16; s.remaining = 0;
  }
  if (!attacker) s.last_alert = 0;
  for (const auto& q : w.side_state[t.side].queue) busy |= q.town == town;
  if (busy || w.side_state[t.side].build_paused) return d;
  const int dev = development(w, rules, town);
  bool poor = income(w, state, town) < 2 * (state.spent * 256 / int64_t(state.elapsed + 1));
  auto buy = [&](const Offer* o) {
    if (!o) return false;
    auto c = command(Pct::Build); c.a = o->type; c.b = o->x; c.c = o->y; c.ids = {anchor};
    d.commands.push_back(c); return true;
  };
  auto find = [&](int type) -> const Offer* {
    for (const auto& o : offers) if (o.type == type && o.placeable) return &o;
    return nullptr;
  };
  auto economy = [&]() {
    static constexpr int farms[] = {12, 39, 65}, warehouses[] = {18, 40, 70}, mines[] = {13, 48, 66};
    int race = std::clamp(t.race, 0, 2);
    if (count(w, town, farms[race]) > 2 && !count(w, town, warehouses[race]) && buy(find(warehouses[race]))) return true;
    return buy(find(farms[race])) || buy(find(mines[race]));
  };
  // G: select cheapest available combat/development item, breaking ties by UGI id.
  auto choose = [&](int category) -> const Offer* {
    const Offer* best = nullptr;
    for (const auto& o : offers) {
      const auto& r = rules.type(o.type);
      bool match = category == 0 ? ((r.mobile || r.flier) && r.armed) :
                   category == 1 ? r.kind == 7 : r.kind == 2 && r.dev_level > 0;
      if (o.placeable && match && (!best || o.price < best->price || (o.price == best->price && o.type < best->type))) best = &o;
    }
    return best;
  };
  auto progress = [&](int done) {
    if (poor && economy()) return;
    if (s.remaining > 0 && buy(choose(0))) { --s.remaining; return; }
    s.remaining = 0; s.phase = done;
  };
  switch (s.phase) {
    case 0: s.phase = 10; break;
    case 1: s.remaining = std::max(0, 3 * (dev + 1) - int(available.size())); s.phase = enemy(s.target_town) ? 2 : 11; break;
    case 2:
      if (s.remaining > 0) { progress(2); break; }
      if (enemy(s.target_town) && !available.empty()) {
        auto c = command(Pct::AttackMove); c.ids = available;
        c.a = w.mission.towns[s.target_town].x * kTilePx + kTilePx / 2;
        c.b = w.mission.towns[s.target_town].y * kTilePx + kTilePx / 2;
        d.commands.push_back(c);
      }
      s.phase = 11; break;
    case 3: if (s.science_timer > 0 && buy(choose(2))) s.phase = 4; else { s.science_timer = 0; s.phase = 13; } break;
    case 4: s.phase = 13; break;
    case 5: case 18:
      if (buy(choose(1))) s.phase = s.phase == 5 ? 6 : 19;
      else {
        // Only a failed placement disables attempts; lack of funds is temporary.
        for (const auto& o : offers) if (rules.type(o.type).kind == 7 && !o.placeable) s.defence_enabled = false;
        s.phase = s.phase == 5 ? 0 : 16;
      }
      break;
    case 6: case 8: s.phase = 11; break;
    case 7: economy(); s.phase = 8; break;
    case 9: s.phase = enemy(s.target_town) ? 1 : 11; break;
    case 10: s.phase = s.science_timer <= 100 * (dev + 1) ? 13 : 3; break;
    case 11: s.phase = poor ? 7 : 12; break;
    case 12: s.phase = !s.defence_enabled ? 11 : defences < dev ? 5 : 0; break;
    case 13: s.phase = forces < 3 * (dev + 1) ? 15 : 9; break;
    case 14: progress(9); break;
    case 15: s.remaining = std::max(0, 3 * (dev + 1) - forces); s.phase = choose(0) ? 14 : 9; break;
    case 16: if (attacker) s.phase = 21; else { s.remaining = 0; s.phase = 0; } break;
    case 17: s.phase = 21; break;
    case 19: s.phase = 16; break;
    case 20: s.phase = s.defence_enabled && defences < dev ? 18 : 16; break;
    case 21: s.phase = forces < 7 * (dev + 1) ? 23 : 20; break;
    case 22: progress(20); break;
    case 23: s.remaining = std::max(0, 7 * (dev + 1) - forces); s.phase = choose(0) ? 22 : 20; break;
    default: s.phase = 0; break;
  }
  return d;
}
}  // namespace aztec::engine::ai
