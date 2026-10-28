// Animation selection facts; no executable code or image data is embedded.
// Evidence and remaining screenshot gaps: docs/sprites_mapping.md.
#pragma once
#include <cstdint>

namespace aztec::data {

enum class AnimState { Idle, Move, Attack, Die, Fidget, Icon, Construct };

struct AnimSequence {
  int slot = -1;
  int frame = -1;  // -1: advance by frame durations; otherwise seek this frame
  bool loop = false;
  constexpr bool valid() const { return slot >= 0; }
};

constexpr int direction_from_angle(int angle) { return ((unsigned(angle) + 16) >> 5) & 7; }
constexpr int direction32_from_angle(int angle) { return ((unsigned(angle) + 4) >> 3) & 31; }

// Angle is a full 0..255 facing. Variant selects death/fidget/icon alternatives.
// For Bird/Zeppelin/Truck, cycle_ready is false while turning between dir8 centres;
// Bird also clears it during its flight steering cooldown (+0x8c).
constexpr AnimSequence body_sequence(int kind, AnimState state, int angle,
                                     int variant = 0, bool cycle_ready = true, bool has_turret = false) {
  const int d8 = direction_from_angle(angle), d32 = direction32_from_angle(angle);
  const int v = variant < 0 ? 0 : variant;
  if (kind == 0) {
    switch (state) {
      case AnimState::Idle: return {0, d8, false};
      case AnimState::Move: return {1 + d8, -1, true};
      case AnimState::Attack: return {9 + d8, -1, false};
      case AnimState::Die: return {17 + v % 3, -1, false};
      case AnimState::Fidget: return {20 + v % 3, -1, false};
      case AnimState::Icon: return {23, v, false};
      case AnimState::Construct: return {};
    }
  }
  if (state == AnimState::Icon) {
    switch (kind) {
      case 1: return {9, v, false};  // add descriptor race * 4 to variant
      case 2: case 7: return {7, v, false};
      case 3: case 4: case 5: return {23, v, false};
      case 6: return {39, v, false};  // add descriptor race * 4 to variant
      case 9: return {17, v, false};
      default: return {};
    }
  }
  switch (kind) {
    case 1:  // Spider: firing changes the separate turret, never the leg animation.
      if (state == AnimState::Move) return {1 + d8, -1, true};
      if (state == AnimState::Idle || state == AnimState::Attack) return {0, 0, false};
      break;
    case 2: case 7:
      if (state == AnimState::Idle || (kind == 7 && state == AnimState::Attack))
        return {state == AnimState::Attack && !has_turret ? 8 : 1, -1,
                state == AnimState::Idle || has_turret};
      break;
    case 3: case 5:
      if (state == AnimState::Idle) return {0, d32, false};
      if (state == AnimState::Move)
        return cycle_ready ? AnimSequence{1 + d8, -1, true} : AnimSequence{0, d32, false};
      // These slots are requested on death, but absent in the current flight resources.
      if (state == AnimState::Die) return {17 + v % 3, -1, false};
      break;
    case 4:
      if (state == AnimState::Idle) return {0, d32, false};
      if (state == AnimState::Move)
        return cycle_ready ? AnimSequence{1 + d8, -1, true} : AnimSequence{0, d32, false};
      if (state == AnimState::Attack) return {9 + d8, -1, false};
      break;
    case 6:
      if (state == AnimState::Idle || state == AnimState::Attack) return {1 + d32, 0, false};
      if (state == AnimState::Move) return {1 + d32, -1, true};
      break;
    case 9:
      if (state == AnimState::Idle) return {0, d32, false};
      if (state == AnimState::Move) return {1 + d8, -1, false};  // one hop
      if (state == AnimState::Attack) return {9 + d8, -1, false};
      break;
  }
  // Boat/Johnka use scripted transport states, not combat states. Vehicles have
  // no body death/fidget sequence; destruction spawns separate effects.
  return {};
}

constexpr AnimSequence turret_sequence(AnimState state, int angle, bool initial_idle = false) {
  if (state == AnimState::Attack) return {2 + direction32_from_angle(angle), -1, false};
  if (state == AnimState::Idle || state == AnimState::Move)
    return {initial_idle ? 1 : 0, direction32_from_angle(angle), false};
  return {};
}

enum class BoatPhase { Closed, Opening, Open, Closing, Turning };
constexpr AnimSequence boat_sequence(BoatPhase phase, int orientation) {
  const int offset = static_cast<int>(phase);
  return {(orientation & 1) * 5 + offset, -1, phase == BoatPhase::Closed || phase == BoatPhase::Open};
}
constexpr AnimSequence johnka_sequence(int orientation) { return {orientation & 1, -1, true}; }

// Construction is a seek, not an animation clock. -1 means no construction
// body; full HP must switch to the built state/slot instead.
constexpr int construction_frame(int frames, int hp, int max_hp) {
  if (frames <= 0 || hp <= 0 || max_hp <= 0) return -1;
  if (hp >= max_hp) return -1;
  return int((int64_t(frames) + 1) * hp / max_hp) - 1;
}
constexpr int destruction_anim(int radius) { return radius < 97 ? 116 : radius < 145 ? 115 : 114; }
constexpr int damage_fire_anim(int hp, int max_hp) {
  if (hp <= 0 || max_hp <= 0 || hp >= max_hp) return 0;
  const int stage = 3 - int((int64_t(hp) * 4 - 1) / max_hp);
  return stage == 0 ? 0 : 153 + stage;
}

}  // namespace aztec::data
