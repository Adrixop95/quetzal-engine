// Invented animation banks and scenes; no original installation is required.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <tuple>
#include "core/endian.hpp"
#include "engine/data/type_anims.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/assets/anim_provider.hpp"
#include "engine/world/object_state.hpp"
#include "temp_dir.hpp"

using namespace aztec;
using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static World scene(int source, int victim = 0, int distance = 100) {
  World w; w.terrain.w = w.terrain.h = 32; w.terrain.cells.resize(1024);
  w.relation[0][1] = w.relation[1][0] = kEnemy;
  Object a; a.id = 1; a.type = source; a.hp = 100000;
  a.teleport(to_fixed(400), to_fixed(400));
  Object b; b.id = 2; b.type = victim; b.hp = 100000; b.side = 1;
  b.cooldown = 10000; b.attack_mode = AttackMode::Direct;
  b.teleport(to_fixed(400 + distance), to_fixed(400));
  w.objects = {a,b}; return w;
}
static void tick(GameSim& sim, World& w) { w.begin_tick(); sim.tick(w); }
static void attack(GameSim& sim) {
  Command c; c.type = Pct::Attack; c.ids = {1}; c.a = 2; sim.submit(c);
}

static std::string animations(const TempDir& dir) {
  using Bytes = std::vector<uint8_t>;
  auto put = [](Bytes& b, size_t at, uint32_t v) { write_le(b.data() + at, v); };
  constexpr int count = 500;
  Bytes bank(20 + count * 4);
  put(bank, 0, 1); put(bank, 4, 0x515); put(bank, 8, 20); put(bank, 16, count);
  for (int id = 0; id < count; ++id) {
    put(bank, 20 + id * 4, uint32_t(bank.size()));
    bool selected = false, turret = false;
    for (int type : {17,23,26,42,49,69,81}) {
      selected |= id == (data::kTypeAnims[type].anim2 ? data::kTypeAnims[type].anim2 : data::kTypeAnims[type].anim);
      turret |= id == data::kTypeAnims[type].anim2;
    }
    if (!selected) continue;
    Bytes animation(0x208 + 137 * 32);
    std::memcpy(animation.data(), "Anim", 4); put(animation, 4, 3);
    if (turret) {
      // An invented, finite reload cue, distinct from directional shots.
      put(animation, 12, 134);
      put(animation, 0x208 + 134 * 32, 77);
      put(animation, 0x208 + 134 * 32 + 28, 2);
    }
    for (int slot = 2; slot < 34; ++slot) {
      if (!turret && slot != 8) continue;
      const int first = 1 + (slot - 2) * 4;
      put(animation, 8 + slot * 4, first);
      for (int frame = 0; frame < 3; ++frame) {
        const int offset = 0x208 + (first + frame) * 32;
        put(animation, offset, uint32_t(frame + 1));
        put(animation, offset + 12, frame == 1);
        put(animation, offset + 28, frame == 0 ? 2 : 1);
      }
    }
    bank.insert(bank.end(), animation.begin(), animation.end());
  }
  auto path = dir.path() / "invented-towers.xbe";
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bank.data()), std::streamsize(bank.size()));
  return path.string();
}

static void towers(const std::shared_ptr<RulesData>& rules) {
  for (int type : {17,23,26,42,49,69,81}) {
    World initial = scene(type, type == 81 ? 33 : 0);
    // Align with the first seeded activation heading. Acquisition/marker checks
    // below stay independent of the separate turn-step controls.
    initial.objects[1].teleport(to_fixed(460),to_fixed(320));
    World w = initial;
    GameSim sim(rules); sim.init(w);
    int launches = 0, launch_height = -1;
    sim.set_combat_observer([&](uint64_t, const char* event, const Effect& e, uint32_t, int, int) {
      if (std::strcmp(event, "launch") == 0) { ++launches; launch_height = e.height; }
    });
    for (int n = 1; n <= 6; ++n) {
      tick(sim,w);
      if (n == 1) {
        CHECK(w.objects[0].has_fired && w.objects[0].release_target == 2 && w.objects[0].target == 0);
        CHECK(w.objects[0].cooldown == rules->attack(type, initial.objects[1].type).reload);
      }
      if (n <= 3) CHECK(launches == 0 && w.objects[1].hp == 100000);
      if (n == 4) {
        CHECK(launches > 0);
        if (type == 49 || type == 23 || type == 26) CHECK(launch_height == rules->type(type).launch_height);
      }
      World restored = initial;
      auto replay = GameSim::replay(restored, rules, sim.log(), n);
      CHECK(replay->state_hash(restored) == sim.state_hash(w));
    }
    CHECK(!w.objects[0].release_target);
    if (type == 69) {
      CHECK(w.objects[1].hp == 100000 - rules->attack(69,0).damage);
      CHECK(w.objects[1].side == 1 && w.objects[1].secondary_hp == rules->type(0).secondary_hp);
    }
  }
  // A tower acquires an arriving target between mobile tactic scans, without
  // waiting for that tactic to discover and list the enemy team.
  World w = scene(69,0,1000); GameSim sim(rules); sim.init(w); tick(sim,w);
  CHECK(!w.objects[0].has_fired);
  const auto& tactic = w.unit_tactics.at(tactic_key(w.objects[0]));
  CHECK(tactic.enemies.empty() && tactic.scan_ticks > 1);
  const int scan_ticks = tactic.scan_ticks;
  sim.teleport(w,2,500,400); tick(sim,w);
  CHECK(w.objects[0].release_target == 2);
  CHECK(tactic.enemies.empty() && tactic.scan_ticks == scan_ticks - 1);

  // Stop/retarget cannot replace a shot already latched by its animation.
  w = scene(69); sim.init(w); tick(sim,w);
  Command stop; stop.type = Pct::Stop; stop.ids = {1}; sim.submit(stop);
  Command pause; pause.type = Pct::PauseOn; sim.submit(pause); tick(sim,w);
  CHECK(w.objects[0].attack_tick == -1 && w.objects[1].hp == 100000);
  pause.type = Pct::PauseOff; sim.submit(pause);
  for (int n = 0; n < 3; ++n) tick(sim,w);
  CHECK(w.objects[1].hp == 100000 - rules->attack(69,0).damage);

  // A 6-unit error is below the full 8-unit step, but above half a step.
  for (int error : {6,8,16}) {
    w = scene(23); sim.init(w); w.objects[0].turret_angle = error;
    tick(sim,w);
    CHECK(w.objects[0].has_fired == (error == 6));
    CHECK(w.objects[0].turret_angle == (error == 6 ? 0 : error - 8));
  }
  for (auto [type, victim, step] : {std::tuple{81,33,32}, std::tuple{69,0,128}}) {
    CHECK(rules->type(type).turn_rate == step);
    for (int error : {step - 1, step}) {
      w = scene(type,victim); sim.init(w); w.objects[0].turret_angle = error;
      tick(sim,w);
      CHECK(w.objects[0].has_fired == (error < step));
      CHECK(w.objects[0].turret_angle == 0);
    }
  }
  w = scene(23); sim.init(w); w.objects[0].cooldown = 18;
  tick(sim,w); CHECK(!w.objects[0].target);
  tick(sim,w); CHECK(!w.objects[0].target); // strict early-window boundary
  tick(sim,w); CHECK(w.objects[0].target == 2 && !w.objects[0].has_fired);

  w = scene(69); sim.init(w); tick(sim,w); sim.script_kill(w,1);
  for (int n = 0; n < 6; ++n) tick(sim,w);
  CHECK(!w.objects[0].release_target && w.objects[1].hp == 100000);
  // Unsupported victim categories never become an aim target or a released shot.
  for (auto pair : {std::pair{23,33}, {81,0}}) {
    w = scene(pair.first,pair.second); sim.init(w); attack(sim);
    for (int n = 0; n < 8; ++n) tick(sim,w);
    CHECK(!w.objects[0].has_fired && !w.objects[0].target && !w.objects[0].release_target);
  }
  w = scene(69); sim.init(w); w.objects[0].progress = 500; w.objects[0].build_total = 100; tick(sim,w);
  CHECK(!w.objects[0].has_fired); // no occupants or ammo are required once completed

  // A ground arrow travels down from its muzzle to the snapshotted aim height.
  w = scene(49); sim.init(w);
  for (int n = 0; n < 20; ++n) tick(sim,w);
  CHECK(w.objects[1].hp < 100000);
}

static void tower_visuals(const std::shared_ptr<RulesData>& rules, const std::string& bank) {
  // Unarmed Defence shares activation, damage stages and sparks with armed towers.
  World initial = scene(80,0,1000), w = initial;
  w.objects[0].hp = rules->type(80).hp_max;
  GameSim sim(rules); sim.init(w);
  uint32_t rng = 0;
  CHECK(w.objects[0].turret_angle == int(msvc_rand(rng) & 255));
  CHECK(sim.random_state() == rng);
  for (int hp : {w.objects[0].hp_max * 3 / 4, w.objects[0].hp_max / 2, w.objects[0].hp_max / 4}) {
    w.objects[0].hp = hp; tick(sim,w);
    CHECK(w.objects[0].tower_damage_level == (hp == w.objects[0].hp_max * 3 / 4 ? 1 : hp == w.objects[0].hp_max / 2 ? 2 : 3));
    CHECK(w.combat_sounds.size() == 1 && w.combat_sounds[0].loop);
    CHECK(w.combat_sounds[0].set == uint32_t(150 + w.objects[0].tower_damage_level));
  }
  auto spark_count = [&]() { int count = 0; for (const auto& e : w.effects) if (e.anim == 105) ++count; return count; };
  CHECK(spark_count() == 1);
  for (const auto& e : w.effects) if (e.anim == 105) {
    CHECK(e.height == 80 && e.y == w.objects[0].cur_y + to_fixed(1));
    CHECK(e.x >= w.objects[0].cur_x - to_fixed(14) && e.x <= w.objects[0].cur_x + to_fixed(14));
    CHECK(!e.flight && e.payload_id == -1);
  }
  const auto hash = sim.state_hash(w);
  ++w.objects[0].tower_spark_ticks; CHECK(hash != sim.state_hash(w)); --w.objects[0].tower_spark_ticks;
  Command pause; pause.type = Pct::PauseOn; sim.submit(pause); tick(sim,w);
  CHECK(spark_count() == 1 && w.objects[0].tower_spark_ticks == 3);
  CHECK(w.combat_sounds.size() == 1 && w.combat_sounds[0].loop);
  pause.type = Pct::PauseOff; sim.submit(pause);
  tick(sim,w); tick(sim,w); CHECK(spark_count() == 1);
  tick(sim,w); CHECK(spark_count() == 2);
  w.objects[0].hp = w.objects[0].hp_max; tick(sim,w);
  CHECK(w.objects[0].tower_damage_level == 0 && w.combat_sounds.empty());
  for (const auto& e : w.effects) CHECK(!e.loop);

  w = scene(23,0,1000); sim.init(w); w.objects[0].cooldown = 14;
  tick(sim,w); CHECK(w.objects[0].turret_reload_tick == -1);
  tick(sim,w); CHECK(w.objects[0].turret_reload_tick == 0);
  AnimSpriteProvider provider; CHECK(provider.open(bank));
  auto st = object_sprite_state(w.objects[0],0);
  auto frames = provider.sound_frames(st,false);
  CHECK(frames.size() == 2 && frames[1].sprite == 77);
  const auto parts = provider.parts(st);
  CHECK(!parts.empty() && parts.back().key.index == 77);
  tick(sim,w); CHECK(w.objects[0].turret_reload_tick == 1);
  tick(sim,w); CHECK(w.objects[0].turret_reload_tick == 2);
  frames = provider.sound_frames(object_sprite_state(w.objects[0],0),false);
  CHECK(frames.size() == 2 && frames[1].sprite != 77);

  // Pause/replay retain an activated tower's presentation clocks and RNG draws.
  initial = scene(80,0,1000); initial.objects[0].hp = rules->type(80).hp_max / 4;
  w = initial; sim.init(w);
  for (int n = 1; n <= 8; ++n) {
    tick(sim,w); World restored = initial;
    auto replay = GameSim::replay(restored,rules,sim.log(),n);
    CHECK(replay->state_hash(restored) == sim.state_hash(w));
  }

  // Legacy construction without a queue activates once when progress completes.
  w = scene(80,0,1000); sim.init(w);
  const auto before_build = sim.random_state(); rng = before_build;
  const int activated_heading = int(msvc_rand(rng) & 255);
  auto& building = w.objects[0];
  building.progress = 0; building.build_elapsed = 0; building.build_total = 2; building.hp = 1;
  tick(sim,w); CHECK(building.progress == 500 && sim.random_state() == before_build);
  tick(sim,w); CHECK(building.progress == 1000 && sim.random_state() == rng);
  CHECK(building.turret_angle == activated_heading);
  tick(sim,w); CHECK(sim.random_state() == rng);
}

static void aircraft(const std::shared_ptr<RulesData>& rules) {
  for (int type : {7,33,34,60,85}) {
    auto w = scene(type,14,700); GameSim sim(rules); sim.init(w);
    CHECK(rules->type(type).flier);
    const int start = w.objects[0].render_height;
    CHECK(start == (rules->type(type).kind == 5 ? 30 : 0));
    tick(sim,w); CHECK(w.objects[0].render_height == start + 1);
    for (int n = 0; n < 120; ++n) tick(sim,w);
    CHECK(w.objects[0].render_height == rules->type(type).cruise_height);
  }
  // Bird alignment applies even to a direct attack order, then the tactic re-picks.
  auto w = scene(60,33,100); w.objects[0].attack_mode = AttackMode::Direct;
  GameSim bird(rules); bird.init(w); attack(bird); tick(bird,w);
  CHECK(w.objects[0].has_fired && !w.objects[0].target);
  CHECK(w.objects[0].order == uint8_t(Order::Move));
  const auto x = w.objects[0].cur_x;
  tick(bird,w); CHECK(w.objects[0].target == 2 && w.objects[0].cur_x > x);
  w = scene(60,33,100); bird.init(w); w.objects[0].body_angle = 128; attack(bird); tick(bird,w);
  CHECK(!w.objects[0].has_fired);
  CHECK(w.objects[0].body_angle != 0); // limited turn, no instantaneous nose snap

  World initial = scene(60,33,100), live = initial;
  GameSim repeated(rules); repeated.init(live); attack(repeated);
  for (int n = 1; n <= 30; ++n) {
    tick(repeated,live);
    World restored = initial;
    auto replay = GameSim::replay(restored,rules,repeated.log(),n);
    CHECK(replay->state_hash(restored) == repeated.state_hash(live));
  }

  // Zeppelin does not chase its attack target, and can bomb facing away from it.
  w = scene(85,14,400); GameSim zeppelin(rules); zeppelin.init(w); attack(zeppelin);
  for (int n = 0; n < 20; ++n) tick(zeppelin,w);
  CHECK(w.objects[0].cur_x == to_fixed(400) && !w.objects[0].has_fired);
  zeppelin.teleport(w,2,420,400); w.objects[0].body_angle = 128; tick(zeppelin,w);
  CHECK(w.objects[0].has_fired && w.objects[0].target == 2 && w.effects.size() == 2);
}

static void protection(const std::shared_ptr<RulesData>& rules) {
  CHECK(rules->type(7).blast_multiplier == 512 && rules->type(85).blast_multiplier == 512);
  CHECK(rules->type(10).blast_multiplier == 0);
  CHECK(rules->type(8).blast_multiplier == 256); // Headtruck record zero is the exception
  for (int type : {3,4,9,58,59,61,62}) CHECK(rules->type(type).blast_multiplier == 128);
  CHECK(rules->flame_damage(rules->type(10),false) == 0);
  CHECK(rules->flame_damage(rules->type(3),false) == 1000);
  CHECK(rules->flame_damage(rules->type(3),true) == 250);
  for (int source : {1,2,6,35,69,83}) {
    auto w = scene(source,10,source == 6 ? 30 : 80); GameSim sim(rules); sim.init(w); attack(sim);
    for (int n = 0; n < 40; ++n) tick(sim,w);
    CHECK(rules->type(10).service_damage_protected);
    CHECK(w.objects[1].hp == 100000 && w.objects[1].alive && !w.objects[1].last_hit_by);
  }
}

int main() {
  TempDir dir;
  auto rules = std::make_shared<RulesData>(); CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  const auto bank = animations(dir);
  CHECK(rules->load_combat_animations(bank));
  towers(rules); aircraft(rules); protection(rules);
  tower_visuals(rules,bank);
  return failures ? 1 : 0;
}
