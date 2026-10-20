#include "core/sprite_alpha.hpp"

#include <cstring>

namespace aztec {

static uint32_t r32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
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
  s.opaque.assign(n, 0);
  s.index.assign(n, 0);
  s.alpha.assign(n, 0);
  if (0x18 + size_t(s.h) * 8 > item.size) return false;
  for (int y = 0; y < s.h; y++) {
    size_t ctrl = r32(d + 0x18 + 8 * y), pix = r32(d + 0x1c + 8 * y);
    int x = 0;
    while (x < s.w) {
      if (ctrl >= item.size) return false;
      unsigned c = d[ctrl++];
      if (c < 0x80) { x += int(c); continue; }
      unsigned cnt = c & 0x3f;
      bool blend = c >= 0xC0;
      for (unsigned k = 0; k < cnt; k++, x++) {
        size_t need = blend ? 2 : 1;
        if (pix + need > item.size) return false;
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

std::vector<uint32_t> alpha_to_argb(const AlphaSprite& s, const Palette& pal) {
  std::vector<uint32_t> out(size_t(s.w) * s.h, 0);
  for (size_t i = 0; i < out.size(); i++)
    if (s.opaque[i]) out[i] = (pal[s.index[i]] & 0x00FFFFFFu) | uint32_t(s.alpha[i]) << 24;
  return out;
}

}  // namespace aztec
