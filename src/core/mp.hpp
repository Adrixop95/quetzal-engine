// Complete Maps/<name>.mp schema (INI, cp1250). Keys the game itself never reads are kept too
// (flagged "editor") so tools can round-trip. See docs/formats.md.
#pragma once
#include <string>
#include <vector>
#include "core/ini.hpp"

namespace aztec {

struct MpUnit { int x = 0, y = 0, type = -1, hp = 0; };           // "Unit#=x, y, type, hp"
struct MpDefence { int scenario_id = -1, x = 0, y = 0, type = -1; };  // "Defence#=id,x,y,type" (3-int form: id = -1)
struct MpCamera { int x = 0, y = 0, z = 0; };                     // "Camera#=x,y,z"
struct MpPath { int id = 0; std::vector<std::pair<int, int>> points; };  // "#=id, x,y, x,y, ..."

struct MpSide {
  int index = 0;
  int race = 0;              // 0 Rus, 1 Aztecs, 2 China
  int color = 0;             // 0..7
  bool neutral = false;
  int start_money = 100;     // default when the key is absent
  int start_x = 2, start_y = 2;  // -1,-1 = none (default 2 when key absent)
  std::string name, remark;  // remark: editor
  int difficulty = 0;        // editor
  bool only_comp = false;    // editor
  std::vector<int> disabled_ids;  // UGI types the side may not build
  std::vector<MpDefence> defences;
  std::vector<MpCamera> cameras;
};

struct MpTown {
  int index = 0, side = 0, scenario_id = -1, x = 2, y = 2;
  int type = 0;              // 0 forpost, 1 village, 2 town, 3 fortress
  int race = 0;
  int tendency = 0, build_speed = 0;  // editor
  std::vector<MpUnit> units;
};
struct MpTeam { int index = 0, side = 0, scenario_id = -1; std::vector<MpUnit> units; };
struct MpVessel {            // [Boat#], [Johnka#], [Submarine#]
  int index = 0, side = 0, scenario_id = -1, x = 0, y = 0;
  bool up = false;           // Up=1
  bool enabled = false;      // Johnka: Enabled
  int editor_x = 0, editor_y = 0, end_position = 0;  // editor
};

struct MpFile {
  int version = 0;           // editor
  std::string map_name, name, name1, name2, remark, avi;
  int map_w = 0, map_h = 0;
  int start_time = 0;        // 0..7199 clamped; daylight clock
  int tileset = 0, winds = 5, max_players = 0;
  int music = 0;             // != 0: play exactly this track
  unsigned music_mask = 0;   // bit (n-1) = track n
  int current_team = 0;      // editor
  std::vector<std::string> notes, notes_ru;   // editor
  std::vector<MpSide> sides;
  std::vector<MpTown> towns;
  std::vector<MpTeam> teams;
  std::vector<MpVessel> boats, johnkas, submarines;
  std::vector<MpPath> paths;
};

bool parse_unit_line(const std::string& s, MpUnit& out);   // needs >= 3 numbers
bool parse_mp(const Ini& ini, MpFile& out);
bool load_mp(const std::string& path, MpFile& out);

}  // namespace aztec
