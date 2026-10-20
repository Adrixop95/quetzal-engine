#include "engine/sim/simulation.hpp"

#include <algorithm>

namespace aztec::engine {

void WanderSimulation::tick(World& w) {
  w.tick++;
  for (Object& o : w.objects) {
    if (o.source != ObjectSource::Team && o.source != ObjectSource::Town) continue;
    if (o.type >= 10 && o.type <= 26) continue;  // buildings (UGI 10..26) stay put
    uint32_t r = next();
    if (((r + o.id) & 3) != 0) {
      continue;  // idle most ticks
    }
    int dx = int((r >> 3) % 3) - 1, dy = int((r >> 6) % 3) - 1;
    o.cur_x = std::clamp(o.cur_x + to_fixed(dx * 16), to_fixed(8), to_fixed(w.width_px() - 8));
    o.cur_y = std::clamp(o.cur_y + to_fixed(dy * 16), to_fixed(8), to_fixed(w.height_px() - 8));
  }
}

}  // namespace aztec::engine
