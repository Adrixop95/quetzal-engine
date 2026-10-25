#include "core/palette.hpp"

namespace aztec {

bool load_palette(const Archive& pal_xbe, uint32_t id, unsigned shade, Palette& out) {
  Item it = pal_xbe.get(0x450, id);
  if (!it || it.size < 512 || shade > (it.size - 512) / 512) return false;
  size_t off = size_t(shade) * 512;
  for (int i = 0; i < 256; i++) {
    uint32_t v = it.data[off + 2 * i] | (it.data[off + 2 * i + 1] << 8);
    uint32_t r = ((v >> 11) & 31) * 255 / 31, g = ((v >> 5) & 63) * 255 / 63, b = (v & 31) * 255 / 31;
    out[i] = 0xFF000000u | r << 16 | g << 8 | b;
  }
  return true;
}

}  // namespace aztec
