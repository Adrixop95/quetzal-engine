// Deterministic transient objects. Positions use pixel * 256; age uses 12 Hz ticks.
#pragma once
#include <cstdint>
#include "engine/data/effects.hpp"

namespace aztec::engine {
struct Effect {
  data::EffectClass kind = data::EffectClass::Decoration;
  uint32_t source = 0, target = 0;
  int side = 0, anim = 0, slot = 0, angle = 0;
  int32_t x = 0, y = 0, prev_x = 0, prev_y = 0;
  int32_t end_x = 0, end_y = 0;
  int height = 0, age = 0, lifetime = 0, speed = 0;
  int damage = 0, splash_px = 0, impact_anim = 0, shockwave = 0;
  bool flight = false, loop = false;
};
}  // namespace aztec::engine
