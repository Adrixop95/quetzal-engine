#include "engine/save_import.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace aztec::engine {
namespace {
struct Invalid : std::runtime_error { using std::runtime_error::runtime_error; };
const SaveSection& section(const SaveGame& s, const char* name) {
  const auto* p = s.section(name);
  if (!p) throw Invalid(std::string("Missing save section: ") + name);
  return *p;
}
uint32_t value(const SaveRecord& r, int offset) {
  const auto* f = r.field(offset);
  if (!f || f->size != 4 || f->kind == SaveValue::bytes) throw Invalid("Missing scalar save field");
  return f->value;
}
const SaveField& named(const SaveRecord& r, const char* name) {
  for (const auto& f : r.fields) if (f.name == name) return f;
  throw Invalid(std::string("Missing save field: ") + name);
}
std::vector<uint8_t> block(const SaveGame& s, const SaveField& f) {
  if (f.kind != SaveValue::bytes || f.offset > s.bytes.size() || f.size > s.bytes.size() - f.offset)
    throw Invalid("Invalid save block range");
  return {s.bytes.begin() + f.offset, s.bytes.begin() + f.offset + f.size};
}
int signed_word(uint32_t v) { return std::bit_cast<int32_t>(v); }
Fixed pixel_position(uint32_t v) {
  int64_t p = int64_t(signed_word(v)) * 256;
  if (p < std::numeric_limits<Fixed>::min() || p > std::numeric_limits<Fixed>::max())
    throw Invalid("Save position exceeds engine coordinate range");
  return Fixed(p);
}
std::string mission_name(const SaveGame& s) {
  auto path = s.header.map_path;
  auto pos = path.find_last_of("/\\");
  if (pos != std::string::npos) path.erase(0, pos + 1);
  if (path.empty() || path == "." || path == ".." || path.find(':') != std::string::npos)
    throw Invalid("Invalid saved mission name");
  return path;
}
struct Group { int side = 0, town = -1; };
}

bool import_original_save(const SaveGame& save, World& world, std::string& error) {
  error.clear();
  try {
    if (world.terrain.w <= 0 || world.terrain.h <= 0) throw Invalid("Mission map is empty");
    auto saved_name = mission_name(save);
    auto dot = saved_name.find_last_of('.');
    if (dot != std::string::npos) saved_name.resize(dot);
    auto loaded_name = world.mission.file_name;
    for (auto* name : {&saved_name, &loaded_name})
      for (auto& c : *name) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (saved_name != loaded_name) throw Invalid("Save does not match the loaded mission");
    World w = world;
    const auto& sides = section(save, "sides").records;
    if (sides.empty() || sides.size() > 8 || save.globals[0] >= sides.size())
      throw Invalid("Unsupported save side count or local side");
    w.objects.clear(); w.effects.clear(); w.selection.clear(); w.messages.clear();
    w.original_unit_ids.clear();
    w.side_state.assign(8, {});
    w.visibility.sides.clear();
    w.mission.objects.clear();
    auto initial_sides = w.mission.sides;
    w.mission.sides.assign(sides.size(), {});
    for (size_t i = 0; i < sides.size(); ++i) {
      const auto& r = sides[i];
      auto& side = w.mission.sides[i];
      for (const auto& initial : initial_sides) if (initial.index == int(i)) { side = initial; break; }
      side.index = int(i);
      side.color = int(value(r, 8) & 0xff);
      if (side.color >= 8) throw Invalid("Invalid saved side colour");
      w.side_state[i].money = signed_word(value(r, 0x698));
      side.money = w.side_state[i].money;
      auto disabled = block(save, named(r, "disabled_types"));
      if (disabled.size() != 0x5b) throw Invalid("Invalid saved type restrictions");
      side.disabled.clear();
      for (int t = 0; t < 90; ++t) if (disabled[size_t(t)]) side.disabled.push_back(t);
      uint32_t fw = named(r, "fog_width").value, fh = named(r, "fog_height").value;
      bool tile_grid = fw == uint32_t(w.terrain.w) && fh == uint32_t(w.terrain.h);
      bool border_grid = fw == uint32_t(w.terrain.w) + 1 && fh == uint32_t(w.terrain.h) + 1;
      if (!tile_grid && !border_grid)
        throw Invalid("Saved fog dimensions do not match the mission map");
      auto fog = block(save, named(r, "fog"));
      if (uint64_t(fw) * fh != fog.size()) throw Invalid("Invalid saved fog size");
      if (border_grid) {
        // The renderer/simulation use one sample per tile. Keep each top-left
        // sample; the original border and stride remain in original_save.
        std::vector<uint8_t> tiles(size_t(w.terrain.w) * w.terrain.h);
        for (int y = 0; y < w.terrain.h; ++y)
          std::copy_n(fog.begin() + size_t(y) * fw, w.terrain.w, tiles.begin() + size_t(y) * w.terrain.w);
        fog = std::move(tiles);
      }
      w.visibility.sides.push_back({w.terrain.w, w.terrain.h, std::move(fog)});
    }
    std::unordered_map<uint32_t, Group> groups;
    auto add_group = [&](const SaveRecord& r, int town) {
      uint32_t owner = value(r, 8);
      if (!owner || owner > sides.size()) throw Invalid("Invalid save group owner");
      if (!r.id || !groups.emplace(r.id, Group{int(owner - 1), town}).second)
        throw Invalid("Duplicate or null save group ID");
    };
    const auto& towns = section(save, "towns").records;
    auto initial_towns = w.mission.towns;
    w.mission.towns.assign(towns.size(), {});
    for (size_t i = 0; i < towns.size(); ++i) {
      if (!(towns[i].id & 0x80000000u) || !(towns[i].id & 0x7fffffffu)) throw Invalid("Invalid saved town ID");
      add_group(towns[i], int(i));
      auto& town = w.mission.towns[i];
      town.index = int(i);
      town.side = groups.at(towns[i].id).side;
      town.race = w.mission.sides[size_t(town.side)].race;
    }
    for (const auto& r : section(save, "tactics").records) {
      if (r.id & 0x80000000u) throw Invalid("Invalid saved tactic ID");
      add_group(r, -1);
    }
    std::unordered_map<uint32_t, uint32_t> units;
    for (const auto& r : section(save, "units").records) {
      if (r.type >= 90 || r.kind != uint32_t(save_unit_kind(r.type))) throw Invalid("Invalid saved unit type");
      Object o;
      o.id = uint32_t(w.objects.size() + 1);
      if (!r.id || !units.emplace(r.id, o.id).second) throw Invalid("Duplicate or null saved unit ID");
      o.type = int(r.type);
      uint32_t owner = value(r, 0x30);
      if (owner) {
        auto it = groups.find(owner);
        if (it == groups.end()) throw Invalid("Unresolved save unit owning group");
        o.side = it->second.side; o.town = it->second.town;
      } else {
        // Ungrouped cameras/vessels have no known side ownership.
        o.side = -1;
      }
      o.source = o.town >= 0 ? ObjectSource::Town : ObjectSource::Team;
      if (r.kind == 8) o.source = ObjectSource::Boat;
      if (r.kind == 10) o.source = ObjectSource::Johnka;
      o.hp = signed_word(value(r, 0x38)); o.secondary_hp = signed_word(value(r, 0x3c));
      o.regen_service_ticks = std::max(0, signed_word(value(r, 0x54)));
      o.alive = o.hp > 0;
      o.teleport(pixel_position(value(r, 0x24)), pixel_position(value(r, 0x28)));
      if (r.kind == 0) o.teleport(signed_word(value(r, 0x6c)), signed_word(value(r, 0x70)));
      o.anim_variant = o.id * 2654435761u >> 8;
      w.objects.push_back(o);
      w.original_unit_ids.push_back(r.id);
    }
    // Town hall references are unit-pool IDs, independent of record order.
    static constexpr int halls[4][3] = {{10, 37, 63}, {11, 38, 64}, {19, 47, 71}, {24, 47, 76}};
    for (size_t i = 0; i < towns.size(); ++i) {
      uint32_t hall = value(towns[i], 0xf0);
      if (!hall) continue;
      auto it = units.find(hall);
      if (it == units.end()) throw Invalid("Unresolved saved town hall");
      auto& o = w.objects[it->second - 1];
      auto& t = w.mission.towns[i];
      if (o.town != int(i) || o.side != t.side) throw Invalid("Saved town hall has a different owner");
      t.x = o.cur_x / 256 / kTilePx; t.y = o.cur_y / 256 / kTilePx;
      // Reuse only matching mission metadata; build-tree level semantics remain unresolved.
      for (const auto& initial : initial_towns)
        if (initial.side == t.side && initial.x == t.x && initial.y == t.y) {
          t.type = initial.type; t.race = initial.race; t.scenario_id = initial.scenario_id; break;
        }
      for (int level = 0; level < 4; ++level)
        for (int race = 0; race < 3; ++race)
          if (halls[level][race] == o.type) {
            t.race = race;
            // Type 47 represents two levels; keep matching mission metadata when ambiguous.
            if (o.type != 47 || (t.type != 2 && t.type != 3)) t.type = level;
          }
    }
    w.local_side = w.script_side = int(save.globals[0]);
    w.tick = save.globals[4];
    const auto& day = section(save, "daylight").records;
    if (day.size() != 1) throw Invalid("Invalid saved daylight section");
    w.day_clock = int(named(day[0], "time").value % kDayTicks);
    w.town_speed.assign(towns.size(), 1); w.repair_credits.assign(towns.size(), 0);
    w.outcome = kRunning;
    std::fill(std::begin(w.side_outcome), std::end(w.side_outcome), 0);
    w.center_view = false;
    ++w.fog_version;
    w.original_save = std::make_shared<SaveGame>(save);
    world = std::move(w);
    return true;
  } catch (const Invalid& e) { error = e.what(); return false; }
}

bool load_original_save(const GameData& data, const std::string& path, World& world, std::string& error) {
  SaveGame save; SaveError decoded_error;
  if (!load_savegame(path, save, &decoded_error)) {
    error = "Cannot decode original save at byte " + std::to_string(decoded_error.offset) + ": " + decoded_error.message;
    return false;
  }
  try {
    World fresh;
    // Discard directory components from Windows/absolute paths; never follow a saved host path.
    auto name = mission_name(save);
    auto mission = find_file_ci(data.maps_dir(), name);
    if (mission.empty()) { error = "Saved mission not found in Maps: " + name; return false; }
    if (!load_world(data, mission, fresh, error) || !import_original_save(save, fresh, error)) return false;
    world = std::move(fresh);
    return true;
  } catch (const Invalid& e) { error = e.what(); return false; }
}
} // namespace aztec::engine
