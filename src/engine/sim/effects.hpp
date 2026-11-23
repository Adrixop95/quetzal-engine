// Deterministic transient objects. Positions use pixel * 256; age uses 12 Hz ticks.
#pragma once
#include <cstdint>
#include "engine/data/effects.hpp"

namespace aztec::engine {
struct SoundChoice {
  uint64_t source = 0; // scene source key, independent of audio voice handles
  uint32_t set = 0, sound = 0, pitch = 0; // pitch is the raw 15-bit CRT draw
  bool loop = false;
};

// One-tick descriptor cues are resolved by the simulation, before view filtering.
struct CombatSound {
  uint64_t source = 0;
  uint32_t set = 0, sound = 0;
  int32_t x = 0, y = 0;
  int side = 0, volume_offset = 0; // DirectSound hundredths of a decibel
  bool loop = false;
};

struct Effect {
  uint64_t trace_id = 0; // diagnostic identity only; never read by combat rules
  data::EffectClass kind = data::EffectClass::Decoration;
  uint32_t source = 0, target = 0;
  int side = 0, anim = 0, slot = 0, angle = 0;
  int32_t x = 0, y = 0, prev_x = 0, prev_y = 0;
  int32_t end_x = 0, end_y = 0;
  int height = 0, age = 0, lifetime = 0, speed = 0;
  int end_height = 0; // snapshotted arrow aim height
  int damage = 0, splash_px = 0, impact_anim = 0, shockwave = 0;
  int payload_id = -1; // spatial blast evaluated independently for every victim
  int weapon_id = -1; // class 3 contact damage is evaluated at each flight step
  int delay_ticks = -1; // class 8: scheduled explosion, no travelling rocket
  bool flame = false; // class 11: follows its source and scans a segment each active tick
  int32_t velocity_x = 0, velocity_y = 0, altitude = 0, vertical_speed = 0, gravity = 0;
  bool flight = false, loop = false;
  bool projectile_pool = false; // lifetime ownership survives flight-to-impact animation transitions
};
}  // namespace aztec::engine
