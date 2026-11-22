// Shared geometry. Original-mode evidence: docs/interface_geometry.md.
#pragma once
#include <algorithm>
#include <array>
#include <SDL.h>

namespace aztec::engine {
enum class InterfaceLayout { Modern, Original };
struct InGamePanelLayout {
  SDL_Rect menu{}, minimap{}, card{}, portrait{}, summary{};
};
inline InGamePanelLayout in_game_panel_layout(const SDL_Rect& side, int scale,
                                            InterfaceLayout mode = InterfaceLayout::Modern) {
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
  if (mode == InterfaceLayout::Original) {
    l.menu = box(0, 0, 60, 22);
    l.minimap = box(12, 25, 180, 180);
    // G: keep the verified 800x600 lower block bottom-anchored at other sizes.
    l.portrait = {}; l.summary = {};
  }
  return l;
}

// V: panel-local coordinates at 800x600. Integer scaling is an engine policy (G).
struct OriginalPanelControls {
  std::array<SDL_Rect, 5> squad{}, town{};
  std::array<SDL_Rect, 12> members{};
  std::array<SDL_Rect, 9> build{};
  std::array<SDL_Rect, 3> queue{};
  std::array<SDL_Rect, 4> towns{}, teams{};
};
inline OriginalPanelControls original_panel_controls(const SDL_Rect& side, int scale) {
  int sc = std::max(1, scale);
  const auto card = in_game_panel_layout(side, sc, InterfaceLayout::Original).card;
  auto box = [&](int x, int y, int w, int h) { return SDL_Rect{card.x + x * sc, card.y + y * sc, w * sc, h * sc}; };
  OriginalPanelControls c;
  c.squad = {box(14,14,59,44), box(78,13,48,44), box(126,13,38,44), box(164,13,38,44), box(202,13,38,44)};
  c.town = {box(12,14,59,44), box(80,14,40,44), box(120,14,36,44), box(156,14,34,44), box(200,15,42,42)};
  for (int i = 0; i < 12; ++i) c.members[i] = box(26 + i % 4 * 47, 71 + i / 4 * 42, 47, 42);
  for (int i = 0; i < 9; ++i) c.build[i] = box(24 + i % 3 * 42, 71 + i / 3 * 42, 42, 42);
  for (int i = 0; i < 3; ++i) c.queue[i] = box(192, 71 + i * 42, 42, 42);
  for (int i = 0; i < 4; ++i) {
    c.towns[i] = {card.x + 72 * sc, side.y + (218 + i * 44) * sc, 59 * sc, 44 * sc};
    c.teams[i] = {card.x + 174 * sc, side.y + (218 + i * 44) * sc, 59 * sc, 44 * sc};
  }
  return c;
}

inline SDL_Point original_tooltip_anchor(const SDL_Rect& side, int scale, bool town, int x, int y) {
  const auto controls = original_panel_controls(side, scale);
  const SDL_Point point{x,y};
  auto anchor = [&](const auto& boxes) -> SDL_Point {
    for (const auto& box : boxes) if (SDL_PointInRect(&point, &box)) return {box.x,box.y};
    return {-1,-1};
  };
  for (auto p : {anchor(town ? controls.town : controls.squad), anchor(controls.towns), anchor(controls.teams)})
    if (p.x >= 0) return p;
  if (town) {
    auto p = anchor(controls.build); if (p.x >= 0) return p;
    p = anchor(controls.queue); if (p.x >= 0) return p;
  } else { auto p = anchor(controls.members); if (p.x >= 0) return p; }
  return point;
}

inline SDL_Rect original_minimap_rect(SDL_Rect box, int map_w, int map_h, int scale) {
  if (map_w <= 0 || map_h <= 0) return {box.x, box.y, 0, 0};
  // V: integer pixels per tile. Oversized maps have zero scale; do not divide by it.
  int step = 180 / std::max(map_w, map_h) * std::max(1, scale);
  int w = map_w * step, h = map_h * step;
  return {box.x + (box.w - w) / 2, box.y + (box.h - h) / 2, w, h};
}
}  // namespace aztec::engine
