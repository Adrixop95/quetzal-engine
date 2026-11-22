// Minimap: local fog brightness, remembered building footprints, unit dots and camera bounds.
// Legacy/no-fog views use a preview or averaged terrain. Left click/drag moves the camera.
#pragma once
#include <SDL.h>
#include <array>
#include "engine/ui/panel_layout.hpp"
#include <vector>
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
  // Fits the map into `area`, centred, at at most one pixel per tile per UI scale.
  void layout_in(const SDL_Rect& area, int ui_scale = 1, InterfaceLayout mode = InterfaceLayout::Modern);
  void hide() { rect_ = {}; }
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
  SDL_Texture* fog_tex_ = nullptr;
  std::array<uint32_t, 256> fog_palette_{};
  // Runtime palette variants of the user's preview; no game pixels are embedded.
  std::array<std::vector<uint32_t>, 20> preview_shades_;
  int texture_w_ = 0, texture_h_ = 0, default_shade_ = 0;
  SDL_Rect rect_{0, 0, 0, 0};
  bool from_preview_ = false;
  SDL_Color tint_{255, 255, 255, 255};
};

}  // namespace aztec::engine
