#include "engine/sim/savegame.hpp"

#include <cstdio>
#include <cstring>

#include "core/mapfiles.hpp"

namespace aztec::engine {

static void put(std::vector<uint8_t>& v, const void* p, size_t n) { const uint8_t* b = static_cast<const uint8_t*>(p); v.insert(v.end(), b, b + n); }
template <class T> static bool get(const std::vector<uint8_t>& v, size_t& o, T& x) {
  if (o + sizeof(T) > v.size()) return false;
  std::memcpy(&x, v.data() + o, sizeof(T));
  o += sizeof(T);
  return true;
}

std::vector<uint8_t> serialize_save(const SaveData& s) {
  std::vector<uint8_t> v;
  put(v, "AZS1", 4);
  uint32_t n = uint32_t(s.mission.size());
  put(v, &n, 4);
  put(v, s.mission.data(), n);
  put(v, &s.tick, 8);
  put(v, &s.hash, 8);
  put(v, &s.cam_x, 4); put(v, &s.cam_y, 4); put(v, &s.cam_zoom, 4);
  std::vector<uint8_t> log = serialize_log(s.log);
  uint32_t ln = uint32_t(log.size());
  put(v, &ln, 4);
  put(v, log.data(), log.size());
  return v;
}

bool deserialize_save(const std::vector<uint8_t>& v, SaveData& out) {
  if (v.size() < 8 || std::memcmp(v.data(), "AZS1", 4) != 0) return false;
  size_t o = 4;
  uint32_t n, ln;
  if (!get(v, o, n) || o + n > v.size()) return false;
  out.mission.assign(reinterpret_cast<const char*>(v.data() + o), n);
  o += n;
  if (!get(v, o, out.tick) || !get(v, o, out.hash) || !get(v, o, out.cam_x) || !get(v, o, out.cam_y) || !get(v, o, out.cam_zoom) || !get(v, o, ln)) return false;
  if (o + ln > v.size()) return false;
  std::vector<uint8_t> log(v.begin() + long(o), v.begin() + long(o + ln));
  return deserialize_log(log, out.log);
}

bool write_save(const std::string& path, const SaveData& s) {
  std::vector<uint8_t> v = serialize_save(s);
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  bool ok = std::fwrite(v.data(), 1, v.size(), f) == v.size();
  std::fclose(f);
  return ok;
}

bool read_save(const std::string& path, SaveData& out) {
  std::vector<uint8_t> v;
  return read_file(path, v) && deserialize_save(v, out);
}

}  // namespace aztec::engine
