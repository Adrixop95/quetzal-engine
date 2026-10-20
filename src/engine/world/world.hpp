// World state shared by simulation (writes) and renderer/UI (reads).
// Positions are fixed point (pixel * 256) so the sim stays deterministic; every object keeps its
// previous and current tick position so the renderer can interpolate between 12 Hz ticks.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/map.hpp"
#include "core/mapfiles.hpp"
#include "core/visfile.hpp"
#include "engine/assets/game_data.hpp"
#include "engine/assets/mission.hpp"

namespace aztec::engine {

constexpr int kTilePx = 48;
using Fixed = int32_t;  // pixel * 256
constexpr Fixed to_fixed(int px) { return Fixed(px) * 256; }
constexpr Fixed tile_center_fixed(int tile) { return to_fixed(tile * kTilePx + kTilePx / 2); }

struct Vec2f { float x = 0, y = 0; };

struct Object {
  uint32_t id = 0;
  int type = -1;         // UGI id, -1 = marker without a type
  ObjectSource source = ObjectSource::Team;
  int side = 0;
  int hp = 0;
  Fixed prev_x = 0, prev_y = 0;  // position at the previous tick (world px * 256)
  Fixed cur_x = 0, cur_y = 0;    // position at the current tick
  uint8_t facing = 0;
  uint16_t anim_state = 0;
  uint32_t anim_tick = 0;      // ticks since anim_state started; advanced by World::begin_tick()
  uint32_t anim_variant = 0;

  Vec2f interpolated(float alpha) const {
    return {(prev_x + (cur_x - prev_x) * alpha) / 256.f, (prev_y + (cur_y - prev_y) * alpha) / 256.f};
  }
  void set_anim(uint16_t state) { if (state != anim_state) { anim_state = state; anim_tick = 0; } }
  void teleport(Fixed x, Fixed y) { prev_x = cur_x = x; prev_y = cur_y = y; }
};

struct World {
  MissionDef mission;
  TerrainMap terrain;
  Grid<uint8_t> passability;     // may be empty
  Grid<uint16_t> resources;      // .rm, may be empty
  VisFile visibility;            // .vis per-side light layers, may be empty
  Sprite preview;                // .spr minimap preview; w == 0 if absent
  std::vector<Object> objects;
  uint64_t tick = 0;
  int local_side = 0;            // side whose fog layer is shown / whose objects are always visible
  uint32_t fog_version = 1;      // bump when the local side's .vis layer changes

  int width_px() const { return terrain.w * kTilePx; }
  int height_px() const { return terrain.h * kTilePx; }
  int side_color(int side) const {
    for (const auto& s : mission.sides)
      if (s.index == side) return s.color;
    return side;
  }
  // Called by the driver right before each simulation tick: prev := cur for everything.
  void begin_tick() {
    for (auto& o : objects) { o.prev_x = o.cur_x; o.prev_y = o.cur_y; o.anim_tick++; }
  }
};

// Loads map + side files and spawns objects from the mission. Fills `err` on failure.
bool load_world(const GameData& data, const std::string& mission_spec, World& out, std::string& err);

}  // namespace aztec::engine
