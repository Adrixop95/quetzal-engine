#include "lockstep_fixture.hpp"
#include "engine/net/udp.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace lockstep_test;
using namespace aztec::engine::net;

int main(int argc, char** argv) {
  try {
    require(argc == 2 && (std::string(argv[1]) == "0" || std::string(argv[1]) == "1"), "Expected peer side 0 or 1");
    auto side = uint8_t(argv[1][0] - '0');
    Peer peer(side);
    UdpSocket socket;
    std::cout << "PORT " << socket.port() << std::endl;
    unsigned port = 0;
    require(bool(std::cin >> port) && port > 0 && port <= 65535, "Expected remote port");
    socket.set_peer("127.0.0.1", uint16_t(port));
    using Clock = std::chrono::steady_clock;
    auto start = Clock::now(), next_send = start;
    std::optional<Clock::time_point> finish;
    uint64_t sealed = UINT64_MAX;
    constexpr uint64_t goal = 24;
    while (Clock::now() - start < std::chrono::seconds(12)) {
      auto now = Clock::now();
      if (peer.link.tick() < goal && sealed != peer.link.tick()) {
        require(peer.link.submit(input(side, peer.link.tick())), "Submit peer input");
        sealed = peer.link.tick();
      }
      for (unsigned i = 0; i < 64; ++i) {
        auto bytes = socket.receive();
        if (!bytes) break;
        require(peer.link.receive(*bytes), "Unexpected packet from configured peer");
      }
      require(peer.link.error().empty(), "Peer desynchronized");
      // Rational deadlines avoid rounding 1/12 second to a whole millisecond.
      auto deadline = start + std::chrono::nanoseconds((peer.link.tick() + 1) * 1000000000ULL / kTickRate);
      if (peer.link.tick() < goal && now >= deadline) peer.link.step(peer.sim, peer.w);
      if (now >= next_send) {
        for (const auto& bytes : peer.link.packets()) socket.send(bytes);
        next_send = now + std::chrono::milliseconds(20);
      }
      if (peer.link.tick() == goal && peer.link.synchronized()) {
        if (!finish) finish = now;
        // Keep servicing duplicate final hashes/ACKs before shutting down.
        if (now - *finish > std::chrono::milliseconds(500)) break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(peer.link.tick() == goal && peer.link.synchronized(), "Two-process lockstep timed out");
    World control = world(); GameSim sim(rules()); sim.init(control);
    for (uint64_t tick = 0; tick < goal; ++tick) {
      if (tick >= 2) for (uint8_t player = 0; player < 2; ++player)
        for (const auto& c : input(player, tick - 2)) sim.submit(c);
      control.begin_tick(); sim.tick(control);
    }
    require(sim.state_hash(control) == peer.sim.state_hash(peer.w), "UDP result differs from local control");
    require(serialize_log(sim.log()) == serialize_log(peer.sim.log()), "UDP command order differs from control");
    std::cout << "RESULT " << peer.link.tick() << ' ' << peer.sim.state_hash(peer.w)
              << ' ' << peer.sim.log().size() << std::endl;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
