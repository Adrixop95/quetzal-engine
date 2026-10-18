#include "core/sprite.hpp"

#include <cstring>

namespace aztec {

static uint32_t rd32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
static uint16_t rd16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }

bool decode_sprite(Item item, Sprite& s) {
  const uint8_t* d = item.data;
  if (!item || item.size < 0x18 || rd32(d) != 0xE0AC0000u) return false;
  s.w = rd16(d + 4);
  s.h = rd16(d + 6);
  s.flags = d[8];
  s.ox = int16_t(rd16(d + 0x14));
  s.oy = int16_t(rd16(d + 0x16));
  size_t n = size_t(s.w) * s.h;
  s.opaque.assign(n, 0);
  s.index.clear();
  s.color.clear();
  bool wide = (s.flags & (kRaw16 | kRle16)) != 0;
  if (wide) s.color.assign(n, 0); else s.index.assign(n, 0);

  auto put = [&](size_t pos, const uint8_t* px) {
    s.opaque[pos] = 1;
    if (wide) s.color[pos] = rd16(px); else s.index[pos] = *px;
  };
  const size_t bpp = wide ? 2 : 1;

  if (s.flags & kRaw) {
    if (0x18 + n * bpp > item.size) return false;
    for (size_t i = 0; i < n; i++) put(i, d + 0x18 + i * bpp);
    return true;
  }
  if (0x18 + size_t(s.h) * 8 > item.size) return false;
  for (int y = 0; y < s.h; y++) {
    size_t ctrl = rd32(d + 0x18 + 8 * y), pix = rd32(d + 0x1c + 8 * y);
    int x = 0;
    bool draw = false;
    while (x < s.w) {
      if (ctrl >= item.size) return false;
      unsigned c = d[ctrl++];
      unsigned count = c;
      bool is_draw = draw;
      if (s.flags & kRle16) {
        count = c & 0x3f;
        if (c < 0x80) count = c;
        is_draw = c >= 0xc0;
      } else {
        draw = !draw;
      }
      for (unsigned k = 0; k < count && x < s.w; k++, x++) {
        if (is_draw) {
          if (pix + bpp > item.size) return false;
          put(size_t(y) * s.w + x, d + pix);
        }
        if (is_draw) pix += bpp;  // keep pixel cursor in step even if clipped
      }
      if (is_draw && x >= s.w) break;
    }
  }
  return true;
}

std::vector<uint32_t> to_argb(const Sprite& s, const Palette& pal) {
  std::vector<uint32_t> out(size_t(s.w) * s.h, 0);
  for (size_t i = 0; i < out.size(); i++) {
    if (!s.opaque[i]) continue;
    if (s.is16()) {
      uint32_t v = s.color[i];
      uint32_t r = ((v >> 11) & 31) * 255 / 31, g = ((v >> 5) & 63) * 255 / 63, b = (v & 31) * 255 / 31;
      out[i] = 0xFF000000u | r << 16 | g << 8 | b;
    } else {
      out[i] = pal[s.index[i]];
    }
  }
  return out;
}

}  // namespace aztec
