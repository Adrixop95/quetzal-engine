#include "engine/save_import.hpp"
#include "engine/app/options.hpp"
#include "engine/sim/game_sim.hpp"
#include "game_dir.hpp"
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

using namespace aztec;
using namespace aztec::engine;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
using Bytes = std::vector<uint8_t>;
static void put(Bytes& b, size_t p, uint32_t v) {
  CHECK(p + 4 <= b.size());
  for (int i = 0; i < 4; ++i) b[p + i] = uint8_t(v >> (i * 8));
}
static void word(Bytes& b, uint32_t v) { size_t p = b.size(); b.resize(p + 4); put(b, p, v); }
struct Fixture {
  std::array<Bytes, 10> sections;
  std::array<uint32_t, 10> counts{};
  Fixture() {
    // One town and one tactic owned by different sides, with forward unit links.
    counts[1] = 1;
    word(sections[1], 0x80000007); size_t p = sections[1].size();
    sections[1].resize(p + 5624); put(sections[1], p + 8, 1); put(sections[1], p + 236, 90);
    counts[3] = 1;
    word(sections[3], 17); word(sections[3], 0); p = sections[3].size();
    sections[3].resize(p + 236); put(sections[3], p + 8, 2);
    counts[2] = 2;
    for (int side = 0; side < 2; ++side) {
      Bytes b(1657);
      put(b, 4, uint32_t(side)); put(b, 32, uint32_t(1234 + side));
      b[720 + 5] = 1; // Side +2b0 type restrictions.
      put(b, 1645, 2); put(b, 1649, 2);
      b.insert(b.begin() + 1653, {0, 96, 152, uint8_t(side)});
      sections[2].insert(sections[2].end(), b.begin(), b.end());
    }
    counts[4] = 3;
    constexpr int lengths[] = {39, 45, 45};
    for (int i = 0; i < 3; ++i) {
      auto& b = sections[4]; word(b, uint32_t(i == 0 ? 90 : i == 1 ? 4 : 700));
      word(b, uint32_t(i == 0 ? 10 : 0)); p = b.size(); b.resize(p + lengths[i] * 4);
      put(b, p + 8 * 4, i == 0 ? 0x80000007 : 17);
      put(b, p + 10 * 4, i == 2 ? 0 : 6000); put(b, p + 11 * 4, 7000);
      put(b, p + 17 * 4, 2);
      put(b, p + 5 * 4, 24); put(b, p + 6 * 4, 72);
      if (i > 0) { put(b, p + 25 * 4, 25 * 256 + 64); put(b, p + 26 * 4, 74 * 256 + 128); }
    }
  }
  SaveGame decode() const {
    Bytes b(372);
    const std::string path = "C:\\Aztec\\Maps\\synthetic.mp";
    std::copy(path.begin(), path.end(), b.begin() + 8);
    put(b, 0x158, 13); put(b, 0x15c, 19); put(b, 0x160, 1); put(b, 0x170, 123);
    for (size_t i = 0; i < 10; ++i) { word(b, counts[i]); b.insert(b.end(), sections[i].begin(), sections[i].end()); }
    b.resize(b.size() + 4520); word(b, 7199); word(b, 8);
    SaveGame s; SaveError e; CHECK(parse_savegame(b, s, &e)); return s;
  }
};
static World base_world() {
  World w; w.terrain.w = w.terrain.h = 2; w.mission.file_name = "synthetic";
  w.resources.w = w.resources.h = 2; w.resources.v = {1, 2, 3, 4};
  w.objects.resize(8); w.side_state.resize(8); return w;
}
static void synthetic() {
  Fixture f; auto save = f.decode(); auto w = base_world(); std::string error;
  CHECK(import_original_save(save, w, error)); CHECK(error.empty());
  CHECK(w.objects.size() == 3 && w.original_unit_ids == std::vector<uint32_t>({90, 4, 700}));
  CHECK(w.find(1)->type == 10 && w.find(1)->town == 0 && w.find(1)->side == 0);
  CHECK(w.find(2)->id == 2 && w.find(2)->side == 1 && w.find(2)->town == -1);
  CHECK(w.find(2)->cur_x == 25 * 256 + 64 && w.find(2)->prev_y == 74 * 256 + 128);
  CHECK(w.find(1)->cur_y == 72 * 256 && w.find(1)->hp == 6000 && w.find(1)->secondary_hp == 7000);
  CHECK(!w.find(3)->alive && w.find(3)->hp == 0);
  CHECK(w.side_state[0].money == 1234 && w.side_state[1].money == 1235);
  CHECK(w.mission.sides[1].disabled == std::vector<int>{5});
  CHECK(w.visibility.sides[1].v == Bytes({0, 96, 152, 1}));
  CHECK(w.resources.v == std::vector<uint16_t>({1, 2, 3, 4}));
  CHECK(w.tick == 123 && w.day_clock == 7199 && w.local_side == 1);
  CHECK(w.original_save && w.original_save->bytes == save.bytes);
  // Original saves can include the extra right/bottom fog border.
  auto bordered = f;
  for (int side = 0; side < 2; ++side) {
    auto& b = bordered.sections[2];
    const size_t p = size_t(side) * 1666;
    put(b, p + 1645, 3); put(b, p + 1649, 3);
    b.erase(b.begin() + p + 1653, b.begin() + p + 1657);
    const Bytes grid = {0, 96, 200, 152, uint8_t(side), 201, 202, 203, 204};
    b.insert(b.begin() + p + 1653, grid.begin(), grid.end());
  }
  auto bw = base_world(); CHECK(import_original_save(bordered.decode(), bw, error));
  CHECK(bw.visibility.sides[1].v == Bytes({0, 96, 152, 1}));
  auto rules = std::make_shared<RulesData>(); rules->init_defaults(); GameSim sim(rules);
  // Computer-owned imported towns must initialize the planner without resetting saved fog.
  w.mission.sides[0].only_comp = true;
  sim.init(w);
  CHECK(w.ai_towns.size() == w.mission.towns.size());
  CHECK(w.town_state.size() == w.mission.towns.size());
  CHECK(sim.tick_count() == 123 && w.day_clock == 7199);
  CHECK(w.visibility.sides[0].v == Bytes({0, 96, 152, 0}));
  CHECK(w.find(1)->secondary_hp == 7000 && w.find(1)->hp_max == rules->type(10).hp_max);
  CHECK(w.find(2)->regen_service_ticks == 2 && !w.find(3)->alive && w.find(3)->hp == 0);
  CHECK(w.visibility.sides[1].v == Bytes({0, 96, 152, 1}));
  w.begin_tick(); sim.tick(w); CHECK(w.tick == 124 && sim.tick_count() == 124);
  // Failed imports must preserve the previous world, including its retained snapshot.
  auto reject = [&](SaveGame bad) {
    const auto snapshot = w.original_save;
    CHECK(!import_original_save(bad, w, error)); CHECK(!error.empty());
    CHECK(w.tick == 124 && w.objects.size() == 3 && w.original_save == snapshot);
  };
  auto bad = save; bad.globals[0] = 8; reject(bad);
  bad = save; bad.sections[4].records[1].id = 90; reject(bad);
  bad = save;
  for (auto& field : bad.sections[3].records[0].fields) if (field.memory_offset == 8 && field.scope == "object") field.value = 9;
  reject(bad);
  bad = save;
  for (auto& field : bad.sections[4].records[1].fields) if (field.memory_offset == 0x30) field.value = 88;
  reject(bad);
  bad = save;
  for (auto& field : bad.sections[2].records[0].fields) if (field.name == "fog_width") field.value = 3;
  reject(bad);
  bad = save;
  for (auto& field : bad.sections[2].records[0].fields) if (field.name == "fog") field.offset = bad.bytes.size();
  reject(bad);
  bad = save;
  for (auto& field : bad.sections[4].records[0].fields) if (field.memory_offset == 0x24) field.value = 0x7fffffff;
  reject(bad);
  bad = save; bad.sections.clear(); reject(bad);
  bad = save; bad.header.map_path = "another.mp"; reject(bad);
  bad = save; bad.header.map_path = "../.."; reject(bad);
  CHECK(!load_original_save(GameData{}, "/nonexistent/original.sav", w, error));
  Options o; bool usage; char a[] = "engine", flag[] = "--load-original", path[] = "test.sav";
  char* args[] = {a, flag, path}; CHECK(parse_options(3, args, o, error, usage));
  CHECK(o.original_save == "test.sav" && !o.menu);
  Options missing; CHECK(!parse_options(2, args, missing, error, usage));
  char mission[] = "1", wander[] = "--wander";
  char* conflicting[] = {a, flag, path, mission};
  Options conflict; CHECK(!parse_options(4, conflicting, conflict, error, usage));
  conflicting[3] = wander; conflict = {};
  CHECK(!parse_options(4, conflicting, conflict, error, usage));
  std::puts("Synthetic original-save import passed");
}
static int private_saves(int argc, char** argv) {
  const auto dir = test_game_dir(argc, argv);
  if (!test_game_dir_exists(dir)) return 77;
  auto saves = find_file_ci(dir, "Saves");
  std::error_code ec; std::vector<std::filesystem::path> paths;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(saves, ec)) {
    auto ext = entry.path().extension().string();
    if (entry.is_regular_file() && (ext == ".sav" || ext == ".SAV")) paths.push_back(entry.path());
  }
  if (paths.empty()) { std::puts("SKIP: no private original saves found"); return 77; }
  GameData data; std::string error; CHECK(data.open(dir, error));
  std::sort(paths.begin(), paths.end());
  for (const auto& path : paths) {
    World w;
    if (!load_original_save(data, path.string(), w, error)) {
      std::fprintf(stderr, "%s: %s\n", path.c_str(), error.c_str()); return 1;
    }
    const auto& decoded = *w.original_save;
    const auto& records = decoded.section("units")->records;
    CHECK(w.objects.size() == records.size());
    for (size_t i = 0; i < records.size(); ++i) {
      const auto& r = records[i]; const auto& o = w.objects[i];
      CHECK(w.find(o.id) == &o && o.type == int(r.type));
      CHECK(w.original_unit_ids[i] == r.id);
      CHECK(uint32_t(o.hp) == r.field(0x38)->value && uint32_t(o.secondary_hp) == r.field(0x3c)->value);
      if (r.kind == 0) {
        CHECK(uint32_t(o.cur_x) == r.field(0x6c)->value && uint32_t(o.cur_y) == r.field(0x70)->value);
      } else {
        CHECK(int64_t(o.cur_x) == int64_t(int32_t(r.field(0x24)->value)) * 256);
        CHECK(int64_t(o.cur_y) == int64_t(int32_t(r.field(0x28)->value)) * 256);
      }
      const uint32_t owner = r.field(0x30)->value;
      if (owner) {
        const char* pool = owner & 0x80000000u ? "towns" : "tactics";
        const auto& owners = decoded.section(pool)->records;
        auto group = std::find_if(owners.begin(), owners.end(), [&](const SaveRecord& g) { return g.id == owner; });
        CHECK(group != owners.end() && uint32_t(o.side + 1) == group->field(8)->value);
      }
    }
    const auto& sides = decoded.section("sides")->records;
    for (size_t i = 0; i < sides.size(); ++i) {
      CHECK(uint32_t(w.side_state[i].money) == sides[i].field(0x698)->value);
      const SaveField* fog = nullptr; uint32_t stride = 0;
      for (const auto& field : sides[i].fields) {
        if (field.name == "fog") fog = &field;
        if (field.name == "fog_width") stride = field.value;
      }
      CHECK(fog && stride);
      const auto& layer = w.visibility.sides[i];
      for (int y = 0; y < layer.h; ++y)
        for (int x = 0; x < layer.w; ++x)
          CHECK(layer.v[size_t(y) * layer.w + x] == decoded.bytes[fog->offset + size_t(y) * stride + x]);
    }
    auto rules = std::make_shared<RulesData>(); rules->init_defaults(); GameSim sim(rules);
    const auto tick = w.tick; const int hp = w.objects.empty() ? 0 : w.objects[0].hp;
    sim.init(w); CHECK(sim.tick_count() == tick);
    CHECK(w.objects.empty() || w.objects[0].hp == hp);
    std::printf("Imported %s: %zu objects, %zu towns, tick %llu\n", path.c_str(), w.objects.size(), w.mission.towns.size(), (unsigned long long)w.tick);
  }
  return 0;
}
int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--private") return private_saves(argc - 1, argv + 1);
  synthetic();
}
