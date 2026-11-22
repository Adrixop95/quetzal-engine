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
#include "engine/sim/sight.hpp"
#include "engine/ui/hud.hpp"
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
      "trigger(2) conditions globalTime == 3 actions trigger(1).enable end");
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
      "trigger(1) conditions globalTime == 3 actions timer(2).start trigger(1).disable end "
      "trigger(2) conditions globalTime == 5 actions timer(2).kill trigger(2).disable end "
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
  w.side_state[2].money = 50;
  GameSim sim(rules(), Modifications::faithful()); sim.init(w); auto r = runner(w, "");
  const std::string checks[] = {
    "always", "localTime == 0", "globalTime == 0", "timer(0) == 0",
    "side(local).units == 2", "side(local).towns == 1", "side(local).enemies == 1",
    "side(local).tactics == 1", "side(local).money == 50", "side(local).unitsInRect(3,4,6,7) == 2",
    "side(local).unitsOfUGIInRect(UGI_FOOTMAN,3,4,3,4) == 1", "side(local).unitsOfId(UGI_FOOTMAN) == 1",
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
  CHECK(condition(r, w, "side(local).unitsInRect(15,15,15,15) == 1")); // defence kind counts
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
  w.objects[2].progress = 999; CHECK(condition(r, w, "side(local).towns == 0"));
}

static void rectangle_runtime_categories() {
  World w = world();
  // Each construction family has an independent runtime category; all these
  // ordinary objects are included, including structures and both vessel types.
  for (int type : {0, 30, 11, 7, 83, 33, 3, 17, 84, 87, 88, 89})
    object(w, type, 2, -1, 5, 6);
  GameSim sim(rules(), Modifications::faithful()); sim.init(w);
  auto r = runner(w, "");
  CHECK(condition(r, w, "side(local).unitsInRect(5,6,5,6) == 11"));
  CHECK(condition(r, w, "side(local).unitsOfUGIInRect(UGI_BOAT,5,6,5,6) == 1"));
  CHECK(condition(r, w, "side(local).unitsOfUGIInRect(UGI_JOHNKA,5,6,5,6) == 1"));
  CHECK(condition(r, w, "side(local).unitsOfUGIInRect(UGI_CAMERA,5,6,5,6) == 0"));
  w.objects[8].alive = false;
  CHECK(condition(r, w, "side(local).unitsOfUGIInRect(UGI_BOAT,5,6,5,6) == 0"));
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
    w.objects[0].order = uint8_t(Order::Idle); r.tick(sim, w);
    CHECK(w.objects[0].dest_x == (inverted ? 6 : 8) * kTilePx + kTilePx / 2);
    w.objects[0].order = uint8_t(Order::Idle); r.tick(sim, w);
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
      "trigger(1) conditions globalTime == 2 actions side(local).tactic(5).joinTactic(6) trigger(1).disable end "
      "trigger(2) conditions globalTime == 3 actions side(local).tactic(6).kill trigger(2).disable end "
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
  w.objects[1].scenario = 30;
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
      "trigger(1) conditions globalTime == 2 actions side(local).setEnemyTo(1) end");
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
    CHECK(ScriptRunner::supports(a.kind) == (a.kind == ActKind::BoatActivate || a.kind == ActKind::JohnkaEnable || a.kind == ActKind::JohnkaDisable || a.kind == ActKind::SubmarineGo || a.kind == ActKind::SideCameraDeactivate));
  unsupported.tick(sim, w); unsupported.tick(sim, w); CHECK(unsupported.unsupported() == 2);
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
  for (const auto& method : {"side(local).buildCommand(UGI_FOOTMAN,1,2)", "side(local).moveCommand(1,2)"}) {
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
  CHECK(w.script_cameras[0].mask == 65 && w.script_cameras[0].ticks_left == 2);
  CHECK(w.script_cameras[1].mask == 64 && w.script_cameras[2].mask == 71);
  CHECK(w.messages.size() == 1 && w.messages[0].seconds_left == 4);
  // Fog stamps the owning side even when another side is being viewed.
  sim.tick(w);
  const auto& own = w.visibility.sides[2];
  CHECK(own.v[size_t(13) * own.w + 12] > 32);
  CHECK(w.visibility.sides[0].v[size_t(13) * own.w + 12] == 0);
  r.tick(sim, w); CHECK(w.script_cameras[0].active && w.script_cameras[0].ticks_left == 1);
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
  CHECK(w.script_cameras.size() == 5);
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

int main() {
  rectangle_runtime_categories(); gift_detaches_home_and_outcome_precedence(); tactic_lookup_and_money(); presentation_contracts();
  campaign_outcome_requires_script(); campaign_gates(); town_tactics_and_capture(); embarked_script_identity(); pending_path_uses_previous_latch();
  attacked_notification();
  source_locations(); order_and_enablement(); timers(); conditions_and_identity(); paths_and_gifts(); actions_and_outcomes(); defence_loading(); action_observation_contracts(); replay();
  std::puts("Synthetic script contracts passed.");
}
