// Deterministic, synthetic robustness coverage; no installed game data is used.
#include "core/archive.hpp"
#include "core/audio.hpp"
#include "core/ini.hpp"
#include "core/map.hpp"
#include "core/mapfiles.hpp"
#include "core/mp.hpp"
#include "core/palette.hpp"
#include "core/script.hpp"
#include "core/savegame.hpp"
#include "core/sprite_alpha.hpp"
#include "core/strbase.hpp"
#include "core/text.hpp"
#include "core/ugi.hpp"
#include "core/userfiles.hpp"
#include "core/visfile.hpp"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <unistd.h>

using namespace aztec;
using Bytes = std::vector<uint8_t>;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static void word(Bytes& d, size_t p, uint32_t v, size_t n = 4) {
  for (size_t i = 0; i < n; ++i) d[p + i] = uint8_t(v >> (8 * i));
}
static Bytes bytes(const std::string& s) { return Bytes(s.begin(), s.end()); }
static size_t cases = 0;
static void mutate(const Bytes& seed, const std::function<void(const Bytes&)>& run) {
  run(seed); ++cases;
  // Every byte boundary includes header, table, control stream and payload truncations.
  for (size_t n = 0; n < seed.size(); ++n) { run(Bytes(seed.begin(), seed.begin() + n)); ++cases; }
  for (size_t p = 0; p < seed.size(); ++p) {
    for (unsigned bit = 0; bit < 8; ++bit) {
      Bytes d = seed; d[p] ^= uint8_t(1u << bit); run(d); ++cases;
    }
    // Unaligned fields too: zero, signed extremes, all bits set and overflow boundaries.
    for (uint32_t v : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
      if (p + 4 > seed.size()) continue;
      Bytes d = seed; word(d, p, v); run(d); ++cases;
    }
  }
}
struct Temp {
  std::string path;
  Temp() {
    char name[] = "/tmp/aztec-fuzz-XXXXXX";
    int fd = mkstemp(name); CHECK(fd >= 0); close(fd); path = name;
  }
  ~Temp() { std::filesystem::remove(path); }
  void put(const Bytes& d) {
    FILE* f = std::fopen(path.c_str(), "wb"); CHECK(f);
    CHECK(d.empty() || std::fwrite(d.data(), 1, d.size(), f) == d.size());
    CHECK(std::fclose(f) == 0);
  }
};
static Bytes archive(uint32_t group, const Bytes& item) {
  Bytes d(24 + item.size()); word(d, 0, 1); word(d, 4, group);
  word(d, 8, 20); word(d, 12, 1); word(d, 16, 2); word(d, 20, 24);
  std::copy(item.begin(), item.end(), d.begin() + 24); return d;
}
int main() {
  Temp file;
  Palette pal{};
  Bytes raw(28); word(raw, 0, 0xe0ac0000); word(raw, 4, 2, 2); word(raw, 6, 2, 2); raw[8] = kRaw;
  Bytes rle(35); word(rle, 0, 0xe0ac0000); word(rle, 4, 2, 2); word(rle, 6, 1, 2);
  word(rle, 24, 32); word(rle, 28, 33); rle[32] = 0; rle[33] = 2;
  // Separate control bytes from the pixel payload.
  rle.resize(36); word(rle, 28, 34);
  Bytes alpha = rle; alpha[8] = 0x40; alpha[32] = 0x82;
  Sprite sprite; AlphaSprite as;
  CHECK(decode_sprite({raw.data(), raw.size()}, sprite));
  CHECK(decode_sprite({rle.data(), rle.size()}, sprite));
  CHECK(decode_alpha_sprite({alpha.data(), alpha.size()}, as));
  Bytes raw16 = raw; raw16[8] = kRaw | kRaw16; raw16.resize(32);
  Bytes rle16 = rle; rle16[8] = kRle16; rle16[32] = 0xc2; rle16.resize(38);
  Bytes blend = alpha; blend[32] = 0xc2; blend.resize(38);
  CHECK(decode_sprite({raw16.data(), raw16.size()}, sprite));
  CHECK(decode_sprite({rle16.data(), rle16.size()}, sprite));
  CHECK(decode_alpha_sprite({blend.data(), blend.size()}, as));
  for (const Bytes& seed : {raw, raw16, rle, rle16, alpha, blend}) mutate(seed, [&](const Bytes& d) {
    Sprite s; if (decode_sprite({d.data(), d.size()}, s)) CHECK(to_argb(s, pal).size() == s.opaque.size());
    AlphaSprite a; if (decode_alpha_sprite({d.data(), d.size()}, a)) CHECK(alpha_to_argb(a, pal).size() == a.opaque.size());
  });
  Bytes ar = archive(0x450, Bytes(512)); file.put(ar);
  Archive a; CHECK(a.open(file.path)); CHECK(load_palette(a, 1, 0, pal));
  mutate(ar, [&](const Bytes& d) {
    file.put(d); a.open(file.path);
    for (const auto& g : a.groups()) {
      for (uint32_t id : {g.lo, g.hi, UINT32_MAX}) { auto item = a.get(g.id, id); (void)item; }
    }
    load_palette(a, 1, UINT32_MAX, pal);
    SoundBank bank; bank.open(file.path); bank.sound_set(1); bank.subtitle(1);
    std::vector<MusicTrack> tracks; read_music_table(file.path, tracks);
  });
  // Failed reopen must not expose offsets into an earlier buffer.
  file.put(ar); CHECK(a.open(file.path)); file.put(Bytes(4)); CHECK(a.open(file.path)); CHECK(!a.get(0x450, 1));
  file.put(ar); CHECK(a.open(file.path)); file.put(Bytes(1)); CHECK(!a.open(file.path)); CHECK(!a.get(0x450, 1));
  Bytes map{2, 0, 2, 0}; map.resize(20);
  file.put(map); TerrainMap terrain; CHECK(load_terrain_map(file.path, terrain));
  Grid<uint16_t> resource; CHECK(parse_resource_map(map, resource));
  mutate(map, [&](const Bytes& d) {
    file.put(d); TerrainMap m; load_terrain_map(file.path, m);
    Grid<uint16_t> r; parse_resource_map(d, r); load_resource_map(file.path, r);
    Grid<uint8_t> p; parse_passability(d, 2, 2, p); load_passability(file.path, 2, 2, p);
    CHECK(!parse_passability(d, INT_MAX, INT_MAX, p)); CHECK(!parse_passability(d, -1, 2, p));
    Sprite s; load_preview(file.path, s);
  });
  Bytes vis{1, 0, 2, 0, 2, 0, 1, 2, 3, 4}; VisFile vf; CHECK(parse_vis(vis, vf));
  mutate(vis, [&](const Bytes& d) { file.put(d); VisFile v; parse_vis(d, v); load_vis(file.path, v); });
  const std::string mp = "[Globals]\nMapSize=2,2\nSides=1\nTowns=1\nTeams=1\n[Side0]\nDefenceCount=1\nDefence0=1,2,3\nCamerasCount=1\nCamera0=1,2,3\n[Town0]\nUnits=1\nUnit0=1,2,3\n[Team0]\nUnits=1\nUnit0=1,2,3\n";
  Ini ini; MpFile mission; CHECK(ini.parse(mp)); CHECK(parse_mp(ini, mission));
  for (const std::string& seed : {mp, std::string("trigger(1) conditions always actions timer(1).set(2) end"),
      std::string("~Title\nText\n~~\n"), std::string("*STRING *ORDINAL 1 *ID 01 *ENGLISH \"Hello\" *RUSSIAN \"World\""),
      std::string("add lshift\nresolution=800x600\nmouse_speed=100\n")}) {
    mutate(bytes(seed), [&](const Bytes& d) {
      std::string t(d.begin(), d.end()); Ini i; i.parse(t); i.get_int("Globals", "Sides"); MpFile m; parse_mp(i, m);
      MpUnit u; parse_unit_line(t, u); script::Script s; script::parse_script(t, s);
      StrBase strings; strings.parse(t); parse_briefing(t);
      std::vector<KeyBinding> keys; parse_keys_ini(t, keys); RuntimeSettings settings; parse_runtime_settings(t, settings);
      cp1250_to_utf8(t); utf8_to_cp1250(t); ugi_from_name(t);
    });
  }
  // Non-terminated views, a lone sign and adversarial text counts are regressions.
  const char view[] = {'t','r','i','g','g','e','r','(','1'}; script::Script sc;
  CHECK(!script::parse_script(std::string_view(view, sizeof view), sc));
  MpUnit unit; CHECK(!parse_unit_line("-", unit));
  for (const char* n : {"-1", "2147483647", "4294967295", "999999999999999999999"}) {
    for (const char* key : {"Sides", "Towns", "Teams", "Boats", "Johnkas", "Submarines", "Notes", "Notesr"}) {
      CHECK(ini.parse(std::string("[Globals]\n") + key + "=" + n)); CHECK(!parse_mp(ini, mission));
    }
  }
  file.put(bytes(mp)); CHECK(ini.load(file.path)); CHECK(load_mp(file.path, mission));
  file.put(bytes("trigger(1) conditions always actions end")); CHECK(script::load_script(file.path, sc));
  StrBase strings; file.put(bytes("*STRING *ORDINAL 1 *ENGLISH \"Hello\"")); CHECK(strings.load(file.path));
  std::vector<BriefingSection> brief; file.put(bytes("~Title\nText\n~~")); CHECK(load_briefing(file.path, brief));
  std::vector<KeyBinding> keys; file.put(bytes("add lshift")); CHECK(load_keys_ini(file.path, keys));
  for (size_t size : {kPlayerFileSize, size_t(0x158)}) mutate(Bytes(size), [&](const Bytes& d) {
    PlayerFile p; parse_player_file(d, p); SaveHeader h; parse_save_header(d, h);
  });
  // Complete empty save: ten pool counts, two selection levels, UI build block and daylight.
  Bytes original_save(0x158 + 28 + 40 + 4520 + 8);
  SaveGame decoded_save; CHECK(parse_savegame(original_save, decoded_save));
  mutate(original_save, [&](const Bytes& d) {
    SaveGame g; SaveError error;
    if (parse_savegame(d, g, &error)) CHECK(g.bytes == d && g.sections.size() == 12);
    else CHECK(g.bytes.empty() && g.sections.empty() && !error.message.empty());
  });
  PlayerFile player; file.put(serialize_player_file(player)); CHECK(load_player_file(file.path, player));
  SaveHeader save; file.put(Bytes(0x158)); CHECK(load_save_header(file.path, save));
  Bytes wav(48); std::copy_n("RIFF", 4, wav.begin()); std::copy_n("WAVEfmt ", 8, wav.begin() + 8);
  word(wav, 16, 16); word(wav, 20, 1, 2); word(wav, 22, 1, 2); word(wav, 24, 22050); word(wav, 34, 16, 2);
  std::copy_n("data", 4, wav.begin() + 36); word(wav, 40, 4);
  WavInfo wi; CHECK(parse_wav(wav.data(), wav.size(), wi));
  mutate(wav, [&](const Bytes& d) { WavInfo w; parse_wav(d.data(), d.size(), w); });
  file.put(archive(0x460, wav)); std::vector<MusicTrack> tracks; CHECK(read_music_table(file.path, tracks));
  Bytes track; CHECK(tracks.size() == 1); CHECK(read_music_track(file.path, tracks[0], track));
  for (uint64_t n : {uint64_t(0), uint64_t(0x80000000), UINT64_MAX}) {
    CHECK(!read_music_track(file.path, {1, UINT64_MAX, n}, track));
    if (n) CHECK(!read_music_track(file.path, {1, 0, n}, track));
  }
  // Reject oversized declarations even when their payload is absent.
  Bytes huge = raw; word(huge, 4, UINT32_MAX);
  CHECK(!decode_sprite({huge.data(), huge.size()}, sprite));
  huge[8] = 0x40; CHECK(!decode_alpha_sprite({huge.data(), huge.size()}, as));
  Bytes idle = rle; idle.resize(64); std::fill(idle.begin() + 32, idle.end(), 0);
  CHECK(!decode_sprite({idle.data(), idle.size()}, sprite));
  idle[8] = 0x40; CHECK(!decode_alpha_sprite({idle.data(), idle.size()}, as));
  for (const char* text : {"[Globals]\nTowns=1\n[Town0]\nUnits=2147483647",
      "[Globals]\nSides=1\n[Side0]\nDefenceCount=-1",
      "[Globals]\nSides=1\n[Side0]\nCamerasCount=2147483647",
      "[Globals]\n[Paths]\nCount=2147483647",
      "[Globals]\nNotes=16384\nNotesr=1"}) {
    CHECK(ini.parse(text)); CHECK(!parse_mp(ini, mission));
  }
  RuntimeSettings settings;
  CHECK(!parse_runtime_settings("resolution=999999999999999999999999x1", settings));
  CHECK(!parse_runtime_settings("mouse_speed=-99999999999999999999999", settings));
  const std::string oversized(1024 * 1024 + 1, 'a');
  CHECK(!ini.parse(oversized)); CHECK(!strings.parse(oversized));
  CHECK(!script::parse_script(oversized, sc)); CHECK(parse_briefing(oversized).empty());
  CHECK(cp1250_to_utf8(oversized).empty()); CHECK(utf8_to_cp1250(oversized).empty());
  file.put(bytes(oversized)); CHECK(!ini.load(file.path)); CHECK(!script::load_script(file.path, sc));
  file.put({}); std::filesystem::resize_file(file.path, 256 * 1024 * 1024 + 1);
  CHECK(!a.open(file.path)); Bytes loaded; CHECK(!read_file(file.path, loaded));
  CHECK(music_allowed_mask(INT_MIN, 0, UINT32_MAX) == 0);
  for (int id : {INT_MIN, -1, 0, INT_MAX}) { ugi_name(id); music_allowed_mask(id, 0, UINT32_MAX); }
  std::printf("Robustness: %zu deterministic mutations passed\n", cases);
}
