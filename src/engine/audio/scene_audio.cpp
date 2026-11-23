#include "engine/audio/scene_audio.hpp"
#include <algorithm>
#include "engine/world/object_state.hpp"
#include "engine/world/type_info.hpp"

namespace aztec::engine {
uint32_t selection_speech_set(int type, int style) {
  if (style < 0 || style > 3 || type < 0 || type >= kNumTypes) return 0;
  // V: native voice follows representative type, including shared/special units,
  // rather than its current owner's race (f_00454240).
  const int native = type <= 26 || type == 82 || type == 83 || type == 85 ? 1 :
      type <= 52 || type == 87 ? 2 : 3;
  const int voice = style ? style : native;
  // V: selection groups in f_00453d50; their members remain runtime data.
  return speech_list_id(101, voice, 0);
}

namespace {
const Object* speaker_for(const World& world, const std::vector<uint32_t>& ids, int style, bool fallback) {
  const Object* speaker = nullptr;
  for (const auto id : ids) {
    const auto* object = world.find(id);
    if (!object || object->removed || !object->alive || object->side != world.local_side) continue;
    const auto* type = type_info(object->type);
    if (!type || type->is_structure()) continue;
    if (!speaker) speaker = object;
    // V: ranking cannot affect the list when every eligible member has the same voice class.
    // G: the previous first-member approximation is available only by explicit opt-in.
    if (!fallback && selection_speech_set(speaker->type, style) != selection_speech_set(object->type, style)) return nullptr;
  }
  return speaker;
}
void acknowledge(const World& world, const Object& speaker, uint32_t set,
                 AudioSystem& audio, float cx, AudioEvent::Trigger trigger) {
  const auto pan = positional_gain(speaker.cur_x / 256.f - cx, 0);
  const auto previous = audio.event_trigger;
  audio.event_tick = world.tick;
  audio.event_trigger = trigger;
  audio.queue_speech_set(set, true, pan.left, pan.right);
  audio.event_trigger = previous;
}
}

void emit_selection_audio(const World& world, AudioSystem& audio, int style, float cx) {
  if (!audio.acknowledgement) return;
  if (const auto* speaker = speaker_for(world, world.selection.ids, style, audio.event_fallbacks))
    acknowledge(world, *speaker, selection_speech_set(speaker->type, style), audio, cx, AudioEvent::Trigger::Selection);
}

void emit_order_audio(const World& world, const Command& command, AudioSystem& audio, int style, float cx) {
  if (command.side != world.local_side || !audio.acknowledgement) return;
  const auto* speaker = speaker_for(world, command.ids, style, audio.event_fallbacks);
  if (!speaker) return;
  const uint32_t base = selection_speech_set(speaker->type, style);
  if (!base) return;
  int offset = -1;
  if (command.type == Pct::Attack) {
    const auto* target = world.find(uint32_t(command.a));
    if (target && target->alive && !target->removed && world.hostile(command.side, target->side)) offset = 2;
  } else if (command.type == Pct::FollowTeam && command.b == 0) offset = 3;
  else if (command.type == Pct::Stop && speaker->follow_tactic >= 0) offset = 6;
  if (offset < 0 && audio.event_fallbacks &&
      (command.type == Pct::Move || command.type == Pct::Attack ||
       command.type == Pct::AttackMove || command.type == Pct::Stop)) offset = 3;
  if (offset >= 0) acknowledge(world, *speaker, base + offset,
                               audio, cx, AudioEvent::Trigger::Orders);
}

bool audio_fog_visible(const World& world, float x, float y) {
  if (world.local_side < 0 || size_t(world.local_side) >= world.visibility.sides.size()) return false;
  const auto& layer = world.visibility.sides[size_t(world.local_side)];
  const int tx = int(x / 48), ty = int(y / 48);
  if (x < 0 || y < 0 || tx >= layer.w || ty >= layer.h) return false;
  const size_t index = size_t(ty) * layer.w + tx;
  if (index >= layer.v.size()) return false;
  const int brightness = world.visibility_brightness ? layer.v[index] : 152 - std::min(152, int(layer.v[index]));
  return brightness >= 33;
}

void emit_scene_audio(const World& world, const AnimSpriteProvider& provider,
                      const Archive& units, AudioSystem& audio, float cx, float cy,
                      const std::function<bool(float, float, int)>& visible) {
  audio.event_tick = world.tick;
  const auto previous_trigger = audio.event_trigger;
  audio.begin_scene_tick();
  for (const auto& cue : world.combat_sounds) {
    const float x = cue.x / 256.f, y = cue.y / 256.f;
    if (!cue.sound) continue;
    const bool audible = visible(x, y, cue.side);
    audio.event_trigger = AudioEvent::Trigger::Combat;
    if (cue.loop) audio.loop_at(cue.source, cue.set, x, y, cx, cy, cue.sound, 65536, audible);
    else if (audible) audio.play_set_at(cue.set, x, y, cx, cy, cue.sound, 65536, cue.volume_offset);
  }
  auto play = [&](const data::SpriteRef& frame, uint64_t key, float x, float y, bool audible) {
    if (!frame.valid()) return;
    auto dispatch = [&](uint32_t set, bool loop) {
      uint32_t chosen = 0, rate = 65536;
      if (world.simulation_sounds) {
        const auto choice = std::find_if(world.sound_choices.begin(), world.sound_choices.end(),
            [&](const auto& c) { return c.source == key && c.set == set && c.loop == loop; });
        if (choice == world.sound_choices.end() || choice->sound == 0) return;
        chosen = choice->sound;
        rate = random_pitch_rate(choice->pitch);
      }
      if (loop) audio.loop_at(key, set, x, y, cx, cy, chosen, rate, audible);
      else audio.play_set_at(set, x, y, cx, cy, chosen, rate);
    };
    if (audible && frame.sound_entry && frame.sound > 0) dispatch(uint32_t(frame.sound), false);
    if (frame.loop_sound > 0) dispatch(uint32_t(frame.loop_sound), true);
  };
  for (const auto& o : world.objects) {
    if (o.removed) continue;
    const float x = o.cur_x / 256.f, y = o.cur_y / 256.f;
    const bool audible = visible(x, y, o.side);
    if (!audible && audio.event_fallbacks) continue;
    const auto st = object_sprite_state(o, world.side_color(o.side));
    const auto* type = type_info(o.type);
    const auto trigger = type && type->is_structure() ? AudioEvent::Trigger::Building :
        (o.anim_state == int(data::AnimState::Attack) || o.anim_state == int(data::AnimState::Die)
         ? AudioEvent::Trigger::Combat : AudioEvent::Trigger::Ambient);
    // Match the renderer: simulation-owned fire replaces the fallback decoration.
    const bool simulated_fire = o.alive && std::any_of(world.effects.begin(), world.effects.end(),
        [&](const Effect& e) { return e.loop && e.source == o.id; });
    const auto frames = provider.sound_frames(st, !simulated_fire);
    for (size_t i = 0; i < frames.size(); ++i) {
      audio.event_trigger = i > 0 && st.attack_tick >= 0 && trigger == AudioEvent::Trigger::Ambient
          ? AudioEvent::Trigger::Combat : trigger;
      play(frames[i], uint64_t(o.id) << 16 | i, x, y, audible);
    }
  }
  for (const auto& e : world.effects) {
    audio.event_trigger = AudioEvent::Trigger::Unknown; // Effect ownership alone does not establish its cause.
    const float x = e.x / 256.f, y = e.y / 256.f;
    const bool audible = visible(x, y, e.side);
    if (!audible && audio.event_fallbacks) continue;
    data::AnimSet animation(units.get(0x515, uint32_t(e.anim)));
    const auto frame = animation.at_tick(e.slot, e.age, e.loop);
    play(frame, (uint64_t(1) << 63) | (uint64_t(e.source) << 32) |
        (uint64_t(uint16_t(e.anim)) << 16) | uint16_t(e.slot), x, y, audible);
  }
  audio.end_scene_tick();
  audio.event_trigger = previous_trigger;
}
}  // namespace aztec::engine
