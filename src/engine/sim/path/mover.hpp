// Deterministic group movement: shared flow fields + slot assignment + velocity-sampling local avoidance
// + position-based overlap resolution (units are discs with sub-tile 24.8 positions, walls are tiles).
// Keeps the original feel (per-type speed in px/tick, 12 Hz tick, 8-way facing) but replaces the original's
// tile-exclusive stepping, two-requester WaveSearch admission and 8-step path memory (docs/rules_movement.md).
#pragma once
#include <memory>
#include <vector>
#include "astar.hpp"
#include "flowfield.hpp"

namespace aztec::path {

enum class AgentState : u8 { Idle, Moving, Arrived };

struct MoverConfig {
  i32 avoid_horizon = 8;        // ticks looked ahead for collisions
  int overlap_iterations = 3;
  int wall_penalty = 4;         // extra field cost next to walls (10 = one straight step)
  i32 slot_gap = 4 * kFix;      // extra spacing between destination slots
  int stuck_window = 24;        // ticks
};

struct Agent {
  u32 id = 0;
  Vec2 pos, vel, prev_pos;      // 24.8 px
  i32 radius = 18 * kFix;
  i32 speed = 9 * kFix;         // fixed units per tick (original: px/tick * 256, e.g. footman 9)
  MoveClass cls = MoveClass::Small;
  AgentState state = AgentState::Idle;
  bool hold = false;            // immovable when idle (hold position)
  int order = -1;
  Vec2 slot;                    // destination point inside the order's goal area
  int facing = 0;               // 0 = N, clockwise, 8 directions (animation)
  int heading = 0;              // 256-step angle of last non-zero velocity
  // diagnostics / stuck handling
  Vec2 win_start; int win_ticks = 0; int stuck_level = 0; int squeeze_ticks = 0; int ghost_ticks = 0;
  u32 moved_ticks = 0;
  Vec2 target; u64 target_until = 0; int backoff_ticks = 0;   // cached steering target (recomputed every few ticks)
};

class MoveSystem {
 public:
  explicit MoveSystem(NavGrid& grid, MoverConfig cfg = {}) : grid_(grid), cfg_(cfg) {}
  u32 add_agent(Vec2 pos, i32 radius, i32 speed, MoveClass cls);   // returns index == id
  // Move the listed agents to `goal` (fixed px). They share one flow field and get distinct destination slots near the goal.
  void order_move(const u32* ids, int n, Vec2 goal);
  void stop(u32 id);
  void tick();
  const std::vector<Agent>& agents() const { return agents_; }
  std::vector<Agent>& agents_mut() { return agents_; }
  u64 state_hash() const;
  u64 tick_count() const { return tick_; }
  struct Stats { u64 fields_built = 0, stuck_events = 0, repaths = 0; } stats;

 private:
  struct Order { FlowField ff[2]; bool have[2] = {false, false}; Vec2 goal; Cell goal_cell; int refs = 0; };
  void ensure_field(Order& o, MoveClass c);
  Vec2 preferred_velocity(Agent& a, const Order& o);
  Vec2 choose_velocity(const Agent& a, Vec2 pref, const std::vector<u32>& nb) const;
  void build_hash();
  void neighbours(const Agent& a, std::vector<u32>& out, i32 reach) const;
  void resolve_overlaps();
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
