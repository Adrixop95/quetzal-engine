// Independently written presentation rules; see docs/renderer_rules.md.
#pragma once
#include <algorithm>
#include <cstdint>

namespace aztec::engine {

constexpr int surface_shade(int byte) { return std::clamp(byte, 0, 152) >> 3; }
constexpr int terrain_flat_shade(int byte) { return (std::clamp(byte, 0, 152) + 4) >> 3; }
constexpr int placement_shade(int status) { return status == 0 ? 20 : status == 1 ? 21 : 22; }
constexpr bool flash_tick(bool flash, unsigned counter) { return flash && (counter & 1); }
constexpr int health_frame(int ratio) { return ratio > 178 ? 0 : ratio > 102 ? 1 : 2; }
constexpr int health_width(int width, int ratio) { return int(int64_t(width) * std::clamp(ratio, 0, 256) >> 8); }
constexpr int team_palette(int palette, int side) { return palette < 4 ? palette + 16 + side * 4 : palette; }

// V: bilinear 48-pixel blocks and 4x4 ordered quantization. G: Bayer
// thresholds stand in for the unresolved original dither table contents.
inline int light_surface_shade(int a, int b, int c, int d, int x, int y) {
  if (a == b && a == c && a == d) return surface_shade(a);
  constexpr int threshold[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  int rx = (x % 48 + 48) % 48, ry = (y % 48 + 48) % 48;
  int value = a * (48 - rx) * (48 - ry) + b * rx * (48 - ry) + c * (48 - rx) * ry + d * rx * ry;
  return std::clamp((value * 16 + threshold[y & 3][x & 3] * 8 * 2304) / (8 * 2304 * 16), 0, 19);
}

struct StatusArt { int threshold, ellipse, health; };
inline constexpr StatusArt status_art[] = {
  {44, 308, 263}, {55, 309, 262}, {67, 310, 261}, {83, 311, 260},
  {102, 312, 259}, {125, 313, 258}, {154, 314, 258}, {189, 315, 258}
};
constexpr int status_art_index(int size) {
  int i = 0;
  while (i < 7 && int64_t(size) * 2 > status_art[i].threshold) ++i;
  return i;
}
}  // namespace aztec::engine
