// Tiny built-in 3x5 bitmap font (A-Z, 0-9, punctuation) for labels and HUD. No external font files.
#pragma once
#include <SDL.h>
#include <string>
#include "engine/render/sprite_store.hpp"

namespace aztec::engine {

class Font {
 public:
  bool init(SDL_Renderer* r, SpriteStore* store = nullptr);  // with a store: original font from Res/fnts.xbe
  ~Font();
  // Draws at (x, y) top-left with an integer `scale`; lower-case is drawn as upper-case.
  void draw(SDL_Renderer* r, int x, int y, const std::string& s, int scale, SDL_Color c, bool shadow = true) const;
  int width(const std::string& s, int scale) const;
  int height(int scale) const { return original_ ? 12 * eff(scale) : 5 * scale; }
  bool original() const { return original_; }

 private:
  // `scale` is given in units of the 3x5 font (5 px tall); the 10 px original font uses about half of it.
  int eff(int scale) const { return scale < 2 ? 1 : (scale + 1) / 2; }
  int glyph_advance(const StoredSprite& g, int s) const { return (g.cw + 1) * s; }
  const StoredSprite* glyph(unsigned char cp) const;
  SDL_Texture* tex_ = nullptr;
  SpriteStore* store_ = nullptr;
  bool original_ = false;
};

}  // namespace aztec::engine
