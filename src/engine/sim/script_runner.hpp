// Executes a mission trigger script (Maps/<name>.scn, parsed by core/script.*) inside the simulation.
// Semantics that the original does not document are marked in docs/engine_rules_status.md:
//   * every trigger is evaluated each tick; all conditions true => actions run; triggers disable themselves with trigger(N).disable
//   * triggers that some other trigger enables are initially disabled
//   * localTime = simulation ticks since mission start; timers count up from 0 (running) unless set / stopped / killed
#pragma once
#include <map>
#include <string>
#include <vector>
#include "core/script.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

class GameSim;

struct TacticState {
  bool locked = false;
  int path = -1, path_index = 0;
  bool cycled = false, inverted = false;
};

class ScriptRunner {
 public:
  bool load(const std::string& path);
  void set(script::Script s);
  bool loaded() const { return !script_.triggers.empty(); }
  void init(const World& w);
  void tick(GameSim& sim, World& w);
  // Condition helpers are public for tests.
  bool eval(const World& w, const script::Condition& c) const;
  uint64_t state_hash() const;
  static bool supports(script::ActKind kind);
  size_t unsupported() const { return unsupported_; }
  // AI hook: advances units of tactics that follow a path.
  void follow_paths(GameSim& sim, World& w);

 private:
  struct Timer { bool running = true; int64_t value = 0; bool exists = true; };
  void run(GameSim& sim, World& w, const script::Action& a);
  std::vector<uint32_t> tactic_members(const World& w, int side, int tactic) const;
  int resolve_side(const World& w, int s) const { return s == script::kLocal ? w.script_side : s; }
  int count_units(const World& w, int side) const;
  int count_towns(const World& w, int side) const;

  script::Script script_;
  std::map<int, bool> enabled_;
  std::map<int, Timer> timers_;
  std::map<std::pair<int, int>, TacticState> tactics_;
  uint64_t local_time_ = 0;
  size_t unsupported_ = 0;
};

bool is_town_building(int type);

}  // namespace aztec::engine
