#include "core/loader_limits.hpp"
#include "core/ini.hpp"

#include <cstdio>
#include <cstdlib>

namespace aztec {

// GetPrivateProfile* is case-insensitive (the game reads "ScenarioId" from "ScenarioID=" lines).
static std::string lower(std::string s) {
  for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c + 32);
  return s;
}

bool Ini::load(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::string text;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
    if (text.size() + n > detail::kTextBytes) { std::fclose(f); return false; }
    text.append(buf, n);
  }
  std::fclose(f);
  return parse(text);
}

bool Ini::parse(const std::string& text) {
  data_.clear();
  if (text.size() > detail::kTextBytes) return false;
  std::string section;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (line.empty()) continue;
    if (line.front() == '[' && line.back() == ']') {
      section = lower(line.substr(1, line.size() - 2));
      data_[section];
    } else if (size_t eq = line.find('='); eq != std::string::npos) {
      data_[section][lower(line.substr(0, eq))] = line.substr(eq + 1);
    }
  }
  return !data_.empty();
}

std::string Ini::get(const std::string& section, const std::string& key, const std::string& def) const {
  auto s = data_.find(lower(section));
  if (s == data_.end()) return def;
  auto k = s->second.find(lower(key));
  return k == s->second.end() ? def : k->second;
}

int Ini::get_int(const std::string& section, const std::string& key, int def) const {
  std::string v = get(section, key);
  return v.empty() ? def : detail::integer(v.c_str(), def);
}

}  // namespace aztec

namespace aztec {
bool Ini::has_section(const std::string& section) const { return data_.count(lower(section)) != 0; }
bool Ini::has_key(const std::string& section, const std::string& key) const {
  auto s = data_.find(lower(section));
  return s != data_.end() && s->second.count(lower(key)) != 0;
}
}  // namespace aztec
