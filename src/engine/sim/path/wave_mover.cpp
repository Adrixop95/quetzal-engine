#include "mover.hpp"
#include <algorithm>

namespace aztec::path {
namespace {
Cell tile(Vec2 p) {
  auto floor_tile = [](i32 v) { return v / kTileFix - (v < 0 && v % kTileFix != 0); };
  return {floor_tile(p.x), floor_tile(p.y)};
}
Vec2 center(Cell c) { return {c.x * kTileFix + kTileFix / 2, c.y * kTileFix + kTileFix / 2}; }
constexpr int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};
}

void MoveSystem::set_fog(u32 team, std::span<const u8> brightness) {
  if (team >= fog_.size()) fog_.resize(size_t(team) + 1);
  fog_[team].assign(brightness.begin(), brightness.end());
}

void MoveSystem::order_wave(const u32* ids, int n, Vec2 goal) {
  if (grid_.width() <= 0 || grid_.height() <= 0) return;
  Cell target = tile(goal);
  target.x = std::clamp(target.x, 0, grid_.width() - 1);
  target.y = std::clamp(target.y, 0, grid_.height() - 1);
  std::vector<u32> sorted(ids, ids + n);
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  for (u32 id : sorted) stop(id);
  int oi = -1;
  for (size_t i = 0; i < orders_.size(); ++i) if (!orders_[i]->refs) { oi = int(i); break; }
  if (oi < 0) { oi = int(orders_.size()); orders_.push_back(std::make_unique<Order>()); }
  *orders_[oi] = Order{};
  Order& order = *orders_[oi]; order.goal = center(target); order.goal_cell = target;
  order.refs = int(sorted.size());
  std::vector<Cell> assigned;
  for (u32 id : sorted) {
    Agent& a = agents_[id];
    // G: unique tile destinations around the command anchor replace the old
    // sub-tile hex slots. The original tactic's group allocation is unresolved.
    Cell destination = a.reserved_tile;
    bool found = false;
    for (int r = 0; r <= std::max(grid_.width(), grid_.height()) && !found; ++r) {
      auto consider = [&](int x, int y) {
        if (found || grid_.blocked(x, y, a.cls)) return;
        for (Cell used : assigned) if (used.x == x && used.y == y) return;
        if (!grid_.connected(a.reserved_tile.x, a.reserved_tile.y, x, y, a.cls, true)) return;
        destination = {x, y}; found = true;
      };
      for (int x = target.x - r; x <= target.x + r; ++x) consider(x, target.y - r);
      for (int x = target.x - r; x <= target.x + r; ++x) consider(x, target.y + r);
      for (int y = target.y - r; y <= target.y + r; ++y) consider(target.x - r, y);
      for (int y = target.y - r; y <= target.y + r; ++y) consider(target.x + r, y);
    }
    assigned.push_back(destination);
    a.order = oi; a.slot = center(destination); a.state = AgentState::Moving;
    a.step_goal = a.pos; a.vel = {};
  }
}

void MoveSystem::tick_wave() {
  if (!cfg_.original_ground || cfg_.unit_unstick) return;
  wave_budget_.begin_tick();
  const int w = grid_.width(), h = grid_.height();
  if (w <= 0 || h <= 0) return;
  std::vector<u32> occupied(size_t(w) * h);
  auto reserve = [&](Cell c, int change) {
    if (grid_.in_bounds(c.x, c.y)) {
      auto& count = occupied[size_t(c.y) * w + c.x];
      if (change > 0) ++count; else if (count) --count;
    }
  };
  for (const Agent& a : agents_) if (a.enabled && a.radius > 0) reserve(original_agent(a) ? a.reserved_tile : tile(a.pos), 1);
  for (Cell c : formation_obstacles_) reserve(c, 1);
  auto enter = [&](const Agent& a, Cell c) {
    return grid_.in_bounds(c.x, c.y) && grid_.can_enter(c.x, c.y, a.cls, occupied[size_t(c.y) * w + c.x] != 0);
  };
  for (Agent& a : agents_) {
    if (!a.enabled || !original_agent(a)) continue;
    a.prev_pos = a.pos;
    if (a.state != AgentState::Moving || a.order < 0) { a.vel = {}; continue; }
    bool request_step = !a.stepping;
    if (a.stepping) {
      if (a.vehicle) {
        Vec2 delta = a.step_goal - a.pos;
        int desired = angle_of(delta.x, delta.y);
        int diff = ((desired - a.heading + 128) & 255) - 128;
        a.heading = vehicle_turn(a.heading, desired);
        // G: the independent sine/atan approximation supplies the direction;
        // no executable-derived trigonometric lookup data is used.
        a.vel = movement_scale(from_angle(a.heading, 4096), a.vehicle_speed_px * kFix);
        a.pos = a.pos + a.vel;
        const i32 distance = movement_norm(a.step_goal - a.pos);
        request_step = distance < 48 * kFix;
        a.vehicle_speed_px = vehicle_speed(a.vehicle_speed_px, a.speed / kFix, distance, diff);
        if (distance < 24 * kFix && !a.vehicle_speed_px) a.stepping = false;
      } else {
        const i32 distance = movement_norm(movement_pixels(a.step_goal) - movement_pixels(a.pos));
        if (distance > a.speed / kFix) {
          a.pos = a.pos + a.vel;
          request_step = !a.about_to_attack && movement_norm(movement_pixels(a.step_goal) - movement_pixels(a.pos)) <= 3 * (a.speed / kFix);
        } else {
          a.pos = a.step_goal; a.vel = {}; a.stepping = false; request_step = true;
        }
      }
      if (a.pos != a.prev_pos) ++a.moved_ticks;
    }
    if (!request_step) continue;
    const Cell current = a.reserved_tile;
    Cell destination = tile(a.slot);
    if (current.x == destination.x && current.y == destination.y) {
      if (!a.stepping) { a.state = AgentState::Arrived; a.vel = {}; }
      continue;
    }
    if (a.wave_path.empty()) {
      if (!wave_budget_.request(a.id, a.retries)) { ++a.retries; continue; }
      a.retries = 0;
      reserve(current, -1);
      WaveOptions options; options.occupied = occupied;
      options.wall_sides = false; // V: member requests use base class 0/1, not margin class 2/3
      if (a.team < fog_.size()) options.fog = fog_[a.team];
      // G: use a global member route until tactic anchors/local-goal lists are
      // implemented. The core supports the documented radius-6 outline.
      auto path = wave_search(grid_, a.cls, current, destination, options);
      reserve(current, 1); ++stats.repaths;
      if (path.tiles.size() < 2 || path.tiles.front().x != current.x || path.tiles.front().y != current.y) {
        ++a.path_failures;
        continue;
      }
      const size_t end = std::min<size_t>(path.tiles.size(), 9);
      a.wave_path.assign(path.tiles.begin() + 1, path.tiles.begin() + end);
    }
    Cell next = a.wave_path.front();
    int direction = -1;
    for (int d = 0; d < 8; ++d) if (next.x - current.x == dx[d] && next.y - current.y == dy[d]) direction = d;
    bool side = false;
    if (direction < 0) { a.wave_path.clear(); continue; }
    if (!enter(a, next)) {
      bool found = false;
      for (int offset : {1, -1}) {
        int d = (direction + offset + 8) % 8;
        Cell candidate{current.x + dx[d], current.y + dy[d]};
        if (enter(a, candidate)) { next = candidate; found = true; side = true; ++stats.sidesteps; break; }
      }
      if (!found) { a.wave_path.clear(); ++a.path_failures; continue; }
    }
    if (side) a.wave_path.clear(); else a.wave_path.erase(a.wave_path.begin());
    // Exactly one owned tile; the old tile becomes available before displacement.
    reserve(current, -1); reserve(next, 1); a.reserved_tile = next;
    a.step_from = a.pos; a.step_goal = center(next); a.stepping = true;
    if (!a.vehicle) {
      a.vel = marine_velocity(a.pos, a.step_goal, a.speed);
      a.heading = angle_of(a.vel.x, a.vel.y);
    }
    a.facing = facing8(a.step_goal - a.pos);
  }
}

}  // namespace aztec::path
