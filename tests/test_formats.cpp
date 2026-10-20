// Format self-checks against the user's own game files: test_formats GAME_DIR
#include <cstdio>
#include <string>
#include "core/archive.hpp"
#include "core/ini.hpp"
#include "core/map.hpp"
#include "core/palette.hpp"
#include "core/sprite.hpp"
#include "core/script.hpp"
#include "core/ugi.hpp"
#include "core/visfile.hpp"
#include <dirent.h>
#include "core/audio.hpp"
#include "core/mp.hpp"
#include "core/sprite_alpha.hpp"
#include "core/userfiles.hpp"

using namespace aztec;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("usage: test_formats GAME_DIR\n"); return 2; }
  std::string g = argv[1];
  for (const char* name : {"Pal", "light", "Units", "Tiles0", "Tiles1", "fnts", "common", "Snd"}) {
    Archive a;
    CHECK(a.open(g + "/Res/" + name + ".xbe"));
    CHECK(!a.groups().empty());
  }
  Archive units;
  CHECK(units.open(g + "/Res/Units.xbe"));
  size_t decoded = 0, failed = 0;
  for (const auto& grp : units.groups()) {
    if (grp.id != 0x44c && grp.id != 0x454) continue;
    for (uint32_t i = grp.lo; i < grp.hi; i++) {
      Item it = units.get(grp.id, i);
      if (!it || it.size < 0x18) continue;
      Sprite s;
      (decode_sprite(it, s) ? decoded : failed)++;
    }
  }
  std::printf("Units sprites decoded=%zu failed=%zu\n", decoded, failed);
  CHECK(decoded > 10000);
  CHECK(failed * 50 < decoded);  // tolerate <2%: variants not yet understood

  Archive pal;
  Palette p;
  CHECK(pal.open(g + "/Res/Pal.xbe"));
  CHECK(load_palette(pal, 4, 14, p));
  CHECK(!load_palette(pal, 2, 0, p));  // item 2 is empty

  Ini ini;
  TerrainMap m;
  std::string maps = g + "/Maps/";
  if (ini.load(maps + "Mission_4.mp")) {
    CHECK(ini.get("Globals", "MapName") == "Mission_4");
    CHECK(ini.get_int("Globals", "Sides") == 4);
    CHECK(load_terrain_map(maps + "Mission_4.map", m));
    CHECK(m.w == 120 && m.h == 90);
  } else {
    std::printf("skip: Maps/Mission_4.* not in game dir\n");
  }
  // .vis / .scn / .mit over every map in the game dir
  {
    CHECK(ugi_from_name("ugi_abdula") >= 0);
    int nvis = 0, nscn = 0, nmit = 0;
    if (DIR* d = opendir(maps.c_str())) {
      while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n.size() < 5 || n[0] == 'z') continue;  // z*: scratch maps of test tools
        std::string ext = n.substr(n.size() - 4), base = maps + n.substr(0, n.size() - 4);
        if (ext == ".vis") {
          VisFile v;
          bool ok = load_vis(base + ".vis", v);
          if (!ok) std::printf("vis fail %s\n", n.c_str());
          CHECK(ok); nvis += ok;
        } else if (ext == ".scn") {
          script::Script s;
          bool ok = script::load_script(base + ".scn", s);
          if (!ok) std::printf("scn fail %s %s\n", n.c_str(), s.error.c_str());
          CHECK(ok); nscn += ok;
        } else if (ext == ".mit") {
          std::vector<BriefingSection> b;
          bool ok = load_briefing(base + ".mit", b);
          CHECK(ok); nmit += ok;
        }
      }
      closedir(d);
    }
    std::printf("vis=%d scn=%d mit=%d\n", nvis, nscn, nmit);
  }
  // user files
  {
    PlayerFile pf;
    if (load_player_file(g + "/111414.plr", pf)) {
      CHECK(pf.name == "ab");
      CHECK(pf.options.frequency == 22050 && pf.options.scroll_speed == 48 && pf.options.game_speed == 83);
      CHECK(pf.options.gamma == 1.0);
      CHECK(serialize_player_file(pf) == pf.raw);  // round trip
      pf.name = "xyz"; pf.options.sfx_volume = 77;
      PlayerFile pf2;
      CHECK(parse_player_file(serialize_player_file(pf), pf2) && pf2.name == "xyz" && pf2.options.sfx_volume == 77);
    } else std::printf("skip: 111414.plr\n");
    SaveHeader sh;
    if (load_save_header(g + "/Saves/single0.sav", sh)) {
      CHECK(sh.description == "test");
      CHECK(sh.map_path.size() > 4 && sh.time[0] >= 1999);
    }
    std::vector<KeyBinding> kb;
    CHECK(key_action_id("Cheatwin") == 52 && dik_from_name("LSHIFT") == 42 && dik_from_name("space") == 57);
    if (load_keys_ini(g + "/Keys.ini", kb)) {
      CHECK(kb.size() >= 20);
      CHECK(kb[0].action == key_action_id("add") && kb[0].dik == dik_from_name("lshift"));
    }
    RuntimeSettings rs;
    CHECK(parse_runtime_settings("resolution=800x600\nwindow_mode=2\n", rs) && rs.res_w == 800 && rs.window_mode == 2);
  }
  // every .mp: full schema vs the files' own counts
  {
    int n = 0;
    if (DIR* d = opendir(maps.c_str())) {
      while (dirent* e = readdir(d)) {
        std::string nm = e->d_name;
        if (nm.size() < 4 || nm[0] == 'z' || nm.substr(nm.size() - 3) != ".mp") continue;
        MpFile mp;
        bool ok = load_mp(maps + nm, mp);
        CHECK(ok);
        if (!ok) continue;
        Ini ini; ini.load(maps + nm);
        CHECK(int(mp.sides.size()) == ini.get_int("Globals", "Sides"));
        CHECK(int(mp.towns.size()) == ini.get_int("Globals", "Towns"));
        CHECK(int(mp.teams.size()) == ini.get_int("Globals", "Teams"));
        for (const auto& t : mp.towns) CHECK(int(t.units.size()) == ini.get_int("Town" + std::to_string(t.index), "Units"));
        for (const auto& t : mp.teams) CHECK(int(t.units.size()) == ini.get_int("Team" + std::to_string(t.index), "Units"));
        CHECK(mp.map_w > 0 && mp.map_h > 0);
        n++;
      }
      closedir(d);
    }
    std::printf("mp=%d\n", n);
    CHECK(n >= 30);
    MpUnit u;
    CHECK(parse_unit_line(" 99, 14, 30,28000", u) && u.x == 99 && u.hp == 28000);
  }
  // flag 0x40 sprites (indexed + alpha) in common/fnts/Units
  {
    for (const char* name : {"common", "fnts", "Units"}) {
      Archive a;
      if (!a.open(g + "/Res/" + name + ".xbe")) continue;
      size_t ok = 0, bad = 0;
      for (const auto& grp : a.groups()) {
        if (grp.id != 0x44c && grp.id != 0x454) continue;
        for (uint32_t i = grp.lo; i < grp.hi; i++) {
          Item it = a.get(grp.id, i);
          if (!it || it.size < 0x18 || !(it.data[8] & 0x40)) continue;
          AlphaSprite s;
          (decode_alpha_sprite(it, s) ? ok : bad)++;
        }
      }
      std::printf("alpha sprites %s ok=%zu bad=%zu\n", name, ok, bad);
      CHECK(bad == 0 && ok > 0);
    }
  }
  // audio
  {
    SoundBank sb;
    if (sb.open(g + "/Res/Snd.xbe")) {
      int ok = 0, bad = 0;
      for (uint32_t id = 1; id < 2014; id++) {
        WavInfo w;
        Item it = sb.wav(id);
        if (!it) continue;  // zero-length items = unused ids
        if (parse_wav(it.data, it.size, w) && w.pcm_bytes > 0 && (w.bits == 8 || w.bits == 16)) ok++; else bad++;
      }
      std::printf("wav ok=%d bad=%d\n", ok, bad);
      CHECK(ok == 367 && bad <= 1);  // id 2013 (table end sentinel) is not a sound
      auto set = sb.sound_set(1);
      CHECK(!set.empty() && set[0] == 1000);
      int inrange = 0, total = 0;
      for (uint32_t k = 1; k < 433; k++) for (uint32_t id : sb.sound_set(k)) { total++; inrange += sb.wav(id) ? 1 : 0; }
      std::printf("sound set entries %d resolvable %d\n", total, inrange);
      CHECK(inrange * 100 > total * 95);
      CHECK(sb.subtitle(1202).size() > 3);
    }
    std::vector<MusicTrack> mt;
    if (read_music_table(g + "/Music/music.xbe", mt)) {
      CHECK(mt.size() >= 6);
      std::vector<uint8_t> hdr;
      MusicTrack head = mt[0]; 
      WavInfo w;
      head.size = 44;
      CHECK(read_music_track(g + "/Music/music.xbe", head, hdr) && parse_wav(hdr.data(), hdr.size(), w) && w.rate == 44100 && w.channels == 2);
    }
    CHECK(music_allowed_mask(0, 0xF3FF, 0xFFF) == 0x3FF);
    CHECK(music_allowed_mask(3, 0xF3FF, 0xFFF) == 4);
  }
  std::printf(failures ? "FAILED\n" : "OK\n");
  return failures ? 1 : 0;
}
