// Point-to-point A* on the tile grid (octile metric, no corner cutting) with string-pulling by disc line of sight.
// Used for single-unit queries (AI, tests); group orders use FlowField.
#pragma once
#include <vector>
#include "flowfield.hpp"

namespace aztec::path {

struct AStarResult {
  bool found = false;           // false: goal unreachable (path leads to the closest reachable tile instead)
  std::vector<Cell> tiles;      // start .. end (inclusive)
  u32 cost = 0;                 // 10/14 octile units (+ wall penalty)
  u32 expanded = 0;
};

AStarResult astar(const NavGrid& g, MoveClass cls, Cell start, Cell goal, int wall_penalty = 4);
// Remove waypoints that a disc of radius r can skip (straight segment between tile centres is clear). Returns pixel-fixed points.
std::vector<Vec2> smooth_path(const NavGrid& g, MoveClass cls, const std::vector<Cell>& tiles, i32 radius);

}  // namespace aztec::path
