#include "engine/world/world.hpp"

namespace aztec::engine {

bool load_world(const GameData& data, const std::string& spec, World& w, std::string& err) {
  std::string mp = resolve_mission(data.maps_dir(), spec);
  if (mp.empty()) { err = "mission not found: " + spec; return false; }
  w = World{};
  if (!load_mission(mp, w.mission)) { err = "cannot parse " + mp; return false; }
  const std::string& base = w.mission.file_name;
  std::string mapf = data.map_file(base, "map");
  if (mapf.empty() && w.mission.map_name != base) mapf = data.map_file(w.mission.map_name, "map");
  if (mapf.empty() || !load_terrain_map(mapf, w.terrain)) { err = "cannot load .map for " + base; return false; }
  if (w.terrain.w <= 0 || w.terrain.h <= 0) { err = "empty map"; return false; }
  if (w.mission.w == 0) { w.mission.w = w.terrain.w; w.mission.h = w.terrain.h; }

  std::string f;
  if (!(f = data.map_file(base, "dat")).empty()) load_passability(f, w.terrain.w, w.terrain.h, w.passability);
  if (!(f = data.map_file(base, "rm")).empty()) load_resource_map(f, w.resources);
  if (!(f = data.map_file(base, "vis")).empty()) load_vis(f, w.visibility);
  if (!(f = data.map_file(base, "spr")).empty()) load_preview(f, w.preview);

  uint32_t next_id = 1;
  for (const PlacedObject& p : w.mission.objects) {
    Object o;
    o.id = next_id++;
    o.type = p.type;
    o.source = p.source;
    o.side = p.side;
    if (p.source == ObjectSource::Town)
      for (size_t i = 0; i < w.mission.towns.size(); ++i)
        if (w.mission.towns[i].index == p.group) o.town = int(i);
    o.hp = p.hp;
    o.facing = 6;  // south
    o.hp_max = p.hp;
    if (p.source == ObjectSource::Team) o.tactic = p.scenario_id;
    o.scenario = p.scenario_id;
    o.anim_variant = o.id * 2654435761u >> 8;
    o.teleport(tile_center_fixed(p.x), tile_center_fixed(p.y));
    w.objects.push_back(o);
  }
  // Each [Town#] section also places its town building ([Town#] Type 0 outpost, 1 village, 2 town, 3 fortress;
  // building type per race: docs/formats.md).
  static const int kTownBuilding[4][3] = {{10, 37, 63}, {11, 38, 64}, {19, 47, 71}, {24, 47, 76}};
  for (const TownDef& t : w.mission.towns) {
    if (t.type < 0 || t.type > 3 || t.race < 0 || t.race > 2) continue;
    Object o;
    o.id = next_id++;
    o.type = kTownBuilding[t.type][t.race];
    o.source = ObjectSource::Town;
    o.side = t.side;
    o.town = int(&t - w.mission.towns.data());
    o.scenario = t.scenario_id;
    o.teleport(tile_center_fixed(t.x), tile_center_fixed(t.y));
    w.objects.push_back(o);
  }
  w.script_side = 0;
  for (const SideDef& sd : w.mission.sides)
    if (!sd.only_comp && !sd.neutral) { w.script_side = sd.index; break; }
  w.local_side = w.script_side;
  w.side_state.assign(8, SideState{});
  for (int a = 0; a < 8; a++)
    for (int b = 0; b < 8; b++) w.relation[a][b] = a == b ? kFriend : kEnemy;
  for (const SideDef& sd : w.mission.sides)
    if (sd.neutral && sd.index >= 0 && sd.index < 8)
      for (int b = 0; b < 8; b++) if (b != sd.index) w.relation[sd.index][b] = w.relation[b][sd.index] = kNeutral;
  for (const SideDef& sd : w.mission.sides)
    if (sd.index >= 0 && sd.index < int(w.side_state.size())) w.side_state[sd.index].money = sd.money;
  return true;
}

}  // namespace aztec::engine
