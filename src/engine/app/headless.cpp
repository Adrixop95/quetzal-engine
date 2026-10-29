#include "engine/app/headless.hpp"
#include "engine/app/options.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

#include "core/script.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/sim/savegame.hpp"

namespace aztec::engine {
namespace {
using namespace aztec::script;

int number(const std::string& s, int minimum = 0) {
  int n = 0;
  auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
  if (ec != std::errc{} || end != s.data() + s.size() || n < minimum)
    throw std::runtime_error("invalid integer: " + s);
  return n;
}

std::string quoted(const std::string& s) {
  std::ostringstream out;
  out << '"';
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') out << '\\' << char(c);
    else if (c < 32 || c >= 127) out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << unsigned(c);
    else out << char(c);
  }
  out << '"';
  return out.str();
}

struct Scheduled { int tick; Command command; bool all = false; };
std::vector<Scheduled> read_orders(const std::string& path, const World& w) {
  std::vector<Scheduled> out;
  if (path.empty()) return out;
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open orders: " + path);
  std::string line;
  int line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    line = line.substr(0, line.find('#'));
    std::istringstream row(line);
    std::vector<std::string> fields;
    for (std::string s; row >> s;) fields.push_back(s);
    if (fields.empty()) continue;
    try {
      if (fields.size() < 5) throw std::runtime_error("expected tick move|attack-move side tile-x tile-y [object IDs]");
      Scheduled order{};
      order.tick = number(fields[0], 1);
      if (fields[1] == "move") order.command.type = Pct::Move;
      else if (fields[1] == "attack-move") order.command.type = Pct::AttackMove;
      else throw std::runtime_error("unknown order " + fields[1]);
      int side = number(fields[2]);
      int x = number(fields[3]), y = number(fields[4]);
      if (side >= 8 || x >= w.terrain.w || y >= w.terrain.h) throw std::runtime_error("side or tile outside mission");
      order.command.side = uint8_t(side);
      order.command.a = x * kTilePx + kTilePx / 2;
      order.command.b = y * kTilePx + kTilePx / 2;
      order.all = fields.size() == 5;
      for (size_t i = 5; i < fields.size(); ++i) order.command.ids.push_back(uint32_t(number(fields[i], 1)));
      out.push_back(std::move(order));
    } catch (const std::exception& e) {
      throw std::runtime_error("orders line " + std::to_string(line_no) + ": " + e.what());
    }
  }
  std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.tick < b.tick; });
  return out;
}

// A deterministic exercising policy for the human side, alongside the engine's computer AI.
void drive(GameSim& sim, const World& w) {
  const int side = w.script_side;
  if (side < 0 || side >= int(w.side_state.size())) return;
  if (w.side_state[size_t(side)].queue.empty()) {
    for (const auto& item : sim.menu(w, side)) if (item.enabled && !item.construct) {
      Command c;
      c.type = Pct::Build; c.side = uint8_t(side); c.a = item.type;
      sim.submit(c);
      break;
    }
  }
  for (const Object& o : w.objects) {
    if (!o.alive || o.removed || o.side != side || o.type < 0 || o.order != uint8_t(Order::Idle)) continue;
    const auto& rules = sim.rules().type(o.type);
    if (!rules.mobile || !rules.armed) continue;
    const Object* target = nullptr;
    int64_t best = std::numeric_limits<int64_t>::max();
    for (const Object& enemy : w.objects) {
      if (!enemy.alive || enemy.removed || enemy.type < 0 || !w.hostile(side, enemy.side)) continue;
      int64_t dx = (enemy.cur_x >> 8) - (o.cur_x >> 8), dy = (enemy.cur_y >> 8) - (o.cur_y >> 8);
      int64_t dist = dx * dx + dy * dy;
      if (dist < best) { best = dist; target = &enemy; }
    }
    if (target) {
      Command c;
      c.type = Pct::AttackMove; c.side = uint8_t(side); c.ids = {o.id};
      c.a = target->cur_x >> 8; c.b = target->cur_y >> 8;
      sim.submit(std::move(c));
    }
  }
}

struct Report {
  std::string mission, status = "error", error;
  uint64_t ticks = 0, hash = 0, trace = 1469598103934665603ull;
  size_t unsupported_executions = 0, commands = 0;
  std::vector<std::string> script_errors, unsupported_actions, unreachable, unmet;
  std::vector<uint32_t> stalled;
  void write(std::ostream& out) const {
    out << "{\"schema\":1,\"mission\":" << quoted(mission) << ",\"status\":" << quoted(status)
        << ",\"error\":" << quoted(error) << ",\"ticks\":" << ticks
        << ",\"state_hash\":\"" << std::hex << hash << "\",\"trace_hash\":\"" << trace << std::dec
        << "\",\"commands\":" << commands << ",\"unsupported_executions\":" << unsupported_executions;
    auto strings = [&](const char* key, const auto& values) {
      out << ",\"" << key << "\":[";
      bool first = true;
      for (const auto& value : values) { if (!first) out << ','; first = false; out << quoted(value); }
      out << ']';
    };
    strings("script_errors", script_errors);
    strings("unsupported_actions", unsupported_actions);
    strings("unreachable_objectives", unreachable);
    strings("unmet_objectives", unmet);
    out << ",\"stalled_order_ids\":[";
    for (size_t i = 0; i < stalled.size(); ++i) { if (i) out << ','; out << stalled[i]; }
    out << "]}\n";
  }
};

void audit(const Script& script, const World& w, Report& report) {
  bool victory = false;
  for (const auto& t : script.triggers) {
    bool objective = false;
    for (const auto& a : t.actions) {
      if (a.kind == ActKind::SideVictory && (a.side == kLocal || a.side == w.script_side)) objective = victory = true;
      if (!ScriptRunner::supports(a.kind)) {
        auto diagnostic = std::string("unsupported action ") + action_name(a.kind) +
                          " in trigger " + std::to_string(t.id) + " at line " + std::to_string(a.line);
        report.unsupported_actions.push_back(diagnostic);
        report.script_errors.push_back(diagnostic);
      }
      if ((a.kind == ActKind::TriggerEnable || a.kind == ActKind::TriggerDisable) && !script.find(a.id))
        report.script_errors.push_back("missing trigger " + std::to_string(a.id) + " at line " + std::to_string(a.line));
    }
    if (!objective) continue;
    // Only classify a necessary condition that is impossible for any in-map position.
    for (const auto& c : t.conditions) {
      bool rect = c.kind == CondKind::SideUnitsInRect || c.kind == CondKind::SideUnitsOfUGIInRect;
      bool positive = (c.cmp == Cmp::Gt && c.value >= 0) ||
                      ((c.cmp == Cmp::Eq || c.cmp == Cmp::Ge) && c.value > 0) ||
                      (c.cmp == Cmp::Ne && c.value == 0);
      if (rect && positive && (c.rect[0] > c.rect[2] || c.rect[1] > c.rect[3] ||
          c.rect[2] < 0 || c.rect[3] < 0 || c.rect[0] >= w.terrain.w || c.rect[1] >= w.terrain.h))
        report.unreachable.push_back("victory trigger " + std::to_string(t.id) + ": impossible rectangle at line " + std::to_string(c.line));
    }
  }
  if (!victory) report.unreachable.push_back("no local victory action in mission script");
}
}  // namespace

int run_headless(int argc, char** argv) {
  Report report;
  std::string game = "game", rules_dir = default_rules_dir(), orders_path, output, observations_path, hashes_path, save_path, resume_path;
  if (const char* value = std::getenv("AZTEC_GAME")) game = value;
  int max_ticks = 43200, interval = 120, save_tick = 0;
  bool auto_orders = true;
  try {
    bool mission_given = false;
    for (int i = 1; i < argc; ++i) {
      std::string a = argv[i];
      auto value = [&]() -> std::string {
        if (++i >= argc) throw std::runtime_error("missing value for " + a);
        return argv[i];
      };
      if (a == "--game") game = value();
      else if (a == "--rules-dir") rules_dir = value();
      else if (a == "--max-ticks") max_ticks = number(value(), 1);
      else if (a == "--order-interval") interval = number(value(), 1);
      else if (a == "--orders") orders_path = value();
      else if (a == "--report") output = value();
      else if (a == "--observations") observations_path = value();
      else if (a == "--tick-hashes") hashes_path = value();
      else if (a == "--save-at") save_tick = number(value(), 1);
      else if (a == "--save") save_path = value();
      else if (a == "--resume") resume_path = value();
      else if (a == "--no-auto-orders") auto_orders = false;
      else if (a == "--help") {
        std::puts("usage: aztec_engine --headless --game DIR --rules-dir DIR [--max-ticks N] [--order-interval N] [--orders FILE] [--no-auto-orders] [--report FILE] [--observations FILE] [--tick-hashes FILE] [--save-at N --save FILE] [--resume FILE] MISSION");
        return 0;
      } else if (a.empty() || a[0] == '-' || mission_given) throw std::runtime_error("unexpected argument: " + a);
      else { report.mission = a; mission_given = true; }
    }
    if (!mission_given) throw std::runtime_error("headless mode requires a mission");
    GameData data;
    std::string error;
    if (!data.open(game, error)) throw std::runtime_error(error);
    World world;
    if (!load_world(data, report.mission, world, error)) throw std::runtime_error(error);
    auto rules = std::make_shared<RulesData>();
    if (!rules->load_dir(rules_dir)) throw std::runtime_error("cannot load rules from " + rules_dir);
    auto session = std::make_unique<GameSim>(rules);
    Script script;
    const std::string scn = data.map_file(world.mission.file_name, "scn");
    if (scn.empty()) report.script_errors.push_back("missing mission script");
    else if (!load_script(scn, script)) {
      report.script_errors.push_back("script parse error: " + script.error);
      throw std::runtime_error(report.script_errors.back());
    }
    else if (!session->attach_script(scn)) throw std::runtime_error("cannot attach mission script");
    audit(script, world, report);
    if ((save_tick != 0) != !save_path.empty() || save_tick >= max_ticks)
      throw std::runtime_error("save requires a path and a tick before the run limit");
    // A headless save fingerprints every simulation input, including optional sight data.
    std::vector<std::pair<std::string, std::string>> inputs;
    inputs.emplace_back("mission", resolve_mission(data.maps_dir(), report.mission));
    for (const char* ext : {"map", "dat", "rm", "vis", "scn"}) {
      auto path = data.map_file(world.mission.file_name, ext);
      if (path.empty() && std::string(ext) == "map") path = data.map_file(world.mission.map_name, ext);
      inputs.emplace_back(ext, path);
    }
    std::vector<std::pair<std::string, std::string>> tables;
    for (const auto& entry : std::filesystem::directory_iterator(rules_dir))
      if (entry.path().extension() == ".csv")
        tables.emplace_back(entry.path().filename().string(), entry.path().string());
    std::sort(tables.begin(), tables.end());
    inputs.insert(inputs.end(), tables.begin(), tables.end());
    const char* sight = std::getenv("AZTEC_GAME");
    for (const char* name : {"Res/Light.xbe", "Res/Units.xbe", "Aztec.exe"})
      inputs.emplace_back(name, sight ? find_file_ci(sight, name) : "");
    uint64_t content = 0;
    if (!save_content_hash(inputs, content)) throw std::runtime_error("cannot fingerprint simulation inputs");
    if (resume_path.empty()) session->init(world);
    else {
      SaveData saved;
      if (!read_save(resume_path, saved)) throw std::runtime_error("cannot read resume save");
      if (save_tick && uint64_t(save_tick) <= saved.tick) throw std::runtime_error("new save tick must follow resume tick");
      if (saved.tick >= uint64_t(max_ticks)) throw std::runtime_error("resume tick must precede run limit");
      session = replay_save(saved, world, rules, content, scn, error);
      if (!session) throw std::runtime_error(error);
    }
    GameSim& sim = *session;
    std::ofstream hashes;
    if (!hashes_path.empty()) {
      hashes.open(hashes_path);
      if (!hashes) throw std::runtime_error("cannot open tick hashes");
    }
    auto hash_row = [&]() {
      if (hashes.is_open()) {
        hashes << sim.tick_count() << ' ' << std::hex << report.hash << std::dec << '\n';
        if (!hashes) throw std::runtime_error("cannot write tick hashes");
      }
    };
    report.hash = sim.state_hash(world);
    hash_row();
    auto orders = read_orders(orders_path, world);
    if (!orders.empty() && orders.back().tick > max_ticks) throw std::runtime_error("order tick exceeds run limit");
    std::ofstream observations;
    if (!observations_path.empty()) {
      observations.open(observations_path);
      if (!observations) throw std::runtime_error("cannot open observations: " + observations_path);
      int mission_id = 0;
      auto [end, ec] = std::from_chars(report.mission.data(), report.mission.data() + report.mission.size(), mission_id);
      if (ec != std::errc{} || end != report.mission.data() + report.mission.size() || mission_id < 1 || mission_id > 15)
        mission_id = 0;  // Custom map names have no campaign number.
      // Engine-local identity contract; a reference adapter must audit its mapping separately.
      observations << "{\"schema\":1,\"mission\":" << mission_id << ",\"phase\":\"before_commands\","
                      "\"contract\":\"engine-index-v1\",\"fields\":[\"hp\",\"side\",\"type\",\"x\",\"y\"],"
                      "\"first_tick\":1,\"last_tick\":" << max_ticks << "}\n";
    }
    size_t next = 0;
    while (next < orders.size() && uint64_t(orders[next].tick) <= sim.tick_count()) ++next;
    struct Position { Fixed x = 0, y = 0; uint64_t since = 0; };
    std::map<uint32_t, Position> positions;
    for (int64_t tick = int64_t(sim.tick_count()) + 1; tick <= max_ticks && world.outcome == kRunning; ++tick) {
      if (observations.is_open()) {
        observations << "{\"tick\":" << tick << ",\"objects\":{";
        bool first = true;
        for (const auto& o : world.objects) {
          if (!o.alive || o.removed) continue;
          if (!first) observations << ',';
          first = false;
          observations << quoted("engine:" + std::to_string(o.id)) << ":{\"hp\":" << o.hp
                       << ",\"side\":" << o.side << ",\"type\":" << o.type
                       << ",\"x\":" << o.cur_x << ",\"y\":" << o.cur_y << '}';
        }
        observations << "}}\n";
        if (!observations) throw std::runtime_error("cannot write observations: " + observations_path);
      }
      if (auto_orders && (tick - 1) % interval == 0) drive(sim, world);
      while (next < orders.size() && orders[next].tick == tick) {
        Command command = orders[next].command;
        if (orders[next].all) for (const auto& o : world.objects)
          if (o.alive && !o.removed && o.side == command.side && o.type >= 0 && rules->type(o.type).mobile) command.ids.push_back(o.id);
        if (command.ids.empty()) throw std::runtime_error("order has no live mobile objects at tick " + std::to_string(tick));
        for (auto id : command.ids) {
          const auto* o = world.find(id);
          if (!o || !o->alive || o->removed || o->side != command.side || o->type < 0 || !rules->type(o->type).mobile)
            throw std::runtime_error("invalid order object at tick " + std::to_string(tick));
        }
        sim.submit(std::move(command));
        ++next;
      }
      world.begin_tick();
      sim.tick(world);
      report.hash = sim.state_hash(world);
      hash_row();
      if (tick == save_tick) {
        SaveData saved;
        saved.mission = world.mission.file_name;
        saved.tick = sim.tick_count(); saved.hash = report.hash;
        saved.script_hash = file_hash(scn); saved.content_hash = content; saved.log = sim.log();
        if (!write_save(save_path, saved)) throw std::runtime_error("cannot write mid-mission save");
      }
      report.trace = (report.trace ^ report.hash) * 1099511628211ull;
      for (const auto& o : world.objects) {
        auto& p = positions[o.id];
        if (!o.alive || o.removed || (o.order != uint8_t(Order::Move) && o.order != uint8_t(Order::AttackMove)) ||
            o.cur_x != p.x || o.cur_y != p.y) p = {o.cur_x, o.cur_y, sim.tick_count()};
      }
      if (world.outcome != kRunning) break;
    }
    if (observations.is_open()) {
      observations.close();
      if (!observations) throw std::runtime_error("cannot finish observations: " + observations_path);
    }
    if (hashes.is_open()) {
      hashes.flush();
      if (!hashes) throw std::runtime_error("cannot flush tick hashes");
    }
    report.ticks = sim.tick_count();
    report.commands = sim.log().size();
    report.unsupported_executions = sim.script().unsupported();
    report.status = world.outcome == kVictory ? "victory" : world.outcome == kDefeat ? "defeat" : "timeout";
    for (const auto& o : world.objects) {
      if (o.alive && !o.removed && (o.order == uint8_t(Order::Move) || o.order == uint8_t(Order::AttackMove)) &&
          report.ticks - positions[o.id].since >= 600) report.stalled.push_back(o.id);
    }
    if (world.outcome != kVictory) for (const auto& t : script.triggers) {
      bool objective = false;
      for (const auto& a : t.actions)
        if (a.kind == ActKind::SideVictory && (a.side == kLocal || a.side == world.script_side)) objective = true;
      if (objective) for (const auto& c : t.conditions) if (!sim.script().eval(world, c))
        report.unmet.push_back("victory trigger " + std::to_string(t.id) + ": false condition at line " + std::to_string(c.line));
    }
  } catch (const std::exception& e) { report.status = "error"; report.error = e.what(); }
  if (output.empty()) report.write(std::cout);
  else {
    std::ofstream out(output);
    report.write(out);
    if (!out) { std::fprintf(stderr, "cannot write headless report: %s\n", output.c_str()); return 2; }
  }
  return report.status == "error" ? 1 : 0;
}
}  // namespace aztec::engine
