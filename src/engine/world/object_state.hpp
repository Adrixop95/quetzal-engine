// One read-only simulation snapshot for sprite, light and sound selection.
#pragma once
#include "engine/assets/sprite_provider.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {
inline ObjectState object_sprite_state(const Object& o, int side_color) {
  ObjectState st;
  st.type_id = o.type;
  if (o.type < 0 && o.source == ObjectSource::Boat) st.type_id = -2;
  if (o.type < 0 && o.source == ObjectSource::Johnka) st.type_id = -3;
  st.side = o.side; st.side_color = side_color;
  st.facing = o.facing; st.angle = o.body_angle; st.secondary_angle = o.turret_angle;
  st.anim_state = o.anim_state; st.anim_tick = o.anim_tick; st.anim_variant = o.anim_variant;
  st.body_tick = o.body_tick; st.attack_tick = o.attack_tick;
  st.moving = o.body_moving; st.initial_turret = !o.has_fired;
  st.hp = o.hp; st.max_hp = o.hp_max; st.progress = o.progress;
  st.height = o.render_height; st.destruction_radius = o.destruction_radius;
  st.construction_overlay = o.construction_overlay;
  st.transport_phase = o.transport_phase; st.orientation = o.orientation;
  return st;
}
}  // namespace aztec::engine
