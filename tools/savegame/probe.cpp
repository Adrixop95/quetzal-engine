// Private import/continuation observer. Rendering and player input are never run.
#include "engine/save_import.hpp"
#include "engine/sim/game_sim.hpp"
#include <cstdio>
#include <fstream>
#include <charconv>

int main(int argc, char** argv) {
  std::string game, save, output, rules_dir;
  unsigned ticks = 0;
  bool continuation = false;
  for (int i = 1; i < argc; ++i) {
    std::string flag = argv[i];
    if (i + 1 >= argc) return 2;
    if (flag == "--game") game = argv[++i];
    else if (flag == "--save") save = argv[++i];
    else if (flag == "--output") output = argv[++i];
    else if (flag == "--rules") rules_dir = argv[++i];
    else if (flag == "--ticks") {
      std::string value = argv[++i];
      const auto parsed = std::from_chars(value.data(), value.data() + value.size(), ticks);
      if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || ticks > 3600) return 2;
      continuation = true;
    }
    else return 2;
  }
  if (game.empty() || save.empty() || output.empty() || rules_dir.empty()) {
    std::fprintf(stderr, "Usage: aztec_save_probe --game DIR --save FILE --rules DIR --output FILE [--ticks 0..3600]\n");
    return 2;
  }
  using namespace aztec::engine;
  GameData data; World world; std::string error;
  auto rules = std::make_shared<RulesData>();
  if (!data.open(game, error) || !load_original_save(data, save, world, error)) {
    std::fprintf(stderr, "%s\n", error.c_str()); return 1;
  }
  if (!rules->load_dir(rules_dir)) { std::fprintf(stderr, "Cannot load rules\n"); return 1; }
  rules->load_sight(data.root());
  GameSim sim(rules); sim.init(world);
  // Diagnostic continuation only: retained orders/scripts are not executable.
  for (unsigned i = 0; i < ticks; ++i) { world.begin_tick(); sim.tick(world); }
  std::ofstream out(output);
  if (!write_original_save_snapshot(world, out, error, continuation)) {
    std::fprintf(stderr, "%s\n", error.c_str()); return 1;
  }
  out.close();
  return out.fail() ? 1 : 0;
}
