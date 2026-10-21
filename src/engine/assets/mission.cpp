#include "engine/assets/mission.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "core/mp.hpp"
#include "core/text.hpp"
#include "engine/assets/game_data.hpp"

namespace fs = std::filesystem;

namespace aztec::engine {

bool parse_mission(const Ini& ini, const std::string& base_name, MissionDef& m) {
  MpFile mp;
  if (!parse_mp(ini, mp)) return false;
  m = MissionDef{};
  m.file_name = base_name;
  m.title = cp1250_to_utf8(mp.name);
  m.map_name = mp.map_name.empty() ? base_name : mp.map_name;
  m.w = mp.map_w;
  m.h = mp.map_h;
  m.tileset = mp.tileset;
  m.start_time = mp.start_time;
  for (const auto& n : mp.notes) m.notes.push_back(cp1250_to_utf8(n));
  for (const MpSide& s : mp.sides) {
    SideDef d;
    d.index = s.index; d.color = s.color; d.race = s.race;
    d.start_x = s.start_x; d.start_y = s.start_y; d.money = s.start_money;
    d.neutral = s.neutral; d.only_comp = s.only_comp;
    d.name = cp1250_to_utf8(s.name);
    d.disabled = s.disabled_ids;
    m.sides.push_back(d);
  }
  auto add_units = [&](const std::vector<MpUnit>& units, int side, ObjectSource src, int group) {
    for (const MpUnit& u : units) {
      PlacedObject o;
      o.x = u.x; o.y = u.y; o.type = u.type; o.hp = u.hp;
      o.side = side; o.source = src; o.group = group;
      m.objects.push_back(o);
    }
  };
  for (const MpTown& t : mp.towns) {
    m.towns.push_back({t.index, t.side, t.x, t.y, t.type, t.race});
    add_units(t.units, t.side, ObjectSource::Town, t.index);
  }
  for (const MpTeam& t : mp.teams) add_units(t.units, t.side, ObjectSource::Team, t.index);
  auto add_vessels = [&](const std::vector<MpVessel>& v, ObjectSource src) {
    for (const MpVessel& b : v) {
      PlacedObject o;
      o.x = b.x; o.y = b.y; o.side = b.side; o.source = src; o.group = b.index;
      m.objects.push_back(o);
    }
  };
  add_vessels(mp.boats, ObjectSource::Boat);
  add_vessels(mp.johnkas, ObjectSource::Johnka);
  add_vessels(mp.submarines, ObjectSource::Submarine);
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
