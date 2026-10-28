#include "engine/assets/daylight.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "core/mapfiles.hpp"
#include "core/endian.hpp"

namespace aztec::engine {

namespace {
struct Section { uint32_t va, vsize, raw, rsize; };

uint32_t u32(const std::vector<uint8_t>& d, size_t o) { return o <= d.size() && d.size() - o >= 4 ? read_le<uint32_t>(d.data() + o) : 0; }
uint16_t u16(const std::vector<uint8_t>& d, size_t o) { return o <= d.size() && d.size() - o >= 2 ? read_le<uint16_t>(d.data() + o) : 0; }

// Entry layout: i32 time, 3 x f64 RGB, i32 level, i32 coefficient (36 bytes, packed).
std::vector<DayKey> read_table(const std::vector<uint8_t>& d, const std::vector<Section>& secs, uint32_t base, uint32_t va) {
  std::vector<DayKey> out;
  for (const Section& s : secs) {
    uint32_t rva = va - base;
    if (rva < s.va || rva >= s.va + std::max(s.vsize, s.rsize)) continue;
    size_t off = s.raw + (rva - s.va);
    for (int i = 0; i < 64 && off + 36 <= d.size(); i++, off += 36) {
      DayKey k;
      k.time = int(u32(d, off));
      for (int c = 0; c < 3; c++) k.rgb[c] = read_le<double>(d.data() + off + 4 + 8 * c);
      k.level = int(u32(d, off + 28));
      k.coeff = int(u32(d, off + 32));
      bool sane = k.time >= 0 && k.time <= Daylight::kDayTicks && k.level >= 0 && k.level < 20;
      for (double c : k.rgb) sane = sane && c > 0.05 && c < 4.0;
      if (!sane) return {};
      if (!out.empty() && k.time < out.back().time) return {};
      out.push_back(k);
      if (k.time >= Daylight::kDayTicks) break;
    }
    break;
  }
  if (out.empty() || out.back().time < Daylight::kDayTicks) return {};
  return out;
}
}  // namespace

bool Daylight::load_from_exe(const std::string& path) {
  std::vector<uint8_t> d;
  if (!read_file(path, d) || d.size() < 0x100 || d[0] != 'M' || d[1] != 'Z') return false;
  uint32_t pe = u32(d, 0x3c);
  if (u32(d, pe) != 0x4550) return false;
  int nsec = u16(d, pe + 6), opt = u16(d, pe + 20);
  uint32_t base = u32(d, pe + 24 + 28);
  std::vector<Section> secs;
  size_t o = pe + 24 + opt;
  for (int i = 0; i < nsec; i++, o += 40) secs.push_back({u32(d, o + 12), u32(d, o + 8), u32(d, o + 20), u32(d, o + 16)});
  keys_[0] = read_table(d, secs, base, 0x4a8f68);
  keys_[1] = read_table(d, secs, base, 0x4a9338);
  return loaded();
}

Light Daylight::sample(int tileset, double time) const {
  Light l;
  const std::vector<DayKey>& k = keys_[tileset ? 1 : 0];
  if (k.size() < 2) return l;
  time = std::fmod(std::max(0.0, time), double(kDayTicks));
  size_t i = 0;
  while (i + 1 < k.size() && k[i].time <= time) i++;  // first key with time > t (the game uses its values)
  const DayKey& b = k[i];
  const DayKey& a = k[i ? i - 1 : 0];
  double span = std::max(1, b.time - a.time);
  double f = i ? std::clamp((time - a.time) / span, 0.0, 1.0) : 1.0;
  double amb = a.level + (b.level - a.level) * f;
  double ramp_lo = amb, ramp_hi = std::max(11.0, amb);
  // The game draws with palette level ~ ambient + 1; tint fades to neutral between the ambient level and `ramp_hi`.
  double draw_level = amb + 1;
  double t = std::clamp((draw_level - ramp_lo) / (ramp_hi - ramp_lo + 1), 0.0, 1.0);
  for (int c = 0; c < 3; c++) {
    double tint = a.rgb[c] + (b.rgb[c] - a.rgb[c]) * f;
    l.rgb[c] = float(tint + (1.0 - tint) * t);
  }
  l.level = float(draw_level);
  return l;
}

}  // namespace aztec::engine
