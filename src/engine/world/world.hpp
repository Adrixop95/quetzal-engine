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
  // --- simulation state (written only by the Simulation) ---
  int32_t hp_max = 0;
  uint8_t order = 0;           // sim::Order
  uint32_t target = 0;         // object id for attack orders (0 = none)
  int32_t dest_x = 0, dest_y = 0;  // order destination, world px
  int32_t cooldown = 0;        // ticks until the next shot
  int32_t corpse_ticks = 0;    // ticks since death
  int32_t tactic = -1;         // team (tactic) number for Team objects, changed by joinTactic
  int32_t scenario = -1;       // ScenarioID of defences / vessels (script references)
  uint32_t last_hit_by = 0;    // id of the last attacker (script condition tactic(N).attacked)
  uint8_t resume = 0;          // attack order interrupted an attack-move: resume it afterwards
  int32_t build_elapsed = 0, build_total = 0;  // construction timer (ticks)
  int32_t progress = 1000;     // construction 0..1000 (1000 = finished); buildings only
  int32_t mover = -1;          // agent index in the movement system, -1 = does not walk
  bool alive = true;
  bool removed = false;        // gone (not drawn, not simulated); ids stay stable

  Vec2f interpolated(float alpha) const {
    return {(prev_x + (cur_x - prev_x) * alpha) / 256.f, (prev_y + (cur_y - prev_y) * alpha) / 256.f};
  }
  void set_anim(uint16_t state) { if (state != anim_state) { anim_state = state; anim_tick = 0; } }
  void teleport(Fixed x, Fixed y) { prev_x = cur_x = x; prev_y = cur_y = y; }
};

struct QueueItem { int type = 0; int remaining = 0; int total = 0; };

// Per-side economy and command state.
struct SideState {
  int money = 0;
  bool build_paused = false;
  std::vector<QueueItem> queue;         // training / construction queue (production time in ticks)
  std::vector<uint32_t> groups[10];     // control groups 0..9
};

enum Relation : uint8_t { kEnemy = 0, kFriend = 1, kNeutral = 2 };
enum Outcome : uint8_t { kRunning = 0, kVictory = 1, kDefeat = 2 };

struct Message { std::string text; int ticks_left = 0; };

struct Selection {
  std::vector<uint32_t> ids;
  bool contains(uint32_t id) const { for (uint32_t i : ids) if (i == id) return true; return false; }
  void clear() { ids.clear(); }
};

struct World {
  MissionDef mission;
  TerrainMap terrain;
  Grid<uint8_t> passability;     // may be empty
  Grid<uint16_t> resources;      // .rm, may be empty
  VisFile visibility;            // .vis per-side light layers, may be empty
  Sprite preview;                // .spr minimap preview; w == 0 if absent
  std::vector<Object> objects;
  std::vector<SideState> side_state;   // indexed by side number
  uint8_t relation[8][8] = {};          // relation[a][b]: how side a regards side b (enemy / friend / neutral)
  uint8_t outcome = kRunning;           // mission result for the local side (scripts: side(local).victory / defeat)
  uint8_t side_outcome[8] = {};         // victory / defeat flags per side
  std::vector<Message> messages;        // printString texts (UTF-8), shown for a few seconds
  bool center_view = false;             // centerView / cameraActivate request for the UI (consumed by the engine loop)
  int center_x = 0, center_y = 0;       // world px
  uint32_t center_serial = 0;
  Selection selection;           // local player's selected objects (UI state, not simulation state)
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
  // Ids are 1-based indices into `objects` (objects are never erased, only marked `removed`).
  bool hostile(int a, int b) const { return a >= 0 && b >= 0 && a < 8 && b < 8 && a != b && relation[a][b] == kEnemy; }
  const Object* find(uint32_t id) const { return id >= 1 && id <= objects.size() ? &objects[id - 1] : nullptr; }
  Object* find_mut(uint32_t id) { return id >= 1 && id <= objects.size() ? &objects[id - 1] : nullptr; }
  // Called by the driver right before each simulation tick: prev := cur for everything.
  void begin_tick() {
    for (auto& o : objects) { o.prev_x = o.cur_x; o.prev_y = o.cur_y; o.anim_tick++; }
  }
};

// Loads map + side files and spawns objects from the mission. Fills `err` on failure.
bool load_world(const GameData& data, const std::string& mission_spec, World& out, std::string& err);

}  // namespace aztec::engine
