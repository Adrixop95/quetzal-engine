// Input and right-hand panel presentation. Gameplay changes are submitted as simulation commands.
#pragma once
#include <SDL.h>
#include <array>
#include <string>
#include <vector>
#include "core/strbase.hpp"
#include "core/userfiles.hpp"
#include "engine/data/sprites.hpp"
#include "engine/platform/input.hpp"
#include "engine/render/font.hpp"
#include "engine/render/sprite_store.hpp"
#include "engine/sim/game_sim.hpp"
#include "engine/ui/minimap.hpp"
#include "engine/ui/panel_layout.hpp"
#include "engine/world/camera.hpp"
#include "engine/world/world.hpp"

namespace aztec::engine {

enum class PanelAction { Move, Attack, Stop, Repair, Garrison, Destroy };
enum class TargetMode { None, Move, Attack };

struct CommandButtonState {
  int frame = 0;
  bool enabled = false, highlight = false;
};

// Read-only placement diagnostics, shared with click validation and the overlay.
struct PlacementPreview {
  int x = 0, y = 0;
  bool valid = false;
  std::vector<PlacementCell> cells;
};

// Shared by drawing, hit testing, and pure tests. All rectangles include the panel origin.
struct CommandPanelLayout {
  SDL_Rect portrait{}, summary{}, previous{}, next{}, pause{};
  std::array<SDL_Rect, 6> commands{};
  std::vector<SDL_Rect> production;
  std::array<SDL_Rect, 8> queue{};
};

class CommandsUi {
 public:
  explicit CommandsUi(GameSim* sim = nullptr);
  void set_sim(GameSim* s) { sim_ = s; cancel_mode(); selection_.clear(); ui_frame_ = 0; }
  void set_layout(InterfaceLayout mode) { layout_mode_ = mode; cancel_mode(); }
  InterfaceLayout layout_mode() const { return layout_mode_; }
  static int selected_town(const World& w);
  bool active() const { return sim_ != nullptr; }
  void set_bindings(std::vector<KeyBinding> bindings) { bindings_ = std::move(bindings); }
  bool action_hit(const Input& in, const char* action) const;
  bool action_down(const Input& in, const char* action) const;
  bool key_bound(SDL_Scancode key) const;
  // Presentation-only camera controls; bindings also work without a simulation.
  bool fast_scrolling(const Input& in) const;
  bool center_selection(const Input& in, const World& w, Camera& cam, float alpha);
  static SDL_Scancode sdl_scancode(int dik);
  bool open_sprites(const std::string& path) { return icons_.open(path); }
  // Returns true if a targeting click was consumed, including the frame that completes the order.
  bool update(const Input& in, World& w, const Camera& cam, float alpha, const Minimap& mm, bool in_world, const SDL_Rect& panel);
  bool right_dragging() const { return layout_mode_ == InterfaceLayout::Modern && right_down_ && right_moved_; }
  bool wants_left_click() const { return mode_ != TargetMode::None || placing_ >= 0; }
  int service_target_mode() const { return service_target_; }
  bool attack_move() const { return mode_ == TargetMode::Attack; }
  TargetMode target_mode() const { return mode_; }
  void cancel_mode() { mode_ = TargetMode::None; placing_ = -1; pressed_control_ = 0; }
  int placing() const { return placing_; }
  PlacementPreview placement_preview(const World& w, const Camera& cam, int mx, int my) const;
  std::string draw_menu(SDL_Renderer* r, SpriteStore& store, const Font& font, const StrBase& strings, const World& w,
                        const SDL_Rect& panel, int scale, int mx, int my) const;
  void draw_ghost(SDL_Renderer* r, const World& w, const Camera& cam, int mx, int my, SpriteStore* store = nullptr) const;
  static CommandPanelLayout layout(const SDL_Rect& panel, int scale, InterfaceLayout mode = InterfaceLayout::Modern, bool town = false);
  static bool action_enabled(PanelAction action, const World& w, const RulesData* rules = nullptr);
  static const char* disabled_reason(PanelAction action, const World& w, const RulesData* rules = nullptr);
  // Shared presentation state for rendering and UI observations; hover only adds a tooltip.
  CommandButtonState button_state(PanelAction action, const World& w) const;
  static int icon_slot(int kind);
  // Resolves icons/portraits from the user's Units.xbe animation sequences, never from embedded pixels.
  uint32_t icon_sprite(int type, bool portrait) const;

 private:
  bool original_control(const Input& in, World& w, const SDL_Rect& panel, int scale);
  std::string draw_original(SDL_Renderer* r, SpriteStore& store, const Font& font, const StrBase& strings,
                            const World& w, const SDL_Rect& panel, int scale, int mx, int my) const;
  InterfaceLayout layout_mode_ = InterfaceLayout::Modern;
  int service_target_ = 0;
  bool center_requested_ = false;
  void issue_to_selection(const World& w, Command c) const;
  void activate(PanelAction action, const World& w);
  bool bound_hit(const Input& in, SDL_Scancode key) const;
  GameSim* sim_;
  data::SpriteDb icons_;
  std::vector<KeyBinding> bindings_;
  TargetMode mode_ = TargetMode::None;
  int placing_ = -1;
  uint32_t placing_member_ = 0;
  int placing_town_ = -1;
  Pct placing_command_ = Pct::Build;
  int page_ = 0;
  int queue_offset_ = 0, queue_town_ = -1;
  int pressed_control_ = 0;
  size_t pressed_queue_size_ = 0;
  // Presentation frames only: never participate in simulation or replay hashes.
  uint32_t ui_frame_ = 0;
  std::vector<uint32_t> selection_;
  bool right_down_ = false, right_moved_ = false;
  int rx_ = 0, ry_ = 0;
};

}  // namespace aztec::engine
