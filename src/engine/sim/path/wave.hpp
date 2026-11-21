// Independent implementation of the behavioral contract in docs/pathfinding_wave.md.
#pragma once
#include <array>
#include <limits>
#include <span>
#include "flowfield.hpp"

namespace aztec::path {

struct WaveOptions {
  std::span<const u8> fog;       // empty = explored; otherwise width * height, brightness < 8 = fog
  std::span<const u32> occupied; // empty = ignore occupancy; clear the requester's own tile before calling
  int radius = -1;              // block only this square outline around the relocated start
  bool wall_sides = true;       // margin layers (classes 2/3); false selects base layers (classes 0/1)
};

struct WaveResult {
  bool found = false;
  bool label_limit = false;     // safe engine extension: never wrap a 12-bit label
  bool extraction_failed = false; // documented non-candidate tie can break descent; no usable route then
  std::vector<Cell> tiles;      // includes start; a one-node partial path is not a usable step
  std::vector<u8> kinds;        // 1 = wall side, 3 = ordinary/fog/goal
  std::vector<u16> cells;       // unpadded wave map after a flood; empty on early return
  u32 expanded = 0;
};

WaveResult wave_search(const NavGrid& grid, MoveClass cls, Cell start,
                       std::span<const Cell> goals, WaveOptions options = {});
inline WaveResult wave_search(const NavGrid& grid, MoveClass cls, Cell start, Cell goal,
                              WaveOptions options = {}) {
  return wave_search(grid, cls, start, std::span<const Cell>(&goal, 1), options);
}
// Direction indices are clockwise from N. This is an arithmetic rule, not an executable table.
int wave_predecessor(std::span<const int> candidates, int previous);

class WaveBudget {
 public:
  static constexpr u32 none = std::numeric_limits<u32>::max();
  struct Waiting { u32 id = none, retries = 0; };
  void begin_tick();
  bool request(u32 id, u32 retries);
  const std::array<u32, 2>& granted() const { return granted_; }
  const std::array<Waiting, 2>& waiting() const { return waiting_; }
 private:
  std::array<u32, 2> granted_{none, none};
  std::array<Waiting, 2> waiting_{};
};

}  // namespace aztec::path
