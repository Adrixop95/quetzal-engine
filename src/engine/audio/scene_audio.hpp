// Simulation-tick audio dispatch. Visibility is supplied by the active view.
#pragma once
#include <functional>
#include "engine/assets/anim_provider.hpp"
#include "engine/audio/audio_system.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {
void emit_scene_audio(const World& world, const AnimSpriteProvider& provider,
                      const Archive& units, AudioSystem& audio, float cx, float cy,
                      const std::function<bool(float, float, int)>& visible);
}
