#include "engine/ui/selection_ui.hpp"

#include <algorithm>
#include <cmath>

#include "engine/ui/hud.hpp"
#include "engine/world/type_info.hpp"

namespace aztec::engine {

bool SelectionUi::update(const Input& in, World& w, const Camera& cam, float alpha, bool active) {
  bool consumed = false;
  if (in.pressed[SDL_BUTTON_LEFT] && active) { down_ = true; dragging_ = false; x0_ = x1_ = in.mx; y0_ = y1_ = in.my; }
  if (down_ && in.buttons[SDL_BUTTON_LEFT]) {
    x1_ = in.mx; y1_ = in.my;
    if (std::abs(x1_ - x0_) > 4 || std::abs(y1_ - y0_) > 4) dragging_ = true;
    consumed = true;
  }
  if (down_ && in.released[SDL_BUTTON_LEFT]) {
    down_ = false;
    consumed = true;
    std::vector<uint32_t> hit;
    if (dragging_) {
      float ax = cam.to_world_x(float(std::min(x0_, x1_))), bx = cam.to_world_x(float(std::max(x0_, x1_)));
      float ay = cam.to_world_y(float(std::min(y0_, y1_))), by = cam.to_world_y(float(std::max(y0_, y1_)));
      for (const Object& o : w.objects) {
        if (o.side != w.local_side || o.type < 0) continue;
        const TypeInfo* ti = type_info(o.type);
        if (ti && ti->is_structure()) continue;  // box select picks units only
        Vec2f p = o.interpolated(alpha);
        if (p.x >= ax && p.x <= bx && p.y >= ay && p.y <= by) hit.push_back(o.id);
      }
    } else if (const Object* o = pick_object(w, cam.to_world_x(float(in.mx)), cam.to_world_y(float(in.my)), alpha, 24.f / cam.zoom + 8)) {
      if (o->side == w.local_side && o->type >= 0) hit.push_back(o->id);
    }
    if (!in.shift) w.selection.clear();
    for (uint32_t id : hit)
      if (!w.selection.contains(id)) w.selection.ids.push_back(id);
    dragging_ = false;
  }
  return consumed;
}

void SelectionUi::draw_box(SDL_Renderer* r) const {
  if (!dragging_) return;
  SDL_Rect b{std::min(x0_, x1_), std::min(y0_, y1_), std::abs(x1_ - x0_), std::abs(y1_ - y0_)};
  SDL_SetRenderDrawColor(r, 90, 255, 90, 40);
  SDL_RenderFillRect(r, &b);
  SDL_SetRenderDrawColor(r, 120, 255, 120, 255);
  SDL_RenderDrawRect(r, &b);
}

}  // namespace aztec::engine
