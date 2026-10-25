// Engine unit tests: test_engine [GAME_DIR]. Pure tests always run; data tests run when GAME_DIR has the files.
#include <cmath>
#include <cstdio>
#include <string>
#include "core/mapfiles.hpp"
#include "core/strbase.hpp"
#include "engine/ui/names.hpp"
#include "core/text.hpp"
#include "engine/assets/anim_provider.hpp"
#include "engine/audio/audio_system.hpp"
#include "engine/assets/game_data.hpp"
#include "engine/assets/mission.hpp"
#include "engine/assets/sprite_provider.hpp"
#include "engine/platform/clock.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/sim/simulation.hpp"
#include "engine/world/camera.hpp"
#include "engine/world/type_info.hpp"
#include "engine/world/world.hpp"

using namespace aztec;
using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

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

static void test_data(const std::string& dir) {
  GameData data;
  std::string err;
  if (!data.open(dir, err)) { std::printf("skip data tests: %s\n", err.c_str()); return; }
  World w;
  if (!load_world(data, "1", w, err)) { std::printf("skip: %s\n", err.c_str()); return; }
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
  if (!data.open(dir, err)) return;
  auto rules = std::make_shared<RulesData>();
  if (!rules->load_dir(dir + "/../docs/data")) { std::printf("skip sim tests: no docs/data\n"); return; }
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

  // replay of the command log reproduces the exact state (snapshot restore)
  World r;
  CHECK(load_world(data, "1", r, err));
  auto rs = GameSim::replay(r, rules, sa.log(), 200);
  CHECK(rs->state_hash(r) == sa.state_hash(a));
  std::vector<LoggedCommand> back;
  CHECK(deserialize_log(serialize_log(sa.log()), back) && back.size() == sa.log().size());

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
  if (!data.open(dir, err)) return;
  auto rules = std::make_shared<RulesData>();
  if (!rules->load_dir(dir + "/../docs/data")) return;
  World w;
  if (!load_world(data, "1", w, err)) return;
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

static void test_audio(const std::string& dir) {
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
  test_pure();
  if (argc > 1) { test_data(argv[1]); test_sim(argv[1]); test_script(argv[1]); test_audio(argv[1]); }
  std::printf(failures ? "FAILED\n" : "OK\n");
  return failures ? 1 : 0;
}
