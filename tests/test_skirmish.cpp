// Entirely invented maps and scripts; no installed game data or reference process.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include "temp_dir.hpp"
#include "core/endian.hpp"
#include "engine/app/engine.hpp"
#include "engine/app/headless.hpp"
#include "engine/assets/skirmish.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/sim/savegame.hpp"
#include "engine/ui/campaign.hpp"
#include "engine/ui/frontend.hpp"

using namespace aztec::engine;
namespace fs = std::filesystem;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)
static void write(const fs::path& path, const std::string& bytes) {
  std::ofstream file(path, std::ios::binary); file << bytes; CHECK(bool(file));
}
static std::string read(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), {});
}
static const std::string spec = "Arena (test)'s";
static void fixture(const fs::path& root) {
  fs::create_directory(root / "Res"); fs::create_directory(root / "Maps");
  for (const char* name : {"Pal", "Units", "Tiles0", "Tiles1", "common", "fnts", "light"})
    write(root / "Res" / (std::string(name) + ".xbe"), std::string(4, '\0'));
  write(root / "Maps" / (spec + ".MP"),
      "[Globals]\nName=Alpha Arena\nMapSize=32,24\nSides=3\nTowns=2\nTeams=1\n"
      "[Side0]\nNeutral=1\nColor=7\nStart_X=-1\nStart_Y=-1\n"
      "[Side1]\nRace=0\nColor=4\nStartMoney=4567\nOnlyComp=1\nStart_X=6\nStart_Y=6\nDisabledID=24\n"
      "[Side2]\nRace=2\nColor=1\nStartMoney=9876\nOnlyComp=0\nStart_X=25\nStart_Y=17\n"
      "[Town0]\nSide=1\nX=6\nY=6\nType=1\nRace=0\n"
      "[Town1]\nSide=2\nX=25\nY=17\nType=1\nRace=2\n"
      "[Team0]\nSide=1\nUnits=1\nUnit0=9,6,0,200\n");
  std::string terrain(4 + 32 * 24 * 4, '\0');
  aztec::write_le<uint16_t>(reinterpret_cast<uint8_t*>(terrain.data()), 32);
  aztec::write_le<uint16_t>(reinterpret_cast<uint8_t*>(terrain.data()) + 2, 24);
  write(root / "Maps" / (spec + ".map"), terrain);
  write(root / "Maps" / (spec + ".scn"),
      "trigger(17) conditions localTime == 120 actions side(local).victory end\n");
  write(root / "Maps" / "mission_1.mp", "[Globals]\nName=Campaign only\n");
  write(root / "Maps" / "other.mp", "[Globals]\nName=Zulu\nMapSize=17,19\n");
  write(root / "Maps" / "duplicate.mp", "[Globals]\nName=Alpha Arena\nMapSize=13,11\n");
  write(root / "Maps" / "empty.mp", "[Globals]\n");
  write(root / "Maps" / "broken.mp", "not an INI mission\n");
  fs::create_directory(root / "Maps" / "directory.mp");
}
static void advance(GameSim& sim, World& world, int ticks) {
  for (int i = 0; i < ticks; ++i) { world.begin_tick(); sim.tick(world); }
}
static void setup_and_replay(GameData& data, const fs::path& root) {
  std::string error;
  const auto maps = custom_maps(data.maps_dir());
  CHECK(maps.size() == 4);
  if (maps.size() != 4) return;
  CHECK(maps[0].spec == spec && maps[0].title == "Alpha Arena");
  CHECK(maps[0].width == 32 && maps[0].height == 24);
  CHECK(maps[1].spec == "duplicate" && maps[1].title == "Alpha Arena");
  CHECK(maps[2].title == "Zulu" && maps[3].title == "empty");
  CHECK(custom_maps((root / "missing").string()).empty());
  World world;
  CHECK(load_world(data, spec, world, error));
  CHECK(!world.skirmish && world.mission.sides[1].only_comp && !world.mission.sides[2].only_comp);
  CHECK(world.side_state[0].money == 100); // absent StartMoney retains parser default
  CHECK(setup_skirmish(world, error));
  CHECK(world.skirmish && world.local_side == 1 && world.script_side == 1);
  CHECK(!world.mission.sides[1].only_comp && world.mission.sides[2].only_comp);
  CHECK(world.mission.sides[0].only_comp && world.mission.sides[0].neutral);
  CHECK(world.side_color(1) == 4 && world.side_color(2) == 1);
  CHECK(world.side_state[1].money == 4567 && world.side_state[2].money == 9876);
  CHECK(world.mission.sides[1].disabled == std::vector<int>{24});
  CHECK(world.relation[1][0] == kEnemy && world.relation[1][2] == kEnemy);
  CHECK(world.enemy_all(0)); // neutral descriptor uses develop's all-enemies sentinel
  CHECK(world.objects.size() == 3 && world.mission.towns.size() == 2);
  World disabled = world; disabled.mission.sides[1].start_x = -1;
  CHECK(setup_skirmish(disabled, error) && disabled.script_side == 2);
  CHECK(disabled.mission.sides[1].only_comp && !disabled.mission.sides[2].only_comp);
  for (int invalid = 0; invalid < 5; ++invalid) {
    World bad = world; bad.skirmish = false;
    if (invalid == 0) bad.mission.sides[1].color = 8;
    if (invalid == 1) bad.mission.sides[1].index = 0;
    if (invalid == 2) bad.mission.sides[1].race = -1;
    if (invalid == 3) for (auto& side : bad.mission.sides) side.neutral = true;
    if (invalid == 4) bad.tick = 1;
    CHECK(!setup_skirmish(bad, error) && !bad.skirmish);
    CHECK(!bad.mission.sides[1].only_comp && bad.mission.sides[2].only_comp);
  }
  auto rules = std::make_shared<RulesData>(); CHECK(rules->load_dir(AZTEC_TEST_RULES_DIR));
  const auto script = data.map_file(spec, "scn");
  GameSim sim(rules); CHECK(sim.attach_script(script)); sim.init(world);
  advance(sim, world, 60);
  CHECK(world.outcome == kRunning && world.ai_towns.size() == 2);
  CHECK(world.ai_towns[0].elapsed == 0 && world.ai_towns[1].elapsed > 0);
  SaveData save;
  save.skirmish = true; save.mission = spec; save.tick = 60;
  save.local_side = uint32_t(world.local_side); save.selection = world.selection.ids;
  save.hash = sim.state_hash(world); save.script_hash = file_hash(script);
  save.modifications = sim.initial_modifications(); save.log = sim.log();
  CHECK(write_save((root / "custom.azs").string(), save));
  SaveData decoded; CHECK(read_save((root / "custom.azs").string(), decoded));
  CHECK(decoded.skirmish && decoded.mission == spec);
  World fresh; CHECK(load_world(data, spec, fresh, error));
  auto resumed = replay_save(decoded, fresh, rules, 0, script, error);
  CHECK(bool(resumed));
  if (!resumed) { std::printf("Replay: %s\n", error.c_str()); return; }
  CHECK(fresh.skirmish && fresh.script_side == 1 && fresh.mission.sides[2].only_comp);
  CHECK(fresh.local_side == world.local_side && fresh.selection.ids == world.selection.ids);
  CHECK(resumed->state_hash(fresh) == sim.state_hash(world));
  advance(sim, world, 60); advance(*resumed, fresh, 60);
  CHECK(world.outcome == kVictory && fresh.outcome == kVictory);
  CHECK(sim.state_hash(world) == resumed->state_hash(fresh));
  World wrong; CHECK(load_world(data, spec, wrong, error));
  decoded.skirmish = false;
  CHECK(!replay_save(decoded, wrong, rules, 0, script, error));
  CHECK(!wrong.skirmish && wrong.tick == 0); // failed replay is transactional

  // Invented elimination script explicitly makes neutral survivors non-hostile.
  write(root / "Maps" / (spec + ".scn"),
      "trigger(40) conditions localTime == 1 actions side(local).setNeutralTo(0) trigger(40).disable end\n"
      "trigger(41) conditions localTime >= 3 side(local).enemies < 1 actions side(local).victory end\n"
      "trigger(42) conditions localTime >= 3 side(local).towns < 1 side(local).units < 1 actions side(local).defeat end\n");
  for (bool lose : {false, true}) {
    World elimination; CHECK(load_world(data, spec, elimination, error));
    CHECK(setup_skirmish(elimination, error));
    Object neutral; neutral.id = uint32_t(elimination.objects.size() + 1); neutral.type = 0; neutral.side = 0;
    neutral.teleport(tile_center_fixed(15), tile_center_fixed(3)); elimination.objects.push_back(neutral);
    GameSim battle(rules); CHECK(battle.attach_script(script)); battle.init(elimination);
    advance(battle, elimination, 2); CHECK(elimination.outcome == kRunning);
    for (const auto& object : elimination.objects)
      if (object.side == (lose ? 1 : 2)) battle.script_kill(elimination, object.id);
    advance(battle, elimination, 1);
    CHECK(elimination.outcome == (lose ? kDefeat : kVictory));
  }
  // The map script also supports objectives other than elimination.
  World defeat; CHECK(load_world(data, spec, defeat, error)); CHECK(setup_skirmish(defeat, error));
  write(root / "Maps" / (spec + ".scn"),
      "trigger(23) conditions localTime == 9 actions side(local).defeat end\n");
  GameSim losing(rules); CHECK(losing.attach_script(script)); losing.init(defeat);
  advance(losing, defeat, 9); CHECK(defeat.outcome == kDefeat);
}
static int headless(std::vector<std::string> args) {
  std::vector<char*> argv;
  for (auto& arg : args) argv.push_back(arg.data());
  return run_headless(int(argv.size()), argv.data());
}
static void command_line(const fs::path& root) {
  const auto report = (root / "headless.json").string();
  const auto save = (root / "headless.azs").string();
  const std::vector<std::string> args = {"test", "--game", root.string(),
      "--rules-dir", AZTEC_TEST_RULES_DIR, "--no-auto-orders", "--max-ticks", "12", "--report", report};
  auto first = args; first.insert(first.end(), {"--skirmish", "--save-at", "4", "--save", save, spec});
  CHECK(headless(first) == 0);
  const auto expected = read(report);
  CHECK(expected.find("\"status\":\"defeat\"") != std::string::npos);
  SaveData saved; CHECK(read_save(save, saved) && saved.skirmish);
  auto resume = args; resume.insert(resume.end(), {"--resume", save, spec});
  CHECK(headless(resume) == 0); // mode comes from the save without repeating --skirmish
  CHECK(read(report).find("\"status\":\"defeat\"") != std::string::npos);
  auto repeat = args; repeat.insert(repeat.end(), {"--skirmish", spec});
  CHECK(headless(repeat) == 0); CHECK(read(report) == expected);
  Options options; std::string error; bool usage = false;
  std::vector<std::string> cli = {"test", "--game", root.string(), "--skirmish", spec};
  std::vector<char*> argv; for (auto& arg : cli) argv.push_back(arg.data());
  CHECK(parse_options(int(argv.size()), argv.data(), options, error, usage));
  CHECK(options.skirmish && !options.menu && options.mission == spec);
}
class FirstDefeat : public Simulation {
 public:
  explicit FirstDefeat(int& ticks) : ticks_(ticks) {}
  void tick(World& world) override { ++ticks_; if (++world.tick == 9) world.outcome = kDefeat; }
 private:
  int& ticks_;
};
static void frontend_and_play(GameData& data, const fs::path& root) {
  Options options;
  options.game_dir = root.string(); options.rules_dir = AZTEC_TEST_RULES_DIR;
  options.audio = options.light = false; options.window.software = true; options.window.vsync = false;
  options.window.width = 800; options.window.height = 600;
  options.menu_script = (root / "menu.txt").string(); options.menu_trace = (root / "menu.jsonl").string();
  write(options.menu_script, "0 move 160 540\n0 down\n1 up\n2 move 640 58\n2 down\n3 up\n4 move 160 260\n4 down\n5 up\n6 keydown Return\n");
  CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
  FrontendResult result;
  {
    Window window; CHECK(window.create(options.window));
    CHECK(run_frontend(window, data, options, result));
  }
  SDL_Quit();
  CHECK(result.skirmish && result.mission == spec && !result.failed && !result.quit);
  const auto menu = read(options.menu_trace);
  CHECK(menu.find("Alpha Arena (32 x 24)") != std::string::npos);
  CHECK(menu.find("\"rendered_screen\":\"custom-setup\"") != std::string::npos);
  CHECK(menu.find("\"rendered_screen\":\"loading\"") != std::string::npos);
  options.mission = result.mission; options.skirmish = result.skirmish;
  options.menu_session = true; options.menu = false;
  options.script = (root / "play.txt").string(); options.ui_trace = (root / "play.jsonl").string();
  write(options.menu_script, "0 close\n");
  write(options.script, "expect tick == 0\nuntil 9\nexpect outcome == 2\nwait 2\nexpect tick == 9\nkey Escape\n");
  CHECK(run_engine(options) == 0);
  CHECK(read(options.ui_trace).find("\"screen\":\"mission-result-defeat\"") != std::string::npos);
  CHECK(read(options.menu_trace).find("\"rendered_screen\":\"main\"") != std::string::npos);
  write(root / "Maps" / (spec + ".scn"),
      "trigger(35) conditions localTime == 9 actions side(local).victory end\n");
  write(options.script, "expect tick == 0\nuntil 9\nwait 2\nexpect tick == 9\nkey Return\n");
  int ticks = 0;
  CHECK(run_engine(options, {}, std::make_unique<FirstDefeat>(ticks)) == 0);
  CHECK(ticks == 9); // Retry rebuilds the real simulation and reaches the generated victory.
  CHECK(read(options.ui_trace).find("\"outcome\":1") != std::string::npos);
  CHECK(read(options.menu_trace).find("\"rendered_screen\":\"main\"") != std::string::npos);
  CHECK(!fs::exists(user_file("Saves/engine_campaign.txt")));
  // A missing terrain or invalid script disables Start while Back remains available.
  fs::remove(root / "Maps" / (spec + ".map"));
  write(options.menu_script, "0 move 160 540\n0 down\n1 up\n2 move 640 58\n2 down\n3 up\n4 move 160 260\n4 down\n5 up\n6 keydown Return\n7 keyup Return\n8 close\n");
  CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
  {
    Window window; CHECK(window.create(options.window));
    FrontendResult rejected; CHECK(!run_frontend(window, data, options, rejected));
    CHECK(rejected.mission.empty());
  }
  SDL_Quit();
  CHECK(read(options.menu_trace).find("\"label\":\"Start\",\"rect\":[550,535,170,36],\"enabled\":false") != std::string::npos);
}
int main() {
  SDL_SetMainReady();
  TempDir temp; const auto root = temp.path(); fixture(root);
  CHECK(SDL_setenv("AZTEC_USER_DIR", (root / "profile").string().c_str(), 1) == 0);
  GameData data; std::string error; CHECK(data.open(root.string(), error));
  setup_and_replay(data, root); command_line(root); frontend_and_play(data, root);
  return failures ? 1 : 0;
}
