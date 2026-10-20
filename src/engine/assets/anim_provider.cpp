#include "engine/assets/anim_provider.hpp"

namespace aztec::engine {

data::SpriteRef AnimSpriteProvider::frame_for(const ObjectState& st) const {
  int s = st.anim_state;
  if (s < 0 || s > int(data::AnimState::Icon)) s = 0;
  auto state = static_cast<data::AnimState>(s);
  int dir = st.facing & 7;
  if (st.type_id >= 0 && st.type_id < 90 && (data::kTypeAnims[st.type_id].kind == 2 || data::kTypeAnims[st.type_id].kind == 7)) {
    // Structures: slot 0 holds the construction stages, slot 1 the finished building (guess confirmed against
    // a screenshot of the original; damage stages are not mapped yet).
    data::AnimSet a = db_.anim(db_.anim_id(st.type_id));
    data::SpriteRef r = a.frame(1, 0);
    if (!r.valid()) r = a.frame(0, a.frame_count(0) - 1);
    return r;
  }
  data::SpriteRef r = db_.sprite_for(st.type_id, state, dir, int(st.anim_tick), int(st.anim_variant));
  if (!r.valid() && state != data::AnimState::Idle) r = db_.sprite_for(st.type_id, data::AnimState::Idle, dir, 0);
  return r;
}

bool AnimSpriteProvider::make(const ObjectState& st, SpriteDraw& out, bool shadow) const {
  if (st.type_id < 0 || st.type_id >= 90 || db_.anim_id(st.type_id) == 0) return false;
  data::SpriteRef r = frame_for(st);
  if (!r.valid()) return false;
  out = SpriteDraw{};
  out.key.archive = ArchiveId::Units;
  out.key.group = shadow ? 0x454 : 0x44c;
  out.key.index = uint32_t(r.sprite);
  out.key.team = uint8_t(st.side_color & 7);
  out.key.shadow = shadow;
  out.dy = r.yoff;
  return true;
}

}  // namespace aztec::engine
