// Forced world-state experiments. Scripts and their condition evaluators stay unchanged.
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/sim/game_sim.hpp"
#include "engine/world/type_info.hpp"

using namespace aztec::engine;
using namespace aztec::script;

namespace {
struct Event {
  uint64_t tick = 0;
  std::string operation;
  std::vector<int> args;
};

std::vector<Event> read_events(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open events");
  std::vector<Event> events;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    Event event;
    if (!(row >> event.tick >> event.operation) || !event.tick)
      throw std::runtime_error("invalid event header");
    int value;
    while (row >> value) event.args.push_back(value);
    if (!row.eof()) throw std::runtime_error("invalid event argument");
    const auto& op = event.operation;
    size_t count = op == "kill-side" || op == "kill-halls" ? 1 :
                   op == "kill-type" || op == "kill-tactic" || op == "transfer-town" || op == "idle-tactic" ? 2 :
                   op == "move-units" || op == "hit-tactic" ? 3 :
                   op == "move-type" ? 4 : 0;
    if (!count || event.args.size() != count) throw std::runtime_error("invalid event operation or arity");
    if (!events.empty() && event.tick < events.back().tick) throw std::runtime_error("events out of order");
    events.push_back(std::move(event));
  }
  return events;
}

void force(const Event& event, GameSim& sim, World& world) {
  const auto& a = event.args;
  if (event.operation == "transfer-town") {
    if (a[0] < 0 || a[0] >= int(world.mission.towns.size()) || a[1] < 0 || a[1] >= 8)
      throw std::runtime_error("invalid town transfer");
    sim.transfer_town(world, a[0], a[1]);
    return;
  }
  if (a[0] < 0 || a[0] >= 8) throw std::runtime_error("invalid event side");
  std::vector<uint32_t> matches;
  for (const auto& object : world.objects) {
    if (!object.alive || object.side != a[0]) continue;
    if (event.operation == "kill-halls" && !object.town_hall) continue;
    if ((event.operation == "kill-type" || event.operation == "move-type") && object.type != a[1]) continue;
    if ((event.operation == "kill-tactic" || event.operation == "idle-tactic" || event.operation == "hit-tactic") && object.tactic != a[1]) continue;
    if (event.operation == "move-units" && (object.removed || !type_info(object.type) ||
        type_info(object.type)->is_structure() || is_transport(object.type))) continue;
    matches.push_back(object.id);
  }
  if (matches.empty()) throw std::runtime_error("event matched no living objects");
  if (event.operation == "hit-tactic") {
    if (a[2] <= 0 || !world.find(uint32_t(a[2]))) throw std::runtime_error("invalid forced attacker");
    for (auto id : matches) world.find_mut(id)->last_hit_by = uint32_t(a[2]);
  } else if (event.operation == "idle-tactic") {
    sim.script_stop(world, matches);
  } else if (event.operation == "move-type" || event.operation == "move-units") {
    const size_t coordinate = event.operation == "move-type" ? 2 : 1;
    const int x = a[coordinate], y = a[coordinate + 1];
    if (x < 0 || y < 0 || x >= world.terrain.w || y >= world.terrain.h)
      throw std::runtime_error("forced position outside map");
    sim.script_stop(world, matches);
    for (auto id : matches) {
      auto* object = world.find_mut(id);
      if (object->removed || object->carrier) throw std::runtime_error("cannot move hidden object");
      sim.teleport(world, id, x * kTilePx + kTilePx / 2, y * kTilePx + kTilePx / 2);
    }
  } else for (auto id : matches) sim.script_kill(world, id);
}
}  // namespace

int main(int argc, char** argv) {
  try {
    std::string game, rules_dir, mission, plan, expected = "victory", mode = "simulation";
    uint64_t limit = 1200;
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (i + 1 >= argc) throw std::runtime_error("missing option value");
      const std::string value = argv[++i];
      if (option == "--game") game = value;
      else if (option == "--rules-dir") rules_dir = value;
      else if (option == "--mission") mission = value;
      else if (option == "--events") plan = value;
      else if (option == "--expect") expected = value;
      else if (option == "--mode") mode = value;
      else if (option == "--max-ticks") limit = std::stoull(value);
      else throw std::runtime_error("unknown option");
    }
    if (game.empty() || rules_dir.empty() || mission.empty() || plan.empty() || !limit || limit > 100000 ||
        (expected != "victory" && expected != "defeat" && expected != "timeout") ||
        (mode != "simulation" && mode != "scripts"))
      throw std::runtime_error("required: --game DIR --rules-dir DIR --mission ID --events FILE [--expect victory|defeat|timeout] [--mode simulation|scripts] [--max-ticks N]");
    const auto events = read_events(plan);
    GameData data;
    World world;
    std::string error;
    if (!data.open(game, error) || !load_world(data, mission, world, error)) throw std::runtime_error(error);
    auto rules = std::make_shared<RulesData>();
    if (!rules->load_dir(rules_dir)) throw std::runtime_error("cannot load rules");
    rules->load_sight(data.root());
    GameSim sim(rules, Modifications::faithful());
    const auto path = data.map_file(world.mission.file_name, "scn");
    Script script;
    if (!load_script(path, script) || !sim.attach_script(path)) throw std::runtime_error("cannot load script");
    sim.init(world);
    ScriptRunner isolated;
    isolated.set(script);
    isolated.init(world);
    const ScriptRunner& runner = mode == "scripts" ? isolated : sim.script();
    size_t unknown = 0, unsupported = 0;
    for (const auto& o : world.objects)
      if (o.type < 0 && o.source != ObjectSource::Submarine) ++unknown;
    for (const auto& t : script.triggers) for (const auto& a : t.actions)
      if (!ScriptRunner::supports(a.kind)) ++unsupported;
    size_t next = 0;
    uint64_t trace = 1469598103934665603ull;
    while (world.tick < limit && world.outcome == kRunning) {
      while (next < events.size() && events[next].tick == world.tick + 1) force(events[next++], sim, world);
      world.begin_tick();
      if (mode == "scripts") { ++world.tick; isolated.tick(sim, world); }
      else sim.tick(world);
      trace = (trace ^ sim.state_hash(world) ^ runner.state_hash()) * 1099511628211ull;
    }
    const std::string status = world.outcome == kVictory ? "victory" : world.outcome == kDefeat ? "defeat" : "timeout";
    size_t observed = 0;
    for (const auto& a : runner.action_observations(world)) if (a.trace.executions) ++observed;
    std::cout << "method\tforced-world-state\nmode\t" << mode << "\nstatus\t" << status << "\nticks\t" << world.tick
              << "\napplied_events\t" << next << "\nunapplied_events\t" << events.size() - next
              << "\nunknown_placed_types\t" << unknown << "\nunsupported_sites\t" << unsupported
              << "\nunsupported_executions\t" << runner.unsupported() << "\nobserved_action_sites\t" << observed
              << "\ntrace_hash\t" << std::hex << trace << "\nstate_hash\t" << sim.state_hash(world) << std::dec << '\n';
    const auto& origin = runner.outcome_origin();
    if (origin) std::cout << "outcome_trigger\t" << origin->trigger << "\noutcome_line\t" << origin->action_line << '\n';
    for (const auto& a : runner.action_observations(world))
      std::cout << "action\t" << a.trigger_index << '\t' << a.action_index << '\t' << action_name(a.kind)
                << '\t' << a.trace.executions << '\n';
    return status == expected && origin.has_value() == (expected != "timeout") &&
                   !unknown && !unsupported && !runner.unsupported() && next == events.size() ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "campaign probe: " << error.what() << '\n';
    return 2;
  }
}
