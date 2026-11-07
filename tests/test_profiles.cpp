// Synthetic profile preferences and command-line precedence across fresh processes.
#include "engine/app/options.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

using namespace aztec::engine;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

static Options parse(std::vector<std::string> args, bool expected = true) {
  std::vector<char*> raw; for (auto& arg : args) raw.push_back(arg.data());
  Options options; std::string error; bool usage;
  CHECK(parse_options(int(raw.size()), raw.data(), options, error, usage) == expected);
  return options;
}
int main(int argc, char** argv) {
  CHECK(argc == 3);
  const std::string mode = argv[1], path = argv[2];
  const auto faithful = Modifications::faithful(), enhanced = Modifications::enhanced();
  CHECK(faithful.bits() == 0 && faithful.overrides == 0);
  CHECK(enhanced.parallel_construction && enhanced.parallel_training == 3 && enhanced.relaxed_placement && enhanced.unit_unstick);
  CHECK(Modifications::from_bits(enhanced.bits()).bits() == enhanced.bits());
  CHECK(Modifications::valid(enhanced.bits()) && !Modifications::valid(36));
  if (mode == "write") {
    auto options = parse({"engine", "--modifications-file", path});
    CHECK(options.modifications.bits() == enhanced.bits()); // first interactive launch
    auto& m = options.modifications;
    CHECK(m.set("parallel_construction=off") && m.set("parallel_training=5") && m.set("unit_unstick=off"));
    CHECK(m.set("relaxed_placement=on")); // explicit override equal to the preset must survive
    CHECK(save_modifications(path, m));
    return 0;
  }
  CHECK(mode == "read");
  auto options = parse({"engine", "--modifications-file", path});
  auto m = options.modifications;
  CHECK(m.profile == PlayProfile::Enhanced && m.overrides == 15);
  CHECK(!m.parallel_construction && m.parallel_training == 5 && m.relaxed_placement && !m.unit_unstick);
  CHECK(save_modifications(path, m));
  Modifications restored; CHECK(load_modifications(path, restored));
  CHECK(restored.bits() == m.bits() && restored.overrides == m.overrides && restored.profile == m.profile);
  for (auto args : {std::vector<std::string>{"--profile", "faithful", "--mod", "parallel_training=on"},
                    std::vector<std::string>{"--mod", "parallel_training=on", "--profile", "faithful"}}) {
    args.insert(args.begin(), {"engine", "--modifications-file", path});
    const auto result = parse(args).modifications;
    CHECK(result.profile == PlayProfile::Faithful && result.parallel_training == 3);
    CHECK(!result.parallel_construction && !result.relaxed_placement && !result.unit_unstick);
  }
  CHECK(parse({"engine", "--modifications-file", path, "--profile", "faithful"}).modifications.bits() == 0);
  for (const auto* invalid : {"parallel_training=0", "parallel_training=9", "parallel_training=256", "parallel_training=-1", "parallel_training=3x", "unknown=on"})
    CHECK(!m.set(invalid));
  CHECK(faithful.parallel_training == 1);
  for (const auto* setting : {"parallel_training=per building", "parallel_training=per-building"}) {
    auto per_building = Modifications::enhanced();
    CHECK(per_building.set(setting) && per_building.parallel_training == 0);
    CHECK(Modifications::from_bits(per_building.bits()).parallel_training == 0);
    CHECK(save_modifications(path, per_building) && load_modifications(path, restored));
    CHECK(restored.bits() == per_building.bits() && restored.overrides == per_building.overrides);
  }
  CHECK(m.set("parallel_training=off") && m.parallel_training == 1);
  CHECK(m.set("parallel_training=on") && m.parallel_training == 3);
  parse({"engine", "--profile", "invalid"}, false);
  parse({"engine", "--profile"}, false);
  parse({"engine", "--mod", "unit_unstick=yes"}, false);
  // Profile entries are applied before overrides, regardless of file order.
  { std::ofstream file(path); file << "parallel_training=7\nprofile=enhanced\nunit_unstick=off\n"; }
  CHECK(load_modifications(path, restored) && restored.parallel_training == 7 && !restored.unit_unstick);
  { std::ofstream file(path); file << "profile=invalid\n"; }
  CHECK(!load_modifications(path, restored) && restored.parallel_training == 7);
  { std::ofstream file(path); file << "parallel_construction=on\n"; }
  CHECK(load_modifications(path, restored) && restored.profile == PlayProfile::Faithful && restored.bits() == 1);
  return 0;
}
