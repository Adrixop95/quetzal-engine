// Engine save files. A save is the mission, the tick reached and the full command log: loading replays the game
// deterministically on a fresh world (docs/engine_rules_status.md). The original .sav layout (docs/savegame.md of the
// formats agent) is not used yet.
#pragma once
#include <string>
#include <vector>
#include "engine/sim/commands.hpp"

namespace aztec::engine {

struct SaveData {
  std::string mission;          // spec to reload ("mission_3")
  uint64_t tick = 0;
  uint64_t hash = 0;            // state hash at `tick`, verified after replay
  float cam_x = 0, cam_y = 0, cam_zoom = 1;
  std::vector<LoggedCommand> log;
};

std::vector<uint8_t> serialize_save(const SaveData& s);
bool deserialize_save(const std::vector<uint8_t>& bytes, SaveData& out);
bool write_save(const std::string& path, const SaveData& s);
bool read_save(const std::string& path, SaveData& out);

}  // namespace aztec::engine
