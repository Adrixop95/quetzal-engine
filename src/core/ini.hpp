// Minimal INI reader for Maps/*.mp (sections, key=value, CRLF, no comments).
#pragma once
#include <map>
#include <string>

namespace aztec {

class Ini {
 public:
  bool load(const std::string& path);
  bool parse(const std::string& text);
  std::string get(const std::string& section, const std::string& key, const std::string& def = "") const;
  int get_int(const std::string& section, const std::string& key, int def = 0) const;
  bool has_section(const std::string& section) const { return data_.count(section) != 0; }

 private:
  std::map<std::string, std::map<std::string, std::string>> data_;
};

}  // namespace aztec
