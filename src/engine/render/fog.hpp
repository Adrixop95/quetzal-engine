// Fog of war from Maps/<name>.vis: one darkness byte per tile for the local side
// (0 lit, 152 unexplored, in between = explored/remembered). Rendered as a one-texel-per-tile
// texture stretched with linear filtering, so shading is smooth across tile borders.
#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>
#include "engine/world/camera.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

class Fog {
 public:
  ~Fog();
  void set_enabled(bool e) { enabled_ = e; }
  bool enabled() const { return enabled_ && has_data_; }
  // Rebuilds the texture when world.fog_version changed.
  void update(SDL_Renderer* r, const World& w);
  // Darkness (0..152+) at a tile for the local side; 0 when unknown.
  int darkness(const World& w, int tx, int ty) const;
  void draw(SDL_Renderer* r, const Camera& cam, const World& w) const;
  void draw_minimap(SDL_Renderer* r, const SDL_Rect& dst) const;

 private:
  SDL_Texture* tex_ = nullptr;
  uint32_t version_ = 0;
  bool enabled_ = true, has_data_ = false;
};

}  // namespace aztec::engine
