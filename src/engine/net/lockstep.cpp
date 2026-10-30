#include "engine/net/lockstep.hpp"
#include "engine/sim/game_sim.hpp"
#include <bit>
#include <limits>
#include <stdexcept>

namespace aztec::engine::net {
namespace {
constexpr uint8_t kFrame = 1, kHash = 2, kAckFrame = 3, kAckHash = 4;
void put(Bytes& out, uint64_t value, unsigned count) {
  for (unsigned i = 0; i < count; ++i) out.push_back(uint8_t(value >> (8 * i)));
}
struct Reader {
  std::span<const uint8_t> bytes;
  size_t pos = 0;
  uint64_t get(unsigned count) {
    uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i) value |= uint64_t(bytes[pos++]) << (8 * i);
    return value;
  }
  bool has(size_t count) const { return count <= bytes.size() - pos; }
};
bool known(Pct type) {
  auto n = uint8_t(type);
  return (n >= uint8_t(Pct::PauseOn) && n <= uint8_t(Pct::Nothing)) ||
         type == Pct::Stop || type == Pct::AttackMove || type == Pct::PlaceBuilding || type == Pct::SetModifications;
}
std::optional<Bytes> encode(const std::vector<Command>& commands, uint8_t side) {
  if (commands.size() > 32) return {};
  Bytes out;
  put(out, commands.size(), 2);
  for (const auto& c : commands) {
    if (c.side != side || !known(c.type) || c.ids.size() > 128) return {};
    put(out, uint8_t(c.type), 1);
    put(out, uint32_t(c.a), 4); put(out, uint32_t(c.b), 4); put(out, uint32_t(c.c), 4);
    put(out, c.ids.size(), 2);
    for (auto id : c.ids) put(out, id, 4);
    if (out.size() + 24 > kMaxDatagram) return {};
  }
  return out;
}
bool decode(std::span<const uint8_t> bytes, uint8_t side, std::vector<Command>& commands) {
  Reader r{bytes};
  if (!r.has(2)) return false;
  auto count = r.get(2);
  if (count > 32) return false;
  for (unsigned i = 0; i < count; ++i) {
    if (!r.has(15)) return false;
    Command c;
    c.side = side; c.type = Pct(r.get(1));
    if (!known(c.type)) return false;
    c.a = std::bit_cast<int32_t>(uint32_t(r.get(4)));
    c.b = std::bit_cast<int32_t>(uint32_t(r.get(4)));
    c.c = std::bit_cast<int32_t>(uint32_t(r.get(4)));
    auto ids = r.get(2);
    if (ids > 128 || !r.has(ids * 4)) return false;
    for (unsigned j = 0; j < ids; ++j) c.ids.push_back(uint32_t(r.get(4)));
    commands.push_back(std::move(c));
  }
  return r.pos == bytes.size();
}
}  // namespace

Lockstep::Lockstep(Config config) : config_(config), hash_(config.initial_hash) {
  if (!config.session || config.side > 1 || config.delay > 8)
    throw std::invalid_argument("Invalid lockstep configuration");
  for (uint64_t t = 0; t < config.delay; ++t) add_frame(t, {}, Bytes{0, 0});
  Bytes value; put(value, hash_, 8);
  outgoing_[{kHash, 0}] = packet(kHash, 0, value);
}

Bytes Lockstep::packet(uint8_t kind, uint64_t tick, const Bytes& payload) const {
  Bytes out{'Q', 'L', 'S', '1', 1, kind, config_.side, config_.delay};
  put(out, config_.session, 8); put(out, tick, 8);
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}
void Lockstep::add_frame(uint64_t tick, const std::vector<Command>& commands, Bytes payload) {
  local_[tick] = commands;
  outgoing_[{kFrame, tick}] = packet(kFrame, tick, payload);
}
bool Lockstep::submit(const std::vector<Command>& commands) {
  if (sealed_ || !error_.empty() || tick_ >= std::numeric_limits<uint64_t>::max() - 9) return false;
  auto payload = encode(commands, config_.side);
  if (!payload) return false;
  add_frame(tick_ + config_.delay, commands, std::move(*payload));
  sealed_ = true;
  return true;
}
void Lockstep::compare() {
  auto it = hashes_.find(tick_);
  if (it != hashes_.end() && it->second != hash_) {
    desync_ = Desync{tick_, hash_, it->second};
    error_ = "State hash mismatch";
  }
}
bool Lockstep::receive(std::span<const uint8_t> bytes) {
  if (bytes.size() < 24 || bytes.size() > kMaxDatagram) return false;
  Reader r{bytes};
  if (r.get(4) != 0x31534c51 || r.get(1) != 1) return false;
  auto kind = uint8_t(r.get(1)), side = uint8_t(r.get(1)), delay = uint8_t(r.get(1));
  if (side != (config_.side ^ 1) || delay != config_.delay || r.get(8) != config_.session) return false;
  auto t = r.get(8);
  if (t > tick_ && t - tick_ > uint64_t(config_.delay) + 1) return false;
  if (kind == kAckFrame || kind == kAckHash) {
    if (bytes.size() != 24) return false;
    outgoing_.erase({uint8_t(kind - 2), t});
    return true;
  }
  if (kind != kFrame && kind != kHash) return false;
  std::vector<Command> commands;
  uint64_t value = 0;
  if (kind == kFrame) {
    if (!decode(bytes.subspan(24), side, commands)) return false;
    if (t < config_.delay && !commands.empty()) return false;
  } else {
    if (bytes.size() != 32 || (t > tick_ && t - tick_ > 1)) return false;
    value = r.get(8);
  }
  // Bounded ACK queue; dropped ACKs are recovered by retransmission.
  if (acks_.size() < 32) acks_[{kind, t}] = packet(uint8_t(kind + 2), t, {});
  if (t < tick_) return true;
  if (kind == kFrame) {
    Bytes payload(bytes.begin() + 24, bytes.end());
    auto found = remote_bytes_.find(t);
    if (found != remote_bytes_.end() && found->second != payload) error_ = "Conflicting input frame";
    else { remote_[t] = std::move(commands); remote_bytes_[t] = std::move(payload); }
  } else {
    auto found = hashes_.find(t);
    if (found != hashes_.end() && found->second != value) error_ = "Conflicting state hash";
    else hashes_[t] = value;
    // A boundary hash implicitly acknowledges all earlier frames and hashes.
    for (auto it = outgoing_.begin(); it != outgoing_.end();) {
      if (it->first.second < t) it = outgoing_.erase(it); else ++it;
    }
    compare();
  }
  return true;
}
std::vector<Bytes> Lockstep::packets() {
  std::vector<Bytes> out;
  for (const auto& [key, bytes] : acks_) out.push_back(bytes);
  acks_.clear();
  for (const auto& [key, bytes] : outgoing_) out.push_back(bytes);
  return out;
}
bool Lockstep::synchronized() const {
  auto it = hashes_.find(tick_);
  return error_.empty() && it != hashes_.end() && it->second == hash_;
}
bool Lockstep::ready() const {
  return sealed_ && synchronized() && local_.contains(tick_) && remote_.contains(tick_);
}
bool Lockstep::step(GameSim& sim, World& world) {
  if (!ready()) return false;
  const auto& first = config_.side == 0 ? local_.at(tick_) : remote_.at(tick_);
  const auto& second = config_.side == 1 ? local_.at(tick_) : remote_.at(tick_);
  for (const auto& c : first) sim.submit(c);
  for (const auto& c : second) sim.submit(c);
  world.begin_tick(); sim.tick(world);
  local_.erase(tick_); remote_.erase(tick_); remote_bytes_.erase(tick_); hashes_.erase(tick_);
  ++tick_; sealed_ = false; hash_ = sim.state_hash(world);
  Bytes value; put(value, hash_, 8);
  outgoing_[{kHash, tick_}] = packet(kHash, tick_, value);
  compare();
  return true;
}

}  // namespace aztec::engine::net
