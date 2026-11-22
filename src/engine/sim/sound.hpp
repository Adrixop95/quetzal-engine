// Immutable user-supplied sound/animation metadata, available in headless simulations.
#pragma once
#include "core/audio.hpp"
#include "engine/assets/anim_provider.hpp"

namespace aztec::engine {
struct SoundData {
  SoundBank bank;
  AnimSpriteProvider sprites;
  Archive units;
};
}  // namespace aztec::engine
