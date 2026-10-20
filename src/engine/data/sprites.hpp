// Type + state -> sprite mapping for Units.xbe animations. Header only; see docs/sprites_mapping.md.
// Loads Anim items (group 0x515) at runtime from the user's own Res/Units.xbe (no game data embedded).
#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include "core/archive.hpp"
#include "type_anims.hpp"

namespace aztec::data {

enum class AnimState { Idle, Move, Attack, Die, Fidget, Icon };

// One 32-byte frame record. Sprite ids index groups 0x44c (body) and 0x454 (shadow, same id) of Res/Units.xbe.
struct SpriteRef {
  int32_t sprite = 0;    // 0 = nothing to draw
  int32_t light = 0;     // light.xbe 0x455 id (muzzle glow), 0 = none
  int32_t sound = 0;     // sound id played on the first tick of the frame
  int32_t fire = 0;      // non-zero: weapon fires on this frame
  int32_t dx = 0, dy = 0;  // world displacement (px) applied each tick while the frame is shown
  int32_t yoff = 0;      // extra y offset added to the draw position
  int32_t duration = 0;  // ticks at 12/s; 0 = hold forever
  bool valid() const { return sprite != 0; }
};

class AnimSet {
 public:
  explicit AnimSet(Item it = {}) : d_(it.data), n_(it.size) {}
  bool ok() const { return n_ > 0x208 && !std::memcmp(d_, "Anim", 4); }
  uint32_t slot_start(int slot) const { return slot < 0 || slot >= 128 ? 0 : u32(8 + 4 * slot); }
  // Frame `idx` of sequence `slot` (invalid ref past the terminator).
  SpriteRef frame(int slot, int idx) const {
    SpriteRef r; uint32_t s = slot_start(slot);
    if (!s || idx < 0) return r;
    for (int i = 0; i <= idx; i++) { size_t o = 0x208 + size_t(s + i) * 32; if (o + 32 > n_ || u32(o) == 0) return r; }
    size_t o = 0x208 + size_t(s + idx) * 32;
    r = {int32_t(u32(o)), int32_t(u32(o + 4)), int32_t(u32(o + 8)), int32_t(u32(o + 12)), int32_t(u32(o + 16)), int32_t(u32(o + 20)), int32_t(u32(o + 24)), int32_t(u32(o + 28))};
    return r;
  }
  int frame_count(int slot) const { int n = 0; while (frame(slot, n).valid()) n++; return n; }
  // Looping sound id of the state (header frame field [2]).
  int loop_sound(int slot) const { uint32_t s = slot_start(slot); return s ? int32_t(u32(0x208 + size_t(s - 1) * 32 + 8)) : 0; }
  // Frame shown `tick` ticks after the sequence start (honours durations; loops when `loop`).
  SpriteRef at_tick(int slot, int tick, bool loop) const {
    int total = 0, n = frame_count(slot);
    for (int i = 0; i < n; i++) { int dur = frame(slot, i).duration; if (!dur) return frame(slot, i); total += dur; }
    if (!total) return {};
    if (loop) tick %= total; else if (tick >= total) return {};
    for (int i = 0; i < n; i++) { SpriteRef f = frame(slot, i); if (tick < f.duration) return f; tick -= f.duration; }
    return {};
  }
 private:
  uint32_t u32(size_t o) const { uint32_t v; std::memcpy(&v, d_ + o, 4); return v; }
  const uint8_t* d_; size_t n_;
};

// Direction 0..7 counter-clockwise from east on screen: dir = ((angle + 0x10) >> 5) & 7, angle 0..255 as in original FUN_00461ac0
// (0 = +x, 0x40 = up, 0x80 = -x, 0xC0 = down).
inline int direction_from_angle(int angle) { return ((angle + 0x10) >> 5) & 7; }

// Sequence slot for a state (mobile units, kinds 0,1,4,5,6,9,10 follow the Marine convention; see docs).
inline int slot_for(AnimState s, int dir, int variant = 0) {
  switch (s) {
    case AnimState::Idle: return 0;            // 8 one-tick frames, frame index = direction
    case AnimState::Move: return 1 + dir;      // looping
    case AnimState::Attack: return 9 + dir;    // one shot
    case AnimState::Die: return 17 + variant % 3;
    case AnimState::Fidget: return 20 + variant % 3;
    case AnimState::Icon: return 23;
  }
  return 0;
}

class SpriteDb {
 public:
  bool open(const std::string& units_xbe_path) { return ar_.open(units_xbe_path); }
  AnimSet anim(int id) const { return AnimSet(ar_.get(0x515, id)); }
  // Main body anim of a type (index 0) or second anim (weapon/turret, index 1).
  int anim_id(int type, int which = 0) const { return type < 0 || type >= 90 ? 0 : (which ? kTypeAnims[type].anim2 : kTypeAnims[type].anim); }
  // Frame for (type, state, direction 0..7, tick since state start). Idle uses frame index = direction.
  SpriteRef sprite_for(int type, AnimState st, int dir, int tick, int variant = 0) const {
    AnimSet a = anim(anim_id(type));
    if (!a.ok()) return {};
    int slot = slot_for(st, dir, variant);
    if (st == AnimState::Idle) return a.frame(0, dir & 7);
    if (st == AnimState::Icon) return a.frame(23, dir);
    return a.at_tick(slot, tick, st == AnimState::Move);
  }
 private:
  Archive ar_;
};

}  // namespace aztec::data
