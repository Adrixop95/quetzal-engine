// Turns mouse/keyboard input into simulation commands: right-click move/attack, stop (H), attack-move (A + click),
// control groups (Ctrl+0-9 / 0-9), minimap orders, and the production / building menu in the right-hand panel.
#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include "core/strbase.hpp"
#include "engine/platform/input.hpp"
#include "engine/render/font.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/ui/minimap.hpp"
#include "engine/world/camera.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

class CommandsUi {
 public:
  explicit CommandsUi(GameSim* sim = nullptr) : sim_(sim) {}
  bool active() const { return sim_ != nullptr; }
  // Call once per frame before the selection UI. `in_world`: the cursor is over the world view.
  void update(const Input& in, World& w, const Camera& cam, float alpha, const Minimap& mm, bool in_world, const SDL_Rect& panel);
  bool right_dragging() const { return right_down_ && right_moved_; }   // the engine pans the map meanwhile
  bool wants_left_click() const { return attack_move_ || placing_ >= 0; }
  bool attack_move() const { return attack_move_; }
  int placing() const { return placing_; }
  // Menu in the panel (below the selection summary). Returns the tooltip of the row under the mouse, if any.
  std::string draw_menu(SDL_Renderer* r, const Font& font, const StrBase& strings, const World& w, const SDL_Rect& panel,
                        int scale, int mx, int my) const;
  // Placement ghost drawn over the world.
  void draw_ghost(SDL_Renderer* r, const World& w, const Camera& cam, int mx, int my) const;
  // Layout shared by draw and click: first row y and row height.
  static void menu_layout(const SDL_Rect& panel, int scale, int& x, int& y0, int& row_h);

 private:
  void issue_to_selection(const World& w, Command c) const;
  GameSim* sim_;
  bool attack_move_ = false;
  int placing_ = -1;
  bool right_down_ = false, right_moved_ = false;
  int rx_ = 0, ry_ = 0;
  SDL_Rect panel_{0, 0, 0, 0};
};

}  // namespace aztec::engine
