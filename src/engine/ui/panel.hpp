// Original right-hand panel (Res/common.xbe sprite 292, 204 px wide) with the minimap window, menu bar and gold.
#pragma once
#include <SDL.h>
#include <string>
#include <vector>
#include "engine/render/font.hpp"
#include "engine/platform/input.hpp"
#include "engine/ui/panel_layout.hpp"
#include "engine/render/sprite_store.hpp"

namespace aztec::engine {

class Panel {
 public:
  bool init(SpriteStore& store);
  bool ok() const { return ok_; }
  // Lays out the panel on the right edge of an out_w x out_h window; `scale` is an integer pixel multiplier.
  void layout(int out_w, int out_h, int scale);
  int width() const { return rect_.w; }
  SDL_Rect rect() const { return rect_; }
  SDL_Rect minimap_rect() const { return mini_; }   // free area for the minimap inside the panel
  SDL_Rect menu_rect() const { return in_game_panel_layout(rect_, scale_).menu; }
  bool contains(int x, int y) const;
  // Menu activates on release inside its captured hit region.
  bool update_menu(const Input& in, bool enabled = true);
  void draw_tooltip(SDL_Renderer* r, const Font& font, const std::string& text) const;
  // `info`: lines shown in the first slot area under the minimap (selection summary); `tooltip` drawn near the bottom.
  void draw(SDL_Renderer* r, SpriteStore& store, const Font& font, int gold, const std::vector<std::string>& info = {},
            const std::string& tooltip = {}) const;

 private:
  bool ok_ = false;
  bool menu_down_ = false, menu_pressed_ = false;
  int scale_ = 1;
  SDL_Rect rect_{0, 0, 0, 0}, mini_{0, 0, 0, 0};
};

}  // namespace aztec::engine
