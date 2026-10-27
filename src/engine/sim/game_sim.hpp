// Deterministic game simulation: fixed 12 Hz, integer / fixed-point only, command driven.
// See docs/engine_rules_status.md for which rules are verified against the original and which are placeholders.
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "engine/sim/commands.hpp"
#include "engine/sim/path/mover.hpp"
#include "engine/sim/rules.hpp"
#include "engine/sim/script_runner.hpp"
#include "engine/sim/simulation.hpp"

namespace aztec::engine {

enum class Order : uint8_t { Idle = 0, Move = 1, Attack = 2, AttackMove = 3 };

// Extra animation state used for buildings under construction (anim_state values 0..5 are data::AnimState).
constexpr int kAnimConstruct = 6;

struct MenuItem {
  int type = 0;
  bool construct = false;     // place on the map (true) or train a unit (false)
  int price = 0, time = 0;
  bool enabled = false;       // prerequisites + money + not disabled
  const char* why_not = "";
};

class GameSim : public Simulation {
 public:
  explicit GameSim(std::shared_ptr<const RulesData> rules);
  void init(World& w) override;
  void tick(World& w) override;

  // Queues a command for the next tick (all clients submit the same commands in the same order in lockstep play).
  void submit(Command c) { pending_.push_back(std::move(c)); }
  bool paused() const { return paused_; }
  uint64_t tick_count() const { return tick_; }
  const std::vector<LoggedCommand>& log() const { return log_; }
  // FNV-1a hash of the complete simulation state (objects, sides, queues, movement system, rng).
  uint64_t state_hash(const World& w) const;

  // Queries for the UI.
  std::vector<MenuItem> menu(const World& w, int side) const;
  bool can_place(const World& w, int side, int type, int tx, int ty, const char** why = nullptr) const;
  const RulesData& rules() const { return *rules_; }
  // Footprint rectangle (tiles) of a structure type centred on tile (tx, ty).
  static void footprint(const TypeRules& r, int tx, int ty, int& x0, int& y0, int& x1, int& y1);

  // Mission script (.scn). Triggers run once per tick before orders.
  bool attach_script(const std::string& scn_path) { return script_.load(scn_path); }
  const ScriptRunner& script() const { return script_; }
  // Used by the script runner (tactic orders).
  void script_order(World& w, const std::vector<uint32_t>& ids, int px, int py, bool attack_move);
  void script_kill(World& w, uint32_t id);
  // Debug/editor helper (not a lockstep command): moves an object and its movement agent instantly.
  void teleport(World& w, uint32_t id, int px, int py);

  // Re-runs a logged game from a fresh world (snapshot restore = deterministic replay). The world must be freshly loaded.
  static std::unique_ptr<GameSim> replay(World& fresh, std::shared_ptr<const RulesData> rules,
                                         const std::vector<LoggedCommand>& log, uint64_t until_tick,
                                         const std::string& scn_path = "");

 private:
  void apply(World& w, const Command& c);
  void economy(World& w);
  void run_orders(World& w);
  void advance_effects(World& w);
  void fire(World& w, const Object& source, Object& target, int damage);
  void fire_weapon(World& w, const WeaponRules& weapon, const Object& source, Object& target, int damage);
  void decoration(World& w, int anim, int slot, Fixed x, Fixed y, int height, int side, uint32_t source = 0);
  void sync_movement(World& w);
  void update_fog(World& w);
  void run_ai(World& w);   // sim/ai.cpp
  void kill(World& w, Object& o);
  uint32_t spawn(World& w, int type, int side, int px, int py, int hp, bool constructing);
  void attach_agent(Object& o);
  bool menu_ok(const World& w, int side, const BuildItem& it, const char** why) const;
  const Object* nearest_enemy(const World& w, const Object& o, int radius_px) const;
  int town_index(const World& w, int side) const;
  uint32_t next_rand() { rng_ = rng_ * 1664525u + 1013904223u; return rng_ >> 8; }

  std::shared_ptr<const RulesData> rules_;
  path::NavGrid nav_;
  std::unique_ptr<path::MoveSystem> mover_;
  ScriptRunner script_;
  std::vector<Command> pending_;
  std::vector<LoggedCommand> log_;
  bool paused_ = false;
  uint32_t rng_ = 12345;
  uint32_t spawn_counter_ = 0;
  uint64_t tick_ = 0;
};

constexpr int kCorpseTicks = 480;   // placeholder: a corpse stays 40 s
constexpr int kIncomeEvery = 12;    // placeholder income period (ticks)

}  // namespace aztec::engine
