#include "engine/sim/rules.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "engine/world/type_info.hpp"
#include <functional>

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

void RulesData::init_defaults() {
  types_.assign(kNumTypes, TypeRules{});
  for (int t = 0; t < kNumTypes; t++) {
    TypeRules& r = types_[t];
    const TypeInfo* ti = type_info(t);
    r.kind = ti ? ti->kind : -1;
    r.structure = ti && ti->is_structure();
    r.mobile = r.kind == 0 || r.kind == 1 || r.kind == 4 || r.kind == 6 || r.kind == 10;
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
  // Marine descriptors: d1 start hp, d4 max hp, d5 step px per tick.
  auto marine = read_csv(dir + "/desc_marine.csv");
  auto building = read_csv(dir + "/desc_building.csv");
  auto speeds = read_csv(dir + "/speed_measured.csv");
  for (int id = 0; id < kNumTypes; id++) {
    TypeRules& t = types_[id];
    int param = kind_param[id].second;
    if (t.kind == 0) {
      for (auto& r : marine)
        if (num(r, 0) == param) {
          t.hp_max = num(r, 5); t.hp_src = Src::Verified;
          t.speed = num(r, 6) * 256;  // d5 (column index shifts by one for the leading param column)
          t.speed_src = Src::Verified;
        }
    } else if (t.structure && t.kind == 2) {
      for (auto& r : building)
        if (num(r, 0) == param) {
          t.hp_max = num(r, 2); t.hp_src = Src::Verified;
          int w = num(r, 5), h = num(r, 6);  // d4, d5 (px)
          if (w > 0 && h > 0) { t.foot_w = std::max(1, (w + 47) / 48); t.foot_h = std::max(1, (h + 47) / 48); t.foot_src = Src::Placeholder; }
        }
    }
  }
  // Descriptor weapon fields: Marine d7, Headtruck/Spider d9, Defence d59.
  // Composite dispatch and unannotated flight fields remain provisional.
  auto ids = read_csv(dir + "/weapons_ids.csv");
  auto arrows = read_csv(dir + "/weapon_class_arrow1.csv");
  auto axes = read_csv(dir + "/weapon_class_axe3.csv");
  auto balls = read_csv(dir + "/weapon_class_ball0.csv");
  auto flames = read_csv(dir + "/weapon_class_w7.csv");
  auto direct = read_csv(dir + "/weapon_class_w2_13.csv");
  auto pairs = read_csv(dir + "/weapon_class_w12.csv");
  auto throws = read_csv(dir + "/weapon_class_w8.csv");
  auto row = [](const auto& rows, int param) -> std::vector<std::string> {
    for (const auto& r : rows) if (num(r, 0, -1) == param) return r;
    return {};
  };
  std::function<WeaponRules(int, int)> weapon = [&](int id, int depth) -> WeaponRules {
    WeaponRules w;
    if (depth > 8) return w;
    auto ir = row(ids, id);
    if (ir.empty()) return w;
    w.klass = num(ir, 1, -1); w.parameter = num(ir, 2);
    if (w.klass == 12) {
      auto pair = row(pairs, w.parameter);
      auto first = weapon(num(pair, 1, -1), depth + 1);
      auto second = weapon(num(pair, 2, -1), depth + 1);
      w = first;
      w.components = {first, second};
      return w;
    }
    if (w.klass == 8) return weapon(num(row(throws, w.parameter), 1, -1), depth + 1);
    if (w.klass == 0 || w.klass == 1 || w.klass == 3) {
      auto r = row(w.klass == 0 ? balls : w.klass == 1 ? arrows : axes, w.parameter);
      if (r.empty()) return w;
      w.anim = num(r, 1); w.speed = num(r, 2); w.reload = num(r, 3); w.range = num(r, 4);
      if (w.klass == 1) w.shockwave = num(r, 14);
      int secondary_id = w.klass == 0 ? num(r, 5, -1) : w.klass == 1 ? num(r, 13, -1) : num(r, 6, -1);
      auto secondary = row(ids, secondary_id);
      if (num(secondary, 1, -1) == 7) {
        auto splash = row(flames, num(secondary, 2));
        w.impact_anim = num(splash, 1); w.splash_px = num(splash, 7);
      }
    } else if (w.klass == 7) {
      w.anim = 157; w.speed = 24; w.range = 96; w.reload = 12;
      w.impact_anim = num(row(flames, w.parameter), 1);
      w.splash_px = num(row(flames, w.parameter), 7);
    } else if (w.klass == 2 || w.klass == 13) {
      auto r = row(direct, w.parameter);
      w.range = num(r, 5); w.reload = num(r, 1);
      w.muzzle = num(r, 11) == 106;
    }
    return w;
  };
  const char* descriptors[] = {"marine", "spider", "", "", "", "", "headtruck", "defence"};
  for (int id = 0; id < kNumTypes; id++) {
    auto& t = types_[id];
    if (t.kind != 0 && t.kind != 1 && t.kind != 6 && t.kind != 7) continue;
    auto r = row(read_csv(dir + "/desc_" + descriptors[t.kind] + ".csv"), kind_param[id].second);
    int field = t.kind == 0 ? 7 : t.kind == 7 ? 59 : 9;
    if (r.size() <= size_t(field + 1)) continue;
    t.weapon = weapon(num(r, field + 1, -1), 0);
    if (t.weapon.range > 0) { t.range_px = t.weapon.range; t.range_src = t.weapon.klass == 1 ? Src::Verified : Src::Placeholder; }
    if (t.weapon.reload > 0) t.reload = t.weapon.reload;
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
    while (ss >> tok) it.prereq.push_back(std::atoi(tok.c_str()));
    tree_[{num(r, 0), num(r, 1)}].push_back(it);
  }
  return true;
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
