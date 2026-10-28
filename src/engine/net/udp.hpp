#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include "engine/net/lockstep.hpp"

namespace aztec::engine::net {

// Nonblocking POSIX IPv4 UDP socket. Numeric addresses only, one configured peer.
// Port zero requests an ephemeral local port. System failures throw runtime_error.
class UdpSocket {
 public:
  explicit UdpSocket(const std::string& bind_address = "127.0.0.1", uint16_t port = 0);
  ~UdpSocket();
  UdpSocket(const UdpSocket&) = delete;
  UdpSocket& operator=(const UdpSocket&) = delete;
  void set_peer(const std::string& address, uint16_t port);
  uint16_t port() const { return port_; }
  // False on temporary backpressure/interruption; retry at the next pump.
  bool send(std::span<const uint8_t> bytes);
  // Empty on no input, interruption, foreign source or oversized datagram.
  std::optional<Bytes> receive();

 private:
  int fd_ = -1;
  uint16_t port_ = 0, peer_port_ = 0;
  uint32_t peer_address_ = 0;
};

}  // namespace aztec::engine::net
