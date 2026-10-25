// Benchmark: runs the movement-experiment courses (same layouts as tools/make_zpath.py in the original game) through
// the new deterministic MoveSystem and prints the same metrics as tools/zpath_analyze.py.
// usage: path_bench [--json] [--n 1,10,30] [--ticks N] [course...]
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include "engine/sim/path/courses.hpp"
#include "engine/sim/path/mover.hpp"

using namespace aztec::path;

static double optimal_px(const Course& c, Cell a, Cell b) {
  NavGrid g(c.w, c.h); g.load_dat(c.w, c.h, c.dat.data());
  AStarResult r = astar(g, MoveClass::Small, a, b, 0);
  double px = 0;
  for (size_t i = 1; i < r.tiles.size(); ++i) px += (r.tiles[i].x != r.tiles[i - 1].x && r.tiles[i].y != r.tiles[i - 1].y) ? 68 : 48;
  return px;
}

struct Result { std::string course; int n; double success, mean_ratio, ms_per_tick; int arrived, makespan, median, lost, max_idle, ideal; double mean_revisits; u64 hash; u64 fields, stuck; int overlaps; };

static Result run(const std::string& name, int n, int max_ticks) {
  Course c; make_course(name, n, c);
  NavGrid g(c.w, c.h); g.load_dat(c.w, c.h, c.dat.data());
  MoveSystem ms(g);
  const int ng = (int)c.groups.size();
  std::vector<std::vector<u32>> ids(ng);
  std::vector<Cell> gstart(ng, c.start);
  for (const CourseUnit& u : c.units) {
    UnitSpec sp = unit_spec(u.type);
    if (ids[u.group].empty()) gstart[u.group] = {u.x, u.y};
    ids[u.group].push_back(ms.add_agent({u.x * kTileFix + kTileFix / 2, u.y * kTileFix + kTileFix / 2}, 18 * kFix, sp.speed_px * kFix, sp.small ? MoveClass::Small : MoveClass::Large));
  }
  auto gpx = [&](int gi) { Cell gc = c.goal_of(gi); return Vec2{gc.x * kTileFix + kTileFix / 2, gc.y * kTileFix + kTileFix / 2}; };
  std::vector<int> arrival(c.units.size(), -1), idle(c.units.size(), 0), maxidle(c.units.size(), 0);
  std::vector<double> path(c.units.size(), 0);
  std::vector<std::vector<std::pair<int, int>>> tiles(c.units.size());
  std::vector<Vec2> last(c.units.size());
  for (size_t i = 0; i < c.units.size(); ++i) last[i] = ms.agents()[i].pos;
  int overlaps = 0;
  auto t0 = std::chrono::steady_clock::now();
  int ticks = 0, last_order = 0; for (auto& gd : c.groups) last_order = std::max(last_order, gd.order_tick);
  for (int t = 0; t <= max_ticks; ++t, ++ticks) {
    for (int gi = 0; gi < ng; ++gi) if (c.groups[gi].order_tick == t && !ids[gi].empty()) ms.order_move(ids[gi].data(), (int)ids[gi].size(), gpx(gi));
    ms.tick();
    int done = 0;
    for (size_t i = 0; i < c.units.size(); ++i) {
      const Agent& a = ms.agents()[i];
      int gi = c.units[i].group; Vec2 gp = gpx(gi);
      if (arrival[i] < 0 && t >= c.groups[gi].order_tick) {
        double d = length(a.pos - last[i]) / 256.0; path[i] += d;
        if (d < 0.5) { if (++idle[i] > maxidle[i]) maxidle[i] = idle[i]; } else idle[i] = 0;
        int tx = a.pos.x / kTileFix, ty = a.pos.y / kTileFix;
        if (tiles[i].empty() || tiles[i].back() != std::make_pair(tx, ty)) tiles[i].push_back({tx, ty});
        if (length(a.pos - gp) <= 6 * kTileFix) arrival[i] = t - c.groups[gi].order_tick;
      }
      last[i] = a.pos;
      if (arrival[i] >= 0) ++done;
    }
    if (t % 12 == 0) for (size_t i = 0; i < c.units.size(); ++i) for (size_t j = i + 1; j < c.units.size(); ++j) {
      const Agent &a = ms.agents()[i], &b = ms.agents()[j];
      if (len2(a.pos - b.pos) < (i64)(a.radius + b.radius) * (a.radius + b.radius) * 6 / 10) ++overlaps;
    }
    if (done == (int)c.units.size() && t > last_order + 30) break;
  }
  double ms_total = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  Result r{}; r.course = name; r.n = n; r.ms_per_tick = ms_total / std::max(ticks, 1);
  std::vector<int> arr; double ratio = 0; double rev = 0;
  std::vector<double> opt(ng);
  for (int gi = 0; gi < ng; ++gi) opt[gi] = optimal_px(c, gstart[gi], c.goal_of(gi));
  for (size_t i = 0; i < c.units.size(); ++i) {
    if (arrival[i] >= 0) { arr.push_back(arrival[i]); ratio += path[i] / opt[c.units[i].group]; } else ++r.lost;
    std::set<std::pair<int, int>> s(tiles[i].begin(), tiles[i].end()); rev += (double)tiles[i].size() - (double)s.size();
    r.max_idle = std::max(r.max_idle, maxidle[i]);
  }
  std::sort(arr.begin(), arr.end());
  r.arrived = (int)arr.size(); r.success = n ? (double)arr.size() / n : 0; r.makespan = arr.empty() ? -1 : arr.back(); r.median = arr.empty() ? -1 : arr[arr.size() / 2];
  r.mean_ratio = arr.empty() ? 0 : ratio / arr.size(); r.mean_revisits = rev / std::max<size_t>(c.units.size(), 1); r.ideal = (int)(opt[0] / 9.0);
  r.hash = ms.state_hash(); r.fields = ms.stats.fields_built; r.stuck = ms.stats.stuck_events; r.overlaps = overlaps;
  return r;
}

int main(int argc, char** argv) {
  bool json = false; std::vector<int> ns = {1, 10, 30}; int max_ticks = 1500; std::vector<std::string> courses;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--json")) json = true;
    else if (!strcmp(argv[i], "--ticks") && i + 1 < argc) max_ticks = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--n") && i + 1 < argc) { ns.clear(); for (char* p = strtok(argv[++i], ","); p; p = strtok(nullptr, ",")) ns.push_back(atoi(p)); }
    else courses.push_back(argv[i]);
  }
  if (courses.empty()) courses = course_names();
  if (!json) printf("%-7s %3s %7s %8s %6s %7s %6s %6s %7s %7s %8s\n", "course", "n", "arrived", "makespan", "ideal", "median", "ratio", "idle", "revisit", "overlap", "ms/tick");
  for (const std::string& c : courses) for (int n : ns) {
    Result r = run(c, n, max_ticks);
    if (json) printf("{\"course\":\"%s\",\"n\":%d,\"arrived\":%d,\"success\":%.3f,\"makespan\":%d,\"median_arrival\":%d,\"mean_path_ratio\":%.3f,\"lost\":%d,\"max_idle_ticks\":%d,\"mean_revisits\":%.2f,\"ideal_ticks\":%d,\"overlap_samples\":%d,\"ms_per_tick\":%.4f,\"hash\":\"%016llx\"}\n",
      r.course.c_str(), r.n, r.arrived, r.success, r.makespan, r.median, r.mean_ratio, r.lost, r.max_idle, r.mean_revisits, r.ideal, r.overlaps, r.ms_per_tick, (unsigned long long)r.hash);
    else printf("%-7s %3d %3d/%-3d %8d %6d %7d %6.3f %6d %7.2f %7d %8.4f\n", r.course.c_str(), r.n, r.arrived, r.n, r.makespan, r.ideal, r.median, r.mean_ratio, r.max_idle, r.mean_revisits, r.overlaps, r.ms_per_tick);
  }
}
