// Mission description parsed from Maps/<name>.mp (INI, cp1250). See docs/formats.md.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/ini.hpp"

namespace aztec::engine {

struct SideDef {
  int index = 0;
  int color = 0;          // palette/team colour index 0..7
  int race = 0;           // 0 Rus, 1 Aztecs, 2 China
  int start_x = 0, start_y = 0;  // tile
  int money = 0;
  bool neutral = false, only_comp = false;
  std::string name;
  std::vector<int> disabled;  // UGI types this side may not build
};

enum class ObjectSource : uint8_t { Town, Team, Boat, Johnka, Submarine, Start };

struct PlacedObject {
  int x = 0, y = 0;       // tile
  int type = -1;          // UGI id; -1 for non-typed markers (boat, start position...)
  int hp = 0;
  int side = 0;
  ObjectSource source = ObjectSource::Team;
  int group = 0;          // Town#/Team#/Boat# index
};

struct TownDef {
  int index = 0, side = 0, x = 0, y = 0, type = 0, race = 0;
};

struct MissionDef {
  std::string file_name;  // base name used for the sibling files (.map, .dat ...)
  std::string title;
  std::string map_name;
  int w = 0, h = 0;
  int tileset = 0;
  int start_time = 0;     // daylight clock 0..7199
  std::vector<SideDef> sides;
  std::vector<TownDef> towns;
  std::vector<PlacedObject> objects;  // Town/Team unit lines, boats, johnkas, submarines
  std::vector<std::string> notes;     // English objective text, UTF-8
};

// Thin adapter over core/mp.* (the full .mp loader). Pure and unit-testable; `base_name` is stored in file_name.
bool parse_mission(const Ini& ini, const std::string& base_name, MissionDef& out);

// Resolves `spec` ("3", "mission_3", "Pentagram", or a path to a .mp) inside `maps_dir` and loads it.
// Returns the resolved .mp path or "" on failure.
std::string resolve_mission(const std::string& maps_dir, const std::string& spec);
bool load_mission(const std::string& mp_path, MissionDef& out);

}  // namespace aztec::engine
