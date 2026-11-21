#include "engine/platform/window.hpp"

#include <cstdio>
#include <array>
#include <cmath>
#include <algorithm>

namespace aztec::engine {

Window::~Window() {
  if (gamma_texture_) SDL_DestroyTexture(gamma_texture_);
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
  focused_ = (SDL_GetWindowFlags(win_) & SDL_WINDOW_INPUT_FOCUS) != 0;
  return set_mode(o.mode);
}

bool Window::set_size(int width, int height) {
  if (width < 160 || height < 120) return false;
  if (mode_ == DisplayMode::Fullscreen) {
    SDL_DisplayMode display{}; display.w = width; display.h = height;
    if (SDL_SetWindowDisplayMode(win_, &display) != 0) return false;
  }
  SDL_SetWindowSize(win_, width, height);
  return true;
}

uint8_t Window::gamma_channel(uint8_t value, double gamma) {
  if (!std::isfinite(gamma) || gamma <= 0) return value;
  return uint8_t(std::clamp(std::lround(255 * std::pow(value / 255.0, 1.0 / gamma)), 0l, 255l));
}

bool Window::apply_gamma(double gamma) {
  if (gamma == 1.0) return true;
  if (!std::isfinite(gamma) || gamma < 0.5 || gamma > 2.5) return false;
  int w, h; output_size(w, h);
  if (w <= 0 || h <= 0) return false;
  if (!gamma_texture_ || w != gamma_width_ || h != gamma_height_) {
    if (gamma_texture_) SDL_DestroyTexture(gamma_texture_);
    gamma_texture_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!gamma_texture_) return false;
    gamma_width_ = w; gamma_height_ = h;
    gamma_pixels_.resize(size_t(w) * h * 4);
    SDL_SetTextureBlendMode(gamma_texture_, SDL_BLENDMODE_NONE);
  }
  if (SDL_RenderReadPixels(ren_, nullptr, SDL_PIXELFORMAT_RGBA32, gamma_pixels_.data(), w * 4) != 0) return false;
  std::array<uint8_t, 256> lut{};
  for (int i = 0; i < 256; ++i) lut[i] = gamma_channel(uint8_t(i), gamma);
  for (size_t i = 0; i < gamma_pixels_.size(); i += 4)
    for (int ch = 0; ch < 3; ++ch) gamma_pixels_[i + ch] = lut[gamma_pixels_[i + ch]];
  return SDL_UpdateTexture(gamma_texture_, nullptr, gamma_pixels_.data(), w * 4) == 0 &&
      SDL_RenderCopy(ren_, gamma_texture_, nullptr, nullptr) == 0;
}

bool Window::set_mode(DisplayMode m) {
  Uint32 flags = 0;
  if (m == DisplayMode::Borderless) flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
  if (m == DisplayMode::Fullscreen) flags = SDL_WINDOW_FULLSCREEN;
  if (SDL_SetWindowFullscreen(win_, flags) != 0) {
    std::fprintf(stderr, "SDL_SetWindowFullscreen: %s\n", SDL_GetError());
    return false;
  }
  mode_ = m;
  apply_mouse_grab();
  return true;
}

void Window::apply_mouse_grab() {
  if (!win_) return;
  // Keep absolute coordinates for edge scrolling and leave system shortcuts available.
  SDL_SetWindowKeyboardGrab(win_, SDL_FALSE);
  SDL_SetWindowMouseGrab(win_, mouse_grab_enabled_ && gameplay_active_ && focused_ &&
      !left_alt_ && !right_alt_ ? SDL_TRUE : SDL_FALSE);
}

void Window::set_gameplay_active(bool active) {
  gameplay_active_ = active;
  apply_mouse_grab();
}

void Window::set_mouse_grab_enabled(bool enabled) {
  mouse_grab_enabled_ = enabled;
  apply_mouse_grab();
}

void Window::handle_event(const SDL_Event& e) {
  if (e.type == SDL_WINDOWEVENT && e.window.windowID == SDL_GetWindowID(win_)) {
    if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
      focused_ = true;
      const SDL_Keymod mods = SDL_GetModState();
      left_alt_ = (mods & KMOD_LALT) != 0;
      right_alt_ = (mods & KMOD_RALT) != 0;
    }
    if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST || e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
      focused_ = false;
      left_alt_ = right_alt_ = false;
    }
  }
  if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && e.key.windowID == SDL_GetWindowID(win_)) {
    const bool down = e.type == SDL_KEYDOWN;
    if (e.key.keysym.scancode == SDL_SCANCODE_LALT) left_alt_ = down;
    if (e.key.keysym.scancode == SDL_SCANCODE_RALT) right_alt_ = down;
    if (down && !e.key.repeat && focused_ && e.key.keysym.scancode == SDL_SCANCODE_F9 &&
        (e.key.keysym.mod & KMOD_CTRL))
      mouse_grab_enabled_ = !mouse_grab_enabled_;
  }
  if (e.type == SDL_QUIT) gameplay_active_ = false;
  apply_mouse_grab();
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
