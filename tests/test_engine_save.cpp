// Synthetic engine-save robustness and fresh-process continuation checks.
#include "core/endian.hpp"
#include "engine/sim/savegame.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/world/world.hpp"
#include "temp_dir.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <random>

using namespace aztec;
using namespace aztec::engine;
using Bytes = std::vector<uint8_t>;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

static void repair_checksum(Bytes& v) {
  uint64_t hash = 1469598103934665603ull;
  for (size_t i = 0; i < v.size() - 8; ++i) hash = (hash ^ v[i]) * 1099511628211ull;
  write_le(v.data() + v.size() - 8, hash);
}
static SaveData fixture() {
  SaveData s;
  s.mission = "synthetic-save"; s.tick = 100; s.hash = 123; s.content_hash = 456;
  s.cam_x = -12.5f; s.cam_y = 200; s.cam_zoom = 1.5f;
  Command c; c.type = Pct::Move; c.a = 200; c.b = -300; c.ids = {1, 2, UINT32_MAX};
  s.log = {{1, c}, {100, c}};
  return s;
}
static void check_save(const Bytes& v) {
  SaveData out = fixture(); out.mission = "unchanged";
  auto before = serialize_save(out);
  if (deserialize_save(v, out)) CHECK(serialize_save(out) == v);
  else CHECK(serialize_save(out) == before);
}
static void check_log(const Bytes& v) {
  auto out = fixture().log;
  auto before = serialize_log(out);
  if (deserialize_log(v, out)) CHECK(serialize_log(out) == v);
  else CHECK(serialize_log(out) == before);
}
static void malformed() {
  auto s = fixture(); auto bytes = serialize_save(s);
  CHECK(!bytes.empty());
  SaveData decoded; CHECK(deserialize_save(bytes, decoded));
  CHECK(decoded.cam_x == s.cam_x && decoded.content_hash == s.content_hash);
  for (size_t i = 0; i < bytes.size(); ++i) {
    check_save(Bytes(bytes.begin(), bytes.begin() + i));
    for (unsigned bit = 0; bit < 8; ++bit) {
      auto v = bytes; v[i] ^= uint8_t(1u << bit);
      CHECK(!deserialize_save(v, decoded)); // checksum must catch every single-bit change
      check_save(v);
      repair_checksum(v); check_save(v); // exercise parser beyond its checksum gate
    }
  }
  for (size_t offset : {size_t(4), size_t(8), size_t(12), size_t(16 + s.mission.size()),
                         size_t(60 + s.mission.size())}) {
    auto v = bytes; write_le(v.data() + offset, UINT32_MAX); repair_checksum(v);
    CHECK(!deserialize_save(v, decoded)); check_save(v);
  }
  auto extra = bytes; extra.push_back(0); repair_checksum(extra);
  CHECK(!deserialize_save(extra, decoded));
  auto old = bytes; old[3] = '2'; repair_checksum(old); CHECK(!deserialize_save(old, decoded));
  for (auto name : {"", "../mission_1", "/mission_1", "mission.mp", "a b"}) {
    auto bad = s; bad.mission = name; CHECK(serialize_save(bad).empty());
  }
  auto bad = s; bad.cam_zoom = 0; CHECK(serialize_save(bad).empty());
  bad = s; bad.cam_x = std::numeric_limits<float>::infinity(); CHECK(serialize_save(bad).empty());
  bad = s; bad.cam_zoom = std::numeric_limits<float>::quiet_NaN(); CHECK(serialize_save(bad).empty());
  bad = s; bad.tick = kMaxSaveTicks + 1; CHECK(serialize_save(bad).empty());
  bad = s; bad.tick = 99; CHECK(serialize_save(bad).empty());
  bad = s; bad.log[0].tick = 101; CHECK(serialize_save(bad).empty());
  bad = s; bad.log[0].tick = 0; CHECK(serialize_save(bad).empty());
  bad = s; bad.log[0].cmd.side = 8; CHECK(serialize_save(bad).empty());
  bad = s; bad.log[0].cmd.type = Pct(255); CHECK(serialize_save(bad).empty());
  bad = s; bad.log[0].cmd.ids.resize(kMaxCommandIds + 1); CHECK(serialize_save(bad).empty());
  bad = s; bad.log.resize(kMaxLogCommands + 1); CHECK(serialize_save(bad).empty());

  auto log = serialize_log(s.log);
  for (size_t i = 0; i < log.size(); ++i) {
    check_log(Bytes(log.begin(), log.begin() + i));
    for (unsigned bit = 0; bit < 8; ++bit) { auto v = log; v[i] ^= uint8_t(1u << bit); check_log(v); }
  }
  for (size_t offset : {size_t(4), size_t(16), size_t(32)}) {
    auto v = log; write_le(v.data() + offset, UINT32_MAX);
    std::vector<LoggedCommand> out; CHECK(!deserialize_log(v, out)); check_log(v);
  }
  log.push_back(0); std::vector<LoggedCommand> out; CHECK(!deserialize_log(log, out));
  // Fixed-seed fuzzing is repeatable and never launches a replay from random bytes.
  std::mt19937 rng(0x415a5333);
  for (unsigned i = 0; i < 10000; ++i) {
    Bytes v(rng() % 1024);
    for (auto& b : v) b = uint8_t(rng());
    check_save(v); check_log(v);
    v = bytes;
    for (unsigned k = 0; k < 4; ++k) v[rng() % (v.size() - 8)] = uint8_t(rng());
    repair_checksum(v); check_save(v);
  }
}

static void files() {
  TempDir dir;
  auto path = (dir.path() / "slot.azs").string();
  auto s = fixture(); CHECK(write_save(path, s));
  SaveData out; CHECK(read_save(path, out)); CHECK(serialize_save(out) == serialize_save(s));
  s.hash++; CHECK(write_save(path, s)); // replace an existing slot
  CHECK(read_save(path, out) && out.hash == s.hash);
  auto invalid = s; invalid.mission.clear(); CHECK(!write_save(path, invalid));
  CHECK(read_save(path, out) && out.hash == s.hash);
  CHECK(!write_save((dir.path() / "absent" / "slot").string(), s));
  auto directory = dir.path() / "directory"; std::filesystem::create_directory(directory);
  { std::ofstream f(directory / "keep"); f << "keep"; }
  CHECK(!write_save(directory.string(), s)); // rename failure must clean up its temporary file
  CHECK(std::filesystem::exists(directory / "keep"));
  for (const auto& entry : std::filesystem::directory_iterator(dir.path()))
    CHECK(entry.path().filename().string().find(".tmp.") == std::string::npos);
  auto sentinel = serialize_save(out);
  CHECK(!read_save((dir.path() / "absent").string(), out)); CHECK(serialize_save(out) == sentinel);
  { std::ofstream f(path, std::ios::binary); f.seekp(kMaxSaveBytes); f.put('x'); }
  CHECK(!read_save(path, out)); CHECK(serialize_save(out) == sentinel);
  { std::ofstream f(path, std::ios::binary); f << "AZS3"; }
  CHECK(!read_save(path, out)); CHECK(serialize_save(out) == sentinel);

  auto a = (dir.path() / "a").string(), b = (dir.path() / "b").string();
  { std::ofstream f(a); f << "same bytes"; }
  std::filesystem::copy_file(a, b);
  uint64_t h1 = 0, h2 = 0;
  CHECK(save_content_hash({{"rules", a}, {"optional", ""}}, h1));
  CHECK(save_content_hash({{"rules", b}, {"optional", ""}}, h2) && h1 == h2);
  { std::ofstream f(b); f << "changed bytes"; }
  CHECK(save_content_hash({{"rules", b}, {"optional", ""}}, h2) && h1 != h2);
  CHECK(save_content_hash({{"rules", a}, {"optional", a}}, h2) && h1 != h2);
  h2 = 42; CHECK(!save_content_hash({{"rules", a + "missing"}}, h2) && h2 == 42);
}

static World initial_world() {
  World w; w.mission.file_name = "synthetic-save";
  w.terrain.w = w.terrain.h = 12; w.terrain.cells.resize(144);
  w.side_state.resize(8);
  Object o; o.id = 1; o.type = 0; o.hp = 6000; o.side = 0;
  o.teleport(to_fixed(100), to_fixed(100)); w.objects.push_back(o);
  return w;
}
static void advance(World& w, GameSim& sim, unsigned ticks, bool orders = false) {
  for (unsigned i = 0; i < ticks; ++i) {
    if (orders && i % 120 == 0) {
      Command c; c.type = Pct::Move; c.ids = {1}; c.a = 100 + (i % 240 ? 0 : 300); c.b = 200;
      sim.submit(c);
    }
    w.begin_tick(); sim.tick(w);
  }
}
static SaveData long_save(const std::shared_ptr<RulesData>& rules) {
  World w = initial_world(); GameSim sim(rules); sim.init(w);
  advance(w, sim, 14400, true);
  auto s = fixture(); s.tick = sim.tick_count(); s.hash = sim.state_hash(w); s.log = sim.log();
  return s;
}
static void continuation(const SaveData& s, const std::shared_ptr<RulesData>& rules) {
  World uninterrupted = initial_world(); GameSim original(rules); original.init(uninterrupted);
  advance(uninterrupted, original, 14400, true);
  CHECK(original.state_hash(uninterrupted) == s.hash);
  CHECK(s.cam_x == -12.5f && s.cam_y == 200 && s.cam_zoom == 1.5f);
  auto fresh = initial_world(); std::string error;
  auto sim = replay_save(s, fresh, rules, s.content_hash, "", error);
  CHECK(sim && error.empty());
  CHECK(fresh.mission.file_name == s.mission);
  for (unsigned i = 0; i < 300; ++i) {
    advance(fresh, *sim, 1, true); advance(uninterrupted, original, 1, true);
    CHECK(sim->state_hash(fresh) == original.state_hash(uninterrupted));
  }
  // All rejection paths must leave even a populated output world unchanged.
  auto before = sim->state_hash(fresh);
  auto bad = s; bad.mission = "another-mission";
  CHECK(!replay_save(bad, fresh, rules, s.content_hash, "", error));
  CHECK(error == "Save mission is incompatible" && sim->state_hash(fresh) == before);
  bad = s; bad.hash ^= 1;
  CHECK(!replay_save(bad, fresh, rules, s.content_hash, "", error));
  CHECK(error == "Save replay hash mismatch" && sim->state_hash(fresh) == before);
  CHECK(!replay_save(s, fresh, rules, s.content_hash + 1, "", error));
  CHECK(error == "Save content is incompatible" && sim->state_hash(fresh) == before);
  bad = s; bad.script_hash = 123;
  CHECK(!replay_save(bad, fresh, rules, s.content_hash, "", error));
  CHECK(error == "Save script is incompatible" && sim->state_hash(fresh) == before);
  TempDir dir; auto script = (dir.path() / "victory.scn").string();
  { std::ofstream f(script); f << "trigger(0) conditions localTime == 10 actions side(local).victory end\n"; }
  auto w = initial_world(); GameSim game(rules); CHECK(game.attach_script(script)); game.init(w);
  advance(w, game, 5);
  auto scripted = fixture(); scripted.tick = 5; scripted.log = game.log();
  scripted.script_hash = file_hash(script); scripted.hash = game.state_hash(w);
  auto restored = initial_world(); auto replay = replay_save(scripted, restored, rules, scripted.content_hash, script, error);
  CHECK(replay);
  advance(w, game, 10); advance(restored, *replay, 10);
  CHECK(w.outcome == kVictory && restored.outcome == kVictory);
  CHECK(game.state_hash(w) == replay->state_hash(restored));
  { std::ofstream f(script); f << "invalid script"; }
  scripted.script_hash = file_hash(script);
  auto unchanged = replay->state_hash(restored);
  CHECK(!replay_save(scripted, restored, rules, scripted.content_hash, script, error));
  CHECK(error == "Cannot parse save script" && replay->state_hash(restored) == unchanged);
}
int main(int argc, char** argv) {
  auto rules = std::make_shared<RulesData>(); rules->init_defaults();
  if (argc == 3 && std::string(argv[1]) == "--write") {
    CHECK(write_save(argv[2], long_save(rules))); return 0;
  }
  if (argc == 3 && std::string(argv[1]) == "--resume") {
    SaveData s; CHECK(read_save(argv[2], s)); continuation(s, rules); return 0;
  }
  if (argc == 3 && std::string(argv[1]) == "--expect-write-failure") {
    auto s = fixture(); s.log.resize(200, s.log.back());
    CHECK(!write_save(argv[2], s)); return 0;
  }
  CHECK(argc == 1);
  malformed(); files(); continuation(long_save(rules), rules);
  std::puts("Engine save robustness and continuation passed");
}
