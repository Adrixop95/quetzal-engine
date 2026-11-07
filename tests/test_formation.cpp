// Invented maps and placements; no campaign measurements or game files.
#include <cstdio>
#include <memory>
#include "engine/sim/game_sim.hpp"
#include "engine/sim/path/mover.hpp"

using namespace aztec;
using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static path::Vec2 centre(int x, int y) {
  return {tile_center_fixed(x), tile_center_fixed(y)};
}

static void test_shared_mover() {
  path::NavGrid grid(18, 18);
  path::MoveSystem mover(grid), repeat(grid);
  for (auto* sim : {&mover, &repeat}) {
    auto anchor = sim->add_agent(centre(11, 9), 14 * 256, 8 * 256, path::MoveClass::Small);
    auto follower = sim->add_agent(centre(3, 9), 14 * 256, 8 * 256, path::MoveClass::Small);
    uint32_t ids[] = {anchor, follower};
    sim->order_formation(ids, 2, {11, 9});
  }
  mover.tick(); repeat.tick();
  CHECK(mover.agents()[1].pos == centre(3, 9));
  mover.tick(); repeat.tick();
  CHECK(mover.agents()[1].pos.x == centre(3, 9).x + 8 * 256);
  for (int t = 2; t < 90; ++t) {
    mover.tick(); repeat.tick();
    CHECK(mover.state_hash() == repeat.state_hash());
    CHECK(mover.agents()[0].pos == centre(11, 9));
  }
  CHECK(mover.agents()[1].pos == centre(10, 9));
  // An ordinary command overrides startup routing and uses the same speed.
  uint32_t id = 1;
  mover.order_move(&id, 1, centre(4, 12));
  CHECK(mover.agents()[1].formation == -1);
  for (int t = 0; t < 100; ++t) mover.tick();
  CHECK(path::length(mover.agents()[1].pos - centre(4, 12)) <= 2 * 256);
}

static void test_passability_and_reservations() {
  path::NavGrid grid(18, 18);
  // A full-height narrow barrier is traversable only by the small class.
  for (int y = 0; y < 18; ++y) grid.set_terrain(7, y, 1);
  path::MoveSystem mover(grid);
  auto small = mover.add_agent(centre(3, 5), 14 * 256, 8 * 256, path::MoveClass::Small);
  auto large = mover.add_agent(centre(3, 13), 20 * 256, 16 * 256, path::MoveClass::Large);
  mover.order_formation(&small, 1, {12, 5});
  mover.order_formation(&large, 1, {12, 13});
  for (int t = 0; t < 100; ++t) mover.tick();
  CHECK(mover.agents()[small].pos == centre(11, 5));
  CHECK(mover.agents()[large].pos == centre(3, 13));

  path::NavGrid open(18, 18);
  path::MoveSystem crowded(open);
  uint32_t ids[] = {
    crowded.add_agent(centre(12, 9), 14 * 256, 8 * 256, path::MoveClass::Small),
    crowded.add_agent(centre(2, 8), 14 * 256, 8 * 256, path::MoveClass::Small),
    crowded.add_agent(centre(2, 10), 14 * 256, 12 * 256, path::MoveClass::Small)};
  // A unit outside the tactic obstructs the direct approach.
  crowded.add_agent(centre(8, 8), 14 * 256, 8 * 256, path::MoveClass::Small);
  crowded.order_formation(ids, 3, {12, 9});
  for (int t = 0; t < 120; ++t) {
    crowded.tick();
    CHECK(!(crowded.agents()[1].step_goal == crowded.agents()[2].step_goal));
    CHECK(!(crowded.agents()[1].step_goal == centre(8, 8)));
  }
  CHECK(!(crowded.agents()[1].pos == crowded.agents()[2].pos));
  CHECK(crowded.agents()[3].pos == centre(8, 8));
}

static void test_disabled_agents() {
  path::NavGrid grid(18, 18);
  for (int y = 0; y < 18; ++y)
    for (int x = 0; x < 18; ++x)
      if (y != 9) grid.set_terrain(x, y, 0);
  path::MoveSystem mover(grid);
  auto follower = mover.add_agent(centre(3, 9), 14 * 256, 8 * 256, path::MoveClass::Small);
  auto embarked = mover.add_agent(centre(6, 9), 14 * 256, 8 * 256, path::MoveClass::Small);
  auto inactive = mover.add_agent(centre(7, 9), 14 * 256, 8 * 256, path::MoveClass::Small);
  uint32_t ids[] = {follower, embarked};
  mover.order_formation(ids, 2, {12, 9});
  mover.order_move(&inactive, 1, centre(12, 9));
  mover.agents_mut()[embarked].enabled = false;
  mover.agents_mut()[inactive].enabled = false;
  // Disabled agents retain their state and do not reserve the only passage.
  for (int t = 0; t < 120; ++t) {
    mover.tick();
    CHECK(mover.agents()[embarked].pos == centre(6, 9));
    CHECK(mover.agents()[inactive].pos == centre(7, 9));
    CHECK(mover.agents()[embarked].win_ticks == 0);
    CHECK(mover.agents()[inactive].win_ticks == 0);
    CHECK(mover.agents()[embarked].moved_ticks == 0);
    CHECK(mover.agents()[inactive].moved_ticks == 0);
  }
  CHECK(mover.agents()[follower].pos == centre(11, 9));
  // Wall correction must also leave disabled agents untouched.
  mover.agents_mut()[inactive].pos = centre(7, 8);
  mover.tick();
  CHECK(mover.agents()[inactive].pos == centre(7, 8));
}

static void test_water_formation() {
  path::NavGrid grid(18, 18);
  for (int x = 0; x < 18; ++x) grid.set_terrain(x, 9, 0);
  path::MoveSystem mover(grid);
  auto vessel = mover.add_agent(centre(3, 9), 14 * 256, 8 * 256, path::MoveClass::Water);
  mover.order_formation(&vessel, 1, {12, 9});
  for (int t = 0; t < 100; ++t) mover.tick();
  CHECK(mover.agents()[vessel].pos == centre(11, 9));
  CHECK(mover.agents()[vessel].state == path::AgentState::Arrived);
}

static World fixture() {
  World w;
  w.terrain.w = w.terrain.h = 18;
  w.mission.w = w.mission.h = 18;
  for (auto [x, y] : {std::pair{11, 9}, {3, 9}, {12, 10}}) {
    PlacedObject p;
    p.x = x; p.y = y; p.type = 0; p.group = 2; p.scenario_id = -1;
    w.mission.objects.push_back(p);
  }
  place_mission_objects(w);
  return w;
}

static void test_initialization_and_replay() {
  auto rules = std::make_shared<RulesData>(); rules->init_defaults();
  World initial = fixture(), w = initial, second = initial;
  GameSim sim(rules), repeated(rules);
  sim.init(w); repeated.init(second);
  for (int t = 0; t < 90; ++t) {
    w.begin_tick(); sim.tick(w);
    second.begin_tick(); repeated.tick(second);
    CHECK(sim.state_hash(w) == repeated.state_hash(second));
    CHECK(w.objects[0].cur_x == tile_center_fixed(11));
    CHECK(w.objects[2].cur_x == tile_center_fixed(12));
    CHECK(w.objects[2].cur_y == tile_center_fixed(10));
  }
  CHECK(w.objects[1].cur_x == tile_center_fixed(10));
  World replayed = initial;
  auto replay = GameSim::replay(replayed, rules, sim.log(), sim.tick_count());
  CHECK(replay->state_hash(replayed) == sim.state_hash(w));
  // Imported original saves must retain their existing movement state.
  World saved = initial;
  saved.original_save = std::make_shared<SaveGame>();
  GameSim imported(rules); imported.init(saved);
  for (int t = 0; t < 60; ++t) { saved.begin_tick(); imported.tick(saved); }
  CHECK(saved.objects[1].cur_x == tile_center_fixed(3));
}

int main() {
  test_shared_mover();
  test_passability_and_reservations();
  test_disabled_agents();
  test_water_formation();
  test_initialization_and_replay();
  return failures ? 1 : 0;
}
