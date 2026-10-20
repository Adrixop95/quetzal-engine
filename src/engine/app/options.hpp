#pragma once
#include <string>
#include "engine/platform/window.hpp"

namespace aztec::engine {

struct Options {
  std::string game_dir = "game";
  std::string mission = "1";       // number, name or path to .mp
  WindowOptions window;
  float zoom = 0;                  // 0 = automatic
  bool has_cam = false;
  float cam_tx = 0, cam_ty = 0;    // initial camera centre in tiles
  bool wander = false;             // demo simulation: units random-walk (tests interpolation)
  bool edge_scroll = true;
  bool hud = true;
  bool help = false;
  int terrain_palette = -1, terrain_shade = 14;
  std::string screenshot;          // write PNG after `frames` frames, then exit
  int frames = 0;                  // 0 = run until quit (screenshot default: 5)
  int pre_ticks = 0;               // simulation ticks to run before the first frame
  int fps_cap = 0;
  bool list = false;               // --list: print campaign/custom mission names and exit
};

// Returns false (with message in `err`) on bad arguments; sets `show_usage` for --help.
bool parse_options(int argc, char** argv, Options& o, std::string& err, bool& show_usage);
const char* usage_text();

}  // namespace aztec::engine
