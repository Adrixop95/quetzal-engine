#include "flowfield.hpp"

namespace aztec::path {

namespace {
constexpr int kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
constexpr int kDy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
constexpr u32 kStep[8] = {10, 10, 10, 10, 14, 14, 14, 14};
}  // namespace

void FlowField::build(const NavGrid& g, MoveClass cls, const Cell* goals, int ngoals, int wall_penalty) {
  w_ = g.width(); h_ = g.height(); cls_ = cls; grid_ = &g; version_ = g.version();
  cost_.assign((size_t)w_ * h_, kInf);
  const u32 kMaxStep = 14 + (u32)wall_penalty;
  std::vector<std::vector<u32>> buckets(kMaxStep + 1);
  u32 cur = 0, pending = 0;
  for (int i = 0; i < ngoals; ++i) {
    int x = goals[i].x, y = goals[i].y;
    if (g.blocked(x, y, cls) && !g.nearest_free(x, y, cls, x, y)) continue;
    u32& c = cost_[(size_t)y * w_ + x];
    if (c == 0) continue;
    c = 0; buckets[0].push_back((u32)(y * w_ + x)); ++pending;
  }
  while (pending) {
    auto& b = buckets[cur % buckets.size()];
    for (size_t bi = 0; bi < b.size(); ++bi) {          // b may grow only for step-cost 0 (never), so index loop is safe
      u32 idx = b[bi]; --pending;
      if (cost_[idx] != cur) continue;                   // stale
      int x = (int)(idx % w_), y = (int)(idx / w_);
      for (int d = 0; d < 8; ++d) {
        int nx = x + kDx[d], ny = y + kDy[d];
        if (g.blocked(nx, ny, cls)) continue;
        if (d >= 4 && (g.blocked(x + kDx[d], y, cls) || g.blocked(x, y + kDy[d], cls))) continue;   // no corner cutting
        u32 step = kStep[d] + (wall_penalty && g.near_wall(nx, ny, cls) ? (u32)wall_penalty : 0);
        u32 nc = cur + step;
        u32& cc = cost_[(size_t)ny * w_ + nx];
        if (nc < cc) { cc = nc; buckets[nc % buckets.size()].push_back((u32)(ny * w_ + nx)); ++pending; }
      }
    }
    b.clear();
    ++cur;
  }
  // Costs are immutable until the next build; cache their digest off the tick path.
  state_hash_ = 1469598103934665603ull;
  auto mix = [&](u64 value) { state_hash_ = (state_hash_ ^ value) * 1099511628211ull; };
  mix(w_); mix(h_); mix(u32(cls_)); mix(version_); mix(cost_.size());
  for (u32 value : cost_) mix(value);
}

bool FlowField::step(int x, int y, int& nx, int& ny) const {
  if (!reachable(x, y) || cost(x, y) == 0) return false;
  u32 best = kInf; bool found = false;
  for (int d = 0; d < 8; ++d) {
    int tx = x + kDx[d], ty = y + kDy[d];
    if (!in_bounds(tx, ty)) continue;
    u32 c = cost(tx, ty);
    if (c == kInf) continue;
    // Compare complete edges: a diagonal must earn its extra cost. The
    // current tile wall penalty is shared by all outgoing choices.
    u32 candidate = c + kStep[d];
    if (candidate >= best) continue;
    if (d >= 4 && (grid_->blocked(x + kDx[d], y, cls_) || grid_->blocked(x, y + kDy[d], cls_))) continue;
    best = candidate; nx = tx; ny = ty; found = true;
  }
  return found;
}

bool FlowField::closest_reachable(int x, int y, int& ox, int& oy) const {
  i64 best = -1;
  for (int ty = 0; ty < h_; ++ty)
    for (int tx = 0; tx < w_; ++tx) {
      if (cost(tx, ty) == kInf) continue;
      i64 d = (i64)(tx - x) * (tx - x) + (i64)(ty - y) * (ty - y);
      if (best < 0 || d < best) { best = d; ox = tx; oy = ty; }
    }
  return best >= 0;
}

}  // namespace aztec::path
