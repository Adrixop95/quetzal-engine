// Goal-rooted integration field (Dijkstra, octile costs 10/14, Dial buckets) shared by every unit of an order.
// One field replaces the original's per-unit WaveSearch calls (which were rate limited to 2 per tick, Chebyshev metric).
#pragma once
#include <vector>
#include "navgrid.hpp"

namespace aztec::path {

struct Cell { int x, y; };

class FlowField {
 public:
  static constexpr u32 kInf = 0xffffffffu;
  // Build from one or more goal cells (e.g. the free ring around a building). Cells blocked for `cls` are never entered
  // (a blocked goal cell is moved to the nearest free tile). `wall_penalty` adds cost to tiles touching a wall.
  void build(const NavGrid& g, MoveClass cls, const Cell* goals, int ngoals, int wall_penalty = 4);
  bool valid() const { return !cost_.empty(); }
  int width() const { return w_; }
  int height() const { return h_; }
  MoveClass move_class() const { return cls_; }
  u32 cost(int x, int y) const { return cost_[(size_t)y * w_ + x]; }
  bool reachable(int x, int y) const { return in_bounds(x, y) && cost(x, y) != kInf; }
  // Best neighbour of (x,y) (no corner cutting). Returns false at a local minimum (goal or unreachable).
  bool step(int x, int y, int& nx, int& ny) const;
  // Reachable tile closest (Euclid) to tile (x,y) among tiles with finite cost; used for unreachable goals.
  bool closest_reachable(int x, int y, int& ox, int& oy) const;
  u32 version() const { return version_; }

 private:
  bool in_bounds(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
  int w_ = 0, h_ = 0;
  MoveClass cls_ = MoveClass::Small;
  u32 version_ = 0;
  const NavGrid* grid_ = nullptr;
  std::vector<u32> cost_;
  friend class MoveSystem;
};

}  // namespace aztec::path
