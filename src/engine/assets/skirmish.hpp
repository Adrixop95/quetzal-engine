// Custom single-player setup. Evidence: docs/custom_mission_skirmish.md.
#pragma once
#include <string>
#include <vector>
#include "engine/assets/mission.hpp"

namespace aztec::engine {
struct World;
struct CustomMap {
  std::string spec, title;
  int width = 0, height = 0;
};
std::vector<CustomMap> custom_maps(const std::string& maps_dir);
// G: first non-neutral side with an enabled start is human; all others use the computer planner.
// Failure leaves the world unchanged. Map colours, resources and objects are preserved.
bool setup_skirmish(World& world, std::string& error);
}  // namespace aztec::engine
