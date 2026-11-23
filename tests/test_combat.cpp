// Invented rules and animation banks. No installation, extracted tables or reference runtime is used.
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "temp_dir.hpp"
#include "core/endian.hpp"
#include "engine/data/type_anims.hpp"
#include "engine/sim/game_sim.hpp"

using namespace aztec;
using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static void csv(const TempDir& dir, const char* name, const std::vector<std::vector<int>>& rows) {
  std::ofstream file(dir.path() / name);
  file << "synthetic header\n";
  for (const auto& row : rows) {
    for (size_t i = 0; i < row.size(); ++i) file << (i ? "," : "") << row[i];
    file << '\n';
  }
}

static std::shared_ptr<RulesData> rules(const TempDir& dir, int reload = 2) {
  csv(dir, "types.csv", {{0,0,0,0,0,0,0}, {1,0,0,1,0,0,0}, {27,0,0,2,0,0,0},
                          {14,0,0,0,0,0,0}, {17,0,0,0,0,0,0}});
  auto marine = [](int parameter, int hp, int weapon) {
    std::vector<int> row(18);
    row[1] = data::kTypeAnims[parameter == 0 ? 0 : parameter == 1 ? 1 : 27].anim;
    row[0] = parameter; row[2] = hp; row[5] = hp;
    row[6] = 2; row[7] = 64; row[8] = weapon; row[12] = 1;
    return row;
  };
  csv(dir, "desc_marine.csv", {marine(0,40,101), marine(1,20,101), marine(2,30,102)});
  csv(dir, "desc_building.csv", {{0,0,40,-30,-20,34,25}});
  std::vector<int> defence(68); defence[4] = 40; defence[6] = -18;
  defence[7] = -30; defence[8] = 22; defence[9] = 10; defence[60] = -1;
  csv(dir, "desc_defence.csv", {defence});
  csv(dir, "weapons_ids.csv", {{101,2,0}, {102,1,0}, {103,10,0}, {104,7,0}, {105,13,1}});
  // reload, base, armour, reserved, range, air/nonorganic/f4/e8/structure, visuals, centre flag
  csv(dir, "weapon_class_w2_13.csv", {{0,reload,7,0,0,100,256,256,256,256,256,0,0,0,0},
                                    {1,3,2,0,0,100,256,192,256,256,192,0,0,0,1}});
  // animation, speed, reload, range, base, armour, reserved, category factors, child, visual
  csv(dir, "weapon_class_arrow1.csv", {{0,111,24,3,240,2,0,0,256,192,256,256,192,0,0}});
  // animation, inner, outer, reserved, base, air, remaining visual metadata
  csv(dir, "weapon_class_w7.csv", {{0,0,5,12,0,11,0,0,0,0}});
  auto result = std::make_shared<RulesData>();
  CHECK(result->load_dir(dir.path().string()));
  return result;
}

static std::string animations(const TempDir& dir, int fire_duration = 1, bool hold = false, int dx = 0, int extra_id = -1) {
  using Bytes = std::vector<uint8_t>;
  auto put = [](Bytes& bytes, size_t offset, uint32_t value) { write_le(bytes.data() + offset, value); };
  constexpr int count = 100;
  Bytes archive(20 + count * 4);
  put(archive, 0, 1); put(archive, 4, 0x515); put(archive, 8, 20); put(archive, 16, count);
  for (int id = 0; id < count; ++id) {
    put(archive, 20 + id * 4, uint32_t(archive.size()));
    if (id != extra_id && id != data::kTypeAnims[0].anim && id != data::kTypeAnims[1].anim && id != data::kTypeAnims[27].anim) continue;
    Bytes animation(0x208 + 33 * 32);
    std::memcpy(animation.data(), "Anim", 4); put(animation, 4, 3);
    for (int facing = 0; facing < 8; ++facing) {
      int first = 1 + facing * 4;
      put(animation, 8 + (9 + facing) * 4, first);
      for (int frame = 0; frame < 3; ++frame) {
        size_t offset = 0x208 + (first + frame) * 32;
        put(animation, offset, uint32_t(frame + 1));
        put(animation, offset + 12, frame == 1 ? 1 : 0);
        put(animation, offset + 16, frame == 1 ? uint32_t(dx) : 0);
        put(animation, offset + 28, frame == 0 ? 2 : frame == 1 ? uint32_t(fire_duration) : hold ? 0 : 1);
      }
    }
    archive.insert(archive.end(), animation.begin(), animation.end());
  }
  auto path = dir.path() / "invented-animation.xbe";
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char*>(archive.data()), std::streamsize(archive.size()));
  return path.string();
}

static World fight(bool group = false) {
  World w;
  w.terrain.w = w.terrain.h = 20; w.terrain.cells.resize(400);
  w.relation[0][1] = w.relation[1][0] = kEnemy;
  const int count = group ? 4 : 2;
  for (int i = 0; i < count; ++i) {
    Object o; o.id = uint32_t(i + 1); o.side = i >= count / 2; o.type = o.side ? 1 : 0;
    o.attack_mode = AttackMode::Direct;
    o.teleport(to_fixed(o.side ? 450 : 350), to_fixed(350 + (group ? i % 2 * 60 : 0)));
    w.objects.push_back(o);
  }
  return w;
}

static void attack(GameSim& sim, int side, std::vector<uint32_t> ids, uint32_t victim) {
  Command command; command.type = Pct::Attack; command.side = uint8_t(side);
  command.ids = std::move(ids); command.a = int(victim); sim.submit(command);
}

static void step(World& world, GameSim& sim) { world.begin_tick(); sim.tick(world); }

static void arithmetic(const RulesData& r) {
  TypeRules target; target.nonorganic = target.structure = true;
  // V formula: integer stages lose the half point; Arrow retains it until return.
  CHECK(r.weapon(105, target).damage == 0);
  CHECK(r.weapon(102, target).damage == 1);
  CHECK(!r.weapon(105, target).edge_range);
  CHECK(r.weapon(101, target).edge_range);
  target = {}; target.category_f4 = target.category_e8 = true;
  CHECK(r.weapon(103, target).damage == 2250); // a single OR-gated 320/256 bonus
  target = {}; target.combat_bounds = {-10,-10,10,10};
  CHECK(r.blast_damage(104, target, 15) == 11);
  CHECK(r.blast_damage(104, target, 16) == 9); // ratio floor(6*256/7)=219; floor(11*219/256)=9
  target.structure = true;
  CHECK(r.blast_damage(104, target, 16) == 9); // +0xec is not +0xe8
  target.category_e8 = true; target.blast_multiplier = 192;
  CHECK(r.blast_damage(104, target, 16) == 5); // retain 8.8 through 204 and 192 factors
  CHECK(r.blast_damage(104, target, 22) == 0);
  target.flier = true;
  CHECK(r.blast_damage(104, target, 0) == 0);
  CHECK(combat_distance(128, 0) == 123);
  CHECK(combat_distance(-128, 128) == 174);
  CHECK(r.type(0).combat_radius() == 28 && r.type(0).radius_px == 14);
  CHECK(r.type(14).combat_bounds == (std::array<int,4>{-30,-20,34,25}));
  CHECK(r.type(17).combat_bounds == (std::array<int,4>{-18,-30,22,10}));
}

static void ranges_and_pursuit(const std::shared_ptr<RulesData>& r) {
  for (int type : {0,27}) for (int outside : {0,1}) {
    auto w = fight(); w.objects[0].type = type;
    // V: norm(162,0)=156=100+28+28; norm(248,0)=240.
    const int distance = (type == 0 ? 162 : 248) + outside;
    w.objects[1].teleport(to_fixed(350 + distance), to_fixed(350));
    GameSim sim(r); sim.init(w); attack(sim, 0, {1}, 2); step(w, sim);
    CHECK(w.objects[0].has_fired == (outside == 0));
  }
  // Explicit orders remain eligible against ordinary buildings.
  auto initial = fight(); initial.objects[1].type = 14;
  auto w = initial; GameSim scan(r); scan.init(w);
  attack(scan, 0, {1}, 2);
  for (int tick = 0; tick < 3; ++tick) step(w, scan);
  CHECK(w.objects[1].hp == 33);
  // V: pursuit drops beyond reference norm 1152, even for an explicit order.
  w = fight(); w.terrain.w = 50; w.terrain.cells.resize(1000);
  w.objects[1].teleport(to_fixed(1600), to_fixed(350));
  GameSim pursuit(r); pursuit.init(w); attack(pursuit, 0, {1}, 2); step(w, pursuit);
  CHECK(w.objects[0].target == 0 && !w.objects[0].has_fired);
}

static void duels(const std::shared_ptr<RulesData>& r) {
  for (bool group : {false, true}) {
    World initial = fight(group), w = initial;
    GameSim sim(r); sim.init(w);
    attack(sim, 0, group ? std::vector<uint32_t>{1,2} : std::vector<uint32_t>{1}, group ? 3 : 2);
    attack(sim, 1, group ? std::vector<uint32_t>{3,4} : std::vector<uint32_t>{2}, 1);
    for (int tick = 1; tick <= (group ? 18 : 13); ++tick) {
      if (group && tick == 10) attack(sim, 0, {1,2}, 4);
      step(w, sim);
      if (tick == 1) CHECK(w.objects.back().hp == 20 && w.objects[0].release_target != 0);
      if (tick == 3) CHECK(w.objects[0].hp == (group ? 26 : 33));
      if (!group && tick < 13) CHECK(w.objects[1].alive);
      World replay_world = initial;
      auto replay = GameSim::replay(replay_world, r, sim.log(), sim.tick_count());
      CHECK(replay->state_hash(replay_world) == sim.state_hash(w));
    }
    // V: 7 damage per release, 4 animation ticks, reload+1=3; starts at 1,6,11,...
    // A 20-HP opponent needs three hits. The newer opponent releases its final shot first.
    CHECK(!w.objects.back().alive);
    CHECK(w.objects[0].hp == (group ? 0 : 19));
    if (group) CHECK(w.objects[1].hp == 40 && !w.objects[2].alive);
  }
}

static void release_controls(const TempDir& dir, const std::shared_ptr<RulesData>& r) {
  CHECK(r->load_combat_animations(animations(dir, 2, true, 1)));
  auto frame = r->attack_frame(0, 0, 2);
  CHECK(frame.known && frame.active && frame.fire && frame.dx == 1);
  CHECK(r->attack_frame(0, 0, 100).active && !r->attack_frame(0, 0, 100).fire);
  World initial = fight(), w = initial;
  GameSim sim(r); sim.init(w); attack(sim, 0, {1}, 2);
  step(w, sim);
  auto hash = sim.state_hash(w);
  ++w.objects[0].release_target; CHECK(sim.state_hash(w) != hash); --w.objects[0].release_target;
  ++w.objects[0].attack_facing; CHECK(sim.state_hash(w) != hash); --w.objects[0].attack_facing;
  Command pause; pause.type = Pct::PauseOn; sim.submit(pause); step(w, sim);
  CHECK(w.objects[0].attack_tick == 0 && w.objects[1].hp == 20);
  pause.type = Pct::PauseOff; sim.submit(pause); step(w, sim);
  CHECK(w.objects[1].hp == 20);
  // A new order must not replace the retained release target, and a victim moving
  // out of CanFire range after attack start does not cancel the release.
  Command stop; stop.type = Pct::Stop; stop.ids = {1}; sim.submit(stop);
  sim.teleport(w, 2, 700, 350);
  step(w, sim); CHECK(w.objects[1].hp == 13);
  step(w, sim); CHECK(w.objects[1].hp == 6);
  for (int i = 0; i < 8; ++i) step(w, sim);
  CHECK(w.objects[1].hp == 6); // held recovery frame blocks a fresh attack
  CHECK(w.objects[0].cur_x == to_fixed(352));

  CHECK(r->load_combat_animations(animations(dir)));
  w = fight(); GameSim killed(r); killed.init(w); attack(killed, 0, {1}, 2);
  step(w, killed); killed.script_kill(w, 1);
  for (int i = 0; i < 5; ++i) step(w, killed);
  CHECK(w.objects[1].hp == 20 && !w.objects[0].release_target);

  w = fight(); Object other = w.objects[1]; other.id = 3; other.cooldown = 100;
  other.teleport(to_fixed(500), to_fixed(350)); w.objects.push_back(other);
  GameSim retarget(r); retarget.init(w); attack(retarget, 0, {1}, 2); step(w, retarget);
  attack(retarget, 0, {1}, 3); step(w, retarget); step(w, retarget);
  CHECK(w.objects[0].target == 3 && w.objects[0].release_target == 2);
  CHECK(w.objects[1].hp == 13 && w.objects[2].hp == 20);

  w = fight(); GameSim dead_victim(r); dead_victim.init(w); attack(dead_victim, 0, {1}, 2);
  step(w, dead_victim); dead_victim.script_kill(w, 2);
  for (int i = 0; i < 4; ++i) step(w, dead_victim);
  CHECK(!w.objects[1].alive && !w.objects[0].release_target && w.effects.empty());
}

static void projectile_escape(const std::shared_ptr<RulesData>& r) {
  for (int x : {-28, 27, 28}) {
    World w = fight(); GameSim sim(r); sim.init(w);
    Effect arrow; arrow.kind = data::EffectClass::Arrow; arrow.flight = true;
    arrow.target = 2; arrow.source = 99; arrow.damage = 7;
    arrow.x = arrow.end_x = w.objects[1].cur_x + to_fixed(x);
    arrow.y = arrow.end_y = w.objects[1].cur_y;
    w.effects.push_back(arrow); step(w, sim);
    CHECK(w.objects[1].hp == (x == 28 ? 20 : 13));
  }
  World w = fight(); GameSim sim(r); sim.init(w);
  Effect arrow; arrow.kind = data::EffectClass::Arrow; arrow.flight = true;
  arrow.target = 2; arrow.source = 99; arrow.damage = 7;
  arrow.x = arrow.end_x = w.objects[1].cur_x;
  arrow.y = arrow.end_y = w.objects[1].cur_y;
  w.effects.push_back(arrow); sim.teleport(w, 2, 650, 350); step(w, sim);
  CHECK(w.objects[1].hp == 20 && w.objects[1].last_hit_by == 0);
}

static void marine_periods() {
  // Both type mappings use invented banks and reloads, never extracted fixtures.
  for (int type : {0, 1}) for (int reload : {2, 4, 6, 9, 20}) for (bool resources : {false, true}) {
    TempDir dir;
    auto r = rules(dir, reload);
    if (resources) CHECK(r->load_combat_animations(animations(dir)));
    const int period = resources ? std::max(reload + 1, 5) : reload + 1;
    for (int facing = 0; facing < 8; ++facing) {
      CHECK(r->marine_attack_ticks(type, facing) == (resources ? 4 : -1));
      constexpr int dx[] = {1,1,0,-1,-1,-1,0,1}, dy[] = {0,-1,-1,-1,0,1,1,1};
      auto w = fight(); w.objects[0].type = type; w.objects[1].type = 14;
      w.objects[1].teleport(to_fixed(350 + dx[facing] * 60), to_fixed(350 + dy[facing] * 60));
      GameSim sim(r); sim.init(w); w.objects[1].hp = 10000;
      attack(sim, 0, {1}, 2);
      std::vector<int> starts, hits;
      for (int tick = 1; tick <= 1 + 3 * period; ++tick) {
        const int hp = w.objects[1].hp;
        step(w, sim);
        if (w.objects[0].attack_tick == 0) {
          starts.push_back(tick);
          CHECK(w.objects[0].cooldown == period);
          CHECK(w.objects[0].facing == facing);
        }
        if (w.objects[1].hp < hp) hits.push_back(tick);
      }
      CHECK(starts == (std::vector<int>{1, 1 + period, 1 + 2 * period, 1 + 3 * period}));
      CHECK(hits.size() >= 3);
      for (size_t i = 1; i < hits.size(); ++i) CHECK(hits[i] - hits[i - 1] == period);
    }
    CHECK(r->marine_attack_ticks(-1, 0) == -1);
    CHECK(r->marine_attack_ticks(90, 0) == -1);
    CHECK(r->marine_attack_ticks(14, 0) == -1);
    CHECK(r->marine_attack_ticks(2, 0) == -1); // missing sequence
    CHECK(r->load_combat_animations(animations(dir, 1, true)));
    CHECK(r->marine_attack_ticks(type, 0) == -1); // holding recovery frame
    CHECK(r->load_combat_animations(animations(dir, -1)));
    CHECK(r->marine_attack_ticks(type, 0) == -1); // invalid duration
  }
}

static void marine_reload_during_step() {
  for (int type : {0, 1}) {
    TempDir dir;
    const int period = type == 0 ? 10 : 21;
    auto r = rules(dir, period - 1);
    CHECK(r->load_combat_animations(animations(dir)));
    auto w = fight(); w.objects[0].type = type; w.objects[1].type = 14;
    w.objects[0].teleport(tile_center_fixed(7), tile_center_fixed(7));
    w.objects[1].teleport(tile_center_fixed(7), tile_center_fixed(8));
    GameSim sim(r); sim.init(w); w.objects[1].hp = 10000;
    attack(sim, 0, {1}, 2); step(w, sim);
    CHECK(w.objects[0].attack_tick == 0);
    for (int tick = 0; tick < 10 && w.objects[0].release_target; ++tick) step(w, sim);
    CHECK(!w.objects[0].release_target && w.objects[0].cooldown > 0);
    Command move; move.type = Pct::Move; move.side = 0; move.ids = {1};
    move.a = tile_center_fixed(8) / 256; move.b = tile_center_fixed(7) / 256;
    sim.submit(move); step(w, sim);
    CHECK(w.objects[0].cur_x > tile_center_fixed(7));
    // Preserve the running tile when replacing the walking order with attack.
    w.objects[0].attack_mode = AttackMode::Balanced;
    attack(sim, 0, {1}, 2);
    bool ready_while_walking = false, fired = false;
    for (int tick = 0; tick < 40; ++tick) {
      const int before_x = w.objects[0].cur_x;
      step(w, sim);
      if (w.objects[0].attack_tick == 0) {
        CHECK(before_x == tile_center_fixed(8));
        CHECK(w.objects[0].cooldown == period);
        fired = true;
        break;
      }
      if (w.objects[0].cooldown == 0 && w.objects[0].cur_x < tile_center_fixed(8))
        ready_while_walking = true;
    }
    CHECK(ready_while_walking && fired);
    for (int tick = 1; tick <= period; ++tick) {
      step(w, sim);
      CHECK((w.objects[0].attack_tick == 0) == (tick == period));
    }
  }
}

int main() {
  TempDir dir;
  auto r = rules(dir); arithmetic(*r);
  CHECK(r->load_combat_animations(animations(dir)));
  ranges_and_pursuit(r); duels(r); release_controls(dir, r); projectile_escape(r);
  CHECK(!r->load_combat_animations((dir.path() / "missing.xbe").string()));
  CHECK(!r->attack_frame(0, 0, 0).known);
  marine_periods(); marine_reload_during_step();
  // A supplied descriptor selects its own resource, including explicit zero.
  auto descriptor = std::vector<int>(20);
  descriptor[1] = 90; descriptor[2] = 40; descriptor[8] = -1;
  csv(dir, "desc_marine.csv", {descriptor});
  CHECK(r->load_dir(dir.path().string()));
  CHECK(r->load_combat_animations(animations(dir)));
  CHECK(!r->attack_frame(0, 0, 2).known);
  CHECK(r->marine_attack_ticks(0, 0) == -1);
  CHECK(r->load_combat_animations(animations(dir, 1, false, 7, 90)));
  CHECK(r->attack_frame(0, 0, 2).fire && r->attack_frame(0, 0, 2).dx == 7);
  CHECK(r->marine_attack_ticks(0, 0) == 4);
  descriptor[1] = 0;
  csv(dir, "desc_marine.csv", {descriptor});
  CHECK(r->load_dir(dir.path().string()));
  CHECK(r->load_combat_animations(animations(dir)));
  CHECK(!r->attack_frame(0, 0, 2).known);
  // Defence selects the descriptor turret, including missing-resource and
  // explicit-zero cases, rather than the static type's base/turret choice.
  std::vector<int> defence(69);
  defence[1] = 90; defence[3] = 90; defence[4] = 40; defence[60] = -1;
  csv(dir, "desc_defence.csv", {defence});
  CHECK(r->load_dir(dir.path().string()));
  CHECK(r->load_combat_animations(animations(dir, 1, false, 7, 90)));
  CHECK(r->attack_frame(17, 0, 2, 56).fire);
  defence[3] = 98;
  csv(dir, "desc_defence.csv", {defence});
  CHECK(r->load_dir(dir.path().string()));
  CHECK(r->load_combat_animations(animations(dir, 1, false, 7, 90)));
  CHECK(!r->attack_frame(17, 0, 2, 56).known);
  defence[3] = 0;
  csv(dir, "desc_defence.csv", {defence});
  CHECK(r->load_dir(dir.path().string()));
  CHECK(r->load_combat_animations(animations(dir, 1, false, 7, 90)));
  CHECK(r->attack_frame(17, 0, 2, 56).known);
  CHECK(!r->attack_frame(17, 0, 2, 56).active); // base slot 8 is absent
  return failures ? 1 : 0;
}
