// Invented full-state scenarios. No campaign files or executable inputs are read.
#include "engine/sim/game_sim.hpp"
#include "engine/sim/savegame.hpp"
#include "engine/sim/sight.hpp"
#include "temp_dir.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace aztec::engine;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void write(const std::filesystem::path& dir, const char* file, const char* text) {
  std::ofstream out(dir / file); out << text; check(bool(out), "fixture write failed");
}
static std::shared_ptr<RulesData> rules(const std::filesystem::path& dir) {
  write(dir, "types.csv", "id,name,kind,param,price,time,development\n"
      "0,unit,0,0,37,80,0\n11,hall,2,0,0,0,1\n14,structure,2,0,43,120,1\n");
  write(dir, "desc_marine.csv", "param,d0,d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13\n"
      "0,0,100000,0,100000,100000,8,0,1,0,0,0,65537,256,0\n");
  write(dir, "desc_building.csv", "param,d0,d1,d2,d3,d4,d5\n0,0,1000,0,0,48,48\n");
  write(dir, "weapons_ids.csv", "id,kind,param\n1,1,0\n");
  write(dir, "weapon_class_arrow1.csv", "param,anim,speed,reload,range,damage,armor,unused,air,vehicle,organic,other,structure,child,shock\n"
      "0,1,8,19,600,37,0,0,256,256,256,256,256,0,0\n");
  write(dir, "townhall_ids.csv", "level,a,b,c\n1,11,38,64\n");
  write(dir, "buildtree.csv", "race,level,slot,type,map,prereq\n0,1,0,0,0,\n0,1,1,14,1,\n");
  auto r = std::make_shared<RulesData>(); check(r->load_dir(dir.string()), "rules load failed");
  auto sight = std::make_shared<SightData>();
  for (auto& day : sight->daylight) day.fill(100);
  for (int id : {r->type(0).vision_mask, 71}) {
    auto& mask = sight->masks[id]; mask.assign(404, 152);
    mask[0] = mask[2] = 20; mask[1] = mask[3] = 0;
    for (int y = 8; y <= 12; ++y) for (int x = 8; x <= 12; ++x) mask[4 + y * 20 + x] = 0;
  }
  r->set_sight(sight); return r;
}
static World initial(int mission) {
  World w; w.mission.file_name = "mission_" + std::to_string(mission);
  w.mission.start_time = mission * 13;
  w.terrain.w = w.terrain.h = 32; w.terrain.cells.resize(1024);
  w.passability = {32, 32, std::vector<uint8_t>(1024, 2)};
  for (int y = 0; y < 32; ++y) for (int x = 5; x <= 10; ++x) w.passability.v[y * 32 + x] = 0;
  w.resources = {32, 32, std::vector<uint16_t>(1024, 0x0080)};
  w.side_state.resize(8); w.local_side = mission % 2;
  for (int side = 0; side < 2; ++side) {
    SideDef s; s.index = side; s.only_comp = side == 1; w.mission.sides.push_back(s);
    TownDef t; t.index = side; t.side = side; t.type = 1; t.x = side ? 27 : 18; t.y = side ? 27 : 8;
    w.mission.towns.push_back(t); w.side_state[side].money = 100000;
    w.visibility.sides.push_back({32, 32, std::vector<uint8_t>(1024, 152)});
  }
  auto add = [&](int type, int side, int x, int y, int town = -1) {
    Object o; o.id = uint32_t(w.objects.size() + 1); o.type = type; o.side = side; o.town = town;
    o.hp = type == 11 ? 1000 : 100000; o.town_hall = type == 11;
    o.teleport(tile_center_fixed(x), tile_center_fixed(y)); w.objects.push_back(o);
  };
  add(84, 0, 5, 7); add(0, 0, 3, 7); add(0, 0, 18, 18); add(0, 1, 23, 18);
  add(11, 0, 18, 8, 0); add(11, 1, 27, 27, 1);
  return w;
}
static void schedule(GameSim& sim, uint64_t tick) {
  auto send = [&](Pct type, std::vector<uint32_t> ids, int a = 0, int b = 0, int c = 0) {
    Command cmd; cmd.type = type; cmd.ids = std::move(ids); cmd.a = a; cmd.b = b; cmd.c = c; sim.submit(cmd);
  };
  if (tick == 1) {
    send(Pct::Board, {2}, 1); send(Pct::Attack, {3}, 4);
    send(Pct::CreateGroup, {3, 2}, 4); send(Pct::SelectGroup, {}, 4);
    send(Pct::PlaceBuilding, {5}, 14, 18, 12); send(Pct::Build, {5}, 0); send(Pct::Build, {5}, 0);
  }
  if (tick == 12) send(Pct::PauseBuild, {5}, 1);
  if (tick == 24) send(Pct::PauseBuild, {5}, 0);
  if (tick == 45) send(Pct::PauseOn, {});
  if (tick == 48) send(Pct::PauseOff, {});
  if (tick == 130) send(Pct::Move, {1}, 10 * 48 + 24, 7 * 48 + 24);
  if (tick == 190) send(Pct::Unload, {1}, 11 * 48 + 24, 7 * 48 + 24);
  if (tick == 260) send(Pct::Move, {2}, 14 * 48 + 24, 10 * 48 + 24);
}
static void row(std::ostream& out, const World& w, const GameSim& sim) {
  out << sim.tick_count() << ' ' << sim.state_hash(w) << ' ' << sim.script().state_hash()
      << ' ' << w.local_side << ' ' << w.center_serial << ':' << w.center_x << ':' << w.center_y;
  for (const auto& message : w.messages) out << " message:" << message.ticks_left << ':' << message.text;
  out << '\n'; check(bool(out), "trace write failed");
}
int main(int argc, char** argv) {
  try {
    check(argc == 6, "expected record|resume DIRECTORY MISSION CHECKPOINT HORIZON");
    const std::string mode = argv[1]; const std::filesystem::path root = argv[2];
    const int mission = std::stoi(argv[3]); const auto checkpoint = std::stoull(argv[4]), horizon = std::stoull(argv[5]);
    check((mode == "record" || mode == "resume") && mission >= 1 && mission <= 15 && checkpoint < horizon,
          "invalid scenario arguments");
    TempDir fixture; auto r = rules(fixture.path());
    write(fixture.path(), "test.scn",
        "trigger(0) conditions localTime == 1 actions timer(1).set(0) trigger(0).disable end\n"
        "trigger(1) conditions timer(1) == 7 actions timer(1).stop trigger(1).disable end\n"
        "trigger(2) conditions localTime == 35 actions timer(1).start trigger(2).disable end\n"
        "trigger(3) conditions always actions side(0).addMoney(1) end\n"
        "trigger(4) conditions localTime == 90 actions side(0).setNeutralTo(1) trigger(4).disable end\n"
        "trigger(5) conditions localTime == 110 actions side(0).setEnemyTo(1) trigger(5).disable end\n"
        "trigger(6) conditions localTime == 3 actions centerView(12,12) printString(\"Synthetic objective\") trigger(6).disable end\n"
        "trigger(7) conditions localTime == 350 actions side(local).victory trigger(7).disable end\n");
    const auto script = (fixture.path() / "test.scn").string();
    auto w = initial(mission); std::unique_ptr<GameSim> sim;
    const auto slot = root / ("save-" + std::to_string(checkpoint) + ".azs");
    if (mode == "resume") {
      SaveData save; std::string error;
      if (!read_save(slot.string(), save, &error)) throw std::runtime_error(error);
      sim = replay_save(save, w, r, 0, script, error); check(bool(sim), error.c_str());
      check(w.selection.ids == save.selection, "UI selection was lost");
      check(save.cam_x == -12.5f && save.cam_y == 200 && save.cam_zoom == 1.5f, "camera was lost");
    } else {
      sim = std::make_unique<GameSim>(r); check(sim->attach_script(script), "script parse failed"); sim->init(w);
      w.selection.ids = {3, 1};
    }
    std::ofstream trace(root / (mode + "-" + std::to_string(checkpoint) + ".txt"));
    auto save = [&]() {
      if (checkpoint == 1) {
        check(w.objects[1].boarding && !w.effects.empty(), "save must occur during boarding and combat");
        check(w.objects.size() > 6 && w.objects[6].progress < 1000, "save must occur during construction");
      }
      if (checkpoint == 120) check(!w.objects[0].cargo.empty(), "cargo checkpoint was missed");
      SaveData s; s.mission = w.mission.file_name; s.tick = sim->tick_count(); s.hash = sim->state_hash(w);
      s.script_hash = file_hash(script); s.log = sim->log(); s.modifications = sim->initial_modifications();
      s.selection = w.selection.ids; s.local_side = uint32_t(w.local_side); s.cam_x = -12.5f; s.cam_y = 200; s.cam_zoom = 1.5f;
      check(!sim->has_pending_commands() && write_save(slot.string(), s), "save failed");
    };
    row(trace, w, *sim);
    if (mode == "record" && checkpoint == 0) save();
    bool boarding = false, cargo = false, construction = false, projectile = false, queued = false, birth = false;
    while (sim->tick_count() < horizon) {
      schedule(*sim, sim->tick_count() + 1); w.begin_tick(); sim->tick(w); row(trace, w, *sim);
      boarding |= w.objects[1].boarding != 0; cargo |= !w.objects[0].cargo.empty();
      for (const auto& o : w.objects) construction |= o.progress < 1000;
      for (const auto& e : w.effects) projectile |= e.flight;
      queued |= w.side_state[0].queue.size() >= 2; birth |= w.objects.size() > 7;
      if (mode == "record" && sim->tick_count() == checkpoint) save();
    }
    if (mode == "record") {
      check(boarding && cargo && construction && projectile && queued && birth, "scenario did not exercise required state");
      check(w.visibility_brightness && w.fog_version > 1, "fog did not evolve");
      check(w.ai_towns[1].elapsed > 0 && w.outcome == kVictory && !sim->script().trigger_enabled(0), "AI/script coverage missing");
    }
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
