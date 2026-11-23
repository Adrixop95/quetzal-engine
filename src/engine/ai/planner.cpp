#include "engine/ai/planner.hpp"
#include "engine/sim/random.hpp"
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <limits>
#include "engine/world/world.hpp"
#include "engine/world/visibility.hpp"

namespace aztec::engine::ai {
namespace {
bool live(const Object& o) { return o.alive && !o.removed && o.type >= 0; }
bool computer(const World& w, int side) {
  return std::any_of(w.mission.sides.begin(), w.mission.sides.end(),
                     [side](const auto& s) { return s.index == side && s.only_comp; });
}
bool near(const TownDef& a, const TownDef& b) { return std::abs(a.x - b.x) < 8 && std::abs(a.y - b.y) < 8; }
int count(const World& w, int town, int type) {
  int n = 0;
  for (const auto& o : w.objects) if (live(o) && o.town == w.mission.towns[town].index && o.side == w.mission.towns[town].side && o.type == type) ++n;
  return n;
}
int64_t income(const World& w, const RulesData& rules, const TownState& s, int town) {
  int towns = 0, side = w.mission.towns[town].side;
  for (const auto& t : w.mission.towns) if (t.side == side) ++towns;
  // V: current shared cash per town / elapsed time, plus instantaneous producer yield.
  int64_t rate = 0;
  const auto& td = w.mission.towns[size_t(town)];
  bool warehouse = false;
  for (const auto& o : w.objects) if (live(o) && o.side == side && o.town == td.index && o.progress == 1000)
    warehouse |= o.type == 18 || o.type == 40 || o.type == 70;
  for (const auto& o : w.objects) {
    if (!live(o) || o.side != side || o.town != td.index || o.progress < 1000) continue;
    const auto& h = rules.type(o.type).harvest;
    if (h.period == -1 || !rules.type(o.type).structure) continue;
    const int ca = int(((int64_t(h.rate_a) * 65536 / 100) * 2 + 256) >> 8);
    const int cb = int(((int64_t(h.rate_b) * 65536 / 100) * 2 + 256) >> 8);
    int64_t sum = 0; int cells = 0;
    for (int cell = 0; cell < 49; ++cell) if (h.occupancy[cell] == 2) {
      const int x = (o.cur_x >> 8) / kTilePx + cell % 7 - 3;
      const int y = (o.cur_y >> 8) / kTilePx + cell / 7 - 3;
      const uint16_t v = w.resources.in(x, y) ? w.resources.at(x, y) : 0;
      sum += 2 * (v & 255) * ca + 2 * (v >> 8) * cb; ++cells;
    }
    int64_t yield = cells ? (sum / cells) >> 8 : 0;
    if (warehouse && (o.type == 12 || o.type == 39 || o.type == 65)) yield = yield * 11184811 / 8388608;
    rate += yield;
  }
  return rate + int64_t(w.side_state[side].money / std::max(1, towns)) * 256 / int64_t(s.elapsed + 1);
}
}

int matchup(const RulesData& rules, int attacker, int target) {
  // V: plan strength is the complement of kill time, in 8.8 units.
  const auto& a = rules.type(attacker);
  const auto& b = rules.type(target);
  if (a.structure) return 1;
  const int ticks = estimated_kill_time(a, b.hp_max, b.secondary_hp, rules.attack(attacker, target));
  if (ticks < 0 || ticks >= 256) return b.structure ? 1 : 0;
  return 255 - ticks;
}

int target_score(const World& w, const RulesData& rules, int town, const Intelligence& record) {
  if (town < 0 || town >= int(w.mission.towns.size()) || record.town < 0 ||
      record.town >= int(w.mission.towns.size())) return 0x7fff00;
  const auto& source = w.mission.towns[size_t(town)];
  const auto& target = w.mission.towns[size_t(record.town)];
  int max_hp = 1;
  for (int type = 0; type < 90; ++type) max_hp = std::max(max_hp, rules.type(type).hp_max);
  int64_t count = 0, weight = 0;
  for (int type = 0; type < 90; ++type) if (!rules.type(type).structure) {
    const int n = std::max(0, record.counts[size_t(type)]);
    count += n;
    weight += int64_t(n) * (int64_t(rules.type(type).hp_max) * 256 / max_hp);
  }
  int64_t distance = combat_distance(source.x - target.x, source.y - target.y) * 256 /
      std::max<int64_t>(1, combat_distance(w.terrain.w, w.terrain.h));
  if (computer(w, target.side) && !w.enemy_all(target.side)) distance *= 3;
  return int(distance + (count ? weight / count / 2 : 0) + std::min<int64_t>(count, 256) / 2);
}

std::vector<PlanEntry> make_plan(const World& w, const RulesData& rules, int town,
                                 int target, bool offensive, uint32_t& random, const std::vector<Intelligence>& knowledge,
                                 int* wanted_type) {
  std::vector<PlanEntry> plan;
  if (town < 0 || target < 0 || town >= int(w.mission.towns.size()) || target >= int(w.mission.towns.size())) return plan;
  const auto& source = w.mission.towns[size_t(town)];
  std::array<int, 90> demand{}, allocated{};
  for (const auto& record : knowledge)
    if (record.town == target) demand = record.counts;
  // V: own intelligence describes buildings; opposing intelligence includes units.
  int64_t limit = 0;
  int level = 0, max_hp = 1;
  for (int n : demand) limit += n;
  for (size_t ti = 0; ti < w.mission.towns.size(); ++ti)
    if (w.mission.towns[ti].side == source.side) level = std::max(level, development(w, rules, int(ti)));
  for (int type = 0; type < 90; ++type) max_hp = std::max(max_hp, rules.type(type).hp_max);
  std::array<std::vector<uint32_t>, 90> reusable;
  if (!w.enemy_all(source.side)) {
    // V: offensive plans can reuse nearby town troops.
    // V: the reuse radius is measured from the requesting town to the candidate tactic.
    // G: script tactic eligibility has no complete adapter yet; preserve explicit script groups.
    for (const auto& o : w.objects) {
      if (!live(o) || o.type >= 90 || o.side != source.side || rules.type(o.type).structure || o.tactic < -1) continue;
      const auto home = std::find_if(w.mission.towns.begin(), w.mission.towns.end(),
          [&](const auto& td) { return td.side == o.side && td.index == o.town; });
      if (home != w.mission.towns.end() && (!offensive || (o.tactic >= 0 && o.tactic != home->scenario_id))) continue;
      if (home == w.mission.towns.end() && o.tactic >= 0) continue;
      const int x = home == w.mission.towns.end() ? (o.cur_x >> 8) / kTilePx : home->x;
      const int y = home == w.mission.towns.end() ? (o.cur_y >> 8) / kTilePx : home->y;
      if (combat_distance(source.x - x, source.y - y) <= 25 && reusable[size_t(o.type)].size() < 32)
        reusable[size_t(o.type)].push_back(o.id);
    }
  }
  auto draw = [&]() {
    // G: reproducible isolated CRT sequence; shared game RNG call order remains unresolved.
    return int(msvc_rand(random));
  };
  // V: ordinary new counters test the source town; the airborne pass uses side availability.
  auto buildable = [&](int type, bool side_wide = false, int* missing = nullptr) {
    for (size_t ti = 0; ti < w.mission.towns.size(); ++ti) {
      const auto& td = w.mission.towns[ti];
      if (td.side != source.side || (!side_wide && ti != size_t(town))) continue;
      for (const auto& item : rules.build_list(td.race, td.type)) if (item.type == type) {
        bool ok = true;
        for (const auto& p : item.prereq) {
          bool present = false;
          for (const auto& o : w.objects)
            if (live(o) && o.side == source.side && o.type == p.type && o.progress == 1000 &&
                (p.side_wide || o.town == td.index)) present = true;
          ok &= present;
          if (!present && missing) *missing = p.type;
        }
        for (const auto& sd : w.mission.sides) if (sd.index == source.side)
          ok &= std::find(sd.disabled.begin(), sd.disabled.end(), type) == sd.disabled.end();
        if (ok) return true;
      }
    }
    return false;
  };
  auto append = [&](int type) {
    if (plan.size() >= 256) return false; // V: plan storage capacity, not a population cap
    PlanEntry e; e.type = type;
    if (!reusable[size_t(type)].empty()) {
      e.object = reusable[size_t(type)].back(); reusable[size_t(type)].pop_back();
    }
    plan.push_back(e); ++allocated[size_t(type)]; return true;
  };
  std::vector<int> candidates;
  for (int type = 89; type >= 0; --type)
    if (!rules.type(type).structure && rules.type(type).dev_level <= level) candidates.push_back(type);
  // V: visit ordinary targets from high type to low type; new counters use HP weights
  // and random 0..63, while reusable counters compete using their unweighted score.
  for (int target_type = 89; target_type >= 0 && int(plan.size()) < limit; --target_type) {
    if (!demand[size_t(target_type)]) continue;
    for (int round = 0; round < 256 && plan.size() < 256; ++round) {
      int best = -1, best_score = 0;
      bool best_reuse = false;
      for (int type : candidates) {
        if (type == 33) continue;
        int strength = matchup(rules, type, target_type);
        if (!strength) continue;
        const bool reuse = !reusable[size_t(type)].empty();
        const int score = reuse ? strength : int(int64_t(rules.type(type).hp_max) * 256 / max_hp) * strength / 256 + (draw() & 0x3f);
        if (reuse) {
          if (best < 0 || strength >= matchup(rules, best, target_type)) {
            best = type; best_score = strength; best_reuse = true;
          }
        } else if (score >= best_score) {
          int missing = 90;
          if (buildable(type, false, &missing)) {
            if (best < 0 || score > best_score) { best = type; best_score = score; best_reuse = false; }
          } else if (wanted_type && missing < 90 && score > 0) {
            // V: 0x44d701 leaves the wanted-score bar at zero. Last eligible failure wins.
            *wanted_type = type;
          }
        }
      }
      if (best < 0) break;
      int64_t residual = 2LL * demand[size_t(target_type)] * matchup(rules, target_type, best);
      for (int type = 0; type < 90; ++type) residual -= int64_t(allocated[size_t(type)]) * matchup(rules, type, target_type);
      if (residual <= 0) break;
      const int batch = draw() % (level + 1) + 1;
      for (int i = 0; i < batch && (residual > 0 || int(plan.size()) < limit); ++i) {
        // Exhaustion returns to candidate choice instead of silently buying an unbuildable type.
        if (best_reuse && reusable[size_t(best)].empty()) break;
        if (!append(best)) break;
        residual -= matchup(rules, best, target_type);
      }
      if (residual <= 0 || int(plan.size()) >= limit) break;
    }
  }
  // V: an additional target-class pass uses a 0..31 perturbation.
  // G: map the native virtual target predicate to the flier classification.
  for (int target_type = 0; target_type < 90; ++target_type) {
    if (!rules.type(target_type).flier || !demand[size_t(target_type)]) continue;
    int best = -1, best_score = 0;
    for (int type : candidates) if (type != 33 && buildable(type, true)) {
      const int strength = matchup(rules, type, target_type);
      if (strength > best_score) { best = type; best_score = strength + draw() % 32; }
    }
    if (best >= 0) for (int n = 0; n < demand[size_t(target_type)]; ++n) if (!append(best)) break;
  }
  return plan;
}

bool visible(const World& w, int observer, uint32_t id) {
  const auto* o = w.find(id);
  if (!o || !live(*o)) return false;
  if (o->side == observer) return true;
  if (w.visibility_brightness) return object_visible(w, observer, *o);
  // Legacy computer intelligence has its own initial darkness threshold.
  const auto* l = visibility_layer(w, observer);
  const int x = o->cur_x >> 8, y = o->cur_y >> 8;
  return l && x >= 0 && y >= 0 && x < w.width_px() && y < w.height_px() &&
      l->v[size_t(y / kTilePx) * l->w + x / kTilePx] <= 24;
}

std::vector<Intelligence> intelligence(const World& w, const RulesData& rules, int observer) {
  std::vector<Intelligence> records;
  for (size_t ti = 0; ti < w.mission.towns.size(); ++ti) {
    const auto& t = w.mission.towns[ti];
    bool seen = false;
    for (const auto& o : w.objects)
      if (live(o) && o.town == t.index && o.side == t.side && visible(w, observer, o.id)) seen = true;
    if (!seen) continue;
    Intelligence record; record.town = int(ti);
    for (const auto& o : w.objects) {
      if (!live(o) || o.type >= 90) continue;
      const auto home = std::find_if(w.mission.towns.begin(), w.mission.towns.end(),
                                     [&](const auto& td) { return td.index == o.town && td.side == o.side; });
      if (home == w.mission.towns.end() || o.tactic < -1) continue;
      if (home == w.mission.towns.begin() + int(ti)) {
        if (t.side == observer && rules.type(o.type).kind != 2) continue;
      } else if (home->side == observer || !near(t, *home)) continue;
      // V: after one member reveals a town, count its live membership, not only visible members.
      ++record.counts[o.type];
    }
    records.push_back(record);
  }
  return records;
}

int development(const World& w, const RulesData& rules, int town) {
  int result = 0;
  for (const auto& o : w.objects)
    if (live(o) && o.town == w.mission.towns[town].index && o.side == w.mission.towns[town].side && rules.type(o.type).kind == 2) result += rules.type(o.type).dev_level;
  for (const auto& s : w.side_state) for (const auto& q : s.queue)
    if (q.town == town && rules.type(q.type).kind == 2 &&
        (!q.object || !w.find(q.object) || !live(*w.find(q.object)))) result += rules.type(q.type).dev_level;
  return std::clamp(result, 0, 9);
}

int build_speed(const World& w, const RulesData& rules, const TownState& state, int town) {
  return income(w, rules, state, town) < 2 * (state.spent * 256 / int64_t(state.elapsed + 1)) ? 1 : 2;
}

Decision step(const World& w, const RulesData& rules, int town, const TownState& state,
              const std::vector<Offer>& offers) {
  Decision d; d.next = state;
  if (town < 0 || town >= int(w.mission.towns.size())) return d;
  const auto& t = w.mission.towns[town];
  if (t.side < 0 || t.side >= int(w.side_state.size())) return d;
  auto& s = d.next;
  ++s.elapsed;
  auto finish = [&]() {
    // V: the phase sees the old counter; increment follows its handler, even while busy.
    if (s.science_timer >= 0 && s.science_timer < std::numeric_limits<int>::max()) ++s.science_timer;
    return d;
  };
  uint32_t anchor = 0;
  bool busy = false;
  int forces = 0, defences = 0;
  std::vector<uint32_t> available;
  const Object* attacker = nullptr;
  for (const auto& o : w.objects) {
    if (!live(o) || o.town != t.index || o.side != t.side || o.tactic < -1) continue;
    const auto& r = rules.type(o.type);
    if (r.structure && !anchor) anchor = o.id;
    busy |= o.progress < 1000;
    if (r.kind == 7) ++defences; else ++forces;
    // G: last-hit alerts supplement the documented proximity threat cycle.
    if (r.structure && o.last_hit_by) {
      const auto* a = w.find(o.last_hit_by);
      if (a && live(*a) && w.hostile(t.side, a->side) && visible(w, t.side, a->id) &&
          std::abs((a->cur_x >> 8) / kTilePx - t.x) < 8 &&
          std::abs((a->cur_y >> 8) / kTilePx - t.y) < 8 && (!attacker || a->id < attacker->id)) attacker = a;
    }
  }
  if (!anchor) return finish();  // A destroyed town cannot issue purchases through a stale mission record.
  auto command = [&](Pct kind) { Command c; c.type = kind; c.side = uint8_t(t.side); return c; };
  const bool queued = std::any_of(w.side_state[t.side].queue.begin(), w.side_state[t.side].queue.end(),
                                  [town](const auto& q) { return q.town == town; });
  int speed = state.phase == 3 && !busy && !queued ? 1 : build_speed(w, rules, state, town);
  if (town >= int(w.town_speed.size()) || w.town_speed[town] != speed) {
    auto c = command(Pct::BuildMode); c.a = speed; c.ids = {anchor}; d.commands.push_back(c);
  }
  auto records = intelligence(w, rules, t.side);
  // V: hidden towns retain their last intelligence until the periodic rebuild.
  if (state.elapsed == 0 || s.elapsed % uint64_t((t.side + 1) * 1000) == 0) s.knowledge.clear();
  for (const auto& record : records) {
    auto previous = std::find_if(s.knowledge.begin(), s.knowledge.end(), [&](const auto& r) { return r.town == record.town; });
    if (previous == s.knowledge.end()) s.knowledge.push_back(record);
    else *previous = record;
  }
  if (!s.knowledge.empty()) {
    s.known_towns.clear();
    for (const auto& record : s.knowledge) s.known_towns.push_back(record.town);
  }
  auto enemy = [&](int ti) {
    return ti >= 0 && ti < int(w.mission.towns.size()) &&
           w.mission.towns[ti].side != t.side && w.relation[t.side][w.mission.towns[ti].side] != kFriend &&
           std::find(s.known_towns.begin(), s.known_towns.end(), ti) != s.known_towns.end() &&
           std::any_of(w.objects.begin(), w.objects.end(), [&](const auto& o) { return live(o) && o.town == w.mission.towns[ti].index && o.side == w.mission.towns[ti].side; });
  };
  if (s.phase < 16) {
    s.target_town = -1;
    int best_score = std::numeric_limits<int>::max();
    for (int ti : s.known_towns) if (enemy(ti)) {
      Intelligence record; record.town = ti;
      for (const auto& r : s.knowledge) if (r.town == ti) record = r;
      const int score = target_score(w, rules, town, record);
      if (score < best_score) { best_score = score; s.target_town = ti; }
    }
  }
  bool entered_threat = false, foreign_nearby = false;
  const int radius = t.type != 0 ? 8 : 4;
  for (const auto& o : w.objects) {
    if (!live(o) || o.side < 0 || o.side >= int(w.side_state.size()) || o.side == t.side) continue;
    const int64_t dx = (o.cur_x >> 8) / kTilePx - t.x, dy = (o.cur_y >> 8) / kTilePx - t.y;
    const int64_t distance = dx * dx + dy * dy;
    foreign_nearby |= distance <= 64; // V: the phase-16 exit check includes allies.
    if (computer(w, t.side) && !computer(w, o.side) && w.relation[t.side][o.side] != kFriend &&
        distance <= radius * radius && s.phase < 16) entered_threat = true;
  }
  if (entered_threat) {
    s.phase = 16; s.plan.clear(); s.remaining = 0; s.plan_target_town = -1;
    busy = false; // All current town production is cancelled before the strategist runs.
    const auto& queue = w.side_state[t.side].queue;
    for (size_t i = queue.size(); i > 0; --i) if (queue[i - 1].town == town) {
      auto c = command(Pct::QueueKill); c.a = int(i - 1); d.commands.push_back(c);
      if (rules.type(queue[i - 1].type).kind == 2) s.science_timer = 0;
    }
    // G: autonomous members stand in for eligible unlocked tactics. Script tactics
    // have no complete eligibility/lock adapter and retain their explicit orders.
    auto recall = command(Pct::Move);
    recall.a = t.x * kTilePx + kTilePx / 2; recall.b = t.y * kTilePx + kTilePx / 2;
    for (const auto& o : w.objects)
      if (live(o) && o.side == t.side && o.tactic == -1 && !rules.type(o.type).structure)
        recall.ids.push_back(o.id);
    if (!recall.ids.empty()) d.commands.push_back(recall);
  }
  if (attacker) {
    const bool new_alert = s.last_alert != attacker->id;
    // G: a new alert redirects autonomous troops; an ongoing alert also receives
    // idle reinforcements without resetting active orders or the production plan.
    auto c = command(Pct::Attack); c.a = int(attacker->id);
    for (const auto& o : w.objects)
      if (live(o) && o.town == t.index && o.side == t.side && o.tactic < 0 &&
          (new_alert || o.order == 0) &&
          (rules.type(o.type).mobile || rules.type(o.type).flier) && rules.type(o.type).armed &&
          rules.attack(o.type, attacker->type).damage > 0) c.ids.push_back(o.id);
    if (!c.ids.empty()) d.commands.push_back(c);
    s.last_alert = attacker->id;
  }
  if (!attacker) s.last_alert = 0;
  if (!entered_threat) for (const auto& q : w.side_state[t.side].queue) busy |= q.town == town;
  if (busy) return finish();
  const int dev = development(w, rules, town);
  bool poor = s.economy_enabled && income(w, rules, state, town) < 2 * (state.spent * 256 / int64_t(state.elapsed + 1));
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
    const int primary = count(w, town, farms[race]) > 2 && !count(w, town, warehouses[race]) ? warehouses[race] : farms[race];
    return buy(find(primary)) || buy(find(mines[race]));
  };
  // V: defence candidates follow the reversed type list, then one CRT draw.
  auto choose = [&](int category) -> const Offer* {
    if (category == 1) {
      std::vector<const Offer*> candidates;
      for (int type = 0; type < 90; ++type)
        for (const auto& o : offers) if (o.type == type && rules.type(type).kind == 7) candidates.push_back(&o);
      if (candidates.empty()) return nullptr;
      return candidates[msvc_rand(s.random) % candidates.size()];
    }
    // G: outpost specialization needs the native preference/ranking fields.
    const Offer* best = nullptr;
    for (const auto& o : offers) {
      const auto& r = rules.type(o.type);
      bool match = category == 1 ? r.kind == 7 : r.kind == 2 && r.dev_level > 0;
      if (o.placeable && match && (!best || o.price < best->price || (o.price == best->price && o.type < best->type))) best = &o;
    }
    return best;
  };
  auto science_offer = [&]() -> const Offer* {
    if (t.type == 0) return choose(2); // G: specialization preferences are not represented yet.
    const auto& tree = rules.build_list(t.race, t.type);
    auto present = [&](const Prerequisite& p) {
      return std::any_of(w.objects.begin(), w.objects.end(), [&](const auto& o) {
        return live(o) && o.side == t.side && o.type == p.type && o.progress == 1000 &&
            (p.side_wide || o.town == t.index);
      });
    };
    std::array<bool, 90> visiting{};
    std::function<const Offer*(int)> resolve = [&](int type) -> const Offer* {
      if (type < 0 || type >= 90 || visiting[size_t(type)]) return nullptr;
      const auto item = std::find_if(tree.begin(), tree.end(), [&](const auto& it) { return it.type == type; });
      if (item == tree.end()) return nullptr;
      for (const auto& sd : w.mission.sides) if (sd.index == t.side &&
          std::find(sd.disabled.begin(), sd.disabled.end(), type) != sd.disabled.end()) return nullptr;
      // Completed/queued unique structures are terminal filter markers, not new leaves.
      if (rules.type(type).kind == 2 && count(w, town, type)) return nullptr;
      for (const auto& q : w.side_state[t.side].queue) if (q.town == town && q.type == type) return nullptr;
      visiting[size_t(type)] = true;
      const Offer* leaf = nullptr;
      bool missing = false, failed = false;
      for (const auto& p : item->prereq) if (!present(p)) {
        missing = true;
        const auto* prerequisite = resolve(p.type);
        if (!prerequisite) { failed = true; break; }
        leaf = prerequisite;
      }
      visiting[size_t(type)] = false;
      if (failed) return nullptr;
      if (!missing) for (const auto& offer : offers) if (offer.type == type) return &offer;
      return leaf;
    };
    if (s.wanted_type != 90) {
      if (const auto* leaf = resolve(s.wanted_type))
        return rules.type(leaf->type).structure ? leaf : nullptr;
    }
    const Offer* best = nullptr;
    int best_score = std::numeric_limits<int>::max();
    // V: the science list is traversed in increasing type order.
    for (int type = 0; type < 90; ++type) {
      const auto& r = rules.type(type);
      if (r.kind != 2 || r.harvest.period != -1 || type == 18 || type == 40 || type == 70) continue;
      const auto* leaf = resolve(type);
      if (!leaf) continue;
      int unlocks = 0;
      for (const auto& item : tree) for (const auto& p : item.prereq)
        if (p.type == type && !present(p)) ++unlocks;
      if (!unlocks) continue;
      // V: recursive costs start at zero and only sum child costs (0x45b940).
      const int score = 10 - unlocks;
      if (score < best_score || (score == best_score && (msvc_rand(s.random) & 127) < 64)) {
        best = leaf; best_score = score;
      }
    }
    return best;
  };
  auto prepare = [&](bool offensive, bool reuse_towns = false) {
    ++s.plan_generation;
    s.plan = make_plan(w, rules, town, offensive ? s.target_town : town, offensive || reuse_towns, s.random, s.knowledge, &s.wanted_type);
    s.remaining = int(s.plan.size());
    const int target = offensive ? s.target_town : town;
    s.plan_target_town = target;
    return std::any_of(s.knowledge.begin(), s.knowledge.end(), [&](const auto& r) { return r.town == target; });
  };
  auto progress = [&](int done, bool allow_economy = true) {
    if (allow_economy && poor) economy();
    if (s.remaining > 0) return; // The side allocator owns outstanding production.
    s.plan.clear();
    s.remaining = 0; s.plan_target_town = -1; s.phase = done;
  };
  switch (s.phase) {
    case 0: s.phase = 10; break;
    case 1: s.phase = enemy(s.target_town) && prepare(true) ? 2 : 11; break;
    case 2: {
      if (s.remaining > 0) { progress(2); break; }
      if (poor) economy();
      for (const auto& entry : s.plan) {
        const auto* member = w.find(entry.object);
        if (member && live(*member) && member->side == t.side && member->progress == 1000)
          available.push_back(entry.object);
      }
      s.plan.clear();
      const int destination = s.plan_target_town >= 0 ? s.plan_target_town : s.target_town;
      if (enemy(destination) && !w.enemy_all(t.side) && !available.empty()) {
        auto c = command(Pct::AttackMove); c.ids = available;
        c.a = w.mission.towns[destination].x * kTilePx + kTilePx / 2;
        c.b = w.mission.towns[destination].y * kTilePx + kTilePx / 2;
        d.commands.push_back(c);
      }
      s.plan_target_town = -1; s.phase = 11; break;
    }
    case 3: {
      if (s.science_timer <= 0) break;
      const Offer* science = science_offer();
      if (science && science->placeable && science->price <= w.side_state[t.side].money && buy(science)) s.phase = 4;
      else { s.science_timer = 0; s.phase = 13; }
      break;
    }
    case 4: s.phase = 13; break;
    case 5: case 18: {
      const auto* defence = choose(1);
      if (defence && defence->placeable && buy(defence)) s.phase = s.phase == 5 ? 6 : 19;
      else {
        if (defence) s.defence_enabled = false;
        s.phase = s.phase == 5 ? 0 : 16;
      }
      break;
    }
    case 6: case 8: s.phase = 11; break;
    case 7: if (!economy()) s.economy_enabled = false; s.phase = 8; break;
    // V: the non-outpost gate concerns the source town, not the target town.
    case 9: s.phase = t.type != 0 && enemy(s.target_town) ? 1 : 11; break;
    case 10: s.phase = s.science_timer <= 100 * (dev + 1) && s.wanted_type == 90 ? 13 : 3; break;
    case 11: s.phase = poor ? 7 : 12; break;
    case 12: s.phase = !s.defence_enabled ? 11 : defences < dev ? 5 : 0; break;
    case 13: s.phase = forces < 3 * (dev + 1) ? 15 : 9; break;
    case 14: progress(9); break;
    case 15: s.phase = prepare(false) ? 14 : 9; break;
    case 16: if (foreign_nearby) s.phase = 21; else { s.plan.clear(); s.remaining = 0; s.plan_target_town = -1; s.phase = 0; } break;
    case 17: s.phase = 21; break;
    case 19: s.phase = 16; break;
    case 20: s.phase = s.defence_enabled && defences < dev ? 18 : 16; break;
    case 21: s.phase = forces < 7 * (dev + 1) ? 23 : 20; break;
    // V: the threatened plan progresses without the economic detour in states 2/14.
    case 22: progress(20, false); break;
    case 23: s.target_town = town; s.phase = prepare(false, true) ? 22 : 20; break;
    default: s.phase = 0; break;
  }
  return finish();
}
}  // namespace aztec::engine::ai
