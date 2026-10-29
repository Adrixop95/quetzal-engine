// Synthetic SDL policy checks; the dummy driver cannot prove desktop confinement.
#include <SDL.h>
#include <cstdio>
#include <string>

#include "engine/app/options.hpp"
#include "engine/platform/input.hpp"
#include "engine/platform/window.hpp"
#include "engine/world/camera.hpp"

using namespace aztec::engine;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static void test_window(DisplayMode mode) {
  Window window;
  WindowOptions options;
  options.width = 800; options.height = 600;
  options.software = true;
  CHECK(window.create(options));
  if (!window.sdl()) return;
  const Uint32 id = SDL_GetWindowID(window.sdl());
  auto grabbed = [&] { return (SDL_GetWindowFlags(window.sdl()) & SDL_WINDOW_MOUSE_GRABBED) != 0; };
  auto focus = [&](Uint8 kind) {
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT; event.window.windowID = id; event.window.event = kind;
    window.handle_event(event);
  };
  auto key = [&](SDL_Scancode code, bool down, bool repeat = false) {
    SDL_Event event{};
    event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.windowID = id; event.key.keysym.scancode = code; event.key.repeat = repeat;
    window.handle_event(event);
  };

  focus(SDL_WINDOWEVENT_FOCUS_GAINED);
  CHECK(!grabbed());  // front-end
  window.set_gameplay_active(true);
  CHECK(grabbed());  // regression: the old window never requested a grab
  window.set_mode(mode);
  CHECK(grabbed());
  CHECK(SDL_GetRelativeMouseMode() == SDL_FALSE);
  CHECK((SDL_GetWindowFlags(window.sdl()) & SDL_WINDOW_KEYBOARD_GRABBED) == 0);

  focus(SDL_WINDOWEVENT_FOCUS_LOST);
  CHECK(!grabbed());
  window.set_mode(DisplayMode::Windowed);
  CHECK(!grabbed());
  focus(SDL_WINDOWEVENT_FOCUS_GAINED);
  CHECK(grabbed());
  window.set_mode(mode);
  key(SDL_SCANCODE_LALT, true);
  CHECK(!grabbed());
  key(SDL_SCANCODE_TAB, true);
  CHECK(!grabbed());
  focus(SDL_WINDOWEVENT_FOCUS_LOST);
  key(SDL_SCANCODE_LALT, false);
  CHECK(!grabbed());
  SDL_SetModState(KMOD_LALT);
  focus(SDL_WINDOWEVENT_FOCUS_GAINED);
  CHECK(!grabbed());  // Alt is still held while switching back to the game
  SDL_SetModState(KMOD_NONE);
  key(SDL_SCANCODE_LALT, false);
  CHECK(grabbed());
  focus(SDL_WINDOWEVENT_FOCUS_GAINED);
  CHECK(grabbed());
  key(SDL_SCANCODE_RALT, true);
  key(SDL_SCANCODE_LALT, true);
  key(SDL_SCANCODE_RALT, false);
  CHECK(!grabbed());
  key(SDL_SCANCODE_LALT, false);
  CHECK(grabbed());

  window.set_gameplay_active(false);  // pause menu / paused view / mission exit
  CHECK(!grabbed());
  focus(SDL_WINDOWEVENT_FOCUS_LOST);
  focus(SDL_WINDOWEVENT_FOCUS_GAINED);
  CHECK(!grabbed());
  window.set_gameplay_active(true);  // resume
  CHECK(grabbed());
  key(SDL_SCANCODE_F9, true);
  CHECK(!window.mouse_grab_enabled() && !grabbed());
  key(SDL_SCANCODE_F9, true, true);
  CHECK(!window.mouse_grab_enabled() && !grabbed());
  focus(SDL_WINDOWEVENT_FOCUS_LOST);
  focus(SDL_WINDOWEVENT_FOCUS_GAINED);
  CHECK(!grabbed());
  key(SDL_SCANCODE_F9, false);
  key(SDL_SCANCODE_F9, true);
  CHECK(window.mouse_grab_enabled() && grabbed());
  focus(SDL_WINDOWEVENT_MINIMIZED);
  CHECK(!grabbed());
  focus(SDL_WINDOWEVENT_FOCUS_GAINED);
  CHECK(grabbed());
  SDL_Event quit{}; quit.type = SDL_QUIT;
  window.handle_event(quit);
  CHECK(!grabbed());
}

static void test_edges() {
  Input input;
  Camera camera;
  camera.set_world(10000, 10000);
  camera.set_view(1200, 900);  // camera viewport may exclude the right panel
  const SDL_Point directions[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, 1}};
  for (const auto direction : directions) {
    camera.center_on(5000, 5000);
    const float start_x = camera.x, start_y = camera.y;
    SDL_Event motion{}; motion.type = SDL_MOUSEMOTION;
    motion.motion.x = direction.x < 0 ? 0 : direction.x > 0 ? 799 : 400;
    motion.motion.y = direction.y < 0 ? 0 : direction.y > 0 ? 599 : 300;
    input.handle(motion, 2, 2);  // 800x600 window, 1600x1200 renderer (HiDPI)
    for (int frame = 0; frame < 12; ++frame) {
      input.begin_frame();  // a confined, stationary cursor sends no further motion
      const SDL_Point edge = input.edge_scroll(1600, 1200, 4);
      CHECK(edge.x == direction.x && edge.y == direction.y);
      CameraInput scroll; scroll.move_x = float(edge.x); scroll.move_y = float(edge.y);
      camera.update(scroll, 1.f / 60);
    }
    CHECK(direction.x == 0 ? camera.x == start_x : (camera.x - start_x) * direction.x > 0);
    CHECK(direction.y == 0 ? camera.y == start_y : (camera.y - start_y) * direction.y > 0);
  }
  input.mx = 1599; input.my = 600;
  input.focused = false;
  CHECK(input.edge_scroll(1600, 1200, 4).x == 0);
  input.focused = true; input.mouse_inside = false;
  CHECK(input.edge_scroll(1600, 1200, 4).x == 0);
  input.mouse_inside = true; input.alt = true;
  CHECK(input.edge_scroll(1600, 1200, 4).x == 0);
  input.alt = false; input.mx = 1600;
  CHECK(input.edge_scroll(1600, 1200, 4).x == 0);
  input.mx = 500;
  CHECK(input.edge_scroll(1600, 1200, 4).x == 0);

  input.buttons[SDL_BUTTON_LEFT] = input.pressed[SDL_BUTTON_LEFT] = true;
  input.key_down[SDL_SCANCODE_F9] = input.key_hit[SDL_SCANCODE_F9] = true;
  input.dx = 20; input.wheel = 1;
  SDL_Event lost{}; lost.type = SDL_WINDOWEVENT; lost.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
  input.handle(lost, 1, 1);
  CHECK(!input.buttons[SDL_BUTTON_LEFT] && !input.pressed[SDL_BUTTON_LEFT]);
  CHECK(!input.down(SDL_SCANCODE_F9) && !input.hit(SDL_SCANCODE_F9));
  CHECK(input.dx == 0 && input.wheel == 0);
}

int main() {
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
  for (auto mode : {DisplayMode::Windowed, DisplayMode::Borderless, DisplayMode::Fullscreen}) test_window(mode);
  test_edges();
  Options options;
  std::string error; bool usage = false;
  char program[] = "engine", flag[] = "--no-mouse-grab";
  char* arguments[] = {program, flag};
  CHECK(options.mouse_grab);
  CHECK(parse_options(2, arguments, options, error, usage));
  CHECK(!options.mouse_grab);
  SDL_Quit();
  return failures ? 1 : 0;
}
