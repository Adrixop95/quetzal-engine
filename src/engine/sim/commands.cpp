#include "engine/sim/commands.hpp"

#include <cstring>
#include "core/loader_limits.hpp"

namespace aztec::engine {

static void put32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; i++) v.push_back(uint8_t(x >> (8 * i))); }
static bool get32(const std::vector<uint8_t>& v, size_t& o, uint32_t& x) {
  if (o + 4 > v.size()) return false;
  x = v[o] | v[o + 1] << 8 | v[o + 2] << 16 | uint32_t(v[o + 3]) << 24;
  o += 4;
  return true;
}

std::vector<uint8_t> serialize_log(const std::vector<LoggedCommand>& log) {
  std::vector<uint8_t> v{'A', 'Z', 'C', '1'};
  put32(v, uint32_t(log.size()));
  for (const LoggedCommand& l : log) {
    put32(v, uint32_t(l.tick)); put32(v, uint32_t(l.tick >> 32));
    put32(v, uint32_t(l.cmd.type) | uint32_t(l.cmd.side) << 8);
    put32(v, uint32_t(l.cmd.a)); put32(v, uint32_t(l.cmd.b)); put32(v, uint32_t(l.cmd.c));
    put32(v, uint32_t(l.cmd.ids.size()));
    for (uint32_t id : l.cmd.ids) put32(v, id);
  }
  return v;
}

bool deserialize_log(const std::vector<uint8_t>& v, std::vector<LoggedCommand>& out) {
  out.clear();
  std::vector<LoggedCommand> parsed;
  if (v.size() > detail::kFileBytes) return false;
  if (v.size() < 8 || std::memcmp(v.data(), "AZC1", 4) != 0) return false;
  size_t o = 4;
  uint32_t n;
  if (!get32(v, o, n)) return false;
  if (n > (v.size() - o) / 28) return false;
  for (uint32_t i = 0; i < n; i++) {
    LoggedCommand l;
    uint32_t lo, hi, ts, a, b, c, cnt;
    if (!get32(v, o, lo) || !get32(v, o, hi) || !get32(v, o, ts) || !get32(v, o, a) || !get32(v, o, b) || !get32(v, o, c) || !get32(v, o, cnt)) return false;
    if (cnt > 100000) return false;
    if (cnt > (v.size() - o) / 4 || ts > 0xffff) return false;
    const unsigned type = ts & 255;
    if (!((type >= 0x13 && type <= 0x25) || type == 0x50 || type == 0x51)) return false;
    l.tick = uint64_t(lo) | uint64_t(hi) << 32;
    l.cmd.type = Pct(ts & 255);
    l.cmd.side = uint8_t(ts >> 8);
    l.cmd.a = int32_t(a); l.cmd.b = int32_t(b); l.cmd.c = int32_t(c);
    for (uint32_t k = 0; k < cnt; k++) {
      uint32_t id;
      if (!get32(v, o, id)) return false;
      l.cmd.ids.push_back(id);
    }
    parsed.push_back(std::move(l));
  }
  if (o != v.size()) return false;
  out = std::move(parsed);
  return true;
}

}  // namespace aztec::engine
