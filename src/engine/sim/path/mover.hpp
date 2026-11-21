// Deterministic movement with an opt-in original ground-wave candidate.
// Disc movement remains the game default until tactic integration passes its gates.
#pragma once
#include <memory>
#include <vector>
#include "astar.hpp"
#include "flowfield.hpp"
#include "wave.hpp"

namespace aztec::path {

enum class AgentState : u8 { Idle, Moving, Arrived };

struct MoverConfig {
  bool original_ground = false; // candidate: tactic coordination must pass integration gates before promotion
  i32 avoid_horizon = 8;        // ticks looked ahead for collisions
  int overlap_iterations = 3;
  int wall_penalty = 4;         // extra field cost next to walls (10 = one straight step)
  i32 slot_gap = 4 * kFix;      // extra spacing between destination slots
  bool unit_unstick = false;   // opt-in recovery; default path and hashes stay unchanged
  int stuck_window = 24;        // ticks
};

struct Agent {
  u32 id = 0;
  u32 team = 0;                 // equal team IDs may cooperate; never swap enemies
  int unstick_ticks = 0;
  i32 unstick_best = 0;
  std::vector<Vec2> local_path;
  Vec2 pos, vel, prev_pos;      // 24.8 px
  i32 radius = 18 * kFix;
  i32 speed = 9 * kFix;         // fixed units per tick (original: px/tick * 256, e.g. footman 9)
  MoveClass cls = MoveClass::Small;
  AgentState state = AgentState::Idle;
  bool enabled = true; // embarked agents retain stable indices but do not collide or move
  bool hold = false;            // immovable when idle (hold position)
  int order = -1;
  Vec2 slot;                    // destination point inside the order's goal area
  int facing = 0;               // 0 = N, clockwise, 8 directions (animation)
  int heading = 0;              // 256-step angle of last non-zero velocity
  // diagnostics / stuck handling
  Vec2 win_start; int win_ticks = 0; int stuck_level = 0; int squeeze_ticks = 0; int ghost_ticks = 0;
  u32 moved_ticks = 0;
  u64 path_failures = 0;       // Diagnostic only; excluded from state_hash: unreachable requests and failed local repaths.
  Vec2 target; u64 target_until = 0; int backoff_ticks = 0;   // cached steering target (recomputed every few ticks)
  int formation = -1;
  Vec2 step_goal;              // reserved tile centre during startup approach
  Vec2 step_from;
  // Original ground stepping; group destination allocation remains an engine policy.
  Cell reserved_tile{0, 0};
  bool stepping = false;
  bool vehicle = false;        // Truck / Headtruck; Spider arrival behavior remains unresolved
  bool about_to_attack = false;
  i32 vehicle_speed_px = 0;
  u32 retries = 0;
  std::vector<Cell> wave_path;  // at most eight remaining steps
};

class MoveSystem {
 public:
  explicit MoveSystem(NavGrid& grid, MoverConfig cfg = {}) : grid_(grid), cfg_(cfg) {}
  u32 add_agent(Vec2 pos, i32 radius, i32 speed, MoveClass cls, u32 team = 0);   // returns index == id
  // Move the listed agents to `goal` (fixed px). They share one flow field and get distinct destination slots near the goal.
  void order_move(const u32* ids, int n, Vec2 goal);
  // Startup settling uses tile centres and occupancy rather than destination slots.
  // This is a measured-data candidate, not a recovered original formation rule.
  void order_formation(const u32* ids, int n, Cell anchor);
  void block_formation_tile(Cell tile) { formation_obstacles_.push_back(tile); }
  void stop(u32 id);
  void tick();
  void set_unit_unstick(bool enabled);
  void set_fog(u32 team, std::span<const u8> brightness);
  const std::vector<Agent>& agents() const { return agents_; }
  std::vector<Agent>& agents_mut() { return agents_; }
  u64 state_hash() const;
  u64 tick_count() const { return tick_; }
  struct Stats { u64 fields_built = 0, stuck_events = 0, repaths = 0, swaps = 0, sidesteps = 0, relocations = 0; } stats;

 private:
  struct Order { FlowField ff[3]; bool have[3] = {false, false, false}; Vec2 goal; Cell goal_cell; int refs = 0; };
  void ensure_field(Order& o, MoveClass c);
  Vec2 preferred_velocity(Agent& a, const Order& o);
  Vec2 choose_velocity(const Agent& a, Vec2 pref, const std::vector<u32>& nb) const;
  void build_hash();
  void neighbours(const Agent& a, std::vector<u32>& out, i32 reach) const;
  void resolve_overlaps();
  void tick_formations();
  struct Formation { Cell anchor; FlowField fields[3]; };
  void build_formation_fields(Formation& f);
  std::vector<Formation> formations_;
  std::vector<Cell> formation_obstacles_;
  bool free_position(const Agent& a, Vec2 p, int ignore = -1) const;
  bool free_segment(const Agent& a, Vec2 p, Vec2 q) const;
  void local_repath(Agent& a);
  bool cooperate(Agent& a);
  void recover(Agent& a);
  bool original_agent(const Agent& a) const { return cfg_.original_ground && !cfg_.unit_unstick && a.cls != MoveClass::Water && a.formation < 0; }
  void order_wave(const u32* ids, int n, Vec2 goal);
  void tick_wave();
  WaveBudget wave_budget_;
  std::vector<std::vector<u8>> fog_;
  NavGrid& grid_;
  MoverConfig cfg_;
  std::vector<Agent> agents_;
  std::vector<std::unique_ptr<Order>> orders_;
  u64 tick_ = 0;
  // spatial hash
  int hw_ = 0, hh_ = 0;
  std::vector<u32> hstart_, hlist_;
  mutable std::vector<u32> scratch_;
};

}  // namespace aztec::path
