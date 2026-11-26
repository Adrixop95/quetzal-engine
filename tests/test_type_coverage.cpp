// Public rules, invented maps and animation frames only. No original installation.
#include "core/endian.hpp"
#include "engine/assets/anim_provider.hpp"
#include "engine/data/effects.hpp"
#include "engine/data/type_anims.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/sim/savegame.hpp"
#include "engine/world/object_state.hpp"
#include "engine/world/type_info.hpp"
#include "temp_dir.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace aztec::engine;
static int current_type = -1;
static void check(bool ok, const std::string& message) {
  if (!ok) throw std::runtime_error("type " + std::to_string(current_type) + ": " + message);
}

static World scene() {
  World w; w.mission.file_name = "synthetic-type-coverage";
  w.terrain.w = w.terrain.h = 32; w.terrain.cells.resize(1024);
  w.resources = {32,32,std::vector<uint16_t>(1024,0x8080)};
  w.relation[0][1] = w.relation[1][0] = kEnemy;
  return w;
}
static void add(World& w, int type, int x = 400, int y = 400, int side = 0) {
  Object o; o.id = uint32_t(w.objects.size() + 1); o.type = type; o.side = side;
  o.tactic = int(o.id); o.attack_mode = AttackMode::Direct;
  o.teleport(to_fixed(x),to_fixed(y)); w.objects.push_back(o);
}
static void tick(GameSim& sim, World& w, int count = 1) {
  while (count-- > 0) { w.begin_tick(); sim.tick(w); }
}
static void command(GameSim& sim, Pct type, int a, int b = 0, int c = 0) {
  Command cmd; cmd.type = type; cmd.ids = {1}; cmd.a = a; cmd.b = b; cmd.c = c;
  sim.submit(cmd);
}
static void saved(const World& initial, World& w, GameSim& sim,
                  const std::shared_ptr<RulesData>& rules, const std::string& script = "") {
  SaveData save; save.mission = initial.mission.file_name; save.tick = sim.tick_count();
  save.hash = sim.state_hash(w); save.log = sim.log(); save.script_hash = file_hash(script);
  SaveData decoded; check(deserialize_save(serialize_save(save),decoded), "save container round trip");
  World restored = initial; std::string error;
  auto replay = replay_save(decoded,restored,rules,0,script,error);
  check(bool(replay), "save restore: " + error);
  check(replay->state_hash(restored) == sim.state_hash(w), "saved state hash");
  for (int n = 0; n < 3; ++n) {
    tick(sim,w); tick(*replay,restored);
    check(replay->state_hash(restored) == sim.state_hash(w), "saved continuation");
  }
}

// The same invented finite sequence in every slot makes marker timing testable
// without copying animation data. Resource IDs still come from loaded descriptors.
static std::string animations(const TempDir& dir) {
  using Bytes = std::vector<uint8_t>;
  auto put = [](Bytes& b, size_t at, uint32_t value) { aztec::write_le(b.data() + at,value); };
  constexpr int count = 500;
  Bytes bank(20 + count * 4);
  put(bank,0,1); put(bank,4,0x515); put(bank,8,20); put(bank,16,count);
  for (int id = 0; id < count; ++id) {
    put(bank,20 + id * 4,uint32_t(bank.size()));
    Bytes animation(0x208 + 34 * 32);
    std::memcpy(animation.data(),"Anim",4); put(animation,4,3);
    for (int slot = 0; slot < 128; ++slot) put(animation,8 + slot * 4,1);
    for (int frame = 0; frame < 32; ++frame) {
      const size_t offset = 0x208 + (1 + frame) * 32;
      put(animation,offset,uint32_t(id * 64 + frame + 1));
      put(animation,offset + 12,frame == 1); put(animation,offset + 28,1);
    }
    bank.insert(bank.end(),animation.begin(),animation.end());
  }
  auto path = dir.path() / "invented-types.xbe";
  std::ofstream out(path,std::ios::binary);
  out.write(reinterpret_cast<const char*>(bank.data()),std::streamsize(bank.size()));
  check(bool(out),"write invented animation bank"); return path.string();
}

static void descriptors(const RulesData& rules, const std::string& expected) {
  const std::map<std::string,int TypeRules::*> members = {
    {"kind",&TypeRules::kind},{"price",&TypeRules::price},{"time",&TypeRules::time},
    {"dev_level",&TypeRules::dev_level},{"anim_id",&TypeRules::anim_id},{"hp_max",&TypeRules::hp_max},
    {"secondary_hp",&TypeRules::secondary_hp},{"regen",&TypeRules::regen},
    {"regen_alt_threshold",&TypeRules::regen_alt_threshold},{"physical_armor",&TypeRules::physical_armor},
    {"auxiliary_armor",&TypeRules::auxiliary_armor},{"blast_multiplier",&TypeRules::blast_multiplier},
    {"weapon_id",&TypeRules::weapon_id},{"vision_mask",&TypeRules::vision_mask},{"speed",&TypeRules::speed},
    {"cruise_height",&TypeRules::cruise_height},{"turret_anim_id",&TypeRules::turret_anim_id},
    {"turret_variant_index",&TypeRules::turret_variant_index},{"turn_rate",&TypeRules::turn_rate},
    {"spawn_sound_list_id",&TypeRules::spawn_sound_list_id},{"death_sound_list_id",&TypeRules::death_sound_list_id},
    {"engine_sound_list_id",&TypeRules::engine_sound_list_id},{"attack_sound_list_id",&TypeRules::attack_sound_list_id},
  };
  std::ifstream input(expected); check(bool(input),"open JSON expectations");
  std::string line; std::set<int> seen; int recipes = 0;
  while (std::getline(input,line)) {
    std::istringstream row(line); std::string first; row >> first;
    if (first == "recipe") {
      int race, level, type, construct; row >> race >> level >> type >> construct;
      current_type = type; const auto& list = rules.build_list(race,level);
      const auto it = std::find_if(list.begin(),list.end(),[&](const auto& item) { return item.type == type; });
      check(it != list.end(),"JSON build recipe missing from loaded tree");
      check(it->construct == bool(construct),"JSON construction flag");
      std::string need; size_t index = 0;
      while (row >> need && need != "-") {
        check(index < it->prereq.size(),"missing prerequisite");
        const auto& loaded = it->prereq[index++];
        check(loaded.type == std::stoi(need) && loaded.side_wide == (need.back() == 'A'),"prerequisite scope");
      }
      check(index == it->prereq.size(),"extra prerequisite"); ++recipes;
    } else {
      current_type = std::stoi(first); seen.insert(current_type);
      std::string member; int value; row >> member >> value;
      const int actual = rules.type(current_type).*members.at(member);
      check(actual == value,member + " expected " + std::to_string(value) + " got " + std::to_string(actual));
    }
  }
  int loaded = 0;
  for (int race = 0; race < 3; ++race) for (int level = 0; level < 4; ++level)
    loaded += int(rules.build_list(race,level).size());
  check(seen.size() == kNumTypes && loaded == recipes,"complete JSON/CSV coverage");
}

static void placement_and_movement(const std::shared_ptr<RulesData>& rules, const AnimSpriteProvider& sprites, int type) {
  const auto& r = rules->type(type);
  World initial = scene(); add(initial,type);
  if (type == 88) initial.objects[0].source = ObjectSource::Johnka;
  World w = initial; GameSim sim(rules); sim.init(w);
  check(w.objects[0].hp_max == r.hp_max && w.objects[0].secondary_hp == r.secondary_hp,"placed health");
  const auto state = object_sprite_state(w.objects[0],0);
  check(state.type_id == type && state.max_hp == r.hp_max,"sprite state descriptor identity");
  const auto parts = sprites.parts(state);
  if (type == 89) check(parts.empty(),"camera has no invented body sprite");
  else {
    check(!parts.empty() && parts.front().key.index / 64 == uint32_t(r.anim_id),"descriptor body sprite");
    if (r.turret_anim_id) check(parts.back().key.index / 64 == uint32_t(r.turret_anim_id),"descriptor turret sprite");
  }
  if (r.kind == 0 || r.kind == 7) {
    const auto frame = rules->attack_frame(type,0,1,0);
    check(frame.known && frame.active && frame.fire,"descriptor animation lookup");
  }
  // Scripted vessels have their own movement controllers; the ferry ignores Move.
  if (type != 88) command(sim,Pct::Move,700,400);
  tick(sim,w,12);
  const bool should_move = r.kind == 0 || r.kind == 1 || r.kind == 3 || r.kind == 4 ||
      r.kind == 5 || r.kind == 6 || r.kind == 9 || r.kind == 10;
  check((w.objects[0].cur_x != to_fixed(400) || w.objects[0].cur_y != to_fixed(400)) == should_move,"movement class");
  check((w.objects[0].mover >= 0) == (should_move && !r.flier && type != 88),"movement agent class");
  saved(initial,w,sim,rules);
}

static void weapon_ids(const WeaponRules& weapon, std::set<int>& ids) {
  ids.insert(weapon.id); ids.insert(weapon.payload_id);
  for (const auto& child : weapon.components) weapon_ids(child,ids);
}

static void fire_types(const std::shared_ptr<RulesData>& rules, int type) {
  const auto& r = rules->type(type);
  World initial = scene(); add(initial,type);
  add(initial,type == 81 || type == 60 ? 33 : 0,type == 85 ? 420 : 440,400,1);
  initial.objects[0].hp = initial.objects[1].hp = 1000000;
  initial.objects[1].cooldown = 10000;
  if (type == 88) initial.objects[0].source = ObjectSource::Johnka;
  World w = initial; GameSim sim(rules); sim.init(w);
  int launches = 0, hits = 0; std::set<int> visuals;
  std::set<int> allowed; weapon_ids(r.weapon,allowed);
  sim.set_combat_observer([&](uint64_t, const char* event, const Effect& e, uint32_t victim, int before, int after) {
    if (e.source != 1) return;
    if (std::strcmp(event,"launch") == 0) {
      ++launches; visuals.insert(e.anim);
      if (e.weapon_id >= 0) check(allowed.count(e.weapon_id) != 0,"descriptor projectile identity");
    }
    if (std::strcmp(event,"damage") == 0 && victim == 2) { ++hits; check(before > after,"positive weapon hit"); }
  });
  command(sim,Pct::Attack,2);
  tick(sim,w,3); saved(initial,w,sim,rules);
  for (int n = 0; n < 80; ++n) {
    tick(sim,w); for (const auto& e : w.effects) visuals.insert(e.anim);
  }
  check((launches > 0) == r.armed,"armed/unarmed release");
  if (r.armed) {
    check(r.weapon_id >= 0 && r.weapon.id == r.weapon_id,"loaded weapon identity");
    check(hits > 0,"weapon reaches eligible target");
    const auto& weapon = r.weapon;
    for (int anim : {weapon.anim,weapon.impact_anim,weapon.hit_anim,weapon.muzzle_anim})
      if (anim) check(visuals.count(anim) != 0,"weapon visual " + std::to_string(anim));
  }
  saved(initial,w,sim,rules);
}

static void cannon_commands(const std::shared_ptr<RulesData>& rules) {
  current_type = 87;
  World initial = scene(); add(initial,87); add(initial,0,440,400,1); add(initial,0,520,400,1);
  for (auto& o : initial.objects) o.cooldown = 100;
  World w = initial; GameSim sim(rules); sim.init(w);
  command(sim,Pct::Attack,3); tick(sim,w);
  check(w.objects[0].target == 3 && w.objects[0].order == uint8_t(Order::Attack),
        "Hopcannon accepts explicit target instead of nearest enemy");
  saved(initial,w,sim,rules);
}

static void damage_and_death(const std::shared_ptr<RulesData>& rules, int type, const std::string& script) {
  const auto& r = rules->type(type);
  World initial = scene(); add(initial,1); add(initial,type,460,400,1);
  initial.objects[0].hp = initial.objects[1].hp = 1000000;
  initial.objects[1].cooldown = 10000;
  World w = initial; GameSim sim(rules); sim.init(w);
  const auto expected = rules->attack(1,type); int hits = 0;
  sim.set_combat_observer([&](uint64_t, const char* event, const Effect& e, uint32_t victim, int before, int after) {
    if (std::strcmp(event,"damage") == 0 && e.source == 1 && victim == 2) {
      ++hits; check(before - after == expected.damage,"armour/category damage at callback boundary");
    }
  });
  command(sim,Pct::Attack,2); tick(sim,w,30);
  const bool damageable = !r.service_damage_protected && type != 84 && type != 88 && expected.damage > 0;
  check((hits > 0) == damageable,"damage/protection class");
  check(w.objects[1].secondary_hp == r.secondary_hp,"damage preserves secondary HP");
  saved(initial,w,sim,rules);

  // A script kill is replayable, including deaths of invulnerable scripted types.
  initial = scene(); add(initial,type); initial.objects[0].tactic = 5;
  w = initial; GameSim dying(rules); check(dying.attach_script(script),"death script"); dying.init(w);
  tick(dying,w,2); saved(initial,w,dying,rules,script);
  tick(dying,w,3); // time 6 latches on tick 7, then executes on tick 8
  check(!w.objects[0].alive && w.objects[0].hp == 0,"death transition");
  auto effect = [&](int anim) { return std::count_if(w.effects.begin(),w.effects.end(),[&](const auto& e) { return e.anim == anim; }); };
  const bool vehicle = r.kind == 1 || r.kind == 3 || r.kind == 4 || r.kind == 5 || r.kind == 6 || r.kind == 9;
  check(effect(aztec::data::kVehicleExplosionAnim) == int(vehicle),"vehicle death explosion");
  check(effect(aztec::data::kVehicleDebrisAnim) == int(vehicle && !r.flier),"ground vehicle debris");
  check(effect(aztec::data::kMarineDeathExplosionAnim) == int(type == 82),"Abdula death effect");
  check(effect(116) == int(r.structure),"unknown-radius rubble fallback");
  if (r.kind == 0) check(w.objects[0].anim_state == uint16_t(aztec::data::AnimState::Die) && !w.objects[0].removed,"Marine death animation");
  saved(initial,w,dying,rules,script);
}

static void armour_channels(const std::shared_ptr<RulesData>& rules, int type) {
  const auto& target = rules->type(type);
  // Tuzem's arrow 31 has zero category base and a 7000 physical term.
  // The flame stream applies the target's fire multiplier on each damage update.
  for (int source : {29,35}) {
    World initial = scene(); add(initial,source); add(initial,type,460,400,1);
    initial.objects[0].hp = initial.objects[1].hp = 1000000;
    initial.objects[1].cooldown = 10000;
    World w = initial; GameSim sim(rules); sim.init(w); int hits = 0;
    const int expected = target.flier ? 0 : source == 29 ?
        int(int64_t(7000) * target.physical_armor / 256) :
        int(int64_t(2000) * target.blast_multiplier / 256);
    sim.set_combat_observer([&](uint64_t, const char* event, const Effect& e, uint32_t victim, int before, int after) {
      if (e.source == 1 && victim == 2 && std::strcmp(event,"damage") == 0) {
        ++hits; check(before - after == expected,"physical/fire armour from " + std::to_string(source) +
            " expected " + std::to_string(expected) + " got " + std::to_string(before - after));
      }
    });
    command(sim,Pct::Attack,2); tick(sim,w,40);
    const bool protected_target = target.service_damage_protected || type == 84 || type == 88;
    check((hits > 0) == (expected > 0 && !protected_target),"physical/fire armour eligibility");
    saved(initial,w,sim,rules);
  }
}

static void production(const std::shared_ptr<RulesData>& rules) {
  std::set<int> produced; int recipes = 0;
  for (int race = 0; race < 3; ++race) for (int level = 0; level < 4; ++level) {
    for (const auto& recipe : rules->build_list(race,level)) {
      current_type = recipe.type; ++recipes;
      World initial = scene(); initial.mission.sides.push_back(SideDef{});
      TownDef town; town.race = race; town.type = level; town.x = town.y = 12;
      initial.mission.towns.push_back(town);
      add(initial,rules->town_building(level,race),600,600);
      initial.objects[0].town_hall = true; initial.objects[0].town = town.index;
      int n = 0;
      for (const auto& need : recipe.prereq) {
        add(initial,need.type,200 + n++ * 200,200);
        initial.objects.back().town = town.index;
      }
      initial.side_state.resize(8); initial.side_state[0].money = 1000000;
      World w = initial; GameSim sim(rules); sim.init(w);
      MenuItem offer; bool found = false;
      for (const auto& item : sim.menu(w,0,0)) if (item.type == recipe.type) { offer = item; found = true; }
      check(found,"production menu recipe");
      int upgrade = -1;
      for (int l = 0; l < 4; ++l) if (rules->town_building(l,race) == recipe.type) upgrade = l;
      if (upgrade >= 0 && upgrade <= level) { check(!offer.enabled,"downgrade blocked"); continue; }
      check(offer.enabled,"recipe enabled: " + std::string(offer.why_not));
      for (size_t index = 0; index < recipe.prereq.size(); ++index) {
        auto& prerequisite = w.objects[index + 1];
        auto enabled = [&]() {
          for (const auto& item : sim.menu(w,0,0)) if (item.type == recipe.type) return item.enabled;
          return false;
        };
        prerequisite.removed = true; check(!enabled(),"missing prerequisite blocks recipe");
        prerequisite.removed = false;
        prerequisite.town = town.index + 1;
        check(enabled() == recipe.prereq[index].side_wide,"town-local/side-wide prerequisite gate");
        prerequisite.town = town.index;
      }
      int x = -1, y = -1;
      if (recipe.construct) {
        for (int ty = 6; ty < 20 && x < 0; ++ty) for (int tx = 6; tx < 20; ++tx)
          if (sim.can_place(w,0,recipe.type,tx,ty)) { x = tx; y = ty; break; }
        check(x >= 0,"legal construction site");
      }
      command(sim,recipe.construct ? Pct::PlaceBuilding : Pct::Build,recipe.type,x,y);
      tick(sim,w,2); check(!w.side_state[0].queue.empty(),"production admitted");
      saved(initial,w,sim,rules);
      tick(sim,w,offer.time + 3);
      check(w.side_state[0].queue.empty(),"production completes");
      const Object* result = upgrade >= 0 ? &w.objects[0] : &w.objects.back();
      const auto& r = rules->type(recipe.type);
      check(result->type == recipe.type && result->progress == 1000 && result->alive,"produced type");
      check(result->hp_max == r.hp_max && result->secondary_hp == r.secondary_hp,"produced descriptor health");
      if (upgrade >= 0) check(w.mission.towns[0].type == upgrade,"hall development");
      saved(initial,w,sim,rules); produced.insert(recipe.type);
    }
  }
  std::cout << "Production: " << recipes << " recipes checked, " << produced.size() << " distinct types completed\n";
}

int main(int argc, char** argv) {
  try {
    check(argc == 3,"expected RULES_DIRECTORY JSON_EXPECTATIONS");
    auto rules = std::make_shared<RulesData>(); check(rules->load_dir(argv[1]),"load rules");
    descriptors(*rules,argv[2]); TempDir dir;
    const auto bank = animations(dir); AnimSpriteProvider sprites;
    check(rules->load_combat_animations(bank) && sprites.open(bank),"load invented animations");
    const auto script = (dir.path() / "death.scn").string();
    std::ofstream(script) << "trigger(0) conditions globalTime == 6 actions side(0).tactic(5).kill trigger(0).disable end\n";
    for (int type = 0; type < kNumTypes; ++type) {
      current_type = type; placement_and_movement(rules,sprites,type); fire_types(rules,type);
      damage_and_death(rules,type,script);
      armour_channels(rules,type);
    }
    cannon_commands(rules); production(rules);
    std::cout << "All 90 types: descriptors, placement, movement, weapons, damage, death and saves checked\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
