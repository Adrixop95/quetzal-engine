// Synthetic byte-order, alignment and case-sensitive filesystem regression checks.
#include "core/endian.hpp"
#include "engine/assets/game_data.hpp"
#include "engine/assets/mission.hpp"
#include "engine/audio/mixer.hpp"
#include "engine/audio/audio_system.hpp"
#include "engine/app/options.hpp"
#include "engine/sim/savegame.hpp"
#include "temp_dir.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

int main() {
  using namespace aztec;
  using namespace aztec::engine;
  // Deliberately start each multi-byte value at an odd address.
  std::array<uint8_t, 17> bytes{0, 0x78, 0x56, 0x34, 0x12};
  CHECK(read_le<uint32_t>(bytes.data() + 1) == 0x12345678);
  write_le(bytes.data() + 1, uint64_t(0x0123456789abcdef));
  CHECK(bytes[1] == 0xef && bytes[8] == 0x01);
  CHECK(read_le<uint64_t>(bytes.data() + 1) == 0x0123456789abcdef);
  write_le(bytes.data() + 1, 1.0f);
  CHECK(bytes[1] == 0 && bytes[3] == 0x80 && bytes[4] == 0x3f);
  CHECK(read_le<float>(bytes.data() + 1) == 1.0f);
  write_le(bytes.data() + 1, -2.0);
  CHECK(bytes[7] == 0 && bytes[8] == 0xc0);
  CHECK(read_le<double>(bytes.data() + 1) == -2.0);

  SaveData save;
  save.mission = "x"; save.tick = 0x010203;
  save.cam_x = 1.0f;
  auto encoded = serialize_save(save);
  CHECK(encoded[4] == kSaveFormatVersion && encoded[5] == 0);
  CHECK(encoded[17] == 3 && encoded[19] == 1 && encoded[24] == 0);
  CHECK(encoded[49] == 0 && encoded[51] == 0x80 && encoded[52] == 0x3f);
  SaveData decoded;
  CHECK(deserialize_save(encoded, decoded) && decoded.tick == save.tick && decoded.cam_x == 1.0f);

  std::array<uint8_t, 5> pcm{0, 0x34, 0x12, 0x00, 0x80};
  WavInfo wav;
  wav.channels = 2; wav.bits = 16; wav.rate = kMixRate;
  wav.pcm = pcm.data() + 1; wav.pcm_bytes = 4;
  auto samples = to_stereo_44k(wav);
  CHECK(samples && samples->size() == 2);
  CHECK((*samples)[0] == 0x1234 && (*samples)[1] == -32768);

  TempDir directory;
  auto root = directory.path() / "Game Files";
  std::filesystem::create_directories(root / "rEs");
  std::filesystem::create_directories(root / "mApS");
  // An empty archive is valid; all bytes here are invented.
  for (const char* name : {"PAL.XBE", "units.Xbe", "tiles0.xbe", "Tiles1.XBE", "COMMON.xbe", "fnts.xbe", "LIGHT.XBE"}) {
    std::ofstream file(root / "rEs" / name, std::ios::binary);
    const char empty[4]{}; file.write(empty, 4);
  }
  { std::ofstream file(root / "mApS" / "Mission_1.Mp"); file << "synthetic"; }
  GameData data;
  std::string error;
  CHECK(data.open(root.string(), error));
  CHECK(!data.map_file("mission_1", "mp").empty());
  CHECK(!resolve_mission(data.maps_dir(), "mission_1.mP").empty());
  CHECK(find_file_ci("", "CMakeLists.txt").empty());
  CHECK(!find_file_ci((root / "RES").string(), "pal.xbe").empty());
  CHECK(find_file_ci((root / "absent").string(), "pal.xbe").empty());
  CHECK(find_file_ci(root.string(), "absent").empty());
  // Verify SDL's native per-user default before isolating writes in this process.
  CHECK(SDL_setenv("AZTEC_USER_DIR", "", 1) == 0);
  char* native_preferences = SDL_GetPrefPath("Quetzal", "Engine");
  CHECK(native_preferences);
  CHECK(std::filesystem::path(user_file("settings.ini")) ==
        std::filesystem::path(native_preferences) / "settings.ini");
  SDL_free(native_preferences);
  const auto profile = directory.path() / "User Settings";
  CHECK(SDL_setenv("AZTEC_USER_DIR", profile.string().c_str(), 1) == 0);
  CHECK(SDL_setenv("AZTEC_GAME", "", 1) == 0);
  const auto settings = user_file("settings.ini");
  // An invented Windows path must remain literal even on other test hosts.
  { std::ofstream file(settings, std::ios::binary); file << "game=C:\\Games\\Aztec Empire\r\n"; }
  auto parse = [&](std::vector<std::string> args, Options& options) {
    std::vector<char*> raw;
    for (auto& a : args) raw.push_back(a.data());
    std::string error; bool usage = false;
    return parse_options(int(raw.size()), raw.data(), options, error, usage);
  };
  Options configured;
  CHECK(parse({"engine"}, configured));
  CHECK(configured.game_dir == "C:\\Games\\Aztec Empire");
  CHECK(SDL_setenv("AZTEC_GAME", root.string().c_str(), 1) == 0);
  CHECK(parse({"engine"}, configured) && configured.game_dir == root.string());
  CHECK(parse({"engine", "--game", "D:\\Other Games"}, configured));
  CHECK(configured.game_dir == "D:\\Other Games");
  CHECK(!parse({"engine", "--settings", (profile / "missing.ini").string()}, configured));
  CHECK(!parse({"engine", "--settings"}, configured));
  { std::ofstream file(settings); file << "game=\n"; }
  CHECK(!parse({"engine"}, configured));
  CHECK(save_modifications(default_modifications_file(), {}));
  CHECK(std::filesystem::is_regular_file(profile / "modifications.ini"));

  // Startup must open a dummy SDL device even when optional sound assets are absent.
  CHECK(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
  {
    AudioSystem audio;
    CHECK(!audio.init(root.string()));
    CHECK(audio.device_ok());
  }
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  CHECK(SDL_setenv("SDL_AUDIODRIVER", "quetzal-missing-driver", 1) == 0);
  {
    AudioSystem unavailable;
    CHECK(!unavailable.init(root.string()));
    CHECK(!unavailable.device_ok());
  }
  SDL_Quit();
  return 0;
}
