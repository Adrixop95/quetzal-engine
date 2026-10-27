// Non-owning reader of light.xbe mask data supplied by the user.
#pragma once
#include "core/archive.hpp"
#include "render_rules.hpp"

namespace aztec::data {

class LightMask {
 public:
  explicit LightMask(Item item = {}) : item_(item) {}
  bool ok() const {
    return item_.data && item_.size == kLightMaskHeaderBytes + kLightMaskWidth * kLightMaskHeight &&
           item_.data[0] == kLightMaskWidth && item_.data[1] == 0 &&
           item_.data[2] == kLightMaskHeight && item_.data[3] == 0;
  }
  // Returns -1 for missing/invalid masks, invalid coordinates or invalid values.
  int sample(int x, int y) const {
    if (!ok() || x < 0 || y < 0 || x >= kLightMaskWidth || y >= kLightMaskHeight) return -1;
    const int value = item_.data[kLightMaskHeaderBytes + y * kLightMaskWidth + x];
    return value <= kMaxLightValue ? value : -1;
  }
  // Sample a translated mask at a world-grid vertex, relative to its origin.
  // Samples outside the 19x19-cell extent contribute no glow.
  int sample_pixels(int dx, int dy) const {
    constexpr int cell = kTilePixels;
    if (dx < 0 || dy < 0 || dx > 19 * cell || dy > 19 * cell) return 0;
    int x = dx / cell, y = dy / cell, rx = dx % cell, ry = dy % cell;
    auto value = [&](int sx, int sy) { int v = sample(sx, sy); return v < 0 ? 0 : v; };
    return (value(x, y) * (cell - rx) * (cell - ry) + value(x + 1, y) * rx * (cell - ry) +
            value(x, y + 1) * (cell - rx) * ry + value(x + 1, y + 1) * rx * ry) / (cell * cell);
  }
 private:
  Item item_;
};

// Values in the combined map are brightness: zero selects the darkest palette.
// The +4 rounding is visible in the uniform terrain blitter call at 0x466357.
constexpr int palette_shade(int brightness) {
  if (brightness <= 0) return 0;
  return brightness >= kMaxLightValue ? 19 : (brightness + 4) / 8;
}

}  // namespace aztec::data
