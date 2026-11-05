// Private tick-zero import observer. No simulation tick or rendering is run.
#include "engine/save_import.hpp"
#include "engine/sim/game_sim.hpp"
#include <cstdio>
#include <fstream>

int main(int argc, char** argv) {
  std::string game, save, output, rules_dir;
  for (int i = 1; i < argc; ++i) {
    std::string flag = argv[i];
    if (i + 1 >= argc) return 2;
    if (flag == "--game") game = argv[++i];
    else if (flag == "--save") save = argv[++i];
    else if (flag == "--output") output = argv[++i];
    else if (flag == "--rules") rules_dir = argv[++i];
    else return 2;
  }
  if (game.empty() || save.empty() || output.empty() || rules_dir.empty()) {
    std::fprintf(stderr, "Usage: aztec_save_probe --game DIR --save FILE --rules DIR --output FILE\n");
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
  std::ofstream out(output);
  if (!write_original_save_snapshot(world, out, error)) {
    std::fprintf(stderr, "%s\n", error.c_str()); return 1;
  }
  out.close();
  return out.fail() ? 1 : 0;
}
