// Projectile/effect ids and selection rules. Detailed frame records, hotspots
// and light timelines are read through AnimSet from the user's Units.xbe.
#pragma once
#include "animation_slots.hpp"
#include "render_rules.hpp"

namespace aztec::data {

enum class EffectClass { Unknown, Turret, Arrow, Axe, BallExpl, FireThrow,
                         SplashDamage, Decoration, Rubble };

struct WeaponAnimation {
  EffectClass owner;
  int parameter, anim;
};
inline constexpr WeaponAnimation kWeaponAnimations[] = {
  {EffectClass::Arrow, 0, 111}, {EffectClass::Arrow, 1, 112},
  {EffectClass::Arrow, 2, 112}, {EffectClass::Arrow, 3, 158},
  {EffectClass::Arrow, 4, 111}, {EffectClass::Arrow, 5, 111},
  {EffectClass::Axe, 0, 101}, {EffectClass::Axe, 1, 102},
  {EffectClass::Axe, 2, 109}, {EffectClass::Axe, 3, 109},
  {EffectClass::Axe, 4, 109}, {EffectClass::Axe, 5, 109},
  {EffectClass::BallExpl, 0, 100}, {EffectClass::BallExpl, 1, 100},
  {EffectClass::BallExpl, 2, 100}, {EffectClass::BallExpl, 3, 100},
  {EffectClass::BallExpl, 4, 117}, {EffectClass::BallExpl, 5, 118},
  {EffectClass::BallExpl, 6, 100},
  {EffectClass::FireThrow, 0, 157},
};
// SplashDamage uses its own descriptor's d0, not the projectile's descriptor.
inline constexpr int kSplashAnimations[] = {150,150,150,150,151,150,150,151,151,151,150};
inline constexpr int kShockwaveAnim = 105;
inline constexpr int kBeamMuzzleAnim = 106, kBeamSegmentAnim = 107, kBeamImpactAnim = 110;
inline constexpr int kVehicleDebrisAnim = 113;
inline constexpr int kVehicleExplosionAnim = 152;
inline constexpr int kMarineDeathExplosionAnim = 151;
inline constexpr int kSmallFireAnim = 154, kMediumFireAnim = 155, kLargeFireAnim = 156;
inline constexpr int kRubbleVariants = 20;

constexpr AnimSequence projectile_sequence(EffectClass owner, int angle, bool impact = false) {
  if (owner == EffectClass::Axe && impact) return {32, -1, false};
  if (impact) return {};  // other impacts spawn a separate effect
  if (owner == EffectClass::Arrow || owner == EffectClass::Axe || owner == EffectClass::BallExpl)
    return {direction32_from_angle(angle), -1, true};
  if (owner == EffectClass::FireThrow) return {direction32_from_angle(angle), -1, false};
  return {};
}
constexpr AnimSequence explosion_sequence(int variant) {
  return {variant < 0 ? 0 : variant % 3, -1, false};
}

// Resource inventory of every nonempty anim in 31..199. Unknown means the
// resource is decoded but its runtime owner has not been established.
struct EffectAnimInfo {
  int id;
  EffectClass owner;
  int slots, first_slot, last_slot;
  int min_frames, max_frames, min_ticks, max_ticks;
};

inline constexpr EffectAnimInfo kEffectAnims[] = {
  {50, EffectClass::Turret, 34, 0, 33, 1, 32, 1, 32},
  {51, EffectClass::Turret, 34, 0, 33, 1, 32, 1, 32},
  {52, EffectClass::Turret, 34, 0, 33, 3, 32, 3, 32},
  {53, EffectClass::Turret, 34, 0, 33, 1, 32, 1, 32},
  {54, EffectClass::Turret, 34, 0, 33, 2, 32, 11, 32},
  {55, EffectClass::Turret, 34, 0, 33, 1, 32, 1, 32},
  {100, EffectClass::BallExpl, 33, 0, 32, 3, 11, 3, 11},
  {101, EffectClass::Axe, 32, 0, 31, 3, 3, 3, 3},
  {102, EffectClass::Axe, 32, 0, 31, 3, 3, 3, 3},
  {104, EffectClass::Unknown, 5, 0, 4, 10, 10, 10, 10},
  {105, EffectClass::Decoration, 1, 0, 0, 30, 30, 30, 30},
  {106, EffectClass::Decoration, 4, 0, 3, 1, 1, 1, 1},
  {107, EffectClass::Decoration, 32, 0, 31, 1, 1, 1, 1},
  {109, EffectClass::Axe, 32, 0, 31, 4, 4, 4, 4},
  {110, EffectClass::Decoration, 1, 0, 0, 4, 4, 4, 4},
  {111, EffectClass::Arrow, 32, 0, 31, 1, 1, 1, 1},
  {112, EffectClass::Arrow, 32, 0, 31, 1, 1, 1, 1},
  {113, EffectClass::Decoration, 20, 0, 19, 1, 1, 250, 250},
  {114, EffectClass::Rubble, 20, 0, 19, 1, 1, 250, 250},
  {115, EffectClass::Rubble, 20, 0, 19, 1, 1, 250, 250},
  {116, EffectClass::Rubble, 20, 0, 19, 1, 1, 250, 250},
  {117, EffectClass::BallExpl, 32, 0, 31, 8, 8, 8, 8},
  {118, EffectClass::BallExpl, 32, 0, 31, 3, 3, 3, 3},
  {150, EffectClass::SplashDamage, 3, 0, 2, 10, 11, 10, 11},
  {151, EffectClass::SplashDamage, 3, 0, 2, 10, 11, 10, 11},
  {152, EffectClass::Decoration, 3, 0, 2, 10, 11, 10, 11},
  {153, EffectClass::Unknown, 3, 0, 2, 10, 11, 10, 11},
  {154, EffectClass::Decoration, 1, 0, 0, 10, 10, 10, 10},
  {155, EffectClass::Decoration, 1, 0, 0, 10, 10, 10, 10},
  {156, EffectClass::Decoration, 1, 0, 0, 10, 10, 10, 10},
  {157, EffectClass::FireThrow, 32, 0, 31, 10, 10, 10, 10},
  {158, EffectClass::Arrow, 32, 0, 31, 6, 6, 6, 6},
};

}  // namespace aztec::data
