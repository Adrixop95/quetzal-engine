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
    o.hp = p.hp;
    o.teleport(tile_center_fixed(p.x), tile_center_fixed(p.y));
    w.objects.push_back(o);
  }
  return true;
}

}  // namespace aztec::engine
