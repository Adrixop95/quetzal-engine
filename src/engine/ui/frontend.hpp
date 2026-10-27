// Front-end screens (main menu, campaign, custom maps, briefing, options) and the in-game pause menu,
// drawn with original backgrounds from Res/common.xbe and text from the original fonts.
#pragma once
#include <SDL.h>
#include <string>
#include <vector>
#include "engine/app/options.hpp"
#include "engine/assets/game_data.hpp"
#include "engine/platform/input.hpp"
#include "engine/platform/window.hpp"
#include "engine/render/font.hpp"

namespace aztec::engine {

struct FrontendResult {
  std::string mission;   // spec to play ("mission_3", "Pentagram")
  bool quit = false;
};

// Runs the menus until a mission is chosen or the user quits. `opts` may be changed by the options screen.
// `open_briefing` (e.g. "mission_4") jumps straight to that mission's briefing.
bool run_frontend(Window& win, GameData& data, Options& opts, FrontendResult& out, const std::string& open_briefing = "");

// Plays a video through an external `ffmpeg` process (raw RGB pipe), as the runtime does. Returns false when
// ffmpeg or the file is missing. Any key / click skips.
bool play_video(Window& win, const std::string& path);

// In-game pause overlay.
class PauseMenu {
 public:
  enum class Action { None, Resume, Save, Load, Restart, Menu, Quit };
  bool open = false;
  Action update(const Input& in, int ow, int oh, int scale);   // handles clicks while open
  void draw(SDL_Renderer* r, const Font& font, int ow, int oh, int scale) const;

 private:
  SDL_Rect button(int i, int ow, int oh, int scale) const;
};

// Greedy word wrap (UTF-8, '\n' kept as line breaks).
std::vector<std::string> wrap_text(const Font& font, const std::string& text, int max_width, int scale);

}  // namespace aztec::engine
