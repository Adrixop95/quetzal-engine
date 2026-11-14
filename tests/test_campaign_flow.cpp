// Invented campaign fixtures exercise presentation transitions, never original routes.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include "temp_dir.hpp"
#include "core/endian.hpp"
#include "engine/app/engine.hpp"
#include "engine/ui/campaign.hpp"

using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)
static void write(const std::filesystem::path& path, const std::string& text) {
  std::ofstream file(path, std::ios::binary); file << text;
  CHECK(bool(file));
}
static std::string read(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), {});
}

// Only the first attempt uses this double. Retry must construct a real GameSim.
class TerminalAttempt : public Simulation {
 public:
  TerminalAttempt(int& ticks, int outcome) : ticks_(ticks), outcome_(outcome) {}
  void tick(World& world) override {
    ++world.tick; ++ticks_;
    if (world.tick == 6) world.outcome = outcome_;
  }
 private:
  int& ticks_;
  int outcome_;
};

int main() {
  TempDir temp;
  const auto root = temp.path();
  CHECK(SDL_setenv("AZTEC_USER_DIR", (root / "profile").string().c_str(), 1) == 0);
  std::filesystem::create_directory(root / "Res");
  std::filesystem::create_directory(root / "Maps");
  for (const char* name : {"Pal", "Units", "Tiles0", "Tiles1", "common", "fnts", "light"})
    write(root / "Res" / (std::string(name) + ".xbe"), std::string(4, '\0'));
  std::string terrain(4 + 8 * 8 * 4, '\0');
  aztec::write_le<uint16_t>(reinterpret_cast<uint8_t*>(terrain.data()), 8);
  aztec::write_le<uint16_t>(reinterpret_cast<uint8_t*>(terrain.data()) + 2, 8);
  for (int n = 1; n <= 15; ++n) {
    const auto base = root / "Maps" / ("mission_" + std::to_string(n));
    write(base.string() + ".mp", "[Globals]\nMapSize=8,8\nSides=1\nTeams=1\n[Side0]\nStart_X=1\nStart_Y=1\n[Team0]\nSide=0\nUnits=1\nUnit0=1,1,0,100\n");
    write(base.string() + ".map", terrain);
    write(base.string() + ".mit", "~Synthetic mission " + std::to_string(n) + "\nWait for the fixture outcome\n~~\n");
    write(base.string() + ".scn", "trigger(0) conditions localTime == 6 actions side(local).victory end\n");
  }
  Options options;
  options.game_dir = root.string(); options.rules_dir = AZTEC_TEST_RULES_DIR;
  options.audio = false; options.light = false; options.menu_session = true;
  options.window.width = 800; options.window.height = 600;
  options.window.software = true; options.window.vsync = false;
  options.script = (root / "play.txt").string();
  options.menu_script = (root / "menu.txt").string();
  options.menu_trace = (root / "menu.jsonl").string();
  options.ui_trace = (root / "play.jsonl").string();
  options.modifications_file = (root / "modifications.ini").string();
  const auto profile = user_file("Saves/engine_campaign.txt");
  std::filesystem::create_directories(std::filesystem::path(profile).parent_path());
  CampaignProgress progress;
  CHECK(progress.save(profile));
  for (int n = 1; n <= 15; ++n) {
    const auto spec = "mission_" + std::to_string(n);
    const auto before = read(profile);
    options.mission = spec;
    // Actual campaign-button selection and briefing Start for each unlocked mission.
    options.menu = true;
    const int y = 260 + ((n - 1) % 9) * 20;
    std::string menu_input = "0 move 160 540\n0 down\n1 up\n";
    int frame = 2;
    if (n > 9) { menu_input += "2 move 645 421\n2 down\n3 up\n"; frame = 4; }
    menu_input += std::to_string(frame) + " move 160 " + std::to_string(y) + "\n" +
        std::to_string(frame) + " down\n" + std::to_string(frame + 1) + " up\n" +
        std::to_string(frame + 2) + " keydown Return\n" + std::to_string(frame + 3) + " keyup Return\n" +
        std::to_string(frame + 4) + " keydown Return\n";
    write(options.menu_script, menu_input);
    CHECK(run_engine(options) == 0);
    CHECK(read(options.menu_trace).find("\"mission\":\"" + spec + "\"") != std::string::npos);
    CHECK(read(options.menu_trace).find("\"next_screen\":\"briefing\"") != std::string::npos);
    options.menu = false;
    write(options.menu_script, "0 keydown Return\n1 close\n");
    // Objective review is modal; the fixture reaches victory through the normal script runner.
    write(options.script, "expect tick == 0\nexpect outcome == 0\nkey Escape\nclick 400 200\nwait 2\nexpect tick == 0\nkey Escape\nclick 400 160\nuntil 6\nexpect outcome == 1\nexpect tick == 6\nwait 2\nexpect tick == 6\nkey Return\n");
    CHECK(run_engine(options) == 0);
    const auto victory = read(options.ui_trace);
    CHECK(victory.find("\"screen\":\"objectives\"") != std::string::npos);
    CHECK(victory.find(n == 15 ? "\"screen\":\"victory\"" : "\"screen\":\"mission-result-victory\"") != std::string::npos);
    const auto next = read(options.menu_trace);
    CHECK(next.find(n == 15 ? "\"rendered_screen\":\"main\"" : "\"rendered_screen\":\"briefing\"") != std::string::npos);
    if (n < 15) CHECK(next.find("mission_" + std::to_string(n + 1)) != std::string::npos);
    CHECK(read(profile) == before); // Playtest completion intentionally cannot unlock a real profile.

    // Defeat and return use the ordinary script dispatch, without changing progression.
    const auto scn = root / "Maps" / (spec + ".scn");
    write(scn, "trigger(0) conditions localTime == 6 actions side(local).defeat end\n");
    write(options.script, "expect tick == 0\nuntil 6\nexpect outcome == 2\nwait 2\nexpect tick == 6\nkey Escape\n");
    CHECK(run_engine(options) == 0);
    CHECK(read(options.ui_trace).find("\"screen\":\"mission-result-defeat\"") != std::string::npos);
    CHECK(read(options.menu_trace).find("\"rendered_screen\":\"main\"") != std::string::npos);
    CHECK(read(profile) == before);

    // Retry the injected defeat. The same input script starts at tick zero again,
    // then reaches the normal victory trigger and continues to the next briefing.
    write(scn, "trigger(0) conditions localTime == 6 actions side(local).victory end\n");
    write(options.script, "expect tick == 0\nexpect outcome == 0\nuntil 6\nexpect tick == 6\nkey Return\n");
    int first_ticks = 0;
    CHECK(run_engine(options, {}, std::make_unique<TerminalAttempt>(first_ticks, kDefeat)) == 0);
    CHECK(first_ticks == 6);
    CHECK(read(options.ui_trace).find("\"outcome\":1") != std::string::npos);
    CHECK(read(options.menu_trace).find(n == 15 ? "\"rendered_screen\":\"main\"" : "\"rendered_screen\":\"briefing\"") != std::string::npos);
    CHECK(read(profile) == before);

    // On victory, the second button restarts; on the following defeat it returns.
    // Different fixture outcomes make a mistaken return or endless restart fail.
    write(scn, "trigger(0) conditions localTime == 6 actions side(local).defeat end\n");
    write(options.script, "expect tick == 0\nexpect outcome == 0\nuntil 6\nexpect tick == 6\nclick 400 355\n");
    first_ticks = 0;
    CHECK(run_engine(options, {}, std::make_unique<TerminalAttempt>(first_ticks, kVictory)) == 0);
    CHECK(first_ticks == 6);
    CHECK(read(options.ui_trace).find("\"outcome\":2") != std::string::npos);
    CHECK(read(options.menu_trace).find("\"rendered_screen\":\"main\"") != std::string::npos);
    CHECK(read(profile) == before);

    // Test the separate persistence contract; this is not an observed player victory.
    progress.record(spec, false); CHECK(progress.completed == (1u << (n - 1)) - 1);
    progress.record(spec, true); CHECK(progress.completed == (1u << n) - 1);
    CHECK(progress.save(profile));
    CampaignProgress reopened; CHECK(reopened.load(profile));
    CHECK(reopened.completed == progress.completed && reopened.won(n));
    if (n < 15) CHECK(reopened.unlocked(n + 1));
    if (n < 14) CHECK(!reopened.unlocked(n + 2));
    std::printf("S mission %d: victory, defeat/return, retry/victory, %s; profile mask %u\n",
                n, n == 15 ? "final ending/menu" : "next briefing", unsigned(reopened.completed));
  }
  return failures ? 1 : 0;
}
