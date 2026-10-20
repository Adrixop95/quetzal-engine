// Draws the game world (terrain, overlays, objects) for a Camera. Everything is batched through
// texture atlases: a full-screen terrain is a few SDL_RenderGeometry calls.
#pragma once
#include <memory>
#include <vector>
#include "engine/assets/daylight.hpp"
#include "engine/platform/window.hpp"
#include "engine/render/batch.hpp"
#include "engine/render/font.hpp"
#include "engine/render/fog.hpp"
#include "engine/render/sprite_store.hpp"
#include "engine/world/camera.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

struct SceneOptions {
  int terrain_palette = -1;   // Pal.xbe item; -1 = automatic per tileset
  int terrain_shade = 14;
  bool labels = true;         // type-id labels on placeholder markers
  bool grid = false;          // tile grid
  bool passability = false;   // tint from .dat
  bool markers = true;        // draw placeholders for objects without a sprite
  bool overlays = true;       // terrain overlay layer (high 16 bits of .map cells)
};

struct RenderStats {
  int tiles = 0, overlays = 0, sprites = 0, placeholders = 0, geometry_calls = 0;
};

class SceneRenderer {
 public:
  SceneRenderer(Window& win, const GameData& data, const World& world);
  bool ok() const { return font_ok_; }
  void set_sprite_provider(std::shared_ptr<const SpriteProvider> p) { provider_ = std::move(p); }
  void draw(const Camera& cam, float alpha);
  // Time-of-day light: tint + ambient shade (sprites are baked at shade 14 and modulated).
  void set_light(const Light& l);

  SceneOptions opts;
  RenderStats stats;
  const Font& font() const { return font_; }
  Fog& fog() { return fog_; }
  SpriteStore& store() { return store_; }
  int default_palette() const;

 private:
  void draw_terrain(const Camera& cam);
  void draw_objects(const Camera& cam, float alpha);
  void blit(const Camera& cam, const StoredSprite& s, int frame, float wx, float wy, int& cur_page);
  ObjectState object_state(const Object& o) const;
  bool visible_in_fog(const Object& o, Vec2f p) const;
  void flush(int page);
  void queue(int page, float x0, float y0, float x1, float y1, const SDL_Rect& src);

  Window& win_;
  const GameData& data_;
  const World& world_;
  SpriteStore store_;
  Font font_;
  Fog fog_;
  bool font_ok_ = false;
  std::shared_ptr<const SpriteProvider> provider_;
  SDL_Color mod_{255, 255, 255, 255};  // vertex colour for terrain/trees/objects
  float gain_[20];
  bool gain_init_ = false;
  std::vector<QuadBatch> batches_;  // per atlas page
};

}  // namespace aztec::engine
