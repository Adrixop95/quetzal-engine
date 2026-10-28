#pragma once
#include "engine/net/lockstep.hpp"
#include "engine/sim/game_sim.hpp"
#include <stdexcept>

namespace lockstep_test {
using namespace aztec::engine;
inline void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
inline std::shared_ptr<RulesData> rules() {
  auto out = std::make_shared<RulesData>(); out->init_defaults(); return out;
}
inline World world() {
  World w;
  w.terrain.w = w.terrain.h = 32; w.terrain.cells.resize(32 * 32);
  for (int side = 0; side < 2; ++side) {
    Object unit; unit.id = uint32_t(side + 1); unit.type = 0; unit.side = side; unit.hp = 6000;
    unit.teleport(to_fixed(200 + side * 500), to_fixed(200)); w.objects.push_back(unit);
  }
  return w;
}
inline std::vector<Command> input(uint8_t side, uint64_t tick) {
  Command c; c.side = side; c.ids = {uint32_t(side + 1)};
  if (tick == 0) { c.type = Pct::Move; c.a = 400 + side * 500; c.b = 400; return {c}; }
  if (tick == 2) {
    c.type = Pct::CreateGroup; c.a = 3;
    Command other = c; other.type = Pct::AddToGroup;
    return {c, other};
  }
  if (side == 0 && (tick == 4 || tick == 5)) {
    c.type = tick == 4 ? Pct::PauseOn : Pct::PauseOff; return {c};
  }
  if (tick == 12) { c.type = Pct::Stop; return {c}; }
  return {};
}
struct Peer {
  World w = world();
  GameSim sim{rules()};
  net::Lockstep link;
  explicit Peer(uint8_t side, uint8_t delay = 2) : link(initialize(side, delay)) {}
 private:
  net::Config initialize(uint8_t side, uint8_t delay) {
    sim.init(w);
    return {0x123456789abcdef0ULL, side, delay, sim.state_hash(w)};
  }
};
}  // namespace lockstep_test
