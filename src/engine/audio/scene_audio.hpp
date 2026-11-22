// Simulation-tick audio dispatch. Visibility is supplied by the active view.
#pragma once
#include <functional>
#include "engine/assets/anim_provider.hpp"
#include "engine/audio/audio_system.hpp"
#include "engine/world/world.hpp"
#include "engine/sim/commands.hpp"

namespace aztec::engine {
// V: representative type/style determines the set. Mixed-class ranking remains unresolved.
uint32_t selection_speech_set(int type, int style);
void emit_selection_audio(const World& world, AudioSystem& audio, int style, float cx);
void emit_order_audio(const World& world, const Command& command, AudioSystem& audio, int style,
                      float cx);
bool audio_fog_visible(const World& world, float x, float y);
void emit_scene_audio(const World& world, const AnimSpriteProvider& provider,
                      const Archive& units, AudioSystem& audio, float cx, float cy,
                      const std::function<bool(float, float, int)>& visible);
}
