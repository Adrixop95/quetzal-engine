// Deterministic integer math for the movement/path module: no floating point anywhere.
// Angles use the original game's 256-step circle (0 = +x, 64 = +y in screen coords, i.e. clockwise on screen).
// Positions are 24.8 fixed point pixels (1/256 px), like the original object fields +0x6c/+0x70.
#pragma once
#include <cstdint>
#include <cstdlib>

namespace aztec::path {

using i32 = std::int32_t;
using i64 = std::int64_t;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using u8 = std::uint8_t;

constexpr int kTilePx = 48;            // tile size in pixels
constexpr int kFix = 256;              // 1 px = 256 fixed units
constexpr i32 kTileFix = kTilePx * kFix;

struct Vec2 {
  i32 x = 0, y = 0;
  constexpr bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
  constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
  constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
};

inline constexpr i64 dot(Vec2 a, Vec2 b) { return (i64)a.x * b.x + (i64)a.y * b.y; }
inline constexpr i64 len2(Vec2 a) { return dot(a, a); }

// floor(sqrt(v)) for 64-bit values (bitwise, exact).
inline constexpr u32 isqrt(std::uint64_t v) {
  std::uint64_t r = 0, bit = std::uint64_t(1) << 62;
  while (bit > v) bit >>= 2;
  while (bit) { if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; }
  return (u32)r;
}
inline constexpr i32 length(Vec2 a) { return (i32)isqrt((std::uint64_t)len2(a)); }

// sin of a quarter wave, 65 entries (0..64), scaled by 4096.
inline constexpr i32 kQuarterSin[65] = {
0, 101, 201, 301, 401, 501, 601, 700, 799, 897, 995, 1092, 1189,
1285, 1380, 1474, 1567, 1660, 1751, 1842, 1931, 2019, 2106, 2191, 2276, 2359,
2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229, 3290,
3349, 3406, 3461, 3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822, 3857, 3889,
3920, 3948, 3973, 3996, 4017, 4036, 4052, 4065, 4076, 4085, 4091, 4095, 4096
};
inline constexpr i32 sin4096(int a) {          // a: 256-step angle, any int
  a &= 255; int q = a >> 6, i = a & 63;
  switch (q) { case 0: return kQuarterSin[i]; case 1: return kQuarterSin[64 - i]; case 2: return -kQuarterSin[i]; default: return -kQuarterSin[64 - i]; }
}
inline constexpr i32 cos4096(int a) { return sin4096(a + 64); }

// Angle (0..255) of vector (x,y); (0,0) gives 0. Pure integer: binary search on cross products.
inline int angle_of(i32 x, i32 y) {
  if (x == 0 && y == 0) return 0;
  // reduce to first octant-ish quadrant search: find a in [0,256) with minimal |cross|
  int lo = 0;
  // quadrant
  int quad = 0; i64 ax = x, ay = y;
  if (ax > 0 && ay >= 0) quad = 0;
  else if (ax <= 0 && ay > 0) { quad = 1; i64 t = ax; ax = ay; ay = -t; }
  else if (ax < 0 && ay <= 0) { quad = 2; ax = -ax; ay = -ay; }
  else { quad = 3; i64 t = ax; ax = -ay; ay = t; }
  // now ax>0, ay>=0, angle in [0,64)
  int a = 0, b = 64;
  while (b - a > 1) {
    int m = (a + b) / 2;
    // angle >= m  <=>  ay*cos(m) >= ax*sin(m)
    if (ay * cos4096(m) >= ax * sin4096(m)) a = m; else b = m;
  }
  (void)lo;
  // choose nearer of a, a+1
  i64 ca = ay * cos4096(a) - ax * sin4096(a);          // >= 0
  i64 cb = ax * sin4096(a + 1) - ay * cos4096(a + 1);  // > 0
  if (cb < ca) ++a;
  return (quad * 64 + a) & 255;
}

// Vector of length `len` (fixed units) in direction a.
inline constexpr Vec2 from_angle(int a, i32 len) {
  return {(i32)(((i64)cos4096(a) * len) >> 12), (i32)(((i64)sin4096(a) * len) >> 12)};
}
// Rotate v by `da` angle steps (length preserved up to rounding).
inline constexpr Vec2 rotate(Vec2 v, int da) {
  i64 c = cos4096(da), s = sin4096(da);
  return {(i32)((v.x * c - v.y * s) >> 12), (i32)((v.x * s + v.y * c) >> 12)};
}
// v scaled to length `len` (0 vector stays 0).
inline Vec2 with_length(Vec2 v, i32 len) {
  i32 l = length(v); if (l == 0) return {0, 0};
  return {(i32)((i64)v.x * len / l), (i32)((i64)v.y * len / l)};
}
inline constexpr int abs_i(int v) { return v < 0 ? -v : v; }
// 8-way facing (0 = N, clockwise, as the original) from a vector.
inline int facing8(Vec2 v) { int a = angle_of(v.x, v.y); return (((a + 64 + 16) >> 5) & 7); }

}  // namespace aztec::path
