// Offline movement-only probe: no SDL, scripts, combat, reference or asset writes.
#include <iostream>
#include <map>
#include <vector>
#include "engine/sim/path/mover.hpp"
#include "engine/sim/rules.hpp"

int main(int argc, char** argv) {
  using namespace aztec::path;
  using namespace aztec::engine;
  if (argc != 2) return 2;
  RulesData rules;
  if (!rules.load_dir(argv[1])) return 2;
  int w, h, count, ticks;
  if (!(std::cin >> w >> h >> count >> ticks) || w <= 0 || h <= 0 ||
      w > 1024 || h > 1024 || count < 0 || count > 100000 || ticks < 1 || ticks > 10000) return 2;
  NavGrid grid(w, h);
  for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
    int v;
    if (!(std::cin >> v) || v < 0 || v > 2) return 2;
    grid.set_terrain(x, y, u8(v));
  }
  MoveSystem mover(grid);
  struct Group { Cell anchor; std::vector<u32> ids; };
  std::map<int, Group> groups;
  std::vector<int> agents;
  for (int i = 0; i < count; ++i) {
    int type, x, y, group, ax, ay;
    if (!(std::cin >> type >> x >> y >> group >> ax >> ay) || type < 0 || type >= 90 || (group >= 0 && !grid.in_bounds(x, y))) return 2;
    const auto& r = rules.type(type);
    int agent = -1;
    if (r.mobile && !r.flier) {
      agent = int(mover.add_agent({x * kTileFix + kTileFix / 2, y * kTileFix + kTileFix / 2},
          r.radius_px * kFix, std::max(kFix, r.speed), r.large ? MoveClass::Large : MoveClass::Small));
      if (group >= 0) {
        auto [it, inserted] = groups.try_emplace(group, Group{{ax, ay}, {}});
        (void)inserted;
        it->second.ids.push_back(u32(agent));
      }
    } else {
      // All objects obstruct a tile; structures also obstruct their loaded mask.
      mover.block_formation_tile({x, y});
      if (r.structure) {
        if (r.has_footprint_mask) {
          for (int j = 0; j < 49; ++j) if (r.footprint_mask[j] == 2)
            grid.add_block(x + j % 7 - 3, y + j / 7 - 3, 1, 1, 1);
        } else grid.add_block(x - (r.foot_w - 1) / 2, y - (r.foot_h - 1) / 2, r.foot_w, r.foot_h, 1);
      }
    }
    agents.push_back(agent);
  }
  for (const auto& [key, group] : groups) {
    (void)key;
    mover.order_formation(group.ids.data(), int(group.ids.size()), group.anchor);
  }
  for (int t = 1; t <= ticks; ++t) {
    mover.tick();
    for (int i = 0; i < count; ++i) if (agents[size_t(i)] >= 0) {
      const auto& a = mover.agents()[size_t(agents[size_t(i)])];
      std::cout << t << ' ' << i << ' ' << a.pos.x / kTileFix << ' ' << a.pos.y / kTileFix << '\n';
    }
  }
  return 0;
}
