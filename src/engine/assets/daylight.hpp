// Time-of-day lighting. The original tints the whole scene with per-time keys (RGB multipliers + an
// ambient palette shade level). The key tables live in the original executable, so they are read at
// run time from the user's own game/Aztec.exe (nothing derived from it is stored in this repository).
// Without the exe the light is neutral.
#pragma once
#include <string>
#include <vector>

namespace aztec::engine {

struct DayKey {
  int time = 0;                 // key time in ticks (day = 7200 ticks = 10 min at 12 Hz)
  double rgb[3] = {1, 1, 1};    // colour multipliers
  int level = 12;               // ambient palette shade level 6..12 (darker .. lighter)
  int coeff = 1;                // unused by the renderer
};

struct Light {
  float rgb[3] = {1, 1, 1};     // tint at the shade level used for drawing
  float level = 14;             // ambient palette shade (0..19), 14 = natural colours
};

class Daylight {
 public:
  // Parses the two key tables (TileSet 0 / 1) from a PE file. Returns false if not found.
  bool load_from_exe(const std::string& exe_path);
  bool loaded() const { return !keys_[0].empty(); }
  // Interpolated key at `time` (0..7199) for a tileset.
  Light sample(int tileset, double time) const;
  const std::vector<DayKey>& keys(int tileset) const { return keys_[tileset ? 1 : 0]; }
  static constexpr int kDayTicks = 7200;

 private:
  std::vector<DayKey> keys_[2];
};

}  // namespace aztec::engine
