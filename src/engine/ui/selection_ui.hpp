// Mouse selection of the local player's objects: click or drag a box with the left button.
#pragma once
#include <SDL.h>
#include "engine/platform/input.hpp"
#include "engine/world/camera.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

class SelectionUi {
 public:
  // `active`: the cursor is over the world view (not the minimap/panel). Returns true when it consumed the mouse.
  bool update(const Input& in, World& w, const Camera& cam, float alpha, bool active);
  bool dragging() const { return dragging_; }
  void draw_box(SDL_Renderer* r) const;

 private:
  bool down_ = false, dragging_ = false;
  int x0_ = 0, y0_ = 0, x1_ = 0, y1_ = 0;
};

}  // namespace aztec::engine
