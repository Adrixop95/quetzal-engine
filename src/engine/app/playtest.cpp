#include "engine/app/playtest.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

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
  lines_.clear(); pc_ = 0;
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

void PlaytestScript::press_key(Input& in, const std::string& name_in, bool down) {
  std::string name = name_in;
  bool ctrl = false, shift = false;
  for (;;) {
    std::string lo = lower(name);
    if (lo.rfind("ctrl+", 0) == 0) { ctrl = true; name = name.substr(5); }
    else if (lo.rfind("shift+", 0) == 0) { shift = true; name = name.substr(6); }
    else break;
  }
  SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());
  if (sc == SDL_SCANCODE_UNKNOWN) { std::fprintf(stderr, "playtest: unknown key %s\n", name.c_str()); return; }
  in.key_down[sc] = down;
  if (down) in.key_hit[sc] = true;
  in.ctrl = ctrl && down;
  in.shift = shift && down;
  release_key_ = down ? name_in : "";
}

bool PlaytestScript::evaluate(const World& w, const Line& l) {
  if (l.args.size() < 3) return true;
  size_t k = 0;
  std::string name = lower(l.args[k++]);
  long v = 0;
  int side = w.local_side;
  if ((name == "units" || name == "towns") && l.args.size() >= 4) side = std::atoi(l.args[k++].c_str());
  std::string op = l.args[k++];
  long rhs = std::atol(l.args[k].c_str());
  if (name == "money") v = side < int(w.side_state.size()) ? w.side_state[size_t(w.local_side)].money : 0;
  else if (name == "tick") v = long(w.tick);
  else if (name == "outcome") v = w.outcome;
  else if (name == "selection") v = long(w.selection.ids.size());
  else if (name == "units") { for (const Object& o : w.objects) { const TypeInfo* ti = type_info(o.type); if (!o.removed && o.alive && o.side == side && ti && !ti->is_structure()) v++; } }
  else if (name == "towns") { for (const Object& o : w.objects) if (!o.removed && o.alive && o.side == side && is_town_building(o.type)) v++; }
  else if (name == "enemies") { for (const Object& o : w.objects) { const TypeInfo* ti = type_info(o.type); if (!o.removed && o.alive && w.hostile(w.local_side, o.side) && ti && !ti->is_structure()) v++; } }
  else { std::fprintf(stderr, "playtest line %d: unknown expect %s\n", l.src_line, name.c_str()); return false; }
  bool ok = op == "==" ? v == rhs : op == "!=" ? v != rhs : op == "<" ? v < rhs : op == "<=" ? v <= rhs : op == ">" ? v > rhs : op == ">=" ? v >= rhs : false;
  std::printf("%s line %d: %s = %ld (%s %ld)\n", ok ? "ok  " : "FAIL", l.src_line, name.c_str(), v, op.c_str(), rhs);
  return ok;
}

bool PlaytestScript::step(Input& in, const World& w, const GameSim* sim, PlaytestRequest& req) {
  // frame after a click / key press: release
  if (release_left_) { in.buttons[SDL_BUTTON_LEFT] = false; in.released[SDL_BUTTON_LEFT] = true; release_left_ = false; }
  if (release_right_) { in.buttons[SDL_BUTTON_RIGHT] = false; in.released[SDL_BUTTON_RIGHT] = true; release_right_ = false; }
  if (release_middle_) { in.buttons[SDL_BUTTON_MIDDLE] = false; in.released[SDL_BUTTON_MIDDLE] = true; release_middle_ = false; }
  if (!release_key_.empty()) { press_key(in, release_key_, false); in.ctrl = in.shift = false; release_key_.clear(); }
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
    if (l.cmd == "wait") { wait_ = std::max(0, num(0) - 1); return true; }
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
    if (l.cmd == "key" && !l.args.empty()) { press_key(in, l.args[0], true); return true; }
    if (l.cmd == "shot" && !l.args.empty()) { req.shot = l.args[0]; return true; }
    if (l.cmd == "print") {
      int units[8] = {};
      for (const Object& o : w.objects) { const TypeInfo* ti = type_info(o.type); if (!o.removed && o.alive && o.side >= 0 && o.side < 8 && ti && !ti->is_structure()) units[o.side]++; }
      std::printf("tick %llu hash %016llx money %d outcome %d units %d/%d selection %zu\n", (unsigned long long)w.tick,
                  sim ? (unsigned long long)sim->state_hash(w) : 0ull, w.side_state.empty() ? 0 : w.side_state[size_t(w.local_side)].money,
                  w.outcome, units[0], units[1], w.selection.ids.size());
      continue;
    }
    if (l.cmd == "expect") { if (!evaluate(w, l)) failures_++; continue; }
    if (l.cmd == "quit") { req.quit = true; return false; }
    std::fprintf(stderr, "playtest line %d: unknown command %s\n", l.src_line, l.cmd.c_str());
  }
  req.quit = true;
  return false;
}

}  // namespace aztec::engine
