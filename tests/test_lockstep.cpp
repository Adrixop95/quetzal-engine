#include "lockstep_fixture.hpp"
#include <algorithm>
#include <iostream>
#include <limits>

using namespace lockstep_test;
using namespace aztec::engine::net;

static void loopback(uint8_t delay) {
  Peer a(0, delay), b(1, delay);
  Peer* peers[] = {&a, &b};
  struct Delivery { unsigned due, side; Bytes bytes; };
  std::vector<Delivery> queue;
  uint64_t sealed[] = {UINT64_MAX, UINT64_MAX};
  unsigned serial = 0;
  constexpr uint64_t goal = 80;
  for (unsigned pump = 0; pump < 5000; ++pump) {
    for (unsigned side = 0; side < 2; ++side) {
      auto& p = *peers[side];
      if (p.link.tick() < goal && sealed[side] != p.link.tick()) {
        require(p.link.submit(input(uint8_t(side), p.link.tick())), "Seal input");
        require(!p.link.submit({}), "Cannot replace sealed input");
        sealed[side] = p.link.tick();
      }
      for (auto bytes : p.link.packets()) {
        ++serial;
        if (serial % 7 == 0) continue;
        queue.push_back({pump + serial % 5, side ^ 1, bytes});
        if (serial % 3 == 0) queue.push_back({pump + 6, side ^ 1, bytes});
      }
    }
    std::reverse(queue.begin(), queue.end());
    for (auto it = queue.begin(); it != queue.end();) {
      if (it->due > pump) { ++it; continue; }
      require(peers[it->side]->link.receive(it->bytes), "Accept valid reordered packet");
      it = queue.erase(it);
    }
    for (auto* p : peers) {
      require(p->link.error().empty(), "No desync in identical simulations");
      if (p->link.tick() < goal) p->link.step(p->sim, p->w);
    }
    if (a.link.tick() == goal && b.link.tick() == goal &&
        a.link.synchronized() && b.link.synchronized()) break;
  }
  require(a.link.tick() == goal && b.link.tick() == goal, "Loss recovery completes");
  require(a.link.synchronized() && b.link.synchronized(), "Final hashes exchanged");
  require(a.sim.state_hash(a.w) == b.sim.state_hash(b.w), "Peer hashes agree");
  require(a.w.objects[0].cur_x != to_fixed(200), "Commands move a real simulation unit");
  World control = world(); GameSim sim(rules()); sim.init(control);
  for (uint64_t tick = 0; tick < goal; ++tick) {
    if (tick >= delay) for (uint8_t side = 0; side < 2; ++side)
      for (const auto& c : input(side, tick - delay)) sim.submit(c);
    control.begin_tick(); sim.tick(control);
  }
  require(sim.state_hash(control) == a.sim.state_hash(a.w), "Same result as ordered local simulation");
  require(serialize_log(sim.log()) == serialize_log(a.sim.log()), "Command order and delay match");
  require(a.sim.log().front().tick == uint64_t(delay) + 1, "Input applies after configured delay");
}

static void validation() {
  Peer a(0, 0), b(1, 0);
  require(a.link.submit(input(0, 0)), "Submit test frame");
  Bytes frame;
  for (const auto& p : a.link.packets()) if (p[5] == 1) frame = p;
  for (size_t length = 0; length < frame.size(); ++length)
    require(!b.link.receive(std::span(frame).first(length)), "Reject truncated frame");
  for (size_t offset : {size_t(0), size_t(4), size_t(5), size_t(6), size_t(7), size_t(8), size_t(16), size_t(26)}) {
    auto bad = frame; bad[offset] = 255;
    require(!b.link.receive(bad), "Reject invalid header or command");
  }
  auto bad = frame; bad.push_back(0);
  require(!b.link.receive(bad), "Reject trailing bytes");
  bad.resize(kMaxDatagram + 1);
  require(!b.link.receive(bad), "Reject oversized packet");
  bad = frame; bad[24] = 33;
  require(!b.link.receive(bad), "Reject excessive remote command count");
  bad = frame; bad[39] = 129;
  require(!b.link.receive(bad), "Reject excessive remote object count");
  require(!a.link.receive(frame), "Reject own-side packet");
  require(b.link.receive(frame), "Accept original after malformed input");
  require(b.link.receive(frame), "Accept exact duplicate");
  auto conflicting = frame; conflicting[27] ^= 1;
  require(b.link.receive(conflicting) && !b.link.error().empty(), "Conflicting input stops peer");
  require(!b.link.ready(), "Conflict prevents advance");

  Peer c(0);
  Command wrong; wrong.side = 1;
  require(!c.link.submit({wrong}), "Reject commands for another side");
  wrong.side = 0; wrong.type = Pct(255);
  require(!c.link.submit({wrong}), "Reject unknown local command");
  wrong.type = Pct::Move; wrong.ids.resize(129);
  require(!c.link.submit({wrong}), "Reject too many object ids");
  require(!c.link.submit(std::vector<Command>(33)), "Reject too many commands");
  wrong.ids.resize(128);
  require(!c.link.submit({wrong, wrong, wrong}), "Reject oversized local frame");
  require(c.link.submit({}), "Rejected input does not seal tick");
  require(!c.link.step(c.sim, c.w) && c.link.tick() == 0, "Missing peer stalls without advancing");
  bool threw = false;
  try { Lockstep invalid({0, 0, 2, 0}); } catch (const std::invalid_argument&) { threw = true; }
  require(threw, "Reject invalid session configuration");
}

static void wire_values() {
  Peer a(0, 0), b(1, 0);
  Command c;
  c.a = std::numeric_limits<int32_t>::min();
  c.b = std::numeric_limits<int32_t>::max(); c.c = -123;
  c.ids = {0, UINT32_MAX};  // Nothing commands carry data without interpreting IDs.
  require(a.link.submit({c}) && b.link.submit({}), "Seal signed wire values");
  for (const auto& p : a.link.packets()) b.link.receive(p);
  for (const auto& p : b.link.packets()) a.link.receive(p);
  require(a.link.step(a.sim, a.w) && b.link.step(b.sim, b.w), "Apply decoded wire values");
  require(serialize_log(a.sim.log()) == serialize_log(b.sim.log()), "Preserve integer bit patterns and IDs");
}

static void desync() {
  Peer a(0), b(1);
  require(a.link.submit({}) && b.link.submit({}), "Seal desync test");
  for (const auto& p : a.link.packets()) b.link.receive(p);
  for (const auto& p : b.link.packets()) a.link.receive(p);
  b.w.side_state[0].money += 1;
  require(a.link.step(a.sim, a.w), "First peer advances");
  for (const auto& p : a.link.packets()) b.link.receive(p);
  require(b.link.error().empty(), "Future hash waits for local boundary");
  require(b.link.step(b.sim, b.w), "Advance divergent state with future hash buffered");
  for (const auto& p : b.link.packets()) a.link.receive(p);
  for (auto* p : {&a, &b}) {
    require(p->link.desync().has_value(), "Report desync on both peers");
    require(p->link.desync()->tick == 1, "Report first divergent boundary");
    require(p->link.desync()->local != p->link.desync()->remote, "Preserve mismatching hashes");
    require(!p->link.step(p->sim, p->w), "Never advance past desync");
  }
  Lockstep x({7, 0, 0, 1}), y({7, 1, 0, 2});
  for (const auto& p : x.packets()) y.receive(p);
  require(y.desync() && y.desync()->tick == 0, "Check initial state before first tick");
}

int main() {
  try {
    validation(); wire_values(); desync();
    for (uint8_t delay : {0, 2, 8}) loopback(delay);
    std::cout << "Lockstep loopback, validation and desync checks passed\n";
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
