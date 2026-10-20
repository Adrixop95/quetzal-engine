#include "engine/render/atlas.hpp"

namespace aztec::engine {

Atlas::~Atlas() {
  for (auto& p : pages_) SDL_DestroyTexture(p.tex);
}

bool Atlas::new_page() {
  Page p;
  p.tex = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, size_, size_);
  if (!p.tex) return false;
  SDL_SetTextureBlendMode(p.tex, SDL_BLENDMODE_BLEND);
  std::vector<uint32_t> zero(size_t(size_) * size_, 0);  // static textures start undefined
  SDL_UpdateTexture(p.tex, nullptr, zero.data(), size_ * 4);
  pages_.push_back(p);
  return true;
}

AtlasRegion Atlas::add(int w, int h, const uint32_t* argb) {
  if (w <= 0 || h <= 0 || w > size_ || h > size_) return {};
  for (int attempt = 0; attempt < 2; attempt++) {
    if (pages_.empty() && !new_page()) return {};
    Page& p = pages_.back();
    if (p.cx + w > size_) { p.cx = 0; p.cy += p.row_h; p.row_h = 0; }
    if (p.cy + h > size_) {
      if (!new_page()) return {};
      continue;
    }
    SDL_Rect r{p.cx, p.cy, w, h};
    SDL_UpdateTexture(p.tex, &r, argb, w * 4);
    AtlasRegion out{int(pages_.size()) - 1, p.cx, p.cy, w, h};
    p.cx += w;
    if (h > p.row_h) p.row_h = h;
    return out;
  }
  return {};
}

}  // namespace aztec::engine
