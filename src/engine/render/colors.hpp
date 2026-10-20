#pragma once
#include <SDL2/SDL.h>

namespace aztec::engine {

// Team colour for [SideN] Color = 0..7. The real in-game mapping is not known yet; these are placeholders.
inline SDL_Color side_color_rgb(int c, Uint8 a = 255) {
  static const SDL_Color t[8] = {{70, 120, 240, 255}, {230, 55, 55, 255},  {70, 200, 80, 255},   {240, 220, 60, 255},
                                 {70, 220, 220, 255}, {210, 80, 210, 255}, {245, 150, 50, 255},  {225, 225, 225, 255}};
  SDL_Color r = t[(c < 0 ? 0 : c) & 7];
  r.a = a;
  return r;
}

}  // namespace aztec::engine
