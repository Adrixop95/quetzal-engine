// Invented animation records and light samples; no game installation is needed.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include "engine/assets/anim_provider.hpp"
#include "engine/render/light_grid.hpp"

using namespace aztec;
using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)
using Bytes = std::vector<uint8_t>;
static void put(Bytes& b, size_t pos, uint32_t value) { std::memcpy(b.data() + pos, &value, 4); }
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
  Bytes archive(20 + 317 * 4);
  put(archive, 0, 1); put(archive, 4, 0x515);  // animation group 0x515
  put(archive, 8, 20); put(archive, 12, 0); put(archive, 16, 317);
  for (int id = 0; id < 317; ++id) {
    put(archive, 20 + id * 4, uint32_t(archive.size()));
    if (id == 4 || id == 5 || id == 14 || id == 22 || id == 23 || id == 29 || id == 51 ||
        id == 113 || id == 114 || id == 115 || id == 116 || id == 152 || id == 154 || id == 155 || id == 156 || id == 200) {
      auto bytes = animation(id); archive.insert(archive.end(), bytes.begin(), bytes.end());
    }
  }
  char path[] = "/tmp/aztec-render-XXXXXX";
  int fd = mkstemp(path); CHECK(fd >= 0); if (fd < 0) return;
  close(fd);
  { std::ofstream file(path, std::ios::binary); file.write(reinterpret_cast<const char*>(archive.data()), archive.size()); }
  AnimSpriteProvider provider; CHECK(provider.open(path)); unlink(path);
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
int main() { provider_controls(); grid_controls(); ordering_controls(); return failures ? 1 : 0; }
