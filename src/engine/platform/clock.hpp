// Fixed-step simulation clock decoupled from the render loop.
#pragma once
#include <algorithm>

namespace aztec::engine {

constexpr int kSimHz = 12;
constexpr double kSimStep = 1.0 / kSimHz;

class FixedStepper {
 public:
  explicit FixedStepper(double step = kSimStep, int max_catchup = 5) : step_(step), max_(max_catchup) {}
  // Feed wall-clock seconds; returns how many simulation ticks to run now.
  int advance(double dt) {
    acc_ += std::min(dt, 0.25);
    int n = 0;
    while (acc_ >= step_ && n < max_) { acc_ -= step_; n++; }
    if (n == max_) acc_ = std::min(acc_, step_);  // drop backlog instead of spiralling
    return n;
  }
  // Interpolation factor in [0,1) between the previous and current simulation state.
  double alpha() const { return acc_ / step_; }
  void reset() { acc_ = 0; }

 private:
  double step_, acc_ = 0;
  int max_;
};

}  // namespace aztec::engine
