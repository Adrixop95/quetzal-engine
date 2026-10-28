// SpriteProvider backed by the animation tables of the sprite mapping (data/sprites.hpp):
// type + state + direction + tick -> Units.xbe sprite (body in group 0x44c, shadow in 0x454).
#pragma once
#include <string>
#include "engine/assets/sprite_provider.hpp"
#include "engine/data/sprites.hpp"
#include "engine/data/render_rules.hpp"

namespace aztec::engine {

class AnimSpriteProvider : public SpriteProvider {
 public:
  // `units_xbe` = path of Res/Units.xbe.
  bool open(const std::string& units_xbe) { return db_.open(units_xbe); }
  bool pick(const ObjectState& st, SpriteDraw& out) const override { return make(st, out, false); }
  bool pick_shadow(const ObjectState& st, SpriteDraw& out) const override { return make(st, out, true); }
  // Raw frame lookup (also used by tests/tools).
  data::SpriteRef frame_for(const ObjectState& st) const;
  std::vector<data::SpriteRef> sound_frames(const ObjectState& st) const;
  std::vector<SpriteDraw> parts(const ObjectState& st) const override;
  std::vector<SpriteItem> items(const ObjectState& st) const override;
  int light_mask(const ObjectState& st) const override { return frame_for(st).light; }
  int vision_mask(const ObjectState& st) const override;
  int height(const ObjectState& st) const override;
  bool hides_missing(const ObjectState& st) const override;
  bool load_visual_rules(const std::string& directory);
  // Missing, malformed or holding phases cannot establish completion.
  bool boat_phase_complete(int phase, int orientation, uint32_t tick) const;

 private:
  bool make(const ObjectState& st, SpriteDraw& out, bool shadow) const;
  data::SpriteDb db_;
  int vision_[90] = {};
};

}  // namespace aztec::engine
