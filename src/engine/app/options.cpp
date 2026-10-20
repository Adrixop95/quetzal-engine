#include "engine/app/options.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace aztec::engine {

const char* usage_text() {
  return "usage: aztec_engine [options] [MISSION]\n"
         "  MISSION            number (campaign mission_N), map name from game/Maps, or path to .mp (default 1)\n"
         "  --game DIR         game directory (default ./game, or $AZTEC_GAME)\n"
         "  --size WxH         window size (default 1280x720)\n"
         "  --fullscreen       exclusive fullscreen;  --borderless  desktop-size borderless window\n"
         "  --no-vsync         disable vsync;  --software  force software renderer;  --fps N  frame cap\n"
         "  --zoom Z           initial zoom (0.5 .. 4); default automatic\n"
         "  --cam X,Y          initial camera centre in tiles\n"
         "  --wander           demo simulation: units walk randomly (shows 12 Hz tick interpolation)\n"
         "  --no-edge-scroll   disable edge scrolling;  --no-hud  hide the info overlay;  --help-overlay  show key help\n"
         "  --palette N --shade N   terrain palette/shade override\n"
         "  --screenshot FILE.png  render --frames N frames (default 5), save PNG and exit\n"
         "  --ticks N          run N simulation ticks before the first frame\n"
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
    else if (a == "--help-overlay") o.help = true;
    else if (a == "--palette") { if (!next(v)) return false; o.terrain_palette = std::atoi(v); }
    else if (a == "--shade") { if (!next(v)) return false; o.terrain_shade = std::atoi(v); }
    else if (a == "--screenshot") { if (!next(v)) return false; o.screenshot = v; }
    else if (a == "--frames") { if (!next(v)) return false; o.frames = std::atoi(v); }
    else if (a == "--ticks") { if (!next(v)) return false; o.pre_ticks = std::atoi(v); }
    else if (a == "--list") o.list = true;
    else if (!a.empty() && a[0] == '-') { err = "unknown option " + a; return false; }
    else o.mission = a;
  }
  if (!o.screenshot.empty() && o.frames <= 0) o.frames = 5;
  return true;
}

}  // namespace aztec::engine
