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
  CHECK(d.next.phase == 21);
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
    const auto& q = w.side_state[0].queue[size_t(ti)];
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
  w.ai_towns[1].phase = 0; w.ai_towns[0].spent = 0;
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
    if (t == 3) CHECK(a.objects[2].dest_x == 8 * kTilePx + 24);
    if (t == 5) CHECK(a.objects[2].dest_x == 22 * kTilePx + 24 && a.objects[2].dest_y == 19 * kTilePx + 24);
    if (t == 7) CHECK(a.objects[3].dest_x == 24 * kTilePx + 24);
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
    object(initial, 0, side, side, town.x + 1, town.y, 7);
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
    if (t == 999) CHECK(a.side_state[0].money == net_income);
    if (t == 1199) CHECK(a.side_state[0].money == cash + 2000 + net_income);
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
  if (argc > 1 && std::string(argv[1]) == "--smoke") smoke(rules); else { unit(*rules); reinforcements(*rules); initialization(rules); neutral_computer(rules); sparse_towns(rules); observation(rules); town_orders(rules); enemy_price_mask(rules); remembered_intelligence(*rules); plan_allocation(rules); scripted(rules); }
  return failures ? 1 : 0;
}
