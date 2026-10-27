// Synthetic byte-order, alignment and case-sensitive filesystem regression checks.
#include "core/endian.hpp"
#include "engine/assets/game_data.hpp"
#include "engine/assets/mission.hpp"
#include "engine/audio/mixer.hpp"
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
  save.mission = "x"; save.tick = 0x0102030405060708;
  save.cam_x = 1.0f;
  auto encoded = serialize_save(save);
  CHECK(encoded[4] == 1 && encoded[5] == 0);
  CHECK(encoded[9] == 8 && encoded[16] == 1);
  CHECK(encoded[33] == 0 && encoded[35] == 0x80 && encoded[36] == 0x3f);
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
  return 0;
}
