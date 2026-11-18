#include "engine/assets/skirmish.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include "engine/ui/campaign.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {
std::vector<CustomMap> custom_maps(const std::string& maps_dir) {
  std::vector<CustomMap> result;
  std::error_code ec;
  std::filesystem::directory_iterator it(maps_dir, ec), end;
  while (!ec && it != end) {
    const auto path = it->path();
    std::string extension = path.extension().string();
    for (char& c : extension) c = char(std::tolower(static_cast<unsigned char>(c)));
    const auto spec = path.stem().string();
    MissionDef mission;
    // G: keep canonical campaign missions in the campaign browser.
    if (extension == ".mp" && !campaign_number(spec) && it->is_regular_file(ec) &&
        load_mission(path.string(), mission))
      result.push_back({spec, mission.title.empty() ? spec : mission.title, mission.w, mission.h});
    it.increment(ec);
  }
  // V: the reference sorts by displayed name. Filename breaks duplicate-title ties (G).
  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    return a.title == b.title ? a.spec < b.spec : a.title < b.title;
  });
  return result;
}

bool setup_skirmish(World& world, std::string& error) {
  error.clear();
  if (world.original_save || world.tick != 0 || world.mission.sides.empty() || world.mission.sides.size() > 8) {
    error = "Custom mission requires one to eight sides in a fresh world";
    return false;
  }
  int human = -1;
  unsigned seen = 0;
  for (const auto& side : world.mission.sides) {
    if (side.index < 0 || side.index >= 8 || (seen & (1u << side.index)) ||
        side.color < 0 || side.color > 7 || side.race < 0 || side.race > 2 || side.money < 0) {
      error = "Custom mission has an invalid side, colour, race or starting cash";
      return false;
    }
    seen |= 1u << side.index;
    if (human < 0 && !side.neutral && side.start_x >= 0 && side.start_y >= 0 &&
        side.start_x < world.terrain.w && side.start_y < world.terrain.h) human = side.index;
  }
  if (human < 0) { error = "Custom mission has no human side"; return false; }
  world.skirmish = true;
  world.script_side = world.local_side = human;
  for (auto& side : world.mission.sides) side.only_comp = side.index != human;
  return true;
}
}  // namespace aztec::engine
