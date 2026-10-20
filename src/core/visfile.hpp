// Maps/<name>.vis (per-side light/fog layers) and Maps/<name>.mit (briefing text).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace aztec {

// .vis: u16 side_count, then per side: u16 w, u16 h, w*h bytes. Lower byte = brighter
// (0 lit, 152 = fully dark/unexplored); one layer per side.
struct VisFile {
  struct Layer { int w = 0, h = 0; std::vector<uint8_t> v; };
  std::vector<Layer> sides;
};
bool parse_vis(const std::vector<uint8_t>& data, VisFile& out);
bool load_vis(const std::string& path, VisFile& out);

// .mit: sections "~Title\r\ntext...\r\n~~" (cp1250); text returned as raw bytes.
struct BriefingSection { std::string title, text; };
std::vector<BriefingSection> parse_briefing(const std::string& raw);
bool load_briefing(const std::string& path, std::vector<BriefingSection>& out);

}  // namespace aztec
