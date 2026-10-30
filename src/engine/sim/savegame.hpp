#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "engine/sim/commands.hpp"
#include "engine/sim/modifications.hpp"

namespace aztec::engine {

class GameSim;
class RulesData;
struct World;

inline constexpr uint32_t kSaveFormatVersion = 4;
// Bump whenever simulation semantics or state hashing change incompatibly.
inline constexpr uint32_t kSaveReplayVersion = 4;
inline constexpr size_t kMaxSaveBytes = kMaxLogBytes + 512;
inline constexpr uint64_t kMaxSaveTicks = 12 * 60 * 60 * 24; // 24 simulation hours

struct SaveData {
  std::string mission;          // spec to reload ("mission_3")
  uint64_t tick = 0;
  uint64_t hash = 0;            // state hash at `tick`, verified after replay
  uint64_t script_hash = 0;     // FNV-1a of the mission .scn file used (0 = no script); loading refuses a different script
  uint64_t content_hash = 0;   // mission/rules/input manifest fingerprint
  float cam_x = 0, cam_y = 0, cam_zoom = 1;
  Modifications modifications; // initial set; later changes are commands in log
  std::vector<LoggedCommand> log;
};

// Invalid input returns empty/false. Failed reads leave output unchanged.
std::vector<uint8_t> serialize_save(const SaveData& s);
bool deserialize_save(const std::vector<uint8_t>& bytes, SaveData& out);
bool write_save(const std::string& path, const SaveData& s);
uint64_t file_hash(const std::string& path);   // FNV-1a of a file's bytes, 0 if unreadable / missing
bool read_save(const std::string& path, SaveData& out);

// Ordered, tagged file manifest; an empty path records an absent optional file.
// File bytes are streamed; host directory paths are excluded from the fingerprint.
bool save_content_hash(const std::vector<std::pair<std::string, std::string>>& files, uint64_t& out);
// Replay in a private copy. Failure leaves fresh unchanged; callers commit only success.
std::unique_ptr<GameSim> replay_save(const SaveData& save, World& fresh,
    std::shared_ptr<const RulesData> rules, uint64_t expected_content_hash,
    const std::string& script_path, std::string& error);

}  // namespace aztec::engine
