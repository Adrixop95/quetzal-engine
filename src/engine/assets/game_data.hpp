// Opened game resource archives plus case-insensitive file lookup in the game directory.
#pragma once
#include <string>
#include "core/archive.hpp"

namespace aztec::engine {

enum class ArchiveId : uint8_t { Units, Tiles0, Tiles1, Common, Pal };

class GameData {
 public:
  // Opens Res/{Pal,Units,Tiles0,Tiles1,common}.xbe. On failure returns false and fills `err`.
  bool open(const std::string& game_dir, std::string& err);
  const std::string& root() const { return root_; }
  const Archive& archive(ArchiveId id) const;
  const Archive& tiles(int tileset) const { return tileset == 1 ? tiles1_ : tiles0_; }
  // Finds `Maps/<name>.<ext>` ignoring case (the data mixes `Mission_4.map` with `mission_4.mp`). Empty if absent.
  std::string map_file(const std::string& name, const std::string& ext) const;
  std::string maps_dir() const { return root_ + "/Maps"; }

 private:
  std::string root_;
  Archive pal_, units_, tiles0_, tiles1_, common_;
};

// Case-insensitive lookup of `file` inside `dir`; returns the full path or "".
std::string find_file_ci(const std::string& dir, const std::string& file);

}  // namespace aztec::engine
