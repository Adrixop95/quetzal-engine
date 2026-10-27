// Adapter from the original save stream to independent engine state.
#pragma once
#include "core/savegame.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

// Applies a decoded save to a world with the matching mission/map already loaded.
// Failure leaves world unchanged. Unknown state remains in World::original_save;
// see docs/savegame.md for the supported subset and resume limitations.
bool import_original_save(const SaveGame& save, World& world, std::string& error);
// Resolves only the saved mission basename inside GameData::maps_dir().
bool load_original_save(const GameData& data, const std::string& path,
                        World& world, std::string& error);

} // namespace aztec::engine
