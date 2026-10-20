#include "engine/assets/mission.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "core/text.hpp"
#include "engine/assets/game_data.hpp"

namespace fs = std::filesystem;

namespace aztec::engine {

bool parse_unit_line(const std::string& s, int& x, int& y, int& type, int& hp) {
  int v[4] = {0, 0, 0, 0};
  int n = 0;
  const char* p = s.c_str();
  while (n < 4 && *p) {
    char* end;
    long val = std::strtol(p, &end, 10);
    if (end == p) { p++; continue; }
    v[n++] = int(val);
    p = end;
    while (*p == ',' || *p == ' ' || *p == '\t') p++;
  }
  if (n < 3) return false;
  x = v[0]; y = v[1]; type = v[2]; hp = v[3];
  return true;
}

static std::string idx(const char* base, int i) { return base + std::to_string(i); }

bool parse_mission(const Ini& ini, const std::string& base_name, MissionDef& m) {
  if (!ini.has_section("Globals")) return false;
  m = MissionDef{};
  m.file_name = base_name;
  m.title = cp1250_to_utf8(ini.get("Globals", "Name"));
  m.map_name = ini.get("Globals", "MapName", base_name);
  m.tileset = ini.get_int("Globals", "TileSet");
  std::string sz = ini.get("Globals", "MapSize");
  if (sscanf(sz.c_str(), "%d , %d", &m.w, &m.h) != 2) { m.w = m.h = 0; }
  int nn = ini.get_int("Globals", "Notes");
  for (int i = 0; i < nn; i++) m.notes.push_back(cp1250_to_utf8(ini.get("Globals", idx("Note", i))));

  int sides = ini.get_int("Globals", "Sides");
  for (int i = 0; i < sides; i++) {
    std::string s = idx("Side", i);
    if (!ini.has_section(s)) continue;
    SideDef d;
    d.index = i;
    d.color = ini.get_int(s, "Color");
    d.race = ini.get_int(s, "Race");
    d.start_x = ini.get_int(s, "Start_X");
    d.start_y = ini.get_int(s, "Start_Y");
    d.money = ini.get_int(s, "StartMoney");
    d.neutral = ini.get_int(s, "Neutral") != 0;
    d.only_comp = ini.get_int(s, "OnlyComp") != 0;
    d.name = cp1250_to_utf8(ini.get(s, "Name"));
    m.sides.push_back(d);
  }

  auto add_units = [&](const std::string& sec, int side, ObjectSource src, int group) {
    int n = ini.get_int(sec, "Units");
    for (int u = 0; u < n; u++) {
      PlacedObject o;
      if (!parse_unit_line(ini.get(sec, idx("Unit", u)), o.x, o.y, o.type, o.hp)) continue;
      o.side = side; o.source = src; o.group = group;
      m.objects.push_back(o);
    }
  };
  int towns = ini.get_int("Globals", "Towns");
  for (int i = 0; i < towns; i++) {
    std::string s = idx("Town", i);
    if (!ini.has_section(s)) continue;
    TownDef t;
    t.index = i;
    t.side = ini.get_int(s, "Side");
    t.x = ini.get_int(s, "X");
    t.y = ini.get_int(s, "Y");
    t.type = ini.get_int(s, "Type");
    t.race = ini.get_int(s, "Race");
    m.towns.push_back(t);
    add_units(s, t.side, ObjectSource::Town, i);
  }
  int teams = ini.get_int("Globals", "Teams");
  for (int i = 0; i < teams; i++) {
    std::string s = idx("Team", i);
    if (ini.has_section(s)) add_units(s, ini.get_int(s, "Side"), ObjectSource::Team, i);
  }
  struct { const char* sec; const char* count; ObjectSource src; } extra[] = {
      {"Boat", "Boats", ObjectSource::Boat}, {"Johnka", "Johnkas", ObjectSource::Johnka},
      {"Submarine", "Submarines", ObjectSource::Submarine}};
  for (auto& e : extra) {
    int n = ini.get_int("Globals", e.count);
    for (int i = 0; i < n; i++) {
      std::string s = idx(e.sec, i);
      if (!ini.has_section(s)) continue;
      PlacedObject o;
      o.x = ini.get_int(s, "X");
      o.y = ini.get_int(s, "Y");
      o.side = ini.get_int(s, "Side");
      o.source = e.src;
      o.group = i;
      m.objects.push_back(o);
    }
  }
  return true;
}

std::string resolve_mission(const std::string& maps_dir, const std::string& spec) {
  std::error_code ec;
  if (fs::is_regular_file(spec, ec)) return spec;
  bool numeric = !spec.empty() && spec.find_first_not_of("0123456789") == std::string::npos;
  std::string name = numeric ? "mission_" + spec : spec;
  std::string stem = name;
  if (stem.size() > 3 && (stem.substr(stem.size() - 3) == ".mp" || stem.substr(stem.size() - 3) == ".MP"))
    stem.resize(stem.size() - 3);
  return find_file_ci(maps_dir, stem + ".mp");
}

bool load_mission(const std::string& mp_path, MissionDef& out) {
  Ini ini;
  if (!ini.load(mp_path)) return false;
  std::string stem = fs::path(mp_path).stem().string();
  return parse_mission(ini, stem, out);
}

}  // namespace aztec::engine
