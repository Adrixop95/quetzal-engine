#include "engine/ui/cursor.hpp"

namespace aztec::engine {

static SpriteKey common(uint32_t id) {
  SpriteKey k;
  k.archive = ArchiveId::Common;
  k.group = 0x44c;
  k.index = id;
  return k;
}

static const char* kFallback[] = {
    "X..........", "XX.........", "XWX........", "XWWX.......", "XWWWX......", "XWWWWX.....", "XWWWWWX....",
    "XWWWWWWX...", "XWWWWWWWX..", "XWWWWWWWWX.", "XWWWWWXXXXX", "XWWXWWX....", "XWX.XWWX...", "XX..XWWX...",
    "X....XWWX..", ".....XWWX..", "......XX...",
};

CursorKind Cursor::scroll_kind(int dx, int dy) {
  static const CursorKind t[3][3] = {{CursorKind::ScrollNW, CursorKind::ScrollN, CursorKind::ScrollNE},
                                     {CursorKind::ScrollW, CursorKind::Arrow, CursorKind::ScrollE},
                                     {CursorKind::ScrollSW, CursorKind::ScrollS, CursorKind::ScrollSE}};
  return t[(dy < 0 ? 0 : dy > 0 ? 2 : 1)][(dx < 0 ? 0 : dx > 0 ? 2 : 1)];
}

CursorKind Cursor::original_kind(bool ready, bool modal, bool placing, bool in_world,
                                 int edge_x, int edge_y, bool box, bool selected,
                                 int service_mode, bool hostile) {
  if (!ready) return CursorKind::Arrow;
  if (modal || placing || !in_world) return CursorKind::Menu;
  if (edge_x || edge_y) return scroll_kind(edge_x, edge_y);
  if (box || !selected) return CursorKind::Menu;
  if (service_mode == 2) return CursorKind::Move;
  if (service_mode != 0) return CursorKind::Pan;
  return hostile ? CursorKind::Attack : CursorKind::Menu;
}

bool Cursor::init(SDL_Renderer* r, SpriteStore& store) {
  // V: ordinary scrolling uses 14..21; 6..13 are the blocked variants.
  static const uint32_t ids[] = {1, 0, 2, 5, 14, 21, 20, 19, 18, 17, 16, 15, 4};
  static_assert(sizeof ids / sizeof *ids == int(CursorKind::Count), "cursor table");
  for (int i = 0; i < int(CursorKind::Count); i++) {
    defs_[i].key = common(ids[i]);
    defs_[i].key.shade = 0;
  }
  bool ok = store.get(defs_[0].key).ok();
  if (!ok) {  // no original images: tiny built-in arrow
    uint32_t px[11 * 17] = {};
    for (int y = 0; y < 17; y++)
      for (int x = 0; x < 11; x++) {
        char c = kFallback[y][x];
        px[y * 11 + x] = c == 'X' ? 0xFF000000u : c == 'W' ? 0xFFFFFFFFu : 0;
      }
    fallback_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 11, 17);
    if (fallback_) { SDL_SetTextureBlendMode(fallback_, SDL_BLENDMODE_BLEND); SDL_UpdateTexture(fallback_, nullptr, px, 11 * 4); }
  }
  return true;
}

void Cursor::draw(SDL_Renderer* r, SpriteStore& store, int x, int y, CursorKind k, int scale, uint32_t t) const {
  const StoredSprite& s = store.get(defs_[int(k)].key);
  if (!s.ok()) {
    if (fallback_) { SDL_Rect d{x, y, 11 * scale, 17 * scale}; SDL_RenderCopy(r, fallback_, nullptr, &d); }
    return;
  }
  SDL_Rect src = s.frame_rect(s.frames > 1 ? int(t / 80) : 0);
  SDL_Rect dst{x - s.ox * scale, y - s.oy * scale, s.cw * scale, s.ch * scale};
  SDL_RenderCopy(r, store.atlas().page(s.region.page), &src, &dst);
}

}  // namespace aztec::engine
