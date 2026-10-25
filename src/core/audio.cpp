#include "core/loader_limits.hpp"
#include "core/audio.hpp"

#include <cstdio>
#include <cstring>

namespace aztec {

static uint32_t u32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
static uint16_t u16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }

bool parse_wav(const uint8_t* d, size_t n, WavInfo& out) {
  out = WavInfo{};
  if (!d || n < 12 || std::memcmp(d, "RIFF", 4) || std::memcmp(d + 8, "WAVE", 4)) return false;
  size_t p = 12;
  bool fmt = false;
  while (p + 8 <= n) {
    uint32_t sz = u32(d + p + 4);
    const uint8_t* c = d + p + 8;
    if (!std::memcmp(d + p, "fmt ", 4) && sz >= 16 && p + 8 + 16 <= n) {
      if (u16(c) != 1) return false;  // PCM only
      out.channels = u16(c + 2);
      out.rate = int(u32(c + 4));
      out.bits = u16(c + 14);
      fmt = true;
    } else if (!std::memcmp(d + p, "data", 4)) {
      if (!fmt) return false;
      size_t avail = n - (p + 8);
      out.pcm = c;
      out.pcm_bytes = sz < avail ? sz : avail;
      return true;
    }
    if (sz > n - p - 8) return false;
    p += 8 + size_t(sz) + (sz & 1);
  }
  return false;
}

bool SoundBank::open(const std::string& path) { return ar_.open(path); }

std::vector<uint32_t> SoundBank::sound_set(uint32_t id) const {
  std::vector<uint32_t> r;
  Item it = ar_.get(0x3e8, id);
  if (!it) return r;
  for (size_t i = 0; i + 4 <= it.size; i += 4) r.push_back(u32(it.data + i));
  return r;
}

std::string SoundBank::subtitle(uint32_t id) const {
  Item it = ar_.get(0x6, id);
  if (!it) return "";
  size_t n = 0;
  while (n < it.size && it.data[n]) n++;
  return std::string(reinterpret_cast<const char*>(it.data), n);
}

bool read_music_table(const std::string& path, std::vector<MusicTrack>& out) {
  out.clear();
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long length = std::ftell(f);
  if (length < 0) { std::fclose(f); return false; }
  uint64_t total = uint64_t(length);
  std::fseek(f, 0, SEEK_SET);
  uint8_t h[20];
  if (std::fread(h, 1, 20, f) != 20 || u32(h) != 1 || u32(h + 4) != 0x460) { std::fclose(f); return false; }
  uint32_t table = u32(h + 8), lo = u32(h + 12), hi = u32(h + 16);
  if (hi < lo || hi - lo > detail::kEntries || table > total || uint64_t(hi - lo) * 4 > total - table) { std::fclose(f); return false; }
  std::vector<uint8_t> t(size_t(hi - lo) * 4);
  std::fseek(f, long(table), SEEK_SET);
  bool ok = std::fread(t.data(), 1, t.size(), f) == t.size();
  std::fclose(f);
  if (!ok) return false;
  for (uint32_t k = 0; k < hi - lo; k++) {
    uint64_t s = u32(&t[k * 4]);
    uint64_t e = k + 1 < hi - lo ? u32(&t[(k + 1) * 4]) : total;
    if (s > e || e > total) { out.clear(); return false; }
    if (e > s) out.push_back({lo + k, s, e - s});
  }
  return true;
}

bool read_music_track(const std::string& path, const MusicTrack& t, std::vector<uint8_t>& out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  out.clear();
  std::fseek(f, 0, SEEK_END);
  long length = std::ftell(f);
  // Validate the requested range before narrowing offsets or allocating.
  if (length < 0 || t.size > detail::kFileBytes || t.offset > uint64_t(length) ||
      t.size > uint64_t(length) - t.offset) { std::fclose(f); return false; }
  out.resize(size_t(t.size));
  std::fseek(f, long(t.offset), SEEK_SET);
  bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
  std::fclose(f);
  return ok;
}

uint32_t music_allowed_mask(int music, uint32_t mask, uint32_t available) {
  if (music < 0) return 0;
  if (music != 0) {
    int n = music > 32 ? 32 : music;
    return ((uint32_t(1) << (n - 1)) | 0) & available;
  }
  return mask & available;
}

}  // namespace aztec
