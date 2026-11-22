// Synthetic replay and state-contract controls; no game installation required.
#include <cstdio>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include "core/script.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/sim/savegame.hpp"
#include "engine/world/camera.hpp"
#include "engine/audio/audio_system.hpp"

using namespace aztec::engine;
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static World initial() {
  World w;
  w.terrain.w = w.terrain.h = 24; w.terrain.cells.resize(576);
  w.resources = {24, 24, std::vector<uint16_t>(576, 0x8080)};
  w.side_state.resize(8);
  for (int side = 0; side < 2; ++side) {
    SideDef sd; sd.index = side; sd.start_x = sd.start_y = side ? 19 : 4;
    w.mission.sides.push_back(sd);
    w.mission.towns.push_back({side, side, sd.start_x, sd.start_y, 1, 0, side});
    w.side_state[side].money = 100000;
    w.visibility.sides.push_back({24, 24, std::vector<uint8_t>(576, 0)});
    for (int n = 0; n < 3; ++n) {
      Object o; o.id = uint32_t(w.objects.size() + 1); o.side = side; o.town = side;
      o.type = n ? 0 : 11; o.town_hall = n == 0; o.hp = n ? 6000 : 30000;
      o.teleport(tile_center_fixed(sd.start_x + n), tile_center_fixed(sd.start_y));
      if (n == 2) { if (side == 0) o.type = 1; o.hp = 1; o.teleport(tile_center_fixed(11 + side), tile_center_fixed(10)); }
      w.objects.push_back(o);
    }
  }
  w.mission.paths = {{{2, 2}, {20, 20}}};
  return w;
}
static std::shared_ptr<RulesData> rules() {
  auto r = std::make_shared<RulesData>();
  require(r->load_dir(AZTEC_TEST_RULES_DIR), "rules unavailable");
  return r;
}
static void phase_controls(const std::shared_ptr<RulesData>& r) {
  auto w = initial();
  for (auto& object : w.objects) object.cooldown = 1000;
  GameSim sim(r); sim.init(w);
  Effect decoration; decoration.lifetime = 10; decoration.kind = aztec::data::EffectClass::Rubble;
  w.effects.push_back(decoration);
  for (uint32_t source : {1u, 2u}) {
    Effect projectile; projectile.source = source; projectile.flight = true;
    projectile.kind = aztec::data::EffectClass::Arrow;
    projectile.speed = to_fixed(1); projectile.end_x = to_fixed(100);
    w.effects.push_back(projectile);
  }
  std::vector<uint32_t> visited;
  sim.set_combat_observer([&](uint64_t, const char* event, const Effect& e, uint32_t, int, int) {
    if (std::string(event) != "position") return;
    require(w.objects[1].cooldown == 999, "projectile phase ran before units");
    require(w.tick == 0, "global time advanced before projectile phase");
    visited.push_back(e.source);
  });
  w.begin_tick(); sim.tick(w);
  require(visited == std::vector<uint32_t>({2, 1}), "projectiles must walk newest first");
  require(w.effects[0].age == 1 && w.effects[1].age == 1 && w.effects[2].age == 1,
          "effects must update once per tick");
  require(!w.effects[0].projectile_pool && w.effects[1].projectile_pool && w.effects[2].projectile_pool,
          "visual class must not select the lifetime pool");
  visited.clear(); w.begin_tick();
  sim.set_combat_observer([&](uint64_t, const char* event, const Effect& e, uint32_t, int, int) {
    if (std::string(event) == "position") visited.push_back(e.source);
  });
  sim.tick(w);
  require(visited == std::vector<uint32_t>({2, 1}), "projectile storage reversed between ticks");
  auto cold = initial(); cold.mission.tileset = 1; cold.mission.start_time = 7199;
  GameSim clock(r); clock.init(cold); cold.begin_tick(); clock.tick(cold);
  require(cold.day_clock == 7199, "nonzero tileset advanced day clock");
  cold.mission.tileset = 0; cold.begin_tick(); clock.tick(cold);
  require(cold.day_clock == 0, "day clock failed to wrap");
  auto birds = initial(); birds.objects.resize(3);
  for (auto& bird : birds.objects) bird.type = 33;
  GameSim loaded(r); loaded.init(birds);
  require(birds.objects[0].air_min_speed_px == 6 && birds.objects[1].air_min_speed_px == 7 &&
          birds.objects[2].air_min_speed_px == 9 && loaded.random_state() == 3539360597u,
          "mission loading did not consume shared CRT draws in creation order");
  loaded.init(birds);
  require(loaded.random_state() == 3539360597u, "mission reinitialization did not reseed zero");
  auto seeded = initial(); GameSim other(r); other.init(seeded);
  require(sim.random_state() == 0 && other.random_state() == 0, "independent simulations share RNG state");
}
static void unit() {
  require(production_value(8388608, 1, 0, false, false, false) == 5592406, "binary32 half rounding");
  require(production_value(16777217, 1, 1, true, false, false) == 13421774, "computer binary32 rounding");
  require(production_value(INT32_MAX, 1, 2, true, false, false) == INT32_MAX, "production overflow clamp");
  uint32_t zero = 0;
  for (uint32_t expected : {38u, 7719u, 21238u}) require(msvc_rand(zero) == expected, "MSVC seed-zero sequence");
  uint32_t one = 1, other = 0;
  require(msvc_rand(one) == 41 && msvc_rand(other) == 38, "independent CRT-default and mission streams");
  uint32_t wrap = UINT32_MAX;
  require(msvc_rand(wrap) == 35 && wrap == 2316998u, "MSVC unsigned wraparound");
  PoolOrder pool;
  const auto first_slot = pool.insert(1), second_slot = pool.insert(2), third_slot = pool.insert(3);
  require(pool.newest_first() == std::vector<uint32_t>({3, 2, 1}), "pool must walk newest first");
  const auto snapshot = pool.newest_first();
  pool.erase(2); pool.erase(3);
  require(pool.insert(4) == third_slot && pool.insert(5) == second_slot, "pool must reuse slots LIFO");
  require(pool.insert(1) == first_slot, "existing pool identity reinserted");
  require(snapshot == std::vector<uint32_t>({3, 2, 1}) && pool.newest_first() == std::vector<uint32_t>({5, 4, 1}), "pool cursor changed after mutation");
  auto r = rules(); phase_controls(r); auto w = initial(); GameSim sim(r, Modifications::faithful()); sim.init(w);
  auto changes_hash = [&](auto change) {
    auto altered = w; change(altered);
    require(sim.state_hash(w) != sim.state_hash(altered), "missing future-affecting field");
  };
  changes_hash([](auto& v) { ++v.tick; });
  changes_hash([](auto& v) { ++v.script_side; });
  changes_hash([](auto& v) { ++v.side_outcome[1]; });
  changes_hash([](auto& v) { ++v.mission.towns[0].side; });
  changes_hash([](auto& v) { ++v.mission.towns[0].x; });
  changes_hash([](auto& v) { ++v.mission.towns[0].scenario_id; });
  changes_hash([](auto& v) { v.mission.sides[0].only_comp = true; });
  changes_hash([](auto& v) { v.mission.sides[0].disabled.push_back(0); });
  changes_hash([](auto& v) { ++v.mission.paths[0][0].first; });
  changes_hash([](auto& v) { ++v.resources.v[0]; });
  changes_hash([](auto& v) { ++v.objects[0].prev_x; });
  changes_hash([](auto& v) { v.objects[0].source = ObjectSource::Boat; });
  changes_hash([](auto& v) { ++v.visibility.sides[0].w; });
  auto viewed = w; viewed.local_side = 1; viewed.selection.ids = {1}; viewed.fog_version += 20;
  viewed.center_view = true; viewed.center_x = 33; viewed.messages.push_back({"presentation", 100});
  require(sim.state_hash(w) == sim.state_hash(viewed), "view settings enter simulation hash");
  Command on; on.type = Pct::PauseOn; on.side = 1;
  Command off; off.type = Pct::PauseOff; off.side = 0;
  auto hash = sim.state_hash(w); sim.submit(on);
  require(hash != sim.state_hash(w), "pending commands omitted");
  sim.submit(off); w.begin_tick(); sim.tick(w);
  require(sim.paused() && sim.log()[0].cmd.side == 0, "commands must sort by side");
  on.side = 0; sim.submit(on); sim.submit(off); w.begin_tick(); sim.tick(w);
  require(!sim.paused(), "same-side submission order must be stable");
  on.side = 7; sim.submit(on); w.begin_tick(); sim.tick(w);
  require(!sim.paused() && sim.log().back().cmd.side == 7, "reserved side-seven slot applied a command");
  on.side = 0;
  auto clean = initial(); sim.init(clean);
  require(sim.log().empty() && sim.tick_count() == 0, "reinit retains command history");
  std::vector<LoggedCommand> log{{1, on}, {1, off}};
  size_t observed = 0;
  auto replay_world = initial();
  auto replay = GameSim::replay(replay_world, r, log, 2, "", [&](uint64_t tick, const World& state, const GameSim& current) {
    require(state.tick == tick - 1 && current.tick_count() == tick - 1, "wrong observation phase");
    require(current.log().size() == (tick == 1 ? 0u : 2u), "observer runs after current commands");
    ++observed;
  });
  require(observed == 2 && !replay->paused(), "replay observer coverage");
  for (auto bad : {std::vector<LoggedCommand>{{0, on}}, {{2, on}, {1, off}}, {{3, on}}}) {
    auto untouched = initial();
    try { GameSim::replay(untouched, r, bad, 2); throw std::runtime_error("bad replay accepted"); }
    catch (const std::invalid_argument&) { require(untouched.tick == 0, "bad replay mutated world"); }
  }
  ScriptRunner a, b; aztec::script::Script first, second;
  require(aztec::script::parse_script("trigger(0)\n  conditions\n    localTime == 1\n  actions\n    side(0).addMoney(1)\nend\n", first), "synthetic script parse");
  second = first; ++second.triggers[0].actions[0].a;
  a.set(first); b.set(second);
  require(a.state_hash() != b.state_hash(), "script program omitted from hash");
  // Generic birth variants are presentation fallbacks and consume no mission random draws.
  auto births = initial(); births.objects.resize(3); births.mission.sides.resize(1); births.mission.towns.resize(1);
  GameSim birth_sim(r, Modifications::faithful()); birth_sim.init(births);
  Command build; build.type = Pct::Build; build.a = 0;
  for (int i = 0; i < 4; ++i) birth_sim.submit(build);
  for (int tick = 0; tick < 1000 && births.objects.size() < 7; ++tick) { births.begin_tick(); birth_sim.tick(births); }
  require(births.objects.size() == 7, "RNG fixture did not spawn");
  for (size_t i = 3; i < 7; ++i)
    require(births.objects[i].anim_variant == (births.objects[i].id * 2654435761u >> 8), "birth variant differs from placement policy");
  require(birth_sim.random_state() == 0, "generic births consumed simulation draws");
  aztec::path::NavGrid g1(8, 8), g2(8, 8);
  g1.add_block(2, 2, 1, 1, 1); g2.add_block(2, 2, 1, 1, 2);
  require(g1.state_hash() != g2.state_hash(), "blocker counts omitted");
  aztec::path::MoverConfig cfg; cfg.stuck_window++;
  aztec::path::MoveSystem m1(g1), m2(g1, cfg);
  require(m1.state_hash() != m2.state_hash(), "movement configuration omitted");
}

static constexpr uint64_t horizon = 14400, save_tick = 7201;
static void schedule(GameSim& sim, const World& w, uint64_t tick) {
  // Cross-side and same-side pause collisions at one boundary.
  if (tick == 17) {
    Command c; c.type = Pct::PauseOff; c.side = 1; sim.submit(c);
    c.type = Pct::PauseOn; c.side = 0; sim.submit(c);
    c.type = Pct::PauseOff; sim.submit(c); c.type = Pct::PauseOn; sim.submit(c);
  }
  if (tick % 1200 == 1) {
    for (int side = 1; side >= 0; --side) {
      Command build; build.type = Pct::Build; build.side = uint8_t(side); build.a = 0; sim.submit(build);
      Command move; move.type = Pct::AttackMove; move.side = uint8_t(side);
      move.a = (tick % 1200 == 1 ? 12 : 10) * kTilePx; move.b = 12 * kTilePx;
      for (const auto& o : w.objects) if (o.side == side && o.type == 0 && o.alive && !o.removed) move.ids.push_back(o.id);
      sim.submit(move);
    }
  }
  if (tick == 1) {
    Command attack; attack.type = Pct::Attack; attack.side = 0; attack.ids = {3}; attack.a = 6; sim.submit(attack);
    attack.side = 1; attack.ids = {6}; attack.a = 3; sim.submit(attack);
  }
}
static void row(std::ostream& out, const World& w, const GameSim& sim) {
  out << w.tick << ' ' << sim.state_hash(w);
  // Engine IDs are stable append-only indices, never claimed as oracle identities.
  for (const auto& o : w.objects)
    out << ' ' << o.id << ':' << o.type << ':' << o.side << ':' << o.hp << ':' << o.cur_x << ':' << o.cur_y << ':' << o.alive;
  out << '\n';
}
static void run(const std::string& mode, const std::string& trace_path, const std::string& save_path, int view, int frames) {
  auto r = rules(); auto w = initial(); w.local_side = view;
  std::unique_ptr<GameSim> sim;
  if (mode == "resume") {
    SaveData save; require(read_save(save_path, save), "cannot read save");
    require(save.mission == "synthetic-v03" && save.tick == save_tick, "invalid synthetic save");
    sim = GameSim::replay(w, r, save.log, save.tick);
    require(sim->state_hash(w) == save.hash, "continuation hash mismatch before restore");
  } else {
    sim = std::make_unique<GameSim>(r, Modifications::faithful()); sim->init(w);
  }
  std::ofstream trace(trace_path); require(bool(trace), "cannot write trace");
  if (mode == "log-replay") {
    SaveData full; require(read_save(save_path + ".full", full), "cannot read full replay");
    require(full.tick == horizon, "full log horizon mismatch");
    sim = GameSim::replay(w, r, full.log, full.tick, "", [&](uint64_t, const World& before, const GameSim& current) {
      if (before.tick) row(trace, before, current);
    });
    row(trace, w, *sim);
    require(sim->state_hash(w) == full.hash, "full log replay differs");
    require(bool(trace), "trace write failed");
    return;
  }
  Camera camera; camera.set_world(float(w.width_px()), float(w.height_px()));
  AudioSystem audio; audio.set_volumes(view ? 0.f : 1.f, view ? 1.f : 0.f);
  bool moved = false, spawned = false, died = false;
  for (uint64_t tick = w.tick + 1; tick <= horizon; ++tick) {
    schedule(*sim, w, tick); w.begin_tick(); sim->tick(w);
    for (int frame = 0; frame < frames; ++frame) {
      camera.update({.move_x = view ? 1.f : -1.f}, 1.f / (12 * frames));
      for (const auto& o : w.objects) { auto position = o.interpolated(float(frame) / frames); (void)position; }
      audio.update();
    }
    for (const auto& o : w.objects) { moved |= o.body_moving; died |= !o.alive; }
    spawned |= w.objects.size() > 6;
    row(trace, w, *sim);
    if (tick == save_tick && mode == "record") {
      SaveData save; save.mission = "synthetic-v03"; save.tick = tick; save.hash = sim->state_hash(w); save.log = sim->log();
      require(write_save(save_path, save), "cannot write save");
    }
  }
  if (mode == "record") {
    SaveData full; full.mission = "synthetic-v03"; full.tick = horizon; full.hash = sim->state_hash(w); full.log = sim->log();
    require(write_save(save_path + ".full", full), "cannot write full replay");
  }
  require(bool(trace), "trace write failed");
  if (mode != "resume") require(moved && spawned && died, "long scenario did not exercise movement, RNG spawns and combat");
}
int main(int argc, char** argv) {
  try {
    if (argc == 1) unit();
    else {
      require(argc == 6, "expected mode trace save view frames");
      run(argv[1], argv[2], argv[3], std::stoi(argv[4]), std::stoi(argv[5]));
    }
  } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
  return 0;
}
