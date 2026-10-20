#include "engine/platform/window.hpp"

#include <cstdio>

namespace aztec::engine {

Window::~Window() {
  if (ren_) SDL_DestroyRenderer(ren_);
  if (win_) SDL_DestroyWindow(win_);
}

bool Window::create(const WindowOptions& o) {
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // nearest: pixel-art sprites
  SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
  Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
  win_ = SDL_CreateWindow(o.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, o.width, o.height, flags);
  if (!win_) { std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return false; }
  if (!o.software) {
    Uint32 rf = SDL_RENDERER_ACCELERATED | (o.vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
    ren_ = SDL_CreateRenderer(win_, -1, rf);
  }
  if (!ren_) ren_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_SOFTWARE);
  if (!ren_) { std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return false; }
  SDL_RendererInfo info;
  SDL_GetRendererInfo(ren_, &info);
  vsync_ = (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
  SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
  SDL_ShowCursor(SDL_DISABLE);  // the engine draws its own cursor every frame
  set_mode(o.mode);
  return true;
}

void Window::set_mode(DisplayMode m) {
  mode_ = m;
  switch (m) {
    case DisplayMode::Windowed: SDL_SetWindowFullscreen(win_, 0); break;
    case DisplayMode::Borderless: SDL_SetWindowFullscreen(win_, SDL_WINDOW_FULLSCREEN_DESKTOP); break;
    case DisplayMode::Fullscreen: SDL_SetWindowFullscreen(win_, SDL_WINDOW_FULLSCREEN); break;
  }
}

void Window::cycle_borderless() {
  set_mode(mode_ == DisplayMode::Windowed ? DisplayMode::Borderless : DisplayMode::Windowed);
}
void Window::toggle_fullscreen() {
  set_mode(mode_ == DisplayMode::Fullscreen ? DisplayMode::Windowed : DisplayMode::Fullscreen);
}

void Window::mouse_scale(float& sx, float& sy) const {
  int ww, wh, ow, oh;
  SDL_GetWindowSize(win_, &ww, &wh);
  SDL_GetRendererOutputSize(ren_, &ow, &oh);
  sx = ww > 0 ? float(ow) / ww : 1.f;
  sy = wh > 0 ? float(oh) / wh : 1.f;
}

}  // namespace aztec::engine
