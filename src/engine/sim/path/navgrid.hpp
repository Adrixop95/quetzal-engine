// Navigation grid: the 48 px tile map with per-class passability, plus dynamic footprints (buildings).
// Terrain bytes are the original .dat passability map: 0 = blocked for everyone, 1 = blocked for large units only,
// 2 = open (verified in WaveSearch layer construction, docs/rules_movement.md).
#pragma once
#include <vector>
#include "fixed.hpp"

namespace aztec::path {

enum class MoveClass : u8 { Small = 0, Large = 1 };   // original: wave class 1 (desc byte +0x2c != 0) / class 0

class NavGrid {
 public:
  NavGrid() = default;
  NavGrid(int w, int h) { reset(w, h); }
  void reset(int w, int h) { w_ = w; h_ = h; terrain_.assign((size_t)w * h, 2); dyn_.assign((size_t)w * h, 0); ++version_; }
  int width() const { return w_; }
  int height() const { return h_; }
  bool in_bounds(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
  u8 terrain(int x, int y) const { return terrain_[(size_t)y * w_ + x]; }
  void set_terrain(int x, int y, u8 v) { terrain_[(size_t)y * w_ + x] = v; ++version_; }
  void load_dat(int w, int h, const u8* bytes) { reset(w, h); for (size_t i = 0; i < (size_t)w * h; ++i) terrain_[i] = bytes[i]; }
  // Dynamic blockers (building footprints): reference counted so overlapping footprints work.
  void add_block(int x, int y, int w, int h, int delta);
  bool blocked(int x, int y, MoveClass c) const {
    if (!in_bounds(x, y)) return true;
    size_t i = (size_t)y * w_ + x;
    if (dyn_[i]) return true;
    return c == MoveClass::Small ? terrain_[i] < 1 : terrain_[i] < 2;
  }
  // Wall proximity: true when any 8-neighbour is blocked (used as a path cost penalty so routes keep off walls).
  bool near_wall(int x, int y, MoveClass c) const;
  u32 version() const { return version_; }

  // Circle (fixed units) vs blocked tiles: true when a disc of radius r centred at p touches a blocked tile.
  bool disc_hits(Vec2 p, i32 r, MoveClass c) const;
  // Straight segment clear for a disc of radius r? (sampled every <= 16 px)
  bool segment_clear(Vec2 a, Vec2 b, i32 r, MoveClass c) const;
  // Push a disc out of blocked tiles (3x3 neighbourhood, one pass). Returns true if moved.
  bool push_out(Vec2& p, i32 r, MoveClass c) const;
  // Nearest passable tile centre to a tile (spiral); false when none within the map.
  bool nearest_free(int x, int y, MoveClass c, int& ox, int& oy) const;

 private:
  int w_ = 0, h_ = 0;
  u32 version_ = 0;
  std::vector<u8> terrain_;
  std::vector<u16> dyn_;
};

}  // namespace aztec::path
