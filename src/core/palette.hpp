#pragma once
#include <array>
#include <cstdint>
#include "core/archive.hpp"

namespace aztec {

// 256-entry palette converted to 0xAARRGGBB.
using Palette = std::array<uint32_t, 256>;

// Palette `id` (1-based item number in Pal.xbe, group 0x450 = RGB565) at shade level `shade`
// (0..19 for items of 10240 bytes, 0 for single-palette items). Returns false if absent.
bool load_palette(const Archive& pal_xbe, uint32_t id, unsigned shade, Palette& out);

}  // namespace aztec
