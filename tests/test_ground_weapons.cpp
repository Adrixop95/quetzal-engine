// Invented duels and groups using public rule facts; no installation or assets.
#include "engine/sim/game_sim.hpp"
#include <cstdio>
#include <memory>

using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static Object object(int id, int type, int side, int x, int y) {
  Object o; o.id = uint32_t(id); o.type = type; o.side = side; o.hp = 100000;
  o.cooldown = 1000; o.attack_mode = AttackMode::Direct;
  o.teleport(to_fixed(x), to_fixed(y));
  return o;
}
static World duel(int type, int distance) {
  World w; w.terrain.w = w.terrain.h = 30; w.terrain.cells.resize(900);
  w.relation[0][1] = w.relation[1][0] = kEnemy;
  w.objects = {object(1, type, 0, 400, 400), object(2, 0, 1, 400 + distance, 400)};
  w.objects[0].cooldown = 0;
  return w;
}
static void attack(GameSim& sim) {
  Command c; c.type = Pct::Attack; c.ids = {1}; c.a = 2; sim.submit(c);
}
static void tick(GameSim& sim, World& w) { w.begin_tick(); sim.tick(w); }

static void test_formulas(const std::shared_ptr<RulesData>& rules) {
  TypeRules t;
  for (int id : {2,30,32,40,43,46,50,5,6,22,24,26,28,3,17,18,25,29,37,38,41,42,49,53}) {
    auto shot = rules->weapon(id, t);
    CHECK(shot.damage_known && shot.getters_known);
  }
  for (int id : {21,23,27,45}) CHECK(rules->weapon(id, t).damage_known);
  CHECK(rules->weapon(0, t).damage_known && rules->weapon(0, t).damage == 0);
  CHECK(!rules->weapon(0, t).getters_known);
  CHECK(rules->weapon(24, t).damage == 3200);
  CHECK(rules->weapon(28, t).damage == 4300);
  CHECK(rules->weapon(38, t).reload == 30 && rules->weapon(38, t).range_px == 180);
  CHECK(rules->blast_damage(3, t, 52) == 3000);
  CHECK(rules->blast_damage(3, t, 76) == 1500);
  CHECK(rules->blast_damage(3, t, 77) == 1429); // ratio truncates to 8.8 first
  CHECK(rules->blast_damage(3, t, 100) == 0);
  t.blast_multiplier = 128; t.category_e8 = true;
  CHECK(rules->blast_damage(3, t, 52) == 3000); // inner hit bypasses both factors
  CHECK(rules->blast_damage(3, t, 76) == 597);
  t.flier = true;
  CHECK(rules->blast_damage(3, t, 0) == 0);
  CHECK(rules->blast_damage(18, t, 0) == 3000);
  CHECK(rules->contact_damage(24, t, false) == 0);
  t = TypeRules{}; t.nonorganic = true;
  CHECK(rules->contact_damage(24, t, false) == 412);
  CHECK(rules->contact_damage(24, t, true) == 0);
  t.category_e8 = t.category_f4 = true;
  CHECK(rules->weapon(11, t).damage == 1406); // OR predicate applies only once
}

static void test_mage_and_tower(const std::shared_ptr<RulesData>& rules) {
  for (int type : {28,49}) {
    const int contact = type == 28 ? 600 : 800, blast = type == 28 ? 2000 : 2700;
    World initial = duel(type, 100);
    initial.objects.push_back(object(3, 0, 1, 540, 400));
    initial.objects.push_back(object(4, 0, 0, 500, 430));
    initial.objects.push_back(object(5, 33, 1, 500, 360));
    initial.objects.push_back(object(6, 0, 1, 650, 400));
    World w = initial;
    GameSim sim(rules); sim.init(w); attack(sim);
    for (int n = 1; n <= 8; ++n) {
      tick(sim, w);
      if (n == (type == 49 ? 3 : 1)) {
        CHECK(w.objects[1].hp == 100000);
        CHECK(w.effects.size() == 1 && w.effects[0].weapon_id == rules->type(type).weapon_id);
        if (!w.effects.empty()) {
          auto hash = sim.state_hash(w); ++w.effects[0].weapon_id;
          CHECK(hash != sim.state_hash(w)); --w.effects[0].weapon_id;
        }
      }
      World replay_world = initial;
      auto replay = GameSim::replay(replay_world, rules, sim.log(), n);
      CHECK(replay->state_hash(replay_world) == sim.state_hash(w));
    }
    CHECK(w.objects[1].hp == 100000 - 2 * contact - blast);
    CHECK(w.objects[2].hp == 100000 - blast);
    CHECK(w.objects[3].hp == 100000 - blast); // contact safe, impact includes allies
    CHECK(w.objects[4].hp == 100000 && w.objects[5].hp == 100000);
    CHECK(w.objects[1].side == 1 && w.objects[1].secondary_hp == rules->type(0).secondary_hp);

    w = duel(type, 100); sim.init(w); attack(sim);
    for (int n = 0; n < (type == 49 ? 3 : 1); ++n) tick(sim, w);
    sim.teleport(w, 2, 900, 400);
    for (int n = 0; n < 8; ++n) tick(sim, w);
    CHECK(w.objects[1].hp == 100000); // no guaranteed damage to a fleeing target

    const int range = rules->attack(type, 0).range_px;
    // Develop's integer combat norm determines the centre-range boundary.
    int reach = 0;
    while (combat_distance(reach + 1, 0) <= range) ++reach;
    for (int outside : {0,1}) {
      w = duel(type, reach + outside); sim.init(w); attack(sim);
      for (int n = 0; n < (type == 49 ? 3 : 1); ++n) tick(sim, w);
      CHECK(w.objects[0].has_fired == (outside == 0));
    }
  }
}

static void test_demolisher(const std::shared_ptr<RulesData>& rules) {
  World initial = duel(2, 150);
  initial.objects.push_back(object(3, 0, 1, 618, 400));
  initial.objects.push_back(object(4, 0, 0, 550, 430));
  initial.objects.push_back(object(5, 33, 1, 550, 400));
  World w = initial; GameSim sim(rules); sim.init(w); attack(sim); tick(sim, w);
  CHECK(w.objects[0].has_fired && w.objects[0].cooldown == 31);
  CHECK(w.objects[1].hp == 96500);
  CHECK(w.objects[2].hp == 100000 - rules->blast_damage(38, rules->type(0), int(combat_distance(68, 0))));
  CHECK(w.objects[3].hp == 96500 && w.objects[4].hp == 100000);
  for (const auto& e : w.effects) CHECK(!e.flight);
  World replay_world = initial;
  auto replay = GameSim::replay(replay_world, rules, sim.log(), 1);
  CHECK(replay->state_hash(replay_world) == sim.state_hash(w));
}

static void test_melee(const std::shared_ptr<RulesData>& rules) {
  for (int type : {6,53}) {
    World w = duel(type, 60);
    w.objects.push_back(object(3, 0, 1, 450, 365)); // flank, about 25/256 of a turn
    w.objects.push_back(object(4, 0, 1, 340, 400)); // behind
    w.objects.push_back(object(5, 0, 0, 470, 400)); // ally in central sector
    w.objects.push_back(object(6, 33, 1, 470, 400));
    w.objects.push_back(object(7, 0, 1, 497, 400)); // outside sweep radius
    GameSim sim(rules); sim.init(w); attack(sim); tick(sim, w);
    CHECK(w.objects[1].hp == 97000 && w.objects[2].hp == 98500);
    for (int i : {3,4,5,6}) CHECK(w.objects[i].hp == 100000);
    CHECK(w.objects[0].cooldown == 20);
  }
  // Healing uses computed damage, including overkill, and caps at primary max HP.
  for (int health : {1000,22990}) {
    World w = duel(31, 30); w.objects[0].hp = health; w.objects[1].hp = 1;
    GameSim sim(rules); sim.init(w); attack(sim);
    const int before = regenerated_hp(rules->type(31), health, false);
    tick(sim, w);
    CHECK(!w.objects[1].alive);
    CHECK(w.objects[0].hp == std::min(rules->type(31).hp_max, before + rules->attack(31,0).damage / 3));
    CHECK(w.objects[0].side == 0 && w.objects[0].secondary_hp == rules->type(31).secondary_hp);
  }
}

static void test_snowman(const std::shared_ptr<RulesData>& rules) {
  World w = duel(56, 30);
  w.objects.push_back(object(3, 0, 1, 460, 460)); // hit rectangle intersects square
  w.objects.push_back(object(4, 0, 1, 476, 400)); // rectangle touches excluded right edge
  w.objects.push_back(object(5, 0, 0, 430, 440));
  w.objects.push_back(object(6, 33, 1, 430, 440));
  w.objects.push_back(object(7, 14, 1, 430, 440)); // non-Marine
  GameSim sim(rules); sim.init(w); attack(sim);
  const int secondary = w.objects[2].secondary_hp;
  const int angle = w.objects[2].body_angle;
  tick(sim, w);
  CHECK(w.objects[1].hp == 82000 && w.objects[1].cooldown == 1017);
  CHECK(w.objects[2].hp == 100000 && w.objects[2].cooldown == 1017);
  for (int i : {3,4,5,6}) CHECK(w.objects[i].cooldown == 999);
  CHECK(w.objects[2].secondary_hp == secondary && w.objects[2].body_angle == angle);
  // Consecutive applications add rather than replace the existing cooldown.
  w.objects[0].cooldown = 0;
  sim.teleport(w, 1, 400, 400); sim.teleport(w, 2, 430, 400); sim.teleport(w, 3, 460, 460);
  tick(sim, w);
  CHECK(w.objects[2].cooldown == 1034);
}

static void test_point_payload(const std::shared_ptr<RulesData>& rules) {
  // Elefant's two ballistic carriers delegate to class 9 at the landing point.
  World w = duel(57, 100); GameSim sim(rules); sim.init(w); attack(sim); tick(sim, w);
  CHECK(w.effects.size() == 2);
  for (const auto& e : w.effects) CHECK(e.payload_id == 45);
  for (int n = 0; n < 12; ++n) tick(sim, w);
  CHECK(w.objects[1].hp < 100000);

  World dense;
  dense.terrain.w = dense.terrain.h = 30; dense.terrain.cells.resize(900);
  for (int i = 0; i <= kWeaponQueryLimit; ++i)
    dense.objects.push_back(object(i + 1, 0, 0, 500, 400));
  GameSim group(rules); group.init(dense);
  // Freeze synthetic overlaps: this control isolates the query limit after the unit phase.
  for (auto& target : dense.objects) target.mover = -1;
  Effect hit; hit.kind = aztec::data::EffectClass::BallExpl; hit.flight = true;
  hit.payload_id = 45; hit.source = 999;
  hit.x = hit.end_x = to_fixed(500); hit.y = hit.end_y = to_fixed(400);
  dense.effects.push_back(hit); tick(group, dense);
  const int damage = rules->weapon(45, rules->type(0)).damage;
  for (int i = 0; i < kWeaponQueryLimit; ++i) CHECK(dense.objects[i].hp == 100000 - damage);
  CHECK(dense.objects.back().hp == 100000);
}

static void test_direct(const std::shared_ptr<RulesData>& rules) {
  for (int distance : {30,31}) {
    World w = duel(54,distance); GameSim sim(rules); sim.init(w); attack(sim); tick(sim,w);
    CHECK(w.effects.size() == size_t(distance == 31));
    if (!w.effects.empty()) {
      CHECK(w.effects[0].anim == 107 && w.effects[0].x == to_fixed(400) && w.effects[0].y == to_fixed(400));
      CHECK(w.effects[0].slot >= 0 && w.effects[0].slot < 32);
    }
  }
  for (int type : {0,1,5,54,55,82}) {
    World initial = duel(type, type == 1 || type == 54 ? 100 : 10), w = initial;
    w.objects.push_back(object(3, 0, 1, 440, 440));
    GameSim sim(rules); sim.init(w); attack(sim); tick(sim, w);
    const auto shot = rules->attack(type, 0);
    CHECK(w.objects[1].hp == 100000 - shot.damage && w.objects[2].hp == 100000);
    CHECK(w.objects[0].cooldown == shot.reload + 1);
    CHECK(w.objects[1].side == 1 && w.objects[1].secondary_hp == rules->type(0).secondary_hp);
    CHECK(w.objects[1].cooldown == 999);
    for (const auto& e : w.effects) CHECK(!e.flight);
    int muzzle = 0, impact = 0, trail = 0;
    for (const auto& e : w.effects) {
      muzzle += e.anim == 106; impact += e.anim == 110; trail += e.anim == 107;
    }
    CHECK(muzzle == (type == 1) && impact == (type == 1));
    CHECK((trail > 0) == (type == 54));
    CHECK(w.combat_sounds.size() == size_t(type == 1));
    if (!w.combat_sounds.empty()) CHECK(w.combat_sounds[0].set == 200);
    // Add the same innocent bystander to the fresh replay world.
    initial.objects.push_back(object(3, 0, 1, 440, 440));
    auto replay = GameSim::replay(initial, rules, sim.log(), 1);
    CHECK(replay->state_hash(initial) == sim.state_hash(w));
  }
}

static void test_abdula_death(const std::shared_ptr<RulesData>& rules) {
  for (int type : {5,28,54,55,82}) {
    World initial = duel(0, 10);
    initial.objects[1].type = type; initial.objects[1].hp = 1;
    World w = initial; GameSim sim(rules); sim.init(w); attack(sim); tick(sim,w);
    CHECK(!w.objects[1].alive && w.objects[1].anim_variant < 3);
    CHECK(w.objects[1].side == 1 && w.objects[1].secondary_hp == rules->type(type).secondary_hp);
    CHECK(w.effects.size() == size_t(type == 82));
    CHECK(w.combat_sounds.size() == size_t(type == 82));
    if (type == 82 && !w.effects.empty() && !w.combat_sounds.empty()) {
      const auto& e = w.effects[0];
      CHECK(e.anim == 151 && e.slot >= 0 && e.slot < 3);
      CHECK(e.x == w.objects[1].cur_x && e.y == w.objects[1].cur_y + to_fixed(1));
      CHECK(e.height == 1 && e.payload_id == -1 && !e.flight && !e.flame);
      CHECK(w.combat_sounds[0].set == 150 && w.combat_sounds[0].volume_offset == -400);
    }
    auto replay = GameSim::replay(initial, rules, sim.log(), 1);
    CHECK(replay->state_hash(initial) == sim.state_hash(w));
    const auto random = sim.random_state();
    sim.script_kill(w,2); CHECK(sim.random_state() == random); // death is emitted once
    Command pause; pause.type = Pct::PauseOn; sim.submit(pause); tick(sim,w);
    CHECK(w.combat_sounds.empty() && sim.random_state() == random);
    pause.type = Pct::PauseOff; sim.submit(pause);
    for (int n = 0; n < 12; ++n) tick(sim,w);
    CHECK(w.effects.empty()); // decoration never becomes an explosion payload or resurrection
  }
}

int main() {
  auto rules = std::make_shared<RulesData>();
  CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  test_formulas(rules); test_mage_and_tower(rules); test_demolisher(rules);
  test_melee(rules); test_snowman(rules); test_point_payload(rules); test_direct(rules);
  test_abdula_death(rules);
  std::puts(failures ? "FAILED" : "ground weapons passed");
  return failures ? 1 : 0;
}
