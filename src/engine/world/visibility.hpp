// Shared visibility contract for simulation, picking and presentation.
#pragma once
#include "engine/sim/sight.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

inline const VisFile::Layer* visibility_layer(const World& w, int side) {
  if (side < 0 || side >= int(w.visibility.sides.size())) return nullptr;
  const auto& l = w.visibility.sides[side];
  const int border = w.visibility_brightness ? 1 : 0;
  if (l.w != w.terrain.w + border || l.h != w.terrain.h + 2 * border ||
      l.w <= 0 || l.h <= 0 || l.v.size() != size_t(l.w) * l.h) return nullptr;
  return &l;
}

inline bool position_visible(const World& w, int side, int x, int y) {
  if (x < 0 || y < 0 || x >= w.width_px() || y >= w.height_px()) return false;
  const auto* l = visibility_layer(w, side);
  // Missing resources preserve the asset-free engine's permissive UI policy.
  if (!l) return !w.visibility_brightness;
  if (w.visibility_brightness) return sight_brightness(*l, x, y) > 32 * 256;
  return l->v[size_t(y / kTilePx) * l->w + x / kTilePx] < 120;
}

inline bool object_visible(const World& w, int side, const Object& o) {
  return o.side == side || position_visible(w, side, o.cur_x >> 8, o.cur_y >> 8);
}

}  // namespace aztec::engine
