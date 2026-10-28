#include "engine/sim/rules.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>
#include <tuple>

#include "engine/world/type_info.hpp"
#include "engine/sim/sight.hpp"
#include "engine/assets/daylight.hpp"

namespace aztec::engine {

bool SightData::load(const std::string& game_dir) {
  masks.clear();
  Archive lights;
  Daylight keys;
  if (!lights.open(find_file_ci(game_dir + "/Res", "light.xbe")) ||
      !keys.load_from_exe(find_file_ci(game_dir, "Aztec.exe"))) return false;
  for (int id = 1; id <= 71; ++id) {
    auto item = lights.get(data::kLightGroup, id);
    if (data::LightMask(item).ok()) masks[id] = {item.data, item.data + item.size};
  }
  for (int set = 0; set < 2; ++set) {
    if (keys.keys(set).empty()) return false;
    for (int t = 0; t < 7200; ++t) {
      auto next = std::upper_bound(keys.keys(set).begin(), keys.keys(set).end(), t,
          [](int time, const DayKey& key) { return time < key.time; });
      if (next == keys.keys(set).end()) return false;
      daylight[set][t] = uint8_t(std::clamp(next->level * 8, 0, 152));
    }
  }
  const auto units = find_file_ci(game_dir + "/Res", "Units.xbe");
  return !masks.empty() && sprites.open(units) && effects.open(units);
}

bool RulesData::load_sight(const std::string& game_dir) {
  auto inputs = std::make_shared<SightData>();
  if (!inputs->load(game_dir)) { sight_.reset(); return false; }
  sight_ = std::move(inputs);
  return true;
}

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
  // Exact binary32 constants, represented over 2^24. Split the 80-bit product
  // before multiplying the second coefficient; this also works without __int128.
  if (base <= 0) return 0;
  constexpr uint64_t one = uint64_t(1) << 24, mask = one - 1;
  constexpr uint64_t price_k[] = {11184811, one, 2 * one};
  constexpr uint64_t time_k[] = {2 * one, one, 11184811};
  int index = hostile_all ? 0 : std::clamp(speed, 0, 2);
  uint64_t k = hall == 0 ? (time ? one * 3 / 2 : one) : (time ? time_k[index] : price_k[index]);
  uint64_t f = computer && !hostile_all ? 13421773 : one;
  uint64_t product = uint64_t(base) * k;
  uint64_t high = (product >> 24) * f, low = (product & mask) * f;
  uint64_t value = (high >> 24) + (((high & mask) * one + low + (uint64_t(1) << 47)) >> 48);
  return int(std::min(value, uint64_t(std::numeric_limits<int>::max())));
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
    r.mobile = r.kind == 0 || r.kind == 1 || r.kind == 4 || r.kind == 6;
    r.nonorganic = r.structure || r.kind == 1 || r.kind == 4 || r.kind == 6 || r.kind == 9;
    r.flier = r.kind == 3 || r.kind == 5;
    r.armed = r.kind == 0 || r.kind == 1 || r.kind == 6 || r.kind == 7 || r.kind == 9 || r.kind == 10;
    // Boats and Johnka use scripted routes, never the ground movement system.
    r.large = r.kind == 1 || r.kind == 4 || r.kind == 6;
    if (r.large) r.class_src = Src::Verified;
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
    t.dev_level = num(r, 6);
    t.price = num(r, 4);
    t.time = num(r, 5);
    t.cost_src = Src::Verified;
    kind_param[id] = {t.kind, num(r, 3)};
  }
  static const char* sight_families[] = {"marine", "spider", "building", "zeppelin", "truck", "bird", "headtruck", "defence", "", "cannon"};
  static const int sight_fields[] = {6, 8, 55, 7, 6, 8, 8, 58, -1, 6};
  for (int kind = 0; kind < 10; ++kind) {
    if (sight_fields[kind] < 0) continue;
    for (const auto& row : read_csv(dir + "/desc_" + sight_families[kind] + ".csv"))
      for (int id = 0; id < kNumTypes; ++id)
        if (kind_param[id] == std::pair{kind, num(row, 0)}) {
          int mask = num(row, sight_fields[kind] + 1);
          if (mask >= 64 && mask <= 71) { types_[id].vision_mask = mask; types_[id].sight_src = Src::Verified; }
        }
  }
  if (const char* game = std::getenv("AZTEC_GAME")) load_sight(game);
  // Established getters only (rules_summary.md). Bird speed remains unresolved.
  for (auto [kind, family, field] : {std::tuple{1, "spider", 7}, {3, "zeppelin", 5},
                                   {4, "truck", 5}, {6, "headtruck", 7}}) {
    for (const auto& row : read_csv(dir + "/desc_" + family + ".csv"))
      for (int id = 0; id < kNumTypes; ++id)
        if (kind_param[id] == std::pair{kind, num(row, 0)} && row.size() > size_t(field + 1)) {
          int speed = num(row, field + 1);
          if (speed > 0 && speed <= (1 << 14)) {
            types_[id].speed = speed * 256; types_[id].speed_src = Src::Verified;
          }
        }
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
          if (r.size() > 12) {
            t.large = (uint32_t(num(r, 12)) & 255u) == 0;  // d11 low byte, not the organic/hypno bytes
            t.class_src = Src::Verified;
            t.radius_px = t.large ? 20 : 14;  // still a provisional collision disc
          }
        }
    } else if (t.structure && t.kind == 2) {
      for (auto& r : building)
        if (num(r, 0) == param) {
          t.hp_max = num(r, 2); t.hp_src = Src::Verified;
          auto& harvest = t.harvest;
          for (int cell = 0; cell < 49; ++cell) harvest.occupancy[cell] = num(r, 7 + cell);
          t.service_damage_protected = (uint32_t(num(r, 60)) & 255u) != 0;
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
  std::function<WeaponRules(int, int)> visual_weapon = [&](int id, int depth) -> WeaponRules {
    WeaponRules w;
    if (depth > 8) return w;
    auto ir = row(ids, id);
    if (ir.empty()) return w;
    w.klass = num(ir, 1, -1); w.parameter = num(ir, 2);
    if (w.klass == 12) {
      auto pair = row(pairs, w.parameter);
      auto first = visual_weapon(num(pair, 1, -1), depth + 1);
      auto second = visual_weapon(num(pair, 2, -1), depth + 1);
      w = first;
      w.components = {first, second};
      return w;
    }
    if (w.klass == 8) return visual_weapon(num(row(throws, w.parameter), 1, -1), depth + 1);
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
    t.weapon = visual_weapon(num(r, field + 1, -1), 0);
    if (t.kind == 1 || t.kind == 6) t.weapon_id = num(r, field + 1, -1);
  }
  // Documented primary HP and Defence weapon fields; visual metadata above
  // does not supply fallback damage for unresolved weapons.
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
    if (v > 0 && types_[id].speed_src != Src::Verified) {
      types_[id].speed = v; types_[id].speed_src = Src::Measured;
    }
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
