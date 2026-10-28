// Engine tests: test_engine --pure, or test_engine [GAME_DIR] for game-data checks.
#include "game_dir.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include "core/mapfiles.hpp"
#include "core/strbase.hpp"
#include "engine/ui/names.hpp"
#include "engine/ui/commands_ui.hpp"
#include "engine/ui/selection_ui.hpp"
#include "core/text.hpp"
#include "engine/assets/anim_provider.hpp"
#include "engine/audio/audio_system.hpp"
#include "engine/assets/game_data.hpp"
#include "engine/assets/mission.hpp"
#include "engine/data/effects.hpp"
#include "engine/data/light_masks.hpp"
#include "engine/data/terrain_sprites.hpp"
#include "engine/assets/sprite_provider.hpp"
#include "engine/platform/clock.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/sim/sight.hpp"
#include "engine/sim/savegame.hpp"
#include "core/endian.hpp"
#include "engine/sim/simulation.hpp"
#include "engine/world/camera.hpp"
#include "engine/world/type_info.hpp"
#include "engine/world/world.hpp"

using namespace aztec;
using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_sight() {
  auto rules = std::make_shared<RulesData>();
  CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  CHECK(rules->type(0).vision_mask >= 64 && rules->type(0).vision_mask <= 71);
  CHECK(rules->type(0).sight_src == Src::Verified);
  for (auto [type, mask] : std::vector<std::pair<int, int>>{{0,64}, {1,68}, {5,65}, {54,69},
       {30,70}, {10,70}, {11,71}, {14,69}, {7,71}, {83,66}, {33,71}, {34,69},
       {3,67}, {4,70}, {17,68}, {23,70}, {26,71}, {87,70}})
    CHECK(rules->type(type).vision_mask == mask);
  auto inputs = std::make_shared<SightData>();
  for (auto& day : inputs->daylight) day.fill(80);
  auto& mask = inputs->masks[rules->type(0).vision_mask];
  mask.assign(404, 152); mask[0] = mask[2] = 20; mask[1] = mask[3] = 0;
  mask[4 + 10 * 20 + 10] = 0;
  rules->set_sight(inputs);
  std::vector<uint8_t> grid(40 * 40, 152);
  stamp_sight(grid, 40, 40, inputs->mask(rules->type(0).vision_mask), 20 * 48, 20 * 48, true);
  CHECK(grid[20 * 40 + 20] == 0 && grid[20 * 40 + 19] == 152);
  // Translation blends four samples, truncating each term on each axis.
  std::fill(grid.begin(), grid.end(), 152);
  stamp_sight(grid, 40, 40, inputs->mask(rules->type(0).vision_mask), 20 * 48 + 24, 20 * 48 + 24, true);
  CHECK(grid[20 * 40 + 20] == 114);
  std::vector<uint8_t> glow(40 * 40, 145);
  stamp_sight(glow, 40, 40, inputs->mask(rules->type(0).vision_mask), -24, -24, false);
  CHECK(*std::max_element(glow.begin(), glow.end()) == 152);
  stamp_sight(glow, 40, 40, data::LightMask{}, 0, 0, false);

  VisFile::Layer boundary{2, 2, {32, 33, 32, 33}};
  CHECK(sight_brightness(boundary, 0, 0) == 32 * 256);
  CHECK(sight_brightness(boundary, 24, 24) > 32 * 256);
  CHECK(sight_brightness(boundary, -1, 0) == 0);
  CHECK(sight_brightness(boundary, 96, 0) == 0);

  World initial;
  initial.terrain.w = initial.terrain.h = 40; initial.terrain.cells.resize(1600);
  Object scout; scout.id = 1; scout.type = 0; scout.hp = 6000;
  scout.teleport(to_fixed(20 * 48), to_fixed(20 * 48)); initial.objects.push_back(scout);
  World world = initial;
  GameSim sim(rules); sim.init(world);
  world.begin_tick(); sim.tick(world);
  CHECK(world.visibility.sides.size() == 8);
  const auto index = size_t(20 * 41 + 20);
  CHECK(world.visibility.sides[0].w == 41 && world.visibility.sides[0].h == 42);
  CHECK(world.visibility.sides[0].v[index] == 80);
  CHECK(world.visibility.sides[1].v[index] == 0); // hostile and allied vision is not shared
  world.objects[0].alive = false;
  world.begin_tick(); sim.tick(world);
  CHECK(world.visibility.sides[0].v[index] == 32);
  world.objects[0].alive = true;
  inputs->daylight[0][3] = 24;
  world.begin_tick(); sim.tick(world);
  CHECK(world.visibility.sides[0].v[index] == 32); // explored floor survives darkness
  world.local_side = 7;
  world.begin_tick(); sim.tick(world);
  CHECK(world.visibility.sides[0].v[index] == 80);
  auto hash = sim.state_hash(world);
  world.visibility.sides[0].v[index]++;
  CHECK(sim.state_hash(world) != hash);

  World a = initial, b = initial;
  GameSim first(rules), second(rules); first.init(a); second.init(b);
  b.local_side = 7;
  for (int t = 0; t < 8; ++t) {
    a.begin_tick(); first.tick(a); b.begin_tick(); second.tick(b);
    CHECK(first.state_hash(a) == second.state_hash(b));
    World replayed = initial;
    auto replay = GameSim::replay(replayed, rules, first.log(), first.tick_count());
    CHECK(first.state_hash(a) == replay->state_hash(replayed));
  }
  // A lit target beyond the old six-tile radius is eligible; exactly 32 is not.
  std::fill(mask.begin() + 4, mask.end(), 0);
  Object enemy; enemy.id = 2; enemy.type = 10; enemy.side = 1; enemy.hp = 30000;
  enemy.teleport(to_fixed(20 * 48 + 400), to_fixed(20 * 48));
  initial.objects.push_back(enemy);
  initial.relation[0][1] = kEnemy;
  for (int ambient : {32, 33}) {
    for (auto& day : inputs->daylight) day.fill(uint8_t(ambient));
    World duel = initial;
    GameSim combat(rules); combat.init(duel);
    for (int t = 0; t < 3; ++t) { duel.begin_tick(); combat.tick(duel); }
    CHECK(duel.objects[0].target == (ambient == 33 ? 2u : 0u));
  }
  // Clean checkouts have no private masks. Their combat remains functional,
  // using the bounded placeholder instead of silently disabling acquisition.
  rules->set_sight(nullptr);
  CHECK(initial.visibility.sides.empty());
  for (int distance : {200, 400}) {
    World duel = initial;
    duel.objects[1].teleport(to_fixed(20 * 48 + distance), to_fixed(20 * 48));
    GameSim combat(rules); combat.init(duel);
    for (int t = 0; t < 3; ++t) { duel.begin_tick(); combat.tick(duel); }
    CHECK(duel.objects[0].target == (distance == 200 ? 2u : 0u));
    CHECK(duel.visibility.sides.empty());
  }
}

static void test_sprite_metadata() {
  using namespace aztec::data;
  CHECK(body_sequence(6, AnimState::Idle, 248).slot == 32);
  CHECK(body_sequence(6, AnimState::Move, 248).loop);
  CHECK(body_sequence(1, AnimState::Attack, 128).slot == 0);
  CHECK(!body_sequence(4, AnimState::Die, 0).valid());
  CHECK(!body_sequence(10, AnimState::Attack, 0).valid());
  CHECK(body_sequence(7, AnimState::Attack, 0, 0, true, true).slot == 1);
  CHECK(body_sequence(7, AnimState::Attack, 0).slot == 8);
  CHECK(turret_sequence(AnimState::Attack, 248).slot == 33);
  CHECK(body_sequence(5, AnimState::Move, 8, 0, false).frame == 1);
  CHECK(boat_sequence(BoatPhase::Turning, 1).slot == 9);
  CHECK(johnka_sequence(1).loop);
  CHECK(construction_frame(5, 1, 6000) == -1);
  CHECK(construction_frame(5, 1000, 6000) == 0);
  CHECK(construction_frame(5, 5999, 6000) == 4);
  CHECK(construction_frame(5, 6000, 6000) == -1);
  CHECK(destruction_anim(96) == 116 && destruction_anim(97) == 115 && destruction_anim(145) == 114);
  CHECK(damage_fire_anim(100, 100) == 0 && damage_fire_anim(75, 100) == 154);
  CHECK(damage_fire_anim(50, 100) == 155 && damage_fire_anim(25, 100) == 156);
  CHECK(palette_shade(0) == 0 && palette_shade(4) == 1 && palette_shade(152) == 19);
  CHECK(overlay_frame(WindStrength::Still, 7) == 0);
  CHECK(overlay_frame(WindStrength::Slow, 7) == 3);

  // A timed prefix must play before a later hold frame, also when looping.
  std::vector<uint8_t> bytes(0x208 + 4 * 32, 0);
  auto put = [&](size_t offset, uint32_t value) { aztec::write_le(bytes.data() + offset, value); };
  std::memcpy(bytes.data(), "Anim", 4); put(4, 3); put(8, 1);
  put(0x208 + 32, 123); put(0x208 + 32 + 28, 2);
  put(0x208 + 64, 456);
  AnimSet anim({bytes.data(), bytes.size()});
  CHECK(anim.at_tick(0, 0, true).sprite == 123 && anim.at_tick(0, 1, false).sprite == 123);
  CHECK(anim.at_tick(0, 2, true).sprite == 456 && anim.at_tick(0, 100, false).sprite == 456);
  CHECK(!anim.at_tick(0, -1, false).valid());
  put(0x208 + 64 + 28, 3);
  CHECK(anim.at_tick(0, 5, true).sprite == 123 && !anim.at_tick(0, 5, false).valid());
  put(8, 0xffffffff);
  CHECK(!anim.frame(0, 0).valid() && anim.loop_sound(0) == 0);
  CHECK(!AnimSet().ok() && !AnimSet({bytes.data(), 7}).frame(0, 0).valid());
  CHECK(!LightMask().ok() && LightMask().sample(0, 0) == -1);
  std::vector<uint8_t> mask_bytes(404, 0);
  mask_bytes[0] = mask_bytes[2] = 20;
  mask_bytes[4] = 8; mask_bytes[5] = 24;
  mask_bytes[24] = 40; mask_bytes[25] = 56;
  LightMask mask({mask_bytes.data(), mask_bytes.size()});
  CHECK(mask.sample_pixels(0, 0) == 8 && mask.sample_pixels(48, 48) == 56);
  CHECK(mask.sample_pixels(24, 24) == 32);  // bilinear translation, not nearest mask texel
  CHECK(mask.sample_pixels(-1, 0) == 0 && mask.sample_pixels(20 * 48, 0) == 0);
  CHECK(LightMask().sample_pixels(24, 24) == 0);
  CHECK(effect_lifetime(150, 0) == 11 && effect_lifetime(150, 2) == 10);
  CHECK(effect_lifetime(113, 19) == 250 && effect_lifetime(105, 0) == 30);
}

static void test_sprite_assets(const std::string& dir) {
  using namespace aztec::data;
  RulesData sight_rules;
  CHECK(sight_rules.load_dir(AZTEC_TEST_RULES_DIR));
  CHECK(sight_rules.load_sight(dir));
  if (sight_rules.sight()) {
    CHECK(sight_rules.sight()->masks.size() == 24);
    for (const auto& day : sight_rules.sight()->daylight)
      for (uint8_t value : day) CHECK(value <= 152 && value % 8 == 0);
  }
  SpriteDb db;
  CHECK(db.open(find_file_ci(dir + "/Res", "Units.xbe")));
  int inventory_count = 0;
  for (int id = 31; id < 200; id++) if (db.anim(id).ok()) inventory_count++;
  CHECK(inventory_count == int(std::size(kEffectAnims)));
  for (const auto& info : kEffectAnims) {
    AnimSet anim = db.anim(info.id);
    CHECK(anim.ok());
    int slots = 0, min_frames = 10000, max_frames = 0, min_ticks = 10000, max_ticks = 0;
    for (int slot = 0; slot < 128; slot++) {
      if (!anim.slot_start(slot)) continue;
      slots++;
      int frames = anim.frame_count(slot), ticks = 0;
      min_frames = std::min(min_frames, frames); max_frames = std::max(max_frames, frames);
      for (int i = 0; i < frames; i++) {
        SpriteRef frame = anim.frame(slot, i);
        CHECK(frame.duration > 0);
        CHECK(anim.at_tick(slot, ticks, false).sprite == frame.sprite);
        ticks += frame.duration;
      }
      CHECK(!anim.at_tick(slot, ticks, false).valid());
      min_ticks = std::min(min_ticks, ticks); max_ticks = std::max(max_ticks, ticks);
    }
    CHECK(slots == info.slots && min_frames == info.min_frames && max_frames == info.max_frames);
    CHECK(min_ticks == info.min_ticks && max_ticks == info.max_ticks);
  }
  CHECK(db.sprite_for(3, AnimState::Idle, 7, 0).sprite == db.anim(14).frame(29, 0).sprite);
  CHECK(db.sprite_for(30, AnimState::Idle, 7, 0).sprite == db.anim(22).frame(0, 0).sprite);
  CHECK(!db.sprite_for(90, AnimState::Idle, 0, 0).valid());
  Archive lights;
  CHECK(lights.open(find_file_ci(dir + "/Res", "light.xbe")));
  int count = 0;
  for (int id = 1; id < 128; id++) {
    Item item = lights.get(kLightGroup, id);
    if (!item) continue;
    LightMask mask(item);
    CHECK(mask.ok()); count++;
    for (int y = 0; y < 20; y++) for (int x = 0; x < 20; x++) CHECK(mask.sample(x, y) >= 0);
    CHECK(mask.sample(-1, 0) == -1 && mask.sample(20, 0) == -1);
    if (id <= 16) CHECK(mask.sample(10, 10) > mask.sample(0, 0));
    if (id >= 64) CHECK(mask.sample(10, 10) < mask.sample(0, 0));
  }
  CHECK(count == 24);
}

static void test_pure() {
  CHECK(cp1250_to_utf8("abc") == "abc");
  CHECK(cp1250_to_utf8("\xb9\xe6") == "\xc4\x85\xc4\x87");  // a-ogonek, c-acute


  Ini ini;
  ini.parse("[Globals]\r\nName=Test\r\nMapSize=10,8\r\nTileSet=1\r\nSides=1\r\nTowns=1\r\nTeams=1\r\n"
            "[Side0]\r\nColor=3\r\nStart_X=2\r\nStart_Y=3\r\n"
            "[Town0]\r\nSide=0\r\nX=2\r\nY=3\r\nUnits=1\r\nUnit0= 4, 5, 19, 100\r\n"
            "[Team0]\r\nSide=0\r\nUnits=2\r\nUnit0=1,1,0,6000\r\nUnit1=2,2,27,3500\r\n");
  MissionDef m;
  CHECK(parse_mission(ini, "test", m));
  CHECK(m.w == 10 && m.h == 8 && m.tileset == 1 && m.sides.size() == 1 && m.sides[0].color == 3);
  CHECK(m.objects.size() == 3 && m.objects[0].type == 19 && m.objects[0].source == ObjectSource::Town);
  CHECK(m.objects[2].type == 27 && m.objects[2].source == ObjectSource::Team);

  Grid<uint16_t> g;
  std::vector<uint8_t> rm{2, 0, 1, 0, 7, 0, 8, 0};
  StrBase strb;
  CHECK(strb.parse("*STRING { *ID 0x1 *ORDINAL 217 *LANGUAGE { *ENGLISH \"%s\\nPrice: %i Time: %i\" *RUSSIAN \"Cena \xb9\" } }"));
  CHECK(format_game_string(strb.get(217), "Bow", "5", "9") == "Bow\nPrice: 5 Time: 9");
  CHECK(strb.get(217, true).find("\xc4\x85") != std::string::npos);
  CHECK(type_name_ordinal(0) == 198 && type_name_ordinal(19) == 170 && type_name_ordinal(47) == 170);
  CHECK(parse_resource_map(rm, g) && g.w == 2 && g.h == 1 && g.at(1, 0) == 8);
  CHECK(!parse_resource_map({2, 0, 2, 0, 1, 0}, g));

  CHECK(parse_fixed256("9.26") == 2371 && parse_fixed256("10") == 2560 && parse_fixed256("0.5") == 128 && parse_fixed256("-1.5") == -384);
  CHECK(parse_fixed256("") == 0 && parse_fixed256("abc") == 0 && parse_fixed256("99999999999") > 0);

  // 12 Hz stepper
  FixedStepper st;
  CHECK(st.advance(0.05) == 0);
  CHECK(st.advance(0.05) == 1);
  CHECK(st.alpha() > 0 && st.alpha() < 1);
  CHECK(st.advance(10.0) <= 5);  // spiral protection

  // interpolation
  Object o;
  o.teleport(to_fixed(100), to_fixed(100));
  o.cur_x = to_fixed(148);
  Vec2f p = o.interpolated(0.5f);
  CHECK(std::fabs(p.x - 124.f) < 0.01f && std::fabs(p.y - 100.f) < 0.01f);
  World w;
  w.objects.push_back(o);
  w.begin_tick();
  CHECK(w.objects[0].prev_x == to_fixed(148));

  // camera: clamps, centres small worlds, zoom keeps pivot
  Camera c;
  c.set_view(800, 600);
  c.set_world(4800, 3200);
  c.x = -50; c.y = 99999;
  CameraInput none;
  c.update(none, 0.016f);
  CHECK(c.x == 0 && std::fabs(c.y - (3200 - 600)) < 0.01f);
  c.x = 1000; c.y = 1000;
  float wx = c.to_world_x(300), wy = c.to_world_y(200);
  c.set_zoom(2.f, 300, 200);
  CHECK(std::fabs(c.to_world_x(300) - wx) < 0.01f && std::fabs(c.to_world_y(200) - wy) < 0.01f);
  Camera small;
  small.set_view(1920, 1080);
  small.set_world(480, 480);
  CHECK(small.x < 0 && small.y < 0);  // centred

  // sprite providers
  TableSpriteProvider tp;
  SpriteDraw d;
  d.key.index = 77;
  tp.set(5, d);
  ObjectState s;
  s.type_id = 5;
  SpriteDraw out;
  CHECK(tp.pick(s, out) && out.key.index == 77);
  s.type_id = 6;
  CHECK(!tp.pick(s, out));
  CHECK(aztec::engine::type_info(0) && std::string(aztec::engine::type_info(0)->name) == "FOOTMAN" && aztec::engine::type_info(10)->is_structure() && !aztec::engine::type_info(90));

  // deterministic wander
  World a, b;
  for (World* ww : {&a, &b}) {
    ww->terrain.w = ww->terrain.h = 10;
    Object u; u.id = 1; u.type = 0; u.teleport(to_fixed(200), to_fixed(200));
    ww->objects.push_back(u);
  }
  WanderSimulation sa(7), sb(7);
  for (int i = 0; i < 50; i++) { a.begin_tick(); sa.tick(a); b.begin_tick(); sb.tick(b); }
  CHECK(a.objects[0].cur_x == b.objects[0].cur_x && a.objects[0].cur_y == b.objects[0].cur_y);
}

static void test_commands_ui() {
  // Hit regions stay within the panel and do not overlap across supported UI scales.
  for (int scale : {1, 2}) for (int height : {493, 600, 700}) {
    SDL_Rect panel{796, 11, 204 * scale, height * scale};
    auto l = CommandsUi::layout(panel, scale);
    std::vector<SDL_Rect> regions(l.commands.begin(), l.commands.end());
    regions.insert(regions.end(), l.production.begin(), l.production.end());
    regions.insert(regions.end(), l.queue.begin(), l.queue.end());
    regions.push_back(l.pause); regions.push_back(l.previous); regions.push_back(l.next);
    for (size_t i = 0; i < regions.size(); i++) {
      CHECK(regions[i].x >= panel.x && regions[i].y >= panel.y);
      CHECK(regions[i].x + regions[i].w <= panel.x + panel.w);
      CHECK(regions[i].y + regions[i].h <= panel.y + panel.h);
      for (size_t j = i + 1; j < regions.size(); j++) CHECK(!SDL_HasIntersection(&regions[i], &regions[j]));
    }
  }
  CommandsUi keys;
  std::vector<KeyBinding> bindings;
  CHECK(parse_keys_ini("Repair r\nDestroy t\nScrollLeft pad4\nScrollLeft left\nAdd rshift\n", bindings));
  keys.set_bindings(bindings);
  Input key;
  key.key_hit[SDL_SCANCODE_R] = true;
  CHECK(keys.action_hit(key, "Repair") && !keys.action_hit(key, "Destroy"));
  CHECK(keys.key_bound(SDL_SCANCODE_R) && !keys.key_bound(SDL_SCANCODE_W));
  key.key_down[SDL_SCANCODE_KP_4] = true;
  CHECK(keys.action_down(key, "ScrollLeft"));
  key.key_down[SDL_SCANCODE_KP_4] = false; key.key_down[SDL_SCANCODE_LEFT] = true;
  CHECK(keys.action_down(key, "ScrollLeft"));
  CHECK(CommandsUi::sdl_scancode(999) == SDL_SCANCODE_UNKNOWN);
  CHECK(CommandsUi::icon_slot(0) == 23 && CommandsUi::icon_slot(2) == 7 && CommandsUi::icon_slot(6) == 39);

  auto rules = std::make_shared<RulesData>();
  CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  World w;
  w.terrain.w = w.terrain.h = 40;
  w.terrain.cells.resize(1600);
  w.mission.sides.push_back(SideDef{});
  w.mission.towns.push_back(TownDef{});
  w.mission.towns.back().type = 1;
  Object unit; unit.id = 1; unit.type = 0; unit.teleport(to_fixed(200), to_fixed(200));
  Object town; town.id = 2; town.type = 10; town.teleport(to_fixed(600), to_fixed(600));
  Object enemy; enemy.id = 3; enemy.type = 0; enemy.side = 1; enemy.teleport(to_fixed(700), to_fixed(600));
  w.objects = {unit, town, enemy};
  GameSim sim(rules); sim.init(w);
  w.side_state[0].money = 2000;
  w.selection.ids = {1};
  World fresh = w;
  CommandsUi ui(&sim);
  Camera cam; cam.set_world(1920, 1920); cam.set_view(796, 700);
  Minimap mm;
  SDL_Rect panel{796, 0, 204, 700};
  auto l = CommandsUi::layout(panel, 1);
  auto tick = [&] { w.begin_tick(); sim.tick(w); };
  auto click = [&](SDL_Rect rect) {
    Input in; in.mx = rect.x + rect.w / 2; in.my = rect.y + rect.h / 2;
    in.pressed[SDL_BUTTON_LEFT] = true; in.buttons[SDL_BUTTON_LEFT] = true;
    return ui.update(in, w, cam, 1, mm, false, panel);
  };
  CHECK(CommandsUi::action_enabled(PanelAction::Move, w));
  CHECK(!CommandsUi::action_enabled(PanelAction::Repair, w));
  CHECK(!CommandsUi::action_enabled(PanelAction::Garrison, w));
  auto before = sim.state_hash(w);
  click(l.commands[0]);
  CHECK(ui.target_mode() == TargetMode::Move);
  CHECK(sim.state_hash(w) == before && sim.log().empty());
  Input target; target.mx = 400; target.my = 300;
  target.pressed[SDL_BUTTON_LEFT] = true; target.buttons[SDL_BUTTON_LEFT] = true;
  CHECK(ui.update(target, w, cam, 1, mm, true, panel));
  CHECK(!ui.wants_left_click() && sim.state_hash(w) == before);
  tick();
  CHECK(sim.log().back().cmd.type == Pct::Move && sim.log().back().cmd.a == 400);
  // A consumed targeting press must not start a new selection when the button is released.
  SelectionUi selection;
  CHECK(!selection.update(target, w, cam, 1, false));
  target.pressed[SDL_BUTTON_LEFT] = false; target.buttons[SDL_BUTTON_LEFT] = false; target.released[SDL_BUTTON_LEFT] = true;
  selection.update(target, w, cam, 1, true);
  CHECK(w.selection.ids == std::vector<uint32_t>{1});
  click(l.commands[1]);
  target.pressed[SDL_BUTTON_LEFT] = true; target.released[SDL_BUTTON_LEFT] = false;
  ui.update(target, w, cam, 1, mm, true, panel); tick();
  CHECK(sim.log().back().cmd.type == Pct::AttackMove);
  click(l.commands[1]);
  target.mx = 700; target.my = 600;
  ui.update(target, w, cam, 1, mm, true, panel); tick();
  CHECK(sim.log().back().cmd.type == Pct::Attack && sim.log().back().cmd.a == 3);
  click(l.commands[2]); tick();
  CHECK(sim.log().back().cmd.type == Pct::Stop);
  size_t logged = sim.log().size();
  click(l.commands[3]); click(l.commands[4]); tick();
  CHECK(!ui.wants_left_click() && sim.log().size() == logged);
  click(l.commands[0]);
  Input cancel; cancel.key_hit[SDL_SCANCODE_ESCAPE] = true;
  ui.update(cancel, w, cam, 1, mm, true, panel);
  CHECK(!ui.wants_left_click());
  w.selection.ids = {2};
  CHECK(!CommandsUi::action_enabled(PanelAction::Move, w));
  CHECK(!CommandsUi::action_enabled(PanelAction::Destroy, w));
  w.selection.ids = {3};
  CHECK(!CommandsUi::action_enabled(PanelAction::Destroy, w));
  w.selection.ids = {1};
  click(l.production[0]);
  CHECK(w.side_state[0].money == 2000 && w.side_state[0].queue.empty());
  tick();
  CHECK(w.side_state[0].money == 1900 && w.side_state[0].queue.size() == 1);
  click(l.pause); tick();
  CHECK(w.side_state[0].build_paused);
  int remaining = w.side_state[0].queue.front().remaining;
  tick(); CHECK(w.side_state[0].queue.front().remaining == remaining);
  click(l.queue[0]); tick();
  CHECK(w.side_state[0].queue.empty() && w.side_state[0].money == 2000);
  // Construction is a targeting mode and submits Build only when a valid tile is clicked.
  auto menu = sim.menu(w, 0);
  panel.h = 493;
  l = CommandsUi::layout(panel, 1);
  int build_index = -1;
  for (size_t i = 0; i < menu.size(); i++) if (menu[i].construct && menu[i].enabled) { build_index = int(i); break; }
  CHECK(build_index >= int(l.production.size()));
  if (build_index >= 0) {
    for (int page = 0; page < build_index / int(l.production.size()); page++) click(l.next);
    click(l.production[size_t(build_index) % l.production.size()]);
    CHECK(ui.placing() == menu[size_t(build_index)].type);
    int money = w.side_state[0].money;
    target.mx = 400; target.my = 500;
    CHECK(ui.update(target, w, cam, 1, mm, true, panel));
    CHECK(!ui.wants_left_click() && w.side_state[0].money == money);
    tick();
    CHECK(sim.log().back().cmd.type == Pct::Build);
    CHECK(w.side_state[0].money == money - menu[size_t(build_index)].price);
    CHECK(w.objects.back().progress < 1000);
  }
  // Original unsupported mode keys never submit gameplay packets or activate engine targeting.
  size_t mode_log_size = sim.log().size();
  for (SDL_Scancode code : {SDL_SCANCODE_W, SDL_SCANCODE_Q, SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_GRAVE}) {
    Input mode; mode.key_hit[code] = true;
    ui.update(mode, w, cam, 1, mm, true, panel);
    CHECK(!ui.wants_left_click());
  }
  tick(); CHECK(sim.log().size() == mode_log_size);
  // Every UI action above replays with the same simulation hash.
  auto replay = GameSim::replay(fresh, rules, sim.log(), sim.tick_count());
  CHECK(replay->state_hash(fresh) == sim.state_hash(w));
}

static void test_data(const std::string& dir) {
  GameData data;
  std::string err;
  if (!data.open(dir, err)) { CHECK(false); std::printf("%s\n", err.c_str()); return; }
  CommandsUi sprites;
  CHECK(sprites.open_sprites(find_file_ci(dir + "/Res", "Units.xbe")));
  CHECK(sprites.icon_sprite(0, false) == 1385 && sprites.icon_sprite(0, true) == 1386);
  CHECK(sprites.icon_sprite(19, false) == 4119 && sprites.icon_sprite(19, true) == 4120);
  CHECK(sprites.icon_sprite(4, true) == 3988);   // cannon, second weapon variant
  CHECK(sprites.icon_sprite(35, true) == 86);    // spider flamethrower, fifth weapon variant
  CHECK(sprites.icon_sprite(59, true) == 6317); // wheeled flamethrower
  for (int type = 0; type < kNumTypes; type++) {
    if (type == 89) continue;  // camera marker has no sprite
    Sprite icon;
    CHECK(decode_sprite(data.archive(ArchiveId::Units).get(0x44c, sprites.icon_sprite(type, true)), icon));
  }
  World w;
  if (!load_world(data, "1", w, err)) { CHECK(false); std::printf("%s\n", err.c_str()); return; }
  CHECK(w.terrain.w == 90 && w.terrain.h == 60);
  CHECK(!w.objects.empty());
  CHECK(w.preview.w == 180 && w.preview.h == 120);
  CHECK(w.passability.w == 90);
  CHECK(w.mission.sides.size() == 2);
  World w2;
  CHECK(load_world(data, "Mission_4", w2, err) && w2.terrain.w == 120);  // mixed-case file names
  CHECK(!load_world(data, "no_such_map", w2, err));
  CHECK(w.mission.towns.size() == 1);
  for (size_t i = 0; i < w.mission.objects.size(); i++) {
    const PlacedObject& p = w.mission.objects[i];
    CHECK(w.objects[i].town == (p.source == ObjectSource::Town && !p.defence ? p.group : -1));
    CHECK(!w.objects[i].town_hall);
  }
  CHECK(w.objects.back().town_hall && w.objects.back().town == w.mission.towns[0].index);
  StrBase real;
  std::string sbp = find_file_ci(dir + "/Res", "strbase.bds");
  CHECK(!sbp.empty() && real.load(sbp) && real.size() > 200 && real.get(198) == "Swordsman");

  // animated sprite provider: footman idle/move, shadow in group 0x454, finished town building
  AnimSpriteProvider ap;
  std::string up = find_file_ci(dir + "/Res", "Units.xbe");
  CHECK(!up.empty() && ap.open(up));
  ObjectState st;
  st.type_id = 0;
  SpriteDraw d, sh;
  CHECK(ap.pick(st, d) && d.key.group == 0x44c && d.key.index > 0);
  CHECK(ap.pick_shadow(st, sh) && sh.key.group == 0x454 && sh.key.shadow && sh.key.index == d.key.index);
  st.anim_state = 1;
  SpriteDraw m0, m1;
  st.anim_tick = 0; CHECK(ap.pick(st, m0));
  st.anim_tick = 3; CHECK(ap.pick(st, m1));
  CHECK(m0.key.index != m1.key.index);  // walk cycle advances with the tick
  st.type_id = 19; st.anim_state = 0;
  CHECK(ap.pick(st, d));  // RUS_TOWN
  CHECK(!ap.pick_shadow(ObjectState{.type_id = 200}, sh));
}

static std::vector<uint32_t> side_units(const World& w, int side, bool mobile_only) {
  std::vector<uint32_t> v;
  for (const Object& o : w.objects) {
    const TypeInfo* ti = aztec::engine::type_info(o.type);
    if (o.side == side && o.type >= 0 && ti && (!mobile_only || !ti->is_structure())) v.push_back(o.id);
  }
  return v;
}

static void run_scenario(World& w, GameSim& sim, int ticks, bool commands) {
  std::vector<uint32_t> mine = side_units(w, 0, true);
  for (int t = 0; t < ticks; t++) {
    if (commands && t == 3) { Command c; c.type = Pct::Move; c.side = 0; c.a = 36 * 48; c.b = 41 * 48; c.ids = mine; sim.submit(c); }
    if (commands && t == 40) { Command c; c.type = Pct::AttackMove; c.side = 0; c.a = 70 * 48; c.b = 40 * 48; c.ids = mine; sim.submit(c); }
    w.begin_tick();
    sim.tick(w);
  }
}

// Synthetic combat checks run without installed assets, using the public rules tables.
static void test_projectiles() {
  auto rules = std::make_shared<RulesData>();
  CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  CHECK(rules->type(27).weapon.anim == 112);
  CHECK(rules->type(27).weapon.speed == 30 && rules->type(27).range_px == 160);
  CHECK(rules->type(3).weapon.anim == 101);
  CHECK(rules->type(4).weapon.anim == 100);
  CHECK(rules->type(2).weapon.anim == 157);
  for (int type : {27, 3, 4, 2}) {
    World initial;
    initial.terrain.w = initial.terrain.h = 40;
    initial.terrain.cells.resize(1600);
    for (int a = 0; a < 8; a++) for (int b = 0; b < 8; b++) initial.relation[a][b] = a == b ? kFriend : kEnemy;
    Object shooter; shooter.id = 1; shooter.type = type; shooter.hp = 30000;
    shooter.teleport(to_fixed(200), to_fixed(200));
    Object target; target.id = 2; target.type = 10; target.side = 1; target.hp = 30000;
    target.teleport(to_fixed(290), to_fixed(200));
    initial.objects = {shooter, target};
    World a = initial, b = initial;
    GameSim sa(rules), sb(rules); sa.init(a); sb.init(b);
    Command attack; attack.type = Pct::Attack; attack.ids = {1}; attack.a = 2;
    sa.submit(attack); sb.submit(attack);
    a.begin_tick(); sa.tick(a); b.begin_tick(); sb.tick(b);
    const auto shot = rules->attack(type, target.type);
    if (!shot.damage_known || !shot.getters_known) {
      // Visual metadata alone must not enable invented damage or scheduling.
      for (int tick = 0; tick < 8; tick++) {
        CHECK(a.effects.empty() && !a.objects[0].has_fired && a.objects[1].hp == 30000);
        CHECK(sa.state_hash(a) == sb.state_hash(b));
        a.begin_tick(); sa.tick(a); b.begin_tick(); sb.tick(b);
      }
      continue;
    }
    CHECK(!a.effects.empty() && a.effects.front().flight);
    CHECK(a.objects[0].cooldown == shot.reload + 1);

    CHECK(a.objects[1].hp == 30000);  // firing does not apply damage early
    CHECK(sa.state_hash(a) == sb.state_hash(b));
    if (!a.effects.empty()) {
      auto hash = sa.state_hash(a);
      a.effects[0].end_x++;
      CHECK(sa.state_hash(a) != hash);
      a.effects[0].end_x--;
      a.effects[0].age++;
      CHECK(sa.state_hash(a) != hash);
      a.effects[0].age--;
    }
    auto replay = GameSim::replay(initial, rules, sa.log(), 1);
    CHECK(replay->state_hash(initial) == sa.state_hash(a));  // save while in flight
    for (int tick = 0; tick < 8; tick++) {
      a.begin_tick(); sa.tick(a); b.begin_tick(); sb.tick(b);
      CHECK(sa.state_hash(a) == sb.state_hash(b));
    }
    CHECK(a.objects[1].hp == 30000 - shot.damage);
    Command pause; pause.type = Pct::PauseOn;
    sa.submit(pause); a.begin_tick(); sa.tick(a);
    auto effects = a.effects;
    a.begin_tick(); sa.tick(a);
    CHECK(a.effects.size() == effects.size());
    for (size_t i = 0; i < effects.size(); i++) CHECK(a.effects[i].x == effects[i].x && a.effects[i].age == effects[i].age);
  }
  // A splash hits hostile neighbours once, keeps allies safe, and retains its source id after death.
  World w;
  w.terrain.w = w.terrain.h = 20; w.terrain.cells.resize(400);
  Object target; target.id = 1; target.type = 10; target.side = 1; target.hp = 10000;
  target.teleport(to_fixed(200), to_fixed(200));
  Object neighbour = target; neighbour.id = 2; neighbour.teleport(to_fixed(210), to_fixed(200));
  Object ally = neighbour; ally.id = 3; ally.side = 0;
  w.objects = {target, neighbour, ally}; w.relation[0][1] = kEnemy;
  GameSim sim(rules); sim.init(w);
  for (auto& o : w.objects) o.hp_max = 10000;
  Effect e; e.flight = true; e.source = 99; e.target = 1; e.side = 0;
  e.x = e.end_x = to_fixed(200); e.y = e.end_y = to_fixed(200);
  e.speed = 256; e.damage = 1000; e.splash_px = 30; e.impact_anim = 150;
  w.effects.push_back(e);
  w.begin_tick(); sim.tick(w);
  CHECK(w.objects[0].hp == 9000 && w.objects[1].hp == 9000 && w.objects[2].hp == 10000);
  CHECK(w.objects[1].last_hit_by == 99);
  CHECK(!w.effects.empty() && !w.effects[0].flight && w.effects[0].anim == 150);
  for (int i = 0; i < 12; i++) { w.begin_tick(); sim.tick(w); }
  CHECK(w.objects[0].hp == 9000);  // effect lifetime never repeats damage
  CHECK(w.effects.empty());
  // Damage fires loop and disappear once a building is repaired.
  w.objects[0].hp = 4000;
  w.begin_tick(); sim.tick(w);
  CHECK(!w.effects.empty() && w.effects.back().loop && w.effects.back().anim == 155);
  w.objects[0].hp = w.objects[0].hp_max;
  w.begin_tick(); sim.tick(w);
  CHECK(w.effects.empty());
  // Known-radius rubble stays on the object; missing radii use one fallback effect.
  for (int radius : {-1, 97, 145}) {
    World rubble;
    rubble.terrain.w = rubble.terrain.h = 20; rubble.terrain.cells.resize(400);
    Object building; building.id = 1; building.type = 10; building.hp = 10000;
    building.destruction_radius = radius; building.teleport(to_fixed(200), to_fixed(200));
    rubble.objects = {building};
    GameSim death(rules); death.init(rubble); death.script_kill(rubble, 1);
    CHECK(!rubble.objects[0].alive && rubble.objects[0].removed == (radius < 0));
    CHECK(rubble.effects.size() == (radius < 0 ? 1u : 0u));
    for (int tick = 0; tick < 249; ++tick) { rubble.begin_tick(); death.tick(rubble); }
    if (radius >= 0) CHECK(!rubble.objects[0].removed);
    rubble.begin_tick(); death.tick(rubble);
    CHECK(rubble.objects[0].removed && rubble.effects.empty());
    rubble = World{};
    rubble.terrain.w = rubble.terrain.h = 20; rubble.terrain.cells.resize(400);
    building.progress = 100; rubble.objects = {building};
    death.init(rubble); rubble.objects[0].progress = 100; death.script_kill(rubble, 1);
    CHECK(rubble.objects[0].removed && rubble.effects.empty());
  }
}

static void test_sim(const std::string& dir) {
  GameData data;
  std::string err;
  if (!data.open(dir, err)) { CHECK(false); return; }
  auto rules = std::make_shared<RulesData>();
  if (!rules->load_dir(AZTEC_TEST_RULES_DIR)) { CHECK(false); return; }
  CHECK(rules->load_sight(dir));
  CHECK(rules->type(0).hp_max == 6000 && rules->type(0).hp_src == Src::Verified);
  CHECK(rules->type(0).price == 100);

  World a, b, c;
  CHECK(load_world(data, "1", a, err) && load_world(data, "1", b, err) && load_world(data, "1", c, err));
  GameSim sa(rules), sb(rules), sc(rules);
  sa.init(a); sb.init(b); sc.init(c);
  uint64_t h0 = sa.state_hash(a);
  run_scenario(a, sa, 200, true);
  run_scenario(b, sb, 200, true);
  CHECK(sa.state_hash(a) == sb.state_hash(b));      // same commands => same hash
  CHECK(sa.state_hash(a) != h0);
  run_scenario(c, sc, 200, false);
  CHECK(sc.state_hash(c) != sa.state_hash(a));      // different commands => different state
  // the move order really moved the units toward the target
  std::vector<uint32_t> mine = side_units(a, 0, true);
  const Object* u = a.find(mine[0]);
  CHECK(u && std::abs(u->cur_x / 256 - 36 * 48) + std::abs(u->cur_y / 256 - 41 * 48) > 0);

  // hostile coordinates are clamped into the map (no overflow), and the destination shows up in the state hash
  {
    World h1;
    CHECK(load_world(data, "1", h1, err));
    GameSim g1(rules);
    g1.init(h1);
    uint64_t before = g1.state_hash(h1);
    Command big; big.type = Pct::Move; big.side = 0; big.a = 2000000000; big.b = -2000000000; big.ids = side_units(h1, 0, true);
    g1.submit(big);
    h1.begin_tick(); g1.tick(h1);
    const Object* mo = h1.find(big.ids[0]);
    CHECK(mo && mo->dest_x >= 0 && mo->dest_x < h1.width_px() && mo->dest_y == 0);
    CHECK(g1.state_hash(h1) != before);
    Command badbuild; badbuild.type = Pct::Build; badbuild.side = 0; badbuild.a = 14; badbuild.b = 2000000000; badbuild.c = 5;
    g1.submit(badbuild);
    h1.begin_tick(); g1.tick(h1);
    // invalid object types are inert markers, not out-of-bounds reads
    World h2;
    CHECK(load_world(data, "1", h2, err));
    h2.objects[0].type = 5000;
    GameSim g2(rules);
    g2.init(h2);
    CHECK(h2.objects[0].type == -1);
  }

  // replay of the command log reproduces the exact state (snapshot restore)
  World r;
  CHECK(load_world(data, "1", r, err));
  auto rs = GameSim::replay(r, rules, sa.log(), 200);
  CHECK(rs->state_hash(r) == sa.state_hash(a));
  std::vector<LoggedCommand> back;
  CHECK(deserialize_log(serialize_log(sa.log()), back) && back.size() == sa.log().size());
  SaveData sv;
  sv.mission = "1"; sv.tick = 200; sv.hash = sa.state_hash(a); sv.cam_x = 12.5f; sv.log = sa.log();
  SaveData sv2;
  CHECK(deserialize_save(serialize_save(sv), sv2) && sv2.mission == "1" && sv2.tick == 200 && sv2.hash == sv.hash && sv2.cam_x == 12.5f && sv2.log.size() == sv.log.size());

  // combat: a footman next to an enemy footman kills it with the measured duel (600 dmg per 10 ticks)
  World f;
  CHECK(load_world(data, "1", f, err));
  GameSim fs(rules);
  fs.init(f);
  Object* mine0 = nullptr; Object* enemy = nullptr;
  for (Object& o : f.objects) { if (o.type == 0 && o.side == 0 && !mine0) mine0 = &o; if (o.side == 1 && o.type == 27 && !enemy) enemy = &o; }
  CHECK(mine0 && enemy);
  if (mine0 && enemy) {
    fs.teleport(f, enemy->id, mine0->cur_x / 256 + 40, mine0->cur_y / 256);
    Command at; at.type = Pct::Attack; at.side = 0; at.a = int32_t(enemy->id); at.ids = {mine0->id};
    fs.submit(at);
    for (int t = 0; t < 400 && enemy->alive; t++) { f.begin_tick(); fs.tick(f); }
    CHECK(!enemy->alive);
  }

  // economy: training spends money and spawns a unit after the build time
  World e;
  CHECK(load_world(data, "1", e, err));
  GameSim es(rules);
  es.init(e);
  e.side_state[0].money = 5000;
  size_t n0 = e.objects.size();
  std::vector<MenuItem> m = es.menu(e, 0);
  CHECK(!m.empty());
  int unit_type = -1;
  for (const MenuItem& it : m) if (it.enabled && !it.construct) { unit_type = it.type; break; }
  if (unit_type >= 0) {
    Command bc; bc.type = Pct::Build; bc.side = 0; bc.a = unit_type;
    es.submit(bc);
    for (int t = 0; t < 600 && e.objects.size() == n0; t++) { e.begin_tick(); es.tick(e); }
    CHECK(e.objects.size() == n0 + 1);
    CHECK(e.side_state[0].money <= 5000 - rules->type(unit_type).price + 100);
  }
}

static void test_script_path_hash() {
  auto rules = std::make_shared<RulesData>();
  rules->init_defaults();
  GameSim sim(rules);
  World w;
  w.terrain.w = w.terrain.h = 2;
  w.terrain.cells.resize(4);
  sim.init(w);
  std::vector<uint64_t> hashes;
  for (auto kind : {script::ActKind::TacticSetPath, script::ActKind::TacticSetCycledPath,
                    script::ActKind::TacticSetInvertedPath, script::ActKind::TacticSetCycledInvertedPath}) {
    script::Script s;
    script::Trigger t;
    script::Action a; a.kind = kind; a.id = 1; a.a = 0;
    t.actions.push_back(a);
    s.triggers.push_back(t);
    ScriptRunner runner;
    runner.set(s); runner.init(w); runner.tick(sim, w);
    auto h = runner.state_hash();
    CHECK(std::find(hashes.begin(), hashes.end(), h) == hashes.end());
    hashes.push_back(h);
  }
  script::Script s;
  script::Trigger t;
  script::Action a; a.kind = script::ActKind::BoatActivate;
  t.actions.push_back(a); s.triggers.push_back(t);
  ScriptRunner runner;
  runner.set(s); runner.init(w); runner.tick(sim, w);
  CHECK(runner.unsupported() == 1);
  runner.init(w);
  CHECK(runner.unsupported() == 0);
}

static void test_script_animation_clocks() {
  const auto directory = std::filesystem::temp_directory_path() / "aztec-script-clocks";
  std::filesystem::create_directories(directory);
  const auto path = directory / "clocks.scn";
  FILE* file = std::fopen(path.string().c_str(), "w");
  if (!file) { CHECK(false); std::filesystem::remove(directory); return; }
  std::fputs("trigger(0) conditions localTime == 1 actions side(local).victory end\n", file);
  CHECK(std::fclose(file) == 0);
  auto rules = std::make_shared<RulesData>();
  CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  for (bool imported : {false, true}) {
    World w;
    w.terrain.w = w.terrain.h = 20;
    w.terrain.cells.resize(400);
    Object o; o.id = 1; o.type = 14; o.hp = o.hp_max = 30000;
    o.attack_tick = 4;
    o.teleport(to_fixed(200), to_fixed(200));
    w.objects.push_back(o);
    if (imported) w.original_save = std::make_shared<SaveGame>();
    GameSim sim(rules);
    CHECK(sim.attach_script(path.string()));
    sim.init(w);
    Command pause; pause.type = Pct::PauseOn;
    sim.submit(pause);
    w.begin_tick(); sim.tick(w);
    CHECK(w.objects[0].attack_tick == 4 && w.objects[0].body_tick == -1);
    CHECK(w.outcome == kRunning);
    pause.type = Pct::PauseOff;
    sim.submit(pause);
    w.begin_tick(); sim.tick(w);
    CHECK(w.objects[0].attack_tick == 5 && w.objects[0].body_tick == 0);
    CHECK(w.outcome == (imported ? kRunning : kVictory));
    w.begin_tick(); sim.tick(w);
    CHECK(w.objects[0].attack_tick == 6 && w.objects[0].body_tick == 1);
  }
  std::filesystem::remove(path);
  std::filesystem::remove(directory);
}

static void test_script(const std::string& dir) {
  GameData data;
  std::string err;
  if (!data.open(dir, err)) { CHECK(false); return; }
  auto rules = std::make_shared<RulesData>();
  if (!rules->load_dir(AZTEC_TEST_RULES_DIR)) { CHECK(false); return; }
  // Keep the script timer checks independent of optional lighting resources.
  rules->set_sight(nullptr);
  World w;
  if (!load_world(data, "1", w, err)) { CHECK(false); return; }
  GameSim sim(rules);
  CHECK(sim.attach_script(data.map_file("mission_1", "scn")));
  sim.init(w);
  // trigger 0 (localTime == 1400): side 1 tactic 7 attacks town 0
  bool ordered = false;
  for (int t = 0; t < 1401; t++) {
    w.begin_tick();
    sim.tick(w);
    if (t == 1390)
      for (const Object& o : w.objects) if (o.side == 1 && o.tactic == 7 && o.order != uint8_t(Order::Idle)) ordered = true;
  }
  CHECK(!ordered);
  bool now = false;
  for (const Object& o : w.objects) if (o.side == 1 && o.tactic == 7 && o.alive && o.order != uint8_t(Order::Idle)) now = true;
  CHECK(now);
  CHECK(w.outcome == kRunning);
  // killing every enemy triggers side(local).enemies == 0 -> victory
  for (const Object& o : w.objects) if (o.side == 1) sim.script_kill(w, o.id);
  w.begin_tick();
  sim.tick(w);
  CHECK(w.outcome == kVictory);
  // determinism with scripts
  World a, b;
  CHECK(load_world(data, "1", a, err) && load_world(data, "1", b, err));
  GameSim sa(rules), sb(rules);
  sa.attach_script(data.map_file("mission_1", "scn")); sb.attach_script(data.map_file("mission_1", "scn"));
  sa.init(a); sb.init(b);
  for (int t = 0; t < 1600; t++) { a.begin_tick(); sa.tick(a); b.begin_tick(); sb.tick(b); }
  CHECK(sa.state_hash(a) == sb.state_hash(b));
}

static World production_world() {
  World w;
  w.terrain.w = w.terrain.h = 40;
  w.terrain.cells.resize(1600);
  w.resources = {40, 40, std::vector<uint16_t>(1600, 65)};
  SideDef sd; w.mission.sides.push_back(sd);
  sd.index = 1; w.mission.sides.push_back(sd);
  w.relation[0][1] = kFriend;
  TownDef town; town.type = 1; w.mission.towns.push_back(town);
  town.index = 1; town.x = 25; w.mission.towns.push_back(town);
  return w;
}

static void production_object(World& w, int type, int town, int x, int y) {
  Object o; o.id = uint32_t(w.objects.size() + 1); o.type = type; o.town = town;
  o.teleport(tile_center_fixed(x), tile_center_fixed(y));
  w.objects.push_back(o);
}

static void production_steps(GameSim& sim, World& w, int ticks) {
  for (int t = 0; t < ticks; ++t) { w.begin_tick(); sim.tick(w); }
}

static void test_production() {
  auto rules = std::make_shared<RulesData>();
  CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  CHECK(rules->type(12).harvest.terrain && rules->type(12).harvest.dates.size() == 6);
  CHECK(rules->type(13).harvest.rate_b == 16);
  CHECK(production_value(100, 1, 2, true, false, false) == 160);
  CHECK(production_value(100, 1, 2, true, false, true) == 53);
  CHECK(production_value(100, 0, 2, true, false, false) == 80);
  CHECK(production_value(100, 0, 2, true, false, true) == 120);
  CHECK(production_value(100, 1, 2, true, true, false) == 67);
  CHECK(production_value(100, 1, 2, true, true, true) == 200);
  CHECK(production_value(100, 1, 0, false, false, false) == 67);
  CHECK(production_value(100, 1, 1, false, false, true) == 100);

  HarvestState periodic; periodic.last_clock = 7198;
  CHECK(harvest_tick(periodic, 64, 7199, 2) == 0);
  CHECK(harvest_tick(periodic, 64, 0, 2) == 1);
  CHECK(periodic.accumulated == 0 && periodic.last_clock == 0);
  HarvestState residue;
  CHECK(harvest_tick(residue, 40, 1, 1) == 0 && residue.accumulated == 40);
  CHECK(harvest_tick(residue, 40, 2, 1) == 0 && residue.accumulated == 80);
  CHECK(harvest_tick(residue, 40, 3, 1) == 0 && residue.accumulated == 120);
  CHECK(harvest_tick(residue, 40, 4, 1) == 1 && residue.accumulated == 0);
  HarvestState schedule;
  std::vector<int> dates{0, 900};
  CHECK(harvest_tick(schedule, 256, 1200, -1, &dates) == 1 && schedule.cursor == 1);
  CHECK(harvest_tick(schedule, 256, 1201, -1, &dates) == 1 && schedule.cursor == 2);
  CHECK(harvest_tick(schedule, 256, 7199, -1, &dates) == 0);
  CHECK(harvest_tick(schedule, 256, 0, -1, &dates) == 0 && schedule.cursor == 0);
  CHECK(harvest_tick(schedule, 256, 1, -1, &dates) == 3);

  World mine = production_world();
  production_object(mine, 13, 0, 10, 10);
  // Occupied mine cells are (-1,0),(0,0). Ignore a high resource outside the mask.
  mine.resources.v.assign(1600, 0);
  mine.resources.v[10 * 40 + 9] = uint16_t(65 << 8);
  mine.resources.v[10 * 40 + 10] = uint16_t(65 << 8);
  mine.resources.v[9 * 40 + 10] = 65535;
  mine.mission.start_time = 7150;
  GameSim ms(rules); ms.init(mine);
  production_steps(ms, mine, 99); CHECK(mine.side_state[0].money == 0);
  production_steps(ms, mine, 1); CHECK(mine.side_state[0].money == 16 && mine.day_clock == 50);

  World farm = production_world();
  production_object(farm, 12, 0, 10, 10);
  production_object(farm, 18, 1, 24, 24); // another town never multiplies this farm
  World farm_fresh = farm;
  GameSim fs(rules); fs.init(farm);
  production_steps(fs, farm, 1000); CHECK(farm.side_state[0].money == 363);
  for (int warehouses : {1, 2}) {
    World w = farm_fresh;
    for (int i = 0; i < warehouses; ++i) production_object(w, i ? 70 : 18, 0, 20 + i * 4, 20);
    GameSim sim(rules); sim.init(w);
    production_steps(sim, w, 1000); CHECK(w.side_state[0].money == 484);
  }
  World mixed_channels = farm_fresh;
  mixed_channels.resources.v.assign(1600, uint16_t(65 | (65 << 8)));
  GameSim cs(rules); cs.init(mixed_channels);
  production_steps(cs, mixed_channels, 1000); CHECK(mixed_channels.side_state[0].money == 367);
  World mine_warehouse = mine; mine_warehouse.side_state[0].money = 0;
  production_object(mine_warehouse, 70, 0, 20, 20);
  GameSim mw(rules); mw.init(mine_warehouse);
  production_steps(mw, mine_warehouse, 100); CHECK(mine_warehouse.side_state[0].money == 16);
  World unfinished = farm_fresh;
  production_object(unfinished, 18, 0, 20, 20);
  GameSim us(rules); us.init(unfinished);
  unfinished.objects.back().progress = 0; unfinished.objects.back().build_total = 2000;
  production_steps(us, unfinished, 1000); CHECK(unfinished.side_state[0].money == 363);
  World hall = production_world(); production_object(hall, 11, 0, 10, 10);
  GameSim hs(rules); hs.init(hall);
  production_steps(hs, hall, 20);
  CHECK(hall.side_state[0].money == 0 && hall.repair_credits[0] == 400);
  production_steps(hs, hall, 280); CHECK(hall.side_state[0].money == 60);

  // At StartTime 1200, consume the first date immediately, without a full-cycle payout.
  for (auto pair : {std::pair{12, 850}, std::pair{39, 150}}) {
    World w = production_world(); w.mission.start_time = 1200;
    production_object(w, pair.first, 0, 10, 10);
    GameSim sim(rules); sim.init(w);
    production_steps(sim, w, 1); CHECK(w.side_state[0].money == 0 && w.objects[0].gold_harvest.cursor == 1);
    production_steps(sim, w, pair.second - 2); CHECK(w.side_state[0].money == 0);
    production_steps(sim, w, 1); CHECK(w.side_state[0].money > 0);
  }
  World w = production_world();
  production_object(w, 11, 0, 5, 5); production_object(w, 11, 1, 25, 25);
  production_object(w, 14, 1, 22, 25); production_object(w, 22, 1, 28, 25);
  GameSim sim(rules); sim.init(w); w.side_state[0].money = 2000;
  auto item = [&](int town, int type) {
    for (const auto& m : sim.menu(w, 0, town)) if (m.type == type) return m;
    return MenuItem{};
  };
  CHECK(!item(0, 1).enabled && item(1, 1).enabled); // local barracks
  production_object(w, 14, 0, 8, 5);
  CHECK(item(0, 2).enabled); // local barracks, side-wide type 22
  CHECK(!item(0, 14).enabled); // completed duplicate
  Command fast; fast.type = Pct::BuildMode; fast.a = 2; fast.ids = {2};
  sim.submit(fast); production_steps(sim, w, 1);
  CHECK(item(0, 0).price == 100 && item(1, 0).price == 200 && item(1, 0).time == 67);
  w.side_state[0].money = 199;
  Command buy; buy.type = Pct::Build; buy.a = 0; buy.ids = {2};
  sim.submit(buy); production_steps(sim, w, 1);
  CHECK(w.side_state[0].queue.empty() && w.side_state[0].money == 199);
  w.side_state[0].money = 200;
  sim.submit(buy); production_steps(sim, w, 1);
  CHECK(w.side_state[0].money == 0 && w.side_state[0].queue.front().town == 1);
  production_steps(sim, w, 66);
  CHECK(w.side_state[0].queue.empty() && w.objects.back().type == 0 && w.objects.back().town == 1);
  CHECK(w.side_state[0].money == 0); // no second charge at spawn

  sim.submit(buy); production_steps(sim, w, 1);
  CHECK(w.side_state[0].queue.empty()); // still cannot afford another unit
  w.side_state[0].money = 200;
  sim.submit(buy); production_steps(sim, w, 1);
  fast.a = 0; sim.submit(fast);
  Command cancel; cancel.type = Pct::QueueKill; sim.submit(cancel);
  production_steps(sim, w, 1);
  CHECK(w.side_state[0].money == 200 && w.side_state[0].queue.empty()); // refund the paid price
  fast.a = 2;

  World construction = production_world();
  GameSim bs(rules); bs.init(construction); construction.side_state[0].money = 5000;
  Command build; build.type = Pct::Build; build.a = 14; build.b = 15; build.c = 15;
  bs.submit(fast); production_steps(bs, construction, 1); // invalid member is ignored
  Command speed; speed.type = Pct::BuildMode; speed.a = 2; bs.submit(speed);
  production_steps(bs, construction, 1);
  MenuItem barracks;
  for (const auto& m : bs.menu(construction, 0)) if (m.type == 14) barracks = m;
  bs.submit(build); production_steps(bs, construction, 1);
  CHECK(construction.objects.size() == 1 && construction.objects[0].town == 0);
  CHECK(construction.objects[0].build_total == barracks.time && construction.side_state[0].money == 5000 - barracks.price);
  int cash = construction.side_state[0].money;
  bs.submit(build); production_steps(bs, construction, 1);
  CHECK(construction.objects.size() == 1 && construction.side_state[0].money == cash);

  // Synthetic command-log roundtrip across a full day, including price selection and harvest cursors.
  World replay_world = farm_fresh; replay_world.side_state.resize(8); replay_world.side_state[0].money = 1000;
  World original = replay_world;
  GameSim rs(rules); rs.init(original);
  fast.ids = {1}; rs.submit(fast);
  buy.ids = {1}; rs.submit(buy);
  production_steps(rs, original, 7210);
  std::vector<LoggedCommand> log;
  CHECK(deserialize_log(serialize_log(rs.log()), log));
  auto replay = GameSim::replay(replay_world, rules, log, rs.tick_count());
  CHECK(replay->state_hash(replay_world) == rs.state_hash(original));
}

static void test_town_production() {
  auto rules = std::make_shared<RulesData>();
  CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  auto item = [](GameSim& sim, const World& w, int town, int type) {
    for (const MenuItem& m : sim.menu(w, 0, town)) if (m.type == type) return m;
    MenuItem missing; missing.type = -1; return missing;
  };
  // Every listed hall upgrade replaces the same hall, even with sparse town section numbers.
  for (int race = 0; race < 3; ++race) for (int level = 1; level <= 3; ++level) {
    int target = rules->town_building(level, race);
    if (target < 0) continue;
    World fresh = production_world();
    fresh.mission.towns[0].index = 4; fresh.mission.towns[0].race = race; fresh.mission.towns[0].type = 0;
    production_object(fresh, rules->town_building(0, race), 4, 10, 10);
    fresh.objects[0].town_hall = true;
    fresh.side_state.resize(8); fresh.side_state[0].money = 10000;
    World w = fresh;
    GameSim sim(rules); sim.init(w);
    MenuItem upgrade = item(sim, w, 0, target);
    CHECK(upgrade.enabled && !upgrade.construct);
    CHECK(upgrade.price == rules->type(target).price);
    CHECK(upgrade.time == production_value(rules->type(target).time, 0, 1, false, false, true));
    Command buy; buy.type = Pct::Build; buy.a = target; buy.ids = {1};
    sim.submit(buy); sim.submit(buy); // repeated and competing upgrades are unavailable while reserved
    production_steps(sim, w, 1);
    CHECK(w.side_state[0].queue.size() == 1 && w.side_state[0].queue[0].remaining == upgrade.time - 1);
    CHECK(w.side_state[0].money == 10000 - upgrade.price);
    for (const MenuItem& m : sim.menu(w, 0)) if (m.type != (race == 0 ? 0 : race == 1 ? 27 : 53)) CHECK(!m.enabled);
    CHECK(w.objects.size() == 1 && w.objects[0].type == rules->town_building(0, race));
    Command pause; pause.type = Pct::PauseBuild; pause.a = 1; sim.submit(pause);
    production_steps(sim, w, 3);
    CHECK(w.side_state[0].queue[0].remaining == upgrade.time - 1);
    pause.a = 0; sim.submit(pause);
    production_steps(sim, w, upgrade.time - 2);
    CHECK(w.mission.towns[0].type == 0);
    production_steps(sim, w, 1);
    CHECK(w.mission.towns[0].type == level && w.mission.towns[1].type == 1);
    CHECK(w.objects.size() == 1 && w.objects[0].id == 1 && w.objects[0].type == target);
    CHECK(w.objects[0].town == 4 && w.objects[0].town_hall && w.town_state[0].hall == 1);
    CHECK(w.objects[0].hp == rules->type(target).hp_max && w.side_state[0].queue.empty());
    CHECK(item(sim, w, 0, race == 0 ? 14 : race == 1 ? 41 : 67).enabled);
    CHECK(!sim.can_place(w, 0, target, 10, 10)); // upgraded footprint remains blocked
    auto hash = sim.state_hash(w); w.mission.towns[0].type = 0;
    CHECK(sim.state_hash(w) != hash); w.mission.towns[0].type = level;
    SaveData save; save.mission = "synthetic-upgrade"; save.tick = sim.tick_count();
    save.hash = sim.state_hash(w); save.log = sim.log();
    SaveData decoded; CHECK(deserialize_save(serialize_save(save), decoded));
    auto replay = GameSim::replay(fresh, rules, decoded.log, decoded.tick);
    CHECK(replay->state_hash(fresh) == decoded.hash);
  }

  // All races' flag-0 farms/mines/defences use queued production, with normal costs and timers.
  for (auto choice : {std::pair{0, 12}, {0, 13}, {0, 17}, {1, 39}, {1, 48}, {2, 65}, {2, 66}}) {
    World fresh = production_world();
    int race = choice.first, type = choice.second;
    int level = type == 48 || type == 66 ? 2 : 1;
    fresh.mission.towns[0].race = race; fresh.mission.towns[0].type = level;
    fresh.resources.v.assign(1600, uint16_t(65 | (65 << 8)));
    fresh.side_state.resize(8); fresh.side_state[0].money = 10000;
    production_object(fresh, rules->town_building(level, race), 0, 10, 10);
    fresh.objects[0].town_hall = true;
    if (type == 17) production_object(fresh, 14, 0, 20, 20);
    World w = fresh; GameSim sim(rules); sim.init(w);
    MenuItem m = item(sim, w, 0, type);
    CHECK(m.enabled && !m.construct);
    CHECK(m.price == rules->type(type).price && m.time == rules->type(type).time);
    Command buy; buy.type = Pct::Build; buy.a = type; buy.ids = {1}; buy.b = -999; buy.c = -999;
    sim.submit(buy); sim.submit(buy); production_steps(sim, w, 1);
    CHECK(w.side_state[0].queue.size() == 1 && w.side_state[0].queue[0].paid == m.price);
    CHECK(w.side_state[0].money == 10000 - m.price);
    CHECK(w.objects.size() == fresh.objects.size() && !item(sim, w, 0, type).enabled);
    production_steps(sim, w, m.time - 2);
    CHECK(w.objects.size() == fresh.objects.size());
    production_steps(sim, w, 1);
    CHECK(w.side_state[0].queue.empty() && w.objects.size() == fresh.objects.size() + 1);
    CHECK(w.objects.back().type == type && w.objects.back().town == 0 && w.objects.back().progress == 1000);
    CHECK(!sim.can_place(w, 0, type, int(w.objects.back().cur_x >> 8) / kTilePx, int(w.objects.back().cur_y >> 8) / kTilePx));
    CHECK(!item(sim, w, 0, type).enabled);
    if (type == 12 || type == 39 || type == 65)
      CHECK(item(sim, w, 0, race == 0 ? 18 : race == 1 ? 40 : 70).enabled);
    // Income/repair accumulation begins at completion, using the resource map at the chosen site.
    if (type == 13) {
      CHECK(w.objects.back().gold_harvest.accumulated > 0);
      int money = w.side_state[0].money;
      production_steps(sim, w, 100); CHECK(w.side_state[0].money > money);
    }
    std::vector<LoggedCommand> log; CHECK(deserialize_log(serialize_log(sim.log()), log));
    auto replay = GameSim::replay(fresh, rules, log, sim.tick_count());
    CHECK(replay->state_hash(fresh) == sim.state_hash(w));
  }

  // A blocked map retains the completed reservation; cancellation releases it.
  World blocked = production_world();
  blocked.passability = {40, 40, std::vector<uint8_t>(1600, 0)};
  blocked.side_state.resize(8); blocked.side_state[0].money = 10000;
  production_object(blocked, 11, 0, 10, 10); blocked.objects[0].town_hall = true;
  GameSim blocked_sim(rules); blocked_sim.init(blocked);
  Command request; request.type = Pct::Build; request.a = 12;
  blocked_sim.submit(request); production_steps(blocked_sim, blocked, 200);
  CHECK(blocked.objects.size() == 1 && blocked.side_state[0].queue[0].remaining == 0);
  CHECK(!item(blocked_sim, blocked, 0, 12).enabled);
  Command remove; remove.type = Pct::QueueKill; blocked_sim.submit(remove);
  production_steps(blocked_sim, blocked, 1);
  CHECK(blocked.side_state[0].queue.empty() && item(blocked_sim, blocked, 0, 12).enabled);

  // Upgrade cancellation keeps the old hall/tree and releases competing upgrade choices.
  World cancelled = production_world(); cancelled.mission.towns[0].type = 0;
  cancelled.side_state.resize(8); cancelled.side_state[0].money = 10000;
  production_object(cancelled, 10, 0, 10, 10); cancelled.objects[0].town_hall = true;
  GameSim cancel_sim(rules); cancel_sim.init(cancelled);
  request.a = 19; cancel_sim.submit(request); production_steps(cancel_sim, cancelled, 1);
  cancel_sim.submit(remove); production_steps(cancel_sim, cancelled, 1);
  CHECK(cancelled.objects[0].type == 10 && cancelled.mission.towns[0].type == 0);
  CHECK(cancelled.side_state[0].money == 10000 && item(cancel_sim, cancelled, 0, 24).enabled);

  World w = production_world(); production_object(w, 11, 0, 10, 10);
  w.objects[0].town_hall = true;
  GameSim sim(rules); sim.init(w); w.side_state[0].money = 0;
  CHECK(!item(sim, w, 0, 12).enabled);
  w.side_state[0].money = 10000; w.mission.sides[0].disabled = {12};
  CHECK(!item(sim, w, 0, 12).enabled);
  w.mission.sides[0].disabled.clear();
  Command buy; buy.type = Pct::Build; buy.a = 12; buy.ids = {999};
  sim.submit(buy); production_steps(sim, w, 1); CHECK(w.side_state[0].queue.empty());
  buy.ids = {1}; sim.submit(buy); production_steps(sim, w, 1);
  int paid = w.side_state[0].queue[0].paid;
  // A dead hall holds completed production without spawning at an unrelated side start.
  sim.script_kill(w, 1); production_steps(sim, w, 200);
  CHECK(w.objects.size() == 1 && w.side_state[0].queue[0].remaining == 0);
  int money = w.side_state[0].money;
  Command cancel; cancel.type = Pct::QueueKill; sim.submit(cancel); production_steps(sim, w, 1);
  CHECK(w.side_state[0].queue.empty() && w.side_state[0].money == money + paid);
}

static void test_descriptor_combat() {
  auto rules = std::make_shared<RulesData>();
  CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  CHECK(rules->type(5).hp_max == 40000 && rules->type(5).secondary_hp == 36000);
  CHECK(rules->type(32).hp_max == 13000 && rules->type(32).secondary_hp == 23000);
  CHECK(rules->type(17).hp_max == 20000);
  CHECK(rules->type(7).hp_max == 9000 && !rules->type(7).nonorganic);
  CHECK(rules->type(33).hp_max == 13000 && !rules->type(33).nonorganic);
  for (int target : {14, 17}) {
    CHECK(rules->attack(0, target).damage == 150);
    CHECK(rules->attack(27, target).damage == 300);
    CHECK(rules->attack(1, target).damage == 1600);
  }
  for (int target : {7, 33}) {
    CHECK(rules->attack(0, target).damage == 0);
    CHECK(rules->attack(27, target).damage == 800);
    CHECK(rules->attack(1, target).damage == 2000);
  }
  CHECK(category_damage(2, {192, 192}) == 0); // combining factors first would give 1
  CHECK(category_damage(255, {256, 128}) == 127);
  CHECK(category_damage(256, {256, 128}) == 128);
  TypeRules target;
  target.physical_armor = 65;
  CHECK(rules->weapon(31, target).damage == 1777); // armor term rounds independently
  target.flier = true;
  CHECK(rules->weapon(31, target).damage == 0); // zero air suppresses armor too
  TypeRules ground;
  CHECK(rules->weapon(8, ground).damage == 3000);
  CHECK(rules->weapon(8, ground, true).damage == 0);
  ground.nonorganic = ground.structure = true;
  CHECK(rules->weapon(8, ground).damage == 1500); // OR predicate applies once
  CHECK(rules->weapon(16, ground).damage == 9000);
  CHECK(rules->weapon(11, ground).damage == 1125);
  ground = TypeRules{}; ground.category_e8 = true;
  CHECK(rules->weapon(11, ground).damage == 2250);
  CHECK(rules->weapon(16, ground).damage == 18000);
  CHECK(!rules->weapon(7, ground).damage_known); // unresolved composite children
  CHECK(!rules->weapon(19, ground).damage_known); // delegated class 7
  CHECK(!rules->weapon(3, target).damage_known); // class 7
  CHECK(!rules->weapon(34, target).damage_known); // burn
  CHECK(!rules->weapon(999, target).damage_known);
  CHECK(rules->attack(0, 0).reload == 9 && rules->attack(0, 0).range_px == 10);
  CHECK(rules->attack(27, 0).reload == 12 && rules->attack(27, 0).projectile_speed == 30);
  const auto& bear = rules->type(5);
  CHECK(regenerated_hp(bear, 19999, true) == 20044);
  CHECK(regenerated_hp(bear, 20000, true) == 20000);
  CHECK(regenerated_hp(bear, 20000, false) == 20045);
  CHECK(regenerated_hp(bear, 40000, false) == 40000);
  CHECK(regenerated_hp(bear, 0, false) == 0);
  CHECK(regenerated_hp(rules->type(33), 100, false) == 100);

  World fresh;
  fresh.terrain.w = fresh.terrain.h = 20;
  fresh.terrain.cells.resize(400);
  fresh.relation[0][1] = fresh.relation[1][0] = kEnemy;
  Object a; a.id = 1; a.type = 0; a.teleport(to_fixed(200), to_fixed(200));
  Object b; b.id = 2; b.type = 32; b.side = 1; b.hp = 5000;
  b.teleport(to_fixed(220), to_fixed(200));
  fresh.objects = {a, b};
  World world = fresh;
  GameSim sim(rules); sim.init(world);
  CHECK(world.objects[1].hp == 5000 && world.objects[1].hp_max == 13000);
  CHECK(world.objects[1].secondary_hp == 23000);
  auto hash = sim.state_hash(world);
  world.objects[1].secondary_hp++;
  CHECK(sim.state_hash(world) != hash);
  world.objects[1].secondary_hp--;
  world.objects[1].regen_alternate = true;
  CHECK(sim.state_hash(world) != hash);
  world.objects[1].regen_alternate = false;
  // A stationary, unarmed structure isolates cooldown from collision and retaliation.
  fresh.objects[1].type = 14; fresh.objects[1].hp = 30000;
  Object recovering; recovering.id = 3; recovering.type = 5; recovering.hp = 19999;
  recovering.regen_alternate = true; recovering.teleport(to_fixed(800), to_fixed(800));
  fresh.objects.push_back(recovering);
  world = fresh; sim.init(world);
  CHECK(!world.objects[0].has_fired);
  Command attack; attack.type = Pct::Attack; attack.side = 0; attack.ids = {1}; attack.a = 2;
  sim.submit(attack);
  for (int tick = 1; tick <= 24; ++tick) {
    world.begin_tick(); sim.tick(world);
    CHECK(world.objects[0].has_fired);
    CHECK(world.objects[0].attack_tick == (tick - 1) % 10);
    CHECK(world.objects[1].body_tick == tick - 1); // the built body clock continues through combat
    if (tick == 11 || tick == 21) CHECK(world.objects[0].body_tick > 0);
    auto visual_hash = sim.state_hash(world);
    ++world.objects[0].attack_tick;
    CHECK(sim.state_hash(world) != visual_hash);
    --world.objects[0].attack_tick;
    if (tick == 1 || tick == 11 || tick == 21) CHECK(world.objects[0].cooldown == 10);
    CHECK(world.objects[1].hp == 30000 - 150 * ((tick - 1) / 10 + 1));
    CHECK(world.objects[2].hp == 20044 && world.objects[2].secondary_hp == 36000);
    if (tick == 10) CHECK(world.objects[0].cooldown == 1);
    World replay_world = fresh;
    auto replay = GameSim::replay(replay_world, rules, sim.log(), tick);
    CHECK(replay->state_hash(replay_world) == sim.state_hash(world));
  }
  // Reusing a loader must not retain measurements, weapons or build trees.
  rules->init_defaults();
  CHECK(!rules->loaded() && !rules->weapon(4, target).damage_known);
}

static void test_town_commands() {
  auto rules = std::make_shared<RulesData>();
  CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  World initial;
  initial.terrain.w = initial.terrain.h = 80;
  initial.terrain.cells.resize(6400);
  initial.mission.towns = {{4, 0, 4, 4, 0, 0, -1}, {9, 0, 40, 40, 0, 0, -1},
                           {12, 1, 60, 60, 0, 0, -1}};
  auto add = [&](int type, int side, int town, int x, int y, bool hall = false) {
    Object o;
    o.id = uint32_t(initial.objects.size() + 1); o.type = type; o.side = side;
    o.town = town; o.town_hall = hall;
    o.teleport(tile_center_fixed(x), tile_center_fixed(y));
    initial.objects.push_back(o);
  };
  add(10, 0, 4, 4, 4, true);   // 1: first hall
  add(10, 0, 9, 40, 40, true); // 2: same-side second hall
  add(10, 1, 12, 60, 60, true);
  add(0, 0, 4, 8, 8);         // 4: extractable troop
  add(0, 0, 9, 42, 42);       // 5: other town's troop
  add(0, 0, -1, 10, 10);      // 6: existing independent tactic
  initial.objects.back().tactic = 7;
  add(14, 0, 4, 12, 12);      // 7: ordinary building
  add(17, 0, 4, 16, 16);      // 8: defence
  add(14, 0, 9, 44, 44);      // 9: other town's building
  add(14, 1, 12, 64, 64);     // 10: hostile building
  add(0, 0, 4, 18, 18);      // 11: dead member
  initial.objects.back().alive = false;
  add(0, 0, 4, 20, 20);      // 12: removed member
  initial.objects.back().removed = true;
  add(14, 0, 4, 22, 22);     // 13: current construction
  World w = initial;
  GameSim sim(rules); sim.init(w);
  w.objects[6].hp /= 2;
  w.objects[7].hp /= 2;
  // Store damaged mission inputs before init for command-log save replay.
  initial.objects[6].hp = w.objects[6].hp;
  initial.objects[7].hp = w.objects[7].hp;
  bool verify_replay = true;
  auto send = [&](Pct type, int a, int town, std::vector<uint32_t> ids = {}, uint8_t side = 0) {
    Command c; c.type = type; c.side = side; c.a = a; c.b = town; c.ids = std::move(ids);
    sim.submit(c); w.begin_tick(); sim.tick(w);
    if (verify_replay) {
      World replay_world = initial;
      auto replay = GameSim::replay(replay_world, rules, sim.log(), sim.tick_count());
      CHECK(replay->state_hash(replay_world) == sim.state_hash(w));
    }
  };
  send(Pct::Garrison, 0, 99);
  CHECK(w.objects[3].town == 4 && w.side_state[0].selected_tactic == -1);
  send(Pct::Garrison, 0, 12); // enemy town
  CHECK(w.objects[3].town == 4);
  w.selection.ids = {1};
  CHECK(CommandsUi::action_enabled(PanelAction::Garrison, w));
  CommandsUi ui(&sim);
  Input input; input.key_hit[SDL_SCANCODE_GRAVE] = true;
  Camera camera; Minimap minimap; SDL_Rect panel{0, 0, 204, 700};
  ui.update(input, w, camera, 1, minimap, false, panel);
  CHECK(w.objects[3].town == 4); // UI only submits a command
  w.begin_tick(); sim.tick(w);
  CHECK(sim.log().back().cmd.type == Pct::Garrison && sim.log().back().cmd.b == 4);
  CHECK(w.objects[3].town == -1 && w.objects[3].tactic == 8);
  CHECK(w.side_state[0].selected_tactic == 8 && w.selection.ids == std::vector<uint32_t>{4});
  CHECK(w.objects[4].town == 9 && w.objects[5].tactic == 7);
  CHECK(w.objects[6].town == 4 && w.objects[10].town == 4 && w.objects[11].town == 4);
  send(Pct::Garrison, 0, 4); // empty extraction does not select a new tactic
  CHECK(w.side_state[0].selected_tactic == 8);
  send(Pct::Garrison, 0, 9);
  CHECK(w.objects[4].tactic == 9 && w.side_state[0].selected_tactic == 9);
  send(Pct::RepairMode, 2, 4);
  CHECK(w.town_state[0].repair_mode == RepairMode::Destroy && w.town_state[0].marked.empty());
  send(Pct::DestroyHere, 0, 4, {7, 7, 8, 1, 4, 9, 10, 999, 0});
  CHECK((w.town_state[0].marked == std::vector<uint32_t>{7, 8}));
  CHECK(w.objects[6].alive && w.objects[7].alive);
  send(Pct::DestroyHere, 0, 4, {7});
  CHECK(w.town_state[0].marked == std::vector<uint32_t>{8});
  send(Pct::RepairMode, 2, 4); // same mode preserves membership
  CHECK(w.town_state[0].marked == std::vector<uint32_t>{8});
  send(Pct::RepairMode, 3, 4); // invalid mode preserves membership
  CHECK(w.town_state[0].marked == std::vector<uint32_t>{8});
  send(Pct::RepairMode, 1, 4);
  CHECK(w.town_state[0].marked.empty());
  send(Pct::DestroyHere, 0, 4, {7, 8});
  CHECK((w.town_state[0].marked == std::vector<uint32_t>{7, 8}));
  send(Pct::RepairMode, 2, 9);
  send(Pct::DestroyHere, 0, 9, {9}); // full HP allowed in destroy mode
  CHECK(w.town_state[1].marked == std::vector<uint32_t>{9});
  send(Pct::RepairMode, 1, 9);
  send(Pct::DestroyHere, 0, 9, {9}); // full HP rejected in repair mode
  CHECK(w.town_state[1].marked.empty());
  send(Pct::RepairMode, 0, 4);
  CHECK(w.town_state[0].marked == std::vector<uint32_t>{7}); // off-mode ordinary service
  send(Pct::RepairMode, 1, 4); // clears automatically collected marks
  CHECK(w.town_state[0].marked.empty());
  send(Pct::DestroyHere, 0, 4, {7, 8});
  CHECK(w.side_state[0].money == 0); // marking has no cash or HP effect
  SaveData save; save.mission = "synthetic-towns"; save.tick = sim.tick_count();
  save.hash = sim.state_hash(w); save.log = sim.log();
  SaveData restored;
  CHECK(deserialize_save(serialize_save(save), restored));
  World fresh = initial;
  auto replay = GameSim::replay(fresh, rules, restored.log, restored.tick);
  CHECK(replay->state_hash(fresh) == restored.hash);
  CHECK(fresh.town_state[0].marked == w.town_state[0].marked);
  World other_view = initial; other_view.local_side = 1;
  auto other_replay = GameSim::replay(other_view, rules, restored.log, restored.tick);
  CHECK(other_replay->state_hash(other_view) == restored.hash);
  CHECK(other_view.selection.ids.empty());
  verify_replay = false;
  const auto hash = sim.state_hash(w);
  w.objects[6].town = 9; CHECK(sim.state_hash(w) != hash); w.objects[6].town = 4;
  w.objects[0].town_hall = false; CHECK(sim.state_hash(w) != hash); w.objects[0].town_hall = true;
  w.town_state[0].marked.clear(); CHECK(sim.state_hash(w) != hash); w.town_state[0].marked = {7, 8};
  w.town_state[0].repair_mode = RepairMode::Destroy; CHECK(sim.state_hash(w) != hash);
  w.town_state[0].repair_mode = RepairMode::Repair;
  w.side_state[0].selected_tactic = -1; CHECK(sim.state_hash(w) != hash);
  w.objects[12].progress = 100;
  w.objects[12].build_total = 1000;
  w.objects[12].hp = 1;
  send(Pct::DestroyHere, 0, 4, {13});
  CHECK((w.town_state[0].marked == std::vector<uint32_t>{7, 8}));
  // Stale, repaired and reassigned objects are pruned independently of explicit toggles.
  w.objects[6].hp = w.objects[6].hp_max;
  w.objects[7].side = 1;
  send(Pct::Nothing, 0, 0);
  CHECK(w.town_state[0].marked.empty());
  w.objects[0].alive = false;
  send(Pct::Garrison, 0, 4);
  CHECK(w.side_state[0].selected_tactic == -1);
}

static void test_audio_pure() {
  // pure rules: no attenuation near the centre, silent far away, pan favours the near ear
  Positional c = positional_gain(0, 0);
  CHECK(c.audible && c.left > 0.99f && c.right > 0.99f);
  Positional far = positional_gain(900, 900);
  CHECK(!far.audible);
  Positional right = positional_gain(300, 300);
  CHECK(right.audible && right.right > right.left);
  Positional mid = positional_gain(0, 650);
  CHECK(mid.audible && mid.left < 0.6f && mid.left > 0.01f);
  // mixer: a voice plays to its end and is released; music stream end is reported
  auto pcm = std::make_shared<Pcm>(200, int16_t(1000));
  Mixer m;
  m.play(pcm, 1.f, 0.5f);
  int16_t out[400];
  m.mix(out, 200);
  CHECK(out[0] == int16_t(1000 * 0.8f) && out[1] == int16_t(1000 * 0.5f * 0.8f));
  CHECK(m.active_voices() == 0);
}

static void test_audio_data(const std::string& dir) {
  // with the real game data: sound set -> mixed non-silent output; music table selection
  AudioSystem a;
  a.init(dir);
  if (a.bank_ok()) {
    int played = 0;
    for (uint32_t id = 1; id < 400 && !played; id++) played = a.play_sound(id) != 0;
    CHECK(played);
    CHECK(a.mixer().active_voices() >= 1);
    a.start_music(0, 0xffff, 1);   // any available track of the first disc
    CHECK(a.current_track() == 0 || a.mixer().music_playing());
  }
}

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--pure") {
    test_pure();
    test_sight();
    test_production();
    test_town_production();
    test_projectiles();
    test_descriptor_combat();
    test_script_animation_clocks();
    test_script_path_hash();
    test_town_commands();
    test_sprite_metadata();
    test_audio_pure();
    test_commands_ui();
  } else {
    std::string dir = test_game_dir(argc, argv);
    if (!test_game_dir_exists(dir)) return 77;
    test_data(dir); test_sim(dir); test_script(dir); test_audio_data(dir);
    test_sprite_assets(dir);
  }
  std::printf(failures ? "FAILED\n" : "OK\n");
  return failures ? 1 : 0;
}
