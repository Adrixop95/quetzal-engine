// Behavioral regressions against the public movement specification; no private inputs.
#include <algorithm>
#include <array>
#include <cstdio>
#include <set>
#include <stdexcept>
#include "engine/sim/path/mover.hpp"

using namespace aztec::path;
static int failures = 0, checks = 0;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)
static MoverConfig wave_config() { MoverConfig c; c.original_ground = true; return c; }
static Vec2 center(int x, int y) { return {(x * 48 + 24) * kFix, (y * 48 + 24) * kFix}; }
static bool same(Cell a, Cell b) { return a.x == b.x && a.y == b.y; }
static u16 label(const WaveResult& r, int width, Cell c) { return r.cells[size_t(c.y) * width + c.x] >> 4; }

static void wave_rules() {
  NavGrid g(9, 7);
  auto r = wave_search(g, MoveClass::Small, {2, 3}, Cell{5, 3});
  CHECK(r.found && r.tiles.size() == 4);
  CHECK(label(r, 9, {2, 3}) == 1);
  CHECK(label(r, 9, {3, 3}) == 2);
  CHECK(label(r, 9, {5, 3}) == 4);
  auto diagonal = wave_search(g, MoveClass::Small, {2, 2}, Cell{5, 5});
  CHECK(diagonal.found && label(diagonal, 9, {5, 5}) == 4);
  // A direct diagonal between two walls is valid, even for a large unit.
  g.set_terrain(3, 2, 0); g.set_terrain(2, 3, 0);
  r = wave_search(g, MoveClass::Large, {2, 2}, Cell{3, 3});
  CHECK(r.found && r.tiles.size() == 2);
  CHECK(g.connected(2, 2, 3, 3, MoveClass::Large, true));
  CHECK(g.can_enter(3, 3, MoveClass::Large, false));
  CHECK(!g.can_enter(3, 3, MoveClass::Large, false, true));
  CHECK(!g.can_enter(3, 3, MoveClass::Large, true));
  g.set_terrain(7, 3, 1);
  CHECK(g.can_enter(7, 3, MoveClass::Small, false));
  CHECK(!g.can_enter(7, 3, MoveClass::Large, false));

  NavGrid squeeze(2, 2); squeeze.set_terrain(1, 0, 0); squeeze.set_terrain(0, 1, 0);
  CHECK(!squeeze.connected(0, 0, 1, 1, MoveClass::Small));
  CHECK(squeeze.connected(0, 0, 1, 1, MoveClass::Small, true));
  squeeze.add_block(1, 1, 1, 1, 1);
  CHECK(!squeeze.connected(0, 0, 1, 1, MoveClass::Small, true));
  squeeze.add_block(1, 1, 1, 1, -1);
  CHECK(squeeze.connected(0, 0, 1, 1, MoveClass::Small, true));

  // Isolate the leaving cost; every tile in this one-row map is wall-side.
  NavGrid corridor(5, 1);
  r = wave_search(corridor, MoveClass::Small, {0, 0}, Cell{4, 0});
  CHECK(r.found && r.tiles.size() == 5);
  CHECK(label(r, 5, {1, 0}) == 2); // the starting tile has no delay
  CHECK(label(r, 5, {2, 0}) == 6); // four rounds paid on leaving tile 1
  CHECK(label(r, 5, {3, 0}) == 10);
  CHECK(label(r, 5, {4, 0}) == 14); // goal's own delay is never paid
  CHECK(r.kinds[1] == 1 && r.kinds.back() == 3);
  WaveOptions base; base.wall_sides = false;
  auto base_result = wave_search(corridor, MoveClass::Small, {0, 0}, Cell{4, 0}, base);
  CHECK(label(base_result, 5, {4, 0}) == 5); // classes 0/1 retain value 9 beside walls
  CHECK(base_result.kinds[1] == 3);
  // Descent crosses a label gap of four, rather than requiring label - 1.
  CHECK(same(r.tiles[3], {3, 0}));
  std::vector<u8> fog(5, 0);
  WaveOptions options; options.fog = fog;
  r = wave_search(corridor, MoveClass::Small, {0, 0}, Cell{4, 0}, options);
  CHECK(label(r, 5, {2, 0}) == 4);
  CHECK(label(r, 5, {3, 0}) == 6);
  CHECK(label(r, 5, {4, 0}) == 8);
  CHECK(r.kinds[1] == 3);
  fog[1] = 8;
  r = wave_search(corridor, MoveClass::Small, {0, 0}, Cell{4, 0}, options);
  CHECK(label(r, 5, {2, 0}) == 6); // brightness 8 does not override terrain

  NavGrid blocked(5, 5);
  blocked.set_terrain(2, 2, 0);
  r = wave_search(blocked, MoveClass::Small, {2, 2}, Cell{4, 4});
  CHECK(same(r.tiles.front(), {1, 1})); // top row first; no Euclidean preference
  r = wave_search(blocked, MoveClass::Small, {-8, -4}, Cell{99, 99});
  CHECK(same(r.tiles.front(), {0, 0}) && same(r.tiles.back(), {4, 4}));
  r = wave_search(blocked, MoveClass::Small, {1, 1}, Cell{1, 1});
  CHECK(r.found && r.tiles.size() == 1);
  // Blocked and occupied goals are still marked after layer construction.
  std::vector<u32> occupancy(25); occupancy[12] = 1;
  options = {}; options.occupied = occupancy;
  r = wave_search(blocked, MoveClass::Small, {1, 1}, Cell{2, 2}, options);
  CHECK(r.found && same(r.tiles.back(), {2, 2}));
  occupancy[6] = 1; fog.assign(25, 32); fog[6] = 7;
  options.fog = fog;
  r = wave_search(blocked, MoveClass::Small, {1, 1}, Cell{4, 4}, options);
  CHECK(same(r.tiles.front(), {1, 1})); // fog ignores occupancy
  CHECK((r.cells[6] & 15) == 10);
  // Fog also makes a blocked terrain cell searchable.
  fog[12] = 0;
  r = wave_search(blocked, MoveClass::Small, {1, 1}, Cell{3, 3}, options);
  CHECK((r.cells[12] & 15) == 10);
  options.fog = {}; occupancy[6] = 0; occupancy[7] = 1;
  r = wave_search(blocked, MoveClass::Small, {1, 1}, Cell{4, 4}, options);
  CHECK(r.cells[7] == 0); // explored occupancy blocks ordinary wave tiles
  occupancy[7] = 0;
  std::array<Cell, 2> goals{{{1, 2}, {1, 0}}};
  r = wave_search(blocked, MoveClass::Small, {1, 1}, goals, options);
  CHECK(r.found && same(r.tiles.back(), {1, 0})); // N precedes S, regardless of goal-list order
  bool rejected = false;
  try { WaveOptions wrong; wrong.fog = std::span<const u8>(fog.data(), 1); wave_search(blocked, MoveClass::Small, {1, 1}, Cell{4, 4}, wrong); }
  catch (const std::invalid_argument&) { rejected = true; }
  CHECK(rejected);

  // An invented grid exposes the documented 3-way tie selecting a blocked
  // non-candidate. Report this gap instead of returning a goal-only success.
  constexpr const char* ambiguous[] = {"02222022", "22002222", "02022222", "02202202",
                                      "02022222", "22220022", "22222222", "22220202"};
  NavGrid ambiguous_grid(8, 8);
  for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) ambiguous_grid.set_terrain(x, y, u8(ambiguous[y][x] - '0'));
  r = wave_search(ambiguous_grid, MoveClass::Small, {1, 1}, Cell{6, 6});
  CHECK(r.found && r.extraction_failed && r.tiles.empty());
  CHECK(!astar(ambiguous_grid, MoveClass::Small, {1, 1}, {6, 6}).found);

  NavGrid divided(7, 5);
  for (int y = 0; y < 5; ++y) divided.set_terrain(3, y, 0);
  r = wave_search(divided, MoveClass::Small, {1, 2}, Cell{5, 2});
  CHECK(!r.found && same(r.tiles.back(), {2, 1})); // first ring, least label, first on ties
  options = {}; options.radius = 2;
  r = wave_search(g, MoveClass::Small, {4, 3}, Cell{8, 3}, options);
  CHECK(!r.found && same(r.tiles.back(), {5, 2})); // interior only, label tie in column order
  r = wave_search(g, MoveClass::Small, {4, 3}, Cell{6, 3}, options);
  CHECK(r.found); // goal overwrites the radius outline
  NavGrid local(20, 20); options.radius = 6;
  r = wave_search(local, MoveClass::Small, {8, 8}, Cell{18, 8}, options);
  CHECK(!r.found && !r.tiles.empty());
  CHECK((r.cells[8 * 20 + 14] & 15) == 0); // 13x13 outline
  CHECK(label(r, 20, {16, 8}) == 0); // outside is not flooded
  CHECK(std::max(abs_i(r.tiles.back().x - 8), abs_i(r.tiles.back().y - 8)) == 5);
  options.radius = 2;
  std::array<Cell, 2> including_start{{{4, 3}, {8, 3}}};
  r = wave_search(g, MoveClass::Small, {4, 3}, including_start, options);
  CHECK(r.found && r.tiles.size() == 1);
  NavGrid isolated(3, 3);
  for (int y = 0; y < 3; ++y) for (int x = 0; x < 3; ++x) isolated.set_terrain(x, y, 0);
  isolated.set_terrain(0, 0, 2);
  r = wave_search(isolated, MoveClass::Small, {0, 0}, Cell{2, 2});
  CHECK(!r.found && r.tiles.size() == 1);
  isolated.set_terrain(0, 0, 0);
  CHECK(wave_search(isolated, MoveClass::Small, {0, 0}, Cell{2, 2}).tiles.empty());
  // Labels cannot overflow into terrain or another cell on oversized maps.
  NavGrid long_map(1100, 1);
  r = wave_search(long_map, MoveClass::Small, {0, 0}, Cell{1099, 0});
  CHECK(!r.found && r.label_limit);
  // Fronts grow safely beyond the original fixed allocation.
  NavGrid broad(601, 601);
  r = wave_search(broad, MoveClass::Small, {300, 300}, Cell{0, 0});
  CHECK(r.found && !r.tiles.empty());
}

static void ties_and_budget() {
  std::array<int, 2> pair{1, 7};
  CHECK(wave_predecessor(pair, 0) == 7); // equal circular distance: second
  CHECK(wave_predecessor(pair, 2) == 1);
  std::array<int, 4> three{0, 2, 6, 7};
  CHECK(wave_predecessor(three, 5) == 1); // only first three; answer need not be a candidate
  three = {0, 5, 6, 7}; CHECK(wave_predecessor(three, 0) == 7);
  three = {0, 1, 4, 6}; CHECK(wave_predecessor(three, 3) == 0);
  three = {1, 3, 6, 7}; CHECK(wave_predecessor(three, 0) == 3);
  CHECK(wave_predecessor({}, 0) == -1);
  WaveBudget budget; budget.begin_tick();
  CHECK(budget.request(1, 0) && budget.request(2, 0));
  CHECK(budget.request(1, 200)); // two identities, not two calls
  CHECK(!budget.request(3, 5)); CHECK(!budget.request(4, 5));
  CHECK(!budget.request(5, 9));
  CHECK(budget.waiting()[0].id == 5 && budget.waiting()[1].id == 3);
  budget.begin_tick();
  CHECK(budget.granted()[0] == 5 && budget.granted()[1] == 3);
  CHECK(budget.request(5, 0) && budget.request(3, 0));
  CHECK(!budget.request(4, 7));
  budget.begin_tick();
  CHECK(budget.request(4, 0) && budget.request(8, 0));
  CHECK(!budget.request(9, 0));
}

static void arithmetic() {
  CHECK(movement_norm({48, 0}) == 47);
  CHECK(movement_norm({48, 48}) == 66);
  CHECK(movement_norm({-48, 48}) == 66);
  CHECK(movement_pixels({-255, -257}) == (Vec2{0, -1}));
  CHECK(movement_scale({}, 7) == (Vec2{7, 7}));
  CHECK(marine_velocity(center(1, 1), center(2, 1), 9 * kFix).x == 48 * 9 * kFix / 47);
  CHECK(marine_velocity(center(1, 1), center(2, 2), 9 * kFix).x == 48 * 9 * kFix / 66);
  CHECK(vehicle_turn(250, 4) == 2);
  CHECK(vehicle_turn(250, 1) == 1);
  CHECK(vehicle_turn(0, 8) == 8);
  CHECK(vehicle_turn(0, 7) == 7);
  CHECK(vehicle_turn(4, 250) == 252);
  CHECK(vehicle_turn(0, 128) == 248); // -128 signed difference
  CHECK(vehicle_speed(0, 8, 48 * kFix, 55) == 2);
  CHECK(vehicle_speed(0, 12, 48 * kFix, 0) == 4);
  CHECK(vehicle_speed(8, 8, 24 * kFix, 0) == 8);
  CHECK(vehicle_speed(8, 8, 24 * kFix - 1, 0) == 6);
  CHECK(vehicle_speed(8, 8, 48 * kFix, 56) == 6);
  CHECK(vehicle_speed(1, 8, 1, 0) == 0);
}

static void movement() {
  NavGrid g(20, 14); MoveSystem mover(g, wave_config());
  u32 lead = mover.add_agent(center(3, 3), 18 * kFix, 9 * kFix, MoveClass::Small);
  u32 follow = mover.add_agent(center(2, 3), 18 * kFix, 9 * kFix, MoveClass::Small);
  mover.order_move(&lead, 1, center(10, 3));
  mover.order_move(&follow, 1, center(3, 3));
  mover.tick();
  CHECK(mover.agents()[lead].pos == center(3, 3));
  CHECK(same(mover.agents()[lead].reserved_tile, {4, 3}));
  CHECK(same(mover.agents()[follow].reserved_tile, {3, 3})); // old tile released immediately
  CHECK(mover.agents()[lead].wave_path.size() <= 7);
  Vec2 initial_velocity = mover.agents()[lead].vel;
  mover.tick(); CHECK(mover.agents()[lead].vel == initial_velocity);
  mover.tick(); CHECK(mover.agents()[lead].vel == initial_velocity);
  mover.tick();
  CHECK(same(mover.agents()[lead].reserved_tile, {5, 3}));
  CHECK(mover.agents()[lead].pos.x < center(4, 3).x); // early next step cuts the center
  for (int tick = 0; tick < 100; ++tick) mover.tick();
  CHECK(mover.agents()[lead].state == AgentState::Arrived);
  CHECK(mover.agents()[lead].pos == center(10, 3));

  MoveSystem replacement(g, wave_config());
  u32 first = replacement.add_agent(center(3, 3), 18 * kFix, 9 * kFix, MoveClass::Small);
  u32 second = replacement.add_agent(center(2, 3), 18 * kFix, 9 * kFix, MoveClass::Small);
  replacement.order_move(&first, 1, center(10, 3));
  replacement.order_move(&second, 1, center(3, 3)); replacement.tick();
  replacement.order_move(&first, 1, center(11, 3));
  CHECK(same(replacement.agents()[first].reserved_tile, {4, 3})); // preserve ownership across orders
  CHECK(same(replacement.agents()[second].reserved_tile, {3, 3}));

  MoveSystem attacking(g, wave_config());
  u32 marine = attacking.add_agent(center(3, 3), 100 * kFix, 9 * kFix, MoveClass::Small);
  attacking.order_move(&marine, 1, center(10, 3));
  attacking.agents_mut()[marine].about_to_attack = true;
  for (int i = 0; i < 4; ++i) attacking.tick();
  CHECK(same(attacking.agents()[marine].reserved_tile, {4, 3})); // suppress early next step
  CHECK(attacking.agents()[marine].vel == initial_velocity); // radius never changes the one-tile step

  for (int blocked = 0; blocked < 3; ++blocked) {
    NavGrid side_grid(9, 9); MoveSystem side(side_grid, wave_config());
    u32 id = side.add_agent(center(3, 3), 18 * kFix, 9 * kFix, MoveClass::Small);
    side.order_move(&id, 1, center(6, 3));
    auto& a = side.agents_mut()[id]; a.wave_path = {{4, 3}};
    side_grid.set_terrain(4, 3, 0); side_grid.set_terrain(3, 4, 0);
    if (blocked >= 1) side_grid.set_terrain(4, 4, 0);
    if (blocked >= 2) side_grid.set_terrain(4, 2, 0);
    side.tick();
    CHECK(same(side.agents()[id].reserved_tile, blocked == 0 ? Cell{4, 4} : blocked == 1 ? Cell{4, 2} : Cell{3, 3}));
    CHECK(side.agents()[id].wave_path.empty());
  }
  NavGrid two(2, 1); MoveSystem no_swap(two, wave_config());
  u32 left = no_swap.add_agent(center(0, 0), 20 * kFix, 8 * kFix, MoveClass::Large);
  u32 right = no_swap.add_agent(center(1, 0), 20 * kFix, 8 * kFix, MoveClass::Large);
  no_swap.order_move(&left, 1, center(1, 0)); no_swap.order_move(&right, 1, center(0, 0));
  for (int i = 0; i < 20; ++i) no_swap.tick();
  CHECK(no_swap.agents()[left].pos == center(0, 0));
  CHECK(no_swap.agents()[right].pos == center(1, 0));

  MoveSystem admitted(g, wave_config());
  for (int i = 0; i < 5; ++i) {
    u32 id = admitted.add_agent(center(2, 2 + i * 2), 18 * kFix, 9 * kFix, MoveClass::Small);
    admitted.order_move(&id, 1, center(17, 2 + i * 2));
  }
  admitted.tick(); CHECK(admitted.stats.repaths == 2 && admitted.agents()[4].retries == 1);
  admitted.tick(); CHECK(admitted.stats.repaths == 4 && admitted.agents()[4].retries == 2);
  admitted.tick(); CHECK(admitted.agents()[4].retries == 0 && admitted.agents()[4].stepping);

  std::vector<u8> darkness(size_t(g.width()) * g.height(), 32);
  admitted.set_fog(0, darkness);
  const u64 original_hash = admitted.state_hash();
  admitted.agents_mut()[0].team = 1;
  CHECK(admitted.state_hash() != original_hash); // ownership selects a future fog layer

  MoveSystem vehicle(g, wave_config()); u32 id = vehicle.add_agent(center(2, 5), 20 * kFix, 8 * kFix, MoveClass::Large);
  vehicle.agents_mut()[id].vehicle = true;
  vehicle.order_move(&id, 1, center(14, 5));
  vehicle.tick(); vehicle.tick();
  CHECK(vehicle.agents()[id].vehicle_speed_px == 2);
  for (int i = 0; i < 200; ++i) vehicle.tick();
  CHECK(vehicle.agents()[id].state == AgentState::Arrived);
  CHECK(vehicle.agents()[id].pos != center(14, 5)); // braking never snaps
  CHECK(movement_norm(center(14, 5) - vehicle.agents()[id].pos) < 24 * kFix);
}

static u64 deterministic_run() {
  NavGrid grid(30, 20); MoveSystem mover(grid, wave_config());
  for (int y = 0; y < 20; ++y) if (y != 10) grid.set_terrain(15, y, 0);
  std::vector<u32> ids;
  for (int i = 0; i < 10; ++i) ids.push_back(mover.add_agent(center(3 + i % 2, 3 + i / 2), 18 * kFix, 9 * kFix, MoveClass::Small));
  mover.order_move(ids.data(), int(ids.size()), center(25, 10));
  u64 digest = 0;
  for (int tick = 0; tick < 600; ++tick) {
    mover.tick(); digest = digest * 1099511628211ull ^ mover.state_hash();
    std::set<std::pair<int, int>> owned;
    for (const Agent& a : mover.agents()) {
      CHECK(owned.emplace(a.reserved_tile.x, a.reserved_tile.y).second);
      CHECK(!grid.blocked(a.reserved_tile.x, a.reserved_tile.y, a.cls));
    }
  }
  for (const Agent& a : mover.agents()) CHECK(a.state == AgentState::Arrived);
  return digest;
}

int main() {
  wave_rules(); ties_and_budget(); arithmetic(); movement();
  CHECK(deterministic_run() == deterministic_run());
  std::printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
