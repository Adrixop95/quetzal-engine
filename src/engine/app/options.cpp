#include "engine/app/options.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace aztec::engine {

const char* usage_text() {
  return "usage: aztec_engine [options] [MISSION]\n"
         "  aztec_engine --headless --help   bounded simulation-only campaign mode\n"
         "  MISSION            number (campaign mission_N), map name from game/Maps, or path to .mp (default 1)\n"
         "  --game DIR         game directory (default ./game, or $AZTEC_GAME)\n"
         "  --load-original FILE  import an original .sav (partial resume; see docs/savegame.md)\n"
         "  --size WxH         window size (default 1280x720)\n"
         "  --fullscreen       exclusive fullscreen;  --borderless  desktop-size borderless window\n"
         "  --no-vsync         disable vsync;  --software  force software renderer;  --fps N  frame cap\n"
         "  --zoom Z           initial zoom (0.5 .. 4); default automatic\n"
         "  --cam X,Y          initial camera centre in tiles\n"
         "  --wander           demo simulation: units walk randomly (shows 12 Hz tick interpolation)\n"
         "  --no-sprites       draw placeholder markers instead of sprites\n"
         "  --no-fog           disable fog of war from .vis (F8 toggles)\n"
         "  --no-panel         no original right-hand panel\n"
         "  --no-light         no time-of-day tint;  --time T  fixed day clock (0..7199)\n"
         "  --rules-dir DIR    docs/data CSVs for the game rules;  --no-sim  no game logic\n"
         "  --script FILE      playtest script: synthetic clicks/keys, screenshots, expectations (virtual time)\n"
         "  --no-menu          start the default mission directly instead of the front-end menu\n"
         "  --no-audio         silent;  --volume V  effects and music volume 0..1\n"
         "  --no-edge-scroll   disable edge scrolling;  --no-hud  hide the info overlay;  --help-overlay  show key help\n"
         "  --palette N --shade N   terrain palette/shade override\n"
         "  --screenshot FILE.png  render --frames N frames (default 5), save PNG and exit\n"
         "  --ticks N          run N simulation ticks before the first frame\n"
         "  --sprite-sheet ARCH:FIRST-LAST OUT.png   dump sprites (units|tiles0|tiles1|common) into a contact sheet\n"
         "  --list             list available missions\n";
}

bool parse_options(int argc, char** argv, Options& o, std::string& err, bool& usage) {
  usage = false;
  if (const char* g = std::getenv("AZTEC_GAME")) o.game_dir = g;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto next = [&](const char*& v) {
      if (i + 1 >= argc) { err = "missing value for " + a; return false; }
      v = argv[++i];
      return true;
    };
    const char* v = nullptr;
    if (a == "--help" || a == "-h") { usage = true; return true; }
    else if (a == "--game") { if (!next(v)) return false; o.game_dir = v; }
    else if (a == "--load-original") { if (!next(v)) return false; o.original_save = v; }
    else if (a == "--size") {
      if (!next(v)) return false;
      if (std::sscanf(v, "%dx%d", &o.window.width, &o.window.height) != 2 || o.window.width < 160 || o.window.height < 120) { err = "bad --size"; return false; }
    }
    else if (a == "--fullscreen") o.window.mode = DisplayMode::Fullscreen;
    else if (a == "--borderless") o.window.mode = DisplayMode::Borderless;
    else if (a == "--no-vsync") o.window.vsync = false;
    else if (a == "--software") o.window.software = true;
    else if (a == "--fps") { if (!next(v)) return false; o.fps_cap = std::atoi(v); }
    else if (a == "--zoom") { if (!next(v)) return false; o.zoom = float(std::atof(v)); }
    else if (a == "--cam") {
      if (!next(v)) return false;
      if (std::sscanf(v, "%f,%f", &o.cam_tx, &o.cam_ty) != 2) { err = "bad --cam"; return false; }
      o.has_cam = true;
    }
    else if (a == "--wander") o.wander = true;
    else if (a == "--no-edge-scroll") o.edge_scroll = false;
    else if (a == "--no-hud") o.hud = false;
    else if (a == "--no-fog") o.fog = false;
    else if (a == "--rules-dir") { if (!next(v)) return false; o.rules_dir = v; }
    else if (a == "--no-sim") o.sim = false;
    else if (a == "--no-audio") o.audio = false;
    else if (a == "--volume") { if (!next(v)) return false; o.sfx_volume = o.music_volume = float(std::atof(v)); }
    else if (a == "--no-menu") o.no_menu = true;
    else if (a == "--script") { if (!next(v)) return false; o.script = v; }
    else if (a == "--cmd-move" || a == "--cmd-attack-move") { if (!next(v)) return false; if (std::sscanf(v, "%d,%d", &o.cmd_x, &o.cmd_y) != 2) { err = "bad " + a; return false; } o.cmd_attack_move = a != "--cmd-move"; }
    else if (a == "--select") { if (!next(v)) return false; if (std::sscanf(v, "%d,%d,%d,%d", &o.select_x0, &o.select_y0, &o.select_x1, &o.select_y1) != 4) { err = "bad --select"; return false; } }
    else if (a == "--no-light") o.light = false;
    else if (a == "--time") { if (!next(v)) return false; o.time = std::atof(v); }
    else if (a == "--cursor") { if (!next(v)) return false; if (std::sscanf(v, "%d,%d", &o.cursor_x, &o.cursor_y) != 2) { err = "bad --cursor"; return false; } }
    else if (a == "--no-panel") o.panel = false;
    else if (a == "--no-sprites") o.sprites = false;
    else if (a == "--help-overlay") o.help = true;
    else if (a == "--palette") { if (!next(v)) return false; o.terrain_palette = std::atoi(v); }
    else if (a == "--shade") { if (!next(v)) return false; o.terrain_shade = std::atoi(v); }
    else if (a == "--screenshot") { if (!next(v)) return false; o.screenshot = v; }
    else if (a == "--frames") { if (!next(v)) return false; o.frames = std::atoi(v); }
    else if (a == "--ticks") { if (!next(v)) return false; o.pre_ticks = std::atoi(v); }
    else if (a == "--list") o.list = true;
    else if (a == "--sprite-sheet") { if (!next(v)) return false; o.sheet_spec = v; if (!next(v)) return false; o.sheet_out = v; }
    else if (!a.empty() && a[0] == '-') { err = "unknown option " + a; return false; }
    else { o.mission = a; o.mission_given = true; }
  }
  if (!o.original_save.empty() && (o.mission_given || o.wander)) {
    err = "--load-original cannot be combined with a mission or --wander"; return false;
  }
  if (!o.screenshot.empty() && o.frames <= 0) o.frames = 5;
  o.menu = o.original_save.empty() && !o.mission_given && o.screenshot.empty() && o.script.empty() && !o.no_menu;
  return true;
}

}  // namespace aztec::engine
