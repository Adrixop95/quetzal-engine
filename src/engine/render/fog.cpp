#include "engine/render/fog.hpp"
#include "engine/world/visibility.hpp"

#include <algorithm>
#include <cmath>

namespace aztec::engine {

Fog::~Fog() { if (tex_) SDL_DestroyTexture(tex_); }

static const VisFile::Layer* local_layer(const World& w) {
  return visibility_layer(w, w.local_side);
}

int Fog::darkness(const World& w, int tx, int ty) const {
  const VisFile::Layer* l = local_layer(w);
  if (!l || tx < 0 || ty < 0 || tx >= l->w || ty >= l->h) return 0;
  int value = l->v[size_t(ty) * l->w + tx];
  return w.visibility_brightness ? 152 - std::min(152, value) : value;
}

void Fog::update(SDL_Renderer* r, const World& w) {
  const VisFile::Layer* l = local_layer(w);
  has_data_ = l != nullptr;
  if (!l) { world_ = nullptr; return; }
  if (tex_ && world_ == &w && side_ == w.local_side && version_ == w.fog_version &&
      width_ == l->w && height_ == l->h && brightness_ == w.visibility_brightness) return;
  if (tex_ && (width_ != l->w || height_ != l->h)) { SDL_DestroyTexture(tex_); tex_ = nullptr; }
  if (!tex_) {
    tex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, l->w, l->h);
    if (!tex_) { has_data_ = false; return; }
    SDL_SetTextureBlendMode(tex_, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex_, SDL_ScaleModeLinear);
  }
  world_ = &w; side_ = w.local_side; version_ = w.fog_version;
  width_ = l->w; height_ = l->h; brightness_ = w.visibility_brightness;
  // Raw samples are vertices, not tile centres. Exclude the extra border
  // from the map extent while retaining it for filtering at the last tile.
  uv_ = brightness_ ? SDL_FRect{0.5f / width_, 0.5f / height_,
      float(w.terrain.w) / width_, float(w.terrain.h) / height_} : SDL_FRect{0, 0, 1, 1};
  std::vector<uint32_t> px(size_t(l->w) * l->h);
  for (size_t i = 0; i < px.size(); i++) {
    int darkness = brightness_ ? 152 - std::min(152, int(l->v[i])) : l->v[i];
    float d = std::min(1.f, darkness / 152.f);
    uint32_t a = uint32_t(std::lround(d * 245.f));  // unexplored: almost black, still hints terrain
    px[i] = a << 24;
  }
  SDL_UpdateTexture(tex_, nullptr, px.data(), l->w * 4);
}

static void draw_fog(SDL_Renderer* r, SDL_Texture* texture, const SDL_FRect& dst, const SDL_FRect& uv) {
  // RenderCopy preserves linear filtering on the software backend, whose
  // geometry path quantizes fractional texture coordinates to source texels.
  SDL_Rect previous{};
  const bool clipped = SDL_RenderIsClipEnabled(r);
  SDL_RenderGetClipRect(r, &previous);
  SDL_Rect clip{int(std::floor(dst.x)), int(std::floor(dst.y)),
      int(std::ceil(dst.x + dst.w)) - int(std::floor(dst.x)),
      int(std::ceil(dst.y + dst.h)) - int(std::floor(dst.y))};
  if (clipped && !SDL_IntersectRect(&clip, &previous, &clip)) return;
  SDL_RenderSetClipRect(r, &clip);
  const SDL_FRect expanded{dst.x - uv.x * dst.w / uv.w, dst.y - uv.y * dst.h / uv.h,
      dst.w / uv.w, dst.h / uv.h};
  SDL_RenderCopyF(r, texture, nullptr, &expanded);
  SDL_RenderSetClipRect(r, clipped ? &previous : nullptr);
}

void Fog::draw(SDL_Renderer* r, const Camera& cam, const World& w) const {
  if (!enabled() || !tex_) return;
  SDL_FRect dst{cam.to_screen_x(0), cam.to_screen_y(0), w.width_px() * cam.zoom, w.height_px() * cam.zoom};
  draw_fog(r, tex_, dst, uv_);
}

void Fog::draw_minimap(SDL_Renderer* r, const SDL_Rect& dst) const {
  if (!enabled() || !tex_) return;
  draw_fog(r, tex_, {float(dst.x), float(dst.y), float(dst.w), float(dst.h)}, uv_);
}

}  // namespace aztec::engine
