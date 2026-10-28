// Read-only campaign inventory. Never emits mission text or executable-derived data.
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>

#include "core/script.hpp"
#include "engine/assets/game_data.hpp"
#include "engine/sim/script_runner.hpp"

using namespace aztec::script;
using namespace aztec::engine;

int main(int argc, char** argv) {
  std::string maps;
  bool require_supported = false, optional = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if ((arg == "--maps" || arg == "--game") && i + 1 < argc) {
      std::string path = argv[++i];
      if (arg == "--game") {
        if (const char* configured = std::getenv("GAME_DIR")) if (*configured) path = configured;
        maps = find_file_ci(path, "Maps");
      } else maps = path;
    } else if (arg == "--require-supported") require_supported = true;
    else if (arg == "--optional") optional = true;
    else {
      std::cerr << "Usage: aztec_script_audit (--maps DIR | --game DIR) [--require-supported] [--optional]\n";
      return 2;
    }
  }
  if (maps.empty()) {
    std::cerr << "Campaign maps absent; supply --maps DIR or --game DIR.\n";
    return optional ? 77 : 2;
  }
  size_t missing = 0, errors = 0, unsupported = 0;
  std::cout << "mission\tsection\tkind\tcount\tsupport\n";
  for (int mission = 1; mission <= 15; ++mission) {
    const std::string name = "mission_" + std::to_string(mission);
    const std::string path = find_file_ci(maps, name + ".scn");
    if (path.empty()) { std::cerr << name << ": missing script\n"; ++missing; continue; }
    Script script;
    if (!load_script(path, script)) {
      std::cerr << name << ": parse failure " << script.error << '\n'; ++errors; continue;
    }
    std::map<CondKind, size_t> conditions;
    std::map<ActKind, size_t> actions;
    std::set<int> ids;
    for (const auto& t : script.triggers) {
      if (!ids.insert(t.id).second)
        std::cerr << name << ": repeated trigger ID " << t.id << " (shared enablement)\n";
      for (const auto& c : t.conditions) ++conditions[c.kind];
      for (const auto& a : t.actions) {
        ++actions[a.kind];
        if (!ScriptRunner::supports(a.kind)) {
          ++unsupported;
          std::cerr << name << ": trigger " << t.id << " line " << a.line << ": unsupported " << action_name(a.kind) << '\n';
        }
        if ((a.kind == ActKind::TriggerEnable || a.kind == ActKind::TriggerDisable) && !script.find(a.id))
          std::cerr << name << ": trigger " << t.id << " line " << a.line << ": missing trigger target " << a.id << '\n';
      }
    }
    std::cout << name << "\ttriggers\tall\t" << script.triggers.size() << "\tengine policy\n";
    for (const auto& [kind, count] : conditions)
      std::cout << name << "\tcondition\t" << condition_name(kind) << '\t' << count << "\tevaluated\n";
    for (const auto& [kind, count] : actions)
      std::cout << name << "\taction\t" << action_name(kind) << '\t' << count << '\t'
                << (ScriptRunner::supports(kind) ? "implemented" : "unsupported") << '\n';
  }
  std::cerr << "Campaign inventory: missing=" << missing << " parse_errors=" << errors
            << " unsupported_action_sites=" << unsupported << '\n';
  // Inventory success establishes parser coverage only. The strict gate also requires action coverage.
  return missing || errors || (require_supported && unsupported) ? 1 : 0;
}
