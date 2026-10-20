#include "core/mp.hpp"

#include <cstdlib>

namespace aztec {

namespace {
std::vector<int> ints(const std::string& s) {
  std::vector<int> r;
  const char* p = s.c_str();
  while (*p) {
    while (*p && *p != '-' && (*p < '0' || *p > '9')) p++;
    if (!*p) break;
    char* e;
    r.push_back(int(std::strtol(p, &e, 10)));
    p = e;
  }
  return r;
}
std::string key(const char* base, int i) { return std::string(base) + std::to_string(i); }

void read_units(const Ini& ini, const std::string& sec, std::vector<MpUnit>& out) {
  int n = ini.get_int(sec, "Units", 0);
  for (int i = 0; i < n; i++) {
    MpUnit u;
    if (parse_unit_line(ini.get(sec, key("Unit", i)), u)) out.push_back(u);
  }
}
}  // namespace

bool parse_unit_line(const std::string& s, MpUnit& u) {
  auto v = ints(s);
  if (v.size() < 3) return false;
  u.x = v[0]; u.y = v[1]; u.type = v[2];
  u.hp = v.size() > 3 ? v[3] : 0;
  return true;
}

bool parse_mp(const Ini& ini, MpFile& m) {
  m = MpFile{};
  if (!ini.has_section("Globals")) return false;
  const char* G = "Globals";
  m.version = ini.get_int(G, "Version");
  m.map_name = ini.get(G, "MapName");
  auto sz = ints(ini.get(G, "MapSize"));
  if (sz.size() >= 2) { m.map_w = sz[0]; m.map_h = sz[1]; }
  m.name = ini.get(G, "Name"); m.name1 = ini.get(G, "Name1"); m.name2 = ini.get(G, "Name2");
  m.remark = ini.get(G, "Remark");
  m.avi = ini.get(G, "AVI");
  int st = ini.get_int(G, "StartTime");
  m.start_time = st < 0 ? 0 : st > 0x1c1f ? 0x1c1f : st;
  m.tileset = ini.get_int(G, "TileSet");
  m.winds = ini.get_int(G, "Winds", 5);
  if (m.winds > 20) m.winds = 20;
  m.max_players = ini.get_int(G, "MaxPlayers");
  m.music = ini.get_int(G, "Music");
  m.music_mask = unsigned(std::strtoul(ini.get(G, "MusicMask", "0").c_str(), nullptr, 10));
  m.current_team = ini.get_int(G, "CurrentTeam");
  for (int i = 0, n = ini.get_int(G, "Notes"); i < n; i++) m.notes.push_back(ini.get(G, key("Note", i)));
  for (int i = 0, n = ini.get_int(G, "Notesr"); i < n; i++) m.notes_ru.push_back(ini.get(G, key("Noter", i)));

  for (int i = 0, n = ini.get_int(G, "Sides"); i < n; i++) {
    std::string sec = key("Side", i);
    MpSide s;
    s.index = i;
    s.race = ini.get_int(sec, "Race"); s.color = ini.get_int(sec, "Color");
    s.neutral = ini.get_int(sec, "Neutral") != 0;
    s.start_money = ini.get_int(sec, "StartMoney", 100);
    s.start_x = ini.get_int(sec, "Start_X", 2); s.start_y = ini.get_int(sec, "Start_Y", 2);
    s.name = ini.get(sec, "Name"); s.remark = ini.get(sec, "Remark");
    s.difficulty = ini.get_int(sec, "Difficulty");
    s.only_comp = ini.get_int(sec, "OnlyComp") != 0;
    s.disabled_ids = ints(ini.get(sec, "DisabledID"));
    for (int d = 0, nd = ini.get_int(sec, "DefenceCount"); d < nd; d++) {
      auto v = ints(ini.get(sec, key("Defence", d)));
      MpDefence df;
      if (v.size() >= 4) df = {v[0], v[1], v[2], v[3]};
      else if (v.size() == 3) df = {-1, v[0], v[1], v[2]};
      else continue;
      s.defences.push_back(df);
    }
    for (int c = 0, nc = ini.get_int(sec, "CamerasCount"); c < nc; c++) {
      auto v = ints(ini.get(sec, key("Camera", c)));
      if (v.size() >= 3) s.cameras.push_back({v[0], v[1], v[2]});
    }
    m.sides.push_back(std::move(s));
  }
  for (int i = 0, n = ini.get_int(G, "Towns"); i < n; i++) {
    std::string sec = key("Town", i);
    MpTown t;
    t.index = i; t.side = ini.get_int(sec, "Side"); t.scenario_id = ini.get_int(sec, "ScenarioId", -1);
    t.x = ini.get_int(sec, "X", 2); t.y = ini.get_int(sec, "Y", 2);
    t.type = ini.get_int(sec, "Type"); t.race = ini.get_int(sec, "Race");
    t.tendency = ini.get_int(sec, "Tendency"); t.build_speed = ini.get_int(sec, "BuildSpeed");
    read_units(ini, sec, t.units);
    m.towns.push_back(std::move(t));
  }
  for (int i = 0, n = ini.get_int(G, "Teams"); i < n; i++) {
    std::string sec = key("Team", i);
    MpTeam t;
    t.index = i; t.side = ini.get_int(sec, "Side"); t.scenario_id = ini.get_int(sec, "ScenarioId", -1);
    read_units(ini, sec, t.units);
    m.teams.push_back(std::move(t));
  }
  auto vessels = [&](const char* count_key, const char* sec_base, std::vector<MpVessel>& out) {
    for (int i = 0, n = ini.get_int(G, count_key); i < n; i++) {
      std::string sec = key(sec_base, i);
      MpVessel v;
      v.index = i; v.side = ini.get_int(sec, "Side"); v.scenario_id = ini.get_int(sec, "ScenarioId", -1);
      v.x = ini.get_int(sec, "X"); v.y = ini.get_int(sec, "Y");
      v.up = ini.get_int(sec, "Up") == 1; v.enabled = ini.get_int(sec, "Enabled") != 0;
      v.editor_x = ini.get_int(sec, "EditorX"); v.editor_y = ini.get_int(sec, "EditorY");
      v.end_position = ini.get_int(sec, "EndPosition");
      out.push_back(v);
    }
  };
  vessels("Boats", "Boat", m.boats);
  vessels("Johnkas", "Johnka", m.johnkas);
  vessels("Submarines", "Submarine", m.submarines);
  for (int i = 0, n = ini.get_int("Paths", "Count"); i < n; i++) {
    auto v = ints(ini.get("Paths", std::to_string(i)));
    if (v.size() < 3) continue;
    MpPath p;
    p.id = v[0];
    for (size_t k = 1; k + 1 < v.size(); k += 2) p.points.push_back({v[k], v[k + 1]});
    m.paths.push_back(std::move(p));
  }
  return true;
}

bool load_mp(const std::string& path, MpFile& out) {
  Ini ini;
  return ini.load(path) && parse_mp(ini, out);
}

}  // namespace aztec
