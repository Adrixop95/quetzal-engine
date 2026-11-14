// Front-end geometry in reference 800x600 coordinates. Evidence: docs/frontend_screens.md.
#pragma once
#include <algorithm>
#include <cmath>
#include <SDL.h>

namespace aztec::engine {
struct FrontendLayout {
  float scale;
  int x, y;
  FrontendLayout(int width, int height) {
    scale = std::max(0.001f, std::min(width / 800.f, height / 600.f));
    x = int((width - 800 * scale) / 2);
    y = int((height - 600 * scale) / 2);
  }
  SDL_Rect physical(SDL_Rect r) const {
    return {x + int(r.x * scale), y + int(r.y * scale), int(r.w * scale), int(r.h * scale)};
  }
  SDL_Point virtual_point(int px, int py) const { return {int(std::floor((px - x) / scale)), int(std::floor((py - y) / scale))}; }
  // V: control rectangles inspected in f_0041eef0 (gen_052.c).
  static constexpr SDL_Rect top_left{72, 28, 170, 60}, top_middle{314, 28, 172, 60}, top_right{556, 28, 172, 60};
  static constexpr SDL_Rect bottom_left{72, 512, 170, 60}, bottom_middle{314, 512, 172, 60}, bottom_right{556, 512, 172, 60};
  static constexpr SDL_Rect previous{620, 230, 60, 40}, next{620, 400, 60, 40};
  // V: list origin/count in f_0041eef0; row height in f_004141e0.
  // G: width is confined to the left journal page until a native capture verifies it.
  static SDL_Rect campaign_row(int row) { return {140, 250 + 20 * row, 450, 20}; }
  // V: illustrated briefing hit regions in f_0041eef0.
  static constexpr SDL_Rect briefing_start{643, 338, 135, 176}, briefing_back{0, 0, 151, 226};
  static constexpr SDL_Rect briefing_diary{339, 18, 256, 139};
  static constexpr SDL_Rect briefing_order{158, 197, 279, 321}, briefing_objectives{462, 308, 148, 161};
};
}  // namespace aztec::engine
