#pragma once
#include <cstdint>
#include <vector>
#include "core/archive.hpp"
#include "core/palette.hpp"

namespace aztec {

enum SpriteFlags : uint8_t {
  kRaw = 0x01,      // uncompressed rows
  kIndexed = 0x02,  // RLE, 8-bit palette indices
  kShaded = 0x04,
  kTranslucent = 0x08,
  kRaw16 = 0x10,    // with kRaw: 16-bit pixels
  kRle16 = 0x40,    // RLE, 16-bit colour
};

// Decoded sprite: `index` holds palette indices (8-bit sprites), `color` holds RGB565 (16-bit
// sprites). `opaque` marks drawn pixels.
struct Sprite {
  int w = 0, h = 0, ox = 0, oy = 0;
  uint8_t flags = 0;
  int cell_w = 0, cell_h = 0;      // header +0x10/+0x12: size of one image when the sprite stacks several (0 = single)
  int palette = 0;                 // header +0xC: Pal.xbe id (< 4: team palette, see docs/sprites_mapping.md)
  std::vector<uint8_t> opaque;     // w*h, 1 = drawn
  std::vector<uint8_t> index;      // w*h (8-bit sprites)
  std::vector<uint16_t> color;     // w*h (16-bit sprites)
  bool is16() const { return !color.empty(); }
};

bool decode_sprite(Item item, Sprite& out);

// Converts to ARGB32 using `pal` for 8-bit sprites (RGB565 for 16-bit ones).
std::vector<uint32_t> to_argb(const Sprite& s, const Palette& pal);

}  // namespace aztec
