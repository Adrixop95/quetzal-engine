// Invented descriptor records only; no extracted tables or game installation.
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>
#include "engine/sim/game_sim.hpp"
#include "engine/world/type_info.hpp"
#include "temp_dir.hpp"

using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)

struct Layout {
  int kind; const char* family;
  int words, hp, secondary, regen, cap, fire, physical, weapon, speed, sight, turn;
};
static constexpr Layout layouts[] = {
  {0,"marine",19,1,4,2,3,12,13,7,5,6,-1},
  {1,"spider",19,3,6,4,5,14,15,9,7,8,13},
  {2,"building",322,1,-1,-1,-1,56,57,-1,-1,55,-1},
  {3,"zeppelin",17,1,4,2,3,12,13,8,5,7,-1},
  {4,"truck",17,1,4,2,3,11,12,7,5,6,-1},
  {5,"bird",22,1,4,2,3,14,15,9,5,8,6},
  {6,"headtruck",20,3,6,4,5,14,15,9,7,8,13},
  {7,"defence",68,3,-1,-1,-1,64,65,59,-1,58,4},
  {9,"cannon",16,2,5,3,4,11,12,7,-1,6,1},
};

static void write_record(const TempDir& dir, const Layout& l, int auxiliary, int unknown) {
  std::vector<int> words(size_t(l.words));
  auto set = [&](int index, int value) { if (index >= 0) words[size_t(index)] = value; };
  set(0, 91); set(l.hp, 1000 + l.kind); set(l.secondary, 700 + l.kind);
  set(l.regen, 7); set(l.cap, 200); set(l.fire, 128);
  set(l.physical, 33 + l.kind); set(l.physical + 1, auxiliary);
  set(l.weapon, 101); set(l.speed, 3); set(l.sight, 65); set(l.turn, 17);
  if (l.kind == 1 || l.kind == 6 || l.kind == 7) { set(1, 3); set(2, 92); }
  if (l.kind == 0 || l.kind == 5) {
    const int first = l.kind == 0 ? 15 : 17;
    set(first, 51); set(first + 1, 52); set(first + 2, 53);
    if (l.kind == 5) { set(13, 0x101); set(20, 54); }
  } else if (l.kind == 1 || l.kind == 6) {
    set(17, 51); if (l.kind == 6) set(18, 55);
  } else if (l.kind == 4 || l.kind == 9) {
    set(14, 51); if (l.kind == 4) set(15, 55);
  } else if (l.kind == 3) set(15, 51);
  if (l.kind == 2 || l.kind == 7) {
    int rect = l.kind == 2 ? 2 : 5, mask = l.kind == 2 ? 6 : 9;
    for (int i = 0; i < 4; ++i) set(rect + i, i < 2 ? -19 : 23);
    set(mask + 24, 2); set(mask + 25, 1);
  }
  if (l.kind == 7) set(63, unknown); // G: must not become projectile height
  if (l.kind == 2) {
    set(59, 0x101); set(60, 21); set(61, 22); set(62, 23);
    set(63, 1); set(64, 2); set(65, 0); set(66, 4); set(320, 9);
  }
  words.back() = unknown; // Guest name-pointer noise must not affect rules.
  std::ofstream out(dir.path() / (std::string("desc_") + l.family + ".csv"));
  out << "parameter,synthetic words\n41";
  for (int word : words) out << ',' << word;
  out << '\n';
}

int main() {
  TempDir dir;
  std::vector<int> ids;
  {
    std::ofstream types(dir.path() / "types.csv");
    types << "id,name,kind,param,price,time,development\n";
    for (const auto& l : layouts) {
      int id = 0;
      while (id < kNumTypes && aztec::engine::type_info(id)->kind != l.kind) ++id;
      CHECK(id < kNumTypes); ids.push_back(id);
      types << id << ",SYNTHETIC,kind,41,37,11,2\n";
      write_record(dir, l, 991, 123456);
    }
    std::ofstream(dir.path() / "weapons_ids.csv") << "id,kind,param\n101,2,0\n";
    std::ofstream(dir.path() / "weapon_class_w2_13.csv") <<
        "parameter,synthetic words\n0,6,9,256,0,77,256,256,256,256,256\n";
  }
  auto rules = std::make_shared<RulesData>();
  CHECK(rules->load_dir(dir.path().string()));
  for (size_t i = 0; i < ids.size(); ++i) {
    const auto& l = layouts[i]; const auto& t = rules->type(ids[i]);
    CHECK(t.hp_max == 1000 + l.kind && t.hp_src == Src::Verified);
    CHECK(t.secondary_hp == (l.secondary < 0 ? 0 : 700 + l.kind));
    CHECK(t.physical_armor == 33 + l.kind && t.auxiliary_armor == 991);
    CHECK(t.blast_multiplier == 128 && t.anim_id == 91 && t.vision_mask == 65);
    CHECK(t.price == 37 && t.time == 11 && t.cost_src == Src::Verified);
    if (l.speed >= 0) CHECK(t.speed == 3 * 256 && t.speed_src == Src::Verified);
    if (l.turn >= 0) CHECK(t.turn_rate == 17);
    if (l.regen >= 0) {
      CHECK(regenerated_hp(t, 199, false) == 206);
      CHECK(regenerated_hp(t, 200, false) == 200);
      CHECK(regenerated_hp(t, 200, true) == 207);
    }
    if (l.weapon >= 0) {
      CHECK(t.range_px == 77 && t.reload == 6 && t.range_src == Src::Verified);
      CHECK(rules->attack(ids[i], ids[i]).damage == 42 + l.kind);
    }
    if (l.kind == 1 || l.kind == 6 || l.kind == 7)
      CHECK(t.turret_anim_id == 92 && t.turret_variant_index == 3);
    if (l.kind != 2 && l.kind != 7) CHECK(t.spawn_sound_list_id == 51);
    if (l.kind == 0 || l.kind == 5) CHECK(t.hit_sound_list_id == 52 && t.death_sound_list_id == 53);
    if (l.kind == 4 || l.kind == 6) CHECK(t.engine_sound_list_id == 55);
    if (l.kind == 5) CHECK(t.organic_flag && t.attack_sound_list_id == 54);
    if (l.kind == 2 || l.kind == 7) {
      CHECK(t.has_footprint_mask && t.foot_w == 1 && t.foot_h == 1);
      CHECK(t.combat_bounds == (std::array<int,4>{-19,-19,23,23}));
    }
    if (l.kind == 7) CHECK(t.launch_height == 0);
    if (l.kind == 2) {
      CHECK(t.service_damage_protected && t.harvest.terrain && t.harvest.scheduled);
      CHECK(t.harvest.rate_a == 21 && t.harvest.rate_b == 22);
      CHECK(t.harvest.repair_rate == 23 && t.harvest.repair_period == 9);
      CHECK(t.harvest.dates == (std::vector<int>{0,4}));
    }
  }
  // Auxiliary accessor values, G words and heap addresses never modify damage.
  for (const auto& l : layouts) write_record(dir, l, 17, 654321);
  RulesData changed; CHECK(changed.load_dir(dir.path().string()));
  for (int id : ids) {
    CHECK(changed.type(id).auxiliary_armor == 17);
    CHECK(changed.attack(id, id).damage == rules->attack(id, id).damage);
    CHECK(changed.type(id).launch_height == 0);
  }
  World world; world.terrain.w = world.terrain.h = 20; world.terrain.cells.resize(400);
  for (size_t i = 0; i < ids.size(); ++i) {
    Object o; o.id = uint32_t(i + 1); o.type = ids[i];
    o.teleport(tile_center_fixed(int(i) + 1), tile_center_fixed(10));
    world.objects.push_back(o);
  }
  GameSim sim(rules); sim.init(world);
  for (size_t i = 0; i < ids.size(); ++i) {
    CHECK(world.objects[i].hp_max == rules->type(ids[i]).hp_max);
    CHECK(world.objects[i].secondary_hp == rules->type(ids[i]).secondary_hp);
  }
  rules->init_defaults();
  CHECK(rules->type(ids[0]).auxiliary_armor == 0 && rules->type(ids[0]).spawn_sound_list_id == 0);
  return failures ? 1 : 0;
}
