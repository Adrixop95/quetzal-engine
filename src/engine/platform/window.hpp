// SDL window + renderer wrapper: windowed / borderless / exclusive fullscreen, any size and aspect.
#pragma once
#include <SDL.h>
#include <string>

namespace aztec::engine {

enum class DisplayMode { Windowed, Borderless, Fullscreen };

struct WindowOptions {
  int width = 1280, height = 720;
  DisplayMode mode = DisplayMode::Windowed;
  bool vsync = true;
  bool software = false;   // force SDL software renderer
  std::string title = "Aztec Empire";
};

class Window {
 public:
  ~Window();
  bool create(const WindowOptions& o);
  void set_mode(DisplayMode m);
  DisplayMode mode() const { return mode_; }
  void cycle_borderless();    // F11
  void toggle_fullscreen();   // Alt+Enter
  void handle_event(const SDL_Event& event);
  void set_gameplay_active(bool active);
  void set_mouse_grab_enabled(bool enabled);
  bool mouse_grab_enabled() const { return mouse_grab_enabled_; }
  void set_title(const std::string& t) { SDL_SetWindowTitle(win_, t.c_str()); }

  SDL_Window* sdl() const { return win_; }
  SDL_Renderer* renderer() const { return ren_; }
  // Renderer output size in pixels (differs from window size on HiDPI).
  void output_size(int& w, int& h) const { SDL_GetRendererOutputSize(ren_, &w, &h); }
  // Factors converting SDL window coordinates (mouse events) to output pixels.
  void mouse_scale(float& sx, float& sy) const;
  bool vsync() const { return vsync_; }

 private:
  SDL_Window* win_ = nullptr;
  SDL_Renderer* ren_ = nullptr;
  DisplayMode mode_ = DisplayMode::Windowed;
  bool vsync_ = false;
  bool mouse_grab_enabled_ = true;
  bool gameplay_active_ = false;
  bool focused_ = false;
  bool left_alt_ = false, right_alt_ = false;
  void apply_mouse_grab();
};

}  // namespace aztec::engine
