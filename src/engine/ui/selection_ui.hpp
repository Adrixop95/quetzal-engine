// Mouse selection of the local player's objects: click or drag a box with the left button.
#pragma once
#include <SDL.h>
#include <functional>
#include "engine/ui/panel_layout.hpp"
#include "engine/platform/input.hpp"
#include "engine/world/camera.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

class SelectionUi {
 public:
  // `active`: the cursor is over the world view (not the minimap/panel). Returns true when it consumed the mouse.
  void set_layout(InterfaceLayout mode) { mode_ = mode; down_ = dragging_ = false; }
  // G: caller-provided object rectangles; native scan ordering remains unresolved.
  std::function<SDL_FRect(const Object&, float)> object_bounds;
  static bool overlaps(const SDL_FRect& a, const SDL_FRect& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
  }
  bool update(const Input& in, World& w, const Camera& cam, float alpha, bool active);
  bool capturing() const { return down_; }
  bool dragging() const { return dragging_; }
  void draw_box(SDL_Renderer* r) const;

 private:
  InterfaceLayout mode_ = InterfaceLayout::Modern;
  bool down_ = false, dragging_ = false;
  int x0_ = 0, y0_ = 0, x1_ = 0, y1_ = 0;
};

}  // namespace aztec::engine
