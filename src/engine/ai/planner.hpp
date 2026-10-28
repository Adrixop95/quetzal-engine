// Independent deterministic town strategy. Unknown reference planner policies are G fallbacks.
#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "engine/sim/commands.hpp"

namespace aztec::engine {
struct World;
class RulesData;
namespace ai {
struct TownState {
  int phase = 0, science_timer = 50, target_town = -1, remaining = 0;
  bool defence_enabled = true;
  uint64_t elapsed = 0;
  int64_t harvested = 0, spent = 0;
  uint32_t last_alert = 0;
  std::vector<int> known_towns;
};
struct Intelligence { int town = -1; std::array<int, 90> counts{}; };
struct Offer {
  int type = -1, price = 0, x = 0, y = 0;
  bool placeable = false;
};
struct Decision { TownState next; std::vector<Command> commands; };

bool visible(const World& world, int observer, uint32_t object);
std::vector<Intelligence> intelligence(const World& world, const RulesData& rules, int observer);
int development(const World& world, const RulesData& rules, int town);
int build_speed(const World& world, const TownState& state, int town);
// Offers are validated placement/build-tree queries supplied by the simulation adapter.
// Neither the world nor the input state is modified; no global RNG or viewing side is read.
Decision step(const World& world, const RulesData& rules, int town,
              const TownState& state, const std::vector<Offer>& offers);
}  // namespace ai
}  // namespace aztec::engine
