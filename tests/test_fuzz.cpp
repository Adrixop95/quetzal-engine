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
#include "engine/assets/daylight.hpp"
#include "engine/assets/mission.hpp"
#include "engine/data/sprites.hpp"
#include "engine/data/light_masks.hpp"
#include "engine/sim/savegame.hpp"
#include "engine/sim/rules.hpp"
#include "engine/assets/anim_provider.hpp"
#include "engine/app/playtest.hpp"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <fstream>
#include <string>
#include "temp_dir.hpp"

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
  TempDir directory;
  std::string path = (directory.path() / "input").string();
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
    file.put(d); Sprite preview; load_preview(file.path, preview);
  });
  Bytes ar = archive(0x450, Bytes(512)); file.put(ar);
  Archive a; CHECK(a.open(file.path)); CHECK(load_palette(a, 1, 0, pal));
  mutate(ar, [&](const Bytes& d) {
    file.put(d); a.open(file.path);
    for (const auto& g : a.groups()) {
      for (uint32_t id : {g.lo, g.hi, UINT32_MAX}) { auto item = a.get(g.id, id); (void)item; }
    }
    load_palette(a, 1, UINT32_MAX, pal);
    load_palette(a, 1, 0, pal);
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
      file.put(d);
      i.load(file.path); load_mp(file.path, m); script::load_script(file.path, s);
      strings.load(file.path); std::vector<BriefingSection> brief; load_briefing(file.path, brief);
      load_keys_ini(file.path, keys);
      engine::MissionDef mission; engine::parse_mission(i, "synthetic", mission);
      engine::load_mission(file.path, mission);
      engine::PlaytestScript playtest; std::string error; playtest.load(file.path, error);
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
    file.put(d); load_player_file(file.path, p); load_save_header(file.path, h);
  });
  // Complete empty save: ten pool counts, two selection levels, UI build block and daylight.
  Bytes original_save(0x158 + 28 + 40 + 4520 + 8);
  SaveGame decoded_save; CHECK(parse_savegame(original_save, decoded_save));
  mutate(original_save, [&](const Bytes& d) {
    SaveGame g; SaveError error;
    if (parse_savegame(d, g, &error)) CHECK(g.bytes == d && g.sections.size() == 12);
    else CHECK(g.bytes.empty() && g.sections.empty() && !error.message.empty());
    file.put(d); load_savegame(file.path, g, &error);
  });
  file.put(original_save); CHECK(load_savegame(file.path, decoded_save));
  file.put(Bytes(0x158)); CHECK(!load_savegame(file.path, decoded_save));
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
  for (uint32_t group : {0x460u, 0x3e8u, 0x6u}) mutate(archive(group, wav), [&](const Bytes& d) {
    file.put(d); SoundBank bank; bank.open(file.path); bank.sound_set(1); bank.subtitle(1);
    std::vector<MusicTrack> table;
    if (read_music_table(file.path, table)) for (const auto& t : table) read_music_track(file.path, t, track);
  });
  // Engine containers are independent formats, not original simulation snapshots.
  engine::SaveData saved; saved.mission = "synthetic"; saved.tick = 2;
  saved.log.push_back({2, {engine::Pct::Move, 0, 1, 2, 0, {1, 2}}});
  const Bytes save_bytes = engine::serialize_save(saved);
  CHECK(!save_bytes.empty());
  for (size_t p : {size_t(4), size_t(60 + saved.mission.size())}) {
    Bytes bad = save_bytes; word(bad, p, UINT32_MAX);
    engine::SaveData result = saved;
    CHECK(!engine::deserialize_save(bad, result)); CHECK(result.mission == saved.mission && result.tick == saved.tick);
  }
  Bytes trailing = save_bytes; trailing.push_back(0);
  engine::SaveData result; CHECK(!engine::deserialize_save(trailing, result));
  Bytes partial = engine::serialize_log(saved.log); partial[4] = 2;
  std::vector<engine::LoggedCommand> commands = saved.log;
  CHECK(!engine::deserialize_log(partial, commands)); CHECK(engine::serialize_log(commands) == engine::serialize_log(saved.log));
  for (const char* id : {"-1", "16384", "2147483647"}) {
    CHECK(ini.parse(std::string("[Paths]\nCount=1\n0=") + id + ",1,2"));
    engine::MissionDef m; CHECK(!engine::parse_mission(ini, "synthetic", m));
  }
  mutate(save_bytes, [&](const Bytes& d) {
    engine::SaveData result;
    if (engine::deserialize_save(d, result)) CHECK(engine::serialize_save(result) == d);
    file.put(d); engine::read_save(file.path, result);
  });
  mutate(engine::serialize_log(saved.log), [&](const Bytes& d) {
    std::vector<engine::LoggedCommand> log;
    if (engine::deserialize_log(d, log)) CHECK(engine::serialize_log(log) == d);
  });
  for (uint32_t tag : {0u, 0x53u, 0x10017u}) {
    Bytes bad = engine::serialize_log(saved.log); word(bad, 16, tag);
    CHECK(!engine::deserialize_log(bad, commands)); CHECK(engine::serialize_log(commands) == engine::serialize_log(saved.log));
  }
  Bytes anim(0x208 + 3 * 32); std::copy_n("Anim", 4, anim.begin());
  word(anim, 4, 3); word(anim, 8, 1); word(anim, 0x228, 1); word(anim, 0x244, 2);
  CHECK(data::AnimSet({anim.data(), anim.size()}).frame_count(0) == 1);
  mutate(anim, [&](const Bytes& d) {
    data::AnimSet set({d.data(), d.size()});
    for (int slot : {-1, 0, 127, 128, INT_MAX}) {
      set.frame(slot, 0); set.frame(slot, INT_MAX); set.loop_sound(slot);
      set.at_tick(slot, INT64_MAX, true); set.at_tick(slot, -1, false);
    }
  });
  Bytes mask(4 + 20 * 20); word(mask, 0, 20, 2); word(mask, 2, 20, 2);
  CHECK(data::LightMask({mask.data(), mask.size()}).ok());
  mutate(mask, [&](const Bytes& d) {
    data::LightMask light({d.data(), d.size()});
    for (int x : {INT_MIN, -1, 0, 19, 20, INT_MAX}) { light.sample(x, x); light.sample_pixels(x, x); }
  });
  // Invented PE headers only; no executable instructions or original data.
  Bytes pe(256); pe[0] = 'M'; pe[1] = 'Z'; word(pe, 0x3c, 64); word(pe, 64, 0x4550);
  word(pe, 70, 1, 2); word(pe, 84, 32, 2);
  word(pe, 128, 1024); word(pe, 132, 0x4a8f68); word(pe, 136, 96); word(pe, 140, 160);
  mutate(pe, [&](const Bytes& d) { file.put(d); engine::Daylight light; light.load_from_exe(file.path); });
  Bytes daylight = pe; daylight.resize(160 + 0x3d0 + 36);
  for (size_t offset : {size_t(160), size_t(160 + 0x3d0)}) {
    word(daylight, offset, 7200); word(daylight, offset + 28, 10);
    // Three invented RGB doubles, each exactly 1.0 in little-endian encoding.
    for (size_t channel = 0; channel < 3; ++channel) word(daylight, offset + 8 + channel * 8, 0x3ff00000);
  }
  engine::Daylight light; file.put(daylight); CHECK(light.load_from_exe(file.path));
  file.put({}); CHECK(!light.load_from_exe(file.path)); CHECK(!light.loaded());
  // Optional CSV metadata is also external input. Keep type/parameter ids valid
  // while making every descriptor field malformed or extreme.
  const auto directory = file.directory.path();
  { std::ofstream types(directory / "types.csv"); types << "header\n0,name,0,3,1,1,0\n10,name,2,0,1,1,0\n"; }
  for (const char* value : {"0", "-1", "2147483647", "-2147483648", "999999999999999999999999", "oops", "\"1,2\""}) {
    for (const char* family : {"marine", "building"}) {
      std::ofstream descriptor(directory / (std::string("desc_") + family + ".csv"));
      descriptor << "header\n" << (std::string(family) == "marine" ? 3 : 0);
      for (int field = 0; field < 322; ++field) descriptor << ',' << value;
      descriptor << '\n';
    }
    engine::RulesData rules; const bool loaded = rules.load_dir(directory.string());
    if (std::string(value) == "2147483647" || std::string(value) == "-2147483648" || std::string(value) == "-1") CHECK(!loaded);
    engine::AnimSpriteProvider visuals; visuals.load_visual_rules(directory.string());
  }
  // Keep Marine speed valid so the extreme building dimensions are reached.
  { std::ofstream marine(directory / "desc_marine.csv"); marine << "header\n3,0,1,0,0,0,0\n"; }
  {
    std::ofstream building(directory / "desc_building.csv"); building << "header\n0";
    for (int field = 0; field < 322; ++field) building << ",2147483647";
    building << '\n';
  }
  engine::RulesData wide_rules; CHECK(wide_rules.load_dir(directory.string()));
  CHECK(wide_rules.type(10).foot_w == (int64_t(INT_MAX) + 47) / 48);
  CHECK(wide_rules.type(10).foot_h == wide_rules.type(10).foot_w);
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
  CHECK(!strings.load(file.path)); CHECK(!load_briefing(file.path, brief)); CHECK(!load_keys_ini(file.path, keys));
  engine::PlaytestScript playtest; std::string error; CHECK(!playtest.load(file.path, error));
  for (const char* name : {"types.csv", "desc_marine.csv", "desc_building.csv"}) {
    std::ofstream output(directory / name); output << oversized;
  }
  engine::RulesData rules; CHECK(!rules.load_dir(directory.string()));
  engine::AnimSpriteProvider visuals; CHECK(!visuals.load_visual_rules(directory.string()));
  file.put({}); std::filesystem::resize_file(file.path, 256 * 1024 * 1024 + 1);
  CHECK(!a.open(file.path)); Bytes loaded; CHECK(!read_file(file.path, loaded));
  CHECK(music_allowed_mask(INT_MIN, 0, UINT32_MAX) == 0);
  for (int id : {INT_MIN, -1, 0, INT_MAX}) { ugi_name(id); music_allowed_mask(id, 0, UINT32_MAX); }
  std::printf("Robustness: %zu deterministic mutations passed\n", cases);
}
