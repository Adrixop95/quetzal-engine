#include "engine/sim/rules.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "engine/world/type_info.hpp"

namespace aztec::engine {

namespace {
std::vector<std::vector<std::string>> read_csv(const std::string& path) {
  std::vector<std::vector<std::string>> rows;
  std::ifstream f(path);
  std::string line;
  bool first = true;
  while (std::getline(f, line)) {
    if (first) { first = false; continue; }  // header
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::vector<std::string> cols;
    std::string cur;
    bool q = false;
    for (char c : line) {
      if (c == '"') q = !q;
      else if (c == ',' && !q) { cols.push_back(cur); cur.clear(); }
      else cur += c;
    }
    cols.push_back(cur);
    rows.push_back(std::move(cols));
  }
  return rows;
}
// Parses a decimal such as "9.26" into value * 256 with integer arithmetic only (no atof: it depends on the C locale).
// Rounds half up, clamps to +-(1<<22) so the result always fits an int. Returns 0 for malformed text.
int parse_fixed256_impl(const std::string& t) {
  size_t i = 0;
  bool neg = false;
  if (i < t.size() && (t[i] == '-' || t[i] == '+')) neg = t[i++] == '-';
  int64_t ip = 0, frac = 0, scale = 1;
  bool digits = false;
  for (; i < t.size() && t[i] >= '0' && t[i] <= '9'; i++) { ip = ip * 10 + (t[i] - '0'); digits = true; if (ip > (1 << 22)) ip = 1 << 22; }
  if (i < t.size() && t[i] == '.') {
    for (i++; i < t.size() && t[i] >= '0' && t[i] <= '9' && scale < 1000000; i++) { frac = frac * 10 + (t[i] - '0'); scale *= 10; digits = true; }
  }
  if (!digits) return 0;
  int64_t v = (ip * scale + frac) * 256;
  v = (v + scale / 2) / scale;
  v = std::min<int64_t>(v, int64_t(1) << 22);
  return int(neg ? -v : v);
}

int num(const std::vector<std::string>& r, size_t i, int def = 0) { return i < r.size() && !r[i].empty() ? std::atoi(r[i].c_str()) : def; }

// Melee types use a short reach; everything else with a weapon shoots. Placeholder classification (see docs/engine_rules_status.md).
bool is_melee(int t) {
  static const int m[] = {0, 2, 5, 6, 29, 31, 32, 53, 54, 55, 56, 57, 82};
  for (int v : m) if (v == t) return true;
  return false;
}
}  // namespace

int parse_fixed256(const std::string& t) { return parse_fixed256_impl(t); }

int harvest_tick(HarvestState& s, int rate, int clock, int period, const std::vector<int>* dates) {
  s.accumulated += rate;
  bool due = false;
  if (dates) {
    if (dates->empty()) return 0;
    if (s.cursor == dates->size()) {
      if (clock <= dates->front()) s.cursor = 0;
    } else if (clock >= (*dates)[s.cursor]) {
      ++s.cursor;
      due = true;
    }
  } else if (period >= 0 && (clock - s.last_clock + kDayTicks) % kDayTicks >= period) {
    s.last_clock = clock;
    due = true;
  }
  if (!due) return 0;
  int value = int((s.accumulated + 128) >> 8);
  if (value != 0) s.accumulated = 0;
  return value;
}

int production_value(int base, int hall, int speed, bool computer, bool hostile_all, bool time) {
  // Preserve float32 constants, then evaluate the x87 expression before truncation.
  constexpr float price_k[] = {2.f / 3.f, 1.f, 2.f};
  constexpr float time_k[] = {2.f, 1.f, 2.f / 3.f};
  int index = hostile_all ? 0 : std::clamp(speed, 0, 2);
  float k = hall == 0 ? (time ? 1.5f : 1.f) : (time ? time_k[index] : price_k[index]);
  float f = computer && !hostile_all ? 0.8f : 1.f;
  return int(static_cast<long double>(base) * k * f + 0.5L);
}

void RulesData::init_defaults() {
  loaded_ = false;
  tree_.clear();
  duel_.clear();
  weapons_.clear();
  types_.assign(kNumTypes, TypeRules{});
  for (int t = 0; t < kNumTypes; t++) {
    TypeRules& r = types_[t];
    const TypeInfo* ti = type_info(t);
    r.kind = ti ? ti->kind : -1;
    r.structure = ti && ti->is_structure();
    r.mobile = r.kind == 0 || r.kind == 1 || r.kind == 4 || r.kind == 6 || r.kind == 10;
    r.nonorganic = r.structure || r.kind == 1 || r.kind == 4 || r.kind == 6 || r.kind == 9;
    r.flier = r.kind == 3 || r.kind == 5;
    r.armed = r.kind == 0 || r.kind == 1 || r.kind == 6 || r.kind == 7 || r.kind == 9 || r.kind == 10;
    r.large = r.kind == 4 || r.kind == 6 || t == 5 || t == 57;
    r.speed = 8 * 256;
    r.radius_px = r.large ? 20 : 14;
    r.range_px = is_melee(t) ? 56 : 288;
    if (r.structure) { r.hp_max = 30000; r.foot_w = r.foot_h = 2; r.range_px = 336; r.dmg = 400; }
    for (int h = 0; h < 4; h++) for (int c = 0; c < 3; c++) town_ids_[h][c] = -1;
  }
}

bool RulesData::load_dir(const std::string& dir) {
  init_defaults();
  auto types = read_csv(dir + "/types.csv");
  if (types.empty()) return false;
  loaded_ = true;
  std::vector<std::pair<int, int>> kind_param(kNumTypes, {-1, 0});
  for (auto& r : types) {
    int id = num(r, 0);
    if (id < 0 || id >= kNumTypes) continue;
    TypeRules& t = types_[id];
    t.price = num(r, 4);
    t.time = num(r, 5);
    t.cost_src = Src::Verified;
    kind_param[id] = {t.kind, num(r, 3)};
  }
  // Marine d1 is primary/max HP; d4 is independent secondary HP.
  auto marine = read_csv(dir + "/desc_marine.csv");
  auto building = read_csv(dir + "/desc_building.csv");
  auto speeds = read_csv(dir + "/speed_measured.csv");
  for (int id = 0; id < kNumTypes; id++) {
    TypeRules& t = types_[id];
    int param = kind_param[id].second;
    if (t.kind == 0) {
      for (auto& r : marine)
        if (num(r, 0) == param) {
          t.hp_max = num(r, 2);
          t.secondary_hp = num(r, 5); t.regen = num(r, 3); t.regen_alt_threshold = num(r, 4);
          t.weapon_id = num(r, 8, -1); t.physical_armor = num(r, 14);
          int flags = num(r, 12);
          t.category_e8 = ((flags >> 8) & 255) != 0;
          t.category_f4 = !t.category_e8;
          t.hypno = ((flags >> 16) & 255) != 0;
          t.hp_src = Src::Verified;
          t.speed = num(r, 6) * 256;  // d5 (column index shifts by one for the leading param column)
          t.speed_src = Src::Verified;
        }
    } else if (t.structure && t.kind == 2) {
      for (auto& r : building)
        if (num(r, 0) == param) {
          t.hp_max = num(r, 2); t.hp_src = Src::Verified;
          auto& harvest = t.harvest;
          for (int cell = 0; cell < 49; ++cell) harvest.occupancy[cell] = num(r, 7 + cell);
          harvest.terrain = (uint32_t(num(r, 60)) & 0xff00u) != 0;  // byte +0xed within d59
          harvest.rate_a = num(r, 61); harvest.rate_b = num(r, 62);
          harvest.repair_rate = num(r, 63); harvest.scheduled = num(r, 64) != 0;
          harvest.period = num(r, 65, -1); harvest.repair_period = num(r, 321, -1);
          if (harvest.scheduled)
            for (int i = 0; i < std::clamp(harvest.period, 0, 255); ++i) harvest.dates.push_back(num(r, 66 + i));
          int w = num(r, 5), h = num(r, 6);  // d4, d5 (px)
          if (w > 0 && h > 0) { t.foot_w = std::max(1, (w + 47) / 48); t.foot_h = std::max(1, (h + 47) / 48); t.foot_src = Src::Placeholder; }
        }
    }
  }
  // Only documented primary HP/weapon fields are enabled; other fields retain provenance.
  for (const auto& family : std::vector<std::pair<int, std::string>>{{7, "defence"}, {3, "zeppelin"}, {5, "bird"}, {6, "headtruck"}, {1, "spider"}}) {
    auto rows = read_csv(dir + "/desc_" + family.second + ".csv");
    for (int id = 0; id < kNumTypes; ++id) {
      auto& t = types_[id];
      if (t.kind != family.first) continue;
      for (const auto& r : rows) if (num(r, 0) == kind_param[id].second) {
        t.hp_max = num(r, (t.kind == 7 || t.kind == 6 || t.kind == 1) ? 4 : 2);
        t.hp_src = Src::Verified;
        if (t.kind == 7) { t.weapon_id = num(r, 60, -1); t.armed = t.weapon_id >= 0; }
      }
    }
  }
  std::map<int, std::map<int, std::vector<int>>> tables;
  for (const auto& entry : std::vector<std::pair<int, std::string>>{
      {1, "arrow1"}, {2, "w2_13"}, {8, "w8"}, {12, "w12"}}) {
    for (const auto& row : read_csv(dir + "/weapon_class_" + entry.second + ".csv")) {
      std::vector<int> data;
      for (size_t i = 1; i < row.size(); ++i) data.push_back(num(row, i));
      tables[entry.first][num(row, 0)] = std::move(data);
    }
  }
  tables[13] = tables[2];
  for (const auto& row : read_csv(dir + "/weapons_ids.csv")) {
    int kind = num(row, 1);
    weapons_[num(row, 0)] = {kind, tables[kind][num(row, 2)]};
  }
  for (auto& t : types_) if (t.weapon_id >= 0) {
    auto shot = weapon(t.weapon_id, TypeRules{});
    if (shot.getters_known) {
      t.range_px = shot.range_px; t.reload = shot.reload; t.range_src = Src::Verified;
    }
    if (shot.damage_known) { t.dmg = shot.damage; t.dmg_src = Src::Verified; }
  }
  for (auto& r : speeds) {
    int id = num(r, 0);
    if (id < 0 || id >= kNumTypes || r.size() < 3) continue;
    int v = parse_fixed256(r[2]);
    if (v > 0) { types_[id].speed = v; types_[id].speed_src = Src::Measured; }
  }
  for (auto& r : read_csv(dir + "/duel_damage.csv")) {
    // attacker,attacker_ugi,target,target_ugi,damage,interval,hits
    int a = num(r, 0), b = num(r, 2), d = num(r, 4), iv = num(r, 5);
    if (d > 0 && iv > 0) duel_[{a, b}] = {d, iv};
  }
  for (auto& r : read_csv(dir + "/townhall_ids.csv"))
    for (int c = 0; c < 3; c++) {
      int h = num(r, 0);
      if (h >= 0 && h < 4) town_ids_[h][c] = num(r, 1 + c, -1);
    }
  for (auto& r : read_csv(dir + "/buildtree.csv")) {
    // race,townhall_type,slot,item_ugi,flagB_construct,prereq ("14 22A")
    BuildItem it;
    it.type = num(r, 3);
    it.construct = num(r, 4) != 0;
    std::stringstream ss(r.size() > 5 ? r[5] : "");
    std::string tok;
    while (ss >> tok) it.prereq.push_back({std::atoi(tok.c_str()), tok.back() == 'A'});
    tree_[{num(r, 0), num(r, 1)}].push_back(it);
  }
  return true;
}

int category_damage(int base, const std::vector<int>& multipliers) {
  int64_t value = base;
  for (int multiplier : multipliers) value = (value * multiplier) >> 8;
  return int(value);
}

int regenerated_hp(const TypeRules& type, int hp, bool alternate_state) {
  if (type.kind != 0 || hp <= 0) return hp;
  const int threshold = alternate_state ? type.regen_alt_threshold : type.hp_max;
  // Check before addition; overshoot is an explicit construction choice pending reference evidence.
  return hp < threshold ? hp + type.regen : hp;
}

WeaponResult RulesData::weapon(int id, const TypeRules& target, bool allied) const {
  return weapon_impl(id, target, allied, 0);
}

WeaponResult RulesData::attack(int attacker, int target, bool allied) const {
  const auto& a = type(attacker);
  if (a.weapon_id >= 0) return weapon(a.weapon_id, type(target), allied);
  // No descriptor weapon: retain explicitly marked placeholder behavior, never duel intervals.
  return {a.dmg, a.reload, a.range_px, 0, true, true};
}

WeaponResult RulesData::weapon_impl(int id, const TypeRules& target, bool allied, int depth) const {
  WeaponResult out;
  if (id == 0) { out.damage_known = true; return out; } // no chained weapon
  auto it = weapons_.find(id);
  if (it == weapons_.end() || depth >= 16) return out;
  const auto& w = it->second;
  const auto& d = w.data;
  if (w.kind == 8 && !d.empty()) return weapon_impl(d[0], target, allied, depth + 1);
  if (w.kind == 12 && d.size() >= 2) {
    auto a = weapon_impl(d[0], target, allied, depth + 1);
    auto b = weapon_impl(d[1], target, allied, depth + 1);
    out.damage = a.damage + b.damage;
    out.damage_known = a.damage_known && b.damage_known;
    // The sum is established; composite scheduling/range is not documented.
    return out;
  }
  const bool arrow = w.kind == 1;
  if ((arrow && d.size() >= 13) || ((w.kind == 2 || w.kind == 13) && d.size() >= 10)) {
    const int base = arrow ? 4 : 1, armor = arrow ? 5 : 2, mult = arrow ? 7 : 5;
    std::vector<int> factors;
    if (target.flier) factors.push_back(d[mult]);
    if (target.nonorganic) factors.push_back(d[mult + 1]);
    if (target.category_f4) factors.push_back(d[mult + 2]);
    if (target.category_e8) factors.push_back(d[mult + 3]);
    if (target.structure) factors.push_back(d[mult + 4]);
    out.damage = category_damage(d[base], factors);
    if (!(arrow && target.flier && d[mult] == 0))
      out.damage += int((int64_t(d[armor]) * target.physical_armor) >> 8);
    out.damage_known = true;
    if (arrow && d[12] != 0) {
      auto extra = weapon_impl(d[12], target, allied, depth + 1);
      out.damage += extra.damage;
      out.damage_known = extra.damage_known;
    }
    out.reload = d[arrow ? 2 : 0]; out.range_px = d[arrow ? 3 : 4];
    out.projectile_speed = arrow ? d[1] : 0;
    out.getters_known = true;
  } else if (w.kind == 4 || w.kind == 5 || w.kind == 6 || w.kind == 10) {
    out.damage_known = out.getters_known = true;
    if (w.kind == 4 || w.kind == 5) {
      out.damage = (target.nonorganic || target.structure) ? 1500 : 3000;
      if (allied) out.damage = 0;
      out.reload = 19; out.range_px = 72;
    } else if (w.kind == 6) {
      out.damage = target.nonorganic ? 9000 : 18000;
      out.reload = 25; out.range_px = 10;
    } else {
      out.damage = (target.nonorganic || target.structure) ? 1125 : 1800;
      if (target.category_f4) out.damage = category_damage(out.damage, {320});
      if (target.category_e8) out.damage = category_damage(out.damage, {320});
      out.reload = 11; out.range_px = 10;
    }
    if (target.flier) out.damage = 0;
  }
  // Classes 0/3/7/9/11 and burn/projectile sequencing remain unresolved.
  return out;
}

bool RulesData::has_duel(int a, int b, DuelEntry& out) const {
  auto it = duel_.find({a, b});
  if (it == duel_.end()) return false;
  out = it->second;
  return true;
}

const std::vector<BuildItem>& RulesData::build_list(int race, int th) const {
  auto it = tree_.find({race, th});
  return it == tree_.end() ? empty_ : it->second;
}

int RulesData::town_building(int th, int race) const {
  return th >= 0 && th < 4 && race >= 0 && race < 3 ? town_ids_[th][race] : -1;
}

}  // namespace aztec::engine
