#include "core/loader_limits.hpp"
#include "core/map.hpp"

#include <cstdio>
#include <cstring>

namespace aztec {

bool load_terrain_map(const std::string& path, TerrainMap& m) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  uint16_t hdr[2];
  bool ok = std::fread(hdr, 2, 2, f) == 2;
  if (ok) {
    m.w = hdr[0];
    m.h = hdr[1];
    const size_t cells = size_t(m.w) * m.h;
    // Check both the budget and available bytes before allocating cells.
    long start = std::ftell(f);
    std::fseek(f, 0, SEEK_END);
    long end = std::ftell(f);
    std::fseek(f, start, SEEK_SET);
    if (!cells || cells > detail::kCells || start < 0 || end < start || cells > size_t(end - start) / 4) {
      std::fclose(f); return false;
    }
    m.cells.resize(cells);
    ok = std::fread(m.cells.data(), 4, m.cells.size(), f) == m.cells.size();
  }
  std::fclose(f);
  return ok;
}

}  // namespace aztec
