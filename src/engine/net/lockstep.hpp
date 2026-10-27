#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "engine/sim/commands.hpp"

namespace aztec::engine {
class GameSim;
struct World;
namespace net {

using Bytes = std::vector<uint8_t>;
constexpr unsigned kTickRate = 12;
constexpr size_t kMaxDatagram = 1200;

struct Config {
  uint64_t session = 0;  // Agreed out of band; must be nonzero.
  uint8_t side = 0;      // This version supports exactly sides 0 and 1.
  uint8_t delay = 2;     // 0..8 simulation ticks.
  uint64_t initial_hash = 0;
};
struct Desync { uint64_t tick, local, remote; };

// Transport-independent two-peer lockstep. Call submit once per tick, including
// empty input. Exchange packets even while stalled. Only step may drive GameSim.
class Lockstep {
 public:
  explicit Lockstep(Config config);
  bool submit(const std::vector<Command>& commands);
  // False means an invalid/foreign/out-of-window packet; it has no effect.
  bool receive(std::span<const uint8_t> packet);
  // Returns ACKs and every unacknowledged message. Call about every 20 ms;
  // repeat until delivered (also after the last simulation tick).
  std::vector<Bytes> packets();
  bool ready() const;
  bool step(GameSim& sim, World& world);
  uint64_t tick() const { return tick_; }
  bool synchronized() const;
  const std::optional<Desync>& desync() const { return desync_; }
  const std::string& error() const { return error_; }

 private:
  using Key = std::pair<uint8_t, uint64_t>;
  Bytes packet(uint8_t kind, uint64_t tick, const Bytes& payload) const;
  void add_frame(uint64_t tick, const std::vector<Command>& commands, Bytes payload);
  void compare();
  Config config_;
  uint64_t tick_ = 0, hash_ = 0;
  bool sealed_ = false;
  std::map<uint64_t, std::vector<Command>> local_, remote_;
  std::map<uint64_t, Bytes> remote_bytes_;
  std::map<uint64_t, uint64_t> hashes_;
  std::map<Key, Bytes> outgoing_, acks_;
  std::optional<Desync> desync_;
  std::string error_;
};

}  // namespace net
}  // namespace aztec::engine
