// Software cursor, drawn as the very last thing of every frame at the freshest mouse position
// (like a hardware cursor: it never lags behind the 12 Hz simulation or the world rendering).
// Images come from Res/common.xbe (see README for the id table); animated cursors cycle at 80 ms/frame like the original.
#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>
#include "engine/render/sprite_store.hpp"

namespace aztec::engine {

enum class CursorKind {
  Arrow,      // feather (common 1)
  Attack,     // sword (common 0, cell 0)
  Move,       // animated amulet (common 2)
  Pan,        // crossed axes (common 5), used while dragging the map
  ScrollN, ScrollNE, ScrollE, ScrollSE, ScrollS, ScrollSW, ScrollW, ScrollNW,  // edge-scroll arrows (common 6..13)
  Count
};

class Cursor {
 public:
  // Loads the original images through `store`; falls back to a built-in arrow when common.xbe images are missing.
  bool init(SDL_Renderer* r, SpriteStore& store);
  // (x, y) in output pixels; scale = integer pixel multiplier; time_ms drives animation.
  void draw(SDL_Renderer* r, SpriteStore& store, int x, int y, CursorKind k, int scale, uint32_t time_ms) const;
  static CursorKind scroll_kind(int dx, int dy);  // dx,dy in -1..1, not both 0

 private:
  struct Def { SpriteKey key; };
  Def defs_[int(CursorKind::Count)];
  SDL_Texture* fallback_ = nullptr;
};

}  // namespace aztec::engine
