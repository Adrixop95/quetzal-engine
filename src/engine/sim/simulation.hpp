// Simulation interface. The real game logic (docs/REWRITE.md "sim") will implement this.
// The engine calls World::begin_tick() (prev := cur) and then Simulation::tick() at a fixed 12 Hz,
// independent of the frame rate. The simulation must only write cur_x/cur_y (not prev_*) so rendering
// can interpolate. It must be deterministic: no wall-clock time, no frame-rate dependence.
#pragma once
#include <cstdint>
#include "engine/world/world.hpp"

namespace aztec::engine {

class Simulation {
 public:
  virtual ~Simulation() = default;
  virtual void init(World& /*w*/) {}
  virtual void tick(World& w) = 0;
};

// No game logic: advances only the tick counter and animation clocks.
class NullSimulation : public Simulation {
 public:
  void tick(World& w) override {
    w.tick++;
    for (auto& o : w.objects) ++o.anim_tick;
  }
};

// Test driver for interpolation: moves unit-typed objects on small deterministic random walks.
class WanderSimulation : public Simulation {
 public:
  explicit WanderSimulation(uint32_t seed = 1) : rng_(seed) {}
  void tick(World& w) override;

 private:
  uint32_t next() { rng_ = rng_ * 1664525u + 1013904223u; return rng_ >> 8; }
  uint32_t rng_;
};

}  // namespace aztec::engine
