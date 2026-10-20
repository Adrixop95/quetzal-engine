#include "engine/sim/simulation.hpp"
#include "engine/world/type_info.hpp"

#include <algorithm>

namespace aztec::engine {

void WanderSimulation::tick(World& w) {
  w.tick++;
  for (Object& o : w.objects) {
    if (o.source != ObjectSource::Team && o.source != ObjectSource::Town) continue;
    if (const TypeInfo* ti = type_info(o.type); ti && ti->is_structure()) continue;  // structures stay put
    uint32_t r = next();
    int dx = 0, dy = 0;
    if (((r + o.id) & 3) == 0) { dx = int((r >> 3) % 3) - 1; dy = int((r >> 6) % 3) - 1; }
    if (dx == 0 && dy == 0) { o.set_anim(0); continue; }  // idle most ticks
    static const uint8_t kDir[3][3] = {{3, 2, 1}, {4, 0, 0}, {5, 6, 7}};  // [dy+1][dx+1], 0 = east, counter-clockwise
    o.facing = kDir[dy + 1][dx + 1];
    o.set_anim(1);
    o.cur_x = std::clamp(o.cur_x + to_fixed(dx * 6), to_fixed(8), to_fixed(w.width_px() - 8));
    o.cur_y = std::clamp(o.cur_y + to_fixed(dy * 6), to_fixed(8), to_fixed(w.height_px() - 8));
  }
}

}  // namespace aztec::engine
