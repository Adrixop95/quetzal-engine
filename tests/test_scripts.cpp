// Synthetic script contracts. These establish engine behavior, not reference parity.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <set>
#include <string>

#include "core/script.hpp"
#include "engine/sim/game_sim.hpp"
#include "temp_dir.hpp"

using namespace aztec;
using namespace aztec::engine;
using namespace aztec::script;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

static std::shared_ptr<RulesData> rules() {
  auto r = std::make_shared<RulesData>(); r->init_defaults(); return r;
}
static World world() {
  World w; w.terrain.w = w.terrain.h = 32; w.terrain.cells.resize(1024);
  w.side_state.resize(8); w.script_side = 2; w.local_side = 0;
  for (int a = 0; a < 8; ++a) for (int b = 0; b < 8; ++b) w.relation[a][b] = a == b ? kFriend : kEnemy;
  return w;
}
static void object(World& w, int type, int side, int tactic, int x, int y) {
  Object o; o.id = uint32_t(w.objects.size() + 1); o.type = type; o.side = side;
  o.hp = o.hp_max = 100; o.tactic = tactic; o.teleport(tile_center_fixed(x), tile_center_fixed(y));
  w.objects.push_back(o);
}
static Script parse(const std::string& text) {
  Script s; CHECK(parse_script(text, s)); return s;
}
static bool condition(const ScriptRunner& r, const World& w, const std::string& text) {
  const auto s = parse("trigger(0) conditions " + text + " actions end");
  CHECK(s.triggers[0].conditions.size() == 1);
  return r.eval(w, s.triggers[0].conditions[0]);
}
static ScriptRunner runner(World& w, const std::string& text) {
  ScriptRunner r; r.set(parse(text)); r.init(w); return r;
}

static void order_and_enablement() {
  World w = world(); GameSim sim(rules()); sim.init(w);
  auto r = runner(w,
      "trigger(9) conditions always actions side(local).addMoney(1) trigger(9).disable end "
      "trigger(2) conditions always actions trigger(9).enable trigger(7).enable trigger(3).disable trigger(2).disable side(local).addMoney(10) end "
      "trigger(7) conditions always actions side(local).addMoney(100) trigger(7).disable end "
      "trigger(3) conditions always actions side(local).addMoney(1000) end "
      "trigger(4) conditions always actions side(local).addMoney(10000) end");
  r.tick(sim, w);
  CHECK(w.side_state[2].money == 10110); // forward enable is immediate, backward enable waits
  r.tick(sim, w); CHECK(w.side_state[2].money == 20111);
  r.tick(sim, w); CHECK(w.side_state[2].money == 30111); // conditions are level-triggered
  CHECK(w.side_state[0].money == 0 && r.unsupported() == 0);
  r.init(w); const auto fresh = r.state_hash();
  r.tick(sim, w); CHECK(r.state_hash() != fresh); r.init(w); CHECK(r.state_hash() == fresh);
  // Repeated IDs use source order and shared ID enablement under the engine policy.
  auto duplicates = runner(w,
      "trigger(5) conditions always actions side(0).addMoney(1) trigger(5).disable end "
      "trigger(5) conditions always actions side(0).addMoney(100) end");
  duplicates.tick(sim, w); CHECK(w.side_state[0].money == 1);
}

static void timers() {
  World w = world(); GameSim sim(rules()); sim.init(w);
  auto r = runner(w,
      "trigger(0) conditions localTime == 1 actions timer(1).set(5) timer(1).stop end "
      "trigger(1) conditions globalTime == 2 actions timer(1).start timer(0).kill end "
      "trigger(2) conditions localTime == 4 actions timer(1).kill end "
      "trigger(3) conditions localTime == 5 actions timer(1).start end "
      "trigger(4) conditions localTime == 6 actions timer(1).set(-2) end "
      "trigger(5) conditions timer(1) == 5 actions side(local).addMoney(1) end");
  CHECK(condition(r, w, "timer(0) == 0"));
  r.tick(sim, w); CHECK(condition(r, w, "timer(0) == 1")); CHECK(condition(r, w, "timer(1) == 5"));
  CHECK(w.side_state[2].money == 1); // later triggers see this tick's timer assignment
  r.tick(sim, w); CHECK(condition(r, w, "timer(1) == 5")); CHECK(w.side_state[2].money == 2);
  CHECK(!condition(r, w, "timer(0) != 100")); // killed timer never satisfies a comparison
  r.tick(sim, w); CHECK(condition(r, w, "timer(1) == 6"));
  r.tick(sim, w); CHECK(!condition(r, w, "timer(1) == 7"));
  r.tick(sim, w); CHECK(condition(r, w, "timer(1) == 7")); // restart retains value
  r.tick(sim, w); CHECK(condition(r, w, "timer(1) == -2"));
  r.tick(sim, w); CHECK(condition(r, w, "timer(1) == -1"));
  CHECK(condition(r, w, "localTime == 7") && condition(r, w, "globalTime == 7"));
  CHECK(condition(r, w, "timer(99) == 0")); // absent timer policy remains explicit
}

static void conditions_and_identity() {
  World w = world(); object(w, 0, 2, 10, 3, 4); object(w, 27, 2, 10, 6, 7);
  object(w, 11, 2, -1, 12, 12); object(w, 17, 2, -1, 15, 15);
  object(w, 0, 1, 10, 20, 20); object(w, 11, 1, -1, 25, 25);
  w.objects[3].scenario = 42; w.objects[3].defence = true;
  w.side_state[2].money = 50;
  GameSim sim(rules()); sim.init(w); auto r = runner(w, "");
  const std::string checks[] = {
    "always", "localTime == 0", "globalTime == 0", "timer(0) == 0",
    "side(local).units == 2", "side(local).towns == 1", "side(local).enemies == 2",
    "side(local).tactics == 1", "side(local).money == 50", "side(local).unitsInRect(3,4,6,7) == 2",
    "side(local).unitsOfUGIInRect(UGI_FOOTMAN,3,4,3,4) == 1", "side(local).unitsOfId(UGI_FOOTMAN) == 1",
    "side(local).tactic(10).valid == 1", "side(local).tactic(10).idle == 1",
    "side(local).tactic(10).locked == 0", "side(local).tactic(10).attacked == -1",
    "side(local).defence(42).valid == 1", "side(local).isEnemy(1) == 1",
    "side(local).isFriendly(local) == 1", "side(local).isNeutral(1) == 0"
  };
  std::set<CondKind> covered;
  for (const auto& c : checks) {
    CHECK(condition(r, w, c));
    covered.insert(parse("trigger(0) conditions " + c + " actions end").triggers[0].conditions[0].kind);
  }
  CHECK(covered.size() == size_t(CondKind::IsEnemy) + 1);
  for (const auto& op : {"== 50", "!= 49", "< 51", "<= 50", "> 49", ">= 50"})
    CHECK(condition(r, w, std::string("side(local).money ") + op));
  CHECK(!condition(r, w, "side(8).isEnemy(1) == 1"));
  CHECK(!condition(r, w, "side(local).unitsInRect(4,5,3,4) > 0"));
  w.objects[0].last_hit_by = 5;
  CHECK(condition(r, w, "side(local).tactic(10).attacked == 5"));
  // An ordinary team tower with a colliding scenario ID is not the scripted defence.
  w.objects[3].defence = false; CHECK(condition(r, w, "side(local).defence(42).valid == 0"));
  w.objects[3].defence = true; w.objects[3].alive = false;
  CHECK(condition(r, w, "side(local).defence(42).valid == 0"));
  w.objects[0].removed = true; CHECK(condition(r, w, "side(local).units == 1"));
  w.objects[1].alive = false; CHECK(condition(r, w, "side(local).tactic(10).valid == 0"));
  w.objects[2].progress = 999; CHECK(condition(r, w, "side(local).towns == 0"));
}

static void paths_and_gifts() {
  for (const auto& method : {"setPath", "setInvertedPath", "setCycledPath", "setCycledInvertedPath"}) {
    World w = world(); object(w, 0, 2, 5, 3, 4);
    w.mission.paths = {{{6, 7}, {8, 9}}};
    GameSim sim(rules()); sim.init(w);
    const std::string m = method;
    auto r = runner(w, "trigger(0) conditions always actions side(local).tactic(5)." + m + "(0) trigger(0).disable end");
    const bool inverted = m.find("Inverted") != std::string::npos;
    const bool cycled = m.find("Cycled") != std::string::npos;
    r.tick(sim, w);
    CHECK(w.objects[0].dest_x == (inverted ? 8 : 6) * kTilePx + kTilePx / 2);
    auto first_hash = r.state_hash();
    r.tick(sim, w); CHECK(r.state_hash() != first_hash); // clock advances while path waits for idle
    CHECK(w.objects[0].dest_y == (inverted ? 9 : 7) * kTilePx + kTilePx / 2);
    w.objects[0].order = uint8_t(Order::Idle); r.tick(sim, w);
    CHECK(w.objects[0].dest_x == (inverted ? 6 : 8) * kTilePx + kTilePx / 2);
    w.objects[0].order = uint8_t(Order::Idle); r.tick(sim, w);
    CHECK(w.objects[0].order == uint8_t(cycled ? Order::AttackMove : Order::Idle));
    if (cycled) CHECK(w.objects[0].dest_x == (inverted ? 8 : 6) * kTilePx + kTilePx / 2);
  }
  World w = world(); object(w, 0, 2, 5, 3, 4); object(w, 0, 1, 5, 6, 7);
  GameSim sim(rules()); sim.init(w);
  auto r = runner(w,
      "trigger(0) conditions localTime == 1 actions side(local).tactic(5).moveTo(20,20) end "
      "trigger(1) conditions localTime == 2 actions side(local).tactic(5).giftToSide(1) end");
  r.tick(sim, w); sim.tick(w); r.tick(sim, w);
  CHECK(w.objects[0].side == 1 && w.objects[0].order == uint8_t(Order::Idle));
  const auto x = w.objects[0].cur_x, y = w.objects[0].cur_y;
  for (int i = 0; i < 10; ++i) { w.begin_tick(); sim.tick(w); }
  CHECK(w.objects[0].cur_x == x && w.objects[0].cur_y == y); // movement agent stopped too
  auto invalid = runner(w, "trigger(0) conditions always actions side(1).tactic(5).giftToSide(99) end");
  invalid.tick(sim, w); CHECK(w.objects[0].side == 1 && w.objects[1].side == 1);
}

static void actions_and_outcomes() {
  World w = world(); object(w, 0, 2, 5, 3, 4); object(w, 11, 1, -1, 10, 12);
  w.objects[1].scenario = 30;
  SideDef sd; sd.index = 2; sd.cameras.push_back({7, 8, 0}); w.mission.sides.push_back(sd);
  GameSim sim(rules()); sim.init(w);
  auto r = runner(w,
      "trigger(0) conditions always actions side(local).tactic(5).lock side(local).tactic(5).attackTown(30) "
      "side(local).setFriendlyTo(1) side(local).addMoney(50) side(local).takeMoney(100) "
      "centerView(5,6) side(local).cameraActivate(0,500) printString(\"Synthetic message\") printStringR(\"Alternate\") "
      "side(1).victory side(local).defeat side(local).victory trigger(0).disable end");
  r.tick(sim, w);
  CHECK(condition(r, w, "side(local).tactic(5).locked == 1"));
  CHECK(w.objects[0].dest_x == 10 * kTilePx + kTilePx / 2);
  CHECK(w.relation[2][1] == kFriend && w.relation[1][2] == kEnemy);
  CHECK(w.side_state[2].money == 0 && w.center_serial == 2);
  CHECK(w.center_x == 7 * kTilePx + kTilePx / 2 && w.center_y == 8 * kTilePx + kTilePx / 2);
  CHECK(w.messages.size() == 1 && w.messages[0].text == "Synthetic message");
  CHECK(w.outcome == kDefeat && w.side_outcome[2] == kDefeat && w.side_outcome[1] == kVictory);
  auto h = sim.state_hash(w); w.side_outcome[1] = kDefeat; CHECK(sim.state_hash(w) != h);
  h = sim.state_hash(w); w.objects[0].defence = true; CHECK(sim.state_hash(w) != h);
  auto next = runner(w,
      "trigger(0) conditions always actions side(local).tactic(5).unlock side(local).tactic(5).joinTactic(6) "
      "side(local).setNeutralTo(1) trigger(0).disable end "
      "trigger(1) conditions localTime == 2 actions side(local).setEnemyTo(1) side(local).tactic(6).kill end");
  next.tick(sim, w); CHECK(w.objects[0].tactic == 6 && w.relation[2][1] == kNeutral);
  CHECK(condition(next, w, "side(local).tactic(5).locked == 0"));
  next.tick(sim, w); CHECK(!w.objects[0].alive && w.relation[2][1] == kEnemy);
  CHECK(r.unsupported() == 0 && next.unsupported() == 0);
  auto unsupported = runner(w,
      "trigger(0) conditions always actions boat(1).activate johnka(1).enable johnka(1).disable submarine(1).go "
      "side(local).buildCommand(UGI_FOOTMAN,1,2) side(local).moveCommand(1,2) side(local).cameraDeactivate(0) end");
  const auto unsupported_script = parse(
      "trigger(0) conditions always actions boat(1).activate johnka(1).enable johnka(1).disable submarine(1).go "
      "side(local).buildCommand(UGI_FOOTMAN,1,2) side(local).moveCommand(1,2) side(local).cameraDeactivate(0) end");
  for (const auto& a : unsupported_script.triggers[0].actions)
    CHECK(!ScriptRunner::supports(a.kind));
  unsupported.tick(sim, w); CHECK(unsupported.unsupported() == 7);
  unsupported.init(w); CHECK(unsupported.unsupported() == 0);
  CHECK(!ScriptRunner::supports(static_cast<ActKind>(999)));
}

static void defence_loading() {
  TempDir directory;
  const auto root = directory.path();
  std::filesystem::create_directory(root / "Res");
  std::filesystem::create_directory(root / "Maps");
  for (const char* name : {"Pal", "Units", "Tiles0", "Tiles1", "common", "Fnts", "Light"}) {
    std::ofstream f(root / "Res" / (std::string(name) + ".xbe"), std::ios::binary);
    const char empty[4]{}; f.write(empty, 4);
  }
  {
    std::ofstream f(root / "Maps" / "synthetic.mp");
    f << "[Globals]\nSides=1\nTowns=1\nTeams=1\n"
         "[Side0]\nDefenceCount=1\nDefence0=42,2,2,17\n"
         "[Town0]\nSide=0\nX=8\nY=8\nType=1\nUnits=0\n"
         "[Team0]\nSide=0\nScenarioID=42\nUnits=1\nUnit0=5,5,17,100\n";
  }
  {
    std::ofstream f(root / "Maps" / "synthetic.map", std::ios::binary);
    const char header[4]{16, 0, 16, 0}; f.write(header, 4);
    const char cells[16 * 16 * 4]{}; f.write(cells, sizeof cells);
  }
  GameData data; std::string error; World w;
  CHECK(data.open(root.string(), error)); CHECK(load_world(data, "synthetic", w, error));
  CHECK(w.objects.size() == 3);
  CHECK(w.objects[0].defence && w.objects[0].town == -1 && w.objects[0].scenario == 42);
  CHECK(!w.objects[1].defence && w.objects[1].tactic == 42);
  auto r = runner(w, ""); CHECK(condition(r, w, "side(0).defence(42).valid == 1"));
  w.objects[0].removed = true; CHECK(condition(r, w, "side(0).defence(42).valid == 0"));
}

static void replay() {
  TempDir dir; const auto path = dir.path() / "synthetic.scn";
  { std::ofstream f(path); f <<
      "trigger(0) conditions localTime == 1 actions timer(3).set(0) side(local).tactic(5).setCycledPath(0) end\n"
      "trigger(1) conditions timer(3) == 10 actions trigger(2).enable timer(3).kill end\n"
      "trigger(2) conditions always actions side(local).tactic(5).giftToSide(1) trigger(2).disable side(1).setFriendlyTo(local) end\n"
      "trigger(3) conditions localTime == 200 actions side(1).victory end\n"; }
  World initial = world(); object(initial, 0, 2, 5, 3, 4);
  initial.mission.paths = {{{6, 7}, {8, 9}}};
  World a = initial, b = initial; b.local_side = 7;
  auto data = rules(); GameSim first(data), second(data);
  CHECK(first.attach_script(path.string()) && second.attach_script(path.string())); first.init(a); second.init(b);
  for (int tick = 0; tick < 240; ++tick) {
    if (tick == 30) {
      Command c; c.type = Pct::Move; c.side = 1; c.ids = {1}; c.a = 12 * kTilePx; c.b = 10 * kTilePx;
      first.submit(c); second.submit(c);
    }
    a.begin_tick(); b.begin_tick(); first.tick(a); second.tick(b);
    CHECK(first.state_hash(a) == second.state_hash(b));
    if (tick == 100) {
      World restored = initial;
      auto continued = GameSim::replay(restored, data, first.log(), first.tick_count(), path.string());
      CHECK(continued->state_hash(restored) == first.state_hash(a));
      for (int step = 0; step < 30; ++step) { restored.begin_tick(); continued->tick(restored); }
      World uninterrupted = initial;
      auto control = GameSim::replay(uninterrupted, data, first.log(), first.tick_count() + 30, path.string());
      CHECK(continued->state_hash(restored) == control->state_hash(uninterrupted));
    }
  }
  CHECK(a.side_outcome[1] == kVictory && a.outcome == kRunning && first.script().unsupported() == 0);
}

int main() {
  order_and_enablement(); timers(); conditions_and_identity(); paths_and_gifts(); actions_and_outcomes(); defence_loading(); replay();
  std::puts("Synthetic script contracts passed.");
}
