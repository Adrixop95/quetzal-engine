#include "core/loader_limits.hpp"
#include "core/visfile.hpp"

#include <cstdio>

namespace aztec {

bool parse_vis(const std::vector<uint8_t>& d, VisFile& out) {
  out.sides.clear();
  if (d.size() > detail::kFileBytes) return false;
  if (d.size() < 2) return false;
  size_t n = d[0] | d[1] << 8, o = 2;
  for (size_t i = 0; i < n; i++) {
    if (o + 4 > d.size()) return false;
    VisFile::Layer l;
    l.w = d[o] | d[o + 1] << 8;
    l.h = d[o + 2] | d[o + 3] << 8;
    o += 4;
    size_t sz = size_t(l.w) * l.h;
    if (!sz || sz > detail::kCells || o + sz > d.size()) return false;
    l.v.assign(d.begin() + o, d.begin() + o + sz);
    o += sz;
    out.sides.push_back(std::move(l));
  }
  return o == d.size();
}

static bool slurp(const std::string& path, std::vector<uint8_t>& d) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  uint8_t buf[4096];
  size_t n;
  d.clear();
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
    if (d.size() + n > detail::kFileBytes) { std::fclose(f); d.clear(); return false; }
    d.insert(d.end(), buf, buf + n);
  }
  std::fclose(f);
  return true;
}

bool load_vis(const std::string& path, VisFile& out) {
  std::vector<uint8_t> d;
  return slurp(path, d) && parse_vis(d, out);
}

std::vector<BriefingSection> parse_briefing(const std::string& raw) {
  std::vector<BriefingSection> res;
  if (raw.size() > detail::kTextBytes) return res;
  size_t p = 0;
  while (p < raw.size()) {
    size_t e = raw.find('\n', p);
    if (e == std::string::npos) e = raw.size();
    std::string line = raw.substr(p, e - p);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    p = e + 1;
    if (line.size() > 1 && line[0] == '~' && line[1] != '~') {
      BriefingSection s;
      s.title = line.substr(1);
      while (p < raw.size()) {
        e = raw.find('\n', p);
        if (e == std::string::npos) e = raw.size();
        std::string l2 = raw.substr(p, e - p);
        p = e + 1;
        std::string t = l2;
        while (!t.empty() && (t.back() == '\r' || t.back() == ' ')) t.pop_back();
        if (t == "~~") break;
        s.text += l2 + "\n";
      }
      res.push_back(std::move(s));
    }
  }
  return res;
}

bool load_briefing(const std::string& path, std::vector<BriefingSection>& out) {
  std::vector<uint8_t> d;
  if (!slurp(path, d)) return false;
  out = parse_briefing(std::string(d.begin(), d.end()));
  return !out.empty();
}

}  // namespace aztec
