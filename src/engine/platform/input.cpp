#include "engine/platform/input.hpp"

#include <cstring>

namespace aztec::engine {

void Input::begin_frame() {
  std::memset(pressed, 0, sizeof pressed);
  std::memset(released, 0, sizeof released);
  std::memset(key_hit, 0, sizeof key_hit);
  dx = dy = 0;
  wheel = 0;
  resized = false;
}

void Input::handle(const SDL_Event& e, float sx, float sy) {
  switch (e.type) {
    case SDL_QUIT: quit = true; break;
    case SDL_MOUSEMOTION:
      mx = int(e.motion.x * sx);
      my = int(e.motion.y * sy);
      dx += int(e.motion.xrel * sx);
      dy += int(e.motion.yrel * sy);
      break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
      int b = e.button.button;
      if (b >= 6) break;
      bool d = e.type == SDL_MOUSEBUTTONDOWN;
      if (d && !buttons[b]) pressed[b] = true;
      if (!d && buttons[b]) released[b] = true;
      buttons[b] = d;
      mx = int(e.button.x * sx);
      my = int(e.button.y * sy);
      break;
    }
    case SDL_MOUSEWHEEL: wheel += e.wheel.preciseY; break;
    case SDL_KEYDOWN:
      if (!e.key.repeat) key_hit[e.key.keysym.scancode] = true;
      key_down[e.key.keysym.scancode] = true;
      break;
    case SDL_KEYUP: key_down[e.key.keysym.scancode] = false; break;
    case SDL_WINDOWEVENT:
      switch (e.window.event) {
        case SDL_WINDOWEVENT_FOCUS_GAINED: focused = true; break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
        case SDL_WINDOWEVENT_MINIMIZED:
          focused = false;
          std::memset(key_down, 0, sizeof key_down);
          std::memset(key_hit, 0, sizeof key_hit);
          std::memset(buttons, 0, sizeof buttons);
          std::memset(pressed, 0, sizeof pressed);
          std::memset(released, 0, sizeof released);
          dx = dy = 0;
          wheel = 0;
          break;
        case SDL_WINDOWEVENT_ENTER: mouse_inside = true; break;
        case SDL_WINDOWEVENT_LEAVE: mouse_inside = false; break;
        case SDL_WINDOWEVENT_SIZE_CHANGED:
        case SDL_WINDOWEVENT_RESIZED: resized = true; break;
      }
      break;
  }
  const Uint8* ks = SDL_GetKeyboardState(nullptr);
  alt = ks[SDL_SCANCODE_LALT] || ks[SDL_SCANCODE_RALT];
  shift = ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT];
  ctrl = ks[SDL_SCANCODE_LCTRL] || ks[SDL_SCANCODE_RCTRL];
}

SDL_Point Input::edge_scroll(int width, int height, int margin) const {
  if (!focused || !mouse_inside || alt || mx < 0 || my < 0 || mx >= width || my >= height) return {0, 0};
  return {mx < margin ? -1 : mx >= width - margin ? 1 : 0,
          my < margin ? -1 : my >= height - margin ? 1 : 0};
}

}  // namespace aztec::engine
