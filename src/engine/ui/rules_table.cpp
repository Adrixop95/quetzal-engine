#include "engine/ui/rules_table.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace aztec::engine {

bool RulesTable::load(const std::string& path) {
  std::ifstream f(path);
  if (!f) return false;
  std::string line;
  std::getline(f, line);  // header
  v_.clear();
  while (std::getline(f, line)) {
    std::vector<std::string> cols;
    std::stringstream ss(line);
    std::string c;
    while (std::getline(ss, c, ',')) cols.push_back(c);
    if (cols.size() < 6) continue;
    int id = std::atoi(cols[0].c_str());
    if (id < 0 || id > 255) continue;
    if (int(v_.size()) <= id) v_.resize(id + 1);
    v_[id] = {std::atoi(cols[4].c_str()), std::atoi(cols[5].c_str())};
  }
  return !v_.empty();
}

}  // namespace aztec::engine
