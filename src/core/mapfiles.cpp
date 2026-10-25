#include "core/loader_limits.hpp"
#include "core/mapfiles.hpp"

#include <cstdio>
#include <cstring>

namespace aztec {

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n < 0 || size_t(n) > detail::kFileBytes) { std::fclose(f); out.clear(); return false; }
  out.resize(size_t(n));
  size_t got = out.empty() ? 0 : std::fread(out.data(), 1, out.size(), f);
  std::fclose(f);
  return got == out.size();
}

static uint16_t u16(const std::vector<uint8_t>& d, size_t o) { return uint16_t(d[o] | d[o + 1] << 8); }

bool parse_passability(const std::vector<uint8_t>& d, int w, int h, Grid<uint8_t>& out) {
  if (w <= 0 || h <= 0 || size_t(w) > detail::kCells / size_t(h) || d.size() < size_t(w) * h) return false;
  out.w = w;
  out.h = h;
  out.v.assign(d.begin(), d.begin() + size_t(w) * h);
  return true;
}

static bool parse_words(const std::vector<uint8_t>& d, size_t hdr, int w, int h, Grid<uint16_t>& out) {
  if (w <= 0 || h <= 0 || size_t(w) > detail::kCells / size_t(h) || d.size() < hdr + size_t(w) * h * 2) return false;
  out.w = w;
  out.h = h;
  out.v.resize(size_t(w) * h);
  for (size_t i = 0; i < out.v.size(); i++) out.v[i] = u16(d, hdr + 2 * i);
  return true;
}

bool parse_resource_map(const std::vector<uint8_t>& d, Grid<uint16_t>& out) {
  if (d.size() < 4) return false;
  return parse_words(d, 4, u16(d, 0), u16(d, 2), out);
}

bool load_passability(const std::string& path, int w, int h, Grid<uint8_t>& out) {
  std::vector<uint8_t> d;
  return read_file(path, d) && parse_passability(d, w, h, out);
}
bool load_resource_map(const std::string& path, Grid<uint16_t>& out) {
  std::vector<uint8_t> d;
  return read_file(path, d) && parse_resource_map(d, out);
}
bool load_preview(const std::string& path, Sprite& out) {
  std::vector<uint8_t> d;
  return read_file(path, d) && decode_sprite({d.data(), d.size()}, out);
}

}  // namespace aztec
