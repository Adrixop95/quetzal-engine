// Original right-hand panel (Res/common.xbe sprite 292, 204 px wide) with the minimap window, menu bar and gold.
#pragma once
#include <SDL2/SDL.h>
#include <string>
#include "engine/render/font.hpp"
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
  void draw(SDL_Renderer* r, SpriteStore& store, const Font& font, int gold) const;

 private:
  bool ok_ = false;
  int scale_ = 1;
  SDL_Rect rect_{0, 0, 0, 0}, mini_{0, 0, 0, 0};
};

}  // namespace aztec::engine
