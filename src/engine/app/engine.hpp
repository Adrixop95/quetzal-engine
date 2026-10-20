#pragma once
#include <memory>
#include "engine/app/options.hpp"
#include "engine/assets/sprite_provider.hpp"
#include "engine/sim/simulation.hpp"

namespace aztec::engine {

// Runs the engine until quit (or until the screenshot frame). Returns the process exit code.
int run_engine(const Options& opts, std::shared_ptr<const SpriteProvider> provider = nullptr,
               std::unique_ptr<Simulation> sim = nullptr);

}  // namespace aztec::engine
