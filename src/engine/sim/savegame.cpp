#include "engine/sim/savegame.hpp"

#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

#include "core/endian.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {
namespace {
constexpr uint64_t kOffset = 1469598103934665603ull;
void mix(uint64_t& h, const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
}
uint64_t checksum(const std::vector<uint8_t>& v, size_t n) {
  uint64_t h = kOffset; mix(h, v.data(), n); return h;
}
template <class T> void scalar(std::vector<uint8_t>& v, T x) {
  size_t o = v.size(); v.resize(o + sizeof(T)); write_le(v.data() + o, x);
}
template <class T> bool get(const std::vector<uint8_t>& v, size_t& o, T& x) {
  if (o > v.size() || sizeof(T) > v.size() - o) return false;
  x = read_le<T>(v.data() + o); o += sizeof(T); return true;
}
bool valid_save(const SaveData& s) {
  if (s.mission.empty() || s.mission.size() > 128 || s.tick > kMaxSaveTicks ||
      s.mission.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos ||
      !std::isfinite(s.cam_x) || !std::isfinite(s.cam_y) || !std::isfinite(s.cam_zoom) ||
      std::fabs(s.cam_x) > 10000000 || std::fabs(s.cam_y) > 10000000 || s.cam_zoom < 0.125f || s.cam_zoom > 8.f)
    return false;
  return s.log.empty() || s.log.back().tick <= s.tick;
}
bool hash_file(const std::string& path, uint64_t& h) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  char buffer[16384];
  while (f.read(buffer, sizeof(buffer)) || f.gcount())
    mix(h, reinterpret_cast<const uint8_t*>(buffer), size_t(f.gcount()));
  return f.eof() && !f.bad();
}
} // namespace

std::vector<uint8_t> serialize_save(const SaveData& s) {
  if (!valid_save(s)) return {};
  auto log = serialize_log(s.log);
  if (log.empty()) return {};
  std::vector<uint8_t> v{'A', 'Z', 'S', '3'};
  scalar(v, kSaveFormatVersion); scalar(v, kSaveReplayVersion);
  scalar(v, uint32_t(s.mission.size()));
  v.insert(v.end(), s.mission.begin(), s.mission.end());
  scalar(v, s.tick); scalar(v, s.hash); scalar(v, s.script_hash); scalar(v, s.content_hash);
  scalar(v, s.cam_x); scalar(v, s.cam_y); scalar(v, s.cam_zoom);
  scalar(v, uint32_t(log.size()));
  v.insert(v.end(), log.begin(), log.end());
  scalar(v, s.modifications.bits());
  scalar(v, checksum(v, v.size()));
  return v;
}

bool deserialize_save(const std::vector<uint8_t>& v, SaveData& out) {
  if (v.size() < 84 || v.size() > kMaxSaveBytes || std::memcmp(v.data(), "AZS3", 4) != 0 ||
      read_le<uint64_t>(v.data() + v.size() - 8) != checksum(v, v.size() - 8)) return false;
  size_t o = 4;
  uint32_t version, replay, n, ln, modifications;
  if (!get(v, o, version) || version != kSaveFormatVersion || !get(v, o, replay) || replay != kSaveReplayVersion ||
      !get(v, o, n) || !n || n > 128 || n > v.size() - o) return false;
  SaveData parsed;
  parsed.mission.assign(reinterpret_cast<const char*>(v.data() + o), n); o += n;
  if (!get(v, o, parsed.tick) || !get(v, o, parsed.hash) || !get(v, o, parsed.script_hash) ||
      !get(v, o, parsed.content_hash) || !get(v, o, parsed.cam_x) || !get(v, o, parsed.cam_y) ||
      !get(v, o, parsed.cam_zoom) || !get(v, o, ln) || o > v.size() - 12 || ln != v.size() - 12 - o) return false;
  std::vector<uint8_t> log(v.begin() + o, v.end() - 12);
  o += ln;
  if (!get(v, o, modifications) || !Modifications::valid(modifications)) return false;
  parsed.modifications = Modifications::from_bits(modifications);
  if (!deserialize_log(log, parsed.log) || !valid_save(parsed)) return false;
  out = std::move(parsed);
  return true;
}

bool write_save(const std::string& path, const SaveData& s) {
  auto v = serialize_save(s);
  if (v.empty()) return false;
  // Exclusive creation avoids collisions and following a pre-existing temporary symlink.
  static std::atomic<unsigned> sequence{0};
  std::filesystem::path target(path), temporary;
  int fd = -1;
  for (int attempt = 0; attempt < 100; ++attempt) {
#ifdef _WIN32
    auto pid = _getpid();
#else
    auto pid = getpid();
#endif
    temporary = target;
    temporary += ".tmp." + std::to_string(pid) + "." + std::to_string(sequence++);
#ifdef _WIN32
    fd = _wopen(temporary.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif
    if (fd >= 0) break;
    if (errno != EEXIST) return false;
  }
  if (fd < 0) return false;
#ifdef _WIN32
  FILE* f = _fdopen(fd, "wb");
#else
  FILE* f = fdopen(fd, "wb");
#endif
  bool ok = false;
  if (f) {
    ok = std::fwrite(v.data(), 1, v.size(), f) == v.size() && std::fflush(f) == 0;
#ifdef _WIN32
    if (ok) ok = _commit(fd) == 0;
#else
    if (ok) ok = fsync(fd) == 0;
#endif
    if (std::fclose(f) != 0) ok = false;
  } else {
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
  }
  if (ok) {
#ifdef _WIN32
    ok = MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = std::rename(temporary.c_str(), target.c_str()) == 0;
#endif
  }
  std::error_code ec;
  if (!ok) std::filesystem::remove(temporary, ec);
  return ok;
}

uint64_t file_hash(const std::string& path) {
  uint64_t h = kOffset;
  return !path.empty() && hash_file(path, h) ? h : 0;
}

bool save_content_hash(const std::vector<std::pair<std::string, std::string>>& files, uint64_t& out) {
  uint64_t h = kOffset;
  for (const auto& [tag, path] : files) {
    uint8_t length[8]; write_le(length, uint64_t(tag.size())); mix(h, length, sizeof(length));
    mix(h, reinterpret_cast<const uint8_t*>(tag.data()), tag.size());
    const uint8_t present = !path.empty();
    mix(h, &present, 1);
    uint64_t digest = kOffset;
    if (present && !hash_file(path, digest)) return false;
    uint8_t encoded[8]; write_le(encoded, digest); mix(h, encoded, sizeof(encoded));
  }
  out = h;
  return true;
}

bool read_save(const std::string& path, SaveData& out) {
  // Check length before allocation, then reject concurrent growth or short reads.
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) return false;
  auto size = f.tellg();
  if (size < 0 || uint64_t(size) > kMaxSaveBytes) return false;
  std::vector<uint8_t> v(static_cast<size_t>(size));
  f.seekg(0);
  if (!f.read(reinterpret_cast<char*>(v.data()), std::streamsize(v.size())) ||
      f.peek() != std::char_traits<char>::eof() || f.bad()) return false;
  return deserialize_save(v, out);
}

std::unique_ptr<GameSim> replay_save(const SaveData& s, World& fresh,
    std::shared_ptr<const RulesData> rules, uint64_t expected_content_hash,
    const std::string& script_path, std::string& error) {
  error.clear();
  if (!rules || !valid_save(s) || serialize_log(s.log).empty()) { error = "Invalid replay data"; return {}; }
  if (s.mission != fresh.mission.file_name || fresh.original_save) { error = "Save mission is incompatible"; return {}; }
  if (s.content_hash != expected_content_hash) { error = "Save content is incompatible"; return {}; }
  if (s.script_hash != file_hash(script_path)) { error = "Save script is incompatible"; return {}; }
  World candidate = fresh;
  // Check script parsing explicitly: replay's low-level API tolerates attach failure.
  if (!script_path.empty()) {
    GameSim probe(rules);
    if (!probe.attach_script(script_path)) { error = "Cannot parse save script"; return {}; }
  }
  auto sim = GameSim::replay(candidate, std::move(rules), s.log, s.tick, script_path, {}, s.modifications);
  if (sim->tick_count() != s.tick || sim->state_hash(candidate) != s.hash) {
    error = "Save replay hash mismatch"; return {};
  }
  fresh = std::move(candidate);
  return sim;
}

}  // namespace aztec::engine
