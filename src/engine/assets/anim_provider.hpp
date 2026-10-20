// SpriteProvider backed by the animation tables of the sprite mapping (data/sprites.hpp):
// type + state + direction + tick -> Units.xbe sprite (body in group 0x44c, shadow in 0x454).
#pragma once
#include <string>
#include "engine/assets/sprite_provider.hpp"
#include "engine/data/sprites.hpp"

namespace aztec::engine {

class AnimSpriteProvider : public SpriteProvider {
 public:
  // `units_xbe` = path of Res/Units.xbe.
  bool open(const std::string& units_xbe) { return db_.open(units_xbe); }
  bool pick(const ObjectState& st, SpriteDraw& out) const override { return make(st, out, false); }
  bool pick_shadow(const ObjectState& st, SpriteDraw& out) const override { return make(st, out, true); }
  // Raw frame lookup (also used by tests/tools).
  data::SpriteRef frame_for(const ObjectState& st) const;

 private:
  bool make(const ObjectState& st, SpriteDraw& out, bool shadow) const;
  data::SpriteDb db_;
};

}  // namespace aztec::engine
