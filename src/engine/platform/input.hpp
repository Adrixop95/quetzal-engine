// Per-frame input snapshot. Pump once per frame, then read.
#pragma once
#include <SDL.h>

namespace aztec::engine {

struct Input {
  int mx = 0, my = 0;            // mouse in window pixels (renderer output space)
  int dx = 0, dy = 0;            // mouse motion this frame
  bool buttons[6] = {};          // SDL_BUTTON_* index (1=left,2=middle,3=right)
  bool pressed[6] = {};          // went down this frame
  bool released[6] = {};
  float wheel = 0;               // notches this frame
  bool quit = false;
  bool focused = true;
  bool mouse_inside = true;
  bool resized = false;
  bool key_down[SDL_NUM_SCANCODES] = {};
  bool key_hit[SDL_NUM_SCANCODES] = {};  // went down this frame (no auto-repeat)
  bool alt = false, shift = false, ctrl = false;

  void begin_frame();            // clears per-frame edges
  void handle(const SDL_Event& e, float scale_x, float scale_y);
  SDL_Point edge_scroll(int width, int height, int margin) const;
  bool down(SDL_Scancode k) const { return key_down[k]; }
  bool hit(SDL_Scancode k) const { return key_hit[k]; }
};

}  // namespace aztec::engine
