// Generated from the UGI name table of the original executable (see docs/object_types.md).
#include "core/ugi.hpp"
#include <cctype>
#include <cstring>

namespace aztec {

static const char* const kUgiNames[kUgiCount] = {
    "UGI_FOOTMAN",
    "UGI_RIFLEMAN",
    "UGI_DEMOLISHER",
    "UGI_TRUCK_AXETHROWER",
    "UGI_CANNON",
    "UGI_BEAR",
    "UGI_KNIGHT",
    "UGI_ZEPPELIN",
    "UGI_TRUCK_FLAMETHROWER",
    "UGI_TRUCK_KATUSHA",
    "UGI_RUS_FORPOST",
    "UGI_RUS_VILLAGE",
    "UGI_RUS_FARM",
    "UGI_RUS_MINE",
    "UGI_RUS_BARRACKS",
    "UGI_RUS_STABLE",
    "UGI_RUS_BEARLAIR",
    "UGI_RUS_WATCH_TOWER",
    "UGI_RUS_WAREHOUSE",
    "UGI_RUS_TOWN",
    "UGI_RUS_WORKSHOP",
    "UGI_RUS_ARMORY",
    "UGI_RUS_LABS",
    "UGI_RUS_FORTRESS_TOWER",
    "UGI_RUS_FORTRESS",
    "UGI_RUS_AIR_FACILITY",
    "UGI_RUS_CANNON_TOWER",
    "UGI_ARCHER",
    "UGI_MAGE",
    "UGI_TUZEM",
    "UGI_SPIDER_CROSSBOW",
    "UGI_JAGUAR",
    "UGI_BIZON",
    "UGI_EAGLE",
    "UGI_DELTAPLAN",
    "UGI_SPIDER_FLAMETHROWER",
    "UGI_SPIDER_AXETHROWER",
    "UGI_AZT_FORPOST",
    "UGI_AZT_VILLAGE",
    "UGI_AZT_FARM",
    "UGI_AZT_WAREHOUSE",
    "UGI_AZT_BARRACKS",
    "UGI_AZT_ARCHER_TOWER",
    "UGI_AZT_HUT",
    "UGI_AZT_STABLE",
    "UGI_AZT_JAGUARLAIR",
    "UGI_AZT_NEST",
    "UGI_AZT_TOWN",
    "UGI_AZT_MINE",
    "UGI_AZT_CROSSBOW_TOWER",
    "UGI_AZT_TEMPLE",
    "UGI_AZT_WORKSHOP",
    "UGI_AZT_AIR_FACILITY",
    "UGI_NINJA",
    "UGI_HYPNO",
    "UGI_CHIEF",
    "UGI_SNOWMAN",
    "UGI_ELEFANT",
    "UGI_WHEEL_KATUSHA",
    "UGI_WHEEL_FIRETHROWER",
    "UGI_PARAPLAN",
    "UGI_WHEEL_AXETHROWER",
    "UGI_WHEEL_CROSSBOW",
    "UGI_CHI_FORPOST",
    "UGI_CHI_VILLAGE",
    "UGI_CHI_FARM",
    "UGI_CHI_MINE",
    "UGI_CHI_BARRACKS",
    "UGI_CHI_FIRETHROW_TOWER",
    "UGI_CHI_HYPNO_TOWER",
    "UGI_CHI_WAREHOUSE",
    "UGI_CHI_TOWN",
    "UGI_CHI_HYPNO_SCHOOL",
    "UGI_CHI_PRISON",
    "UGI_CHI_WORKSHOP",
    "UGI_CHI_ARMORY",
    "UGI_CHI_FORTRESS",
    "UGI_CHI_STABLE",
    "UGI_CHI_CAVE",
    "UGI_CHI_AIR_FACILITY",
    "UGI_CHI_WATCHTOWER",
    "UGI_CHI_AIRTOWER",
    "UGI_ABDULA",
    "UGI_CATAPULT",
    "UGI_BOAT",
    "UGI_ZEPPELIN2",
    "UGI_SHIPYARD",
    "UGI_HOPCANNON",
    "UGI_JOHNKA",
    "UGI_CAMERA",
};

const char* ugi_name(int id) { return id >= 0 && id < kUgiCount ? kUgiNames[id] : nullptr; }

int ugi_from_name(std::string_view name) {
  for (int i = 0; i < kUgiCount; i++) {
    const char* n = kUgiNames[i];
    size_t len = std::strlen(n);
    if (len != name.size()) continue;
    bool eq = true;
    for (size_t k = 0; k < len && eq; k++)
      eq = std::toupper((unsigned char)n[k]) == std::toupper((unsigned char)name[k]);
    if (eq) return i;
  }
  return -1;
}

}  // namespace aztec
