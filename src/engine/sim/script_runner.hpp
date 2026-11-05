// Executes a mission trigger script (Maps/<name>.scn, parsed by core/script.*) inside the simulation.
// Semantics that the original does not document are marked in docs/engine_rules_status.md:
//   * every trigger is evaluated each tick; all conditions true => actions run; triggers disable themselves with trigger(N).disable
//   * triggers that some other trigger enables are initially disabled
//   * localTime = simulation ticks since mission start; timers count up from 0 (running) unless set / stopped / killed
#pragma once
#include <map>
#include <optional>
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

// Diagnostic provenance only; reconstructed by replay and excluded from gameplay hashes.
struct ScriptOutcomeOrigin {
  int trigger = 0, action_line = 0;
  uint64_t tick = 0;
};

// Bounded diagnostic history per source trigger, reconstructed by replay. Not gameplay state.
struct ScriptTriggerTrace {
  uint64_t executions = 0, first_tick = 0, last_tick = 0;
};

struct ScriptActionObservation {
  size_t trigger_index = 0, action_index = 0;
  int trigger = 0, line = 0;
  script::ActKind kind = script::ActKind::TriggerEnable;
  bool enabled = false;
  ScriptTriggerTrace trace;
  std::vector<int> false_condition_lines;
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
  const std::optional<ScriptOutcomeOrigin>& outcome_origin() const { return outcome_origin_; }
  bool trigger_enabled(int id) const;
  // Terminal conditions/enablement are a snapshot, not an explanation of historical nonexecution.
  std::vector<ScriptActionObservation> action_observations(const World& w) const;
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
  std::optional<ScriptOutcomeOrigin> outcome_origin_;
  std::vector<ScriptTriggerTrace> trigger_traces_;
};

bool is_town_building(int type);

}  // namespace aztec::engine
