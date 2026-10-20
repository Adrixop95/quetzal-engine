// Sprites with header flag 0x40 ("indexed + alpha RLE"). Decoded from the 16-bit blitters
// (FUN_0047f0a0 / FUN_0047f270); src/core/sprite.cpp treats these as 16-bit colour, which is wrong.
#pragma once
#include <cstdint>
#include <vector>
#include "core/archive.hpp"
#include "core/palette.hpp"

namespace aztec {

struct AlphaSprite {
  int w = 0, h = 0, ox = 0, oy = 0;
  int palette = 0;               // header +0xC: Pal.xbe palette id to use
  std::vector<uint8_t> opaque;   // w*h, 1 = drawn
  std::vector<uint8_t> index;    // w*h palette index
  std::vector<uint8_t> alpha;    // w*h, 255 = solid; other values only on "blended" pixels
};

// Row stream (row table at +0x18, 8 bytes per row: ctrl offset, pixel offset), control byte c:
//   c < 0x80   skip c pixels
//   0x80..0xBF draw (c & 0x3f) solid pixels, 1 byte each (palette index)
//   0xC0..0xFF draw (c & 0x3f) blended pixels, 2 bytes each (palette index, blend/alpha byte)
bool decode_alpha_sprite(Item item, AlphaSprite& out);

// The blitters (FUN_0047f0a0) compute, per 5-bit channel, out = min(31, (pal >> 3) + floor(dest * (255 - a) / 255))
// (table 0x4bd25c is exactly that), i.e. the palette colour is PREMULTIPLIED by the blend byte `a`
// (255 = opaque, small = nearly transparent). Solid pixels (ctrl 0x80..0xBF) are a = 255.
int blend_channel5(int dest5, int src5, int a);                 // one 5-bit channel
uint32_t blend_over_rgb(uint32_t dst_rgb, uint32_t pal_rgb, int a);  // 0xRRGGBB, 5-bit precision like the game

// ARGB with straight (non-premultiplied) alpha: colour = pal * 255 / a clamped, alpha = a.
std::vector<uint32_t> alpha_to_argb(const AlphaSprite& s, const Palette& pal);

}  // namespace aztec
