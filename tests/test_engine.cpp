// Engine tests: test_engine --pure, or test_engine [GAME_DIR] for game-data checks.
#include "game_dir.hpp"
#include <cmath>
#include <cstdio>
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
#include "engine/sim/savegame.hpp"
#include "engine/sim/simulation.hpp"
#include "engine/world/camera.hpp"
#include "engine/world/type_info.hpp"
#include "engine/world/world.hpp"

using namespace aztec;
using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

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
  auto put = [&](size_t offset, uint32_t value) { std::memcpy(bytes.data() + offset, &value, 4); };
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
}

static void test_sprite_assets(const std::string& dir) {
  using namespace aztec::data;
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
  CHECK(type_info(0) && std::string(type_info(0)->name) == "FOOTMAN" && type_info(10)->is_structure() && !type_info(90));

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
  CHECK(CommandsUi::action_enabled(PanelAction::Destroy, w));
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
    const TypeInfo* ti = type_info(o.type);
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

static void test_sim(const std::string& dir) {
  GameData data;
  std::string err;
  if (!data.open(dir, err)) { CHECK(false); return; }
  auto rules = std::make_shared<RulesData>();
  if (!rules->load_dir(AZTEC_TEST_RULES_DIR)) { CHECK(false); return; }
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

static void test_script(const std::string& dir) {
  GameData data;
  std::string err;
  if (!data.open(dir, err)) { CHECK(false); return; }
  auto rules = std::make_shared<RulesData>();
  if (!rules->load_dir(AZTEC_TEST_RULES_DIR)) { CHECK(false); return; }
  World w;
  if (!load_world(data, "1", w, err)) { CHECK(false); return; }
  GameSim sim(rules);
  CHECK(sim.attach_script(data.map_file("mission_1", "scn")));
  sim.init(w);
  // trigger 0 (localTime == 1400): side 1 tactic 7 attacks town 0
  bool ordered = false;
  for (int t = 0; t < 1500; t++) {
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
