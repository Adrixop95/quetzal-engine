// Campaign flow, input and synthetic SDL presentation contracts; no original data.
#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include "temp_dir.hpp"
#include "core/endian.hpp"
#include "engine/render/png.hpp"
#include "engine/ui/campaign.hpp"
#include "engine/ui/frontend.hpp"
#include "engine/app/engine.hpp"

using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)
static void write(const std::filesystem::path& path, const std::string& text) { std::ofstream file(path, std::ios::binary); file << text; }

int main(int argc, char** argv) {
  TempDir temp;
  const std::filesystem::path output = argc > 1 ? argv[1] : temp.path() / "captures";
  std::filesystem::create_directories(output);
  const auto root = temp.path();
  std::filesystem::create_directory(root / "Maps");
  std::filesystem::create_directory(root / "Res");
  for (const char* name : {"Pal", "Units", "Tiles0", "Tiles1", "common", "fnts", "light"})
    write(root / "Res" / (std::string(name) + ".xbe"), std::string(4, '\0'));
  Options mod_options; mod_options.modifications_file = (root / "modifications.ini").string();
  CHECK(!mod_options.modifications.parallel_construction);
  ModificationOptions mod_menu; mod_menu.open = true;
  Input mod_click; auto mod_rect = ModificationOptions::row(0, 800, 600, 1);
  mod_click.mx = mod_rect.x + 2; mod_click.my = mod_rect.y + 2; mod_click.pressed[SDL_BUTTON_LEFT] = true;
  mod_menu.update(mod_click, mod_options, 800, 600, 1);
  CHECK(mod_options.modifications.parallel_construction);
  Modifications persisted; CHECK(load_modifications(mod_options.modifications_file, persisted));
  CHECK(persisted.parallel_construction);
  write(root / "invalid-modifications.ini", "unknown=on\n");
  CHECK(!load_modifications((root / "invalid-modifications.ini").string(), persisted));
  CHECK(persisted.parallel_construction); // invalid settings leave the caller unchanged
  std::string mod_path = mod_options.modifications_file;
  std::vector<std::string> args{"engine", "--mod", "parallel_construction=off", "--modifications-file", mod_path};
  std::vector<char*> raw; for (auto& arg : args) raw.push_back(arg.data());
  Options parsed_mods; std::string parse_error; bool usage = false;
  CHECK(parse_options(int(raw.size()), raw.data(), parsed_mods, parse_error, usage));
  CHECK(!parsed_mods.modifications.parallel_construction); // CLI overrides persisted on
  CHECK(!persisted.set("unknown=on") && !persisted.set("parallel_construction=yes"));
  GameData data; std::string error; CHECK(data.open(root.string(), error));
  CHECK(campaign_number("MISSION_15.MP") == 15);
  CHECK(campaign_number("/private/Maps/mission_4.mp") == 4);
  CHECK(campaign_number("1") == 1);
  for (const char* invalid : {"mission_0", "mission_16", "mission_1extra", "mission_01", "mission_", "custom", "-1", "1junk"}) CHECK(campaign_number(invalid) == 0);
  write(root / "Maps/Mission_2.MP", "[Globals]\nNotes=Fallback objective\n");
  CHECK(next_campaign_mission(data, "1") == "mission_2");
  CHECK(next_campaign_mission(data, "mission_2").empty());
  CHECK(next_campaign_mission(data, "mission_15").empty());
  CHECK(next_campaign_mission(data, "custom").empty());
  write(root / "Maps/mission_2.mit", "~Diary\r\nFirst section\r\n~~\r\n~Objectives\r\nReach the exit\r\n~~\r\n");
  const auto briefing = mission_briefing(data, "mission_2");
  CHECK(briefing.find("Diary\nFirst section") != std::string::npos);
  CHECK(briefing.find("Objectives\nReach the exit") != std::string::npos);
  CHECK(mission_briefing(data, "missing").find("No briefing") == 0);
  CampaignProgress progress;
  progress.record("mission_1", false); CHECK(!progress.won(1));
  progress.record("mission_1", true); progress.record("MISSION_15.MP", true);
  progress.record("custom", true); CHECK(progress.completed == 16385);
  CHECK(!progress.won(0) && !progress.won(16));
  const auto path = (root / "engine_campaign.txt").string();
  CHECK(progress.save(path));
  CampaignProgress loaded; CHECK(loaded.load(path) && loaded.completed == progress.completed);
  progress.record("mission_2", true); CHECK(progress.save(path));
  CHECK(loaded.load(path) && loaded.won(2));
  std::filesystem::create_directory(path + ".pending");
  CHECK(!progress.save(path)); CHECK(loaded.load(path) && loaded.won(15));
  std::filesystem::remove(path + ".pending");
  for (const std::string& bad : std::vector<std::string>{"", "QUETZAL_CAMPAIGN_1\n32768\n", "QUETZAL_CAMPAIGN_1\n1junk\n", "QUETZAL_CAMPAIGN_2\n1\n", "QUETZAL_CAMPAIGN_1\n1", std::string(100, 'A')}) {
    write(path, bad); const auto before = loaded.completed;
    CHECK(!loaded.load(path) && loaded.completed == before);
  }
  CHECK(!progress.save((root / "missing/profile").string()));
  for (int n = 1; n <= 15; ++n) progress.record("mission_" + std::to_string(n), true);
  CHECK(progress.completed == 32767 && progress.save(path));
  CHECK(loaded.load(path) && loaded.completed == 32767);

  auto parse = [&](std::vector<std::string> args, Options& options) {
    std::vector<char*> argv;
    for (auto& arg : args) argv.push_back(arg.data());
    std::string error; bool usage = false;
    return parse_options(int(argv.size()), argv.data(), options, error, usage);
  };
  Options parsed;
  CHECK(parse({"engine", "--volume", "0.5", "--speech-volume", "0.2", "--no-subtitles", "--no-acknowledgement"}, parsed));
  CHECK(parsed.sfx_volume == 0.5f && parsed.music_volume == 0.5f && parsed.speech_volume == 0.2f && !parsed.subtitles && !parsed.acknowledgement);
  for (const char* bad : {"nan", "inf", "-1", "2", "", "0.5junk"}) {
    Options invalid;
    CHECK(!parse({"engine", "--speech-volume", bad}, invalid));
  }
  CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
  for (auto size : {SDL_Point{800, 493}, SDL_Point{800, 600}, SDL_Point{1024, 768}, SDL_Point{1600, 1200}}) {
    const int w = size.x, h = size.y, sc = w == 1600 ? 2 : 1;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888); CHECK(surface);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface); CHECK(renderer);
    {
      Font font; CHECK(font.init(renderer));
      auto capture = [&](const std::string& name) {
        std::vector<uint8_t> pixels(size_t(w) * h * 4);
        CHECK(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, pixels.data(), w * 4) == 0);
        for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
        CHECK(write_png((output / (name + "-" + std::to_string(w) + "x" + std::to_string(h) + ".png")).string(), w, h, pixels));
        return pixels;
      };
      for (int i = 0; i < 3; ++i) {
        auto rect = OutcomeMenu::button(i, w, h, sc);
        CHECK(rect.x >= 0 && rect.y >= 0 && rect.x + rect.w <= w && rect.y + rect.h <= h);
        Input in; in.mx = rect.x + 1; in.my = rect.y + 1; in.pressed[SDL_BUTTON_LEFT] = true;
        CHECK(OutcomeMenu::update(in, w, h, sc, true) == (i == 0 ? OutcomeMenu::Action::Continue : i == 1 ? OutcomeMenu::Action::Retry : OutcomeMenu::Action::Menu));
      }
      Input enter; enter.key_hit[SDL_SCANCODE_RETURN] = true;
      CHECK(OutcomeMenu::update(enter, w, h, sc, false) == OutcomeMenu::Action::Retry);
      const auto menu = OutcomeMenu::button(1, w, h, sc);
      Input menu_click; menu_click.mx = menu.x + 1; menu_click.my = menu.y + 1; menu_click.pressed[SDL_BUTTON_LEFT] = true;
      CHECK(OutcomeMenu::update(menu_click, w, h, sc, false) == OutcomeMenu::Action::Menu);
      enter.alt = true; CHECK(OutcomeMenu::update(enter, w, h, sc, true) == OutcomeMenu::Action::None);
      OutcomeMenu::draw(renderer, font, w, h, sc, true, true, false);
      const auto victory_pixels = capture("victory");
      OutcomeMenu::draw(renderer, font, w, h, sc, false, false, false);
      CHECK(capture("defeat") != victory_pixels);
      AudioOptions audio; audio.open = true; Options options;
      for (int i = 0; i < 6; ++i) {
        auto rect = AudioOptions::row(i, w, h, sc);
        CHECK(rect.x >= 0 && rect.y >= 0 && rect.x + rect.w <= w && rect.y + rect.h <= h);
      }
      auto rect = AudioOptions::row(2, w, h, sc);
      Input click; click.mx = rect.x + 1; click.my = rect.y + 1; click.pressed[SDL_BUTTON_LEFT] = true;
      for (int i = 0; i < 20; ++i) audio.update(click, options, w, h, sc);
      CHECK(options.speech_volume == 0 && options.sfx_volume == 0.8f);
      click.mx = rect.x + rect.w - 1;
      for (int i = 0; i < 20; ++i) audio.update(click, options, w, h, sc);
      CHECK(options.speech_volume == 1);
      audio.draw(renderer, font, options, w, h, sc);
      capture("audio");
      TextOverlay overlay; overlay.show("Briefing / objectives", briefing + std::string(50000, 'A'));
      Input end; end.key_hit[SDL_SCANCODE_END] = true; overlay.update(end);
      SDL_Rect clip{10, 10, w - 20, h - 20}; SDL_RenderSetClipRect(renderer, &clip);
      overlay.draw(renderer, font, w, h, sc); CHECK(overlay.scroll > 0);
      const auto end_pixels = capture("briefing-end");
      SDL_Rect restored; SDL_RenderGetClipRect(renderer, &restored);
      CHECK(SDL_RenderIsClipEnabled(renderer) && restored.x == clip.x && restored.w == clip.w);
      Input home; home.key_hit[SDL_SCANCODE_HOME] = true; overlay.update(home); CHECK(overlay.scroll == 0);
      overlay.draw(renderer, font, w, h, sc);
      const auto first_pixels = capture("briefing-start"); CHECK(first_pixels != end_pixels);
      CHECK(std::equal(first_pixels.begin(), first_pixels.begin() + 4, end_pixels.begin()));
      overlay.update(enter); CHECK(overlay.open); // Alt+Enter changes display, not the modal.
      Input escape; escape.key_hit[SDL_SCANCODE_ESCAPE] = true; overlay.update(escape); CHECK(!overlay.open);
    }
    SDL_DestroyRenderer(renderer); SDL_FreeSurface(surface);
  }
  SDL_Quit();
  // Exercise the real mission loop: modal clicks/keys must not leak to commands or advance time.
  write(root / "Maps/mission_1.mp", "[Globals]\nMapSize=8,8\nSides=1\nTeams=1\n[Side0]\nStart_X=1\nStart_Y=1\n[Team0]\nSide=0\nUnits=1\nUnit0=1,1,0,100\n");
  std::string map(4 + 8 * 8 * 4, '\0');
  aztec::write_le<uint16_t>(reinterpret_cast<uint8_t*>(map.data()), 8);
  aztec::write_le<uint16_t>(reinterpret_cast<uint8_t*>(map.data()) + 2, 8);
  write(root / "Maps/mission_1.map", map);
  write(root / "Maps/mission_1.scn", "trigger(0) conditions localTime == 6 actions side(local).victory end\n");
  write(root / "controls.txt",
      "key Escape\nexpect tick == 0\nclick 400 452\nclick 400 310\nexpect tick == 0\nkey Escape\nclick 400 200\nexpect tick == 0\n"
      "key M\nwait 2\nexpect tick == 0\nkey Escape\n"
      "click 400 238\nexpect tick == 0\nclick 590 270\nwait 2\nexpect tick == 0\n"
      "click 400 410\nclick 400 160\nexpect tick == 1\nwait 10\n"
      "expect outcome == 1\nexpect tick == 6\nkey M\nwait 2\nexpect tick == 6\nquit\n");
  Options options; options.modifications_file = (root / "mission-modifications.ini").string(); options.game_dir = root.string(); options.mission = "1";
  options.rules_dir = AZTEC_TEST_RULES_DIR; options.menu_session = true;
  options.script = (root / "controls.txt").string(); options.audio = false; options.light = false;
  options.window.width = 800; options.window.height = 600; options.window.software = true; options.window.vsync = false;
  CHECK(run_engine(options) == 0);
  return failures ? 1 : 0;
}
