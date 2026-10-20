// Software cursor, drawn as the very last thing of every frame at the freshest mouse position
// (like a hardware cursor: it never lags behind the 12 Hz simulation or the world rendering).
#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>

namespace aztec::engine {

enum class CursorKind { Arrow, Pan, Crosshair, Count };

class Cursor {
 public:
  ~Cursor();
  bool init(SDL_Renderer* r);
  // Replaces a built-in image with ARGB pixels (e.g. decoded from the original game cursors later).
  bool set_image(SDL_Renderer* r, CursorKind k, int w, int h, const uint32_t* argb, int hot_x, int hot_y);
  // (x, y) in output pixels; scale = integer pixel multiplier.
  void draw(SDL_Renderer* r, int x, int y, CursorKind k, int scale) const;

 private:
  struct Image { SDL_Texture* tex = nullptr; int w = 0, h = 0, hx = 0, hy = 0; };
  Image img_[int(CursorKind::Count)];
};

}  // namespace aztec::engine
