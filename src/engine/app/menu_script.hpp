// Frame-scheduled front-end input. This observes the independent engine only.
#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "core/loader_limits.hpp"
#include "core/mapfiles.hpp"
#include "engine/platform/input.hpp"

namespace aztec::engine {

class MenuScript {
 public:
  struct Request { std::string shot; bool quit = false; };

  // Each line is FRAME move X Y | down | up | keydown NAME | keyup NAME |
  // shot PATH | close | quit. Frames start at zero, with a 60 Hz cursor clock.
  // Equal frames retain file order. A script ends after its final frame.
  bool load(const std::string& path, std::string& error) {
    events_.clear(); next_ = 0;
    std::vector<uint8_t> bytes;
    if (!read_file(path, bytes) || bytes.size() > detail::kTextBytes) {
      error = "Cannot read bounded menu script: " + path; return false;
    }
    std::istringstream file(std::string(bytes.begin(), bytes.end()));
    std::string line;
    int number = 0, previous = -1;
    while (std::getline(file, line)) {
      ++number;
      if (auto hash = line.find('#'); hash != std::string::npos) line.resize(hash);
      if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
      std::istringstream row(line);
      Event event;
      bool valid = bool(row >> event.frame >> event.command);
      valid = valid && event.frame >= previous && event.frame >= 0 && event.frame <= 100000;
      if (event.command == "move") valid = valid && bool(row >> event.x >> event.y);
      else if (event.command == "shot" || event.command == "keydown" || event.command == "keyup") {
        std::getline(row >> std::ws, event.argument);
        while (!event.argument.empty() && (event.argument.back() == '\r' || event.argument.back() == ' ')) event.argument.pop_back();
        valid = valid && !event.argument.empty();
        if (event.command != "shot") valid = valid && SDL_GetScancodeFromName(event.argument.c_str()) != SDL_SCANCODE_UNKNOWN;
      } else valid = valid && (event.command == "down" || event.command == "up" || event.command == "close" || event.command == "quit");
      std::string extra;
      if (row >> extra) valid = false;
      if (!valid) { error = "Invalid menu script line " + std::to_string(number); events_.clear(); return false; }
      previous = event.frame;
      events_.push_back(std::move(event));
    }
    if (events_.empty()) { error = "Empty menu script"; return false; }
    return true;
  }

  Request step(int frame, Input& input) {
    Request request;
    if (next_ == events_.size()) { request.quit = true; return request; }
    while (next_ < events_.size() && events_[next_].frame == frame) {
      const auto& event = events_[next_++];
      if (event.command == "shot") { request.shot = event.argument; continue; }
      if (event.command == "quit") { request.quit = true; continue; }
      SDL_Event e{};
      if (event.command == "move") {
        e.type = SDL_MOUSEMOTION;
        e.motion.x = event.x; e.motion.y = event.y;
        e.motion.xrel = event.x - input.mx; e.motion.yrel = event.y - input.my;
      } else if (event.command == "down" || event.command == "up") {
        e.type = event.command == "down" ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        e.button.button = SDL_BUTTON_LEFT;
        e.button.x = input.mx; e.button.y = input.my;
      } else if (event.command == "close") e.type = SDL_QUIT;
      else {
        e.type = event.command == "keydown" ? SDL_KEYDOWN : SDL_KEYUP;
        e.key.keysym.scancode = SDL_GetScancodeFromName(event.argument.c_str());
      }
      input.handle(e, 1, 1);
    }
    // SDL's physical keyboard state does not contain injected key events.
    input.alt = input.down(SDL_SCANCODE_LALT) || input.down(SDL_SCANCODE_RALT);
    input.ctrl = input.down(SDL_SCANCODE_LCTRL) || input.down(SDL_SCANCODE_RCTRL);
    input.shift = input.down(SDL_SCANCODE_LSHIFT) || input.down(SDL_SCANCODE_RSHIFT);
    return request;
  }

 private:
  struct Event { int frame = 0, x = 0, y = 0; std::string command, argument; };
  std::vector<Event> events_;
  size_t next_ = 0;
};
}  // namespace aztec::engine
