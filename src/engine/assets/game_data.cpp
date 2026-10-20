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

std::string find_file_ci(const std::string& dir, const std::string& file) {
  std::error_code ec;
  std::string want = lower(file);
  std::string exact = dir + "/" + file;
  if (fs::exists(exact, ec)) return exact;
  for (const auto& e : fs::directory_iterator(dir, ec))
    if (lower(e.path().filename().string()) == want) return e.path().string();
  return {};
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

std::string GameData::map_file(const std::string& name, const std::string& ext) const {
  return find_file_ci(maps_dir(), name + "." + ext);
}

}  // namespace aztec::engine
