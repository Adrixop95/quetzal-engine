#include "engine/audio/scene_audio.hpp"
#include "engine/world/object_state.hpp"
#include "engine/world/type_info.hpp"

namespace aztec::engine {
void emit_scene_audio(const World& world, const AnimSpriteProvider& provider,
                      const Archive& units, AudioSystem& audio, float cx, float cy,
                      const std::function<bool(float, float, int)>& visible) {
  audio.event_tick = world.tick;
  const auto previous_trigger = audio.event_trigger;
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
    const auto st = object_sprite_state(o, world.side_color(o.side));
    const auto* type = type_info(o.type);
    const auto trigger = type && type->is_structure() ? AudioEvent::Trigger::Building :
        (o.anim_state == int(data::AnimState::Attack) || o.anim_state == int(data::AnimState::Die)
         ? AudioEvent::Trigger::Combat : AudioEvent::Trigger::Ambient);
    const auto frames = provider.sound_frames(st);
    for (size_t i = 0; i < frames.size(); ++i) {
      audio.event_trigger = i > 0 && st.attack_tick >= 0 && trigger == AudioEvent::Trigger::Ambient
          ? AudioEvent::Trigger::Combat : trigger;
      play(frames[i], uint64_t(o.id) << 16 | i, x, y);
    }
  }
  for (const auto& e : world.effects) {
    audio.event_trigger = AudioEvent::Trigger::Unknown; // Effect ownership alone does not establish its cause.
    const float x = e.x / 256.f, y = e.y / 256.f;
    if (!visible(x, y, e.side)) continue;
    data::AnimSet animation(units.get(0x515, uint32_t(e.anim)));
    const auto frame = animation.at_tick(e.slot, e.age, e.loop);
    play(frame, (uint64_t(1) << 63) | (uint64_t(e.source) << 32) |
        (uint64_t(uint16_t(e.anim)) << 16) | uint16_t(e.slot), x, y);
  }
  audio.end_scene_tick();
  audio.event_trigger = previous_trigger;
}
}  // namespace aztec::engine
