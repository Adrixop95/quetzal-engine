// Shared visibility contract for simulation, picking and presentation.
#pragma once
#include "engine/sim/sight.hpp"
#include "engine/world/world.hpp"
#include "engine/world/type_info.hpp"

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

inline bool visibility_structure(const World& w, const Object& o) {
  if (w.visibility_rules) return w.visibility_rules->type(o.type).structure;
  const auto* info = type_info(o.type);
  return info && info->is_structure();
}

inline std::array<int, 4> visibility_bounds(const World& w, const Object& o) {
  auto bounds = w.visibility_rules ? w.visibility_rules->type(o.type).combat_bounds :
      std::array<int, 4>{-28, -48, 28, 16}; // G: resource-free bounds fallback
  for (int i : {0, 2}) bounds[i] += o.cur_x >> 8;
  for (int i : {1, 3}) bounds[i] += (o.cur_y >> 8) - o.render_height;
  return bounds;
}

inline bool building_visible(const World& w, int side, const Object& o) {
  if (w.width_px() <= 0 || w.height_px() <= 0) return false;
  const auto* layer = visibility_layer(w, side);
  if (!w.visibility_brightness || !layer)
    return position_visible(w, side, o.cur_x >> 8, o.cur_y >> 8);
  const auto bounds = visibility_bounds(w, o);
  int left = bounds[0], right = bounds[2], top = bounds[1], bottom = bounds[3];
  if (right <= 0 || bottom <= 0 || left >= w.width_px() - 1 || top >= w.height_px() - 1) return false;
  left = std::clamp(left, 0, w.width_px() - 1) / kTilePx;
  top = std::clamp(top, 0, w.height_px() - 1) / kTilePx;
  right = std::clamp(right, 0, w.width_px() - 1) / kTilePx + 1;
  bottom = std::clamp(bottom, 0, w.height_px() - 1) / kTilePx + 1;
  for (int y : {top, bottom}) for (int x : {left, right})
    if (layer->v[size_t(y) * layer->w + x] > 32) return true;
  return false;
}

inline bool object_visible(const World& w, int side, const Object& o) {
  if (!w.visibility_brightness && o.side == side) return true;
  return visibility_structure(w, o) ? building_visible(w, side, o) :
      position_visible(w, side, o.cur_x >> 8, o.cur_y >> 8);
}

// Freeze the last observed building state, including animation clocks and owner.
// Updating all sides keeps view changes independent of simulation and draw cadence.
inline void update_building_memory(World& w) {
  if (!w.visibility_brightness) return;
  w.building_memory.resize(w.visibility.sides.size());
  for (size_t side = 0; side < w.building_memory.size(); ++side) {
    auto& memory = w.building_memory[side];
    for (auto it = memory.begin(); it != memory.end();) {
      it->second.prev_x = it->second.cur_x; it->second.prev_y = it->second.cur_y;
      const Object* source = w.find(it->first);
      if ((!source || source->removed || !source->alive) &&
          position_visible(w, int(side), it->second.cur_x >> 8, it->second.cur_y >> 8))
        it = memory.erase(it);
      else ++it;
    }
    for (const auto& o : w.objects) {
      if (!visibility_structure(w, o) || o.removed || !o.alive || !building_visible(w, int(side), o)) continue;
      // A newly observed replacement invalidates overlapping orphan pictures.
      if (!memory.contains(o.id)) for (auto it = memory.begin(); it != memory.end();) {
        const Object* old = w.find(it->first);
        const auto a = visibility_bounds(w, o), b = visibility_bounds(w, it->second);
        if (it->first != o.id && (!old || old->removed || !old->alive) &&
            a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3]) it = memory.erase(it);
        else ++it;
      }
      memory[o.id] = o;
    }
  }
}

}  // namespace aztec::engine
