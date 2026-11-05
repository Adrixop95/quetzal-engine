// Independent implementation of the grid contract in docs/sprites_mapping.md.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/data/light_masks.hpp"
#include "engine/sim/sight.hpp"

namespace aztec::engine {

class LightGrid {
 public:
  void reset(int map_width, int map_height, int ambient) {
    w_ = std::max(0, map_width) + 1; h_ = std::max(0, map_height) + 2;
    glow_.assign(size_t(w_) * h_, std::clamp(ambient, 0, data::kMaxLightValue));
    vision_.assign(glow_.size(), data::kMaxLightValue);
    if (combined_.size() != glow_.size()) {
      combined_.assign(glow_.size(), 0);
      explored_.assign(glow_.size(), 0);
    }
  }
  void stamp(data::LightMask mask, float x, float y, bool vision) {
    if (!mask.ok() || !std::isfinite(x) || !std::isfinite(y)) return;
    // Share the simulation's next-vertex origin and per-axis truncation.
    if (x < -1000000 || y < -1000000 || x > 1000000 || y > 1000000) return;
    stamp_sight(vision ? vision_ : glow_, w_, h_, mask, int(std::floor(x)), int(std::floor(y)), vision);
  }
  void combine(bool use_vision) {
    for (size_t i = 0; i < glow_.size(); ++i) {
      if (use_vision) {
        explored_[i] = std::max(std::min(explored_[i], 32), std::max(0, int(glow_[i]) - vision_[i]));
        combined_[i] = explored_[i];
      } else combined_[i] = glow_[i];
    }
  }
  float sample(float x, float y) const {
    if (combined_.empty()) return 0;
    const float gx = std::clamp(x / data::kTilePixels, 0.f, float(w_ - 1));
    const float gy = std::clamp(y / data::kTilePixels, 0.f, float(h_ - 1));
    const int ix = int(gx), iy = int(gy), jx = std::min(ix + 1, w_ - 1), jy = std::min(iy + 1, h_ - 1);
    const float fx = gx - ix, fy = gy - iy;
    auto at = [&](int xx, int yy) { return combined_[size_t(yy) * w_ + xx]; };
    return (at(ix, iy) * (1 - fx) + at(jx, iy) * fx) * (1 - fy) +
           (at(ix, jy) * (1 - fx) + at(jx, jy) * fx) * fy;
  }
  int width() const { return w_; }
  int height() const { return h_; }
 private:
  int w_ = 0, h_ = 0;
  std::vector<uint8_t> glow_, vision_;
  std::vector<int> combined_, explored_;
};

// Equal keys retain insertion order; underlay mutual order is unspecified.
inline bool display_before(float ax, float ay, int ah, float bx, float by, int bh) {
  if ((ah == -1) != (bh == -1)) return ah == -1;
  if (ah == -1) return false;
  return ay != by ? ay < by : ax > bx;
}
}  // namespace aztec::engine
