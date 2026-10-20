// Developer tool: --sprite-sheet ARCHIVE:FIRST-LAST OUT.png renders sprites (all stacked images) into a PNG
// contact sheet, to look for UI/cursor/unit images in Res/*.xbe.
#pragma once
#include <string>

namespace aztec::engine {

// spec = "common:0-21" / "units:1368-1380[:group]"; palette 0 = from sprite header (team 0).
bool write_sprite_sheet(const std::string& game_dir, const std::string& spec, const std::string& out_png, std::string& err);

}  // namespace aztec::engine
