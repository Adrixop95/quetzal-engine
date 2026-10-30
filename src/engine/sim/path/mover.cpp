#include "mover.hpp"
#include <algorithm>

namespace aztec::path {

namespace {
constexpr i32 kHashCell = 96 * kFix;
inline Vec2 tile_center(int x, int y) { return {x * kTileFix + kTileFix / 2, y * kTileFix + kTileFix / 2}; }
inline i32 floor_div(i32 a, i32 b) { i32 q = a / b; return (a % b != 0 && (a < 0)) ? q - 1 : q; }
constexpr int kDa[13] = {0, 6, -6, 12, -12, 20, -20, 32, -32, 48, -48, 64, -64};
}  // namespace

u32 MoveSystem::add_agent(Vec2 pos, i32 radius, i32 speed, MoveClass cls) {
  Agent a; a.id = (u32)agents_.size(); a.pos = a.prev_pos = a.win_start = pos; a.radius = radius; a.speed = speed; a.cls = cls;
  agents_.push_back(a);
  return a.id;
}

void MoveSystem::ensure_field(Order& o, MoveClass c) {
  int k = (int)c;
  if (o.have[k] && o.ff[k].version() == grid_.version()) return;
  Cell g = o.goal_cell;
  o.ff[k].build(grid_, c, &g, 1, cfg_.wall_penalty);
  o.have[k] = true; ++stats.fields_built;
}

void MoveSystem::stop(u32 id) {
  Agent& a = agents_[id];
  if (a.order >= 0) { --orders_[a.order]->refs; a.order = -1; }
  a.state = AgentState::Idle; a.vel = {0, 0};
  a.target = {}; a.target_until = 0; a.backoff_ticks = 0;
}

void MoveSystem::order_move(const u32* ids, int n, Vec2 goal) {
  if (n <= 0) return;
  int oi = -1;
  for (size_t i = 0; i < orders_.size(); ++i) if (orders_[i]->refs == 0) { oi = (int)i; break; }
  if (oi < 0) { orders_.push_back(std::make_unique<Order>()); oi = (int)orders_.size() - 1; }
  Order& o = *orders_[oi];
  o = Order{}; o.goal = goal; o.refs = n;
  int gx = floor_div(goal.x, kTileFix), gy = floor_div(goal.y, kTileFix);
  MoveClass strict = MoveClass::Small; i32 maxr = 0;
  for (int i = 0; i < n; ++i) { Agent& a = agents_[ids[i]]; if (a.cls == MoveClass::Large) strict = MoveClass::Large; if (a.radius > maxr) maxr = a.radius; }
  bool relocated = false;
  if (!grid_.in_bounds(gx, gy)) { gx = std::min(std::max(gx, 0), grid_.width() - 1); gy = std::min(std::max(gy, 0), grid_.height() - 1); relocated = true; }
  if (grid_.blocked(gx, gy, strict)) { grid_.nearest_free(gx, gy, strict, gx, gy); relocated = true; }
  if (relocated) { goal = tile_center(gx, gy); o.goal = goal; }   // slots must surround the real destination, not the blocked / outside point
  o.goal_cell = {gx, gy};
  ensure_field(o, strict);
  // Unreachable goal: flood from the first agent, aim at the reachable tile nearest to the goal ("closest approach", like the
  // original's FUN_0046ce80 fallback, but chosen before the search instead of after a failed wave).
  {
    Agent& a0 = agents_[ids[0]];
    int sx = floor_div(a0.pos.x, kTileFix), sy = floor_div(a0.pos.y, kTileFix);
    if (grid_.in_bounds(sx, sy) && o.ff[(int)strict].cost(sx, sy) == FlowField::kInf) {
      std::vector<u8> seen((size_t)grid_.width() * grid_.height(), 0); std::vector<Cell> q{{sx, sy}}; seen[(size_t)sy * grid_.width() + sx] = 1;
      i64 best = -1; Cell bc{sx, sy};
      for (size_t qi = 0; qi < q.size(); ++qi) {
        Cell c = q[qi];
        i64 d = (i64)(c.x - gx) * (c.x - gx) + (i64)(c.y - gy) * (c.y - gy);
        if (best < 0 || d < best) { best = d; bc = c; }
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
          if (!dx && !dy) continue;
          int nx = c.x + dx, ny = c.y + dy;
          if (grid_.blocked(nx, ny, strict) || seen[(size_t)ny * grid_.width() + nx]) continue;
          if (dx && dy && (grid_.blocked(c.x + dx, c.y, strict) || grid_.blocked(c.x, c.y + dy, strict))) continue;
          seen[(size_t)ny * grid_.width() + nx] = 1; q.push_back({nx, ny});
        }
      }
      o.goal_cell = bc; o.have[0] = o.have[1] = false; ensure_field(o, strict);
      goal = tile_center(bc.x, bc.y);
    }
  }
  const FlowField& sf = o.ff[(int)strict];
  // destination slots: hex lattice around the goal, nearest first
  i32 sp = 2 * maxr + cfg_.slot_gap;
  std::vector<Vec2> slots;
  for (int R = 2;; R *= 2) {
    slots.clear();
    for (int j = -R; j <= R; ++j)
      for (int i = -R; i <= R; ++i) {
        Vec2 p{goal.x + i * sp + ((j & 1) ? sp / 2 : 0), goal.y + (i32)((i64)j * sp * 222 / 256)};
        int tx = floor_div(p.x, kTileFix), ty = floor_div(p.y, kTileFix);
        if (!grid_.in_bounds(tx, ty) || grid_.blocked(tx, ty, strict) || sf.cost(tx, ty) == FlowField::kInf) continue;
        if (grid_.disc_hits(p, maxr, strict)) continue;
        // A nearby tile across a wall can be globally reachable only by a
        // long detour. The shared field ends at the anchor, so slots must
        // belong to its visible arrival area rather than another room.
        // Single-agent pursuit keeps its nearest collision-free destination.
        if (n > 1 && !grid_.segment_clear(tile_center(o.goal_cell.x, o.goal_cell.y), p, maxr, strict)) continue;
        slots.push_back(p);
      }
    if ((int)slots.size() >= n || R > 64) break;
  }
  auto d2 = [&](Vec2 p) { return len2(p - goal); };
  std::sort(slots.begin(), slots.end(), [&](Vec2 a, Vec2 b) { i64 da = d2(a), db = d2(b); if (da != db) return da < db; if (a.y != b.y) return a.y < b.y; return a.x < b.x; });
  if ((int)slots.size() > n) slots.resize(n);
  while ((int)slots.size() < n) slots.push_back(goal);
  // pair by projection on the approach direction so the front of the group takes the front slots
  Vec2 c{0, 0}; { i64 sx = 0, sy = 0; for (int i = 0; i < n; ++i) { sx += agents_[ids[i]].pos.x; sy += agents_[ids[i]].pos.y; } c = {(i32)(sx / n), (i32)(sy / n)}; }
  Vec2 dir = goal - c; if (dir.x == 0 && dir.y == 0) dir = {kFix, 0};
  dir = with_length(dir, 4096);
  std::vector<u32> ord(ids, ids + n);
  std::sort(ord.begin(), ord.end(), [&](u32 a, u32 b) { i64 pa = dot(agents_[a].pos, dir), pb = dot(agents_[b].pos, dir); if (pa != pb) return pa > pb; return a < b; });
  std::sort(slots.begin(), slots.end(), [&](Vec2 a, Vec2 b) { i64 pa = dot(a, dir), pb = dot(b, dir); if (pa != pb) return pa > pb; if (a.y != b.y) return a.y < b.y; return a.x < b.x; });
  for (int i = 0; i < n; ++i) {
    Agent& a = agents_[ord[i]];
    if (a.order >= 0 && a.order != oi) --orders_[a.order]->refs;
    a.target = {}; a.target_until = 0; a.backoff_ticks = 0;   // drop steering cached for the previous order
    a.order = oi; a.slot = slots[i]; a.state = AgentState::Moving; a.win_start = a.pos; a.win_ticks = 0; a.stuck_level = 0; a.ghost_ticks = a.squeeze_ticks = 0;
    ensure_field(o, a.cls);
  }
}

void MoveSystem::build_hash() {
  hw_ = (grid_.width() * kTileFix) / kHashCell + 2; hh_ = (grid_.height() * kTileFix) / kHashCell + 2;
  hstart_.assign((size_t)hw_ * hh_ + 1, 0); hlist_.assign(agents_.size(), 0);
  auto cell = [&](const Agent& a) { int cx = std::min(std::max(floor_div(a.pos.x, kHashCell), 0), hw_ - 1), cy = std::min(std::max(floor_div(a.pos.y, kHashCell), 0), hh_ - 1); return (size_t)cy * hw_ + cx; };
  for (const Agent& a : agents_) ++hstart_[cell(a) + 1];
  for (size_t i = 1; i < hstart_.size(); ++i) hstart_[i] += hstart_[i - 1];
  std::vector<u32> fill(hstart_.begin(), hstart_.end() - 1);
  for (const Agent& a : agents_) hlist_[fill[cell(a)]++] = a.id;
}

void MoveSystem::neighbours(const Agent& a, std::vector<u32>& out, i32 reach) const {
  out.clear();
  int cx = std::min(std::max(floor_div(a.pos.x, kHashCell), 0), hw_ - 1), cy = std::min(std::max(floor_div(a.pos.y, kHashCell), 0), hh_ - 1);
  for (int y = std::max(cy - 1, 0); y <= std::min(cy + 1, hh_ - 1); ++y)
    for (int x = std::max(cx - 1, 0); x <= std::min(cx + 1, hw_ - 1); ++x) {
      size_t c = (size_t)y * hw_ + x;
      for (u32 k = hstart_[c]; k < hstart_[c + 1]; ++k) {
        u32 j = hlist_[k]; if (j == a.id) continue;
        if (len2(agents_[j].pos - a.pos) <= (i64)reach * reach) out.push_back(j);
      }
    }
  std::sort(out.begin(), out.end());
}

Vec2 MoveSystem::preferred_velocity(Agent& a, const Order& o) {
  Vec2 to = a.slot - a.pos; i32 dist = length(to);
  if (dist <= a.speed) return to;                                   // lands exactly on the slot
  if (tick_ < a.target_until && length(a.target - a.pos) > 2 * kTileFix) { Vec2 d0 = a.target - a.pos; return with_length(d0, a.speed); }
  const FlowField& ff = o.ff[(int)a.cls];
  Vec2 target = a.slot;
  bool direct = dist < 16 * kTileFix && grid_.segment_clear(a.pos, a.slot, a.radius * 9 / 10, a.cls);
  if (!direct) {
    int tx = floor_div(a.pos.x, kTileFix), ty = floor_div(a.pos.y, kTileFix);
    if (!grid_.in_bounds(tx, ty) || ff.cost(tx, ty) == FlowField::kInf) { target = a.slot; }
    else {
      Cell cur{tx, ty}; Vec2 best = tile_center(tx, ty); bool tail_goal = false;
      for (int k = 0; k < 10; ++k) {
        int nx, ny; if (!ff.step(cur.x, cur.y, nx, ny)) { tail_goal = true; break; }
        cur = {nx, ny};
        Vec2 c = tile_center(nx, ny);
        if (grid_.segment_clear(a.pos, c, a.radius * 9 / 10, a.cls)) best = c; else if (k > 1) break;
      }
      target = best;
      if (tail_goal && grid_.segment_clear(a.pos, a.slot, a.radius * 9 / 10, a.cls)) target = a.slot;
    }
  }
  a.target = target; a.target_until = tick_ + 3 + (a.id & 3);
  Vec2 d = target - a.pos; if (d.x == 0 && d.y == 0) return {0, 0};
  return with_length(d, a.speed);
}

Vec2 MoveSystem::choose_velocity(const Agent& a, Vec2 pref, const std::vector<u32>& nb) const {
  if (pref.x == 0 && pref.y == 0) return {0, 0};
  const i32 H = cfg_.avoid_horizon;
  int base = angle_of(pref.x, pref.y); i32 sp = length(pref);
  i64 bestCost = -1; Vec2 best = pref;
  for (int f = 0; f < 3; ++f) {
    i32 s = f == 0 ? sp : f == 1 ? sp * 5 / 10 : 0;
    for (int ci = 0; ci < 13; ++ci) {
      if (f == 2 && ci) break;
      int da = kDa[ci];
      Vec2 vc = f == 2 ? Vec2{0, 0} : from_angle(base + da, s);
      i64 cost = (i64)abs_i(da) * 8 + (f == 1 ? 250 : f == 2 ? 900 : 0);
      if (a.vel.x || a.vel.y) { int pa = angle_of(a.vel.x, a.vel.y); int dd = abs_i(((base + da - pa + 128) & 255) - 128); cost += dd * 2; }
      if (f != 2 && grid_.disc_hits(a.pos + vc, a.radius, a.cls)) cost += 4000;
      if (a.ghost_ticks == 0) {
        for (u32 j : nb) {
          const Agent& b = agents_[j];
          Vec2 p = b.pos - a.pos; i64 R = (i64)a.radius + b.radius + 2 * kFix;
          bool bmov = b.state == AgentState::Moving;
          i64 w = bmov ? 2 : (b.hold ? 2 : 1);
          i64 dist2 = len2(p);
          Vec2 vrel = vc - (bmov ? b.vel : Vec2{0, 0});
          if (dist2 < R * R) { if (dot(vrel, p) > 0) cost += 1500 * w; continue; }
          i64 aa = len2(vrel); if (aa == 0) continue;
          i64 bb = dot(vrel, p); if (bb <= 0) continue;
          i64 disc = bb * bb - aa * (dist2 - R * R); if (disc < 0) continue;
          i64 t16 = ((bb - (i64)isqrt((std::uint64_t)disc)) * 16) / aa;        // ticks * 16
          if (t16 >= (i64)H * 16) continue;
          cost += w * (1200 * ((i64)H * 16 - t16) / ((i64)H * 16)) + (t16 < 32 ? w * 1500 : 0);
        }
      }
      if (bestCost < 0 || cost < bestCost) { bestCost = cost; best = vc; }
    }
  }
  return best;
}

void MoveSystem::resolve_overlaps() {
  std::vector<u32> nb;
  for (int it = 0; it < cfg_.overlap_iterations; ++it) {
    for (Agent& a : agents_) {
      neighbours(a, nb, 2 * (a.radius + 24 * kFix));
      for (u32 j : nb) {
        if (j < a.id) continue;
        Agent& b = agents_[j];
        i32 ra = a.squeeze_ticks ? a.radius * 7 / 10 : a.radius, rb = b.squeeze_ticks ? b.radius * 7 / 10 : b.radius;
        Vec2 d = b.pos - a.pos; i64 dd = len2(d); i64 R = (i64)ra + rb;
        if (dd >= R * R) continue;
        i32 l = (i32)isqrt((std::uint64_t)dd);
        Vec2 n;
        if (l == 0) n = from_angle((int)((a.id * 37u + b.id * 11u + (u32)tick_) & 255), 4096); else n = {(i32)((i64)d.x * 4096 / l), (i32)((i64)d.y * 4096 / l)};
        i32 ov = (i32)(R - l);
        bool am = a.state == AgentState::Moving, bm = b.state == AgentState::Moving;
        i32 sa = 128, sb = 128;                                   // share of the correction (out of 256) taken by a / b
        if (am && !bm) { sa = 48; sb = 208; } else if (!am && bm) { sa = 208; sb = 48; }
        if (!am && a.hold) { sa = 0; sb = 256; } else if (!bm && b.hold) { sb = 0; sa = 256; }
        a.pos.x -= (i32)((i64)n.x * ov / 4096 * sa / 256); a.pos.y -= (i32)((i64)n.y * ov / 4096 * sa / 256);
        b.pos.x += (i32)((i64)n.x * ov / 4096 * sb / 256); b.pos.y += (i32)((i64)n.y * ov / 4096 * sb / 256);
      }
    }
    for (Agent& a : agents_) grid_.push_out(a.pos, a.squeeze_ticks ? a.radius * 7 / 10 : a.radius, a.cls);
  }
  for (Agent& a : agents_) if (grid_.disc_hits(a.pos, a.radius * 6 / 10, a.cls)) a.pos = a.prev_pos;   // never end inside a wall
}

void MoveSystem::tick() {
  ++tick_;
  build_hash();
  std::vector<Vec2> nv(agents_.size());
  std::vector<u32> nb;
  for (Agent& a : agents_) {
    a.prev_pos = a.pos;
    Vec2 pref{0, 0};
    if (a.state == AgentState::Moving && a.order >= 0) {
      Order& o = *orders_[a.order];
      ensure_field(o, a.cls);
      pref = preferred_velocity(a, o);
      if (a.backoff_ticks > 0) { --a.backoff_ticks; pref = {-pref.x / 2, -pref.y / 2}; }   // deadlock breaker: lower priority agent retreats
      else if (a.stuck_level >= 3) pref = rotate(pref, ((a.id & 1) ? 1 : -1) * (24 + (int)((a.id * 7 + tick_) % 24)));
    }
    neighbours(a, nb, a.radius + 24 * kFix + 2 * (a.speed + 12 * kFix) * cfg_.avoid_horizon / 2);
    nv[a.id] = choose_velocity(a, pref, nb);
  }
  for (Agent& a : agents_) {
    a.vel = nv[a.id]; a.pos = a.pos + a.vel;
    if (a.vel.x || a.vel.y) { a.heading = angle_of(a.vel.x, a.vel.y); a.facing = facing8(a.vel); ++a.moved_ticks; }
    if (a.ghost_ticks) --a.ghost_ticks;
    if (a.squeeze_ticks) --a.squeeze_ticks;
  }
  resolve_overlaps();
  build_hash();
  for (Agent& a : agents_) {
    if (a.state != AgentState::Moving) continue;
    i32 dist = length(a.slot - a.pos);
    if (dist <= 2 * kFix) { a.state = AgentState::Arrived; a.vel = {0, 0}; continue; }
    if (++a.win_ticks >= cfg_.stuck_window) {
      i32 moved = length(a.pos - a.win_start);
      if ((i64)moved * 4 < (i64)a.speed * a.win_ticks) {
        ++a.stuck_level; ++stats.stuck_events;
        a.ghost_ticks = cfg_.stuck_window;
        if (a.stuck_level >= 5) a.squeeze_ticks = cfg_.stuck_window * 2;
        if (a.stuck_level >= 4 && ((a.id + (u32)(tick_ / cfg_.stuck_window)) & 1)) { a.backoff_ticks = cfg_.stuck_window / 2; a.stuck_level = 2; }
        // close to the slot and others already settled: take the spot you are in
        if (dist < 3 * kTileFix && a.stuck_level >= 2) {
          neighbours(a, nb, a.radius * 2 + 8 * kFix);
          for (u32 j : nb) if (agents_[j].state == AgentState::Arrived) { a.state = AgentState::Arrived; a.vel = {0, 0}; break; }
        }
      } else a.stuck_level = a.stuck_level > 0 ? a.stuck_level - 1 : 0;
      a.win_start = a.pos; a.win_ticks = 0;
    }
  }
}

u64 MoveSystem::state_hash() const {
  u64 h = 1469598103934665603ull;
  auto mix = [&](u64 v) { h = (h ^ v) * 1099511628211ull; };
  mix(tick_); mix(grid_.state_hash());
  mix(cfg_.avoid_horizon); mix(cfg_.overlap_iterations); mix(cfg_.wall_penalty);
  mix(cfg_.slot_gap); mix(cfg_.stuck_window);
  mix(agents_.size());
  for (const Agent& a : agents_) {
    mix(a.id); mix((u32)a.pos.x); mix((u32)a.pos.y); mix((u32)a.vel.x); mix((u32)a.vel.y); mix((u32)a.prev_pos.x); mix((u32)a.prev_pos.y);
    mix((u32)a.radius); mix((u32)a.speed); mix((u64)a.cls); mix((u64)a.state); mix(a.hold); mix((u32)a.order);
    mix((u32)a.slot.x); mix((u32)a.slot.y); mix((u32)a.facing); mix((u32)a.heading);
    mix((u32)a.win_start.x); mix((u32)a.win_start.y); mix((u32)a.win_ticks); mix((u32)a.stuck_level); mix((u32)a.squeeze_ticks); mix((u32)a.ghost_ticks); mix(a.moved_ticks);
    mix((u32)a.target.x); mix((u32)a.target.y); mix(a.target_until); mix((u32)a.backoff_ticks);
  }
  mix(orders_.size());
  for (const auto& o : orders_) {
    mix((u32)o->refs); mix((u32)o->goal.x); mix((u32)o->goal.y); mix((u32)o->goal_cell.x); mix((u32)o->goal_cell.y); mix(o->have[0]); mix(o->have[1]);
    for (int k = 0; k < 2; ++k) if (o->have[k]) mix(o->ff[k].state_hash());
  }
  return h;
}

}  // namespace aztec::path
