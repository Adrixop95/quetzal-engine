// Camera with smooth scrolling (edge scroll, keys, drag) and pixel-exact zoom steps.
#pragma once
#include <algorithm>
#include <cmath>

namespace aztec::engine {

struct CameraInput {
  float move_x = 0, move_y = 0;  // -1..1 desired scroll direction (keys + edge scroll)
  bool dragging = false;         // drag-pan: world follows the cursor
  float drag_dx = 0, drag_dy = 0;  // screen px moved this frame while dragging
  float zoom_steps = 0;          // wheel notches
  float zoom_pivot_x = 0, zoom_pivot_y = 0;  // screen point to keep fixed
};

class Camera {
 public:
  // Top-left of the view in world pixels.
  float x = 0, y = 0;
  float zoom = 1.f;
  float view_w = 1280, view_h = 720;  // screen px
  float world_w = 0, world_h = 0;     // world px

  float speed = 1100.f;   // world px/s at zoom 1 at full deflection
  float accel = 9.f;      // smoothing rate (1/s)

  void set_view(float w, float h) { view_w = w; view_h = h; clamp(); }
  void set_world(float w, float h) { world_w = w; world_h = h; clamp(); }
  void center_on(float wx, float wy) { x = wx - view_w / zoom / 2; y = wy - view_h / zoom / 2; vx_ = vy_ = 0; clamp(); }
  void set_zoom(float z, float pivot_x, float pivot_y);
  // Advance by dt seconds (render rate, not sim rate).
  void update(const CameraInput& in, float dt);

  float to_screen_x(float wx) const { return (wx - x) * zoom; }
  float to_screen_y(float wy) const { return (wy - y) * zoom; }
  float to_world_x(float sx) const { return sx / zoom + x; }
  float to_world_y(float sy) const { return sy / zoom + y; }
  float visible_w() const { return view_w / zoom; }
  float visible_h() const { return view_h / zoom; }

  static float step_zoom(float z, int dir);

 private:
  void clamp();
  float vx_ = 0, vy_ = 0;
};

}  // namespace aztec::engine
