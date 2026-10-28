#include "core/endian.hpp"
#include "core/loader_limits.hpp"
#include "core/sprite_alpha.hpp"

#include <cstring>

namespace aztec {

static uint32_t r32(const uint8_t* p) { return read_le<uint32_t>(p); }
static int r16(const uint8_t* p) { return p[0] | p[1] << 8; }

bool decode_alpha_sprite(Item item, AlphaSprite& s) {
  const uint8_t* d = item.data;
  if (!item || item.size < 0x18 || r32(d) != 0xE0AC0000u || !(d[8] & 0x40)) return false;
  s = AlphaSprite{};
  s.w = r16(d + 4);
  s.h = r16(d + 6);
  s.palette = r16(d + 12);
  s.ox = int16_t(r16(d + 0x14));
  s.oy = int16_t(r16(d + 0x16));
  size_t n = size_t(s.w) * s.h;
  // Reject implausible dimensions before allocating decoded planes.
  if (!n || n > detail::kPixels) return false;
  s.opaque.assign(n, 0);
  s.index.assign(n, 0);
  s.alpha.assign(n, 0);
  if (0x18 + size_t(s.h) * 8 > item.size) return false;
  for (int y = 0; y < s.h; y++) {
    size_t ctrl = r32(d + 0x18 + 8 * y), pix = r32(d + 0x1c + 8 * y);
    int x = 0;
    unsigned idle = 0;
    while (x < s.w) {
      if (ctrl >= item.size) return false;
      unsigned c = d[ctrl++];
      if (!(c < 0x80 ? c : c & 0x3f)) {
        if (++idle > 2) return false;
      } else idle = 0;
      if (c < 0x80) { x += int(c); continue; }
      unsigned cnt = c & 0x3f;
      bool blend = c >= 0xC0;
      for (unsigned k = 0; k < cnt; k++, x++) {
        size_t need = blend ? 2 : 1;
        if (pix > item.size || need > item.size - pix) return false;
        if (x < s.w) {
          size_t pos = size_t(y) * s.w + x;
          s.opaque[pos] = 1;
          s.index[pos] = d[pix];
          s.alpha[pos] = blend ? d[pix + 1] : 255;
        }
        pix += need;
      }
    }
  }
  return true;
}

int blend_channel5(int dest5, int src5, int a) {
  int v = src5 + dest5 * (255 - a) / 255;
  return v > 31 ? 31 : v;
}

uint32_t blend_over_rgb(uint32_t dst, uint32_t pal, int a) {
  uint32_t out = 0;
  for (int sh = 16; sh >= 0; sh -= 8) {
    int v = blend_channel5(int((dst >> sh) & 255) >> 3, int((pal >> sh) & 255) >> 3, a);
    out |= uint32_t((v << 3) | (v >> 2)) << sh;
  }
  return out;
}

std::vector<uint32_t> alpha_to_argb(const AlphaSprite& s, const Palette& pal) {
  std::vector<uint32_t> out(size_t(s.w) * s.h, 0);
  for (size_t i = 0; i < out.size(); i++) {
    if (!s.opaque[i]) continue;
    uint32_t c = pal[s.index[i]], a = s.alpha[i];
    uint32_t px = a << 24;
    for (int sh = 16; sh >= 0; sh -= 8) {
      uint32_t v = (c >> sh) & 255;
      if (a && a < 255) { v = v * 255 / a; if (v > 255) v = 255; }
      px |= v << sh;
    }
    out[i] = px;
  }
  return out;
}

}  // namespace aztec
