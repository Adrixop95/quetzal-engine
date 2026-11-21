#include "wave.hpp"
#include <algorithm>
#include <stdexcept>

namespace aztec::path {
namespace {
constexpr int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

// Duplicated corners do not alter first-found ties.
template<class Visit> void ring(Cell c, int r, Visit visit) {
  for (int x = c.x - r; x <= c.x + r; ++x) visit(x, c.y - r);
  for (int x = c.x - r; x <= c.x + r; ++x) visit(x, c.y + r);
  for (int y = c.y - r; y <= c.y + r; ++y) visit(c.x - r, y);
  for (int y = c.y - r; y <= c.y + r; ++y) visit(c.x + r, y);
}
}

int wave_predecessor(std::span<const int> candidates, int previous) {
  if (candidates.empty()) return -1;
  if (candidates.size() == 1) return candidates[0];
  if (candidates.size() == 2) {
    auto distance = [previous](int d) { int n = abs_i(d - previous); return std::min(n, 8 - n); };
    return distance(candidates[0]) < distance(candidates[1]) ? candidates[0] : candidates[1];
  }
  if (candidates[0] != 0) return candidates[1];
  if (candidates[1] == 2 || candidates[2] == 2) return 1;
  if (candidates[1] == 6 || candidates[2] == 6) return 7;
  return 0;
}

WaveResult wave_search(const NavGrid& g, MoveClass cls, Cell start,
                       std::span<const Cell> goals, WaveOptions options) {
  WaveResult result;
  const int w = g.width(), h = g.height();
  if (w <= 0 || h <= 0 || goals.empty()) return result;
  const size_t count = size_t(w) * h;
  if ((!options.fog.empty() && options.fog.size() != count) ||
      (!options.occupied.empty() && options.occupied.size() != count))
    throw std::invalid_argument("wave layers must match the navigation grid");
  const int stride = w + 2;
  std::vector<u16> map(size_t(stride) * (h + 2));
  auto at = [&](int x, int y) -> u16& { return map[size_t(y + 1) * stride + x + 1]; };
  auto clamp = [&](Cell c) { return Cell{std::clamp(c.x, 0, w - 1), std::clamp(c.y, 0, h - 1)}; };
  for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
    const size_t i = size_t(y) * w + x;
    u16 value = g.blocked(x, y, cls) ? 0 : options.wall_sides && g.near_wall(x, y, cls) ? 4 : 9;
    if (!options.fog.empty() && options.fog[i] < 8) value = 10;
    if (!options.occupied.empty() && options.occupied[i] && value != 10) value = 0;
    at(x, y) = value;
  }
  start = clamp(start);
  if (!at(start.x, start.y)) {
    bool found = false;
    const Cell original = start;
    for (int r = 1; r <= std::max(w, h) && !found; ++r)
      ring(original, r, [&](int x, int y) {
        if (!found && g.in_bounds(x, y) && at(x, y)) { start = {x, y}; found = true; }
      });
    if (!found) return result;
  }
  if (options.radius >= 0)
    ring(start, options.radius, [&](int x, int y) { if (g.in_bounds(x, y)) at(x, y) = 0; });
  Cell endpoint = clamp(goals.front());
  for (Cell goal : goals) { goal = clamp(goal); at(goal.x, goal.y) = 15; }
  if (at(start.x, start.y) == 15) {
    result.found = true; result.tiles.push_back(start); result.kinds.push_back(3);
    return result;
  }
  struct Entry { Cell tile; int delay; };
  std::vector<Entry> front{{start, 0}}, next;
  at(start.x, start.y) |= 16;
  // Dynamic fronts avoid the reference's unchecked 2000-entry buffer overflow.
  for (u32 label = 32; !front.empty(); label += 16) {
    if (label > 0xfff0) { result.label_limit = true; break; }
    next.clear();
    for (Entry entry : front) {
      if (entry.delay >= 2) { --entry.delay; next.push_back(entry); continue; }
      ++result.expanded;
      for (int d = 0; d < 8; ++d) {
        // N, NW, W, SW, S, SE, E, NE.
        const int direction = (8 - d) % 8;
        Cell c{entry.tile.x + dx[direction], entry.tile.y + dy[direction]};
        u16& value = at(c.x, c.y);
        if (!(value & 15) || (value & 0xfff0)) continue;
        value |= u16(label);
        next.push_back({c, value & 7});
      }
    }
    for (Entry entry : next) if ((at(entry.tile.x, entry.tile.y) & 15) == 15) {
      endpoint = entry.tile; result.found = true; break;
    }
    if (result.found) break;
    front.swap(next);
  }
  if (!result.found) {
    bool found = false;
    const Cell target = endpoint;
    for (int r = 1; r <= std::max(w, h) && !found; ++r) {
      u16 best = 0xffff;
      ring(target, r, [&](int x, int y) {
        if (!g.in_bounds(x, y)) return;
        u16 label = at(x, y) & 0xfff0;
        if (label && label < best) { best = label; endpoint = {x, y}; found = true; }
      });
    }
    if (!found) return result;
  }
  Cell current = endpoint;
  int previous = 0;
  for (;;) {
    u16 own = at(current.x, current.y);
    result.tiles.push_back(current);
    result.kinds.push_back(u8(((own & 8) | 4) >> 2));
    if (current.x == start.x && current.y == start.y) break;
    u16 best = u16((own & 0xfff0) - 16);
    std::array<int, 8> candidates{}; size_t n = 0;
    for (int d = 0; d < 8; ++d) {
      u16 label = at(current.x + dx[d], current.y + dy[d]) & 0xfff0;
      if (!label || label > best) continue;
      if (label < best) { n = 0; best = label; }
      candidates[n++] = d;
    }
    int direction = wave_predecessor(std::span<const int>(candidates.data(), n), previous);
    // The documented 3-way rule may select a non-candidate. Guard malformed
    // descent rather than following a zero label or wrapping indefinitely.
    if (direction < 0) { result.extraction_failed = true; break; }
    Cell next_tile{current.x + dx[direction], current.y + dy[direction]};
    u16 next_label = at(next_tile.x, next_tile.y) & 0xfff0;
    if (!next_label || next_label >= (own & 0xfff0)) { result.extraction_failed = true; break; }
    current = next_tile; previous = direction;
  }
  std::reverse(result.tiles.begin(), result.tiles.end());
  std::reverse(result.kinds.begin(), result.kinds.end());
  if (result.extraction_failed) { result.tiles.clear(); result.kinds.clear(); }
  result.cells.resize(count);
  for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) result.cells[size_t(y) * w + x] = at(x, y);
  return result;
}

void WaveBudget::begin_tick() {
  for (size_t i = 0; i < granted_.size(); ++i) granted_[i] = waiting_[i].id;
  waiting_ = {};
}

bool WaveBudget::request(u32 id, u32 retries) {
  for (u32 granted : granted_) if (granted == id) return true;
  for (u32& granted : granted_) if (granted == none) { granted = id; return true; }
  for (const Waiting& entry : waiting_) if (entry.id == id) return false;
  for (size_t i = 0; i < waiting_.size(); ++i) if (waiting_[i].id == none || waiting_[i].retries < retries) {
    for (size_t j = waiting_.size() - 1; j > i; --j) waiting_[j] = waiting_[j - 1];
    waiting_[i] = {id, retries}; break;
  }
  return false;
}

}  // namespace aztec::path
