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

// ARGB with straight alpha: solid pixels alpha 255, blended pixels alpha = blend byte.
std::vector<uint32_t> alpha_to_argb(const AlphaSprite& s, const Palette& pal);

}  // namespace aztec
