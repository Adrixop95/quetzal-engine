#include "engine/app/playtest.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "engine/app/ui_trace.hpp"

#include "engine/sim/script_runner.hpp"
#include "engine/world/type_info.hpp"
#include "core/loader_limits.hpp"
#include "core/mapfiles.hpp"

namespace aztec::engine {

static std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}

bool PlaytestScript::load(const std::string& path, std::string& err) {
  lines_.clear(); pc_ = 0; until_tick_ = 0; wait_ = 0; failures_ = 0; deadline_ = 0; completed_ = false;
  observations_.close(); observations_.clear(); captures_.clear(); observed_tick_ = UINT64_MAX;
  observed_commands_ = 0; tick_limit_ = UINT64_MAX;
  release_left_ = release_right_ = release_middle_ = false; release_keys_.clear(); drag_phase_ = 0;
  std::vector<uint8_t> bytes;
  if (!read_file(path, bytes) || bytes.size() > detail::kTextBytes) { err = "cannot read bounded playtest script " + path; return false; }
  std::istringstream f(std::string(bytes.begin(), bytes.end()));
  std::string line;
  int n = 0;
  while (std::getline(f, line)) {
    n++;
    size_t h = line.find('#');
    if (h != std::string::npos) line.resize(h);
    std::stringstream ss(line);
    Line l;
    std::string tok;
    if (!(ss >> l.cmd)) continue;
    l.cmd = lower(l.cmd);
    while (ss >> tok) l.args.push_back(tok);
    l.src_line = n;
    lines_.push_back(std::move(l));
  }
  return true;
}

void PlaytestScript::update_modifiers(Input& in) {
  in.ctrl = in.down(SDL_SCANCODE_LCTRL) || in.down(SDL_SCANCODE_RCTRL);
  in.shift = in.down(SDL_SCANCODE_LSHIFT) || in.down(SDL_SCANCODE_RSHIFT);
  in.alt = in.down(SDL_SCANCODE_LALT) || in.down(SDL_SCANCODE_RALT);
}

void PlaytestScript::press_key(Input& in, const std::string& name_in, bool down, bool auto_release) {
  std::string name = name_in;
  std::vector<SDL_Scancode> keys;
  for (;;) {
    std::string lo = lower(name);
    if (lo.rfind("ctrl+", 0) == 0) { keys.push_back(SDL_SCANCODE_LCTRL); name = name.substr(5); }
    else if (lo.rfind("shift+", 0) == 0) { keys.push_back(SDL_SCANCODE_LSHIFT); name = name.substr(6); }
    else if (lo.rfind("alt+", 0) == 0) { keys.push_back(SDL_SCANCODE_LALT); name = name.substr(4); }
    else break;
  }
  SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());
  const std::string alias = lower(name);
  if (alias == "ctrl" || alias == "lctrl") sc = SDL_SCANCODE_LCTRL;
  else if (alias == "rctrl") sc = SDL_SCANCODE_RCTRL;
  else if (alias == "shift" || alias == "lshift") sc = SDL_SCANCODE_LSHIFT;
  else if (alias == "rshift") sc = SDL_SCANCODE_RSHIFT;
  else if (alias == "alt" || alias == "lalt") sc = SDL_SCANCODE_LALT;
  else if (alias == "ralt") sc = SDL_SCANCODE_RALT;
  if (sc == SDL_SCANCODE_UNKNOWN) { ++failures_; std::fprintf(stderr, "FAIL playtest: unknown key %s\n", name.c_str()); return; }
  keys.push_back(sc);
  for (SDL_Scancode key : keys) {
    if (down && !in.key_down[key]) {
      in.key_hit[key] = true;
      if (auto_release) release_keys_.push_back(key);
    }
    in.key_down[key] = down;
  }
  update_modifiers(in);
}

static int integer(const std::string& text) {
  size_t end = 0;
  int value = std::stoi(text, &end);
  if (end != text.size()) throw std::invalid_argument("expected integer");
  return value;
}

bool PlaytestScript::evaluate(const World& w, const GameSim* sim, const Line& l, bool quiet) {
  try {
    if (l.args.size() < 3) throw std::invalid_argument("incomplete condition");
    size_t k = 0;
    const std::string name = lower(l.args[k++]);
    int64_t v = 0;
    int side = w.local_side;
    if ((name == "units" || name == "towns") && l.args.size() == 4) side = integer(l.args.at(k++));
    if (name == "money") v = w.side_state.at(size_t(side)).money;
    else if (name == "income") v = sim ? sim->income_total(side) : 0;
    else if (name == "tick") v = int64_t(w.tick);
    else if (name == "outcome") v = w.outcome;
    else if (name == "selection") v = int64_t(w.selection.ids.size());
    else if (name == "objective") v = sim && sim->script().trigger_enabled(integer(l.args.at(k++)));
    else if (name == "tile" || name == "cargo" || name == "complete") {
      const Object* o = w.find(uint32_t(integer(l.args.at(k++))));
      if (name == "tile") {
        int x = integer(l.args.at(k++)), y = integer(l.args.at(k++));
        v = o && o->alive && !o->removed && !o->carrier &&
            o->cur_x / (256 * kTilePx) == x && o->cur_y / (256 * kTilePx) == y;
      } else if (name == "cargo") v = o ? int64_t(o->cargo.size()) : -1;
      else v = o && o->alive && !o->removed && o->progress == 1000;
    } else if (name == "units" || name == "towns" || name == "enemies" || name == "count") {
      int type = name == "count" ? integer(l.args.at(k++)) : -1;
      for (const Object& o : w.objects) {
        const TypeInfo* ti = type_info(o.type);
        if (!o.alive || o.removed || !ti) continue;
        if (name == "enemies") v += w.hostile(w.local_side, o.side) && !ti->is_structure();
        else if (o.side == side) {
          if (name == "count") v += o.type == type && o.progress == 1000;
          else v += name == "towns" ? is_town_building(o.type) : !ti->is_structure();
        }
      }
    } else throw std::invalid_argument("unknown condition");
    const std::string op = l.args.at(k++);
    const int rhs = integer(l.args.at(k++));
    if (k != l.args.size() || (op != "==" && op != "!=" && op != "<" && op != "<=" && op != ">" && op != ">="))
      throw std::invalid_argument("invalid comparison");
    bool ok = op == "==" ? v == rhs : op == "!=" ? v != rhs : op == "<" ? v < rhs : op == "<=" ? v <= rhs : op == ">" ? v > rhs : v >= rhs;
    if (!quiet) std::printf("%s line %d: %s = %lld (%s %d)\n", ok ? "ok  " : "FAIL", l.src_line, name.c_str(), (long long)v, op.c_str(), rhs);
    return ok;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "playtest line %d: FAIL invalid condition: %s\n", l.src_line, e.what());
    return false;
  }
}

bool PlaytestScript::player_command(World& w, GameSim* sim, const Line& l) {
  try {
    if (!sim) throw std::invalid_argument("requires game simulation");
    auto number = [&](size_t i) { return integer(l.args.at(i)); };
    auto owned = [&](int id) -> const Object& {
      const Object* o = id > 0 ? w.find(uint32_t(id)) : nullptr;
      if (!o || !o->alive || o->removed || o->side != w.local_side || o->type < 0)
        throw std::invalid_argument("missing, dead or foreign object");
      return *o;
    };
    auto tile = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= w.terrain.w || y >= w.terrain.h)
        throw std::invalid_argument("tile outside map");
    };
    Command c; c.side = uint8_t(w.local_side); c.ids = w.selection.ids;
    const std::string action = l.cmd;
    if (action == "select") {
      if (l.args.size() != 2) throw std::invalid_argument("select requires type, id or group and integer");
      const std::string mode = lower(l.args[0]);
      const int value = number(1);
      std::vector<uint32_t> ids;
      if (mode == "id") ids.push_back(owned(value).id);
      else if (mode == "type") {
        for (const auto& o : w.objects) if (o.alive && !o.removed && o.side == w.local_side && o.type >= 0 && o.type == value) ids.push_back(o.id);
      } else if (mode == "group") {
        if (value < 0 || value > 9) throw std::invalid_argument("group outside 0..9");
        c.type = Pct::SelectGroup; c.a = value; c.ids.clear(); sim->submit(c);
        for (auto id : w.side_state.at(size_t(w.local_side)).groups[value]) {
          const Object* o = w.find(id);
          if (o && o->alive && !o->removed && o->side == w.local_side) ids.push_back(id);
        }
      } else throw std::invalid_argument("unknown selection mode");
      if (ids.empty()) throw std::invalid_argument("empty selection");
      w.selection.ids = std::move(ids); return true;
    }
    if (action == "train" || action == "build" || action == "harvest" || action == "gather") {
      if (l.args.size() != (action == "train" ? 3u : 4u)) throw std::invalid_argument("invalid production arguments");
      c.a = number(0);
      const int member = number(action == "train" ? 2 : 3);
      const auto& producer = owned(member);
      if (producer.town < 0) throw std::invalid_argument("producer has no town");
      auto menu = sim->menu(w, w.local_side, producer.town);
      auto item = std::find_if(menu.begin(), menu.end(), [&](const auto& m) { return m.type == c.a; });
      if (item == menu.end() || !item->enabled) throw std::invalid_argument("production unavailable");
      c.ids = {producer.id};
      if (action == "train") {
        int count = number(1);
        if (count < 1 || count > kProductionQueueCapacity || sim->rules().type(c.a).structure)
          throw std::invalid_argument("train requires 1..50 mobile units");
        c.type = Pct::Build;
        for (int i = 0; i < count; ++i) sim->submit(c);
        return true;
      }
      c.b = number(1); c.c = number(2); tile(c.b, c.c);
      const auto& rules = sim->rules().type(c.a);
      if (!rules.structure) throw std::invalid_argument("unsupported action: worker gathering has no player command");
      if ((action == "harvest" || action == "gather") && rules.harvest.rate_a <= 0 && rules.harvest.rate_b <= 0)
        throw std::invalid_argument("harvest requires an income producing structure");
      const char* why = nullptr;
      if (!sim->can_place(w, w.local_side, c.a, c.b, c.c, &why, nullptr, producer.town))
        throw std::invalid_argument(why ? why : "invalid placement");
      c.type = item->construct ? Pct::Build : Pct::PlaceBuilding;
    } else {
      if (c.ids.empty()) throw std::invalid_argument("empty selection");
      for (auto id : c.ids) owned(int(id));
      if (action == "group") {
        if (l.args.size() != 2 || lower(l.args[0]) != "assign" || number(1) < 0 || number(1) > 9)
          throw std::invalid_argument("group assign requires 0..9; recall with select group");
        c.type = Pct::CreateGroup; c.a = number(1);
      } else if (action == "order-move" || action == "attack-move" || action == "unload") {
        if (l.args.size() != 2) throw std::invalid_argument("requires tile X Y");
        tile(number(0), number(1)); c.a = number(0) * kTilePx + kTilePx / 2; c.b = number(1) * kTilePx + kTilePx / 2;
        c.type = action == "unload" ? Pct::Unload : action == "attack-move" ? Pct::AttackMove : Pct::Move;
        if (action == "unload") c.c = 0;
      } else if (action == "context") {
        if (l.args.size() != 1) throw std::invalid_argument("context requires object ID");
        const auto* target = w.find(uint32_t(number(0)));
        if (!target || !target->alive || target->removed) throw std::invalid_argument("missing context target");
        if (target->side == w.local_side && is_transport(target->type)) { c.type = Pct::Board; c.a = int(target->id); }
        else if (target->side != w.local_side && target->type >= 0) { c.type = Pct::Attack; c.a = int(target->id); }
        else { c.type = Pct::Move; c.a = target->cur_x / 256; c.b = target->cur_y / 256; }
      } else throw std::invalid_argument("unsupported action");
    }
    sim->submit(c); return true;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL playtest line %d tick %llu: %s\n", l.src_line, (unsigned long long)w.tick, e.what());
    return false;
  }
}

void PlaytestScript::observe(const World& w, const GameSim* sim) {
  if (!observations_.is_open() || !sim || observed_tick_ == w.tick) return;
  observed_tick_ = w.tick;
  observations_ << "{\"tick\":" << w.tick << ",\"outcome\":" << int(w.outcome)
                << ",\"hash\":\"" << sim->state_hash(w) << "\",\"failures\":" << failures_ << ",\"money\":" << w.side_state.at(size_t(w.local_side)).money
                << ",\"income\":" << sim->income_total(w.local_side) << ",\"objects\":[";
  bool comma = false;
  for (const auto& o : w.objects) {
    if (o.side != w.local_side) continue;
    observations_ << (comma ? "," : "") << "{\"id\":" << o.id << ",\"type\":" << o.type
        << ",\"alive\":" << (o.alive && (!o.removed || o.carrier) ? "true" : "false") << ",\"position\":[" << o.cur_x / 256 << ',' << o.cur_y / 256
        << "],\"order\":" << int(o.order) << ",\"destination\":[" << o.dest_x << ',' << o.dest_y << "],\"target\":" << o.target
        << ",\"carrier\":" << o.carrier << ",\"boarding\":" << o.boarding << ",\"progress\":" << o.progress
        << ",\"mobile\":" << (sim->rules().type(o.type).mobile ? "true" : "false")
        << ",\"path_failures\":" << sim->path_failures(o) << ",\"cargo\":[";
    for (size_t i = 0; i < o.cargo.size(); ++i) observations_ << (i ? "," : "") << o.cargo[i];
    observations_ << "]}"; comma = true;
  }
  observations_ << "],\"queue\":[";
  const auto& queue = w.side_state.at(size_t(w.local_side)).queue;
  for (size_t i = 0; i < queue.size(); ++i) observations_ << (i ? "," : "") << queue[i].type;
  observations_ << "],\"commands\":[";
  const auto& log = sim->log();
  bool command_comma = false;
  for (size_t i = observed_commands_; i < log.size(); ++i) {
    const auto& c = log[i].cmd;
    if (c.side != w.local_side) continue;
    observations_ << (command_comma ? "," : "") << "{\"side\":" << int(c.side) << ",\"type\":" << int(c.type) << ",\"a\":" << c.a << ",\"b\":" << c.b
                  << ",\"c\":" << c.c << ",\"ids\":[";
    for (size_t j = 0; j < c.ids.size(); ++j) observations_ << (j ? "," : "") << c.ids[j];
    observations_ << "]}"; command_comma = true;
  }
  observed_commands_ = log.size();
  size_t unsupported_sites = 0;
  for (const auto& action : sim->script().action_observations(w))
    if (!ScriptRunner::supports(action.kind)) ++unsupported_sites;
  observations_ << "],\"unsupported_sites\":" << unsupported_sites << ",\"unsupported\":" << sim->script().unsupported() << "}\n";
  observations_.flush();
  if (!observations_) { ++failures_; std::fprintf(stderr, "FAIL playthrough observation write\n"); }
}

void PlaytestScript::finish(const World& w) {
  if (!observations_.is_open()) return;
  observations_ << "{\"event\":\"end\",\"tick\":" << w.tick << ",\"script_complete\":"
                << (completed_ ? "true" : "false") << ",\"failures\":" << failures_ << "}\n";
  observations_.flush();
  if (!observations_) { ++failures_; std::fprintf(stderr, "FAIL playthrough final observation write\n"); }
}

bool PlaytestScript::step(Input& in, World& w, GameSim* sim, PlaytestRequest& req) {
  if (limit_reached(w)) { req.quit = true; return false; }
  // frame after a click / key press: release
  if (release_left_) { in.buttons[SDL_BUTTON_LEFT] = false; in.released[SDL_BUTTON_LEFT] = true; release_left_ = false; }
  if (release_right_) { in.buttons[SDL_BUTTON_RIGHT] = false; in.released[SDL_BUTTON_RIGHT] = true; release_right_ = false; }
  if (release_middle_) { in.buttons[SDL_BUTTON_MIDDLE] = false; in.released[SDL_BUTTON_MIDDLE] = true; release_middle_ = false; }
  for (SDL_Scancode key : release_keys_) in.key_down[key] = false;
  release_keys_.clear();
  update_modifiers(in);
  if (until_tick_ && w.tick < until_tick_ && w.outcome == kRunning) return true;
  until_tick_ = 0;
  if (drag_phase_ > 0) {
    // 1: move to start, 2: press, 3: move to end (buttons held), 4: release
    switch (drag_phase_) {
      case 1: in.mx = dx0_; in.my = dy0_; drag_phase_ = 2; return true;
      case 2: in.buttons[SDL_BUTTON_LEFT] = true; in.pressed[SDL_BUTTON_LEFT] = true; drag_phase_ = 3; return true;
      case 3: in.mx = dx1_; in.my = dy1_; drag_phase_ = 4; return true;
      default: in.buttons[SDL_BUTTON_LEFT] = false; in.released[SDL_BUTTON_LEFT] = true; drag_phase_ = 0; return true;
    }
  }
  if (wait_ > 0) { wait_--; return true; }
  while (pc_ < lines_.size()) {
    const Line& l = lines_[pc_++];
    auto num = [&](size_t i) { return i < l.args.size() ? std::atoi(l.args[i].c_str()) : 0; };
    if (l.cmd == "budget") {
      try {
        if (l.args.size() != 1 || integer(l.args[0]) < 1 || integer(l.args[0]) > 10000000)
          throw std::invalid_argument("budget requires 1..10000000 ticks");
        tick_limit_ = uint64_t(integer(l.args[0]));
      } catch (const std::exception&) { failures_++; req.quit = true; return false; }
      continue;
    }
    if (l.cmd == "capture") {
      try {
        if (l.args.size() != 2 || integer(l.args[0]) < 1) throw std::invalid_argument("capture tick and path required");
        captures_[uint64_t(integer(l.args[0]))] = l.args[1];
      } catch (const std::exception&) { failures_++; req.quit = true; return false; }
      continue;
    }
    if (l.cmd == "record") {
      if (l.args.size() != 1 || observations_.is_open()) { failures_++; req.quit = true; return false; }
      observations_.open(l.args[0]);
      if (!observations_) { failures_++; req.quit = true; return false; }
      observed_commands_ = sim ? sim->log().size() : 0; observe(w, sim); continue;
    }
    if (l.cmd == "select" || l.cmd == "train" || l.cmd == "build" || l.cmd == "harvest" || l.cmd == "gather" ||
        l.cmd == "group" || l.cmd == "order-move" || l.cmd == "attack-move" || l.cmd == "context" || l.cmd == "unload") {
      if (!player_command(w, sim, l)) { failures_++; req.quit = true; return false; }
      return true;
    }
    if (l.cmd == "await") {
      try {
        Line condition = l;
        const int timeout = integer(condition.args.at(0));
        if (timeout < 1 || timeout > 10000000) throw std::invalid_argument("invalid timeout");
        condition.args.erase(condition.args.begin());
        if (!deadline_) deadline_ = w.tick + uint64_t(timeout);
        if (evaluate(w, sim, condition, true)) { deadline_ = 0; continue; }
        if (w.tick >= deadline_ || w.outcome != kRunning) {
          evaluate(w, sim, condition); failures_++; req.quit = true;
          std::fprintf(stderr, "FAIL condition timeout line %d tick %llu\n", l.src_line, (unsigned long long)w.tick);
          return false;
        }
        --pc_; return true;
      } catch (const std::exception&) { failures_++; req.quit = true; return false; }
    }
    if (l.cmd == "checkpoint") {
      if (!evaluate(w, sim, l)) { failures_++; req.quit = true; return false; }
      continue;
    }
    if (l.cmd == "wait") { wait_ = std::max(0, num(0) - 1); return true; }
    if (l.cmd == "until") {
      char* end = nullptr;
      const long target = l.args.size() == 1 ? std::strtol(l.args[0].c_str(), &end, 10) : 0;
      if (!end || end == l.args[0].c_str() || *end || target < 1 || target > 7200) {
        std::fprintf(stderr, "playtest line %d: until requires 1..7200 ticks\n", l.src_line);
        failures_++; continue;
      }
      if (w.tick < uint64_t(target) && w.outcome == kRunning) { until_tick_ = uint64_t(target); return true; }
      continue;
    }
    if (l.cmd == "run") { req.run_ticks = num(0); return true; }
    if (l.cmd == "move") { in.mx = num(0); in.my = num(1); return true; }
    if (l.cmd == "click" || l.cmd == "rclick" || l.cmd == "mclick") {
      in.mx = num(0); in.my = num(1);
      int b = l.cmd == "click" ? SDL_BUTTON_LEFT : l.cmd == "rclick" ? SDL_BUTTON_RIGHT : SDL_BUTTON_MIDDLE;
      in.buttons[b] = true; in.pressed[b] = true;
      (b == SDL_BUTTON_LEFT ? release_left_ : b == SDL_BUTTON_RIGHT ? release_right_ : release_middle_) = true;
      return true;
    }
    if (l.cmd == "down") { in.buttons[SDL_BUTTON_LEFT] = true; in.pressed[SDL_BUTTON_LEFT] = true; return true; }
    if (l.cmd == "up") { in.buttons[SDL_BUTTON_LEFT] = false; in.released[SDL_BUTTON_LEFT] = true; return true; }
    if (l.cmd == "rdown") { in.buttons[SDL_BUTTON_RIGHT] = true; in.pressed[SDL_BUTTON_RIGHT] = true; return true; }
    if (l.cmd == "rup") { in.buttons[SDL_BUTTON_RIGHT] = false; in.released[SDL_BUTTON_RIGHT] = true; return true; }
    if (l.cmd == "drag") { dx0_ = num(0); dy0_ = num(1); dx1_ = num(2); dy1_ = num(3); drag_phase_ = 1; return step(in, w, sim, req); }
    if (l.cmd == "key" || l.cmd == "keydown" || l.cmd == "keyup") {
      if (!l.args.empty()) press_key(in, l.args[0], l.cmd != "keyup", l.cmd == "key");
      return true;
    }
    if (l.cmd == "shot" && !l.args.empty()) { req.shot = l.args[0]; return true; }
    if (l.cmd == "print") {
      int units[8] = {};
      for (const Object& o : w.objects) { const TypeInfo* ti = type_info(o.type); if (!o.removed && o.alive && o.side >= 0 && o.side < 8 && ti && !ti->is_structure()) units[o.side]++; }
      std::printf("tick %llu hash %016llx money %d outcome %d units %d/%d selection %zu\n", (unsigned long long)w.tick,
                  sim ? (unsigned long long)sim->state_hash(w) : 0ull, w.side_state.empty() ? 0 : w.side_state[size_t(w.local_side)].money,
                  w.outcome, units[0], units[1], w.selection.ids.size());
      continue;
    }
    if (l.cmd == "expect") { if (!evaluate(w, sim, l)) failures_++; continue; }
    if (l.cmd == "quit") { completed_ = pc_ == lines_.size(); req.quit = true; return false; }
    std::fprintf(stderr, "FAIL playtest line %d: unsupported action %s\n", l.src_line, l.cmd.c_str());
    failures_++; req.quit = true; return false;
  }
  completed_ = true; req.quit = true;
  return false;
}

}  // namespace aztec::engine
