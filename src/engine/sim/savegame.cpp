#include "engine/sim/savegame.hpp"

#include <cstdio>
#include <cstring>
#include <cmath>

#include "core/mapfiles.hpp"
#include "core/endian.hpp"
#include "core/loader_limits.hpp"

namespace aztec::engine {

static void put(std::vector<uint8_t>& v, const void* p, size_t n) { const uint8_t* b = static_cast<const uint8_t*>(p); v.insert(v.end(), b, b + n); }
template <class T> static void scalar(std::vector<uint8_t>& v, T x) {
  size_t o = v.size(); v.resize(o + sizeof(T)); write_le(v.data() + o, x);
}
template <class T> static bool get(const std::vector<uint8_t>& v, size_t& o, T& x) {
  if (o + sizeof(T) > v.size()) return false;
  x = read_le<T>(v.data() + o);
  o += sizeof(T);
  return true;
}

std::vector<uint8_t> serialize_save(const SaveData& s) {
  std::vector<uint8_t> v;
  put(v, "AZS2", 4);
  uint32_t n = uint32_t(s.mission.size());
  scalar(v, n);
  put(v, s.mission.data(), n);
  scalar(v, s.tick);
  scalar(v, s.hash);
  scalar(v, s.script_hash);
  scalar(v, s.cam_x); scalar(v, s.cam_y); scalar(v, s.cam_zoom);
  std::vector<uint8_t> log = serialize_log(s.log);
  uint32_t ln = uint32_t(log.size());
  scalar(v, ln);
  put(v, log.data(), log.size());
  return v;
}

bool deserialize_save(const std::vector<uint8_t>& v, SaveData& out) {
  if (v.size() > detail::kFileBytes) return false;
  SaveData parsed;
  if (v.size() < 8 || std::memcmp(v.data(), "AZS2", 4) != 0) return false;
  size_t o = 4;
  uint32_t n, ln;
  if (!get(v, o, n) || n > detail::kTextBytes || n > v.size() - o) return false;
  parsed.mission.assign(reinterpret_cast<const char*>(v.data() + o), n);
  if (parsed.mission.find('\0') != std::string::npos) return false;
  o += n;
  if (!get(v, o, parsed.tick) || !get(v, o, parsed.hash) || !get(v, o, parsed.script_hash) || !get(v, o, parsed.cam_x) || !get(v, o, parsed.cam_y) || !get(v, o, parsed.cam_zoom) || !get(v, o, ln)) return false;
  if (ln != v.size() - o || !std::isfinite(parsed.cam_x) || !std::isfinite(parsed.cam_y) ||
      !std::isfinite(parsed.cam_zoom) || parsed.cam_zoom <= 0) return false;
  std::vector<uint8_t> log(v.begin() + long(o), v.begin() + long(o + ln));
  if (!deserialize_log(log, parsed.log)) return false;
  out = std::move(parsed);
  return true;
}

bool write_save(const std::string& path, const SaveData& s) {
  std::vector<uint8_t> v = serialize_save(s);
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  bool ok = std::fwrite(v.data(), 1, v.size(), f) == v.size();
  std::fclose(f);
  return ok;
}

uint64_t file_hash(const std::string& path) {
  std::vector<uint8_t> v;
  if (path.empty() || !read_file(path, v)) return 0;
  uint64_t h = 1469598103934665603ull;
  for (uint8_t b : v) h = (h ^ b) * 1099511628211ull;
  return h;
}

bool read_save(const std::string& path, SaveData& out) {
  std::vector<uint8_t> v;
  return read_file(path, v) && deserialize_save(v, out);
}

}  // namespace aztec::engine
