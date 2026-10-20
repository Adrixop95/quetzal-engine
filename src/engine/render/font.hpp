// Tiny built-in 3x5 bitmap font (A-Z, 0-9, punctuation) for labels and HUD. No external font files.
#pragma once
#include <SDL2/SDL.h>
#include <string>

namespace aztec::engine {

class Font {
 public:
  bool init(SDL_Renderer* r);
  ~Font();
  // Draws at (x, y) top-left with an integer `scale`; lower-case is drawn as upper-case.
  void draw(SDL_Renderer* r, int x, int y, const std::string& s, int scale, SDL_Color c, bool shadow = true) const;
  int width(const std::string& s, int scale) const { return int(s.size()) * 4 * scale - scale; }
  int height(int scale) const { return 5 * scale; }

 private:
  SDL_Texture* tex_ = nullptr;
};

}  // namespace aztec::engine
