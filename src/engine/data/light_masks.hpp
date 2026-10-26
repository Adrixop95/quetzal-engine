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
