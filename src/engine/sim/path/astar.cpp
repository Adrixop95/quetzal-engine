#include "astar.hpp"
#include <queue>

namespace aztec::path {

namespace {
constexpr int kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
constexpr int kDy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
constexpr u32 kStep[8] = {10, 10, 10, 10, 14, 14, 14, 14};
inline u32 octile(int dx, int dy) { dx = dx < 0 ? -dx : dx; dy = dy < 0 ? -dy : dy; int mn = dx < dy ? dx : dy, mx = dx < dy ? dy : dx; return (u32)(14 * mn + 10 * (mx - mn)); }
}  // namespace

AStarResult astar(const NavGrid& g, MoveClass cls, Cell start, Cell goal, int wall_penalty) {
  AStarResult res;
  const int w = g.width(), h = g.height();
  if (g.blocked(start.x, start.y, cls) || !g.in_bounds(goal.x, goal.y)) return res;
  std::vector<u32> gc((size_t)w * h, 0xffffffffu);
  std::vector<i32> parent((size_t)w * h, -1);
  struct Node { u32 f, g, idx; bool operator<(const Node& o) const { if (f != o.f) return f > o.f; if (g != o.g) return g < o.g; return idx > o.idx; } };
  std::priority_queue<Node> open;
  auto I = [&](int x, int y) { return (u32)(y * w + x); };
  gc[I(start.x, start.y)] = 0; open.push({octile(goal.x - start.x, goal.y - start.y), 0, I(start.x, start.y)});
  u32 bestIdx = I(start.x, start.y); u32 bestH = octile(goal.x - start.x, goal.y - start.y);
  const bool goal_ok = !g.blocked(goal.x, goal.y, cls);
  while (!open.empty()) {
    Node n = open.top(); open.pop();
    if (n.g != gc[n.idx]) continue;
    ++res.expanded;
    int x = (int)(n.idx % w), y = (int)(n.idx / w);
    u32 hh = octile(goal.x - x, goal.y - y);
    if (hh < bestH) { bestH = hh; bestIdx = n.idx; }
    if (goal_ok && x == goal.x && y == goal.y) { bestIdx = n.idx; res.found = true; break; }
    for (int d = 0; d < 8; ++d) {
      int nx = x + kDx[d], ny = y + kDy[d];
      if (g.blocked(nx, ny, cls)) continue;
      if (d >= 4 && (g.blocked(x + kDx[d], y, cls) || g.blocked(x, y + kDy[d], cls))) continue;
      u32 ng = n.g + kStep[d] + (wall_penalty && g.near_wall(nx, ny, cls) ? (u32)wall_penalty : 0);
      u32 ni = I(nx, ny);
      if (ng < gc[ni]) { gc[ni] = ng; parent[ni] = (i32)n.idx; open.push({ng + octile(goal.x - nx, goal.y - ny), ng, ni}); }
    }
  }
  res.cost = gc[bestIdx];
  for (i32 i = (i32)bestIdx; i >= 0; i = parent[i]) res.tiles.push_back({i % w, i / w});
  for (size_t a = 0, b = res.tiles.size(); a + 1 < b; ++a, --b) { Cell t = res.tiles[a]; res.tiles[a] = res.tiles[b - 1]; res.tiles[b - 1] = t; }
  return res;
}

std::vector<Vec2> smooth_path(const NavGrid& g, MoveClass cls, const std::vector<Cell>& tiles, i32 radius) {
  std::vector<Vec2> out;
  if (tiles.empty()) return out;
  auto C = [](Cell c) { return Vec2{c.x * kTileFix + kTileFix / 2, c.y * kTileFix + kTileFix / 2}; };
  size_t i = 0; out.push_back(C(tiles[0]));
  while (i + 1 < tiles.size()) {
    size_t j = tiles.size() - 1;
    while (j > i + 1 && !g.segment_clear(C(tiles[i]), C(tiles[j]), radius, cls)) --j;
    out.push_back(C(tiles[j])); i = j;
  }
  return out;
}

}  // namespace aztec::path
