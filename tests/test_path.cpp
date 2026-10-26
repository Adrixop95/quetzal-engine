// Unit tests for src/engine/sim/path (plain asserts, no framework). usage: test_path
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "engine/sim/path/courses.hpp"
#include "engine/sim/path/mover.hpp"

using namespace aztec::path;
static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_fail; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static u32 rng_state = 12345;
static u32 rnd() { rng_state = rng_state * 1664525u + 1013904223u; return rng_state >> 8; }

static void test_math() {
  for (u64 v : {0ull, 1ull, 2ull, 3ull, 4ull, 15ull, 16ull, 17ull, 1000000ull, 4611686014132420609ull}) { u64 r = isqrt(v); CHECK(r * r <= v && (r + 1) * (r + 1) > v); }
  CHECK(angle_of(1000, 0) == 0); CHECK(angle_of(0, 1000) == 64); CHECK(angle_of(-1000, 0) == 128); CHECK(angle_of(0, -1000) == 192);
  CHECK(angle_of(1000, 1000) == 32); CHECK(angle_of(-1000, -1000) == 160);
  for (int a = 0; a < 256; ++a) { Vec2 v = from_angle(a, 100000); int b = angle_of(v.x, v.y); int d = (b - a + 128 + 256) % 256 - 128; CHECK(d >= -1 && d <= 1); }
  CHECK(facing8({0, -1000}) == 0); CHECK(facing8({1000, 0}) == 2); CHECK(facing8({0, 1000}) == 4); CHECK(facing8({-1000, 0}) == 6);
  Vec2 r = rotate({100000, 0}, 64); CHECK(r.x > -50 && r.x < 50 && r.y > 99000);
}

static void random_grid(NavGrid& g, int w, int h, int pct) {
  g.reset(w, h);
  for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) { u32 r = rnd() % 100; g.set_terrain(x, y, r < (u32)pct ? 0 : r < (u32)pct + 5 ? 1 : 2); }
}

static void test_field_vs_astar() {
  for (int trial = 0; trial < 40; ++trial) {
    NavGrid g; random_grid(g, 40, 30, 25);
    MoveClass cls = (trial & 1) ? MoveClass::Large : MoveClass::Small;
    Cell s{(int)(rnd() % 40), (int)(rnd() % 30)}, e{(int)(rnd() % 40), (int)(rnd() % 30)};
    if (g.blocked(s.x, s.y, cls) || g.blocked(e.x, e.y, cls)) continue;
    FlowField ff; ff.build(g, cls, &e, 1, 4);
    AStarResult a = astar(g, cls, s, e, 4);
    CHECK(a.found == ff.reachable(s.x, s.y));
    if (a.found) {
      // the field charges the wall penalty on the tile it expands into (start side), A* on the tile entered (goal side)
      u32 ps = g.near_wall(s.x, s.y, cls) ? 4 : 0, pe = g.near_wall(e.x, e.y, cls) ? 4 : 0;
      CHECK(a.cost + ps == ff.cost(s.x, s.y) + pe);
      // following the field from s reaches e in exactly the A* number of steps cost-wise
      int x = s.x, y = s.y, guard = 2000; u32 prev = ff.cost(x, y), walked = 0;
      while (ff.cost(x, y) != 0 && guard--) { int nx, ny; if (!ff.step(x, y, nx, ny)) break; CHECK(ff.cost(nx, ny) < prev); walked += (nx != x && ny != y ? 14 : 10) + (g.near_wall(nx, ny, cls) ? 4 : 0); prev = ff.cost(nx, ny); x = nx; y = ny; }
      CHECK(x == e.x && y == e.y);
      CHECK(walked == a.cost);
    }
  }
}

static void test_no_corner_cut() {
  NavGrid g(10, 10); g.set_terrain(5, 4, 0); g.set_terrain(4, 5, 0);   // diagonal squeeze between (4,4) and (5,5)
  AStarResult a = astar(g, MoveClass::Small, {4, 4}, {5, 5}, 0);
  CHECK(a.found); CHECK(a.tiles.size() > 2);   // must go around, not through the diagonal gap
}

static u64 run_course(const char* name, int n, int ticks, int* arrived = nullptr, bool check_walls = true) {
  Course c; CHECK(make_course(name, n, c));
  NavGrid g(c.w, c.h); g.load_dat(c.w, c.h, c.dat.data());
  MoveSystem ms(g);
  std::vector<std::vector<u32>> ids(c.groups.size());
  for (const CourseUnit& u : c.units) { UnitSpec sp = unit_spec(u.type); ids[u.group].push_back(ms.add_agent({u.x * kTileFix + kTileFix / 2, u.y * kTileFix + kTileFix / 2}, 18 * kFix, sp.speed_px * kFix, sp.small ? MoveClass::Small : MoveClass::Large)); }
  for (int t = 0; t < ticks; ++t) {
    for (size_t gi = 0; gi < ids.size(); ++gi) if (c.groups[gi].order_tick == t && !ids[gi].empty()) { Cell gc = c.goal_of((int)gi); ms.order_move(ids[gi].data(), (int)ids[gi].size(), {gc.x * kTileFix + kTileFix / 2, gc.y * kTileFix + kTileFix / 2}); }
    ms.tick();
    if (check_walls) for (const Agent& a : ms.agents()) if (g.disc_hits(a.pos, a.radius * 5 / 10, a.cls)) { CHECK(false && "agent inside wall"); check_walls = false; break; }
  }
  { int bad = 0; const auto& ag = ms.agents(); for (size_t i = 0; i < ag.size(); ++i) for (size_t j = i + 1; j < ag.size(); ++j) if (len2(ag[i].pos - ag[j].pos) < (i64)(ag[i].radius + ag[j].radius) * (ag[i].radius + ag[j].radius) * 36 / 100) ++bad; CHECK(bad == 0); if (bad) printf("  %s: %d deeply overlapping pairs\n", name, bad); }
  if (arrived) { int n2 = 0; for (const Agent& a : ms.agents()) if (a.state == AgentState::Arrived) ++n2; *arrived = n2; }
  return ms.state_hash();
}

static void test_courses() {
  struct { const char* c; int n; int ticks; } cases[] = {{"clusters", 30, 700}, {"open", 30, 600}, {"gap1", 30, 900}, {"corr1", 30, 900}, {"maze", 30, 1700}, {"utrap", 30, 700}, {"swap", 30, 700}, {"diag", 30, 700}, {"rooms", 30, 700}, {"park", 30, 900}, {"mixed", 30, 1300}, {"swap1", 30, 1200}};
  for (auto& k : cases) { int arrived = 0; run_course(k.c, k.n, k.ticks, &arrived); CHECK(arrived == k.n); if (arrived != k.n) printf("  %s: %d/%d arrived\n", k.c, arrived, k.n); }
}

static void test_determinism() {
  u64 a = run_course("gap1", 30, 500), b = run_course("gap1", 30, 500);
  CHECK(a == b);
  // Golden value: lockstep requires identical state on every platform. Update only after an intentional algorithm change.
  const u64 golden = 0xb523e3cb0e07c250ull;
  CHECK(a == golden);
}

static void test_unreachable_goal() {
  NavGrid g(30, 20); for (int y = 0; y < 20; ++y) for (int x = 0; x < 30; ++x) if (x == 0 || y == 0 || x == 29 || y == 19) g.set_terrain(x, y, 0);
  for (int y = 5; y <= 13; ++y) for (int x = 18; x <= 26; ++x) g.set_terrain(x, y, (x == 18 || x == 26 || y == 5 || y == 13) ? 0 : 2);   // sealed room
  MoveSystem ms(g);
  u32 id = ms.add_agent({5 * kTileFix, 10 * kTileFix}, 18 * kFix, 9 * kFix, MoveClass::Small);
  ms.order_move(&id, 1, {22 * kTileFix, 9 * kTileFix});
  for (int t = 0; t < 400; ++t) ms.tick();
  const Agent& a = ms.agents()[0];
  CHECK(a.state == AgentState::Arrived);
  CHECK(a.pos.x / kTileFix >= 16 && a.pos.x / kTileFix <= 17);   // stopped at the wall nearest to the goal
}

static void test_large_class() {
  NavGrid g(30, 20); for (int y = 0; y < 20; ++y) for (int x = 0; x < 30; ++x) g.set_terrain(x, y, (x == 0 || y == 0 || x == 29 || y == 19) ? 0 : 2);
  for (int y = 1; y < 19; ++y) g.set_terrain(15, y, 0);
  g.set_terrain(15, 10, 1);   // wall whose only door is passable for small units only
  MoveSystem ms(g);
  u32 small = ms.add_agent({5 * kTileFix, 10 * kTileFix}, 18 * kFix, 9 * kFix, MoveClass::Small);
  u32 large = ms.add_agent({5 * kTileFix, 12 * kTileFix}, 18 * kFix, 9 * kFix, MoveClass::Large);
  ms.order_move(&small, 1, {25 * kTileFix, 10 * kTileFix}); ms.order_move(&large, 1, {25 * kTileFix, 12 * kTileFix});
  for (int t = 0; t < 300; ++t) ms.tick();
  CHECK(ms.agents()[small].state == AgentState::Arrived && ms.agents()[small].pos.x > 20 * kTileFix);
  CHECK(ms.agents()[large].pos.x < 15 * kTileFix);   // large unit cannot pass; ends on its side of the wall
}

static void test_dynamic_block() {
  NavGrid g(30, 20); for (int y = 0; y < 20; ++y) for (int x = 0; x < 30; ++x) g.set_terrain(x, y, (x == 0 || y == 0 || x == 29 || y == 19) ? 0 : 2);
  FlowField ff; Cell goal{25, 10}; ff.build(g, MoveClass::Small, &goal, 1);
  u32 c0 = ff.cost(5, 10);
  g.add_block(14, 1, 2, 18, +1);   // building footprint wall, leaves y=0 and y=19 rows blocked -> sealed
  FlowField ff2; ff2.build(g, MoveClass::Small, &goal, 1);
  CHECK(c0 != FlowField::kInf); CHECK(ff2.cost(5, 10) == FlowField::kInf);
  g.add_block(14, 1, 2, 18, -1);
  FlowField ff3; ff3.build(g, MoveClass::Small, &goal, 1);
  CHECK(ff3.cost(5, 10) == c0);
}

static void test_random_stress() {
  int total = 0, settled = 0;
  for (int trial = 0; trial < 12; ++trial) {
    NavGrid g; random_grid(g, 50, 40, 18);
    for (int x = 0; x < 50; ++x) { g.set_terrain(x, 0, 0); g.set_terrain(x, 39, 0); }
    for (int y = 0; y < 40; ++y) { g.set_terrain(0, y, 0); g.set_terrain(49, y, 0); }
    MoveSystem ms(g);
    std::vector<u32> ids; int guard = 0;
    while ((int)ids.size() < 16 && guard++ < 2000) {
      int x = 1 + (int)(rnd() % 48), y = 1 + (int)(rnd() % 38);
      if (g.blocked(x, y, MoveClass::Large)) continue;
      Vec2 p{x * kTileFix + kTileFix / 2, y * kTileFix + kTileFix / 2}; bool ok = true;
      for (u32 j : ids) if (len2(ms.agents()[j].pos - p) < (i64)40 * kFix * 40 * kFix) ok = false;
      if (ok) ids.push_back(ms.add_agent(p, 18 * kFix, (6 + (int)(rnd() % 10)) * kFix, (rnd() & 1) ? MoveClass::Small : MoveClass::Large));
    }
    int gx, gy; do { gx = 1 + (int)(rnd() % 48); gy = 1 + (int)(rnd() % 38); } while (g.blocked(gx, gy, MoveClass::Large));
    ms.order_move(ids.data(), (int)ids.size(), {gx * kTileFix + kTileFix / 2, gy * kTileFix + kTileFix / 2});
    for (int t = 0; t < 1500; ++t) {
      ms.tick();
      for (const Agent& a : ms.agents()) if (g.disc_hits(a.pos, a.radius * 5 / 10, a.cls)) { CHECK(false && "stress: agent inside wall"); t = 1500; break; }
    }
    for (const Agent& a : ms.agents()) { ++total; if (a.state == AgentState::Arrived) ++settled; }
  }
  printf("  stress: %d/%d agents settled\n", settled, total);
  CHECK(settled * 100 >= total * 90);
}

int main() {
  test_math(); test_field_vs_astar(); test_no_corner_cut(); test_courses(); test_determinism(); test_unreachable_goal(); test_large_class(); test_dynamic_block(); test_random_stress();
  printf("%d checks, %d failed\n", g_checks, g_fail);
  return g_fail ? 1 : 0;
}
