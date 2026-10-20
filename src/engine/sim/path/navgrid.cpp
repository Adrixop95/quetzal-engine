#include "navgrid.hpp"

namespace aztec::path {

void NavGrid::add_block(int x0, int y0, int bw, int bh, int delta) {
  for (int y = y0; y < y0 + bh; ++y)
    for (int x = x0; x < x0 + bw; ++x)
      if (in_bounds(x, y)) { u16& d = dyn_[(size_t)y * w_ + x]; d = (u16)((int)d + delta); }
  ++version_;
}

bool NavGrid::near_wall(int x, int y, MoveClass c) const {
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx)
      if ((dx || dy) && blocked(x + dx, y + dy, c)) return true;
  return false;
}

bool NavGrid::disc_hits(Vec2 p, i32 r, MoveClass c) const {
  i32 x0 = (p.x - r) / kTileFix, x1 = (p.x + r) / kTileFix, y0 = (p.y - r) / kTileFix, y1 = (p.y + r) / kTileFix;
  if (p.x - r < 0) x0 = -1;
  if (p.y - r < 0) y0 = -1;
  for (i32 ty = y0; ty <= y1; ++ty)
    for (i32 tx = x0; tx <= x1; ++tx) {
      if (!blocked(tx, ty, c)) continue;
      // closest point of the tile rectangle to p
      i32 rx0 = tx * kTileFix, ry0 = ty * kTileFix, rx1 = rx0 + kTileFix, ry1 = ry0 + kTileFix;
      i32 cx = p.x < rx0 ? rx0 : p.x > rx1 ? rx1 : p.x, cy = p.y < ry0 ? ry0 : p.y > ry1 ? ry1 : p.y;
      if (len2({p.x - cx, p.y - cy}) < (i64)r * r) return true;
    }
  return false;
}

bool NavGrid::segment_clear(Vec2 a, Vec2 b, i32 r, MoveClass c) const {
  Vec2 d = b - a; i32 l = length(d);
  int n = l / (16 * kFix) + 1;
  for (int i = 0; i <= n; ++i) {
    Vec2 p{a.x + (i32)((i64)d.x * i / n), a.y + (i32)((i64)d.y * i / n)};
    if (disc_hits(p, r, c)) return false;
  }
  return true;
}

bool NavGrid::push_out(Vec2& p, i32 r, MoveClass c) const {
  bool moved = false;
  i32 tx0 = p.x / kTileFix - 1, ty0 = p.y / kTileFix - 1;
  if (p.x < 0) tx0 = -2;
  if (p.y < 0) ty0 = -2;
  for (i32 ty = ty0; ty <= ty0 + 2; ++ty)
    for (i32 tx = tx0; tx <= tx0 + 2; ++tx) {
      if (!blocked(tx, ty, c)) continue;
      i32 rx0 = tx * kTileFix, ry0 = ty * kTileFix, rx1 = rx0 + kTileFix, ry1 = ry0 + kTileFix;
      i32 cx = p.x < rx0 ? rx0 : p.x > rx1 ? rx1 : p.x, cy = p.y < ry0 ? ry0 : p.y > ry1 ? ry1 : p.y;
      Vec2 d{p.x - cx, p.y - cy};
      i64 dd = len2(d);
      if (dd >= (i64)r * r) continue;
      if (dd == 0) {   // centre inside the rectangle: leave through the nearest side
        i32 l = p.x - rx0, rr = rx1 - p.x, u = p.y - ry0, dn = ry1 - p.y, m = l;
        int side = 0; if (rr < m) { m = rr; side = 1; } if (u < m) { m = u; side = 2; } if (dn < m) { m = dn; side = 3; }
        if (side == 0) p.x = rx0 - r; else if (side == 1) p.x = rx1 + r; else if (side == 2) p.y = ry0 - r; else p.y = ry1 + r;
      } else {
        i32 l = (i32)isqrt((std::uint64_t)dd);
        p.x = cx + (i32)((i64)d.x * r / l); p.y = cy + (i32)((i64)d.y * r / l);
      }
      moved = true;
    }
  return moved;
}

bool NavGrid::nearest_free(int x, int y, MoveClass c, int& ox, int& oy) const {
  x = x < 0 ? 0 : x >= w_ ? w_ - 1 : x;
  y = y < 0 ? 0 : y >= h_ ? h_ - 1 : y;
  int maxr = w_ > h_ ? w_ : h_;
  for (int r = 0; r <= maxr; ++r) {
    i64 best = -1; int bx = 0, by = 0;
    for (int dy = -r; dy <= r; ++dy)
      for (int dx = -r; dx <= r; ++dx) {
        if ((dx < 0 ? -dx : dx) != r && (dy < 0 ? -dy : dy) != r) continue;
        if (blocked(x + dx, y + dy, c)) continue;
        i64 d = (i64)dx * dx + (i64)dy * dy;
        if (best < 0 || d < best) { best = d; bx = x + dx; by = y + dy; }
      }
    if (best >= 0) { ox = bx; oy = by; return true; }
  }
  return false;
}

}  // namespace aztec::path
