#include "engine/ui/names.hpp"

#include <cctype>
#include <cstring>

#include "engine/world/type_info.hpp"

namespace aztec::engine {

int type_name_ordinal(int id) {
  const TypeInfo* ti = type_info(id);
  if (!ti) return 0;
  struct Pair { const char* name; int ordinal; };
  static const Pair exact[] = {
      {"FOOTMAN", 198}, {"RIFLEMAN", 204}, {"DEMOLISHER", 209}, {"BEAR", 202}, {"KNIGHT", 201}, {"ARCHER", 205},
      {"MAGE", 203}, {"TUZEM", 196}, {"JAGUAR", 207}, {"BIZON", 208}, {"NINJA", 197}, {"HYPNO", 195},
      {"SNOWMAN", 199}, {"ELEFANT", 206}, {"ABDULA", 200}, {"JOHNKA", 194}, {"BOAT", 168}, {"EAGLE", 167},
      {"DELTAPLAN", 166}, {"SPIDER_AXETHROWER", 210}};
  for (const Pair& p : exact)
    if (!std::strcmp(ti->name, p.name)) return p.ordinal;
  // Buildings: same text for every race, matched by the part after RUS_/AZT_/CHI_.
  static const Pair suffix[] = {
      {"FORPOST", 169}, {"TOWN", 170}, {"FORTRESS", 171}, {"AIR_FACILITY", 172}, {"BARRACKS", 174}, {"BEARLAIR", 175},
      {"FARM", 176}, {"MINE", 177}, {"STABLE", 178}, {"VILLAGE", 179}, {"WAREHOUSE", 180}, {"JAGUARLAIR", 181},
      {"NEST", 182}, {"TEMPLE", 183}, {"WORKSHOP", 191}, {"ARMORY", 192}, {"LABS", 186}, {"PRISON", 187},
      {"HUT", 188}, {"CAVE", 189}, {"HYPNO_SCHOOL", 193}};
  if (std::strlen(ti->name) > 4 && ti->name[3] == '_')
    for (const Pair& p : suffix)
      if (!std::strcmp(ti->name + 4, p.name)) return p.ordinal;
  return 0;
}

std::string type_display_name(const StrBase& strings, int id) {
  int o = type_name_ordinal(id);
  if (o) {
    const std::string& s = strings.get(o);
    if (!s.empty()) return s;
  }
  const TypeInfo* ti = type_info(id);
  if (!ti) return "?";
  std::string n = ti->name;
  for (size_t i = 0; i < n.size(); i++) n[i] = i == 0 ? char(std::toupper(n[i])) : n[i] == '_' ? ' ' : char(std::tolower(n[i]));
  return n;
}

}  // namespace aztec::engine
