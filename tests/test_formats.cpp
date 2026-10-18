// Format self-checks against the user's own game files: test_formats GAME_DIR
#include <cstdio>
#include <string>
#include "core/archive.hpp"
#include "core/ini.hpp"
#include "core/map.hpp"
#include "core/palette.hpp"
#include "core/sprite.hpp"

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
  std::printf(failures ? "FAILED\n" : "OK\n");
  return failures ? 1 : 0;
}
