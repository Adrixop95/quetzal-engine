// Shared geometry from the 800x600 interface observation in docs/formats.md.
#pragma once
#include <algorithm>
#include <SDL.h>

namespace aztec::engine {
struct InGamePanelLayout {
  SDL_Rect menu{}, minimap{}, card{}, portrait{}, summary{};
};
inline InGamePanelLayout in_game_panel_layout(const SDL_Rect& side, int scale) {
  const int sc = std::max(1, scale);
  auto box = [&](int x, int y, int w, int h) {
    return SDL_Rect{side.x + x * sc, side.y + y * sc, w * sc, h * sc};
  };
  InGamePanelLayout l;
  l.menu = box(2, 0, 60, 22);
  l.minimap = box(12, 28, 180, 174);
  l.card = {side.x - 50 * sc, side.y + side.h - 208 * sc, 254 * sc, 208 * sc};
  l.portrait = box(126, 218, 58, 56);
  l.summary = box(14, 218, 108, 56);
  return l;
}
}  // namespace aztec::engine
