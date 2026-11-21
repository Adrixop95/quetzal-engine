// Point queries use the original wave for ground and the legacy octile A* for
// water. The historical API name is retained for callers. Group integration is separate.
#pragma once
#include <vector>
#include "flowfield.hpp"

namespace aztec::path {

struct AStarResult {
  bool found = false;           // false: goal unreachable (path leads to the closest reachable tile instead)
  std::vector<Cell> tiles;      // start .. end (inclusive)
  u32 cost = 0;                 // ground: discovery rounds after the first step; water: legacy 10/14 units
  u32 expanded = 0;
};

// Ground uses the documented fixed costs; wall_penalty applies only to water.
AStarResult astar(const NavGrid& g, MoveClass cls, Cell start, Cell goal, int wall_penalty = 4);
// Explicit legacy query for field comparisons and stable benchmark denominators.
AStarResult legacy_astar(const NavGrid& g, MoveClass cls, Cell start, Cell goal, int wall_penalty = 4);
// Remove waypoints that a disc of radius r can skip (straight segment between tile centres is clear). Returns pixel-fixed points.
std::vector<Vec2> smooth_path(const NavGrid& g, MoveClass cls, const std::vector<Cell>& tiles, i32 radius);

}  // namespace aztec::path
