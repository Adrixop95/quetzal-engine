#include "core/loader_limits.hpp"
#include "core/userfiles.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace aztec {

namespace {
struct NameId { const char* name; int id; };
const NameId kActions[] = {
    {"cheatwin", 52},
    {"add", 2},
    {"inv", 3},
    {"filtertype", 4},
    {"filtercondition", 5},
    {"fastscroll", 6},
    {"viewall", 7},
    {"temporary", 8},
    {"center", 9},
    {"putontop", 12},
    {"garrison", 13},
    {"send", 14},
    {"friend", 15},
    {"autorepair", 20},
    {"repair", 21},
    {"destroy", 22},
    {"aggressive", 23},
    {"defencive", 24},
    {"normal", 25},
    {"directcontrol", 26},
    {"lastcaller", 27},
    {"toggleinterface", 28},
    {"scrollleft", 38},
    {"scrollright", 39},
    {"scrollup", 40},
    {"scrolldown", 41},
    {"printscreen", 45},
};
const NameId kKeys[] = {
    {"-", 12},
    {"=", 13},
    {"backspace", 14},
    {"tab", 15},
    {"q", 16},
    {"w", 17},
    {"e", 18},
    {"r", 19},
    {"t", 20},
    {"y", 21},
    {"u", 22},
    {"i", 23},
    {"o", 24},
    {"p", 25},
    {"[", 26},
    {"]", 27},
    {"enter", 28},
    {"lcontrol", 29},
    {"a", 30},
    {"s", 31},
    {"d", 32},
    {"f", 33},
    {"g", 34},
    {"h", 35},
    {"j", 36},
    {"k", 37},
    {"l", 38},
    {";", 39},
    {"'", 40},
    {"`", 41},
    {"lshift", 42},
    {"\\", 43},
    {"z", 44},
    {"x", 45},
    {"c", 46},
    {"v", 47},
    {"b", 48},
    {"n", 49},
    {"m", 50},
    {",", 51},
    {".", 52},
    {"/", 53},
    {"rshift", 54},
    {"f1", 59},
    {"f2", 60},
    {"f3", 61},
    {"f4", 62},
    {"f5", 63},
    {"f6", 64},
    {"f7", 65},
    {"f8", 66},
    {"f9", 67},
    {"f10", 68},
    {"f11", 87},
    {"f12", 88},
    {"pad1", 79},
    {"pad2", 80},
    {"pad3", 81},
    {"pad4", 75},
    {"pad5", 76},
    {"pad6", 77},
    {"pad7", 71},
    {"pad8", 72},
    {"pad9", 73},
    {"pad0", 82},
    {"pad.", 83},
    {"numlock", 69},
    {"gray/", 181},
    {"gray*", 55},
    {"gray-", 74},
    {"gray+", 78},
    {"grayenter", 156},
    {"lalt", 56},
    {"space", 57},
    {"caps", 58},
    {"scrolllock", 70},
    {"rcontrol", 157},
    {"printscreen", 183},
    {"ralt", 184},
    {"home", 199},
    {"up", 200},
    {"pageup", 201},
    {"left", 203},
    {"right", 205},
    {"end", 207},
    {"down", 208},
    {"pagedown", 209},
    {"ins", 210},
    {"del", 211},
};

bool ieq(const std::string& a, const char* b) {
  size_t n = std::strlen(b);
  if (a.size() != n) return false;
  for (size_t i = 0; i < n; i++)
    if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
  return true;
}
uint32_t rd32(const std::vector<uint8_t>& d, size_t o) {
  return uint32_t(d[o]) | uint32_t(d[o + 1]) << 8 | uint32_t(d[o + 2]) << 16 | uint32_t(d[o + 3]) << 24;
}
void wr32(std::vector<uint8_t>& d, size_t o, uint32_t v) { for (int i = 0; i < 4; i++) d[o + i] = uint8_t(v >> (8 * i)); }
bool slurp(const std::string& path, std::vector<uint8_t>& d) {
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
std::string cstr(const std::vector<uint8_t>& d, size_t o, size_t max) {
  size_t n = 0;
  while (n < max && d[o + n]) n++;
  return std::string(d.begin() + o, d.begin() + o + n);
}
}  // namespace

constexpr size_t kOpt = 0x4c;

bool parse_player_file(const std::vector<uint8_t>& d, PlayerFile& p) {
  if (d.size() != kPlayerFileSize) return false;
  p = PlayerFile{};
  p.raw = d;
  p.flag = d[0];
  p.name = cstr(d, 1, 0x43);
  p.const_one = rd32(d, 0x44);
  PlayerOptions& o = p.options;
  o.acknowledgement = rd32(d, kOpt + 0x08);
  o.unit_speed_mode = rd32(d, kOpt + 0x0c);
  o.sfx_volume = rd32(d, kOpt + 0x94);
  o.music_volume = rd32(d, kOpt + 0x98);
  o.speech_volume = rd32(d, kOpt + 0x9c);
  o.frequency = rd32(d, kOpt + 0xa0);
  o.display_mode = rd32(d, kOpt + 0xa4);
  o.scroll_speed = rd32(d, kOpt + 0xa8);
  o.game_speed = rd32(d, kOpt + 0xac);
  uint64_t g = uint64_t(rd32(d, kOpt + 0xb0)) | uint64_t(rd32(d, kOpt + 0xb4)) << 32;
  std::memcpy(&o.gamma, &g, 8);
  o.view_all = d[kOpt + 0xb8];
  o.b9 = d[kOpt + 0xb9];
  o.default_provider = d[kOpt + 0xba];
  return true;
}

std::vector<uint8_t> serialize_player_file(const PlayerFile& p) {
  std::vector<uint8_t> d = p.raw.size() == kPlayerFileSize ? p.raw : std::vector<uint8_t>(kPlayerFileSize, 0);
  d[0] = p.flag;
  size_t nl = p.name.size() > 0x42 ? 0x42 : p.name.size();
  std::memcpy(&d[1], p.name.data(), nl);
  d[1 + nl] = 0;  // bytes after the NUL are uninitialised stack garbage in the original files
  wr32(d, 0x44, p.const_one);
  const PlayerOptions& o = p.options;
  wr32(d, kOpt + 0x08, o.acknowledgement);
  wr32(d, kOpt + 0x0c, o.unit_speed_mode);
  wr32(d, kOpt + 0x94, o.sfx_volume);
  wr32(d, kOpt + 0x98, o.music_volume);
  wr32(d, kOpt + 0x9c, o.speech_volume);
  wr32(d, kOpt + 0xa0, o.frequency);
  wr32(d, kOpt + 0xa4, o.display_mode);
  wr32(d, kOpt + 0xa8, o.scroll_speed);
  wr32(d, kOpt + 0xac, o.game_speed);
  uint64_t g;
  std::memcpy(&g, &o.gamma, 8);
  wr32(d, kOpt + 0xb0, uint32_t(g));
  wr32(d, kOpt + 0xb4, uint32_t(g >> 32));
  d[kOpt + 0xb8] = o.view_all;
  d[kOpt + 0xb9] = o.b9;
  d[kOpt + 0xba] = o.default_provider;
  return d;
}

bool load_player_file(const std::string& path, PlayerFile& out) {
  std::vector<uint8_t> d;
  return slurp(path, d) && parse_player_file(d, out);
}

bool parse_save_header(const std::vector<uint8_t>& d, SaveHeader& h) {
  if (d.size() < 0x158) return false;
  h = SaveHeader{};
  h.zero0 = rd32(d, 0);
  h.zero1 = rd32(d, 4);
  h.map_path = cstr(d, 8, 256);
  h.description = cstr(d, 0x108, 64);
  for (int i = 0; i < 8; i++) h.time[i] = uint16_t(d[0x148 + 2 * i] | d[0x149 + 2 * i] << 8);
  return true;
}
bool load_save_header(const std::string& path, SaveHeader& out) {
  std::vector<uint8_t> d;
  return slurp(path, d) && parse_save_header(d, out);
}

int key_action_id(const std::string& name) {
  for (const auto& a : kActions) if (ieq(name, a.name)) return a.id;
  return -1;
}
const char* key_action_name(int id) {
  for (const auto& a : kActions) if (a.id == id) return a.name;
  return nullptr;
}
int dik_from_name(const std::string& name) {
  for (const auto& k : kKeys) if (ieq(name, k.name)) return k.id;
  return -1;
}

bool parse_keys_ini(const std::string& text, std::vector<KeyBinding>& out) {
  out.clear();
  if (text.size() > detail::kTextBytes) return false;
  size_t p = 0;
  while (p < text.size()) {
    size_t e = text.find('\n', p);
    if (e == std::string::npos) e = text.size();
    std::string line = text.substr(p, e - p);
    p = e + 1;
    size_t i = 0;
    auto tok = [&]() {
      while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) i++;
      size_t s = i;
      while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r') i++;
      return line.substr(s, i - s);
    };
    std::string a = tok(), k = tok();
    if (a.empty() || k.empty()) continue;
    int ai = key_action_id(a), ki = dik_from_name(k);
    if (ai >= 0 && ki >= 0) out.push_back({ai, ki});
  }
  return !out.empty();
}
bool load_keys_ini(const std::string& path, std::vector<KeyBinding>& out) {
  std::vector<uint8_t> d;
  return slurp(path, d) && parse_keys_ini(std::string(d.begin(), d.end()), out);
}

bool parse_runtime_settings(const std::string& text, RuntimeSettings& s) {
  s = RuntimeSettings{};
  if (text.size() > detail::kTextBytes) return false;
  size_t p = 0;
  int n = 0;
  while (p < text.size()) {
    size_t e = text.find('\n', p);
    if (e == std::string::npos) e = text.size();
    std::string line = text.substr(p, e - p);
    p = e + 1;
    // scanf's integer conversions have undefined behavior on overflowing text.
    auto number = [](const char*& p, int& value) {
      char* end;
      errno = 0;
      long v = std::strtol(p, &end, 10);
      if (end == p || errno == ERANGE || v < INT_MIN || v > INT_MAX) return false;
      p = end; value = int(v); return true;
    };
    auto setting = [&](const char* key, int& value) {
      size_t len = std::strlen(key);
      if (line.compare(0, len, key)) return false;
      const char* p = line.c_str() + len;
      return number(p, value);
    };
    int w, h;
    if (line.compare(0, 11, "resolution=") == 0) {
      const char* p = line.c_str() + 11;
      if (number(p, w) && *p == 'x' && (++p, number(p, h))) { s.res_w = w; s.res_h = h; n++; }
    } else if (setting("window_mode=", s.window_mode)) n++;
    else if (setting("smooth=", s.smooth)) n++;
    else if (setting("video=", s.video)) n++;
    else if (setting("mouse_speed=", s.mouse_speed)) {
      s.mouse_speed = s.mouse_speed < 20 ? 20 : s.mouse_speed > 500 ? 500 : s.mouse_speed; n++;
    }
  }
  return n > 0;
}

}  // namespace aztec
