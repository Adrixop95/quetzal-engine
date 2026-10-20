#include "engine/render/fog.hpp"

#include <algorithm>
#include <cmath>

namespace aztec::engine {

Fog::~Fog() { if (tex_) SDL_DestroyTexture(tex_); }

static const VisFile::Layer* local_layer(const World& w) {
  int s = w.local_side;
  if (s < 0 || s >= int(w.visibility.sides.size())) return nullptr;
  const VisFile::Layer& l = w.visibility.sides[s];
  return l.w == w.terrain.w && l.h == w.terrain.h && l.v.size() >= size_t(l.w) * l.h ? &l : nullptr;
}

int Fog::darkness(const World& w, int tx, int ty) const {
  const VisFile::Layer* l = local_layer(w);
  if (!l || tx < 0 || ty < 0 || tx >= l->w || ty >= l->h) return 0;
  return l->v[size_t(ty) * l->w + tx];
}

void Fog::update(SDL_Renderer* r, const World& w) {
  const VisFile::Layer* l = local_layer(w);
  has_data_ = l != nullptr;
  if (!l || version_ == w.fog_version) return;
  version_ = w.fog_version;
  if (!tex_) {
    tex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, l->w, l->h);
    if (!tex_) { has_data_ = false; return; }
    SDL_SetTextureBlendMode(tex_, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex_, SDL_ScaleModeLinear);
  }
  std::vector<uint32_t> px(size_t(l->w) * l->h);
  for (size_t i = 0; i < px.size(); i++) {
    float d = std::min(1.f, l->v[i] / 152.f);
    uint32_t a = uint32_t(std::lround(d * 245.f));  // unexplored: almost black, still hints terrain
    px[i] = a << 24;
  }
  SDL_UpdateTexture(tex_, nullptr, px.data(), l->w * 4);
}

void Fog::draw(SDL_Renderer* r, const Camera& cam, const World& w) const {
  if (!enabled() || !tex_) return;
  SDL_FRect dst{cam.to_screen_x(0), cam.to_screen_y(0), w.width_px() * cam.zoom, w.height_px() * cam.zoom};
  SDL_RenderCopyF(r, tex_, nullptr, &dst);
}

void Fog::draw_minimap(SDL_Renderer* r, const SDL_Rect& dst) const {
  if (!enabled() || !tex_) return;
  SDL_RenderCopy(r, tex_, nullptr, &dst);
}

}  // namespace aztec::engine
