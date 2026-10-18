#include "core/ini.hpp"

#include <cstdio>
#include <cstdlib>

namespace aztec {

bool Ini::load(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::string text;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
  std::fclose(f);
  return parse(text);
}

bool Ini::parse(const std::string& text) {
  data_.clear();
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
      section = line.substr(1, line.size() - 2);
      data_[section];
    } else if (size_t eq = line.find('='); eq != std::string::npos) {
      data_[section][line.substr(0, eq)] = line.substr(eq + 1);
    }
  }
  return !data_.empty();
}

std::string Ini::get(const std::string& section, const std::string& key, const std::string& def) const {
  auto s = data_.find(section);
  if (s == data_.end()) return def;
  auto k = s->second.find(key);
  return k == s->second.end() ? def : k->second;
}

int Ini::get_int(const std::string& section, const std::string& key, int def) const {
  std::string v = get(section, key);
  return v.empty() ? def : std::atoi(v.c_str());
}

}  // namespace aztec
