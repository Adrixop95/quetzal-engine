// Independent renderer diagnostics; engine IDs and commands require normalization
// before comparison with reference observations. No reference data is embedded.
#pragma once
#include <iomanip>
#include <ostream>
#include <string>
#include "engine/ui/commands_ui.hpp"
#include "engine/ui/cursor.hpp"

namespace aztec::engine {
inline void trace_string(std::ostream& out, const std::string& text) {
  out << '"';
  for (unsigned char c : text) {
    if (c == '"' || c == '\\') out << '\\' << char(c);
    else if (c < 32) out << "\\u00" << "0123456789abcdef"[c >> 4] << "0123456789abcdef"[c & 15];
    else out << char(c);
  }
  out << '"';
}
inline void trace_ui(std::ostream& out, int frame, const World& world, const Camera& cam,
                     float alpha, const Input& input, const CommandsUi& ui, const GameSim* sim,
                     size_t command_start, SDL_Rect panel, int scale, bool panel_visible,
                     bool menu_open, CursorKind cursor, bool cursor_visible, uint32_t cursor_ms,
                     const std::string& tooltip, const std::string& screenshot) {
  static const char* cursors[] = {"arrow", "attack", "move", "pan", "north", "northeast", "east",
                                 "southeast", "south", "southwest", "west", "northwest"};
  out << std::setprecision(9) << "{\"schema\":1,\"contract\":\"engine-ui-raw-v1\",\"frame\":" << frame
      << ",\"tick\":" << world.tick << ",\"phase\":\"after_input_before_present\",\"camera\":["
      << cam.x << ',' << cam.y << ',' << cam.zoom << "],\"alpha\":" << alpha
      << ",\"mouse\":[" << input.mx << ',' << input.my << "],\"left_down\":" << (input.buttons[SDL_BUTTON_LEFT] ? "true" : "false")
      << ",\"target_mode\":" << int(ui.target_mode()) << ",\"placing\":" << ui.placing()
      << ",\"panel\":" << (panel_visible ? "true" : "false") << ",\"menu\":" << (menu_open ? "true" : "false")
      << ",\"cursor\":{\"shape\":\"" << cursors[int(cursor)] << "\",\"visible\":" << (cursor_visible ? "true" : "false")
      << ",\"time_ms\":" << cursor_ms << "},\"tooltip\":";
  trace_string(out, tooltip);
  out << ",\"screenshot\":"; trace_string(out, screenshot);
  out << ",\"selection\":[";
  for (size_t i = 0; i < world.selection.ids.size(); ++i) out << (i ? "," : "") << world.selection.ids[i];
  out << "],\"buttons\":[";
  auto layout = CommandsUi::layout(panel, scale);
  for (size_t i = 0; panel_visible && i < layout.commands.size(); ++i) {
    SDL_Point p{input.mx, input.my};
    bool hover = SDL_PointInRect(&p, &layout.commands[i]);
    bool selected = (i == 0 && ui.target_mode() == TargetMode::Move) || (i == 1 && ui.target_mode() == TargetMode::Attack);
    bool enabled = sim && CommandsUi::action_enabled(PanelAction(i), world, &sim->rules());
    out << (i ? "," : "") << "{\"frame\":0,\"hover\":" << (hover ? "true" : "false")
        << ",\"highlight\":" << ((hover || selected) ? "true" : "false")
        << ",\"enabled\":" << (enabled ? "true" : "false") << '}';
  }
  out << "],\"commands\":[";
  if (sim) {
    const auto& commands = sim->pending_commands();
    for (size_t i = command_start; i < commands.size(); ++i) {
      const auto& c = commands[i];
      out << (i > command_start ? "," : "") << "{\"type\":" << int(c.type) << ",\"side\":" << int(c.side)
          << ",\"a\":" << c.a << ",\"b\":" << c.b << ",\"c\":" << c.c << ",\"ids\":[";
      for (size_t j = 0; j < c.ids.size(); ++j) out << (j ? "," : "") << c.ids[j];
      out << "]}";
    }
  }
  out << "]}\n";
}
}  // namespace aztec::engine
