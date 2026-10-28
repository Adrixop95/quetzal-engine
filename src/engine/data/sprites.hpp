// Type + state -> sprite mapping for Units.xbe animations. Header only; see docs/sprites_mapping.md.
// Loads Anim items (group 0x515) at runtime from the user's own Res/Units.xbe (no game data embedded).
#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include "core/archive.hpp"
#include "type_anims.hpp"
#include "animation_slots.hpp"

namespace aztec::data {

// One 32-byte frame record. Sprite ids index groups 0x44c (body) and 0x454 (shadow, same id) of Res/Units.xbe.
struct SpriteRef {
  int32_t sprite = 0;    // 0 = nothing to draw
  int32_t light = 0;     // light.xbe 0x455 id (muzzle glow), 0 = none
  int32_t sound = 0;     // sound-set id played on the first tick of the frame
  int32_t fire = 0;      // non-zero: weapon fires on this frame
  int32_t dx = 0, dy = 0;  // world displacement (px) applied each tick while the frame is shown
  int32_t yoff = 0;      // height subtracted from draw Y (ground units); see render_rules.hpp
  int32_t duration = 0;  // ticks at 12/s; 0 = hold forever
  bool sound_entry = true;  // runtime lookup metadata, not part of the bank record
  int32_t loop_sound = 0;
  bool valid() const { return sprite != 0; }
};

class AnimSet {
 public:
  explicit AnimSet(Item it = {}) : d_(it.data), n_(it.size) {}
  bool ok() const { return d_ && n_ >= 0x228 && !std::memcmp(d_, "Anim", 4) && u32(4) == 3; }
  uint32_t slot_start(int slot) const { return !ok() || slot < 0 || slot >= 128 ? 0 : u32(8 + 4 * slot); }
  // Frame `idx` of sequence `slot` (invalid ref past the terminator).
  SpriteRef frame(int slot, int idx) const {
    SpriteRef r; uint32_t s = slot_start(slot);
    if (!s || idx < 0) return r;
    for (int i = 0; i <= idx; i++) { uint64_t o = 0x208 + (uint64_t(s) + i) * 32; if (o + 32 > n_ || u32(size_t(o)) == 0) return r; }
    size_t o = size_t(0x208 + (uint64_t(s) + idx) * 32);
    r = {int32_t(u32(o)), int32_t(u32(o + 4)), int32_t(u32(o + 8)), int32_t(u32(o + 12)), int32_t(u32(o + 16)), int32_t(u32(o + 20)), int32_t(u32(o + 24)), int32_t(u32(o + 28))};
    return r;
  }
  int frame_count(int slot) const { int n = 0; while (frame(slot, n).valid()) n++; return n; }
  // Looping sound id of the state (header frame field [2]).
  int loop_sound(int slot) const {
    uint32_t s = slot_start(slot);
    uint64_t o = 0x208 + uint64_t(s ? s - 1 : 0) * 32 + 8;
    return s && o + 4 <= n_ ? int32_t(u32(size_t(o))) : 0;
  }
  // Frame shown `tick` ticks after the sequence start (honours durations; loops when `loop`).
  SpriteRef at_tick(int slot, int64_t tick, bool loop) const {
    if (tick < 0) return {};
    int64_t total = 0, elapsed = tick;
    int n = frame_count(slot);
    bool hold = false;
    for (int i = 0; i < n; i++) {
      int dur = frame(slot, i).duration;
      if (dur < 0) return {};
      if (!dur) { hold = true; break; }
      total += dur;
    }
    if (!hold) {
      if (!total) return {};
      if (loop) elapsed %= total; else if (elapsed >= total) return {};
    }
    for (int i = 0; i < n; i++) {
      SpriteRef f = frame(slot, i);
      if (!f.duration || elapsed < f.duration) {
        f.sound_entry = elapsed == 0;
        f.loop_sound = loop_sound(slot);
        return f;
      }
      elapsed -= f.duration;
    }
    return {};
  }
 private:
  uint32_t u32(size_t o) const { uint32_t v; std::memcpy(&v, d_ + o, 4); return v; }
  const uint8_t* d_; size_t n_;
};

// Marine-only compatibility helper. Use body_sequence/turret_sequence for other kinds.
inline int slot_for(AnimState state, int dir, int variant = 0) {
  return body_sequence(0, state, (dir & 7) * 32, variant).slot;
}

class SpriteDb {
 public:
  bool open(const std::string& units_xbe_path) { return ar_.open(units_xbe_path); }
  AnimSet anim(int id) const { return AnimSet(ar_.get(0x515, id)); }
  // Main body anim of a type (index 0) or second anim (weapon/turret, index 1).
  int anim_id(int type, int which = 0) const { return type < 0 || type >= 90 ? 0 : (which ? kTypeAnims[type].anim2 : kTypeAnims[type].anim); }
  // Frame for (type, state, direction 0..7, tick since state start). Idle uses frame index = direction.
  SpriteRef sprite_for(int type, AnimState st, int dir, int tick, int variant = 0) const {
    if (type < 0 || type >= 90) return {};
    AnimSet a = anim(anim_id(type));
    if (!a.ok()) return {};
    AnimSequence sequence = body_sequence(kTypeAnims[type].kind, st, (dir & 7) * 32,
                                          st == AnimState::Icon ? dir : variant,
                                          true, kTypeAnims[type].anim2 != 0);
    return sprite_for_sequence(a, sequence, tick);
  }
  // Full-angle callers can select body/turret/transport sequences explicitly.
  static SpriteRef sprite_for_sequence(const AnimSet& a, AnimSequence sequence, int64_t tick) {
    if (!a.ok() || !sequence.valid()) return {};
    if (sequence.frame < 0) return a.at_tick(sequence.slot, tick, sequence.loop);
    auto r = a.frame(sequence.slot, sequence.frame);
    r.sound_entry = tick == 0; r.loop_sound = a.loop_sound(sequence.slot);
    return r;
  }

 private:
  Archive ar_;
};

}  // namespace aztec::data
