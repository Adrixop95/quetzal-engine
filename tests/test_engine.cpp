// Engine unit tests: test_engine [GAME_DIR]. Pure tests always run; data tests run when GAME_DIR has the files.
#include <cmath>
#include <cstdio>
#include <string>
#include "core/mapfiles.hpp"
#include "core/text.hpp"
#include "engine/assets/anim_provider.hpp"
#include "engine/assets/game_data.hpp"
#include "engine/assets/mission.hpp"
#include "engine/assets/sprite_provider.hpp"
#include "engine/platform/clock.hpp"
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

  int x, y, t, hp;
  CHECK(parse_unit_line(" 35, 41,  0, 6000", x, y, t, hp) && x == 35 && y == 41 && t == 0 && hp == 6000);
  CHECK(parse_unit_line("11,10,4,12000", x, y, t, hp) && t == 4 && hp == 12000);
  CHECK(!parse_unit_line("garbage", x, y, t, hp));

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

int main(int argc, char** argv) {
  test_pure();
  if (argc > 1) test_data(argv[1]);
  std::printf(failures ? "FAILED\n" : "OK\n");
  return failures ? 1 : 0;
}
