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
    m.cells.resize(size_t(m.w) * m.h);
    ok = std::fread(m.cells.data(), 4, m.cells.size(), f) == m.cells.size();
  }
  std::fclose(f);
  return ok;
}

}  // namespace aztec
