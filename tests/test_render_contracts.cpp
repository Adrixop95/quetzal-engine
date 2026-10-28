// Invented animation records and light samples; no game installation is needed.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "temp_dir.hpp"
#include "core/endian.hpp"
#include "engine/assets/anim_provider.hpp"
#include "engine/render/light_grid.hpp"
#include "engine/render/scene_renderer.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/sim/sight.hpp"
#include "engine/world/object_state.hpp"

using namespace aztec;
using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)
using Bytes = std::vector<uint8_t>;
static void put(Bytes& b, size_t pos, uint32_t value) { write_le(b.data() + pos, value); }
static Bytes animation(int id) {
  Bytes bytes(0x208 + 128 * 4 * 32);
  std::memcpy(bytes.data(), "Anim", 4); put(bytes, 4, 3);
  for (int slot = 0; slot < 128; ++slot) {
    int start = slot * 4 + 1;
    put(bytes, 8 + slot * 4, start);
    for (int frame = 0; frame < 2; ++frame) {
      size_t p = 0x208 + (start + frame) * 32;
      put(bytes, p, id * 1000 + slot * 2 + frame + 1);
      put(bytes, p + 4, 1); put(bytes, p + 24, 7);
      put(bytes, p + 28, frame == 0 ? 2 : 0);  // two ticks, then indefinite hold
    }
  }
  return bytes;
}

static void provider_controls() {
  // A tiny single-group archive, created privately and deleted after the test.
  Bytes archive(20 + 417 * 4);
  put(archive, 0, 1); put(archive, 4, 0x515);  // animation group 0x515
  put(archive, 8, 20); put(archive, 12, 0); put(archive, 16, 417);
  for (int id = 0; id < 417; ++id) {
    put(archive, 20 + id * 4, uint32_t(archive.size()));
    bool unit_anim = false;
    for (const auto& type : data::kTypeAnims) unit_anim |= type.anim == id || type.anim2 == id;
    if (unit_anim || id == 4 || id == 5 || id == 14 || id == 22 || id == 23 || id == 29 || id == 51 ||
        id == 113 || id == 114 || id == 115 || id == 116 || id == 152 || id == 154 || id == 155 || id == 156 || id == 200) {
      auto bytes = animation(id);
      if (id == 18 || id == 52 || id == 13) {
        for (int slot = 0; slot < 128; ++slot)
          put(bytes, 0x208 + (slot * 4 + 2) * 32 + 28, 10);
      }
      if (id == 23) {
        // Both orientations: finite opening/closing/turning, looping stable phases.
        for (int slot : {1, 3, 4, 6, 8, 9})
          put(bytes, 0x208 + (slot * 4 + 2) * 32 + 28, 3);
      }
      archive.insert(archive.end(), bytes.begin(), bytes.end());
    }
  }
  TempDir directory;
  std::string path = (directory.path() / "animations.xbe").string();
  { std::ofstream file(path, std::ios::binary); file.write(reinterpret_cast<const char*>(archive.data()), archive.size()); }
  AnimSpriteProvider provider; CHECK(provider.open(path));
  for (int orientation : {0, 1}) {
    for (int phase : {1, 3, 4}) {
      CHECK(!provider.boat_phase_complete(phase, orientation, 4));
      CHECK(provider.boat_phase_complete(phase, orientation, 5));
    }
    CHECK(!provider.boat_phase_complete(0, orientation, 100));
    CHECK(!provider.boat_phase_complete(2, orientation, 100));
  }
  AnimSpriteProvider missing;
  CHECK(!missing.boat_phase_complete(1, 0, 100));
  for (uint32_t duration : {0u, uint32_t(-1)}) {
    auto invalid = archive;
    const auto start = read_le<uint32_t>(invalid.data() + 20 + 23 * 4);
    put(invalid, start + 0x208 + (1 * 4 + 2) * 32 + 28, duration);
    const auto invalid_path = (directory.path() / "invalid.xbe").string();
    { std::ofstream file(invalid_path, std::ios::binary); file.write(reinterpret_cast<const char*>(invalid.data()), invalid.size()); }
    AnimSpriteProvider held; CHECK(held.open(invalid_path));
    CHECK(!held.boat_phase_complete(1, 0, 100));
  }
  auto inputs = std::make_shared<SightData>(); CHECK(inputs->sprites.open(path));
  auto rules = std::make_shared<RulesData>(); rules->init_defaults(); rules->set_sight(inputs);
  for (int orientation : {0, 1}) {
    for (int phase : {1, 3, 4}) {
      World world; world.terrain.w = world.terrain.h = 2; world.terrain.cells.resize(4);
      Object boat; boat.id = 1; boat.source = ObjectSource::Boat;
      boat.orientation = orientation; boat.transport_phase = phase;
      world.objects.push_back(boat);
      GameSim sim(rules); sim.init(world);
      Command pause; pause.type = Pct::PauseOn; sim.submit(pause);
      for (int tick = 0; tick < 8; ++tick) { world.begin_tick(); sim.tick(world); }
      CHECK(world.objects[0].anim_tick == 0 && world.objects[0].transport_phase == phase);
      pause.type = Pct::PauseOff; sim.submit(pause);
      for (int tick = 0; tick < 4; ++tick) { world.begin_tick(); sim.tick(world); }
      CHECK(world.objects[0].transport_phase == phase);
      world.begin_tick(); sim.tick(world);
      const auto& result = world.objects[0];
      CHECK(result.transport_phase == (phase == 1 ? 2 : 0));
      CHECK(result.orientation == (phase == 4 ? orientation ^ 1 : orientation));
      CHECK(result.anim_tick == 0);
      for (int tick = 0; tick < 8; ++tick) { world.begin_tick(); sim.tick(world); }
      CHECK(result.transport_phase == (phase == 1 ? 2 : 0));
    }
  }
  // Every initialized type has a built/idle body and a supported walk path.
  // The synthetic tables deliberately supply every slot; selection remains kind-specific.
  for (int type = 0; type < 90; ++type) {
    ObjectState input; input.type_id = type;
    const auto kind = data::kTypeAnims[type].kind;
    if (kind == 11) { CHECK(!provider.frame_for(input).valid()); continue; }
    CHECK(provider.frame_for(input).valid());
    if (kind == 8 || kind == 10) {
      CHECK(provider.height(input) == 0 && provider.vision_mask(input) == 71);
    }
    if (kind != 2 && kind != 7) {
      input.anim_state = int(data::AnimState::Move);
      CHECK(provider.frame_for(input).valid());
    }
  }
  ObjectState timed; timed.type_id = 1; timed.anim_state = int(data::AnimState::Attack);
  timed.body_tick = 42; timed.attack_tick = 0;
  CHECK(provider.frame_for(timed).sprite == 18019);
  timed.attack_tick = 2; CHECK(provider.frame_for(timed).sprite == 18020);
  // The simulation may have returned to idle at tick 8; resource timing lasts 12 ticks.
  timed.anim_state = int(data::AnimState::Idle); timed.attack_tick = 9;
  CHECK(provider.frame_for(timed).sprite == 18020);
  timed.attack_tick = 12; CHECK(provider.frame_for(timed).sprite == 18001);
  timed.anim_state = int(data::AnimState::Die); timed.anim_tick = 12;
  CHECK(provider.frame_for(timed).sprite == 18036); // final Marine corpse frame is retained
  timed.type_id = 4; timed.anim_state = int(data::AnimState::Move); timed.moving = true;
  timed.body_tick = 2; timed.attack_tick = 0;
  CHECK(provider.parts(timed)[1].key.index == 14004);
  CHECK(provider.parts(timed)[3].key.index == 52005);
  timed.attack_tick = 9; CHECK(provider.parts(timed)[3].key.index == 52006);
  timed.attack_tick = 12; CHECK(provider.parts(timed)[3].key.index == 52001);
  CHECK(provider.parts(timed)[1].key.index == 14004); // firing never resets wheel phase
  timed.type_id = 33; timed.angle = 8;
  CHECK(provider.frame_for(timed).sprite == 13002); // unaligned flight seeks dir32
  timed.angle = 0; CHECK(provider.frame_for(timed).sprite == 13004);
  timed.body_tick = int64_t(UINT32_MAX) + 7;
  CHECK(provider.frame_for(timed).valid());
  timed = {}; timed.type_id = 10; timed.anim_state = int(data::AnimState::Construct);
  timed.hp = 34; timed.max_hp = 100;
  CHECK(provider.frame_for(timed).sprite == 200001); // explicit state, even without progress glue
  ObjectState st; st.type_id = 0;
  auto parts = provider.parts(st);
  CHECK(parts.size() == 2 && parts[0].key.shadow && !parts[1].key.shadow);
  CHECK(parts[0].dy == 0 && parts[1].dy == -7 && parts[0].key.shade == 12);
  st.anim_state = int(data::AnimState::Move);
  CHECK(provider.frame_for(st).sprite == 4003);
  st.anim_tick = 1; CHECK(provider.frame_for(st).sprite == 4003);
  st.anim_tick = 2; CHECK(provider.frame_for(st).sprite == 4004);
  st.anim_tick = 100; CHECK(provider.frame_for(st).sprite == 4004);
  st.type_id = 3; st.anim_state = 0; st.anim_tick = 0;
  parts = provider.parts(st);
  CHECK(parts.size() == 4 && parts[0].key.shadow && !parts[1].key.shadow && parts[2].key.shadow && !parts[3].key.shadow);
  CHECK(parts[2].dy == 0 && parts[3].dy == -7);
  CHECK(parts[3].key.index == 51003);
  st.initial_turret = false; CHECK(provider.parts(st)[3].key.index == 51001);
  st.anim_state = int(data::AnimState::Attack); CHECK(provider.parts(st)[3].key.index == 51005);
  st.anim_state = int(data::AnimState::Die);
  CHECK(!provider.frame_for(st).valid() && provider.parts(st).empty());
  auto items = provider.items(st);
  CHECK(items.size() == 3 && items[2].height == -1);
  CHECK(items[1].light == 1);  // independent death decoration, not an invented turret glow
  st.type_id = 7; st.anim_state = 0; st.height = 80;
  CHECK(provider.parts(st)[1].dy == -80 && provider.parts(st)[0].dy == 0);
  st.type_id = 30; st.height = 99; CHECK(provider.height(st) == 0);
  st.anim_state = int(data::AnimState::Attack); st.moving = true;
  CHECK(provider.frame_for(st).sprite == 22003);
  st.moving = false; CHECK(provider.frame_for(st).sprite == 22001);
  st.type_id = 10; st.progress = 1; st.hp = 1; st.max_hp = 100; st.anim_state = 6;
  CHECK(!provider.frame_for(st).valid());
  st.hp = 34; CHECK(provider.frame_for(st).sprite == 200001);
  st.hp = 99; CHECK(provider.frame_for(st).sprite == 200002);
  st.hp = 100; st.progress = 1000; st.anim_state = 0;
  CHECK(provider.frame_for(st).sprite == 200003);
  st.hp = 75; CHECK(provider.items(st).back().parts[1].key.index == 154001);
  CHECK(provider.items(st).back().dy == 1);
  st.hp = 76; CHECK(provider.items(st).size() == 1);
  st.hp = 50; CHECK(provider.items(st).back().parts[1].key.index == 155001);
  st.hp = 25; CHECK(provider.items(st).back().parts[1].key.index == 156001);
  st.construction_overlay = true; CHECK(provider.parts(st).back().key.index == 200013);
  st.anim_state = int(data::AnimState::Die); CHECK(!provider.frame_for(st).valid());
  st.destruction_radius = 97; CHECK(provider.frame_for(st).sprite == 115001);
  st.type_id = -2; st.anim_state = 0; st.transport_phase = 4; st.orientation = 1;
  CHECK(provider.frame_for(st).sprite == 23019);
  st.type_id = -3; CHECK(provider.frame_for(st).sprite == 29003);
  CHECK(provider.vision_mask(st) == 71);
#ifdef AZTEC_TEST_RULES_DIR
  CHECK(provider.load_visual_rules(AZTEC_TEST_RULES_DIR));
  st.type_id = 0; CHECK(provider.vision_mask(st) == 64);
  st.type_id = 10; CHECK(provider.vision_mask(st) == 70);
#endif
}

static void grid_controls() {
  Bytes mask(404, 0); mask[0] = mask[2] = 20;
  // A deliberately asymmetric plane detects wrong origins and axis swaps.
  for (int y = 0; y < 20; ++y) for (int x = 0; x < 20; ++x) mask[4 + y * 20 + x] = x + 2 * y;
  data::LightMask light({mask.data(), mask.size()});
  LightGrid grid; grid.reset(30, 30, 0);
  CHECK(grid.width() == 31 && grid.height() == 32);
  grid.stamp(light, 12 * 48, 12 * 48, false); grid.combine(false);
  CHECK(grid.sample(12 * 48, 12 * 48) == 27);
  CHECK(grid.sample(12 * 48 + 24, 12 * 48) == 27.5f);
  grid.reset(30, 30, 0); grid.stamp(light, 12 * 48 + 24, 12 * 48, false); grid.combine(false);
  CHECK(grid.sample(12 * 48, 12 * 48) == 26);
  grid.reset(30, 30, 150); grid.stamp(light, 12 * 48, 12 * 48, false); grid.combine(false);
  CHECK(grid.sample(12 * 48, 12 * 48) == 152);
  grid = LightGrid{}; grid.reset(30, 30, 100); grid.stamp(light, 12 * 48, 12 * 48, true); grid.combine(true);
  CHECK(grid.sample(12 * 48, 12 * 48) == 73);
  grid.reset(30, 30, 0); grid.combine(true); CHECK(grid.sample(12 * 48, 12 * 48) == 32);
  grid = LightGrid{}; grid.reset(30, 30, 152); grid.combine(false);
  grid.reset(30, 30, 0); grid.combine(true);
  CHECK(grid.sample(12 * 48, 12 * 48) == 0);  // disabling fog must not explore the map
  // Off-map sources, including negative positions, clip instead of shifting inward.
  grid = LightGrid{}; grid.reset(2, 2, 0); grid.stamp(light, -24, -24, false); grid.combine(false);
  CHECK(grid.sample(0, 0) == 28);
  grid.stamp(light, 1e30f, 1e30f, false);
  grid.stamp(data::LightMask(), 0, 0, true);
  CHECK(data::damage_fire_anim(0, 100) == 0);
  CHECK(data::damage_fire_anim(26, 100) == 155 && data::damage_fire_anim(51, 100) == 154);
}
static void ordering_controls() {
  struct Key { float x, y; int height, id; };
  std::vector<Key> keys{{10, 20, 80, 1}, {20, 20, 0, 2}, {0, 21, -1, 3}, {0, 19, 0, 4}, {10, 20, 0, 5}};
  std::stable_sort(keys.begin(), keys.end(), [](auto a, auto b) { return display_before(a.x, a.y, a.height, b.x, b.y, b.height); });
  CHECK(keys[0].id == 3 && keys[1].id == 4 && keys[2].id == 2 && keys[3].id == 1 && keys[4].id == 5);
  CHECK(!display_before(0, 0, -1, 10, 20, -1));
}
// A real SceneRenderer check with invented raw RGB565 sprites. No game data is used.
static void headless_controls() {
  TempDir directory;
  const auto& path = directory.path();
  namespace fs = std::filesystem;
  fs::create_directory(fs::path(path) / "Res");
  auto write = [&](const std::string& name, const Bytes& bytes) {
    std::ofstream file(fs::path(path) / "Res" / (name + ".xbe"), std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  };
  // Body sprites 1 and 2, then animation 4 in a second group.
  Bytes archive(36 + 2 * 4 + 1 * 4);
  put(archive, 0, 2);
  put(archive, 4, 0x44c); put(archive, 8, 36); put(archive, 12, 1); put(archive, 16, 3);
  put(archive, 20, 0x515); put(archive, 24, 44); put(archive, 28, 4); put(archive, 32, 5);
  for (int i = 0; i < 2; ++i) {
    put(archive, 36 + 4 * i, uint32_t(archive.size()));
    Bytes sprite(24 + 8 * 8 * 2);
    put(sprite, 0, 0xe0ac0000); put(sprite, 4, 8 | (8 << 16));
    put(sprite, 8, 0x11); put(sprite, 16, 8 | (8 << 16)); put(sprite, 20, 4 | (4 << 16));
    uint16_t color = i == 0 ? 0xf800 : 0x07e0;
    for (size_t p = 24; p < sprite.size(); p += 2) write_le(sprite.data() + p, color);
    archive.insert(archive.end(), sprite.begin(), sprite.end());
  }
  put(archive, 44, uint32_t(archive.size()));
  auto anim = animation(4);
  for (int slot = 0; slot < 128; ++slot) {
    const size_t p = 0x208 + (slot * 4 + 1) * 32;
    put(anim, p, slot == 10 ? 2 : 1); put(anim, p + 32, 2); put(anim, p + 32 + 28, 10);
  }
  archive.insert(archive.end(), anim.begin(), anim.end());
  write("Units", archive);
  for (const auto* name : {"Pal", "Tiles0", "Tiles1", "common", "fnts", "light"}) write(name, Bytes(4));
  const int initialized = SDL_Init(SDL_INIT_VIDEO); CHECK(initialized == 0);
  if (initialized != 0) return;
  {
    Window win; WindowOptions options; options.width = options.height = 64;
    options.software = true; options.vsync = false;
    const bool created = win.create(options); CHECK(created);
    if (!created) { SDL_Quit(); return; }
    GameData assets; std::string error; CHECK(assets.open(path.string(), error));
    auto provider = std::make_shared<AnimSpriteProvider>();
    CHECK(provider->open((path / "Res" / "Units.xbe").string()));
    World world; Object unit; unit.type = 0; unit.id = 1;
    unit.teleport(to_fixed(32), to_fixed(32)); unit.attack_tick = 0; unit.body_tick = 0;
    unit.has_fired = true; world.objects.push_back(unit);
    SceneRenderer scene(win, assets, world); scene.set_sprite_provider(provider);
    scene.fog().set_enabled(false); scene.opts.labels = scene.opts.markers = false;
    Camera camera; camera.view_w = camera.view_h = 64;
    auto pixels = [&](float alpha) {
      scene.draw(camera, alpha);
      std::vector<uint32_t> result(64 * 64);
      CHECK(SDL_RenderReadPixels(win.renderer(), nullptr, SDL_PIXELFORMAT_ARGB8888, result.data(), 64 * 4) == 0);
      CHECK(scene.stats.sprites == 1);
      return result;
    };
    auto first = pixels(0); CHECK((first[24 * 64 + 32] & 0xffffff) == 0xff0000);
    world.objects[0].body_angle = 32;
    CHECK((pixels(0)[24 * 64 + 32] & 0xffffff) == 0x00ff00);
    CHECK(world.objects[0].facing == 0 && world.objects[0].body_angle == 32);
    world.objects[0].body_angle = 0;
    world.objects[0].attack_tick = 9; // beyond the legacy eight-tick attack state
    auto firing = pixels(1); CHECK((firing[24 * 64 + 32] & 0xffffff) == 0x00ff00);
    for (int i = 0; i < 20; ++i) CHECK(pixels(float(i) / 20) == firing);
    CHECK(world.objects[0].attack_tick == 9 && world.objects[0].body_tick == 0);
    world.objects[0].attack_tick = 12;
    CHECK(pixels(0) == first); // resource duration completes, returning to idle
  }
  SDL_Quit();
}
int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--headless") headless_controls();
  else { provider_controls(); grid_controls(); ordering_controls(); }
  return failures ? 1 : 0;
}
