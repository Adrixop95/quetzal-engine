// Synthetic script contracts. These establish engine behavior, not reference parity.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <limits>
#include <set>
#include <string>

#include "core/script.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/sim/sight.hpp"
#include "engine/ui/hud.hpp"
#include "engine/world/camera.hpp"
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

static void source_locations() {
  const auto s = parse("\r\ntrigger(5)\r\n  conditions\r\n\r\n    always\r\n  actions\r\n\r\n    side(local).victory\r\nend\r\n");
  CHECK(s.triggers[0].conditions[0].line == 5);
  CHECK(s.triggers[0].actions[0].line == 8);
}

static void attacked_notification() {
  // Invented projectile impacts exercise notifications independently of weapons.
  // V: attacked returns a side and belongs to the tactic, not the damaged member.
  for (int source_side : {0, 1, 2}) for (int source_kind : {0, 17, -1})
      for (auto relation : {kEnemy, kNeutral, kFriend}) {
    World w = world();
    object(w, source_kind == 17 ? 17 : 0, source_side, source_kind < 0 ? -1 : 20, 25, 25);
    object(w, 0, 2, 10, 3, 3);
    object(w, 0, 2, 10, 5, 3);
    object(w, 0, 0, 10, 7, 3); // same number on another side is independent
    if (source_side != 2) w.relation[2][source_side] = relation;
    for (auto& o : w.objects) o.attack_mode = AttackMode::Direct;
    GameSim sim(rules(), Modifications::faithful()); sim.init(w);
    auto r = runner(w, "trigger(1) conditions side(local).tactic(10).attacked == 1 "
        "actions side(local).addMoney(7) trigger(1).disable end");
    CHECK(condition(r, w, "side(local).tactic(10).attacked == -1"));
    auto hit = [&](int damage) {
      Effect e; e.kind = data::EffectClass::Arrow; e.flight = true;
      e.source = 1; e.target = 2; e.side = source_side; e.damage = damage;
      e.x = e.end_x = w.objects[1].cur_x; e.y = e.end_y = w.objects[1].cur_y;
      w.effects.push_back(e); w.begin_tick(); sim.tick(w);
    };
    hit(0);
    CHECK(w.tactic_attacked_by.empty());
    hit(1);
    CHECK(w.objects[1].last_hit_by == 1);
    const bool notified = source_side != 2 && source_kind == 0 && relation != kFriend;
    const int expected = notified ? source_side : -1;
    CHECK(condition(r, w, "side(local).tactic(10).attacked == " + std::to_string(expected)));
    CHECK(condition(r, w, "side(0).tactic(10).attacked == -1"));
    if (notified) {
      const auto hash = sim.state_hash(w);
      w.tactic_attacked_by[{2, 10}] = 6;
      CHECK(sim.state_hash(w) != hash);
      w.tactic_attacked_by[{2, 10}] = source_side;
    }
    // Keep the notification after the hit member dies and the attacker leaves.
    hit(1000);
    CHECK(!w.objects[1].alive && w.objects[2].alive);
    w.objects[0].removed = true;
    CHECK(condition(r, w, "side(local).tactic(10).attacked == " + std::to_string(expected)));
    r.tick(sim, w); r.tick(sim, w);
    CHECK(w.side_state[2].money == (notified && source_side == 1 ? 7 : 0));
  }

  // Direct weapon damage produces the same notification and replays it.
  World initial = world();
  object(initial, 0, 1, 20, 3, 3);
  object(initial, 0, 2, 10, 4, 3);
  // Both units survive the newest-first attack exchange.
  initial.objects[0].hp = initial.objects[1].hp = 2000;
  auto w = initial;
  auto definitions = rules();
  GameSim sim(definitions, Modifications::faithful()); sim.init(w);
  Command attack; attack.side = 1; attack.type = Pct::Attack; attack.ids = {1}; attack.a = 2;
  sim.submit(attack); w.begin_tick(); sim.tick(w);
  CHECK(w.objects[1].last_hit_by == 1);
  CHECK(w.tactic_attacked_by.at({2, 10}) == 1);
  auto replay_world = initial;
  auto replay = GameSim::replay(replay_world, definitions, sim.log(), sim.tick_count());
  CHECK(replay->state_hash(replay_world) == sim.state_hash(w));
}

static void order_and_enablement() {
  World w = world(); GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w,
      "trigger(9) conditions always actions side(local).addMoney(1) trigger(9).disable end "
      "trigger(2) conditions always actions trigger(9).enable trigger(7).enable trigger(3).disable trigger(2).disable side(local).addMoney(10) end "
      "trigger(7) conditions always actions side(local).addMoney(100) trigger(7).disable end "
      "trigger(3) conditions always actions side(local).addMoney(1000) end "
      "trigger(4) conditions always actions side(local).addMoney(10000) end");
  const auto fresh = r.state_hash();
  r.tick(sim, w); CHECK(w.side_state[2].money == 0); // initial latch is false
  r.tick(sim, w); CHECK(w.side_state[2].money == 10111);
  r.tick(sim, w); CHECK(w.side_state[2].money == 20112);
  r.tick(sim, w); CHECK(w.side_state[2].money == 30112); // repeats while latched
  CHECK(w.side_state[0].money == 0 && r.unsupported() == 0);
  r.init(w); CHECK(r.state_hash() == fresh);
  auto duplicates = runner(w,
      "trigger(5) conditions always actions side(0).addMoney(1) trigger(5).disable end "
      "trigger(5) conditions always actions side(0).addMoney(100) end");
  duplicates.tick(sim, w); CHECK(w.side_state[0].money == 0);
  duplicates.tick(sim, w); CHECK(w.side_state[0].money == 101);
  duplicates.tick(sim, w); CHECK(w.side_state[0].money == 201);
  const auto rows = duplicates.action_observations(w);
  CHECK(!rows.front().enabled && rows.back().enabled); // only first ID addressed

  // A false current condition cannot cancel yesterday's latch. Disabled triggers
  // retain it; enabling a later trigger can execute it in the same update pass.
  w.side_state[2].money = 1;
  auto stale = runner(w,
      "trigger(1) conditions always actions trigger(2).enable end "
      "trigger(2) conditions side(local).money > 0 actions side(0).addMoney(1) trigger(2).disable end");
  stale.tick(sim, w);
  w.side_state[2].money = 0;
  stale.tick(sim, w); CHECK(w.side_state[0].money == 202);
  stale.tick(sim, w); CHECK(w.side_state[0].money == 202);
  auto empty = runner(w, "trigger(0) conditions actions side(0).addMoney(1000) end");
  empty.tick(sim, w); empty.tick(sim, w); CHECK(w.side_state[0].money == 202);
  auto and_list = runner(w,
      "trigger(0) conditions always side(local).money > 0 actions side(0).addMoney(1000) end");
  and_list.tick(sim, w); and_list.tick(sim, w); CHECK(w.side_state[0].money == 202);
  w.side_state[2].money = 1;
  auto frozen = runner(w,
      "trigger(0) conditions always actions trigger(1).disable trigger(0).disable end "
      "trigger(1) conditions side(local).money > 0 actions side(0).addMoney(3) trigger(1).disable end "
      "trigger(2) conditions globalTime == 2 actions trigger(1).enable end");
  frozen.tick(sim, w); frozen.tick(sim, w);
  w.side_state[2].money = 0;
  frozen.tick(sim, w); frozen.tick(sim, w); CHECK(w.side_state[0].money == 202);
  frozen.tick(sim, w); CHECK(w.side_state[0].money == 205); // backward enable preserves stale true

}

static void timers() {
  World w = world(); GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w,
      "trigger(0) conditions timer(7) == 0 actions side(local).addMoney(1) trigger(0).disable end");
  const auto before = r.state_hash();
  r.action_observations(w); CHECK(r.state_hash() == before); // observation never creates timers
  r.tick(sim, w); CHECK(condition(r, w, "timer(7) == 0"));
  CHECK(w.side_state[2].money == 0);
  r.tick(sim, w); CHECK(condition(r, w, "timer(7) == 1"));
  CHECK(w.side_state[2].money == 1);

  auto lifecycle = runner(w,
      "trigger(0) conditions always actions timer(2).stop timer(2).set(20) trigger(0).disable end "
      "trigger(1) conditions globalTime == 2 actions timer(2).start trigger(1).disable end "
      "trigger(2) conditions globalTime == 4 actions timer(2).kill trigger(2).disable end "
      "trigger(3) conditions timer(2) == 0 actions side(0).addMoney(1) end");
  lifecycle.tick(sim, w); CHECK(condition(lifecycle, w, "timer(2) == 0"));
  lifecycle.tick(sim, w); CHECK(condition(lifecycle, w, "timer(2) == 20"));
  CHECK(w.side_state[0].money == 1); // acts on the old latch after set
  lifecycle.tick(sim, w); CHECK(condition(lifecycle, w, "timer(2) == 20"));
  lifecycle.tick(sim, w); CHECK(condition(lifecycle, w, "timer(2) == 20"));
  lifecycle.tick(sim, w); CHECK(condition(lifecycle, w, "timer(2) == 21"));
  lifecycle.tick(sim, w); CHECK(condition(lifecycle, w, "timer(2) == 0")); // kill + condition recreates
  lifecycle.tick(sim, w); CHECK(condition(lifecycle, w, "timer(2) == 1"));
  CHECK(w.side_state[0].money == 2);

  auto skipped = runner(w,
      "trigger(0) conditions side(local).money > 10 timer(9) == 0 actions side(0).addMoney(2) end");
  skipped.tick(sim, w); skipped.tick(sim, w);
  w.side_state[2].money = 11;
  skipped.tick(sim, w); CHECK(condition(skipped, w, "timer(9) == 0"));
  skipped.tick(sim, w); CHECK(condition(skipped, w, "timer(9) == 1"));
  CHECK(w.side_state[0].money == 4); // skipped timer was not created on earlier ticks

  auto overflow = runner(w,
      "trigger(0) conditions always actions timer(3).set(2147483647) trigger(0).disable end");
  overflow.tick(sim, w); overflow.tick(sim, w); overflow.tick(sim, w);
  CHECK(condition(overflow, w, "timer(3) == -2147483648"));
  CHECK(condition(overflow, w, "timer(3) < 0"));
  w.mission.start_time = 7198;
  auto clocks = runner(w, "");
  CHECK(condition(clocks, w, "localTime == 7198"));
  clocks.tick(sim, w); CHECK(condition(clocks, w, "localTime == 7199"));
  clocks.tick(sim, w); CHECK(condition(clocks, w, "localTime == 0"));
  CHECK(condition(clocks, w, "globalTime == 2"));
}

static void conditions_and_identity() {
  World w = world(); object(w, 0, 2, 10, 3, 4); object(w, 27, 2, 10, 6, 7);
  object(w, 11, 2, -1, 12, 12); object(w, 17, 2, -1, 15, 15);
  object(w, 0, 1, 10, 20, 20); object(w, 11, 1, -1, 25, 25);
  w.objects[3].scenario = 42; w.objects[3].defence = true;
  w.mission.towns = {{0, 2, 12, 12, 1, 0, 50}, {1, 1, 25, 25, 1, 0, 51}};
  w.objects[2].town = 0; w.objects[2].town_hall = true;
  w.objects[5].town = 1; w.objects[5].town_hall = true;
  w.side_state[2].money = 50;
  GameSim sim(rules(), Modifications::faithful()); sim.init(w); auto r = runner(w, "");
  const std::string checks[] = {
    "always", "localTime == 0", "globalTime == 0", "timer(0) == 0",
    "side(local).units == 2", "side(local).towns == 1", "side(local).enemies == 1",
    "side(local).tactics == 1", "side(local).money == 50", "side(local).unitsInRect(3,4,7,8) == 2",
    "side(local).unitsOfUGIInRect(UGI_FOOTMAN,3,4,4,5) == 1", "side(local).unitsOfId(UGI_FOOTMAN) == 1",
    "side(local).tactic(10).valid == 1", "side(local).tactic(10).idle == 1",
    "side(local).tactic(10).locked == 1", "side(local).tactic(10).attacked == -1",
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
  CHECK(condition(r, w, "side(local).unitsInRect(4,5,3,4) == 1"));
  CHECK(condition(r, w, "side(local).unitsInRect(15,15,16,16) == 1")); // defence kind counts
  w.objects[0].last_hit_by = 5;
  const auto quiet_hash = sim.state_hash(w);
  CHECK(condition(r, w, "side(local).tactic(10).attacked == -1"));
  r.notify_attack(w, w.objects[0], w.objects[4].side);
  CHECK(condition(r, w, "side(local).tactic(10).attacked == 1"));
  CHECK(!condition(r, w, "side(local).tactic(10).attacked == 5"));
  const auto attacked_hash = sim.state_hash(w);
  CHECK(attacked_hash != quiet_hash);
  r.notify_attack(w, w.objects[1], 2); // friendly damage leaves notification unchanged
  CHECK(sim.state_hash(w) == attacked_hash);
  w.objects[0].alive = false; w.objects[4].alive = false;
  CHECK(condition(r, w, "side(local).tactic(10).attacked == 1"));
  w.objects[0].alive = true; w.objects[4].alive = true;
  CHECK(!condition(r, w, "side(local).tactic(99).idle == 1"));
  CHECK(!condition(r, w, "side(local).tactic(99).idle == 0"));
  CHECK(!condition(r, w, "side(local).tactic(99).attacked == -1"));
  r.notify_attack(w, w.objects[1], 0);
  CHECK(condition(r, w, "side(local).tactic(10).attacked == 0"));
  // An ordinary team tower with a colliding scenario ID is not the scripted defence.
  w.objects[3].defence = false; CHECK(condition(r, w, "side(local).defence(42).valid == 0"));
  w.objects[3].defence = true; w.objects[3].alive = false;
  CHECK(condition(r, w, "side(local).defence(42).valid == 0"));
  w.objects[0].removed = true; CHECK(condition(r, w, "side(local).units == 1"));
  w.objects[1].alive = false; CHECK(condition(r, w, "side(local).tactic(10).valid == 0"));
  w.objects[2].progress = 999; CHECK(condition(r, w, "side(local).towns == 1"));
}

static void rectangle_runtime_categories() {
  World w = world();
  // Each construction family has an independent runtime category; all these
  // ordinary objects are included, including structures and both vessel types.
  for (int type : {0, 30, 11, 7, 83, 33, 3, 17, 84, 87, 88, 89})
    object(w, type, 2, -1, 5, 6);
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "");
  CHECK(condition(r, w, "side(local).unitsInRect(5,6,6,7) == 11"));
  CHECK(condition(r, w, "side(local).unitsOfUGIInRect(UGI_BOAT,5,6,6,7) == 1"));
  CHECK(condition(r, w, "side(local).unitsOfUGIInRect(UGI_JOHNKA,5,6,6,7) == 1"));
  CHECK(condition(r, w, "side(local).unitsOfUGIInRect(UGI_CAMERA,5,6,6,7) == 0"));
  w.objects[8].alive = false;
  CHECK(condition(r, w, "side(local).unitsOfUGIInRect(UGI_BOAT,5,6,6,7) == 0"));
}

static void rectangle_box_edges() {
  World w = world(); object(w, 0, 2, 5, 4, 6);
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "trigger(1) conditions side(local).unitsInRect(5,6,6,7) == 1 "
      "actions side(local).addMoney(1) end");
  const auto bounds = sim.rules().type(0).combat_bounds;
  auto place = [&](int x, int y) { w.objects[0].teleport(to_fixed(x), to_fixed(y)); };
  auto inside = [&] { return condition(r, w, "side(local).unitsInRect(5,6,6,7) == 1"); };
  place(5 * 48 - bounds[2], 6 * 48 + 24);
  CHECK(!inside()); // right edge merely touches the query's left edge
  place(5 * 48 - bounds[2] + 1, 6 * 48 + 24);
  CHECK(inside()); // anchor is outside, but the box overlaps
  CHECK(condition(r, w, "side(local).unitsOfUGIInRect(UGI_FOOTMAN,6,7,5,6) == 1"));
  CHECK(condition(r, w, "side(local).unitsInRect(5,6,5,7) == 0"));
  CHECK(condition(r, w, "side(local).unitsInRect(5,6,6,6) == 0"));
  CHECK(condition(r, w, "side(local).unitsInRect(-2147483648,-2147483648,2147483647,2147483647) == 1"));
  r.tick(sim, w); CHECK(w.side_state[2].money == 0);
  place(6 * 48 - bounds[0], 6 * 48 + 24);
  CHECK(!inside());
  r.tick(sim, w); CHECK(w.side_state[2].money == 1); // previous overlapping box was latched
  r.tick(sim, w); CHECK(w.side_state[2].money == 1);
  place(6 * 48 - bounds[0] - 1, 6 * 48 + 24); CHECK(inside());
  place(5 * 48 + 24, 6 * 48 - bounds[3]); CHECK(!inside());
  place(5 * 48 + 24, 6 * 48 - bounds[3] + 1); CHECK(inside());
  place(5 * 48 + 24, 7 * 48 - bounds[1]); CHECK(!inside());
  place(5 * 48 + 24, 7 * 48 - bounds[1] - 1); CHECK(inside());
  w.objects[0].render_height = 48; CHECK(inside());
  w.objects[0].removed = true; CHECK(!inside());
}

static void side_command_contracts() {
  const std::string command = "side(local).buildCommand(7) {UGI_FOOTMAN,UGI_TRUCK_AXETHROWER,UGI_FOOTMAN}";
  const auto s = parse("trigger(0) conditions always actions " + command + " end");
  const auto& a = s.triggers[0].actions[0];
  CHECK(a.kind == ActKind::SideBuildCommand && a.id == 7 && a.ugi == -1);
  CHECK(a.args == std::vector<int>({0, 3, 0})); // preserve duplicate requests and order
  CHECK(!ScriptRunner::supports(a.kind)); // no general side-plan model yet
  Script invalid;
  for (const auto& bad : {"buildCommand(7) {}", "buildCommand(7) {UGI_UNKNOWN}",
                         "buildCommand(7) {UGI_FOOTMAN,}", "buildCommand(7) {UGI_FOOTMAN"})
    CHECK(!parse_script("trigger(0) conditions always actions side(local)." + std::string(bad) + " end", invalid));
  std::string list = "UGI_FOOTMAN";
  for (int i = 1; i < 32; ++i) list += ",UGI_FOOTMAN";
  parse("trigger(0) conditions always actions side(local).buildCommand(7) {" + list + "} end");
  CHECK(!parse_script("trigger(0) conditions always actions side(local).buildCommand(7) {" + list + ",UGI_FOOTMAN} end", invalid));

  World w = world(); object(w, 0, 2, 5, 4, 6);
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "trigger(0) conditions always actions side(local).moveCommand(30,30) "
      "side(local).addMoney(2) end");
  CHECK(ScriptRunner::supports(ActKind::SideMoveCommand));
  const auto x = w.objects[0].cur_x, y = w.objects[0].cur_y;
  for (int i = 0; i < 4; ++i) r.tick(sim, w);
  CHECK(r.unsupported() == 0 && w.side_state[2].money == 6);
  CHECK(w.objects[0].cur_x == x && w.objects[0].cur_y == y);
  CHECK(w.objects[0].order == uint8_t(Order::Idle) && w.objects[0].tactic == 5);
  CHECK(r.action_observations(w)[0].trace.executions == 3);
}

static void paths_and_gifts() {
  for (const auto& method : {"setPath", "setInvertedPath", "setCycledPath", "setCycledInvertedPath"}) {
    World w = world(); object(w, 0, 2, 5, 3, 4);
    w.mission.paths = {{{6, 7}, {8, 9}}};
    GameSim sim(rules(), Modifications::faithful()); sim.init(w);
    const std::string m = method;
    auto r = runner(w, "trigger(0) conditions always actions side(local).tactic(5)." + m + "(0) trigger(0).disable end");
    const bool inverted = m.find("Inverted") != std::string::npos;
    const bool cycled = m.find("Cycled") != std::string::npos;
    r.tick(sim, w); r.tick(sim, w);
    CHECK(w.objects[0].dest_x == (inverted ? 8 : 6) * kTilePx + kTilePx / 2);
    auto first_hash = r.state_hash();
    r.tick(sim, w); CHECK(r.state_hash() != first_hash); // clock advances while path waits for idle
    CHECK(w.objects[0].dest_y == (inverted ? 9 : 7) * kTilePx + kTilePx / 2);
    auto& anchor = w.unit_tactics.at({2, 5});
    anchor.x = inverted ? 8 : 6; anchor.y = inverted ? 9 : 7;
    sim.script_stop(w, {1}); r.tick(sim, w);
    CHECK(w.objects[0].dest_x == (inverted ? 6 : 8) * kTilePx + kTilePx / 2);
    anchor.x = inverted ? 6 : 8; anchor.y = inverted ? 7 : 9;
    sim.script_stop(w, {1}); r.tick(sim, w);
    CHECK(w.objects[0].order == uint8_t(cycled ? Order::AttackMove : Order::Idle));
    if (cycled) CHECK(w.objects[0].dest_x == (inverted ? 8 : 6) * kTilePx + kTilePx / 2);
  }
  World w = world(); object(w, 0, 2, 5, 3, 4); object(w, 0, 1, 5, 6, 7);
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w,
      "trigger(0) conditions localTime == 1 actions side(local).tactic(5).moveTo(20,20) end "
      "trigger(1) conditions localTime == 2 actions side(local).tactic(5).giftToSide(1) end");
  r.tick(sim, w); sim.tick(w); r.tick(sim, w); r.tick(sim, w);
  CHECK(w.objects[0].side == 1 && w.objects[0].order == uint8_t(Order::Idle));
  CHECK(w.objects[0].tactic != w.objects[1].tactic); // gift never merges a colliding group
  CHECK(condition(r, w, "side(1).tactics == 2"));
  const auto x = w.objects[0].cur_x, y = w.objects[0].cur_y;
  for (int i = 0; i < 10; ++i) { w.begin_tick(); sim.tick(w); }
  CHECK(w.objects[0].cur_x == x && w.objects[0].cur_y == y); // movement agent stopped too
  auto invalid = runner(w, "trigger(0) conditions always actions side(1).tactic(5).giftToSide(99) end");
  invalid.tick(sim, w); invalid.tick(sim, w); CHECK(w.objects[0].side == 1 && w.objects[1].side == 1);
}

static void tactic_lookup_and_money() {
  World w = world(); object(w, 0, 2, 5, 3, 4); object(w, 0, 2, 6, 7, 8);
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w,
      "trigger(0) conditions always actions side(local).tactic(5).joinTactic(99) trigger(0).disable end "
      "trigger(1) conditions globalTime == 1 actions side(local).tactic(5).joinTactic(6) trigger(1).disable end "
      "trigger(2) conditions globalTime == 2 actions side(local).tactic(6).kill trigger(2).disable end "
      "trigger(3) conditions always actions side(local).addMoney(-10) trigger(3).disable end");
  r.tick(sim, w); r.tick(sim, w); CHECK(w.objects[0].tactic == 5);
  CHECK(w.side_state[2].money == 0);
  r.tick(sim, w); CHECK(w.objects[0].tactic == 6 && w.objects[0].alive);
  r.tick(sim, w); CHECK(!w.objects[0].alive && !w.objects[1].alive);
  CHECK(w.tactic_attacked_by.empty()); // scripted kill has no attacker notification
  CHECK(int(Cmp::Eq) == 0 && int(Cmp::Gt) == 1 && int(Cmp::Ge) == 2);
  CHECK(int(Cmp::Ne) == 3 && int(Cmp::Lt) == -1 && int(Cmp::Le) == -2);
}

static void gift_detaches_home_and_outcome_precedence() {
  World w = world();
  w.mission.towns = {{0, 2, 15, 15, 1, 0, 52}};
  object(w, 0, 2, 7, 3, 4); w.objects[0].town = 0;
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  w.side_outcome[2] = kVictory;
  auto r = runner(w,
      "trigger(0) conditions always actions side(local).tactic(7).giftToSide(1) "
      "side(local).defeat trigger(0).disable end");
  r.tick(sim, w); r.tick(sim, w);
  CHECK(w.objects[0].side == 1 && w.objects[0].town == -1);
  CHECK(w.mission.towns[0].side == 2);
  CHECK(w.side_outcome[2] == kVictory && w.outcome == kRunning);
  CHECK(!r.outcome_origin()); // The attempted defeat cannot replace an existing victory.
  w.begin_tick(); sim.tick(w);
  CHECK(w.outcome == kVictory);
  auto money = runner(w,
      "trigger(0) conditions always actions side(0).addMoney(2147483647) "
      "side(0).addMoney(1) trigger(0).disable end");
  money.tick(sim, w); money.tick(sim, w); CHECK(w.side_state[0].money == 0);
}

static void actions_and_outcomes() {
  World w = world(); object(w, 0, 2, 5, 3, 4); object(w, 11, 1, -1, 10, 12);
  w.objects[1].scenario = 30; w.objects[1].town = 4; w.objects[1].town_hall = true;
  w.mission.towns.push_back({4, 1, 10, 12, 11, 0, 30});
  SideDef sd; sd.index = 2; sd.cameras.push_back({7, 8, 0}); w.mission.sides.push_back(sd);
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w,
      "trigger(0) conditions always actions side(local).tactic(5).lock side(local).tactic(5).attackTown(30) "
      "side(local).setFriendlyTo(1) side(local).addMoney(50) side(local).takeMoney(100) "
      "centerView(5,6) side(local).cameraActivate(0,500) printString(\"Synthetic message\") printStringR(\"Alternate\") "
      "side(1).victory side(local).defeat side(local).victory trigger(0).disable end");
  r.tick(sim, w); r.tick(sim, w);
  CHECK(condition(r, w, "side(local).tactic(5).locked == 1"));
  CHECK(w.objects[0].dest_x == 10 * kTilePx + kTilePx / 2);
  CHECK(w.relation[2][1] == kFriend && w.relation[1][2] == kEnemy);
  CHECK(w.side_state[2].money == 0 && w.center_serial == 1);
  CHECK(w.center_x == 5 * kTilePx && w.center_y == 6 * kTilePx);
  CHECK(w.messages.size() == 1 && w.messages[0].text == "Synthetic message");
  CHECK(w.outcome == kRunning && w.side_outcome[2] == kDefeat && w.side_outcome[1] == kVictory);
  CHECK(r.outcome_origin() && r.outcome_origin()->trigger == 0 && r.outcome_origin()->tick == w.tick);
  r.tick(sim, w); CHECK(r.outcome_origin()->tick == w.tick);
  r.init(w); CHECK(!r.outcome_origin());
  auto h = sim.state_hash(w); w.side_outcome[1] = kDefeat; CHECK(sim.state_hash(w) != h);
  h = sim.state_hash(w); w.objects[0].defence = true; CHECK(sim.state_hash(w) != h);
  auto next = runner(w,
      "trigger(0) conditions always actions side(local).tactic(5).unlock "
      "side(local).tactic(5).lock side(local).tactic(5).kill side(local).tactic(5).giftToSide(1) "
      "side(local).setNeutralTo(1) trigger(0).disable end "
      "trigger(1) conditions globalTime == 1 actions side(local).setEnemyTo(1) end");
  next.tick(sim, w); next.tick(sim, w);
  CHECK(w.objects[0].tactic == 5 && w.objects[0].alive && w.objects[0].side == 2);
  CHECK(w.relation[2][1] == kNeutral);
  CHECK(condition(next, w, "side(local).tactic(5).valid == 0"));
  CHECK(!condition(next, w, "side(local).tactic(5).locked == 0"));
  next.tick(sim, w); CHECK(w.objects[0].alive && w.relation[2][1] == kEnemy);
  CHECK(r.unsupported() == 0 && next.unsupported() == 0);
  auto unsupported = runner(w,
      "trigger(0) conditions always actions boat(1).activate johnka(1).enable johnka(1).disable submarine(1).go "
      "side(local).buildCommand(UGI_FOOTMAN,1,2) side(local).moveCommand(1,2) side(local).cameraDeactivate(0) end");
  const auto unsupported_script = parse(
      "trigger(0) conditions always actions boat(1).activate johnka(1).enable johnka(1).disable submarine(1).go "
      "side(local).buildCommand(UGI_FOOTMAN,1,2) side(local).moveCommand(1,2) side(local).cameraDeactivate(0) end");
  for (const auto& a : unsupported_script.triggers[0].actions)
    CHECK(ScriptRunner::supports(a.kind) == (a.kind == ActKind::SideMoveCommand || a.kind == ActKind::BoatActivate || a.kind == ActKind::JohnkaEnable || a.kind == ActKind::JohnkaDisable || a.kind == ActKind::SubmarineGo || a.kind == ActKind::SideCameraDeactivate));
  unsupported.tick(sim, w); unsupported.tick(sim, w); CHECK(unsupported.unsupported() == 1);
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

static void action_observation_contracts() {
  // Each unresolved grammar branch must remain visible both before and after dispatch.
  for (const auto& method : {"side(local).buildCommand(7) {UGI_FOOTMAN}", "side(local).buildCommand(UGI_FOOTMAN,1,2)"}) {
    World w = world(); GameSim sim(rules(), Modifications::faithful()); sim.init(w);
    auto r = runner(w,
        "trigger(7) conditions localTime == 2 actions " + std::string(method) + " trigger(7).disable end\n"
        "trigger(8) conditions localTime > 10 actions " + method + " end\n"
        "trigger(9) conditions always actions trigger(8).enable trigger(9).disable end\n");
    auto observations = r.action_observations(w);
    CHECK(observations.size() == 5);
    CHECK(observations[2].enabled && observations[2].trace.executions == 0);
    r.tick(sim, w); observations = r.action_observations(w);
    CHECK(observations[2].enabled && observations[2].trace.executions == 0);
    CHECK(!observations[0].false_condition_lines.empty());
    r.tick(sim, w);
    CHECK(r.unsupported() == 0);
    r.tick(sim, w); observations = r.action_observations(w);
    CHECK(r.unsupported() == 1);
    const auto& a = observations[0];
    CHECK(a.trigger_index == 0 && a.action_index == 0 && a.trigger == 7 && a.line == 1);
    CHECK(!a.enabled && !ScriptRunner::supports(a.kind));
    CHECK(a.trace.executions == 1 && a.trace.first_tick == 3 && a.trace.last_tick == 3);
    CHECK(a.false_condition_lines.size() == 1); // action uses the previous tick's true latch
    CHECK(observations[1].trace.executions == 1); // self-disable completes the action list
    r.tick(sim, w); observations = r.action_observations(w);
    CHECK(observations[0].false_condition_lines.size() == 1);
    const auto h = r.state_hash(); r.action_observations(w); CHECK(h == r.state_hash());
    r.init(w); observations = r.action_observations(w);
    for (const auto& row : observations)
      CHECK(row.trace.executions == 0 && row.trace.first_tick == 0 && row.trace.last_tick == 0);
  }
  // Repeated IDs have independent latches and enablement; only the first is addressed.
  World w = world(); GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w,
      "trigger(3) conditions always actions trigger(3).disable end "
      "trigger(3) conditions always actions boat(1).activate end");
  r.tick(sim, w); r.tick(sim, w);
  auto rows = r.action_observations(w);
  CHECK(rows[0].trace.executions == 1 && rows[1].trace.executions == 1);
  CHECK(rows[0].trigger_index != rows[1].trigger_index && rows[1].enabled && r.unsupported() == 0);
}

static void replay() {
  TempDir dir; const auto path = dir.path() / "synthetic.scn";
  { std::ofstream f(path); f <<
      "trigger(0) conditions localTime == 1 actions timer(3).set(0) side(local).cameraActivate(0,180) printString(\"Replay notice\") side(local).tactic(5).setCycledPath(0) end\n"
      "trigger(1) conditions timer(3) == 10 actions trigger(2).enable timer(3).kill end\n"
      "trigger(2) conditions always actions side(local).tactic(5).giftToSide(1) trigger(2).disable side(1).setFriendlyTo(local) end\n"
      "trigger(3) conditions localTime == 200 actions side(1).victory end\n"; }
  World initial = world(); object(initial, 0, 2, 5, 3, 4);
  initial.mission.paths = {{{6, 7}, {8, 9}}};
  SideDef camera_side; camera_side.index = 2; camera_side.cameras.push_back({12, 13, 6});
  initial.mission.sides.push_back(camera_side);
  World a = initial, b = initial; b.local_side = 7;
  auto data = rules(); GameSim first(data, Modifications::faithful()), second(data, Modifications::faithful());
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
      const auto original = first.script().action_observations(a);
      const auto rebuilt = continued->script().action_observations(restored);
      CHECK(original.size() == rebuilt.size());
      for (size_t i = 0; i < original.size(); ++i) {
        CHECK(original[i].trace.executions == rebuilt[i].trace.executions);
        CHECK(original[i].trace.first_tick == rebuilt[i].trace.first_tick);
        CHECK(original[i].trace.last_tick == rebuilt[i].trace.last_tick);
        CHECK(original[i].enabled == rebuilt[i].enabled);
        CHECK(original[i].false_condition_lines == rebuilt[i].false_condition_lines);
      }
      for (int step = 0; step < 30; ++step) { restored.begin_tick(); continued->tick(restored); }
      World uninterrupted = initial;
      auto control = GameSim::replay(uninterrupted, data, first.log(), first.tick_count() + 30, path.string());
      CHECK(continued->state_hash(restored) == control->state_hash(uninterrupted));
    }
  }
  CHECK(a.side_outcome[1] == kVictory && a.outcome == kRunning && first.script().unsupported() == 0);
}

static void presentation_contracts() {
  World w = world();
  SideDef side; side.index = 2; side.cameras = {{12, 13, 6}, {4, 5, -100}, {6, 7, 100}};
  w.mission.sides.push_back(side);
  auto data = rules();
  auto sight = std::make_shared<SightData>();
  for (auto& day : sight->daylight) day.fill(80);
  for (int id : {64, 65, 71}) {
    auto& mask = sight->masks[id]; mask.assign(404, 152);
    mask[0] = mask[2] = 20; mask[1] = mask[3] = 0;
    for (int y : {9, 10}) for (int x : {9, 10}) mask[4 + y * 20 + x] = 0;
  }
  data->set_sight(sight);
  GameSim sim(data, Modifications::faithful()); sim.init(w);
  auto r = runner(w,
      "trigger(0) conditions localTime == 1 actions side(local).cameraActivate(0,2) "
      "side(local).cameraActivate(1,0) side(local).cameraActivate(2,-1) "
      "printString(\"First invented notice\") printStringR(\"Discarded alternate\") trigger(0).disable end");
  r.tick(sim, w); r.tick(sim, w); CHECK(w.script_cameras.size() == 3 && w.center_serial == 0);
  CHECK(w.script_cameras[0].mask == 65 && w.script_cameras[0].ticks_left == 1);
  CHECK(w.script_cameras[1].mask == 64 && w.script_cameras[2].mask == 71);
  CHECK(w.messages.size() == 1 && w.messages[0].seconds_left == 4);
  // Fog stamps the owning side even when another side is being viewed.
  sim.tick(w);
  const auto& own = w.visibility.sides[2];
  CHECK(own.v[size_t(13) * own.w + 12] > 32);
  CHECK(w.visibility.sides[0].v[size_t(13) * own.w + 12] == 0);
  r.tick(sim, w); CHECK(!w.script_cameras[0].active && w.script_cameras[0].ticks_left == 0);
  r.tick(sim, w); CHECK(!w.script_cameras[0].active);
  CHECK(w.script_cameras[1].active && w.script_cameras[2].active);
  sim.tick(w); CHECK(w.visibility.sides[2].v[size_t(13) * own.w + 12] <= 32);
  // Invalid indices/sides do nothing; deactivation affects only its side and latest activation.
  auto repeat = runner(w,
      "trigger(0) conditions localTime == 1 actions side(local).cameraActivate(0,0) "
      "side(local).cameraActivate(0,1) side(7).cameraDeactivate(0) "
      "side(local).cameraActivate(99,1) side(99).cameraActivate(0,1) trigger(0).disable end "
      "trigger(1) conditions localTime == 2 actions side(local).cameraDeactivate(0) trigger(1).disable end");
  repeat.tick(sim, w); repeat.tick(sim, w);
  CHECK(w.script_cameras.size() == 4);
  repeat.tick(sim, w);
  CHECK(w.script_cameras[w.script_cameras.size() - 2].active);
  CHECK(!w.script_cameras.back().active); // expired latest does not address older indefinite source
  CHECK(repeat.unsupported() == 0);
  auto h = sim.state_hash(w); ++w.script_cameras.front().mask; CHECK(sim.state_hash(w) != h);
  --w.script_cameras.front().mask; h = sim.state_hash(w);
  ++w.mission.sides[0].cameras[0][2]; CHECK(sim.state_hash(w) != h);
  --w.mission.sides[0].cameras[0][2]; h = sim.state_hash(w);
  advance_messages(w, 3.5); CHECK(w.messages.size() == 1);
  CHECK(w.messages[0].seconds_left == 0.5 && sim.state_hash(w) == h);
  advance_messages(w, 0.5); CHECK(w.messages.empty() && sim.state_hash(w) == h);
  auto flood = runner(w, "trigger(0) conditions always actions printString(\"Queue notice\") end");
  for (int i = 0; i < 20; ++i) flood.tick(sim, w);
  CHECK(w.messages.size() == 15);
  for (const auto& message : w.messages) CHECK(message.seconds_left == 4); // simulation speed does not age prints
  advance_messages(w, 4); CHECK(w.messages.size() == 14);
  advance_messages(w, 0.5); CHECK(w.messages.size() == 14);
  advance_messages(w, 0.5); CHECK(w.messages.size() == 13);
  advance_messages(w, -10); CHECK(w.messages.size() == 13);
  sim.init(w); CHECK(w.script_cameras.empty() && w.messages.empty() && w.message_retirement == 0);
}

static void camera_view_and_input() {
  World w = world();
  SideDef first; first.index = 2; first.cameras = {{-1, 8, 6}};
  SideDef second; second.index = 1; second.cameras = {{7, 9, 6}};
  w.mission.sides = {first, second};
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  Camera camera; camera.original_bounds = true;
  camera.set_world(float(w.width_px()), float(w.height_px())); camera.set_view(400, 300);
  camera.center_on(600, 600);
  const auto x = camera.x, y = camera.y;
  auto r = runner(w,
      "trigger(0) conditions always actions side(1).cameraActivate(0,1) "
      "side(local).cameraActivate(0,0) trigger(0).disable end");
  r.tick(sim, w); CHECK(w.script_cameras.empty());
  r.tick(sim, w);
  CHECK(w.script_cameras.size() == 2 && !w.script_cameras[0].active && w.script_cameras[1].active);
  CHECK(w.script_cameras[0].side == 1 && w.script_cameras[1].side == 2);
  CHECK(w.script_cameras[1].x == -24); // off-map cameras keep their defined position
  CHECK(w.local_side == 0 && w.script_side == 2 && w.center_serial == 0);
  CHECK(camera.x == x && camera.y == y);
  CameraInput input; input.dragging = true; input.drag_dx = 24; input.drag_dy = -12;
  camera.update(input, 0.1f);
  CHECK(camera.x == x - 24 && camera.y == y + 12); // vision sources do not gate input

  auto center = runner(w,
      "trigger(0) conditions always actions centerView(32,32) trigger(0).disable end");
  center.tick(sim, w); CHECK(w.center_serial == 0);
  center.tick(sim, w); CHECK(w.center_serial == 1 && w.center_x == 1536 && w.center_y == 1536);
  camera.center_on(float(w.center_x), float(w.center_y));
  CHECK(camera.x == 1132 && camera.y == 1235);
  camera.update({}, 0.1f); CHECK(camera.x == 1132 && camera.y == 1235);
  camera.center_on(-48, -48); CHECK(camera.x == 0 && camera.y == 0);
  camera.set_view(401, 301); camera.center_on(600, 600);
  CHECK(camera.x == 400 && camera.y == 450); // native integer division with an odd viewport
  camera.set_view(1600, 1600); camera.center_on(0, 0);
  CHECK(camera.x == -68 && camera.y == -65); // upper clamp follows the lower clamp
  camera.set_view(400, 300); camera.set_zoom(2, 0, 0); camera.center_on(1536, 1536);
  CHECK(camera.x == 1332 && camera.y == 1385);
  w.mission.sides[0].cameras.push_back({-44739243, 44739242, std::numeric_limits<int32_t>::max()});
  auto extreme = runner(w,
      "trigger(0) conditions always actions side(local).cameraActivate(1,0) "
      "centerView(2147483647,-2147483648) trigger(0).disable end");
  extreme.tick(sim, w); extreme.tick(sim, w);
  const auto& source = w.script_cameras.back();
  CHECK(source.x == -2147483640 && source.y == 2147483640 && source.mask == 64);
  CHECK(w.center_x == -48 && w.center_y == 0);
  std::vector<uint8_t> grid(4, 152), mask(404, 0); mask[0] = mask[2] = 20;
  stamp_sight(grid, 2, 2, data::LightMask({mask.data(), mask.size()}), source.x, source.y, true);
  CHECK(std::all_of(grid.begin(), grid.end(), [](uint8_t value) { return value == 152; }));
}

static void timer_set_kill_delay() {
  World w = world(); GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w,
      "trigger(0) conditions always actions timer(4).set(9) trigger(0).disable end "
      "trigger(1) conditions timer(4) == 9 actions timer(4).kill trigger(1).disable end "
      "trigger(2) conditions timer(4) == 0 actions side(local).addMoney(1) end");
  r.tick(sim, w); CHECK(condition(r, w, "timer(4) == 0") && w.side_state[2].money == 0);
  r.tick(sim, w); CHECK(condition(r, w, "timer(4) == 9") && w.side_state[2].money == 1);
  r.tick(sim, w); CHECK(condition(r, w, "timer(4) == 0") && w.side_state[2].money == 1);
  r.tick(sim, w); CHECK(condition(r, w, "timer(4) == 1") && w.side_state[2].money == 2);
  // Killing an absent timer must leave the state unchanged, including pool membership.
  auto absent = runner(w, "trigger(0) conditions always actions timer(77).kill trigger(0).disable end");
  for (int i = 0; i < 4; ++i) absent.tick(sim, w);
  CHECK(condition(absent, w, "timer(77) == 0"));
  auto set_after_kill = runner(w,
      "trigger(0) conditions always actions timer(8).stop timer(8).kill timer(8).set(17) trigger(0).disable end");
  set_after_kill.tick(sim, w); set_after_kill.tick(sim, w);
  CHECK(condition(set_after_kill, w, "timer(8) == 17"));
  set_after_kill.tick(sim, w); CHECK(condition(set_after_kill, w, "timer(8) == 18"));
}

static void johnka_enable_selection() {
  World w = world();
  for (int side : {1, 2, 3}) {
    object(w, 88, side, -1, 10, 10);
    auto& o = w.objects.back(); o.source = ObjectSource::Johnka; o.scenario = 17; o.vessel_enabled = false;
  }
  w.objects.back().alive = false;
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "trigger(0) conditions always actions johnka(17).enable trigger(0).disable end");
  r.tick(sim, w); r.tick(sim, w);
  CHECK(!w.objects[0].vessel_enabled && w.objects[1].vessel_enabled && !w.objects[2].vessel_enabled);
  CHECK(w.objects[1].removed && !w.objects[1].vessel_route); // action changes only the byte
  sim.tick(w); CHECK(!w.objects[1].removed && w.objects[1].vessel_route);
  const auto position = w.objects[1].cur_x;
  CHECK(sim.activate_vessel(w, 17, ObjectSource::Johnka));
  CHECK(w.objects[1].cur_x == position); // repeated enable cannot reset a patrol
  auto disable = runner(w, "trigger(0) conditions always actions johnka(17).disable trigger(0).disable end");
  disable.tick(sim, w); disable.tick(sim, w);
  CHECK(!w.objects[1].vessel_enabled && !w.objects[1].removed);
  CHECK(!sim.activate_vessel(w, 999, ObjectSource::Johnka));
  auto missing = runner(w, "trigger(0) conditions always actions johnka(999).enable trigger(0).disable end");
  missing.tick(sim, w); missing.tick(sim, w); CHECK(w.messages.empty());
}

static void script_message_layout() {
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 100, 90, 32, SDL_PIXELFORMAT_ARGB8888);
  CHECK(surface);
  SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface); CHECK(renderer);
  {
    Font font; CHECK(font.init(renderer));
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
    SDL_Rect previous{0, 0, 95, 90}; SDL_RenderSetClipRect(renderer, &previous);
    std::vector<Message> messages = {{std::string(100, 'E'), 0, 4}, {"B", 0, 4}, {"C", 0, 4}, {"D", 0, 4}};
    draw_script_messages(renderer, font, messages, {0, 0, 80, 70}, 1);
    SDL_Rect restored{}; SDL_RenderGetClipRect(renderer, &restored);
    CHECK(restored.x == 0 && restored.y == 0 && restored.w == 95 && restored.h == 90);
    std::vector<uint32_t> pixels(100 * 90);
    CHECK(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(), 100 * 4) == 0);
    auto lit = [&](int x, int y) { return (pixels[size_t(y) * 100 + x] & 0x00ffffffu) != 0; };
    CHECK(lit(10, 20) && lit(10, 38) && lit(11, 56)); // oldest first; one entry per 18-pixel line
    for (int y = 0; y < 90; ++y) for (int x = 0; x < 100; ++x) {
      if (x < 10 || x >= 80 || y < 20 || (y >= 26 && y < 38) || y >= 70) CHECK(!lit(x, y));
    }
  }
  SDL_DestroyRenderer(renderer); SDL_FreeSurface(surface);
}

static void campaign_outcome_requires_script() {
  // Empty forces alone must not impose a universal campaign victory or defeat.
  World w = world();
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  for (int i = 0; i < 12; ++i) { w.begin_tick(); sim.tick(w); }
  CHECK(w.outcome == kRunning);
  // An AI outcome cannot finish the human mission; either human outcome latches once.
  for (bool victory : {false, true}) {
    World local = world();
    GameSim attempt(rules(), Modifications::faithful()); attempt.init(local);
    auto script = runner(local,
        "trigger(0) conditions always actions side(1).victory end");
    script.tick(attempt, local); script.tick(attempt, local);
    local.begin_tick(); attempt.tick(local);
    CHECK(local.side_outcome[1] == kVictory && local.outcome == kRunning);
    auto finish = runner(local, std::string("trigger(0) conditions side(local).units == 0 actions side(local).") +
        (victory ? "victory side(local).defeat" : "defeat side(local).victory") + " end");
    finish.tick(attempt, local); finish.tick(attempt, local);
    local.begin_tick(); attempt.tick(local);
    CHECK(local.outcome == (victory ? kVictory : kDefeat));
    CHECK(finish.outcome_origin() && finish.outcome_origin()->trigger == 0);
  }
}

static void campaign_gates() {
  World w = world(); object(w, 0, 1, -1, 25, 25);
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  // An enable action elsewhere must not suppress the initial watcher. Startup
  // disables the countdown; eliminating the enemy then starts it through real actions.
  auto r = runner(w,
      "trigger(1) conditions side(1).units == 0 actions timer(7).set(0) trigger(2).enable trigger(1).disable end "
      "trigger(2) conditions timer(7) == 3 actions side(local).victory end "
      "trigger(3) conditions always actions trigger(2).disable trigger(3).disable end "
      "trigger(4) conditions localTime == 100 actions trigger(1).enable end");
  r.tick(sim, w); r.tick(sim, w); CHECK(w.outcome == kRunning && r.trigger_enabled(1) && !r.trigger_enabled(2));
  sim.script_kill(w, 1);
  r.tick(sim, w); r.tick(sim, w); CHECK(r.trigger_enabled(2) && w.outcome == kRunning);
  r.tick(sim, w); r.tick(sim, w); r.tick(sim, w); CHECK(w.outcome == kRunning);
  r.tick(sim, w);
  CHECK(w.outcome == kRunning && w.side_outcome[w.script_side] == kVictory && r.outcome_origin()->trigger == 2);
  w.begin_tick(); sim.tick(w); // Outcome is copied after all simulation phases.
  CHECK(w.outcome == kVictory);
  auto timer = runner(w, "trigger(0) conditions timer(8) == 3 actions side(0).addMoney(1) trigger(0).disable end");
  timer.tick(sim, w); timer.tick(sim, w); CHECK(w.side_state[0].money == 0);
  timer.tick(sim, w); timer.tick(sim, w); CHECK(w.side_state[0].money == 0);
  timer.tick(sim, w); CHECK(w.side_state[0].money == 1);
}

static void town_tactics_and_capture() {
  World w = world();
  w.mission.towns = {{0, 1, 15, 15, 1, 0, 52}};
  PlacedObject member; member.source = ObjectSource::Town; member.group = 0;
  member.type = 0; member.side = 1; member.x = 16; member.y = 15;
  w.mission.objects.push_back(member);
  place_mission_objects(w);
  CHECK(w.objects[0].tactic == 52 && w.objects[1].tactic == 52);
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  QueueItem queued; queued.type = 0; queued.town = 0; queued.total = queued.remaining = 10;
  w.side_state[1].queue.push_back(queued);
  auto r = runner(w,
      "trigger(0) conditions side(1).tactic(52).valid != 1 actions side(local).defeat end "
      "trigger(1) conditions localTime == 2 actions side(1).tactic(52).giftToSide(local) end");
  r.tick(sim, w); CHECK(w.outcome == kRunning);
  r.tick(sim, w); r.tick(sim, w); CHECK(w.mission.towns[0].side == 2 && w.town_state[0].side == 2);
  CHECK(w.objects[0].side == 2 && w.objects[1].side == 2);
  CHECK(w.side_state[1].queue.empty() && w.side_state[2].queue.empty());
  CHECK(condition(r, w, "side(local).towns == 1") && condition(r, w, "side(1).towns == 0"));
  CHECK(condition(r, w, "side(local).tactic(52).valid == 1"));
  CHECK(w.town_state[0].hall == w.objects[1].id);
  // Invalid and same-owner transfers leave state unchanged.
  auto hash = sim.state_hash(w); sim.transfer_town(w, 0, -1); sim.transfer_town(w, 0, 2);
  CHECK(sim.state_hash(w) == hash);
  // A nearby defender prevents capture. Killing it lets an enemy ground unit capture.
  object(w, 0, 1, -1, 14, 15); sim.init(w);
  w.relation[2][2] = kEnemy; // Self-ownership defends even with a default relation table.
  sim.tick(w); CHECK(w.mission.towns[0].side == 2);
  sim.script_kill(w, 1); sim.tick(w); CHECK(w.mission.towns[0].side == 1);
  CHECK(w.town_state[0].side == 1 && w.objects[1].side == 1);

  // Scripted ownership changes reconstruct through ordinary command-log replay.
  TempDir directory;
  const auto path = directory.path() / "town-gift.scn";
  std::ofstream(path) << "trigger(0) conditions localTime == 2 actions side(1).tactic(52).giftToSide(local) trigger(0).disable end";
  World initial = world(); initial.mission = w.mission;
  initial.mission.towns[0].side = 1;
  place_mission_objects(initial);
  auto data = rules(); World live = initial;
  GameSim original(data, Modifications::faithful()); CHECK(original.attach_script(path.string())); original.init(live);
  for (int i = 0; i < 3; ++i) { live.begin_tick(); original.tick(live); }
  CHECK(live.mission.towns[0].side == 2);
  World restored = initial;
  auto replayed = GameSim::replay(restored, data, original.log(), original.tick_count(), path.string());
  CHECK(replayed->state_hash(restored) == original.state_hash(live));
}

static void embarked_script_identity() {
  World w = world(); object(w, 82, 2, 9, 5, 5); object(w, 84, 2, -1, 5, 5);
  w.objects[0].removed = true; w.objects[0].carrier = 2; w.objects[1].cargo = {1};
  GameSim sim(rules(), Modifications::faithful()); sim.init(w); auto r = runner(w, "");
  CHECK(condition(r, w, "side(local).unitsOfId(UGI_ABDULA) == 1"));
  CHECK(condition(r, w, "side(local).tactic(9).valid == 1"));
  CHECK(condition(r, w, "side(local).unitsOfUGIInRect(UGI_ABDULA,0,0,10,10) == 0"));
  sim.script_kill(w, 2);
  CHECK(condition(r, w, "side(local).unitsOfId(UGI_ABDULA) == 0"));
}

static void tactic_idle_uses_controller() {
  auto w = world(); object(w, 0, 2, 5, 3, 10);
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "trigger(0) conditions always actions end");
  w.objects[0].order = uint8_t(Order::Attack); w.objects[0].target = 99;
  CHECK(condition(r, w, "side(local).tactic(5).idle == 1"));
  sim.script_order(w, {1}, 26 * 48 + 24, 10 * 48 + 24, true);
  CHECK(condition(r, w, "side(local).tactic(5).idle == 0"));
}
static void distant_join_replay() {
  TempDir directory; const auto path = directory.path() / "join.scn";
  std::ofstream(path) << "trigger(0) conditions always actions side(local).tactic(5).joinTactic(6) trigger(0).disable end";
  auto data = rules();
  auto initial = world(); object(initial, 0, 2, 5, 3, 10); object(initial, 0, 2, 6, 26, 10);
  auto w = initial;
  GameSim sim(data, Modifications::faithful()); CHECK(sim.attach_script(path.string())); sim.init(w);
  for (int tick = 0; tick < 700; ++tick) {
    w.begin_tick(); sim.tick(w);
    if (tick == 2) {
      CHECK(w.objects[0].tactic == 5);
      CHECK(w.unit_tactics.at({2, 5}).merging);
    }
    if (tick == 100 || tick == 699) {
      auto restored = initial;
      auto replayed = GameSim::replay(restored, data, sim.log(), sim.tick_count(), path.string());
      CHECK(replayed->state_hash(restored) == sim.state_hash(w));
    }
  }
  CHECK(w.objects[0].tactic == 6);
  CHECK(w.unit_tactics.at({2, 6}).x == 26);
}
static void pending_path_uses_previous_latch() {
  World w = world(); object(w, 0, 2, 7, 3, 4);
  w.mission.paths = {{{8, 9}}};
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w,
      "trigger(0) conditions always actions side(local).tactic(7).setPath(0) trigger(0).disable end "
      "trigger(1) conditions side(local).tactic(7).idle != 0 actions side(local).addMoney(1) trigger(1).disable end");
  r.tick(sim, w); CHECK(w.side_state[2].money == 0);
  r.tick(sim, w); CHECK(w.side_state[2].money == 1); // latched idle before path assignment
}

static void controller_idle_states() {
  World w = world(); object(w, 0, 2, 5, 3, 4);
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "");
  auto& group = w.unit_tactics.at({2, 5});
  std::set<uint64_t> hashes;
  for (auto state : {GroupState::Inactive, GroupState::Stopped, GroupState::Waiting, GroupState::Moving}) {
    group.state = state;
    hashes.insert(sim.state_hash(w));
    const bool idle = state == GroupState::Inactive || state == GroupState::Stopped;
    // Member orders deliberately disagree with the controller.
    w.objects[0].order = uint8_t(idle ? Order::AttackMove : Order::Idle);
    CHECK(condition(r, w, std::string("side(local).tactic(5).idle == ") + (idle ? "1" : "0")));
  }
  CHECK(hashes.size() == 4);
  CHECK(!condition(r, w, "side(local).tactic(99).idle != 1"));
  // V: inactive groups do not supply member movement callbacks.
  group.state = GroupState::Inactive; group.x = 5; group.y = 4;
  w.objects[0].order = uint8_t(Order::Move); w.objects[0].return_to_tactic = true;
  const auto x = w.objects[0].cur_x, y = w.objects[0].cur_y;
  for (int i = 0; i < 3; ++i) { w.begin_tick(); sim.tick(w); }
  CHECK(w.objects[0].cur_x == x && w.objects[0].cur_y == y);

}

static void attack_town_controller_lookup() {
  for (bool own : {false, true}) {
    World w = world(); object(w, 0, 2, 5, 3, 4);
    const int target_side = own ? 2 : 1;
    // First controller wins; object scenario numbers and later duplicates lose.
    w.mission.towns.push_back({7, target_side, 12, 14, 11, 0, 30});
    w.mission.towns.push_back({8, 0, 25, 26, 11, 0, 30});
    object(w, 11, target_side, -1, 18, 19); w.objects.back().town = 7; // earlier hall-shaped decoy
    object(w, 11, target_side, -1, 12, 14); w.objects.back().town = 7; w.objects.back().town_hall = true;
    w.objects.back().scenario = 999;
    object(w, 11, 0, -1, 25, 26); w.objects.back().town = 8; w.objects.back().town_hall = true;
    w.objects.back().scenario = 30;
    // A friendly foreign town is still a valid script destination.
    w.relation[2][1] = kFriend;
    GameSim sim(rules(), Modifications::faithful()); sim.init(w);
    auto r = runner(w, "trigger(0) conditions always actions side(local).tactic(5).attackTown(30) "
        "trigger(0).disable end");
    r.tick(sim, w); CHECK(w.objects[0].order == uint8_t(Order::Idle));
    r.tick(sim, w);
    if (own) CHECK(w.objects[0].order == uint8_t(Order::Idle));
    else {
      CHECK(w.objects[0].dest_x == 12 * kTilePx + 24);
      CHECK(w.objects[0].dest_y == 14 * kTilePx + 24);
      CHECK(w.unit_tactics.at({2, 5}).state == GroupState::Moving);
      CHECK(condition(r, w, "side(local).tactic(5).idle == 0"));
    }
    const auto before = w.objects[0].dest_x;
    auto missing = runner(w, "trigger(0) conditions always actions side(local).tactic(5).attackTown(777) "
        "side(local).tactic(99).attackTown(30) trigger(0).disable end");
    missing.tick(sim, w); missing.tick(sim, w);
    CHECK(w.objects[0].dest_x == before);
  }
}

static void join_follows_before_merging() {
  World w = world(); object(w, 0, 2, 5, 3, 4); object(w, 0, 2, 6, 23, 4);
  w.objects[0].town = 7; w.objects[1].town = 8;
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "trigger(0) conditions always actions side(local).tactic(5).joinTactic(6) "
      "trigger(0).disable end");
  r.tick(sim, w); r.tick(sim, w);
  CHECK(w.objects[0].tactic == 5); // no remote reassignment
  CHECK(w.objects[0].town == 7);
  CHECK(w.unit_tactics.at({2, 5}).merging);
  CHECK(w.unit_tactics.at({2, 5}).parent == TacticKey(2, 6));
  CHECK(w.objects[0].order == uint8_t(Order::Follow));
  const auto x = w.objects[0].cur_x;
  for (int i = 0; i < 12; ++i) { w.begin_tick(); sim.tick(w); }
  CHECK(w.objects[0].cur_x > x && w.objects[0].tactic == 5);
  // Exactly reach is outside: only strictly inside allows absorption.
  auto& source = w.unit_tactics.at({2, 5});
  const auto& target = w.unit_tactics.at({2, 6});
  const int reach = tactic_reach(w.objects[0].attack_mode);
  source.x = target.x - reach; source.y = target.y;
  source.path_ticks = 0; source.state = GroupState::Stopped;
  CHECK(tactic_norm(source.x - target.x, 0) == reach);
  w.begin_tick(); sim.tick(w); CHECK(w.objects[0].tactic == 5);
  w.unit_tactics.at({2, 5}).x = target.x - reach + 1;
  w.begin_tick(); sim.tick(w); CHECK(w.objects[0].tactic == 6);
  CHECK(!w.unit_tactics.contains({2, 5}));
  CHECK(w.objects[0].town == 8); // visible absorption adopts the receiving town
}

static void joining_replay_and_followers() {
  TempDir directory;
  const auto path = directory.path() / "joining.scn";
  std::ofstream(path) << "trigger(0) conditions always actions side(local).tactic(5).joinTactic(6) "
      "side(local).tactic(7).joinTactic(5) trigger(0).disable end";
  World initial = world();
  object(initial, 0, 2, 5, 3, 4); object(initial, 0, 2, 6, 23, 4);
  object(initial, 0, 2, 7, 3, 24);
  auto data = rules(); World w = initial;
  GameSim sim(data, Modifications::faithful()); CHECK(sim.attach_script(path.string())); sim.init(w);
  for (int i = 0; i < 12; ++i) { w.begin_tick(); sim.tick(w); }
  CHECK(w.unit_tactics.at({2, 5}).merging && w.unit_tactics.at({2, 7}).merging);
  const auto before = sim.state_hash(w);
  w.unit_tactics.at({2, 5}).merging = false;
  CHECK(sim.state_hash(w) != before);
  w.unit_tactics.at({2, 5}).merging = true;
  World rebuilt = initial;
  auto replayed = GameSim::replay(rebuilt, data, sim.log(), sim.tick_count(), path.string());
  CHECK(replayed->state_hash(rebuilt) == sim.state_hash(w));
  for (int i = 0; i < 24; ++i) {
    w.begin_tick(); rebuilt.begin_tick(); sim.tick(w); replayed->tick(rebuilt);
    CHECK(replayed->state_hash(rebuilt) == sim.state_hash(w));
  }
  // Reject an engine merge cycle without disturbing the existing follow chain.
  sim.script_join(w, {2}, {1});
  CHECK(!w.unit_tactics.at({2, 6}).merging);
  const auto& target = w.unit_tactics.at({2, 6});
  w.objects[2].follow_tactic = 5; // ordinary followers are redirected as well
  w.unit_tactics.at({2, 5}).x = target.x;
  w.unit_tactics.at({2, 5}).y = target.y;
  w.begin_tick(); sim.tick(w);
  CHECK(w.objects[0].tactic == 6);
  CHECK(w.unit_tactics.at({2, 7}).parent == TacticKey(2, 6));
  CHECK(w.objects[2].follow_tactic == 6);
}

static void clock_startup_phase() {
  for (int start : {-2147483647 - 1, -1, 0, 7198, 7199, 7200, 2147483647}) {
    World w = world(); w.mission.start_time = start;
    GameSim sim(rules(), Modifications::faithful()); sim.init(w);
    auto r = runner(w, "");
    const int clock = std::clamp(start, 0, 7199);
    CHECK(w.day_clock == clock);
    CHECK(condition(r, w, "localTime == " + std::to_string(clock)));
    r.tick(sim, w);
    CHECK(condition(r, w, "localTime == " + std::to_string((clock + 1) % 7200)));
  }
  TempDir directory;
  const auto path = directory.path() / "clock.scn";
  std::ofstream(path) << "trigger(0) conditions localTime == 0 globalTime == 0 actions side(local).addMoney(7) end";
  for (int tileset : {0, 1}) {
    World w = world(); w.mission.start_time = 7199; w.mission.tileset = tileset;
    GameSim sim(rules(), Modifications::faithful()); CHECK(sim.attach_script(path.string())); sim.init(w);
    w.begin_tick(); sim.tick(w); CHECK(w.day_clock == 0 && w.side_state[2].money == 0);
    w.begin_tick(); sim.tick(w); CHECK(w.day_clock == 1 && w.side_state[2].money == 7);
    w.begin_tick(); sim.tick(w); CHECK(w.side_state[2].money == 7);
  }
}

static void town_list_and_validity() {
  World w = world();
  // Duplicate scenario IDs and unfinished halls do not collapse town controllers.
  w.mission.towns = {{0, 2, 4, 4, 1, 0, 50}, {1, 2, 9, 9, 1, 0, 50}, {2, 1, 20, 20, 1, 0, 50}};
  place_mission_objects(w);
  object(w, 11, 2, -1, 12, 12); // an unattached hall is not another town
  w.objects[0].progress = 100;
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "");
  for (const auto& comparison : {"== 2", "!= 1", "> 1", ">= 2", "< 3", "<= 2"})
    CHECK(condition(r, w, std::string("side(local).towns ") + comparison));
  CHECK(condition(r, w, "side(1).towns == 1"));
  CHECK(condition(r, w, "side(7).towns == 0"));
  sim.script_kill(w, 1);
  CHECK(condition(r, w, "side(local).towns == 2"));
  CHECK(condition(r, w, "side(local).tactic(50).valid == 0"));
  // The first empty town wins over the later live duplicate.
  CHECK(condition(r, w, "side(local).tactic(999).valid == 0"));
  sim.transfer_town(w, 1, 1);
  CHECK(condition(r, w, "side(local).towns == 1"));
  CHECK(condition(r, w, "side(1).towns == 2"));
  w.script_side = 1;
  CHECK(condition(r, w, "side(local).towns == 2"));
}

static void town_source_dispatch() {
  World w = world(); w.mission.towns = {{0, 2, 3, 4, 1, 0, 50}, {1, 1, 25, 24, 1, 0, 60}};
  place_mission_objects(w);
  object(w, 0, 2, 50, 4, 4); w.objects.back().town = 0;
  object(w, 17, 2, 50, 5, 4); w.objects.back().town = 0;
  object(w, 0, 2, 7, 26, 26);
  object(w, 0, 2, 9, 20, 10); w.objects.back().town = 0; // separate group remembers home
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "trigger(0) conditions always actions side(local).tactic(50).moveTo(20,20) "
      "side(local).tactic(50).attackTown(60) trigger(0).disable end");
  const auto x = w.objects[2].cur_x, y = w.objects[2].cur_y;
  r.tick(sim, w); r.tick(sim, w);
  CHECK(w.objects[2].order == uint8_t(Order::Idle));
  CHECK(w.objects[2].cur_x == x && w.objects[2].cur_y == y);
  auto join = runner(w, "trigger(0) conditions always actions side(local).tactic(50).joinTactic(7) trigger(0).disable end");
  join.tick(sim, w); join.tick(sim, w);
  CHECK(w.objects[0].town == 0 && w.objects[0].tactic == 50);
  CHECK(w.objects[3].town == 0 && w.objects[3].tactic == 50); // defence stays home
  CHECK(w.objects[2].town == -1 && w.objects[2].tactic != 50 && w.objects[2].tactic != 7);
  const auto& launched = w.unit_tactics.at(tactic_key(w.objects[2]));
  CHECK(launched.merging && launched.parent == tactic_key(w.objects[4]));
  CHECK(condition(join, w, "side(local).tactic(50).valid == 1"));
  CHECK(condition(join, w, "side(local).tactic(" + std::to_string(w.objects[2].tactic) + ").valid == 0"));
  // A same-owner gift still resets the town's production and repair state.
  w.town_state[0].repair_mode = RepairMode::Repair;
  QueueItem item; item.type = 0; item.town = 0; item.total = item.remaining = 10;
  w.side_state[2].queue.push_back(item);
  auto gift = runner(w, "trigger(0) conditions always actions side(local).tactic(50).giftToSide(local) trigger(0).disable end");
  gift.tick(sim, w); gift.tick(sim, w);
  CHECK(w.side_state[2].queue.empty() && w.town_state[0].repair_mode == RepairMode::Off);
  CHECK(w.objects[2].town == -1); // detached recruits are not town members
  CHECK(w.objects[5].town == 0 && w.objects[5].tactic == 9);
  auto foreign = runner(w, "trigger(0) conditions always actions side(local).tactic(50).giftToSide(1) trigger(0).disable end");
  foreign.tick(sim, w); foreign.tick(sim, w);
  CHECK(w.objects[0].side == 1 && w.objects[3].side == 1);
  CHECK(w.objects[5].side == 2 && w.objects[5].tactic == 9);
}

static void gift_controller_reset() {
  World w = world(); object(w, 0, 2, 5, 3, 4); object(w, 0, 2, 5, 4, 4);
  w.objects[0].attack_mode = w.objects[1].attack_mode = AttackMode::Aggressive;
  object(w, 0, 1, 5, 25, 25);
  object(w, 17, 2, 5, 5, 4); w.objects.back().defence = true; w.objects.back().scenario = 42;
  w.mission.paths = {{{10, 10}, {20, 20}}};
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "trigger(0) conditions always actions side(local).tactic(5).setCycledPath(0) trigger(0).disable end "
      "trigger(1) conditions globalTime == 2 actions side(local).tactic(5).giftToSide(1) trigger(1).disable end");
  for (int i = 0; i < 4; ++i) r.tick(sim, w);
  CHECK(w.objects[0].side == 1 && w.objects[1].side == 1);
  CHECK(w.objects[0].tactic == w.objects[1].tactic && w.objects[0].tactic != 5);
  CHECK(w.unit_tactics.at(tactic_key(w.objects[0])).scenario_id == 5);
  CHECK(w.objects[3].scenario == 42 && condition(r, w, "side(1).defence(42).valid == 1"));
  CHECK(w.objects[0].attack_mode == AttackMode::Balanced && w.objects[0].follow_tactic == -1);
  CHECK(w.objects[0].town == -1 && w.objects[0].target == 0);
  CHECK(condition(r, w, "side(2).tactic(5).valid == 0"));
  CHECK(condition(r, w, "side(1).tactic(5).valid == 1")); // receiver's preexisting reserved group
  CHECK(condition(r, w, "side(1).tactic(" + std::to_string(w.objects[0].tactic) + ").valid == 0"));
  w.begin_tick(); sim.tick(w);
  const auto& receiving = w.unit_tactics.at(tactic_key(w.objects[0]));
  CHECK(receiving.idle() && !receiving.merging && !receiving.lost && receiving.path.empty());
  const auto hash = sim.state_hash(w);
  w.unit_tactics.at(tactic_key(w.objects[0])).scenario_id = 99;
  CHECK(sim.state_hash(w) != hash);
  w.unit_tactics.at(tactic_key(w.objects[0])).scenario_id = 5;
  CHECK(sim.state_hash(w) == hash);
}

static void path_and_gift_replay() {
  TempDir directory; const auto path = directory.path() / "path-gift.scn";
  std::ofstream(path) << "trigger(0) conditions always actions side(local).tactic(5).setCycledInvertedPath(0) trigger(0).disable end "
      "trigger(1) conditions globalTime == 80 actions side(local).tactic(5).giftToSide(1) trigger(1).disable end";
  World initial = world(); object(initial, 0, 2, 5, 3, 4); object(initial, 0, 2, 5, 4, 4);
  initial.mission.paths = {{{6, 4}, {8, 4}}};
  const auto data = rules(); World live = initial;
  GameSim sim(data, Modifications::faithful()); CHECK(sim.attach_script(path.string())); sim.init(live);
  for (int tick = 1; tick <= 140; ++tick) {
    live.begin_tick(); sim.tick(live);
    if (tick != 40 && tick != 82 && tick != 140) continue;
    World restored = initial;
    auto replayed = GameSim::replay(restored, data, sim.log(), sim.tick_count(), path.string());
    CHECK(replayed->state_hash(restored) == sim.state_hash(live));
    for (int extra = 0; extra < 10; ++extra) { restored.begin_tick(); replayed->tick(restored); }
    World continued = initial;
    auto control = GameSim::replay(continued, data, sim.log(), sim.tick_count() + 10, path.string());
    CHECK(replayed->state_hash(restored) == control->state_hash(continued));
  }
  CHECK(live.objects[0].side == 1 && live.objects[1].side == 1);
}

static void persistent_cycle_flag() {
  World w = world(); object(w, 0, 2, 5, 3, 4);
  w.mission.paths = {{{8, 9}, {10, 11}}, {{12, 13}, {14, 15}}};
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "trigger(0) conditions always actions side(local).tactic(5).setCycledPath(0) "
      "side(local).tactic(5).setInvertedPath(1) trigger(0).disable end");
  r.tick(sim, w); r.tick(sim, w);
  auto& anchor = w.unit_tactics.at({2, 5});
  CHECK(w.objects[0].dest_x == 14 * kTilePx + 24);
  anchor.x = 14; anchor.y = 15; sim.script_stop(w, {1}); r.tick(sim, w);
  CHECK(w.objects[0].dest_x == 12 * kTilePx + 24);
  anchor.x = 12; anchor.y = 13; sim.script_stop(w, {1}); r.tick(sim, w);
  CHECK(w.objects[0].dest_x == 14 * kTilePx + 24); // plain inverted setter retained cycling
}

static void path_replacement_and_anchor() {
  for (const std::string method : {"setPath", "setCycledPath", "setInvertedPath", "setCycledInvertedPath"}) {
    World w = world(); object(w, 0, 2, 5, 3, 4); object(w, 0, 2, 6, 26, 26);
    w.mission.paths = {{{3, 4}, {3, 4}, {8, 9}, {10, 11}}};
    const bool inverted = method.find("Inverted") != std::string::npos;
    if (inverted) std::reverse(w.mission.paths[0].begin(), w.mission.paths[0].end());
    GameSim sim(rules(), Modifications::faithful()); sim.init(w);
    sim.script_join(w, {1}, {2});
    CHECK(w.unit_tactics.at({2, 5}).merging);
    auto r = runner(w, "trigger(0) conditions always actions side(local).tactic(5)." + method + "(0) trigger(0).disable end");
    r.tick(sim, w); r.tick(sim, w);
    CHECK(!w.unit_tactics.at({2, 5}).merging);
    CHECK(w.objects[0].dest_x == 8 * kTilePx + 24); // skip duplicate starting-anchor points
    // Stopping before arrival retries this waypoint rather than skipping it.
    sim.script_stop(w, {1}); r.tick(sim, w);
    CHECK(w.objects[0].dest_x == 8 * kTilePx + 24);
    // A member reaching the destination does not complete the shared route.
    sim.teleport(w, 1, 8 * kTilePx + 24, 9 * kTilePx + 24);
    sim.script_stop(w, {1}); r.tick(sim, w);
    CHECK(w.objects[0].dest_x == 8 * kTilePx + 24);
    auto& anchor = w.unit_tactics.at({2, 5}); anchor.x = 8; anchor.y = 9;
    sim.script_stop(w, {1}); r.tick(sim, w);
    CHECK(w.objects[0].dest_x == 10 * kTilePx + 24);
    auto empty = runner(w, "trigger(0) conditions always actions side(local).tactic(5).setPath(1) trigger(0).disable end");
    w.mission.paths.push_back({});
    empty.tick(sim, w); empty.tick(sim, w);
    CHECK(w.unit_tactics.at({2, 5}).idle() && w.objects[0].order == uint8_t(Order::Idle));
    CHECK(condition(empty, w, "side(local).tactic(5).idle == 1"));
  }
}

int main() {
  town_list_and_validity(); town_source_dispatch(); gift_controller_reset(); path_and_gift_replay(); persistent_cycle_flag(); path_replacement_and_anchor();
  controller_idle_states(); attack_town_controller_lookup(); join_follows_before_merging(); joining_replay_and_followers(); clock_startup_phase();
  rectangle_box_edges(); side_command_contracts();
  rectangle_runtime_categories(); gift_detaches_home_and_outcome_precedence(); tactic_lookup_and_money(); presentation_contracts();
  campaign_outcome_requires_script(); campaign_gates(); town_tactics_and_capture(); embarked_script_identity(); tactic_idle_uses_controller(); distant_join_replay(); pending_path_uses_previous_latch();
  attacked_notification();
  source_locations(); order_and_enablement(); timers(); conditions_and_identity(); paths_and_gifts(); actions_and_outcomes(); defence_loading(); action_observation_contracts(); replay();
  camera_view_and_input(); timer_set_kill_delay(); johnka_enable_selection(); script_message_layout();
  std::puts("Synthetic script contracts passed.");
}
