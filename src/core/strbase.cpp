#include "core/strbase.hpp"

#include <cstdlib>

#include "core/mapfiles.hpp"
#include "core/text.hpp"

namespace aztec {

static const std::string kEmpty;

// Reads a quoted string starting after `key` inside block [b, e); handles \" and \\ escapes.
static bool quoted_after(const std::string& s, size_t b, size_t e, const char* key, std::string& out) {
  size_t p = s.find(key, b);
  if (p == std::string::npos || p >= e) return false;
  p = s.find('"', p);
  if (p == std::string::npos || p >= e) return false;
  out.clear();
  for (size_t i = p + 1; i < e && s[i] != '"'; i++) {
    if (s[i] == '\\' && i + 1 < e) {
      i++;
      out += s[i] == 'n' ? '\n' : s[i] == 't' ? '\t' : s[i];
    } else {
      out += s[i];
    }
  }
  return true;
}

bool StrBase::parse(const std::string& s) {
  by_ordinal_.clear();
  size_t pos = 0;
  while ((pos = s.find("*STRING", pos)) != std::string::npos) {
    size_t next = s.find("*STRING", pos + 7);
    size_t end = next == std::string::npos ? s.size() : next;
    size_t o = s.find("*ORDINAL", pos);
    size_t i = s.find("*ID", pos);
    StrEntry e;
    int ordinal = -1;
    if (o != std::string::npos && o < end) ordinal = std::atoi(s.c_str() + o + 8);
    if (i != std::string::npos && i < end) e.id = unsigned(std::strtoul(s.c_str() + i + 3, nullptr, 16));
    std::string en, pl;
    if (quoted_after(s, pos, end, "*ENGLISH", en)) e.en = cp1250_to_utf8(en);
    if (quoted_after(s, pos, end, "*RUSSIAN", pl)) e.pl = cp1250_to_utf8(pl);
    if (ordinal >= 0) by_ordinal_[ordinal] = e;
    pos = end;
  }
  return !by_ordinal_.empty();
}

bool StrBase::load(const std::string& path) {
  std::vector<uint8_t> d;
  if (!read_file(path, d)) return false;
  return parse(std::string(d.begin(), d.end()));
}

const StrEntry* StrBase::entry(int ordinal) const {
  auto it = by_ordinal_.find(ordinal);
  return it == by_ordinal_.end() ? nullptr : &it->second;
}

const std::string& StrBase::get(int ordinal, bool polish) const {
  const StrEntry* e = entry(ordinal);
  return e ? (polish ? e->pl : e->en) : kEmpty;
}

std::string format_game_string(const std::string& fmt, const std::string& a, const std::string& b, const std::string& c) {
  const std::string* args[3] = {&a, &b, &c};
  int n = 0;
  std::string out;
  for (size_t i = 0; i < fmt.size(); i++) {
    if (fmt[i] == '%' && i + 1 < fmt.size() && (fmt[i + 1] == 'i' || fmt[i + 1] == 'd' || fmt[i + 1] == 's')) {
      if (n < 3) out += *args[n++];
      i++;
    } else {
      out += fmt[i];
    }
  }
  return out;
}

}  // namespace aztec
