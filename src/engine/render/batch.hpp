// Collects textured/coloured quads and submits them with SDL_RenderGeometry (one call per texture).
#pragma once
#include <SDL.h>
#include <vector>
#include "engine/render/atlas.hpp"

namespace aztec::engine {

class QuadBatch {
 public:
  void clear() { v_.clear(); i_.clear(); }
  bool empty() const { return v_.empty(); }
  // Screen-space rectangle sampling `src` (pixels) from a texture of size tw x th.
  void add(float x0, float y0, float x1, float y1, const SDL_Rect& src, int tw, int th, SDL_Color c = {255, 255, 255, 255}) {
    float u0 = float(src.x) / tw, v0 = float(src.y) / th, u1 = float(src.x + src.w) / tw, v1 = float(src.y + src.h) / th;
    int b = int(v_.size());
    v_.push_back({{x0, y0}, c, {u0, v0}});
    v_.push_back({{x1, y0}, c, {u1, v0}});
    v_.push_back({{x1, y1}, c, {u1, v1}});
    v_.push_back({{x0, y1}, c, {u0, v1}});
    for (int k : {0, 1, 2, 0, 2, 3}) i_.push_back(b + k);
  }
  void add_solid(float x0, float y0, float x1, float y1, SDL_Color c) {
    int b = int(v_.size());
    v_.push_back({{x0, y0}, c, {0, 0}});
    v_.push_back({{x1, y0}, c, {0, 0}});
    v_.push_back({{x1, y1}, c, {0, 0}});
    v_.push_back({{x0, y1}, c, {0, 0}});
    for (int k : {0, 1, 2, 0, 2, 3}) i_.push_back(b + k);
  }
  void flush(SDL_Renderer* r, SDL_Texture* tex) {
    if (!v_.empty())
      SDL_RenderGeometry(r, tex, v_.data(), int(v_.size()), i_.data(), int(i_.size()));
    clear();
  }
  size_t quads() const { return v_.size() / 4; }

 private:
  std::vector<SDL_Vertex> v_;
  std::vector<int> i_;
};

}  // namespace aztec::engine
