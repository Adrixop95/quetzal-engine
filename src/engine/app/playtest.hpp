// Headless gameplay verification: --script FILE feeds synthetic input to the engine, one frame = one simulation tick
// (virtual time, deterministic), takes screenshots and checks expectations.
//
// Script lines (case-insensitive, '#' starts a comment):
//   wait N                 N frames (each advances the simulation by one tick)
//   run N                  N simulation ticks without rendering
//   move X Y               mouse to window pixel (X, Y)
//   click X Y | rclick X Y | mclick X Y      move + press + release (left / right / middle)
//   down | up | rdown | rup                  hold / release the left / right button
//   drag X0 Y0 X1 Y1       left-button drag (box selection)
//   key NAME               key press, SDL key names ("H", "A", "Space", "1", "Escape"), prefix ctrl+ / shift+
//   shot FILE.png          screenshot of the next rendered frame
//   print                  one status line: tick, state hash, money, outcome, object counts
//   expect NAME OP VALUE   NAME = money | tick | outcome (0 running, 1 victory, 2 defeat) | selection | units SIDE | towns SIDE | enemies;
//                          OP = == != < <= > >=. A failed expectation makes the process exit with status 3.
//   quit
#pragma once
#include <string>
#include <vector>
#include "engine/platform/input.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

struct PlaytestRequest {
  std::string shot;     // take a screenshot after this frame
  int run_ticks = 0;    // run these ticks immediately (no frames)
  bool quit = false;
};

class PlaytestScript {
 public:
  bool load(const std::string& path, std::string& err);
  // Applies the next frame of the script to `in` (call after Input::begin_frame()). Returns false when the script is finished.
  bool step(Input& in, const World& w, const GameSim* sim, PlaytestRequest& req);
  int failures() const { return failures_; }

 private:
  struct Line { std::string cmd; std::vector<std::string> args; int src_line = 0; };
  void press_key(Input& in, const std::string& name, bool down);
  bool evaluate(const World& w, const Line& l);
  std::vector<Line> lines_;
  size_t pc_ = 0;
  int wait_ = 0;
  // pending releases for the frame after a click / key
  bool release_left_ = false, release_right_ = false, release_middle_ = false;
  std::string release_key_;
  int drag_phase_ = 0, dx0_ = 0, dy0_ = 0, dx1_ = 0, dy1_ = 0;
  int failures_ = 0;
};

}  // namespace aztec::engine
