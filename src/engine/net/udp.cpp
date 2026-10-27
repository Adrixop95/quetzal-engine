#include "engine/net/udp.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace aztec::engine::net {
namespace {
sockaddr_in endpoint(const std::string& address, uint16_t port) {
  sockaddr_in out{};
  out.sin_family = AF_INET; out.sin_port = htons(port);
  if (inet_pton(AF_INET, address.c_str(), &out.sin_addr) != 1)
    throw std::invalid_argument("Expected a numeric IPv4 address");
  return out;
}
std::runtime_error failure(const char* operation) {
  return std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
}
bool temporary() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
}  // namespace

UdpSocket::UdpSocket(const std::string& address, uint16_t port) {
  auto local = endpoint(address, port);
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ < 0) throw failure("UDP socket");
  try {
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0 || fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0 ||
        fcntl(fd_, F_SETFD, FD_CLOEXEC) < 0) throw failure("UDP nonblocking setup");
    if (bind(fd_, reinterpret_cast<sockaddr*>(&local), sizeof local) < 0) throw failure("UDP bind");
    socklen_t length = sizeof local;
    if (getsockname(fd_, reinterpret_cast<sockaddr*>(&local), &length) < 0) throw failure("UDP local port");
    port_ = ntohs(local.sin_port);
  } catch (...) { close(fd_); throw; }
}
UdpSocket::~UdpSocket() { if (fd_ >= 0) close(fd_); }
void UdpSocket::set_peer(const std::string& address, uint16_t port) {
  if (!port) throw std::invalid_argument("Peer port must be nonzero");
  auto peer = endpoint(address, port);
  peer_port_ = peer.sin_port; peer_address_ = peer.sin_addr.s_addr;
}
bool UdpSocket::send(std::span<const uint8_t> bytes) {
  if (!peer_port_) throw std::logic_error("UDP peer is not configured");
  if (bytes.empty() || bytes.size() > kMaxDatagram) throw std::invalid_argument("Invalid UDP packet size");
  sockaddr_in peer{};
  peer.sin_family = AF_INET; peer.sin_port = peer_port_; peer.sin_addr.s_addr = peer_address_;
  auto count = sendto(fd_, bytes.data(), bytes.size(), 0, reinterpret_cast<sockaddr*>(&peer), sizeof peer);
  if (count < 0) { if (temporary()) return false; throw failure("UDP send"); }
  if (size_t(count) != bytes.size()) throw std::runtime_error("Incomplete UDP send");
  return true;
}
std::optional<Bytes> UdpSocket::receive() {
  Bytes bytes(kMaxDatagram + 1);
  sockaddr_in source{};
  socklen_t length = sizeof source;
  auto count = recvfrom(fd_, bytes.data(), bytes.size(), 0, reinterpret_cast<sockaddr*>(&source), &length);
  if (count < 0) { if (temporary()) return {}; throw failure("UDP receive"); }
  if (!peer_port_ || source.sin_family != AF_INET || source.sin_port != peer_port_ ||
      source.sin_addr.s_addr != peer_address_ || count > ssize_t(kMaxDatagram)) return {};
  bytes.resize(size_t(count));
  return bytes;
}

}  // namespace aztec::engine::net
