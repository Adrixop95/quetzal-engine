#include <cstdio>
#include <memory>
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
  s.spent = 20000; CHECK(ai::build_speed(w, s, 0) == 1);
  s.elapsed = 100; s.harvested = 40000; CHECK(ai::build_speed(w, s, 0) == 2);
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
// Initialization must provide both planner state and town-command hall identities.
static void initialization(std::shared_ptr<const RulesData> rules) {
  auto w = world();
  w.mission.start_time = -1;
  w.mission.sides[1].only_comp = false;
  w.objects[0].town_hall = w.objects[1].town_hall = true;
  w.side_state[0].selected_tactic = 7;
  GameSim sim(rules); sim.init(w);
  CHECK(sim.tick_count() == 0 && w.day_clock == kDayTicks - 1);
  CHECK(w.ai_towns.size() == 2 && w.town_state.size() == 2);
  CHECK(w.town_state[0].hall == 1 && w.town_state[1].hall == 2);
  CHECK(w.side_state[0].selected_tactic == -1);
  CHECK(w.visibility.sides[0].v == std::vector<uint8_t>(1600, 24));
  CHECK(w.visibility.sides[1].v == std::vector<uint8_t>(1600, 0));
}
static void scripted(std::shared_ptr<const RulesData> rules) {
  auto a = world(); a.objects[0].scenario = 42;
  object(a, 0, 1, 1, 25, 26, 7); object(a, 0, 1, 1, 26, 26, 8);
  a.mission.paths = {{{24, 22}, {23, 21}}};
  auto b = a; b.local_side = 1;
  GameSim sa(rules), sb(rules);
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
  auto initial = world();
  for (auto& side : initial.side_state) side.money = 2000;
  initial.resources.v.assign(1600, 65);
  auto a = initial, b = initial; b.local_side = 1;
  GameSim sa(rules), sb(rules); sa.init(a); sb.init(b);
  bool construction = false, movement = false, casualties = false;
  const int ticks = 20 * 60 * 12;
  for (int t = 0; t < ticks; ++t) {
    if (t == 100 || t == 110) {
      Command c; c.type = Pct::PauseBuild; c.side = 0; c.a = t == 100 ? 1 : 0;
      sa.submit(c); sb.submit(c);
    }
    a.begin_tick(); b.begin_tick(); sa.tick(a); sb.tick(b);
    if ((t + 1) % 3600 == 0) std::fprintf(stderr, "Skirmish tick %d: %zu objects\n", t + 1, a.objects.size());
    if (sa.state_hash(a) != sb.state_hash(b)) { CHECK(false); return; }
    for (const auto& o : a.objects) {
      construction |= o.progress < 1000;
      movement |= o.order == uint8_t(Order::AttackMove) || o.order == uint8_t(Order::Attack);
      casualties |= !o.alive;
    }
  }
  CHECK(construction); CHECK(movement); CHECK(casualties);
  CHECK(a.ai_towns[0].spent > 0 && a.ai_towns[0].harvested > 0);
  CHECK(a.objects.size() > 2); CHECK(sa.log().size() == 2);
  auto replay = initial; auto sim = GameSim::replay(replay, rules, sa.log(), ticks);
  CHECK(sim->state_hash(replay) == sa.state_hash(a));
  std::printf("20-minute skirmish: %d ticks, %zu objects, hash %016llx\n", ticks, a.objects.size(),
              static_cast<unsigned long long>(sa.state_hash(a)));
}
int main(int argc, char** argv) {
  auto rules = std::make_shared<RulesData>(); CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  if (argc > 1 && std::string(argv[1]) == "--smoke") smoke(rules); else { unit(*rules); initialization(rules); scripted(rules); }
  return failures ? 1 : 0;
}
