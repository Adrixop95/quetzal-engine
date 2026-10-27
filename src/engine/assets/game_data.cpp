#include "engine/assets/game_data.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace aztec::engine {

static std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}

// Resolve every component so a lower-case installation also works on case-sensitive hosts.
static fs::path resolve_ci(const fs::path& path) {
  std::error_code ec;
  if (fs::exists(path, ec)) return path;
  if (path.empty() || path == path.parent_path()) return {};
  fs::path parent = path.has_parent_path() ? resolve_ci(path.parent_path()) : fs::path(".");
  if (parent.empty()) return {};
  std::string want = lower(path.filename().string());
  fs::path match;
  fs::directory_iterator it(parent, ec), end;
  while (!ec && it != end) {
    const auto candidate = it->path();
    if (lower(candidate.filename().string()) == want &&
        (match.empty() || candidate < match)) match = candidate;
    it.increment(ec);
  }
  return match;
}

std::string find_file_ci(const std::string& dir, const std::string& file) {
  if (dir.empty() || file.empty()) return {};
  return resolve_ci(fs::path(dir) / fs::path(file)).string();
}

bool GameData::open(const std::string& game_dir, std::string& err) {
  root_ = game_dir;
  struct { Archive* a; const char* f; } list[] = {{&pal_, "Pal"}, {&units_, "Units"}, {&tiles0_, "Tiles0"},
                                                  {&tiles1_, "Tiles1"}, {&common_, "common"}, {&fnts_, "fnts"}, {&light_, "light"}};
  for (auto& e : list) {
    std::string p = find_file_ci(root_ + "/Res", std::string(e.f) + ".xbe");
    if (p.empty() || !e.a->open(p)) { err = std::string("cannot open Res/") + e.f + ".xbe in " + root_; return false; }
  }
  return true;
}

const Archive& GameData::archive(ArchiveId id) const {
  switch (id) {
    case ArchiveId::Units: return units_;
    case ArchiveId::Tiles0: return tiles0_;
    case ArchiveId::Tiles1: return tiles1_;
    case ArchiveId::Common: return common_;
    case ArchiveId::Fnts: return fnts_;
    case ArchiveId::Light: return light_;
    case ArchiveId::Pal: break;
  }
  return pal_;
}

std::string GameData::maps_dir() const { return find_file_ci(root_, "Maps"); }

std::string GameData::map_file(const std::string& name, const std::string& ext) const {
  return find_file_ci(maps_dir(), name + "." + ext);
}

}  // namespace aztec::engine
