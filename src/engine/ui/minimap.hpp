// Minimap: the mission's .spr preview (or a texture averaged from terrain tiles when absent),
// object dots and the camera rectangle. Click/drag with the left button to move the camera.
#pragma once
#include <SDL.h>
#include "engine/assets/game_data.hpp"
#include "engine/platform/input.hpp"
#include "engine/render/fog.hpp"
#include "engine/world/camera.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

class Minimap {
 public:
  ~Minimap();
  bool init(SDL_Renderer* r, const GameData& data, const World& world, int palette, int shade);
  // Places the minimap in the bottom-left corner, at most `max_frac` of the window's smaller side.
  void layout(int out_w, int out_h, int ui_scale);
  // Fits the minimap (map aspect ratio) into `area`, centred.
  void layout_in(const SDL_Rect& area);
  SDL_Rect rect() const { return rect_; }
  bool contains(int x, int y) const { return x >= rect_.x && y >= rect_.y && x < rect_.x + rect_.w && y < rect_.y + rect_.h; }
  void set_tint(SDL_Color tint) { tint_ = tint; }
  bool from_preview() const { return from_preview_; }
  // Maps a screen point inside the minimap to world pixels.
  void to_world(int sx, int sy, float& wx, float& wy) const;
  void draw(SDL_Renderer* r, const Camera& cam, float alpha, const Fog* fog = nullptr) const;

 private:
  const World* world_ = nullptr;
  SDL_Texture* tex_ = nullptr;
  SDL_Rect rect_{0, 0, 0, 0};
  bool from_preview_ = false;
  SDL_Color tint_{255, 255, 255, 255};
};

}  // namespace aztec::engine
