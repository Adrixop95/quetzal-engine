// Simulation lighting inputs. Mask bytes and daylight keys stay in user files.
#pragma once
#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <vector>
#include "core/visfile.hpp"
#include "engine/assets/anim_provider.hpp"
#include "engine/data/light_masks.hpp"

namespace aztec::engine {

struct SightData {
  std::map<int, std::vector<uint8_t>> masks;
  std::array<std::array<uint8_t, 7200>, 2> daylight{};
  AnimSpriteProvider sprites;
  data::SpriteDb effects;

  bool load(const std::string& game_dir);
  data::LightMask mask(int id) const {
    auto it = masks.find(id);
    return it == masks.end() ? data::LightMask{} : data::LightMask({it->second.data(), it->second.size()});
  }
};

// Integer grid operations shared by fog updates and acquisition checks.
inline void stamp_sight(std::vector<uint8_t>& grid, int w, int h, data::LightMask mask,
                        int x, int y, bool vision) {
  if (!mask.ok() || w <= 0 || h <= 0 || grid.size() != size_t(w) * h) return;
  for (int my = 0; my < 20; ++my)
    for (int mx = 0; mx < 20; ++mx) if (mask.sample(mx, my) < 0) return;
  int left = x - 9 * 48, top = y - 9 * 48;
  auto floor_cell = [](int p) { return p >= 0 ? p / 48 : (p - 47) / 48; };
  int x0 = floor_cell(left), y0 = floor_cell(top);
  for (int gy = std::max(0, y0); gy < h && gy < y0 + 19; ++gy)
    for (int gx = std::max(0, x0); gx < w && gx < x0 + 19; ++gx) {
      // Grid cells select the next mask vertex at aligned positions.
      int dx = gx * 48 - left + 48, dy = gy * 48 - top + 48;
      int ix = dx / 48, iy = dy / 48, rx = dx % 48, ry = dy % 48;
      auto sample = [&](int xx, int yy) { return std::max(0, mask.sample(xx, yy)); };
      // Each weighted contribution truncates before the next axis is blended.
      int a = sample(ix, iy) * (48 - rx) / 48 + sample(ix + 1, iy) * rx / 48;
      int b = sample(ix, iy + 1) * (48 - rx) / 48 + sample(ix + 1, iy + 1) * rx / 48;
      int value = a * (48 - ry) / 48 + b * ry / 48;
      auto& cell = grid[size_t(gy) * w + gx];
      cell = uint8_t(vision ? std::min(int(cell), value) : std::min(152, cell + value));
    }
}

inline int sight_brightness(const VisFile::Layer& layer, int x, int y) {
  if (x < 0 || y < 0 || layer.w <= 0 || layer.h <= 0 ||
      layer.v.size() != size_t(layer.w) * layer.h) return 0;
  int gx = x / 48, gy = y / 48;
  if (gx >= layer.w || gy >= layer.h) return 0;
  int rx = x % 48, ry = y % 48;
  auto at = [&](int xx, int yy) { return int(layer.v[size_t(std::min(yy, layer.h - 1)) * layer.w + std::min(xx, layer.w - 1)]); };
  // Retain 8.8 fractions through the final strict threshold comparison.
  int fx = rx * 256 / 48, fy = ry * 256 / 48;
  return at(gx, gy) * (256 - fx) * (256 - fy) / 256 +
         at(gx + 1, gy) * fx * (256 - fy) / 256 +
         at(gx, gy + 1) * (256 - fx) * fy / 256 +
         at(gx + 1, gy + 1) * fx * fy / 256;
}

}  // namespace aztec::engine
