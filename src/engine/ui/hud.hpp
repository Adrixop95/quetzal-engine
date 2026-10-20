// Debug/info overlay: mission, fps, camera, hovered tile/object, key help.
#pragma once
#include <string>
#include "engine/render/font.hpp"
#include "engine/world/camera.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

struct HudInfo {
  double fps = 0;
  int sim_ticks_per_s = 0;
  bool paused = false;
  int mouse_x = 0, mouse_y = 0;
  float alpha = 0;
  int sprites = 0, placeholders = 0, geometry_calls = 0;
  bool show_help = false;
};

// Returns the object nearest to world point (wx, wy) within `radius` px, or nullptr.
const Object* pick_object(const World& w, float wx, float wy, float alpha, float radius);

void draw_hud(SDL_Renderer* r, const Font& font, const World& w, const Camera& cam, const HudInfo& info, int scale);

}  // namespace aztec::engine
