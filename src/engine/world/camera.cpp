#include "engine/world/camera.hpp"

namespace aztec::engine {

static const float kZoomSteps[] = {0.5f, 0.75f, 1.f, 1.5f, 2.f, 3.f, 4.f};
static const int kNumZoom = sizeof kZoomSteps / sizeof *kZoomSteps;

float Camera::step_zoom(float z, int dir) {
  int best = 0;
  for (int i = 0; i < kNumZoom; i++)
    if (std::fabs(kZoomSteps[i] - z) < std::fabs(kZoomSteps[best] - z)) best = i;
  best = std::clamp(best + dir, 0, kNumZoom - 1);
  return kZoomSteps[best];
}

void Camera::clamp() {
  float vw = view_w / zoom, vh = view_h / zoom;
  // If the world is smaller than the view, centre it; otherwise keep the view inside.
  x = world_w <= vw ? (world_w - vw) / 2 : std::clamp(x, 0.f, world_w - vw);
  y = world_h <= vh ? (world_h - vh) / 2 : std::clamp(y, 0.f, world_h - vh);
}

void Camera::set_zoom(float z, float px, float py) {
  float wx = to_world_x(px), wy = to_world_y(py);
  zoom = z;
  x = wx - px / zoom;
  y = wy - py / zoom;
  clamp();
}

void Camera::update(const CameraInput& in, float dt) {
  if (in.zoom_steps != 0) {
    int dir = in.zoom_steps > 0 ? 1 : -1;
    float nz = step_zoom(zoom, dir);
    if (nz != zoom) set_zoom(nz, in.zoom_pivot_x, in.zoom_pivot_y);
  }
  if (in.dragging) {
    x -= in.drag_dx / zoom;
    y -= in.drag_dy / zoom;
    vx_ = vy_ = 0;
  } else {
    // Exponential approach to the target velocity: smooth start and stop, frame-rate independent.
    float k = 1.f - std::exp(-accel * dt);
    float tx = in.move_x * speed / std::sqrt(zoom), ty = in.move_y * speed / std::sqrt(zoom);
    vx_ += (tx - vx_) * k;
    vy_ += (ty - vy_) * k;
    if (std::fabs(vx_) < 0.5f && tx == 0) vx_ = 0;
    if (std::fabs(vy_) < 0.5f && ty == 0) vy_ = 0;
    x += vx_ * dt;
    y += vy_ * dt;
  }
  clamp();
}

}  // namespace aztec::engine
