// Invented maps, descriptors and weapon values; no original installation is used.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <vector>
#include "engine/sim/game_sim.hpp"
#include "engine/sim/sight.hpp"
#include "temp_dir.hpp"

using namespace aztec;
using namespace aztec::engine;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

static void row(std::ofstream& out, int param, const std::vector<int>& values) {
  out << param;
  for (int value : values) out << ',' << value;
  out << '\n';
}
static auto rules() {
  TempDir directory;
  std::ofstream(directory.path() / "types.csv") <<
      "id,ugi,kind,param,price,time,level\n"
      "0,UNIT,Marine,0,7,5,0\n54,HYPNO,Marine,1,7,5,0\n"
      "55,CHIEF,Marine,2,7,5,0\n57,ELEFANT,Marine,3,7,5,0\n"
      "82,ABDULA,Marine,4,7,5,0\n64,HALL,Building,0,7,5,0\n"
      "67,BARRACKS,Building,0,7,5,0\n72,SCHOOL,Building,0,7,5,0\n"
      "73,PRISON,Building,0,7,5,0\n69,TOWER,Defence,0,7,5,0\n"
      "86,SHIPYARD,Defence,1,7,5,0\n88,JOHNKA,Johnka,0,7,5,0\n";
  {
    std::ofstream out(directory.path() / "desc_marine.csv"); out << "param,fields\n";
    const int weapons[] = {1, 56, 55, 47, 59};
    for (int i = 0; i < 5; ++i) {
      std::vector<int> d(19); d[1] = d[3] = 500; d[4] = 23; d[5] = 2;
      d[7] = weapons[i]; d[11] = i == 1 ? 65793 : 257; d[12] = d[13] = d[14] = 256;
      row(out, i, d);
    }
  }
  {
    std::ofstream out(directory.path() / "desc_building.csv"); out << "param,fields\n";
    std::vector<int> d(322); d[1] = 500; d[30] = 2; d[64] = d[320] = -1;
    row(out, 0, d);
  }
  {
    std::ofstream out(directory.path() / "desc_defence.csv"); out << "param,fields\n";
    std::vector<int> d(68); d[3] = 500; d[33] = 2; d[59] = 57;
    row(out, 0, d); d[59] = -1; row(out, 1, d);
  }
  std::ofstream(directory.path() / "weapons_ids.csv") <<
      "weapon_id,class,param\n1,2,0\n55,2,0\n56,13,0\n57,13,0\n59,2,0\n"
      "47,12,0\n46,0,0\n45,9,0\n54,8,0\n53,7,0\n";
  std::ofstream(directory.path() / "weapon_class_w2_13.csv") <<
      "param,fields\n0,3,17,0,0,200,0,256,256,256,256,0,0,0,0\n";
  std::ofstream(directory.path() / "weapon_class_w12.csv") << "param,fields\n0,46,46,5\n";
  std::ofstream(directory.path() / "weapon_class_ball0.csv") << "param,fields\n0,1,2,3,200,45\n";
  std::ofstream(directory.path() / "weapon_class_w9.csv") << "param,fields\n0,19,0,0,0,256,256,256,256\n";
  std::ofstream(directory.path() / "weapon_class_w8.csv") << "param,fields\n0,53,9,123,3\n";
  std::ofstream(directory.path() / "weapon_class_w7.csv") << "param,fields\n0,1,2,4,0,13,0,0,0,0\n";
  std::ofstream(directory.path() / "townhall_ids.csv") << "level,a,b,c\n1,11,38,64\n";
  std::ofstream(directory.path() / "buildtree.csv") <<
      "race,level,slot,type,map,prereq\n2,1,0,54,0,67 72A\n2,1,1,55,0,67 73A\n";
  auto data = std::make_shared<RulesData>(); CHECK(data->load_dir(directory.path().string()));
  return data;
}
static World world() {
  World w; w.terrain.w = w.terrain.h = 32; w.terrain.cells.resize(1024); w.side_state.resize(8);
  for (int a = 0; a < 8; ++a) for (int b = 0; b < 8; ++b) w.relation[a][b] = a == b ? kFriend : kNeutral;
  return w;
}
static void add(World& w, int type, int side, int x, int y, int town = -1) {
  Object o; o.id = uint32_t(w.objects.size() + 1); o.type = type; o.side = side; o.town = town;
  o.teleport(tile_center_fixed(x), tile_center_fixed(y)); w.objects.push_back(o);
}
static void ticks(GameSim& sim, World& w, int count = 1) {
  while (count--) { w.begin_tick(); sim.tick(w); }
}
static bool enabled(const GameSim& sim, const World& w, int type) {
  for (const auto& item : sim.menu(w, 0, 0)) if (item.type == type) return item.enabled;
  return false;
}
static void school_and_prison() {
  auto data = rules(); World w = world();
  w.mission.towns = {{0,0,4,4,1,2,10}, {1,0,24,24,1,2,11}};
  add(w,64,0,4,4,0); w.objects.back().town_hall = true;
  add(w,64,0,24,24,1); w.objects.back().town_hall = true;
  add(w,67,0,5,4,0); add(w,72,0,25,24,1); add(w,73,0,26,24,1);
  w.side_state[0].money = 1000;
  GameSim sim(data); sim.init(w);
  w.objects[3].progress = 999;
  CHECK(!enabled(sim,w,54) && enabled(sim,w,55));
  w.objects[3].progress = 1000; CHECK(enabled(sim,w,54));
  w.objects[3].side = 1; CHECK(!enabled(sim,w,54));
  w.objects[3].side = 0; w.objects[4].alive = false; CHECK(!enabled(sim,w,55));
  w.objects[4].alive = true;
  Command buy; buy.type = Pct::Build; buy.a = 54; buy.ids = {1}; sim.submit(buy);
  ticks(sim,w,6);
  CHECK(std::count_if(w.objects.begin(),w.objects.end(),[](const auto& o) { return o.type == 54; }) == 1);
  CHECK(w.objects.back().side == 0 && w.objects.back().town == 0);
  CHECK(w.objects.back().cur_x < tile_center_fixed(10)); // trained in the requesting town
}
static void ordinary_damage() {
  auto data = rules();
  for (int attacker : {54,55,69,82}) {
    World w = world(); add(w,attacker,0,10,10); add(w,0,1,11,10);
    w.relation[0][1] = w.relation[1][0] = kEnemy; // towers re-pick an enemy after every shot
    GameSim sim(data); sim.init(w);
    w.objects[0].hp = 300; w.objects[1].cooldown = 100; // isolate damage from automatic retaliation
    Command attack; attack.type = Pct::Attack; attack.ids = {1}; attack.a = 2;
    if (attacker == 69) { w.objects[0].order = uint8_t(Order::Attack); w.objects[0].target = 2; }
    else sim.submit(attack);
    ticks(sim,w);
    CHECK(w.objects[1].hp == 483 && w.objects[1].secondary_hp == 23 && w.objects[1].side == 1);
    CHECK(w.objects[0].hp == 300); // no healing special
    w.objects[1].hp = 1; ticks(sim,w,5);
    CHECK(!w.objects[1].alive && w.objects[1].side == 1 && w.objects[1].secondary_hp == 23);
  }
  CHECK(data->type(57).weapon_id == 47 && data->type(57).weapon.components.size() == 2);
  CHECK(data->type(57).weapon.components[0].splash_px == 0); // class 9 is a point effect, not an explosion
  CHECK(data->type(86).weapon_id == -1 && !data->type(86).armed);
  CHECK(data->type(88).weapon_id == 54 && data->type(88).weapon.klass == 8);
  CHECK(data->type(88).weapon.payload_id == 53 && data->type(88).weapon.range == 123);
  const auto& volley = data->type(88).weapon;
  CHECK(volley.spread_px == 9 && volley.reload == 3 && volley.duration == 10);
  CHECK(volley.impact_anim == 1 && volley.splash_px == 0); // explosion belongs to the payload
  const auto shot = data->attack(88,0);
  CHECK(shot.damage_known && shot.getters_known && shot.range_px == 123 && shot.reload == 3);
  World w = world(); add(w,88,0,10,10); add(w,0,1,11,10);
  GameSim sim(data); sim.init(w);
  Command attack; attack.type = Pct::Attack; attack.ids = {1}; attack.a = 2; sim.submit(attack);
  ticks(sim,w);
  CHECK(w.effects.size() == 16 && w.objects[1].hp == 500); // scheduled, no immediate direct shot
  for (const auto& effect : w.effects) {
    CHECK(effect.payload_id == 53 && effect.impact_anim == 1);
    CHECK(effect.delay_ticks >= 0 && effect.delay_ticks < 10);
  }
}
static World town(int level, int attacker_x = 12, int attacker_y = 10) {
  World w = world(); w.mission.towns = {{0,0,10,10,level,2,7}};
  add(w,64,0,10,10,0); w.objects.back().town_hall = true;
  add(w,0,1,attacker_x,attacker_y); w.objects.back().cooldown = 100; return w;
}
static void capture() {
  auto data = rules();
  for (int level : {0,1}) {
    World w = town(level); add(w,0,0,16,10); GameSim sim(data); sim.init(w); ticks(sim,w);
    CHECK(w.mission.towns[0].side == (level == 0 ? 1 : 0));
  }
  for (int guard : {72,73,86,33}) {
    World w = town(0); add(w,guard,0,13,10); GameSim sim(data); sim.init(w); ticks(sim,w);
    CHECK(w.mission.towns[0].side == (guard == 72 || guard == 73 ? 1 : 0));
  }
  World diagonal = town(0,12,11); GameSim ds(data); ds.init(diagonal); ticks(ds,diagonal);
  CHECK(diagonal.mission.towns[0].side == 0); // squared distance five is excluded
  World edge = town(0); edge.objects[1].cur_x += to_fixed(23);
  GameSim es(data); es.init(edge); ticks(es,edge); CHECK(edge.mission.towns[0].side == 1);
  World building = town(0); add(building,86,0,28,28,0);
  GameSim construction(data); construction.init(building);
  building.objects.back().progress = 500;
  QueueItem active; active.started = active.paused = true; active.town = 0;
  active.type = 86; active.object = 3; active.remaining = active.total = 100;
  building.side_state[0].queue.push_back(active);
  ticks(construction,building); CHECK(building.mission.towns[0].side == 0);
  building.side_state[0].queue.clear();
  ticks(construction,building); CHECK(building.mission.towns[0].side == 1);
  World initial = town(0); add(initial,72,0,11,11,0); World live = initial;
  GameSim sim(data); sim.init(live); ticks(sim,live);
  CHECK(live.objects[2].side == 1 && live.objects[2].hp == 500 && live.town_state[0].side == 1);
  auto replay = GameSim::replay(initial,data,sim.log(),sim.tick_count());
  CHECK(replay->state_hash(initial) == sim.state_hash(live));
}
static void gifts() {
  auto data = rules(); World a = world(); add(a,0,0,10,10); World b = a;
  GameSim sa(data,Modifications::enhanced()), sb(data,Modifications::enhanced()); sa.init(a); sb.init(b);
  sa.transfer_objects(a,{1},1); b.objects[0].side = 1;
  CHECK(sa.state_hash(a) != sb.state_hash(b)); // ownership includes the mover's team
  sb.transfer_objects(b,{1},1); CHECK(sa.state_hash(a) == sb.state_hash(b));
  const auto hash = sa.state_hash(a); sa.transfer_objects(a,{1},99); CHECK(sa.state_hash(a) == hash);
}
static void cameras() {
  TempDir directory; const auto file = directory.path()/"cameras.scn";
  std::ofstream(file) <<
      "trigger(0) conditions localTime == 1 actions side(1).cameraActivate(0,2) trigger(0).disable end\n"
      "trigger(1) conditions localTime == 4 actions side(1).cameraActivate(0,-1) trigger(1).disable end\n"
      "trigger(2) conditions localTime == 5 actions side(1).cameraDeactivate(0) trigger(2).disable end\n";
  auto data = rules(); auto sight = std::make_shared<SightData>();
  for (auto& day : sight->daylight) day.fill(80);
  auto& mask = sight->masks[64]; mask.assign(404,0); mask[0] = mask[2] = 20;
  data->set_sight(sight);
  World initial = world(); SideDef side; side.index = 1; side.cameras = {{10,10,-50}};
  initial.mission.sides.push_back(side); World w = initial;
  GameSim sim(data); CHECK(sim.attach_script(file.string())); sim.init(w);
  ticks(sim,w); CHECK(w.script_cameras.size() == 1 && w.script_cameras[0].active && w.center_serial == 0);
  const size_t cell = 10*33+10;
  CHECK(w.visibility.sides[1].v[cell] == 80 && w.visibility.sides[0].v[cell] == 0);
  ticks(sim,w); CHECK(w.script_cameras.size() == 1 && w.script_cameras[0].active);
  ticks(sim,w); CHECK(!w.script_cameras.back().active && w.visibility.sides[1].v[cell] == 32);
  ticks(sim,w); CHECK(w.script_cameras.back().ticks_left == -1 && w.script_cameras.back().active);
  ticks(sim,w); CHECK(!w.script_cameras.back().active && sim.script().unsupported() == 0);
  auto replay = GameSim::replay(initial,data,sim.log(),sim.tick_count(),file.string());
  CHECK(replay->state_hash(initial) == sim.state_hash(w));
  // An expired latest marker must not expose an older marker to deactivation.
  script::Script script;
  CHECK(script::parse_script(
      "trigger(0) conditions localTime == 1 actions side(1).cameraActivate(0,0) trigger(0).disable end "
      "trigger(1) conditions localTime == 2 actions side(1).cameraActivate(0,1) trigger(1).disable end "
      "trigger(2) conditions localTime == 3 actions side(1).cameraDeactivate(0) trigger(2).disable end "
      "trigger(3) conditions localTime == 4 actions side(1).cameraActivate(1,-1) trigger(3).disable end", script));
  w.mission.sides[0].cameras.push_back({12,12,100});
  w.script_cameras.clear();
  ScriptRunner runner; runner.set(std::move(script)); runner.init(w);
  runner.tick(sim,w); runner.tick(sim,w); CHECK(w.script_cameras.size() == 2);
  runner.tick(sim,w); CHECK(w.script_cameras.size() == 2 && w.script_cameras[0].active &&
      !w.script_cameras[0].latest && !w.script_cameras[1].active);
  runner.tick(sim,w); CHECK(w.script_cameras.size() == 3 && w.script_cameras.back().mask == 71);
}
static void johnka() {
  auto data = rules(); World initial = world(); add(initial,88,0,20,20);
  initial.objects[0].source = ObjectSource::Johnka; initial.objects[0].scenario = 9;
  initial.objects[0].vessel_enabled = false; World w = initial;
  GameSim sim(data); sim.init(w); CHECK(w.objects[0].removed && w.objects[0].mover == -1);
  CHECK(sim.activate_vessel(w,9,ObjectSource::Johnka)); const auto start = w.objects[0].cur_x;
  add(w,0,0,20,19);
  Command board; board.type = Pct::Board; board.ids = {2}; board.a = 1; sim.submit(board);
  ticks(sim,w); CHECK(w.objects[0].cur_x == start + to_fixed(5));
  CHECK(w.objects[0].cargo.empty() && !w.objects[1].boarding);
  CHECK(sim.activate_vessel(w,9,ObjectSource::Johnka,false));
  ticks(sim,w); CHECK(!w.objects[0].removed); // finish this patrol leg
  ticks(sim,w,300); CHECK(w.objects[0].removed && !w.objects[0].vessel_route);
  CHECK(sim.activate_vessel(w,9,ObjectSource::Johnka));
  const auto turn = w.objects[0].cur_x; ticks(sim,w);
  CHECK(w.objects[0].cur_x == turn - to_fixed(5));
  // Startup patrol and its turn phase reconstruct without external state.
  initial.objects[0].vessel_enabled = true; w = initial; GameSim moving(data); moving.init(w); ticks(moving,w,400);
  auto replay = GameSim::replay(initial,data,moving.log(),moving.tick_count());
  CHECK(replay->state_hash(initial) == moving.state_hash(w));
}
int main() {
  school_and_prison(); ordinary_damage(); capture(); gifts(); cameras(); johnka();
  return 0;
}
