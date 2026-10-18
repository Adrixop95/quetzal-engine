#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace aztec {

// Maps/<name>.map: u16 width, u16 height, then width*height u32 cells.
// Low 16 bits = base terrain tile, high 16 bits = overlay tile (0 = none).
struct TerrainMap {
  int w = 0, h = 0;
  std::vector<uint32_t> cells;
  uint16_t base(int x, int y) const { return uint16_t(cells[size_t(y) * w + x] & 0xffff); }
  uint16_t overlay(int x, int y) const { return uint16_t(cells[size_t(y) * w + x] >> 16); }
};

bool load_terrain_map(const std::string& path, TerrainMap& out);

}  // namespace aztec
