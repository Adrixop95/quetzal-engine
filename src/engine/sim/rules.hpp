// Static gameplay data for the simulation, loaded at run time from the rules agent's CSV dumps (docs/data/*.csv).
// Every value carries a provenance tag; docs/engine_rules_status.md lists which rules are verified.
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace aztec::engine {

enum class Src : uint8_t { Verified, Measured, Placeholder };

constexpr int kDayTicks = 7200;

struct HarvestRule {
  int rate_a = 0, rate_b = 0;  // descriptor d60/d61; resource channel names remain unresolved
  bool terrain = false, scheduled = false;
  int period = -1, repair_rate = 0, repair_period = -1;
  std::array<int, 49> occupancy{};  // descriptor cells equal to 2 contribute
  std::vector<int> dates;
};

struct HarvestState {
  int64_t accumulated = 0;
  int last_clock = 0;
  size_t cursor = 0;
};

// Accumulate each active tick; an event harvests rounded 8.8 units, retaining sub-half residue.
int harvest_tick(HarvestState& state, int rate, int clock, int period,
                 const std::vector<int>* dates = nullptr);
int production_value(int base, int hall_level, int speed, bool computer, bool hostile_all, bool time);

struct TypeRules {
  HarvestRule harvest;
  int kind = -1;                 // class kind (type_info)
  bool structure = false, mobile = false, flier = false, armed = false, large = false;
  int hp_max = 0;     Src hp_src = Src::Placeholder;      // Marine desc d4 (verified), else largest mission hp
  int speed = 0;      Src speed_src = Src::Placeholder;   // fixed 24.8 px per tick; measured (speed_measured.csv) or Marine d5
  int radius_px = 14; Src radius_src = Src::Placeholder;
  int sight_px = 288; Src sight_src = Src::Placeholder;
  int range_px = 56;  Src range_src = Src::Placeholder;
  int dmg = 500, reload = 12; Src dmg_src = Src::Placeholder;  // default shot when no duel measurement matches
  int price = 0, time = 0; Src cost_src = Src::Placeholder;    // types.csv (price/time verified from the UI tooltip function)
  int foot_w = 1, foot_h = 1; Src foot_src = Src::Placeholder; // footprint in tiles (Building desc d4/d5 px, rounded up)
};

struct Prerequisite { int type = 0; bool side_wide = false; };

struct BuildItem {
  int type = 0;
  bool construct = false;        // true: building placed on the map, false: unit trained
  std::vector<Prerequisite> prereq;  // A suffix selects side-wide completed counts
};

struct DuelEntry { int dmg = 0, interval = 0; };

// Decimal text -> value*256 using integer arithmetic only (locale independent), rounded half up, bounded.
int parse_fixed256(const std::string& text);

class RulesData {
 public:
  // Loads from a directory containing types.csv, desc_*.csv, speed_measured.csv, duel_damage.csv, buildtree.csv.
  // Missing files are tolerated (placeholders are used). Returns true if at least types.csv was read.
  bool load_dir(const std::string& dir);
  // Built-in defaults only (tests, missing data).
  void init_defaults();
  const TypeRules& type(int id) const { return id >= 0 && id < int(types_.size()) ? types_[id] : dummy_; }
  bool has_duel(int attacker, int target, DuelEntry& out) const;
  // Buildable items for a race (0 Rus, 1 Aztecs, 2 China) and town hall type (0..3).
  const std::vector<BuildItem>& build_list(int race, int townhall) const;
  int town_building(int townhall, int race) const;   // townhall_ids.csv, -1 if none
  bool loaded() const { return loaded_; }

 private:
  std::vector<TypeRules> types_;
  std::map<std::pair<int, int>, DuelEntry> duel_;
  std::map<std::pair<int, int>, std::vector<BuildItem>> tree_;
  int town_ids_[4][3];
  bool loaded_ = false;
  TypeRules dummy_;
  std::vector<BuildItem> empty_;
};

}  // namespace aztec::engine
