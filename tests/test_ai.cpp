#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include "temp_dir.hpp"
#include <memory>
#include <sstream>
#include <string>
#include "engine/ai/planner.hpp"
#include "engine/sim/game_sim.hpp"

using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static void object(World& w, int type, int side, int town, int x, int y, int tactic = -1) {
  Object o; o.id = uint32_t(w.objects.size() + 1); o.type = type; o.side = side; o.town = town; o.tactic = tactic;
  o.teleport(tile_center_fixed(x), tile_center_fixed(y)); w.objects.push_back(o);
}
static World world() {
  World w; w.terrain.w = w.terrain.h = 40; w.terrain.cells.resize(1600);
  w.resources = {40, 40, std::vector<uint16_t>(1600, 0x8080)};
  w.side_state.resize(8); w.town_speed = {1, 1}; w.ai_towns.resize(2);
  for (int side = 0; side < 2; ++side) {
    SideDef sd; sd.index = side; sd.only_comp = true; w.mission.sides.push_back(sd);
    TownDef t; t.index = side; t.side = side; t.type = 1; t.x = t.y = side ? 28 : 8;
    w.mission.towns.push_back(t); w.side_state[side].money = 12000;
    object(w, 11, side, side, t.x, t.y);
    w.visibility.sides.push_back({40, 40, std::vector<uint8_t>(1600, 0)});
  }
  return w;
}
static const Command* issued(const ai::Decision& d, Pct kind) {
  for (const auto& c : d.commands) if (c.type == kind) return &c;
  return nullptr;
}
static void unit(const RulesData& rules) {
  auto w = world(); ai::TownState s;
  auto d = ai::step(w, rules, 0, s, {});
  CHECK(s.phase == 0 && s.elapsed == 0); CHECK(d.next.phase == 10);
  CHECK(d.next.science_timer == 51 && issued(d, Pct::BuildMode)->a == 2);
  w.local_side = 1;
  CHECK(serialize_log({{1, d.commands.front()}}) == serialize_log({{1, ai::step(w, rules, 0, s, {}).commands.front()}}));
  s.spent = 20000; CHECK(ai::build_speed(w, rules, s, 0) == 1);
  s.elapsed = 100; s.spent = 0; CHECK(ai::build_speed(w, rules, s, 0) == 2);
  CHECK(ai::development(w, rules, 0) == 1);
  for (int i = 0; i < 8; ++i) object(w, 16, 0, 0, 2 + i, 2);
  CHECK(ai::development(w, rules, 0) == 9);
  w = world();
  s = {}; s.phase = 7; s.spent = 20000;
  std::vector<ai::Offer> offers = {{12, 240, 12, 8, true}, {13, 150, 13, 8, true}, {18, 500, 14, 8, true}};
  d = ai::step(w, rules, 0, s, offers); CHECK(issued(d, Pct::Build)->a == 12);
  for (int i = 0; i < 3; ++i) object(w, 12, 0, 0, 4 + i, 4);
  d = ai::step(w, rules, 0, s, offers); CHECK(issued(d, Pct::Build)->a == 18);
  offers[2].placeable = false; offers[0].placeable = false;
  d = ai::step(w, rules, 0, s, offers); CHECK(issued(d, Pct::Build)->a == 13);
  for (const auto [race, mine] : {std::pair{1, 48}, std::pair{2, 66}}) {
    w.mission.towns[0].race = race;
    auto economic = ai::step(w, rules, 0, s, {{mine, 200, 13, 8, true}});
    CHECK(issued(economic, Pct::Build) && issued(economic, Pct::Build)->a == mine);
  }
  w.mission.towns[0].race = 0;
  s.phase = 5; d = ai::step(w, rules, 0, s, {{17, 500, 0, 0, false}}); CHECK(!d.next.defence_enabled && d.next.phase == 0);
  s.phase = 18; d = ai::step(w, rules, 0, s, {{17, 500, 15, 15, true}});
  CHECK(d.next.phase == 19 && issued(d, Pct::Build)->a == 17);
  w.objects[0].progress = 500;
  d = ai::step(w, rules, 0, s, offers); CHECK(d.next.phase == 18 && !issued(d, Pct::Build));
  w.objects[0].progress = 1000;
  w.side_state[0].queue.push_back({0, 20, 20, 0, 100});
  d = ai::step(w, rules, 0, s, offers); CHECK(d.next.phase == 18);
  w = world();
  object(w, 0, 0, 0, 10, 8); object(w, 0, 0, 0, 12, 8, 7);
  s = {}; s.phase = 2; s.target_town = 1; s.known_towns = {0, 1}; s.elapsed = 1;
  d = ai::step(w, rules, 0, s, {});
  CHECK(issued(d, Pct::AttackMove) && issued(d, Pct::AttackMove)->ids == std::vector<uint32_t>{3});
  w.visibility.sides[0].v.assign(1600, 152);
  CHECK(!ai::visible(w, 0, 2)); CHECK(ai::intelligence(w, rules, 0).size() == 1);
  s.known_towns.clear();
  d = ai::step(w, rules, 0, s, {}); CHECK(!issued(d, Pct::AttackMove));
  // A new visible attacker redirects moving autonomous troops, preserving script-owned troops.
  w.visibility.sides[0].v.assign(1600, 0);
  object(w, 0, 1, 1, 10, 9); w.objects[0].last_hit_by = 5; w.objects[2].order = uint8_t(Order::Move);
  s = {}; d = ai::step(w, rules, 0, s, {});
  CHECK(issued(d, Pct::Attack) && issued(d, Pct::Attack)->ids == std::vector<uint32_t>{3});
  CHECK(d.next.phase == 10); // A computer attacker can draw defenders but cannot enter the human-threat branch.
  auto again = ai::step(w, rules, 0, d.next, {}); CHECK(!issued(again, Pct::Attack));
  w.relation[0][1] = kFriend;
  CHECK(!issued(ai::step(w, rules, 0, s, {}), Pct::Attack));
  // No arbitrary 80-unit cap: a large idle force can still launch an offensive plan.
  w = world(); for (int i = 0; i < 101; ++i) object(w, 0, 0, 0, 12, 12);
  s = {}; s.phase = 2; s.target_town = 1; s.known_towns = {1}; s.elapsed = 1;
  d = ai::step(w, rules, 0, s, {}); CHECK(issued(d, Pct::AttackMove)->ids.size() == 101);
}
// Busy towns still update speed and dispatch newly available defenders.
static void reinforcements(const RulesData& rules) {
  auto w = world();
  object(w, 0, 0, 0, 9, 8);
  object(w, 0, 1, 1, 10, 8);
  w.objects[0].last_hit_by = 4;
  ai::TownState s;
  auto first = ai::step(w, rules, 0, s, {});
  CHECK(issued(first, Pct::Attack) && issued(first, Pct::Attack)->ids == std::vector<uint32_t>{3});
  w.town_speed[0] = 2;
  s = first.next; s.phase = 22; s.remaining = 4; s.spent = 20000;
  w.objects[2].order = uint8_t(Order::Attack); w.objects[2].target = 4;
  object(w, 0, 0, 0, 9, 9);       // a newly trained autonomous defender
  object(w, 0, 0, 0, 9, 10, 7);   // mission tactic remains under script control
  object(w, 0, 0, 1, 9, 11);      // another town's forces remain independent
  object(w, 0, 0, 0, 9, 12);
  w.objects.back().alive = false;
  w.side_state[0].queue.push_back({0, 20, 20, 0, 100});
  auto d = ai::step(w, rules, 0, s, {});
  CHECK(issued(d, Pct::Attack) && issued(d, Pct::Attack)->ids == std::vector<uint32_t>{5});
  CHECK(d.next.phase == 22 && d.next.remaining == 4);
  CHECK(d.next.science_timer == s.science_timer + 1 && d.next.elapsed == s.elapsed + 1);
  CHECK(issued(d, Pct::BuildMode) && issued(d, Pct::BuildMode)->a == 1);
  w.objects[4].order = uint8_t(Order::Attack); w.objects[4].target = 4;
  CHECK(!issued(ai::step(w, rules, 0, d.next, {}), Pct::Attack));
  // Hidden, dead, allied or departed attackers cannot draw reinforcements.
  w.objects[4].order = uint8_t(Order::Idle);
  w.visibility.sides[0].v.assign(1600, 152);
  CHECK(!issued(ai::step(w, rules, 0, s, {}), Pct::Attack));
  w.visibility.sides[0].v.assign(1600, 0);
  w.objects[3].alive = false;
  CHECK(!issued(ai::step(w, rules, 0, s, {}), Pct::Attack));
  w.objects[3].alive = true; w.relation[0][1] = kFriend;
  CHECK(!issued(ai::step(w, rules, 0, s, {}), Pct::Attack));
  w.relation[0][1] = kEnemy;
  w.objects[3].teleport(tile_center_fixed(16), tile_center_fixed(8));
  CHECK(!issued(ai::step(w, rules, 0, s, {}), Pct::Attack));
}

static void threatened_plan(const RulesData& rules) {
  auto w = world();
  object(w, 0, 0, 0, 9, 8);
  object(w, 0, 1, 1, 10, 8);
  object(w, 0, 1, 1, 10, 9);
  w.objects[0].last_hit_by = 4;
  ai::TownState s; s.phase = 22; s.remaining = 3; s.last_alert = 5; s.spent = 20000;
  s.plan = {{0}, {0}, {0}}; s.plan_generation = 7;
  auto d = ai::step(w, rules, 0, s, {{0, 100, 0, 0, true}, {12, 100, 13, 8, true}});
  // Production remains with the side allocator; the planner must not buy a farm
  // or consume outstanding requests while waiting for the threatened recruits.
  CHECK(d.next.phase == 22 && d.next.remaining == 3);
  CHECK(!issued(d, Pct::Build));
  CHECK(std::equal(d.next.plan.begin(), d.next.plan.end(), s.plan.begin(), s.plan.end(),
      [](const auto& a, const auto& b) { return a.type == b.type && a.producer == b.producer && a.object == b.object; }));
  CHECK(d.next.plan_generation == s.plan_generation);
  CHECK(issued(d, Pct::Attack) && issued(d, Pct::Attack)->a == 4);
  // A changed raider must not discard the remaining threatened production plan.
  w.objects[0].last_hit_by = 5;
  d = ai::step(w, rules, 0, d.next, {{0, 100, 0, 0, true}});
  CHECK(d.next.phase == 22 && d.next.remaining == 3);
  CHECK(std::equal(d.next.plan.begin(), d.next.plan.end(), s.plan.begin(), s.plan.end(),
      [](const auto& a, const auto& b) { return a.type == b.type && a.producer == b.producer && a.object == b.object; }));
  CHECK(d.next.plan_generation == s.plan_generation);
  CHECK(!issued(d, Pct::Build));
  s = d.next; s.remaining = 0;
  d = ai::step(w, rules, 0, s, {{12, 100, 13, 8, true}});
  CHECK(d.next.phase == 20 && d.next.plan.empty() && !issued(d, Pct::Build));
  // Normal local-plan progress retains its economic detour.
  s.phase = 14; s.remaining = 3;
  d = ai::step(w, rules, 0, s, {{12, 100, 13, 8, true}});
  CHECK(d.next.phase == 14 && d.next.remaining == 3);
  CHECK(issued(d, Pct::Build) && issued(d, Pct::Build)->a == 12);
  w.objects[3].alive = w.objects[4].alive = false;
  s = d.next; s.phase = 16;
  d = ai::step(w, rules, 0, s, {});
  CHECK(d.next.phase == 0 && d.next.remaining == 0 && d.next.last_alert == 0);
  s = {}; s.phase = 9; s.elapsed = 1; s.known_towns = {1};
  w.mission.towns[1].type = 0;
  CHECK(ai::step(w, rules, 0, s, {}).next.phase == 1);
  w.relation[0][1] = kNeutral;
  CHECK(ai::step(w, rules, 0, s, {}).next.phase == 1);
  w.relation[0][1] = kFriend;
  CHECK(ai::step(w, rules, 0, s, {}).next.phase == 11);
  w.relation[0][1] = kEnemy;
  w.mission.towns[0].type = 0; w.mission.towns[1].type = 1;
  CHECK(ai::step(w, rules, 0, s, {}).next.phase == 11);
}

// Invented worlds exercise complete simulation orders, construction and replay.
// Their timings and composition are G engine policies, not original measurements.
static void army_scenarios(std::shared_ptr<const RulesData> rules) {
  auto initial = world();
  initial.resources.v.assign(1600, 128); // clear mineral deposits for ordinary building placement
  initial.mission.sides[1].only_comp = false;
  for (auto& o : initial.objects) { o.town_hall = true; o.hp = 1000000; }
  for (int type = 0; type < 90; ++type)
    if (type != 0 && type != 11) initial.mission.sides[0].disabled.push_back(type);
  auto wave_initial = initial;
  // The intelligence-driven planner sizes the wave from known enemy membership.
  // Give it several defenders to counter while keeping the source army empty.
  for (int x = 27; x <= 29; ++x) object(wave_initial, 0, 1, 1, x, 29, 7);
  auto w = wave_initial;
  GameSim wave(rules, Modifications::faithful()); wave.init(w);
  int productions = 0, launches = 0, released = 0;
  wave.set_ai_observer([&](uint64_t, const AiEvent& event) {
    if (event.side != 0) return;
    if (std::string(event.kind) == "production") ++productions;
    if (std::string(event.kind) == "launch") { ++launches; released += event.composition[0]; }
  });
  while (!launches && wave.tick_count() < 4000) { w.begin_tick(); wave.tick(w); }
  std::printf("Army scenario: %d productions, %d launches, %d released recruits\n", productions, launches, released);
  CHECK(productions > 1 && launches == 1 && released > 1);
  auto restored = wave_initial;
  auto replay = GameSim::replay(restored, rules, wave.log(), wave.tick_count());
  CHECK(replay->state_hash(restored) == wave.state_hash(w));

  auto raid_initial = initial;
  object(raid_initial, 0, 0, 0, 11, 10);
  // The attacked condition requires the source to belong to an active tactic.
  object(raid_initial, 0, 1, 1, 9, 8, 7);
  // Halls are protected by their descriptor; provoke defence at an ordinary building.
  object(raid_initial, 14, 0, 0, 9, 9, 77);
  raid_initial.objects.back().hp = 1000000;
  TempDir raid_directory;
  const auto raid_script = raid_directory.path() / "raid-notification.scn";
  std::ofstream(raid_script) << "trigger(0) conditions side(0).tactic(77).attacked == 1 actions side(0).addMoney(7) trigger(0).disable end";
  w = raid_initial;
  GameSim raid(rules, Modifications::faithful());
  CHECK(raid.attach_script(raid_script.string())); raid.init(w);
  int defenders = 0;
  raid.set_ai_observer([&](uint64_t, const AiEvent& event) {
    if (event.side == 0 && std::string(event.kind) == "defence") defenders += event.composition[0];
  });
  Command attack; attack.type = Pct::Attack; attack.side = 1; attack.ids = {4}; attack.a = 5;
  raid.submit(attack);
  for (int tick = 0; tick < 100 && !defenders; ++tick) { w.begin_tick(); raid.tick(w); }
  CHECK(w.objects[0].hp == 1000000 && w.objects[4].hp < 1000000 && defenders > 0);
  CHECK(w.objects[2].order == uint8_t(Order::Attack) && w.objects[2].target == 4);
  for (int i = 0; i < 2; ++i) { w.begin_tick(); raid.tick(w); }
  CHECK(!raid.script().trigger_enabled(0));
  restored = raid_initial;
  replay = GameSim::replay(restored, rules, raid.log(), raid.tick_count(), raid_script.string());
  CHECK(replay->state_hash(restored) == raid.state_hash(w));

  auto rebuild_initial = initial;
  rebuild_initial.mission.sides[0].disabled.clear();
  for (int type = 0; type < 90; ++type)
    if (type != 0 && type != 11 && type != 14) rebuild_initial.mission.sides[0].disabled.push_back(type);
  object(rebuild_initial, 14, 0, 0, 13, 8, 22);
  TempDir directory;
  const auto script = directory.path() / "destroy-building.scn";
  std::ofstream(script) << "trigger(0) conditions localTime == 1 actions side(0).tactic(22).kill trigger(0).disable end";
  w = rebuild_initial;
  GameSim rebuild(rules, Modifications::faithful());
  CHECK(rebuild.attach_script(script.string())); rebuild.init(w);
  bool replaced = false;
  for (int tick = 0; tick < 5000 && !replaced; ++tick) {
    w.begin_tick(); rebuild.tick(w);
    for (const auto& o : w.objects)
      if (o.id != 3 && o.type == 14 && o.side == 0 && o.town == 0 && o.alive && o.progress == 1000) replaced = true;
  }
  CHECK(!w.objects[2].alive && replaced);
  CHECK(ai::development(w, *rules, 0) == ai::development(rebuild_initial, *rules, 0));
  restored = rebuild_initial;
  replay = GameSim::replay(restored, rules, rebuild.log(), rebuild.tick_count(), script.string());
  CHECK(replay->state_hash(restored) == rebuild.state_hash(w));
}
// Initialization must provide both planner state and town-command hall identities.
static void neutral_computer(std::shared_ptr<const RulesData> rules) {
  auto initial = world();
  initial.mission.sides[0].neutral = true;
  initial.relation[0][1] = initial.relation[1][0] = kNeutral;
  initial.mission.sides[1].only_comp = false;
  initial.objects[0].town_hall = initial.objects[1].town_hall = true;
  World w = initial;
  GameSim sim(rules, Modifications::faithful()); sim.init(w);
  for (int tick = 1; tick <= 12; ++tick) {
    w.begin_tick(); sim.tick(w);
    if (tick == 5) {
      CHECK(w.side_state[0].queue.size() == 1);
      CHECK(w.side_state[0].money == 12000); // enqueue does not charge
    }
    if (tick >= 6) CHECK(w.side_state[0].money == 11840);
    CHECK(w.side_state[1].money == 12000);
  }
  auto replay = GameSim::replay(initial, rules, sim.log(), sim.tick_count());
  CHECK(replay->state_hash(initial) == sim.state_hash(w));
}

static void initialization(std::shared_ptr<const RulesData> rules) {
  auto w = world();
  w.mission.start_time = -1;
  w.mission.sides[1].only_comp = false;
  w.objects[0].town_hall = w.objects[1].town_hall = true;
  w.side_state[0].selected_tactic = 7;
  GameSim sim(rules, Modifications::faithful()); sim.init(w);
  CHECK(sim.tick_count() == 0 && w.day_clock == kDayTicks - 1);
  CHECK(w.ai_towns.size() == 2 && w.town_state.size() == 2);
  CHECK(w.town_state[0].hall == 1 && w.town_state[1].hall == 2);
  CHECK(w.side_state[0].selected_tactic == -1);
  CHECK(w.visibility.sides[0].v == std::vector<uint8_t>(1600, 24));
  CHECK(w.visibility.sides[1].v == std::vector<uint8_t>(1600, 0));
}
// Object membership uses mission town IDs; queues and planner state use vector positions.
static void sparse_towns(std::shared_ptr<const RulesData> rules) {
  auto w = world();
  // Farm production needs agricultural ground, not a map filled with mineral deposits.
  w.resources.v.assign(1600, 128);
  for (int ti = 0; ti < 2; ++ti) {
    w.mission.towns[ti].index = 4 + ti * 5;
    w.objects[ti].town = w.mission.towns[ti].index;
    w.objects[ti].town_hall = true;
  }
  object(w, 0, 0, 4, 10, 8);
  CHECK(ai::development(w, *rules, 0) == 1);
  auto intel = ai::intelligence(w, *rules, 0);
  CHECK(intel.size() == 2 && intel[0].town == 0 && intel[1].town == 1);
  CHECK(intel[0].counts[11] == 1 && intel[1].counts[11] == 1);
  ai::TownState attack; attack.phase = 2; attack.target_town = 1;
  attack.known_towns = {1}; attack.elapsed = 1;
  auto decision = ai::step(w, *rules, 0, attack, {});
  CHECK(issued(decision, Pct::AttackMove) && issued(decision, Pct::AttackMove)->ids == std::vector<uint32_t>{3});

  w.mission.sides[1].only_comp = false;
  GameSim sim(rules, Modifications::faithful()); sim.init(w);
  std::vector<std::pair<uint64_t, AiEvent>> events;
  sim.set_ai_observer([&](uint64_t tick, const AiEvent& e) { events.emplace_back(tick, e); });
  w.ai_towns[0].phase = 7; w.ai_towns[0].spent = 20000;
  w.begin_tick(); sim.tick(w);
  CHECK(w.side_state[0].queue.size() == 1);
  if (w.side_state[0].queue.empty()) return;
  auto& q = w.side_state[0].queue.front();
  CHECK(q.type == 12 && q.town == 0);
  CHECK(w.ai_towns[0].spent == 20000 && q.paid == 0);
  CHECK(events.size() == 1 && events[0].first == 1);
  CHECK(std::string(events[0].second.kind) == "build_order");
  CHECK(events[0].second.town == 0 && events[0].second.composition[12] == 1);
  CHECK(events[0].second.x == q.tile_x && events[0].second.y == q.tile_y);
  auto phase = w.ai_towns[0].phase;
  w.begin_tick(); sim.tick(w);
  CHECK(w.ai_towns[0].phase == phase); // queued construction suspends the planner
  CHECK(w.ai_towns[0].spent > 20000 && w.ai_towns[1].spent == 0);
  CHECK(events.size() == 2 && events[1].first == 2);
  CHECK(std::string(events[1].second.kind) == "production");
  q.remaining = 1;
  // Building work is its live HP, so prepare the final increment explicitly.
  w.find_mut(q.object)->hp = w.find(q.object)->hp_max - q.work_rate;
  w.begin_tick(); sim.tick(w);
  CHECK(w.side_state[0].queue.empty());
  CHECK(w.objects.back().type == 12 && w.objects.back().town == 4);
  // Disable new planning while measuring harvest in isolation.
  w.mission.sides[0].only_comp = false;
  const auto before = w.ai_towns;
  const auto money = w.side_state;
  for (int i = 0; i < kDayTicks; ++i) { w.begin_tick(); sim.tick(w); }
  for (int ti = 0; ti < 2; ++ti) {
    auto gain = w.ai_towns[ti].harvested - before[ti].harvested;
    CHECK(gain > 0 && gain == w.side_state[ti].money - money[ti].money);
  }
  auto hash = sim.state_hash(w);
  ++w.ai_towns[0].spent; CHECK(sim.state_hash(w) != hash); --w.ai_towns[0].spent;
  ++w.mission.towns[0].type; CHECK(sim.state_hash(w) != hash);
}
// Diagnostics must observe decisions without changing viewing-side or replay state.
static void observation(std::shared_ptr<const RulesData> rules) {
  auto a = world();
  a.mission.sides[1].only_comp = false;
  object(a, 0, 0, 0, 10, 8);
  object(a, 0, 0, 0, 11, 8, 7); // Script-owned troops do not join a planner launch.
  auto b = a;
  GameSim plain(rules, Modifications::faithful()), traced(rules, Modifications::faithful()); plain.init(a); traced.init(b);
  a.ai_towns[0].phase = b.ai_towns[0].phase = 2;
  a.ai_towns[0].known_towns = b.ai_towns[0].known_towns = {1};
  a.ai_towns[0].elapsed = b.ai_towns[0].elapsed = 1;
  std::vector<AiEvent> events;
  std::ostringstream json;
  traced.set_ai_observer([&](uint64_t tick, const AiEvent& event) {
    events.push_back(event); write_ai_event(json, tick, event);
  });
  a.begin_tick(); b.begin_tick(); plain.tick(a); traced.tick(b);
  CHECK(plain.state_hash(a) == traced.state_hash(b));
  CHECK(events.size() == 1 && std::string(events[0].kind) == "launch");
  CHECK(events[0].composition[0] == 1 && events[0].x == -1);
  CHECK(json.str().find("\"composition\":[[0,1]]") != std::string::npos);
  // A visible attack redirects exactly the autonomous troop while production is busy.
  object(a, 0, 1, 1, 10, 9); object(b, 0, 1, 1, 10, 9);
  a.objects[0].last_hit_by = b.objects[0].last_hit_by = 5;
  a.side_state[0].queue.push_back({0, 100, 100, 0, 0});
  b.side_state[0].queue.push_back({0, 100, 100, 0, 0});
  a.begin_tick(); b.begin_tick(); plain.tick(a); traced.tick(b);
  CHECK(plain.state_hash(a) == traced.state_hash(b));
  CHECK(std::string(events.back().kind) == "defence" && events.back().composition[0] == 1);
}
// Two towns sharing cash select their own speeds before pricing their orders.
static void town_orders(std::shared_ptr<const RulesData> rules) {
  auto w = world();
  w.resources.v.assign(1600, 128); // farm sites must be valid before payment starts
  w.mission.towns[1].side = w.objects[1].side = 0;
  w.mission.sides[1].only_comp = false;
  w.relation[0][1] = kFriend;
  GameSim sim(rules, Modifications::faithful()); sim.init(w);
  w.town_speed[0] = 2;
  w.ai_towns[0].phase = w.ai_towns[1].phase = 7;
  w.ai_towns[0].spent = 20000;
  w.begin_tick(); sim.tick(w);
  CHECK(w.town_speed[0] == 1 && w.town_speed[1] == 2);
  CHECK(w.side_state[0].queue.size() == 2);
  if (w.side_state[0].queue.size() != 2) return;
  CHECK(w.side_state[0].queue[0].paid == 0 && w.side_state[0].queue[1].paid == 0);
  w.begin_tick(); sim.tick(w); // charge when production starts, using each town's selected speed
  for (int ti = 0; ti < 2; ++ti) {
    const auto& q = w.side_state[0].queue[size_t(1 - ti)];
    CHECK(q.town == ti && q.type == 12);
    CHECK(q.paid == production_value(rules->type(12).price, 1, ti + 1, true, false, false));
    CHECK(w.ai_towns[ti].phase == 8);
  }
  // Speed comparison includes equality in the fast branch and keeps updating
  // while production suspends state transitions.
  w.side_state[0].money = 0;
  w.ai_towns[0].spent = 100;
  w.side_state[0].money = 400;
  w.ai_towns[0].elapsed = 0;
  CHECK(ai::build_speed(w, *rules, w.ai_towns[0], 0) == 2);
  --w.side_state[0].money;
  CHECK(ai::build_speed(w, *rules, w.ai_towns[0], 0) == 1);
  w.begin_tick(); sim.tick(w);
  CHECK(w.town_speed[0] == 1 && w.ai_towns[0].phase == 8);
}
static void enemy_price_mask(std::shared_ptr<const RulesData> rules) {
  TempDir directory;
  const auto path = directory.path() / "diplomacy.scn";
  std::ofstream(path) << "trigger(0) conditions localTime == 1 actions side(0).setFriendlyTo(1) end\n"
                     << "trigger(1) conditions localTime == 2 actions side(0).setEnemyTo(1) end\n";
  auto w = world();
  for (auto& side : w.mission.sides) side.only_comp = false;
  w.side_state[0].enemy_mask = UINT32_MAX;
  GameSim sim(rules, Modifications::faithful()); CHECK(sim.attach_script(path.string())); sim.init(w);
  CHECK(w.enemy_all(0));
  w.begin_tick(); sim.tick(w); CHECK(w.enemy_all(0));
  w.begin_tick(); sim.tick(w); CHECK(!w.enemy_all(0));
  w.begin_tick(); sim.tick(w); CHECK(w.enemy_all(0));
}

static void remembered_intelligence(const RulesData& rules) {
  auto w = world();
  object(w, 0, 1, 1, 26, 26);
  w.visibility.sides[0].v.assign(1600, 152);
  w.visibility.sides[0].v[28 * 40 + 28] = 0;
  const auto records = ai::intelligence(w, rules, 0);
  const auto enemy = std::find_if(records.begin(), records.end(), [](const auto& r) { return r.town == 1; });
  CHECK(enemy != records.end() && enemy->counts[0] == 1); // one visible hall reveals the town's membership
  w.visibility.sides[0].v.assign(1600, 152);
  ai::TownState state; state.elapsed = 10; state.phase = 1; state.target_town = 1;
  ai::Intelligence remembered; remembered.town = 1; remembered.counts[11] = 1;
  state.knowledge = {remembered}; state.known_towns = {1};
  const auto decision = ai::step(w, rules, 0, state, {});
  CHECK(decision.next.phase == 2 && !decision.next.plan.empty());
  CHECK(std::any_of(decision.next.knowledge.begin(), decision.next.knowledge.end(),
      [](const auto& r) { return r.town == 1 && r.counts[11] == 1; }));
  w.mission.towns[0].type = 0; state.phase = 9;
  CHECK(ai::step(w, rules, 0, state, {}).next.phase == 11); // an outpost does not create an offensive plan
}

// A production plan can reserve work in an outpost without a local force request.
static void plan_allocation(std::shared_ptr<const RulesData> rules) {
  auto w = world();
  w.mission.towns[1].side = w.objects[1].side = 0;
  w.mission.towns[1].type = 0;
  w.mission.sides[1].only_comp = false;
  for (auto& o : w.objects) o.town_hall = true;
  w.side_state[0].money = 0;
  w.side_state[0].enemy_mask = UINT32_MAX;
  GameSim sim(rules, Modifications::faithful()); sim.init(w);
  w.ai_towns[0].phase = 14; w.ai_towns[0].remaining = 2;
  w.ai_towns[0].plan = {{0}, {0}};
  w.ai_towns[1].phase = 16; // The newer unthreatened outpost reaches idle before the older planner runs.
  w.begin_tick(); sim.tick(w);
  CHECK(w.side_state[0].queue.size() == 2);
  if (w.side_state[0].queue.size() != 2) return;
  CHECK(w.ai_towns[1].phase == 0); // its queued plan request suspends local strategy
  CHECK(w.ai_towns[0].plan[0].producer == 1 && w.ai_towns[0].plan[1].producer == 0);
  CHECK(w.side_state[0].queue[1].plan_town == 0 && w.side_state[0].queue[1].plan_entry == 0);
  for (int tick = 0; tick < 3; ++tick) { w.begin_tick(); sim.tick(w); }
  CHECK(!w.side_state[0].queue[1].started && w.side_state[0].money == 0);
  w.side_state[0].money = 500;
  w.begin_tick(); sim.tick(w);
  CHECK(w.side_state[0].queue[1].started);
  CHECK(w.side_state[0].queue[1].paid == rules->type(0).price); // outpost uses its base price
  auto& q = w.side_state[0].queue[1];
  q.remaining = 1; q.work_rate = rules->type(0).hp_max;
  w.begin_tick(); sim.tick(w);
  const uint32_t recruit = w.ai_towns[0].plan[0].object;
  CHECK(recruit != 0 && w.find(recruit) && w.find(recruit)->town == w.mission.towns[1].index);
  CHECK(w.ai_towns[0].plan[1].object == 0); // completed work fills its owning slot
  if (!recruit) return;
  w.find_mut(recruit)->alive = false;
  w.begin_tick(); sim.tick(w);
  CHECK(w.ai_towns[0].plan[0].object == 0);
  CHECK(w.side_state[0].queue.size() == 1 && w.ai_towns[0].plan[0].producer == -1);
  // A cancelled request is made available again without a second live reservation.
  w.ai_towns[1].phase = 16; w.ai_towns[0].spent = 0; // reaches idle before the older planner
  Command cancel; cancel.type = Pct::QueueKill; cancel.side = 0; cancel.a = 0;
  sim.submit(cancel); w.begin_tick(); sim.tick(w);
  CHECK(w.side_state[0].queue.size() == 2);
  CHECK(w.side_state[0].queue[1].plan_entry == 0 && !w.side_state[0].queue[1].started);
  // Old work cannot fill a replacement plan that reused the same slot number.
  w.begin_tick(); sim.tick(w);
  w.side_state[0].queue[1].remaining = 1;
  ++w.ai_towns[0].plan_generation;
  w.ai_towns[0].phase = 11;
  w.begin_tick(); sim.tick(w);
  CHECK(w.ai_towns[0].plan[0].object == 0);
  const uint64_t hash = sim.state_hash(w);
  ++w.ai_towns[0].random; CHECK(sim.state_hash(w) != hash); --w.ai_towns[0].random;
  w.side_state[0].enemy_mask = 0; CHECK(sim.state_hash(w) != hash);
}

static void scoring(const RulesData& rules) {
  auto w = world();
  w.mission.sides[1].only_comp = false;
  ai::Intelligence record; record.town = 1;
  const int distance = int(combat_distance(20, 20) * 256 / combat_distance(40, 40));
  CHECK(ai::target_score(w, rules, 0, record) == distance);
  int max_hp = 1;
  for (int t = 0; t < 90; ++t) max_hp = std::max(max_hp, rules.type(t).hp_max);
  const int weight = int(int64_t(rules.type(0).hp_max) * 256 / max_hp);
  record.counts[0] = 256;
  CHECK(ai::target_score(w, rules, 0, record) == distance + weight / 2 + 128);
  record.counts[0] = 512;
  CHECK(ai::target_score(w, rules, 0, record) == distance + weight / 2 + 128);
  record.counts[11] = 1000; // Ordinary buildings do not enter the mean unit weight/count.
  CHECK(ai::target_score(w, rules, 0, record) == distance + weight / 2 + 128);
  record.counts[0] = 2; record.counts[1] = 2;
  const int other = int(int64_t(rules.type(1).hp_max) * 256 / max_hp);
  const int force = (weight + other) / 2 / 2 + 2;
  CHECK(ai::target_score(w, rules, 0, record) == distance + force);
  w.mission.sides[1].only_comp = true;
  CHECK(ai::target_score(w, rules, 0, record) == distance * 3 + force);
  w.side_state[1].enemy_mask = UINT32_MAX;
  CHECK(ai::target_score(w, rules, 0, record) == distance + force);
  w.mission.towns[1].x = w.mission.towns[0].x; w.mission.towns[1].y = w.mission.towns[0].y;
  CHECK(ai::target_score(w, rules, 0, record) == force); // No additive computer penalty.
  record.town = -1; CHECK(ai::target_score(w, rules, 0, record) == 0x7fff00);
  // Selection must reconsider a valid target every update, including through fog.
  w = world();
  TownDef third = w.mission.towns[1]; third.index = 2; third.x = 10; third.y = 8;
  w.mission.towns.push_back(third); object(w, 11, 1, 2, 10, 8);
  w.visibility.sides[0].v.assign(1600, 152);
  ai::TownState s; s.elapsed = 1; s.target_town = 1; s.known_towns = {1, 2};
  ai::Intelligence far; far.town = 1;
  ai::Intelligence close; close.town = 2;
  s.knowledge = {far, close};
  CHECK(ai::step(w, rules, 0, s, {}).next.target_town == 2);
  // Rescoring the town's next target must not redirect a plan already in flight.
  object(w, 0, 0, 0, 9, 8);
  s.phase = 2; s.plan_target_town = 1;
  auto launch = ai::step(w, rules, 0, s, {});
  CHECK(launch.next.target_town == 2 && issued(launch, Pct::AttackMove));
  CHECK(issued(launch, Pct::AttackMove) && issued(launch, Pct::AttackMove)->a == 28 * kTilePx + kTilePx / 2);
  s.phase = 0;
  // A sufficiently large force can make the nearby town less attractive.
  s.knowledge[1].counts[0] = 256;
  w.mission.towns[1].x = 12; w.mission.towns[1].y = 8;
  CHECK(ai::step(w, rules, 0, s, {}).next.target_town == 1);
  w.relation[0][1] = kFriend;
  CHECK(ai::step(w, rules, 0, s, {}).next.target_town == -1);
}

// Invented descriptors and build recipes isolate arithmetic from private game data.
static RulesData plan_rules(const std::filesystem::path& dir, int level, bool blocked = false) {
  std::ofstream types(dir / "types.csv");
  types << "id,ugi,kind,param,unit_price,build_time,dev_level\n";
  for (int type = 0; type < 90; ++type)
    types << type << ",TEST,Marine," << type << ",100,100,"
          << (type == 11 ? level : type == 0 || type == 1 || type == 33 ? 0 : 10) << '\n';
  types.close();
  std::ofstream(dir / "desc_marine.csv") <<
      "param,d0,d1,d2,d3,d4,d5,d6,d7\n"
      "0,0,30000,0,0,30000,8,0,1\n"
      "1,0,15000,0,0,15000,8,0,1\n"
      "2,0,30000,0,0,30000,8,0,1\n";
  std::ofstream(dir / "weapons_ids.csv") << "weapon_id,class,param\n1,13,0\n";
  std::ofstream(dir / "weapon_class_w2_13.csv") <<
      "param,d0,d1,d2,d3,d4,d5,d6,d7,d8,d9\n"
      "0,1,30000,0,0,100,256,256,256,256,256\n";
  std::ofstream tree(dir / "buildtree.csv");
  tree << "race,townhall_type,slot,item_ugi,flagB_construct,prereq_ugi(A=flag 0x8000000)\n";
  for (int type : {0, 1, 33}) tree << "0,1," << type << ',' << type << ",0," << (blocked ? "14" : "") << '\n';
  tree.close();
  RulesData rules; CHECK(rules.load_dir(dir.string())); return rules;
}

static void composition() {
  TempDir directory;
  auto rules = plan_rules(directory.path(), 9);
  auto w = world();
  ai::Intelligence enemy; enemy.town = 1; enemy.counts[0] = 30;
  // The synthetic weapon kills in one tick: M=254, with W[0]=256 and W[1]=128.
  CHECK(ai::matchup(rules, 0, 0) == 254 && ai::matchup(rules, 1, 0) == 254);
  uint32_t seed = 1;
  auto draw = [&]() { seed = seed * 214013u + 2531011u; return int((seed >> 16) & 32767); };
  // Type 0 always beats type 1 even after the full 0..63 perturbation.
  // Two draws per pick, one for each eligible new candidate; type 33 consumes none.
  int expected = 0;
  while (expected < 30) {
    draw(); draw(); const int batch = draw() % 10 + 1;
    for (int i = 0; i < batch && (2 * 30 - expected > 0 || expected < 30); ++i) ++expected;
  }
  uint32_t random = 1;
  auto plan = ai::make_plan(w, rules, 0, 1, false, random, {enemy});
  CHECK(int(plan.size()) == expected && random == seed);
  CHECK(std::all_of(plan.begin(), plan.end(), [](const auto& e) { return e.type == 0 && !e.object; }));
  for (uint32_t initial = 1; initial <= 32; ++initial) {
    random = initial;
    const auto weighted = ai::make_plan(w, rules, 0, 1, false, random, {enemy});
    CHECK(!weighted.empty() && weighted.front().type == 0);
  }
  enemy.counts[0] = 1; random = 1;
  plan = ai::make_plan(w, rules, 0, 1, false, random, {enemy});
  CHECK(plan.size() == 2); // Need is twice the enemy count; a five-entry batch stops at zero.
  enemy.counts = {}; enemy.counts[2] = 2; enemy.counts[1] = 1; enemy.counts[0] = 100;
  random = 1; plan = ai::make_plan(w, rules, 0, 1, false, random, {enemy});
  // Four picks against type 2 already cover type 1's need; it consumes candidate
  // draws but no batch draw. The remaining insertion sequence is deterministic.
  CHECK(plan.size() == 105 && random == 0x7f965a34u);
  enemy.counts = {}; enemy.counts[0] = 30;
  // Side maximum development controls batches even when the source is undeveloped.
  rules = plan_rules(directory.path(), 0);
  w.mission.towns[1].side = w.objects[1].side = 0;
  object(w, 14, 0, 1, 29, 28); // synthetic dev weight 10, clamped to 9
  random = 1;
  plan = ai::make_plan(w, rules, 0, 1, false, random, {enemy});
  CHECK(int(plan.size()) == expected && random == seed);
  w = world(); random = 1;
  plan = ai::make_plan(w, rules, 0, 1, false, random, {enemy});
  CHECK(plan.size() == 30); // Development zero makes every batch exactly one.
  // Need subtracts every prior pick, even across distinct enemy types.
  enemy.counts = {}; enemy.counts[1] = 1; enemy.counts[0] = 1;
  random = 1; plan = ai::make_plan(w, rules, 0, 1, false, random, {enemy});
  CHECK(plan.size() == 2);
  enemy.counts = {}; enemy.counts[0] = 1000;
  random = 1; plan = ai::make_plan(w, rules, 0, 1, false, random, {enemy});
  CHECK(plan.size() == 256);
  enemy.counts[33] = 1000; random = 1;
  CHECK(ai::make_plan(w, rules, 0, 1, false, random, {enemy}).size() == 256);
  enemy.counts[33] = 0;
  // Reuse is capped at 32 per type, has no score noise and must be reconsidered
  // when exhausted; an unbuildable reused type cannot turn into a new purchase.
  rules = plan_rules(directory.path(), 0, true);
  for (int n = 0; n < 40; ++n) object(w, 0, 0, 0, 9, 8);
  int wanted = 90; random = 1;
  plan = ai::make_plan(w, rules, 0, 1, true, random, {enemy}, &wanted);
  CHECK(plan.size() == 32);
  CHECK(std::all_of(plan.begin(), plan.end(), [](const auto& e) { return e.type == 0 && e.object; }));
  for (int n = 0; n < 40; ++n) object(w, 1, 0, 0, 9, 8);
  random = 1; plan = ai::make_plan(w, rules, 0, 1, true, random, {enemy});
  CHECK(plan.size() == 64);
  CHECK(std::count_if(plan.begin(), plan.end(), [](const auto& e) { return e.type == 1 && e.object; }) == 32);
  random = 1;
  CHECK(ai::make_plan(w, rules, 0, 1, false, random, {enemy}).empty()); // town troops require mode 1
  for (auto& o : w.objects) if (o.type == 0 || o.type == 1) o.town = -1;
  random = 1;
  CHECK(ai::make_plan(w, rules, 0, 1, false, random, {enemy}).size() == 64); // detached autonomous tactics work in either mode
  for (auto& o : w.objects) if (o.type == 0 || o.type == 1) o.tactic = -2;
  random = 1; CHECK(ai::make_plan(w, rules, 0, 1, true, random, {enemy}).empty());
  for (auto& o : w.objects) if (o.type == 0 || o.type == 1) {
    o.tactic = -1; o.teleport(tile_center_fixed(34), tile_center_fixed(8));
  }
  random = 1; CHECK(ai::make_plan(w, rules, 0, 1, true, random, {enemy}).empty());
  for (auto& o : w.objects) if (o.type == 0 || o.type == 1) o.teleport(tile_center_fixed(33), tile_center_fixed(8));
  random = 1; CHECK(ai::make_plan(w, rules, 0, 1, true, random, {enemy}).size() == 64);
  w.side_state[0].enemy_mask = UINT32_MAX; random = 1;
  CHECK(ai::make_plan(w, rules, 0, 1, true, random, {enemy}).empty());
  rules = plan_rules(directory.path(), 9, true);
  w = world();
  for (int n = 0; n < 40; ++n) object(w, 0, 0, 0, 9, 8);
  for (uint32_t initial = 1; initial <= 8; ++initial) {
    random = initial;
    plan = ai::make_plan(w, rules, 0, 1, true, random, {enemy});
    CHECK(plan.size() == 32); // A batch crossing the reuse boundary cannot buy the unbuildable counter.
    CHECK(std::all_of(plan.begin(), plan.end(), [](const auto& e) { return e.object != 0; }));
  }
  rules = plan_rules(directory.path(), 0, true);
  // The original wanted-score bug is observable: weaker type 1 overwrites type 0.
  // Correcting its score bar would leave type 0 (254+noise versus 127+noise).
  w = world(); random = 1; wanted = 90;
  plan = ai::make_plan(w, rules, 0, 1, false, random, {enemy}, &wanted);
  CHECK(plan.empty() && wanted == 1);
  object(w, 33, 0, 0, 9, 8); ai::Intelligence building; building.town = 1; building.counts[11] = 10;
  random = 1; CHECK(ai::make_plan(w, rules, 0, 1, true, random, {building}).empty()); // Type 33 is excluded even when reusable.
  ai::TownState s; s.phase = 10; s.wanted_type = wanted;
  CHECK(ai::step(w, rules, 0, s, {}).next.phase == 3);
  s.wanted_type = 90;
  CHECK(ai::step(w, rules, 0, s, {}).next.phase == 13);
  // Airborne targets get the extra pass, still bounded by plan capacity.
  rules = plan_rules(directory.path(), 0); enemy.counts = {}; enemy.counts[33] = 3; random = 1;
  plan = ai::make_plan(w, rules, 0, 1, false, random, {enemy});
  CHECK(plan.size() == 3); // M[33][best]=0 suppresses the main pass, not the extra airborne pass.
  CHECK(std::none_of(plan.begin(), plan.end(), [](const auto& e) { return e.type == 33; }));
  // Equal weights isolate random scoring and the strict new-candidate tie rule.
  std::ofstream(directory.path() / "desc_marine.csv") <<
      "param,d0,d1,d2,d3,d4,d5,d6,d7\n"
      "0,0,30000,0,0,30000,8,0,1\n"
      "1,0,30000,0,0,30000,8,0,1\n";
  CHECK(rules.load_dir(directory.path().string()));
  enemy.counts = {}; enemy.counts[0] = 1;
  bool tie = false, first = false, second = false;
  for (uint32_t initial = 1; initial <= 1024 && !(tie && first && second); ++initial) {
    seed = initial; const int a = draw() & 0x3f, b = draw() & 0x3f; draw();
    random = initial; plan = ai::make_plan(w, rules, 0, 1, false, random, {enemy});
    CHECK(plan.size() == 1 && plan[0].type == (b > a ? 1 : 0) && random == seed);
    tie |= a == b; first |= a > b; second |= b > a;
  }
  CHECK(tie && first && second);
}

static void threat_rules(const RulesData& rules) {
  auto w = world(); w.mission.sides[1].only_comp = false;
  object(w, 0, 1, 1, 16, 8); // Exactly eight tiles, with no attack or visibility requirement.
  w.visibility.sides[0].v.assign(1600, 152);
  object(w, 0, 0, 1, 25, 25);
  object(w, 0, 0, 0, 9, 8, 7); // Explicit script tactic is preserved by the G recall adapter.
  ai::TownState s; s.phase = 2; s.remaining = 3; s.plan = {{0}, {0}, {0}};
  w.side_state[0].queue = {{0, 100, 100, 0, 0}, {0, 100, 100, 1, 0}, {1, 100, 100, 0, 0}};
  auto d = ai::step(w, rules, 0, s, {});
  CHECK(d.next.phase == 21 && d.next.plan.empty() && d.next.remaining == 0);
  std::vector<int> cancelled;
  for (const auto& c : d.commands) if (c.type == Pct::QueueKill) cancelled.push_back(c.a);
  CHECK(cancelled == std::vector<int>({2, 0}));
  CHECK(issued(d, Pct::Move) && issued(d, Pct::Move)->ids == std::vector<uint32_t>{4});
  auto reject = [&]() { CHECK(ai::step(w, rules, 0, s, {}).next.phase == 2); };
  w.objects[2].teleport(tile_center_fixed(16), tile_center_fixed(9)); reject(); // Outside the circle.
  w.objects[2].teleport(tile_center_fixed(16), tile_center_fixed(8));
  w.relation[0][1] = kFriend; reject(); w.relation[0][1] = kEnemy;
  w.mission.sides[1].only_comp = true; reject(); w.mission.sides[1].only_comp = false;
  w.objects[2].alive = false; reject(); w.objects[2].alive = true;
  w.objects[2].removed = true; reject(); w.objects[2].removed = false;
  w.mission.sides[0].only_comp = false; reject(); w.mission.sides[0].only_comp = true;
  w.mission.towns[0].type = 0; reject();
  w.objects[2].teleport(tile_center_fixed(12), tile_center_fixed(8));
  CHECK(ai::step(w, rules, 0, s, {}).next.phase == 21); // Outpost boundary is four tiles.
  w.objects[2].teleport(tile_center_fixed(12), tile_center_fixed(9)); reject();
  // Phase 16 exit uses eight tiles even for an outpost, and includes allies/computers.
  w.side_state[0].queue.clear(); s.phase = 16;
  w.objects[2].teleport(tile_center_fixed(16), tile_center_fixed(8));
  w.relation[0][1] = kFriend; w.mission.sides[1].only_comp = true;
  CHECK(ai::step(w, rules, 0, s, {}).next.phase == 21);
  w.objects[2].teleport(tile_center_fixed(17), tile_center_fixed(8));
  d = ai::step(w, rules, 0, s, {});
  CHECK(d.next.phase == 0 && d.next.plan.empty());
}

static void prices_and_caps(const RulesData& rules) {
  struct Price { int hall, speed; bool computer, all; int price, time; };
  for (const auto& c : std::vector<Price>{{0,1,false,false,100,150}, {0,2,false,false,100,150},
       {0,1,true,false,80,120}, {0,2,true,true,100,150}, {1,1,false,false,100,100},
       {1,2,false,false,200,67}, {1,1,true,false,80,80}, {1,2,true,false,160,53},
       {1,1,true,true,67,200}, {1,2,true,true,67,200}}) {
    CHECK(production_value(100, c.hall, c.speed, c.computer, c.all, false) == c.price);
    CHECK(production_value(100, c.hall, c.speed, c.computer, c.all, true) == c.time);
  }
  auto w = world();
  const int base = ai::development(w, rules, 0);
  object(w, 0, 0, 0, 9, 8); object(w, 17, 0, 0, 9, 9); object(w, 12, 0, 0, 10, 8);
  CHECK(ai::development(w, rules, 0) == base);
  QueueItem q; q.type = 14; q.town = 0;
  w.side_state[0].queue.push_back(q);
  CHECK(ai::development(w, rules, 0) == base + rules.type(14).dev_level);
  object(w, 14, 0, 0, 12, 8); w.objects.back().progress = 500;
  w.side_state[0].queue.back().object = w.objects.back().id;
  CHECK(ai::development(w, rules, 0) == base + rules.type(14).dev_level); // Count one queued construction once.
  for (int n = 0; n < 15; ++n) w.side_state[0].queue.push_back(q);
  CHECK(ai::development(w, rules, 0) == 9);
  w = world(); ai::TownState threshold; threshold.phase = 13;
  for (int n = 0; n < 4; ++n) object(w, 0, 0, 0, 9, 8);
  CHECK(ai::step(w, rules, 0, threshold, {}).next.phase == 15); // Five members < 3*1+3.
  object(w, 0, 0, 0, 9, 8);
  CHECK(ai::step(w, rules, 0, threshold, {}).next.phase == 9);
  threshold.phase = 21;
  for (int n = 0; n < 7; ++n) object(w, 0, 0, 0, 9, 8);
  CHECK(ai::step(w, rules, 0, threshold, {}).next.phase == 23); // Thirteen < 7*(1+1).
  object(w, 0, 0, 0, 9, 8);
  CHECK(ai::step(w, rules, 0, threshold, {}).next.phase == 20);
  threshold.phase = 12;
  CHECK(ai::step(w, rules, 0, threshold, {}).next.phase == 5);
  object(w, 17, 0, 0, 9, 9);
  CHECK(ai::step(w, rules, 0, threshold, {}).next.phase == 0);
  // Science alone has a pre-purchase money gate; economics can queue while broke.
  w = world(); w.side_state[0].money = 0;
  ai::TownState s; s.phase = 3;
  auto d = ai::step(w, rules, 0, s, {{14, 100, 12, 8, true}});
  CHECK(!issued(d, Pct::Build) && d.next.phase == 13);
  w.side_state[0].money = 100; w.town_speed[0] = 2;
  d = ai::step(w, rules, 0, s, {{14, 100, 12, 8, true}});
  CHECK(issued(d, Pct::Build) && issued(d, Pct::BuildMode) && issued(d, Pct::BuildMode)->a == 1);
  w.side_state[0].money = 0; s.phase = 7;
  CHECK(issued(ai::step(w, rules, 0, s, {{12, 100, 12, 8, true}}), Pct::Build));
}

static void money_and_cancellation(std::shared_ptr<const RulesData> rules) {
  auto w = world();
  w.mission.sides[0].only_comp = w.mission.sides[1].only_comp = false;
  for (auto& o : w.objects) o.town_hall = true;
  GameSim sim(rules, Modifications::faithful()); sim.init(w);
  w.side_state[0].money = 0;
  for (int type : {0, 0}) {
    Command c; c.type = Pct::Build; c.side = 0; c.a = type; c.ids = {1}; sim.submit(c);
  }
  w.begin_tick(); sim.tick(w);
  CHECK(w.side_state[0].queue.size() == 2 && w.side_state[0].money == 0);
  if (w.side_state[0].queue.size() != 2) return;
  w.side_state[0].queue.front().type = 1; // Valid rifleman head with a completed prerequisite.
  object(w, 14, 0, 0, 12, 8);
  w.side_state[0].money = production_value(rules->type(0).price, 1, 1, false, false, false);
  const int money = w.side_state[0].money;
  w.begin_tick(); sim.tick(w);
  CHECK(!w.side_state[0].queue[0].started && !w.side_state[0].queue[1].started);
  CHECK(w.side_state[0].money == money);
  w.side_state[0].money = production_value(rules->type(1).price, 1, 1, false, false, false);
  w.begin_tick(); sim.tick(w);
  CHECK(w.side_state[0].queue[0].started && !w.side_state[0].queue[1].started);
  CHECK(w.side_state[0].money == 0); // Equality passes and charges exactly once.
  // A human proximity threat cancels paid and unpaid work through the existing refund path.
  w.mission.sides[0].only_comp = true;
  object(w, 0, 1, 1, 15, 8);
  w.ai_towns[0].phase = 2; w.ai_towns[0].plan = {{0}}; w.ai_towns[0].remaining = 1;
  w.begin_tick(); sim.tick(w);
  CHECK(w.side_state[0].queue.empty() && w.ai_towns[0].phase == 21 && w.ai_towns[0].plan.empty());
  CHECK(w.side_state[0].money > 0);
  const auto hash = sim.state_hash(w);
  w.ai_towns[0].wanted_type = 1;
  CHECK(sim.state_hash(w) != hash);
}

static void scripted(std::shared_ptr<const RulesData> rules) {
  auto a = world(); a.objects[0].scenario = 42;
  object(a, 0, 1, 1, 25, 26, 7); object(a, 0, 1, 1, 26, 26, 8);
  a.mission.paths = {{{24, 22}, {23, 21}}};
  auto b = a; b.local_side = 1;
  GameSim sa(rules, Modifications::faithful()), sb(rules, Modifications::faithful());
  CHECK(sa.attach_script(AZTEC_AI_SCRIPT)); CHECK(sb.attach_script(AZTEC_AI_SCRIPT));
  sa.init(a); sb.init(b);
  for (int t = 1; t <= 20; ++t) {
    a.begin_tick(); b.begin_tick(); sa.tick(a); sb.tick(b);
    CHECK(sa.state_hash(a) == sb.state_hash(b));
    if (t == 4) CHECK(a.objects[2].dest_x == 8 * kTilePx + 24);
    if (t == 6) CHECK(a.objects[2].dest_x == 22 * kTilePx + 24 && a.objects[2].dest_y == 19 * kTilePx + 24);
    if (t == 8) CHECK(a.objects[3].dest_x == 24 * kTilePx + 24);
  }
  CHECK(sa.script().unsupported() == 0);
}
static void smoke(std::shared_ptr<const RulesData> rules) {
  // Use ranged recruits in this synthetic pressure soak so melee crowding at
  // durable halls cannot end opposition after production timing changes.
  // Keep the footman's price, nominal time and build-tree slot, but use the
  // rifleman's combat descriptor. This fixture does not measure campaign balance.
  TempDir directory;
  for (const auto& file : std::filesystem::directory_iterator(AZTEC_TEST_RULES_DIR))
    if (file.is_regular_file() && file.path().extension() == ".csv")
      std::filesystem::copy_file(file.path(), directory.path() / file.path().filename());
  std::ifstream types(directory.path() / "types.csv");
  std::ostringstream table;
  const std::string footman = "0,FOOTMAN,Marine,3,";
  for (std::string line; std::getline(types, line);) {
    if (line.starts_with(footman)) line.replace(0, footman.size(), "0,FOOTMAN,Marine,9,");
    table << line << '\n';
  }
  types.close();
  std::ofstream(directory.path() / "types.csv") << table.str();
  auto synthetic = std::make_shared<RulesData>();
  CHECK(synthetic->load_dir(directory.path().string()));
  rules = synthetic;
  auto initial = world();
  // Keep the pressure fronts close enough for both armies to stay in contact.
  initial.mission.towns[1].x = initial.mission.towns[1].y = 24;
  initial.objects[1].teleport(tile_center_fixed(24), tile_center_fixed(24));
  // Hold technology constant so this pressure/replay soak does not also assert
  // campaign balance across changing building masks and formation geometry.
  // Development branches and higher-tech offers have separate unit coverage.
  for (auto& side : initial.mission.sides)
    for (int type = 0; type < 90; ++type)
      if (type != 0 && type != 11 && type != 12) side.disabled.push_back(type);
  for (auto& side : initial.side_state) side.money = 2000;
  // Rich synthetic terrain supports sustained combat while repeated farms remain available.
  initial.resources.v.assign(1600, 130);
  // Synthetic durable halls keep both producers alive through the full soak.
  // These HP values exercise sustained opposition, not campaign balance.
  for (auto& o : initial.objects) { o.hp = 10000000; o.town_hall = true; }
  // Capture ignores hall HP. Durable script-owned guards keep this pressure
  // fixture producing on both sides without prescribing a planner force target.
  for (int side = 0; side < 2; ++side) {
    const auto& town = initial.mission.towns[size_t(side)];
    // Keep guards outside the hall footprint, facing the opposing town. Halls
    // are damage-protected, so defenders must be able to reach incoming troops.
    object(initial, 0, side, side, town.x + (side ? -3 : 3), town.y, 7);
    initial.objects.back().hp = 10000000;
  }
  auto a = initial, b = initial; b.local_side = 1;
  GameSim sa(rules, Modifications::faithful()), sb(rules, Modifications::faithful());
  CHECK(sa.attach_script(AZTEC_AI_ECONOMY_SCRIPT)); CHECK(sb.attach_script(AZTEC_AI_ECONOMY_SCRIPT));
  sa.init(a); sb.init(b);
  bool construction = false, movement = false, casualties = false;
  std::array<int, 4> waves{}, shots{}, losses{}, births{};
  std::array<int, 2> side_waves{};
  std::array<std::array<int, 2>, 4> side_shots{}, side_recruits{}, side_purchases{};
  std::array<std::array<int, 90>, 2> wave_composition{};
  std::vector<uint64_t> hashes{sa.state_hash(a)};
  sa.set_ai_observer([&](uint64_t tick, const AiEvent& event) {
    if (std::string(event.kind) != "launch" || tick == 0 || tick > 14400) return;
    const size_t interval = size_t((tick - 1) / 3600);
    ++waves[interval]; ++side_waves[size_t(event.side)];
    for (size_t type = 0; type < 90; ++type) wave_composition[size_t(event.side)][type] += event.composition[type];
  });
  const int ticks = 20 * 60 * 12;
  for (int t = 0; t < ticks; ++t) {
    if (t == 100 || t == 110) {
      Command c; c.type = Pct::PauseBuild; c.side = 0; c.a = t == 100 ? 1 : 0;
      sa.submit(c); sb.submit(c);
    }
    auto previous = a.ai_towns;
    const auto objects = a.objects;
    const int cash = a.side_state[0].money;
    a.begin_tick(); b.begin_tick(); sa.tick(a); sb.tick(b);
    const auto net_income = a.ai_towns[0].harvested - previous[0].harvested -
                            (a.ai_towns[0].spent - previous[0].spent);
    for (size_t side = 0; side < 2; ++side)
      side_purchases[size_t(t / 3600)][side] += a.ai_towns[side].spent > previous[side].spent;
    if (t == 1000) CHECK(a.side_state[0].money == net_income);
    if (t == 1200) CHECK(a.side_state[0].money == cash + 2000 + net_income);
    for (size_t i = 0; i < a.objects.size(); ++i) {
      const auto& o = a.objects[i];
      if (i >= objects.size()) {
        ++births[size_t(t / 3600)];
        if (o.side >= 0 && o.side < 2 && rules->type(o.type).armed &&
            (rules->type(o.type).mobile || rules->type(o.type).flier))
          ++side_recruits[size_t(t / 3600)][size_t(o.side)];
        continue;
      }
      shots[size_t(t / 3600)] += a.objects[i].cooldown > objects[i].cooldown;
      if (o.side >= 0 && o.side < 2)
        side_shots[size_t(t / 3600)][size_t(o.side)] += o.cooldown > objects[i].cooldown;
      losses[size_t(t / 3600)] += objects[i].alive && !a.objects[i].alive;
    }
    if ((t + 1) % 3600 == 0) std::fprintf(stderr, "Skirmish tick %d: %zu objects\n", t + 1, a.objects.size());
    hashes.push_back(sa.state_hash(a));
    if (hashes.back() != sb.state_hash(b)) { CHECK(false); return; }
    for (const auto& side : a.side_state) for (const auto& q : side.queue)
      construction |= rules->type(q.type).structure;
    for (const auto& o : a.objects) {
      construction |= o.progress < 1000;
      movement |= o.order == uint8_t(Order::AttackMove) || o.order == uint8_t(Order::Attack);
      casualties |= !o.alive;
    }
  }
  CHECK(construction); CHECK(movement); CHECK(casualties);
  std::printf("Wave completions by five-minute interval: %d %d %d %d\n", waves[0], waves[1], waves[2], waves[3]);
  for (size_t i = 0; i < waves.size(); ++i) {
    std::printf("Interval %zu: %d shots, %d losses, %d births\n", i, shots[i], losses[i], births[i]);
    CHECK(waves[i] > 0 && shots[i] > 0 && losses[i] > 0 && births[i] > 0);
    for (size_t side = 0; side < 2; ++side) {
      std::printf("  Side %zu: %d shots, %d recruits, %d purchases\n", side,
                  side_shots[i][side], side_recruits[i][side], side_purchases[i][side]);
      // Reusing surviving troops can satisfy one side's plans for an entire interval.
      CHECK(side_shots[i][side] > 0);
    }
  }
  for (int side = 0; side < 2; ++side) {
    CHECK(a.objects[size_t(side)].alive && side_waves[size_t(side)] > 0);
    int recruits = 0, purchases = 0;
    for (size_t interval = 0; interval < side_recruits.size(); ++interval) {
      recruits += side_recruits[interval][size_t(side)]; purchases += side_purchases[interval][size_t(side)];
    }
    CHECK(recruits > 0 && purchases > 0);
    CHECK(a.ai_towns[size_t(side)].spent > 0 && a.ai_towns[size_t(side)].harvested > 0);
    // Only the synthetic type-0 recruits are enabled. Count actual released
    // members, so phase transitions alone cannot masquerade as an attack wave.
    CHECK(wave_composition[size_t(side)][0] > 0);
    for (size_t type = 1; type < 90; ++type) CHECK(wave_composition[size_t(side)][type] == 0);
  }
  CHECK(sa.script().unsupported() == 0);
  CHECK(a.objects.size() > 2); CHECK(sa.log().size() == 2);
  auto replay = initial;
  auto sim = GameSim::replay(replay, rules, sa.log(), ticks, AZTEC_AI_ECONOMY_SCRIPT,
    [&](uint64_t tick, const World& w, const GameSim& current) {
      CHECK(current.state_hash(w) == hashes[size_t(tick - 1)]);
    });
  CHECK(sim->state_hash(replay) == sa.state_hash(a));
  std::printf("20-minute skirmish: %d ticks, %zu objects, hash %016llx\n", ticks, a.objects.size(),
              static_cast<unsigned long long>(sa.state_hash(a)));
}
int main(int argc, char** argv) {
  auto rules = std::make_shared<RulesData>(); CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  if (argc > 1 && std::string(argv[1]) == "--smoke") smoke(rules); else { unit(*rules); reinforcements(*rules); threatened_plan(*rules); army_scenarios(rules); initialization(rules); neutral_computer(rules); sparse_towns(rules); observation(rules); town_orders(rules); enemy_price_mask(rules); remembered_intelligence(*rules); plan_allocation(rules); scripted(rules); }
  if (!(argc > 1 && std::string(argv[1]) == "--smoke")) {
    scoring(*rules); composition(); threat_rules(*rules); prices_and_caps(*rules); money_and_cancellation(rules);
  }
  return failures ? 1 : 0;
}
