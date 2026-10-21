// Res/strbase.bds: text database "*STRING { *ID 0x.. *ORDINAL n *LANGUAGE { *ENGLISH ".." *RUSSIAN ".." } }".
// The *RUSSIAN field actually holds Polish text in cp1250 (docs/formats.md). Texts are returned as UTF-8.
#pragma once
#include <map>
#include <string>

namespace aztec {

struct StrEntry { unsigned id = 0; std::string en, pl; };

class StrBase {
 public:
  bool load(const std::string& path);
  bool parse(const std::string& raw_cp1250);
  size_t size() const { return by_ordinal_.size(); }
  // Text for an ordinal (1..238); empty string when missing. `polish` selects the *RUSSIAN (= Polish) field.
  const std::string& get(int ordinal, bool polish = false) const;
  const StrEntry* entry(int ordinal) const;

 private:
  std::map<int, StrEntry> by_ordinal_;
};

// Replaces printf-style %i / %d / %s in a game string with the given values (in order); used for "Price: %i Time: %i".
std::string format_game_string(const std::string& fmt, const std::string& a = "", const std::string& b = "", const std::string& c = "");

}  // namespace aztec
