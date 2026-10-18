// SDL2 backend: window, presentation, keyboard/mouse state, event pump.
#include "backend.h"

#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aztec_rt.h"

static SDL_Window* win;
static SDL_Renderer* ren;
static SDL_Texture* tex;
static int tex_w, tex_h, tex_fmt;
static int mode_w = 640, mode_h = 480;
static pthread_t main_thread;
static int main_thread_set;

uint8_t g_dik_down[256];
int g_mouse_dx, g_mouse_dy, g_mouse_x, g_mouse_y;
uint8_t g_mouse_btn[3];

static void ensure_sdl(void) {
  if (!main_thread_set) { main_thread = pthread_self(); main_thread_set = 1; }
  if (!SDL_WasInit(SDL_INIT_VIDEO)) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) rt_fatal("SDL_Init: %s", SDL_GetError());
  }
}

void backend_create_window(const char* title, int w, int h) {
  ensure_sdl();
  if (win) return;
  if (w < 320) w = 640;
  if (h < 200) h = 480;
  win = SDL_CreateWindow(title && *title ? title : "Aztec Empire", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                         SDL_WINDOW_RESIZABLE);
  if (!win) rt_fatal("SDL_CreateWindow: %s", SDL_GetError());
  ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  if (!ren) rt_fatal("SDL_CreateRenderer: %s", SDL_GetError());
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
}

void backend_set_mode(int w, int h, int bpp) {
  (void)bpp;
  ensure_sdl();
  if (!win) backend_create_window("Aztec Empire", w, h);
  mode_w = w; mode_h = h;
  SDL_RenderSetLogicalSize(ren, w, h);  // keeps aspect ratio on any window shape
}

void backend_show_cursor(int show) { ensure_sdl(); SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE); }

static void dump_frame(const void* pixels, int pitch, int w, int h, int bpp, const uint32_t* palette) {
  static int n;
  const char* dir = getenv("RT_DUMP_DIR");
  if (!dir) return;
  n++;
  if (!(n == 1 || n % 50 == 0)) return;
  char path[512];
  snprintf(path, sizeof path, "%s/frame_%05d.ppm", dir, n);
  FILE* f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = 0; y < h; y++) {
    const uint8_t* row = (const uint8_t*)pixels + (size_t)y * pitch;
    for (int x = 0; x < w; x++) {
      uint8_t rgb[3];
      if (bpp == 8) { uint32_t c = palette ? palette[row[x]] : row[x] * 0x010101u; rgb[0] = c >> 16; rgb[1] = c >> 8; rgb[2] = c; }
      else if (bpp == 16) { uint16_t v = ((const uint16_t*)row)[x]; rgb[0] = (v >> 11) << 3; rgb[1] = ((v >> 5) & 63) << 2; rgb[2] = (v & 31) << 3; }
      else { uint32_t v = ((const uint32_t*)row)[x]; rgb[0] = v >> 16; rgb[1] = v >> 8; rgb[2] = v; }
      fwrite(rgb, 1, 3, f);
    }
  }
  fclose(f);
}

void backend_present(const void* pixels, int pitch, int w, int h, int bpp, const uint32_t* palette) {
  dump_frame(pixels, pitch, w, h, bpp, palette);
  if (!ren) return;
  int fmt = bpp == 16 ? SDL_PIXELFORMAT_RGB565 : bpp == 15 ? SDL_PIXELFORMAT_RGB555 : SDL_PIXELFORMAT_ARGB8888;
  if (!tex || tex_w != w || tex_h != h || tex_fmt != fmt) {
    if (tex) SDL_DestroyTexture(tex);
    tex = SDL_CreateTexture(ren, fmt, SDL_TEXTUREACCESS_STREAMING, w, h);
    tex_w = w; tex_h = h; tex_fmt = fmt;
  }
  if (bpp == 8) {
    void* dst; int dp;
    SDL_LockTexture(tex, NULL, &dst, &dp);
    for (int y = 0; y < h; y++) {
      const uint8_t* s = (const uint8_t*)pixels + (size_t)y * pitch;
      uint32_t* d = (uint32_t*)((uint8_t*)dst + (size_t)y * dp);
      for (int x = 0; x < w; x++) d[x] = palette[s[x]];
    }
    SDL_UnlockTexture(tex);
  } else {
    SDL_UpdateTexture(tex, NULL, pixels, pitch);
  }
  SDL_RenderClear(ren);
  SDL_RenderCopy(ren, tex, NULL, NULL);
  SDL_RenderPresent(ren);
}

// SDL scancode -> DirectInput (DIK_*) scan code
static int dik_of(SDL_Scancode sc) {
  static const uint8_t letters[26] = {30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44};
  if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) return letters[sc - SDL_SCANCODE_A];
  if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) return 2 + (sc - SDL_SCANCODE_1);
  if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F10) return 59 + (sc - SDL_SCANCODE_F1);
  switch (sc) {
    case SDL_SCANCODE_0: return 11;
    case SDL_SCANCODE_ESCAPE: return 1;
    case SDL_SCANCODE_MINUS: return 12;
    case SDL_SCANCODE_EQUALS: return 13;
    case SDL_SCANCODE_BACKSPACE: return 14;
    case SDL_SCANCODE_TAB: return 15;
    case SDL_SCANCODE_LEFTBRACKET: return 26;
    case SDL_SCANCODE_RIGHTBRACKET: return 27;
    case SDL_SCANCODE_RETURN: return 28;
    case SDL_SCANCODE_LCTRL: return 29;
    case SDL_SCANCODE_SEMICOLON: return 39;
    case SDL_SCANCODE_APOSTROPHE: return 40;
    case SDL_SCANCODE_GRAVE: return 41;
    case SDL_SCANCODE_LSHIFT: return 42;
    case SDL_SCANCODE_BACKSLASH: return 43;
    case SDL_SCANCODE_COMMA: return 51;
    case SDL_SCANCODE_PERIOD: return 52;
    case SDL_SCANCODE_SLASH: return 53;
    case SDL_SCANCODE_RSHIFT: return 54;
    case SDL_SCANCODE_KP_MULTIPLY: return 55;
    case SDL_SCANCODE_LALT: return 56;
    case SDL_SCANCODE_SPACE: return 57;
    case SDL_SCANCODE_CAPSLOCK: return 58;
    case SDL_SCANCODE_NUMLOCKCLEAR: return 69;
    case SDL_SCANCODE_SCROLLLOCK: return 70;
    case SDL_SCANCODE_KP_7: return 71; case SDL_SCANCODE_KP_8: return 72; case SDL_SCANCODE_KP_9: return 73;
    case SDL_SCANCODE_KP_MINUS: return 74;
    case SDL_SCANCODE_KP_4: return 75; case SDL_SCANCODE_KP_5: return 76; case SDL_SCANCODE_KP_6: return 77;
    case SDL_SCANCODE_KP_PLUS: return 78;
    case SDL_SCANCODE_KP_1: return 79; case SDL_SCANCODE_KP_2: return 80; case SDL_SCANCODE_KP_3: return 81;
    case SDL_SCANCODE_KP_0: return 82; case SDL_SCANCODE_KP_PERIOD: return 83;
    case SDL_SCANCODE_F11: return 87; case SDL_SCANCODE_F12: return 88;
    case SDL_SCANCODE_KP_ENTER: return 156;
    case SDL_SCANCODE_RCTRL: return 157;
    case SDL_SCANCODE_KP_DIVIDE: return 181;
    case SDL_SCANCODE_RALT: return 184;
    case SDL_SCANCODE_HOME: return 199; case SDL_SCANCODE_UP: return 200; case SDL_SCANCODE_PAGEUP: return 201;
    case SDL_SCANCODE_LEFT: return 203; case SDL_SCANCODE_RIGHT: return 205;
    case SDL_SCANCODE_END: return 207; case SDL_SCANCODE_DOWN: return 208; case SDL_SCANCODE_PAGEDOWN: return 209;
    case SDL_SCANCODE_INSERT: return 210; case SDL_SCANCODE_DELETE: return 211;
    default: return 0;
  }
}

int backend_vk_state(int vk) {
  ensure_sdl();
  const uint8_t* k = SDL_GetKeyboardState(NULL);
  SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
  if (vk >= 'A' && vk <= 'Z') sc = (SDL_Scancode)(SDL_SCANCODE_A + vk - 'A');
  else if (vk >= '1' && vk <= '9') sc = (SDL_Scancode)(SDL_SCANCODE_1 + vk - '1');
  else switch (vk) {
    case 0x01: return g_mouse_btn[0] ? 0x8000 : 0;
    case 0x02: return g_mouse_btn[1] ? 0x8000 : 0;
    case 0x10: return (k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT]) ? 0x8000 : 0;
    case 0x11: return (k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL]) ? 0x8000 : 0;
    case 0x12: return (k[SDL_SCANCODE_LALT] || k[SDL_SCANCODE_RALT]) ? 0x8000 : 0;
    case 0x14: return k[SDL_SCANCODE_CAPSLOCK] ? 1 : 0;
    case 0x1B: sc = SDL_SCANCODE_ESCAPE; break;
    case 0x20: sc = SDL_SCANCODE_SPACE; break;
    case '0': sc = SDL_SCANCODE_0; break;
    default: return 0;
  }
  return sc != SDL_SCANCODE_UNKNOWN && k[sc] ? 0x8000 : 0;
}

static int on_main_thread(void) { return main_thread_set && pthread_equal(pthread_self(), main_thread); }

void rt_pump_events(void) {
  if (!win || !on_main_thread()) return;
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    switch (e.type) {
      case SDL_QUIT: rt_post_message(0x10 /*WM_CLOSE*/, 0, 0); break;
      case SDL_KEYDOWN: case SDL_KEYUP: {
        int d = dik_of(e.key.keysym.scancode);
        if (d) g_dik_down[d] = e.type == SDL_KEYDOWN;
        break;
      }
      case SDL_MOUSEMOTION: {
        float fx, fy;
        SDL_RenderWindowToLogical(ren, e.motion.x, e.motion.y, &fx, &fy);
        int lx = (int)fx, ly = (int)fy;
        g_mouse_dx += e.motion.xrel; g_mouse_dy += e.motion.yrel;
        g_mouse_x = lx < 0 ? 0 : lx >= mode_w ? mode_w - 1 : lx;
        g_mouse_y = ly < 0 ? 0 : ly >= mode_h ? mode_h - 1 : ly;
        break;
      }
      case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: {
        int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 1 : e.button.button == SDL_BUTTON_MIDDLE ? 2 : -1;
        if (b >= 0) g_mouse_btn[b] = e.type == SDL_MOUSEBUTTONDOWN;
        break;
      }
      default: break;
    }
  }
}
