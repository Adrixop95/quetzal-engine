// Synthetic state gallery: all records and pixels below are invented.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include "temp_dir.hpp"
#include "core/endian.hpp"
#include "engine/assets/anim_provider.hpp"
#include "engine/render/scene_renderer.hpp"
#include "engine/ui/cursor.hpp"
#include "engine/world/object_state.hpp"

using namespace aztec;
using namespace aztec::engine;
using Bytes = std::vector<uint8_t>;
static int failures;
static std::string label;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s line %d: %s\n", label.c_str(), __LINE__, #c); ++failures; } } while (0)
static void put(Bytes& b, size_t p, uint32_t v) { write_le(b.data() + p, v); }
static Bytes sprite(int w, int h, int frames, int ox, int oy, uint16_t color) {
  Bytes b(24 + size_t(w) * h * frames * 2);
  put(b, 0, 0xe0ac0000); put(b, 4, uint32_t(w | (h * frames << 16)));
  put(b, 8, 0x11); put(b, 16, uint32_t(w | (h << 16)));
  put(b, 20, uint16_t(ox) | (uint32_t(uint16_t(oy)) << 16));
  for (size_t p = 24; p < b.size(); p += 2) {
    const auto frame = uint16_t((p - 24) / (size_t(w) * h * 2));
    write_le(b.data() + p, uint16_t(color + (frame << 5)));
  }
  return b;
}
struct Group { uint32_t id, first, last; std::map<uint32_t, Bytes> items; };
static void archive(const std::filesystem::path& path, const std::vector<Group>& groups) {
  size_t table = 4 + 16 * groups.size(), total = table;
  for (const auto& g : groups) total += 4 * (g.last - g.first);
  Bytes b(total); put(b, 0, uint32_t(groups.size()));
  for (size_t n = 0; n < groups.size(); ++n) {
    const auto& g = groups[n];
    put(b, 4 + 16 * n, g.id); put(b, 8 + 16 * n, uint32_t(table));
    put(b, 12 + 16 * n, g.first); put(b, 16 + 16 * n, g.last);
    for (uint32_t id = g.first; id < g.last; ++id) {
      put(b, table, uint32_t(b.size())); table += 4;
      if (auto it = g.items.find(id); it != g.items.end()) b.insert(b.end(), it->second.begin(), it->second.end());
    }
  }
  std::ofstream f(path, std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()), b.size());
}
static Bytes animation() {
  Bytes b(0x208 + 128 * 34 * 32); std::memcpy(b.data(), "Anim", 4); put(b, 4, 3);
  for (int slot = 0; slot < 128; ++slot) {
    const int start = slot * 34 + 1; put(b, 8 + slot * 4, start);
    for (int frame = 0; frame < 32; ++frame) {
      const size_t p = 0x208 + size_t(start + frame) * 32;
      put(b, p, 1 + slot * 32 + frame); put(b, p + 24, 7);
      put(b, p + 28, frame == 0 ? 2 : 1);
    }
  }
  return b;
}
// Independent expected selectors, expressed as a table of kind/state contracts.
// V contracts are cited in docs/rendering_animation_second_pass.md; fixture
// durations, dimensions, colours and the presence of all slots are G controls.
static int expected(int kind, int state, int direction, int tick, bool turret) {
  int slot = -1, frame = tick < 2 ? 0 : tick - 1;
  const int cyclic = tick % 33 < 2 ? 0 : tick % 33 - 1;
  if (kind == 8) return 1 + cyclic;
  if (kind == 10) return 1 + cyclic;
  if (state == 3) {
    if (kind == 0) { slot = 17; frame = std::min(31, frame); }
    if (kind == 2 || kind == 7) { slot = 0; if (tick >= 33) return 0; }
    if (kind == 3 || kind == 5) { slot = 17; if (tick >= 33) return 0; }
  } else if (kind == 0) {
    if (state == 0) { slot = 0; frame = direction; }
    if (state == 1) { slot = 1 + direction; frame = cyclic; }
    if (state == 2) { slot = tick < 33 ? 9 + direction : 0; if (tick >= 33) frame = direction; }
    if (state == 4) { slot = tick < 33 ? 20 : 0; if (tick >= 33) frame = direction; }
  } else if (kind == 1) {
    if (state == 0 || state == 2) { slot = 0; frame = 0; }
    if (state == 1) { slot = 1 + direction; frame = cyclic; }
  } else if (kind == 2 || kind == 7) {
    if (state == 0 || state == 2) { slot = 1; frame = cyclic; }
    if (kind == 7 && state == 2 && !turret && tick < 33) { slot = 8; frame = tick < 2 ? 0 : tick - 1; }
  } else if (kind == 3 || kind == 4 || kind == 5 || kind == 9) {
    if (state == 0 || state == 2) { slot = 0; frame = direction * 4; }
    if (state == 1) { slot = 1 + direction; frame = cyclic; }
    if (state == 1 && kind == 9 && tick >= 33) { slot = 0; frame = direction * 4; }
    if (state == 2 && (kind == 4 || kind == 9) && tick < 33) { slot = 9 + direction; frame = tick < 2 ? 0 : tick - 1; }
  } else if (kind == 6) {
    if (state == 0 || state == 2) { slot = 1 + direction * 4; frame = 0; }
    if (state == 1) { slot = 1 + direction * 4; frame = cyclic; }
  }
  // Fidget falls back to idle if the kind has no dedicated sequence.
  if (state == 4 && kind != 0 && kind != 11) return expected(kind, 0, direction, tick, turret);
  return slot < 0 ? 0 : 1 + slot * 32 + frame;
}
struct Placement { SpriteKey key; int frame; SDL_FRect rect; };
static void gallery(const std::filesystem::path& path) {
  Window win; WindowOptions options; options.width = options.height = 192;
  options.software = true; options.vsync = false; CHECK(win.create(options));
  if (!win.renderer()) return;
  GameData assets; std::string error; CHECK(assets.open(path.string(), error));
  auto provider = std::make_shared<AnimSpriteProvider>(); CHECK(provider->open((path / "Res/Units.xbe").string()));
  World world; SceneRenderer scene(win, assets, world); scene.set_sprite_provider(provider);
  scene.fog().set_enabled(false); scene.opts.markers = scene.opts.labels = false;
  Camera cam; cam.view_w = cam.view_h = 192;
  std::vector<Placement> placements;
  scene.observe_sprite = [&](const SpriteKey& key, int frame, const SDL_FRect& rect) { placements.push_back({key, frame, rect}); };
  auto draw = [&] {
    placements.clear(); scene.draw(cam, 1);
    scene.font().draw(win.renderer(), 2, 2, label, 1, {255, 255, 255, 255}, false);
    std::vector<uint32_t> pixels(192 * 192);
    CHECK(SDL_RenderReadPixels(win.renderer(), nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(), 192 * 4) == 0);
    return pixels;
  };
  int cases = 0;
  for (int type = 0; type < 90; ++type) {
    const auto info = data::kTypeAnims[type];
    for (int state = 0; state < 5; ++state) for (int dir = 0; dir < 8; ++dir) for (int tick : {0, 2, 34}) {
      label = "TYPE " + std::to_string(type) + " STATE " + std::to_string(state) + " DIR " + std::to_string(dir) + " TICK " + std::to_string(tick);
      Object o; o.id = 1; o.type = type; o.teleport(to_fixed(96), to_fixed(112));
      o.body_angle = dir * 32; o.turret_angle = 0; o.anim_state = uint8_t(state);
      o.anim_tick = tick; o.body_tick = tick; o.attack_tick = -1; o.render_height = 19; o.hp = o.hp_max = 100; o.destruction_radius = 97;
      world.objects = {o};
      auto st = object_sprite_state(o, 0);
      const int index = expected(info.kind, state, dir, tick, info.anim2 != 0);
      CHECK(provider->frame_for(st).sprite == index);
      auto pixels = draw();
      if (index) {
        CHECK(placements.size() >= 2);
        if (placements.size() < 2) continue;
        CHECK(placements[0].key.shadow && !placements[1].key.shadow);
        CHECK(placements[1].key.index == uint32_t(index));
        const int height = info.kind == 3 || info.kind == 5 ? 19 : info.kind == 1 || info.kind == 8 || info.kind == 10 ? 0 : 7;
        CHECK(placements[0].rect.x == 95 && placements[0].rect.y == 108);
        CHECK(placements[1].rect.x == 95 && placements[1].rect.y == 108 - height);
        CHECK(placements[1].rect.w == 3 && placements[1].rect.h == 5);
        CHECK((pixels[size_t(108 - height) * 192 + 95] & 0xffffff) != 0x080a0e);
      }
      CHECK(world.objects[0].anim_tick == uint32_t(tick));
      ++cases;
    }
  }
  // V: the nearest dir32 cell, rather than exact dir8 alignment, gates cycles.
  for (int type : {7, 33, 83}) for (int angle : {0, 3, 4, 27, 28, 31, 32, 35, 36, 251, 252, 255}) {
    label = "ALIGNMENT " + std::to_string(type) + " ANGLE " + std::to_string(angle);
    ObjectState st; st.type_id = type; st.anim_state = 1; st.anim_tick = 2; st.angle = angle;
    const bool aligned = angle == 0 || angle == 3 || angle == 28 || angle == 31 || angle == 32 || angle == 35 || angle == 252 || angle == 255;
    const int dir8 = ((angle + 16) / 32) % 8, dir32 = ((angle + 4) / 8) % 32;
    CHECK(provider->frame_for(st).sprite == (aligned ? 1 + (1 + dir8) * 32 + 1 : 1 + dir32));
    st.cycle_ready = false; CHECK(provider->frame_for(st).sprite == 1 + dir32);
  }
  label = "CONSTRUCTION HP SEEK";
  Object building; building.id = 2; building.type = 10;
  building.teleport(to_fixed(96), to_fixed(112)); building.hp_max = 100;
  building.progress = 500; building.anim_state = 6;
  for (int hp : {1, 34, 99}) {
    building.hp = hp; world.objects = {building}; draw();
    if (hp == 1) CHECK(placements.empty());
    else {
      CHECK(placements.size() == 2);
      CHECK(placements[1].key.index == uint32_t(hp == 34 ? 11 : 32));
      CHECK(placements[1].rect.y == 101);
    }
  }
  label = "BUILDING DAMAGE FIRE";
  building.progress = 1000; building.anim_state = 0; building.hp = 75;
  world.objects = {building}; draw(); CHECK(placements.size() == 4);
  CHECK(placements[3].rect.y == 109); // separate fire item at ground Y+1
  label = "DEFENCE CONSTRUCTION OVERLAY";
  building.type = 23; building.hp = 100; building.construction_overlay = true;
  world.objects = {building}; draw(); CHECK(placements.size() == 5);
  CHECK(placements.back().key.index == 193 && placements.back().rect.y == 101);
  label = "SELECTION AND HEALTH ART";
  Object selected; selected.id = 3; selected.type = 0; selected.hp = 50; selected.hp_max = 100;
  selected.teleport(to_fixed(96), to_fixed(112)); world.objects = {selected}; world.selection.ids = {3};
  scene.opts.view_counter = 3; auto selected_pixels = draw();
  CHECK(placements.size() == 4 && placements[0].key.index == 309 && placements[0].frame == 3);
  CHECK(placements[3].key.index == 263 && placements[3].frame == 1 && placements[3].rect.w == 8);
  CHECK((selected_pixels[59 * 192 + 88] & 0xffffff) == 0x080000);
  CHECK((selected_pixels[59 * 192 + 96] & 0xffffff) != 0x080000);
  label = "SELECTION IN DISPLAY ORDER";
  Object behind = selected; behind.id = 4; behind.teleport(to_fixed(96), to_fixed(111));
  world.objects = {selected, behind}; draw();
  CHECK(placements.size() == 6 && placements[0].key.shadow && placements[2].key.index == 309);
  label = "FERRY SELECTION EXCLUSION";
  Object ferry = selected; ferry.type = 84; world.objects = {ferry};
  auto selected_ferry_pixels = draw();
  CHECK(std::none_of(placements.begin(), placements.end(), [](const auto& p) {
    return p.key.archive == ArchiveId::Common && (p.key.index == 309 || p.key.index == 314);
  }));
  world.selection.clear(); CHECK(draw() == selected_ferry_pixels);
  world.selection.clear(); world.objects.clear();
  label = "PROJECTILE HEIGHT AND COMPLETION";
  Effect effect; effect.anim = 157; effect.slot = 2; effect.kind = data::EffectClass::FireThrow;
  effect.x = effect.prev_x = to_fixed(96); effect.y = effect.prev_y = to_fixed(112);
  effect.height = 20; effect.age = 2; world.effects = {effect}; draw();
  CHECK(placements.size() == 2 && placements[1].key.index == 66);
  CHECK(placements.size() == 2 && placements[0].rect.y == 108 && placements[1].rect.y == 88);
  world.effects[0].age = 33; draw(); CHECK(placements.empty());
  world.effects.clear();
  label = "CAMERA ZOOM AND HOTSPOT";
  selected.hp = 100; selected.teleport(to_fixed(52), to_fixed(60)); world.objects = {selected};
  cam.x = 4; cam.y = 8; cam.zoom = 2; draw();
  CHECK(placements[1].rect.x == 94 && placements[1].rect.y == 82);
  CHECK(placements[1].rect.w == 6 && placements[1].rect.h == 10);
  cam.x = cam.y = 0; cam.zoom = 1;
  world.objects.clear(); world.terrain.w = world.terrain.h = 1; world.terrain.cells = {1};
  label = "WATER VIEW CLOCK";
  scene.opts.view_counter = 3; auto water = draw();
  CHECK(placements.size() == 1 && placements[0].frame == 3);
  CHECK(placements[0].rect.x == 0 && placements[0].rect.y == 0 && placements[0].rect.w == 48);
  world.tick = 999; CHECK(draw() == water); // simulation cannot advance view animation
  scene.opts.view_counter = 8; CHECK(draw() != water); CHECK(placements[0].frame == 0);
  label = "OVERLAY WIND";
  world.terrain.cells = {1u | (2u << 16)}; scene.opts.wind = data::WindStrength::Slow; scene.opts.wind_counter = 6;
  draw(); CHECK(placements.size() == 3 && placements[1].frame == 3 && placements[2].frame == 3);
  CHECK(placements[2].rect.x == 23 && placements[2].rect.y == 20);
  // Cursor IDs are tested through rendered pixels, including every diagonal.
  Cursor cursor; CHECK(cursor.init(win.renderer(), scene.store()));
  const int ids[] = {1, 0, 2, 5, 14, 21, 20, 19, 18, 17, 16, 15, 4};
  static_assert(sizeof ids / sizeof *ids == int(CursorKind::Count), "cursor expectations");
  for (int kind = 0; kind < int(CursorKind::Count); ++kind) {
    label = "CURSOR " + std::to_string(kind);
    SDL_SetRenderDrawColor(win.renderer(), 0, 0, 0, 255); SDL_RenderClear(win.renderer());
    cursor.draw(win.renderer(), scene.store(), 50, 50, CursorKind(kind), 2, 0);
    std::vector<uint32_t> pixels(192 * 192);
    SDL_RenderReadPixels(win.renderer(), nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(), 192 * 4);
    const uint32_t r = uint32_t(ids[kind] + 1) * 255 / 31;
    CHECK(((pixels[42 * 192 + 48] >> 16) & 255) == r);
    CHECK((pixels[41 * 192 + 48] & 0xffffff) == 0);
  }
  std::printf("Rendered %d labelled synthetic type/state/direction/time cases\n", cases);
}
// A provider supplies presentation inputs independently of simulation flags.
class RuleProvider : public SpriteProvider {
 public:
  bool flash = false;
  bool underlay = false;
  bool pick(const ObjectState&, SpriteDraw& out) const override { out.key.index = 1; return true; }
  std::vector<SpriteItem> items(const ObjectState&) const override {
    SpriteDraw body; body.key.index = 1; body.dy = -7;
    SpriteDraw shadow; shadow.key.index = 1; shadow.key.group = 0x454; shadow.key.shadow = true;
    SpriteDraw ground; ground.key.index = 2; ground.underlay = true;
    SpriteItem item; item.height = 7; item.display_size = 24; item.flash = flash; item.marker = 1;
    item.parts = underlay ? std::vector<SpriteDraw>{ground, shadow, body} : std::vector<SpriteDraw>{shadow, body};
    return {item};
  }
};
static void rule_gallery(const std::filesystem::path& path) {
  label = "WORLD RULES";
  Window win; WindowOptions options; options.width = options.height = 192;
  options.software = true; options.vsync = false; CHECK(win.create(options));
  if (!win.renderer()) return;
  GameData assets; std::string error; CHECK(assets.open(path.string(), error));
  World world; world.terrain = {3, 3, std::vector<uint32_t>(9, 1)};
  world.visibility_brightness = true;
  world.visibility.sides = {{4, 5, std::vector<uint8_t>(20, 76)}};
  Object unit; unit.id = 1; unit.type = 0; unit.hp = 50; unit.hp_max = 100;
  unit.teleport(to_fixed(60), to_fixed(80)); unit.prev_x = to_fixed(40);
  world.objects.push_back(unit); world.selection.ids = {1};
  SceneRenderer scene(win, assets, world);
  auto provider = std::make_shared<RuleProvider>(); scene.set_sprite_provider(provider);
  scene.opts.original_look = true; scene.opts.markers = scene.opts.labels = false;
  Camera cam; cam.view_w = cam.view_h = 192;
  std::vector<Placement> draws;
  scene.observe_sprite = [&](const SpriteKey& key, int frame, const SDL_FRect& rect) { draws.push_back({key, frame, rect}); };
  auto draw = [&] { draws.clear(); scene.draw(cam, 0); };
  draw();
  CHECK(draws.size() == 14); // nine tiles, selection, shadow/body, health, marker
  CHECK(draws[0].key.shade == 10); // direct uniform terrain rounds
  CHECK(draws[9].key.index == 309 && draws[9].key.palette == 3);
  CHECK(draws[9].rect.y == 70.5f || draws[9].rect.y == 71); // ellipse follows height
  CHECK(draws[10].key.shadow && draws[10].key.shade == 12);
  CHECK(draws[11].key.shade == 9 && draws[11].rect.x == 59); // surface truncates; no interpolation
  CHECK(draws[12].key.index == 263 && draws[12].frame == 1 && draws[12].rect.w == 8);
  CHECK(draws[13].key.index == 5);
  world.objects[0].hp = 1; world.objects[0].hp_max = 1000; draw();
  CHECK(std::none_of(draws.begin(), draws.end(), [](const auto& part) {
    return part.key.archive == ArchiveId::Common && part.key.index == 263;
  })); // a zero 8.8 ratio suppresses the health entry
  world.objects[0].hp = 50; world.objects[0].hp_max = 100;
  Object next = unit; next.id = 2; next.teleport(to_fixed(60), to_fixed(81));
  world.objects.push_back(next); draw();
  CHECK(draws.size() == 17 && draws[12].key.index == 263 && draws[14].key.shadow &&
      draws[15].key.archive == ArchiveId::Units && draws[16].key.index == 5);
  world.objects.pop_back();
  provider->flash = true; scene.opts.view_counter = 1; draw();
  CHECK(draws[11].key.shade == 19);
  scene.opts.view_counter = 2; draw(); CHECK(draws[11].key.shade == 9);
  provider->flash = false; provider->underlay = true; draw();
  CHECK(draws[9].key.index == 2 && draws[10].key.index == 2); // one full-2D row per call
  CHECK(draws[14].key.index == 309); // collected underlay precedes sorted selection
  // A vertical gradient changes terrain and underlay rows; bodies use one ground row.
  for (int y = 0; y < 5; ++y) for (int x = 0; x < 4; ++x)
    world.visibility.sides[0].v[size_t(y) * 4 + x] = uint8_t(48 + y * 24);
  ++world.fog_version; draw();
  std::set<int> terrain_shades, underlay_shades;
  for (const auto& part : draws) {
    if (part.key.archive == ArchiveId::Tiles0) terrain_shades.insert(part.key.shade);
    if (part.key.archive == ArchiveId::Units && part.key.index == 2) underlay_shades.insert(part.key.shade);
    if (part.key.archive == ArchiveId::Units && part.key.index == 1 && !part.key.shadow)
      CHECK(part.rect.h == 5); // never a 2D body slice
  }
  CHECK(terrain_shades.size() > 2 && !underlay_shades.empty());
  // The post-world overlay must be above bodies, status art and markers.
  bool after = false;
  scene.draw(cam, 1, [&] { after = !draws.empty() && draws.back().key.index == 5; });
  CHECK(after);
}
int main() {
  TempDir dir; const auto root = dir.path(); std::filesystem::create_directory(root / "Res");
  Group body{0x44c, 1, 4097, {}}, shadow{0x454, 1, 4097, {}}, anim{0x515, 0, 417, {}};
  for (int i = 1; i <= 4096; ++i) body.items[i] = shadow.items[i] = sprite(3, 5, 1, 1, 4, 0xf800);
  std::set<int> ids{23, 29, 113, 114, 115, 116, 152, 154, 155, 156, 157};
  for (const auto& t : data::kTypeAnims) { ids.insert(t.anim); if (t.anim2) ids.insert(t.anim2); }
  for (int id : ids) anim.items[id] = animation();
  archive(root / "Res/Units.xbe", {body, shadow, anim});
  Group common{0x44c, 0, 315, {}};
  for (int i = 0; i < 22; ++i) common.items[i] = sprite(3, 5, 1, 1, 4, uint16_t((i + 1) << 11));
  common.items[263] = sprite(16, 4, 3, 0, 0, 0x07e0);
  for (int id : {309, 314}) common.items[id] = sprite(3, 5, 4, 1, 4, 0x001f);
  archive(root / "Res/common.xbe", {common});
  archive(root / "Res/Tiles0.xbe", {{0x44c, 1, 3, {{1, sprite(48, 48, 8, 0, 0, 0x07e0)}, {2, sprite(3, 5, 4, 1, 4, 0x001f)}}},
                                      {0x454, 2, 3, {{2, sprite(3, 5, 4, 1, 4, 0x001f)}}}});
  for (const auto* name : {"Pal", "Tiles1", "fnts", "light"}) archive(root / "Res" / (std::string(name) + ".xbe"), {});
  label = "SDL INITIALIZATION"; const int initialized = SDL_Init(SDL_INIT_VIDEO); CHECK(initialized == 0);
  if (initialized == 0) { gallery(root); rule_gallery(root); }
  SDL_Quit(); return failures ? 1 : 0;
}
