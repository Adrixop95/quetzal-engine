#include "engine/audio/scene_audio.hpp"

namespace aztec::engine {
void emit_scene_audio(const World& world, const AnimSpriteProvider& provider,
                      const Archive& units, AudioSystem& audio, float cx, float cy,
                      const std::function<bool(float, float, int)>& visible) {
  audio.event_tick = world.tick;
  audio.begin_scene_tick();
  auto play = [&](const data::SpriteRef& frame, uint64_t key, float x, float y) {
    if (!frame.valid()) return;
    if (frame.sound_entry && frame.sound > 0) audio.play_set_at(uint32_t(frame.sound), x, y, cx, cy);
    if (frame.loop_sound > 0) audio.loop_at(key, uint32_t(frame.loop_sound), x, y, cx, cy);
  };
  for (const auto& o : world.objects) {
    if (o.removed) continue;
    const float x = o.cur_x / 256.f, y = o.cur_y / 256.f;
    if (!visible(x, y, o.side)) continue;
    ObjectState st;
    st.type_id = o.type;
    if (o.type < 0 && o.source == ObjectSource::Boat) st.type_id = -2;
    if (o.type < 0 && o.source == ObjectSource::Johnka) st.type_id = -3;
    st.hp = o.hp; st.max_hp = o.hp_max; st.progress = o.progress;
    st.facing = o.facing; st.anim_state = o.anim_state; st.anim_tick = o.anim_tick;
    st.anim_variant = o.anim_variant; st.body_tick = o.body_tick; st.attack_tick = o.attack_tick;
    st.moving = o.body_moving; st.initial_turret = !o.has_fired;
    st.destruction_radius = o.destruction_radius;
    st.transport_phase = o.transport_phase; st.orientation = o.orientation;
    const auto frames = provider.sound_frames(st);
    for (size_t i = 0; i < frames.size(); ++i) play(frames[i], uint64_t(o.id) << 16 | i, x, y);
  }
  for (const auto& e : world.effects) {
    const float x = e.x / 256.f, y = e.y / 256.f;
    if (!visible(x, y, e.side)) continue;
    data::AnimSet animation(units.get(0x515, uint32_t(e.anim)));
    const auto frame = animation.at_tick(e.slot, e.age, e.loop);
    play(frame, (uint64_t(1) << 63) | (uint64_t(e.source) << 32) |
        (uint64_t(uint16_t(e.anim)) << 16) | uint16_t(e.slot), x, y);
  }
  audio.end_scene_tick();
}
}  // namespace aztec::engine
