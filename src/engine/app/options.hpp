#pragma once
#include <string>
#include "engine/platform/window.hpp"

namespace aztec::engine {

// Installed packages keep rules beside the executable, independent of the working directory.
std::string default_rules_dir();

struct Options {
  std::string game_dir = "game";
  std::string original_save;        // --load-original FILE
  std::string mission = "1";
  bool mission_given = false;      // a mission was named on the command line
  bool menu_session = false;       // the front-end is in use: Esc opens the pause menu instead of quitting
  bool no_menu = false;
  bool menu = false;               // start in the front-end (set when no mission/script/screenshot was requested)       // number, name or path to .mp
  WindowOptions window;
  float zoom = 0;                  // 0 = automatic
  bool has_cam = false;
  float cam_tx = 0, cam_ty = 0;    // initial camera centre in tiles
  bool wander = false;             // demo simulation: units random-walk (tests interpolation)
  bool edge_scroll = true;
  bool mouse_grab = true;
  bool hud = true;
  bool fog = true;
  std::string audio_log;           // --audio-log FILE: successful playback events
  bool audio = true;               // --no-audio
  float sfx_volume = 0.8f, music_volume = 0.6f, speech_volume = 0.8f;
  bool acknowledgement = true, subtitles = true;
  std::string ui_trace;            // --ui-trace FILE: raw per-frame UI observations
  std::string script;              // --script FILE: headless playtest (see app/playtest.hpp)
  int cmd_x = -1, cmd_y = -1; bool cmd_attack_move = false;  // --cmd-move / --cmd-attack-move X,Y (tiles) for the --select group
  std::string rules_dir;           // --rules-dir DIR; otherwise packaged rules or source-tree docs/data
  bool sim = true;                 // --no-sim: static world (no game logic)
  int select_x0 = -1, select_y0 = 0, select_x1 = 0, select_y1 = 0;  // --select X0,Y0,X1,Y1 (screenshot mode)
  bool light = true;               // time-of-day tint (needs Aztec.exe next to Res/)
  double time = -1;                // --time T: fixed day clock 0..7199
  int cursor_x = -1, cursor_y = -1; // --cursor X,Y: draw the cursor there in screenshot mode
  bool panel = true;               // original right-hand panel
  bool sprites = true;             // --no-sprites: placeholder markers only
  bool help = false;
  int terrain_palette = -1, terrain_shade = 14;
  std::string screenshot;          // write PNG after `frames` frames, then exit
  int frames = 0;                  // 0 = run until quit (screenshot default: 5)
  int pre_ticks = 0;               // simulation ticks to run before the first frame
  int fps_cap = 0;
  std::string sheet_spec, sheet_out;  // --sprite-sheet SPEC OUT.png
  bool list = false;               // --list: print campaign/custom mission names and exit
};

// Returns false (with message in `err`) on bad arguments; sets `show_usage` for --help.
bool parse_options(int argc, char** argv, Options& o, std::string& err, bool& show_usage);
const char* usage_text();

}  // namespace aztec::engine
