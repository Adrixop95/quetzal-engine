// SDL2 backend: window, presentation, keyboard/mouse state, event pump.
#include "backend.h"

#include <SDL2/SDL.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aztec_rt.h"

static SDL_Window* win;
static SDL_Renderer* ren;
static SDL_Texture* tex;
static int tex_w, tex_h;
static int mode_w = 640, mode_h = 480;
static pthread_t main_thread;
static int main_thread_set;

uint8_t g_dik_down[256];
int g_mouse_dx, g_mouse_dy, g_mouse_x, g_mouse_y;
uint8_t g_mouse_btn[3];

void backend_start_script(void);
static void ensure_sdl(void) {
  if (!main_thread_set) { main_thread = pthread_self(); main_thread_set = 1; }
  if (!SDL_WasInit(SDL_INIT_VIDEO)) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) rt_fatal("SDL_Init: %s", SDL_GetError());
  }
}

void backend_create_window(const char* title, int w, int h) {
  ensure_sdl();
  if (win) return;
  backend_start_script();
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

static volatile int shot_pending;
static volatile int shot_index;
static void dump_frame(const void* pixels, int pitch, int w, int h, int bpp, const uint32_t* palette) {
  static int n;
  const char* dir = getenv("RT_DUMP_DIR");
  if (!dir) return;
  n++;
  char path[512];
  if (shot_pending) { snprintf(path, sizeof path, "%s/shot_%02d.ppm", dir, shot_index); shot_pending = 0; }
  else if (n == 1 || n % 500 == 0) snprintf(path, sizeof path, "%s/frame_%05d.ppm", dir, n);
  else return;
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

// The SDL renderer (OpenGL) must only be used from the thread that created it. Game threads convert the
// frame into a staging buffer; the main thread (event pump) uploads and presents it.
static uint32_t* stage;
static int stage_w, stage_h;
static volatile int stage_dirty;
static pthread_mutex_t stage_lock = PTHREAD_MUTEX_INITIALIZER;
static int on_main_thread(void);

static void present_stage(void) {
  pthread_mutex_lock(&stage_lock);
  if (stage_dirty && ren && stage) {
    if (!tex || tex_w != stage_w || tex_h != stage_h) {
      if (tex) SDL_DestroyTexture(tex);
      tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, stage_w, stage_h);
      if (!tex) rt_log("SDL_CreateTexture failed: %s", SDL_GetError());
      tex_w = stage_w; tex_h = stage_h;
    }
    if (tex) {
      static int lw, lh;
      if (lw != stage_w || lh != stage_h) { SDL_RenderSetLogicalSize(ren, stage_w, stage_h); lw = stage_w; lh = stage_h; }
      SDL_UpdateTexture(tex, NULL, stage, stage_w * 4);
      SDL_RenderClear(ren);
      SDL_RenderCopy(ren, tex, NULL, NULL);
      SDL_RenderPresent(ren);
    }
    stage_dirty = 0;
  }
  pthread_mutex_unlock(&stage_lock);
}

// Widescreen: the menus are 800x600 pictures drawn in the corner of a larger surface. When everything outside
// that corner is black (sparse sample), present only the corner so the menu fills the window.
static int outside_800x600_is_black(const void* pixels, int pitch, int w, int h, int bpp) {
  if (bpp != 16 || (w <= 800 && h <= 600)) return 0;
  const uint8_t* base = (const uint8_t*)pixels;
  for (int y = 0; y < h; y += (y < 600 ? 37 : 9))
    for (int x = (y < 600 ? 800 : 0); x < w; x += 7) {
      if (y >= 600 && x >= 800 + 0 && x > 0 && 0) continue;
      if (*(const uint16_t*)(base + (size_t)y * pitch + (size_t)x * 2)) return 0;
    }
  return 1;
}

void backend_present(const void* pixels, int pitch, int w, int h, int bpp, const uint32_t* palette) {
  if (outside_800x600_is_black(pixels, pitch, w, h, bpp)) { w = 800; h = 600; }   // pitch stays, rows are cropped
  dump_frame(pixels, pitch, w, h, bpp, palette);
  if (!ren) return;
  pthread_mutex_lock(&stage_lock);
  if (!stage || stage_w != w || stage_h != h) { free(stage); stage = malloc((size_t)w * h * 4); stage_w = w; stage_h = h; }
  for (int y = 0; y < h; y++) {
    const uint8_t* s = (const uint8_t*)pixels + (size_t)y * pitch;
    uint32_t* d = stage + (size_t)y * w;
    if (bpp == 8) {
      for (int x = 0; x < w; x++) d[x] = palette ? palette[s[x]] : 0xFF000000u | s[x] * 0x010101u;
    } else if (bpp == 16) {
      const uint16_t* p16 = (const uint16_t*)s;
      for (int x = 0; x < w; x++) {
        uint32_t v = p16[x];
        uint32_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
        d[x] = 0xFF000000u | ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
      }
    } else if (bpp == 15) {
      const uint16_t* p16 = (const uint16_t*)s;
      for (int x = 0; x < w; x++) {
        uint32_t v = p16[x];
        uint32_t r = (v >> 10) & 31, g = (v >> 5) & 31, b = v & 31;
        d[x] = 0xFF000000u | ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
      }
    } else {
      memcpy(d, s, (size_t)w * 4);
    }
  }
  stage_dirty = 1;
  pthread_mutex_unlock(&stage_lock);
  if (on_main_thread()) present_stage();
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

extern void rt_dinput_poll(void);
void rt_pump_events(void) {
  rt_dinput_poll();
  if (!win || !on_main_thread()) return;
  present_stage();
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
  rt_dinput_poll();
}

// ---- scripted input for automated tests: RT_SCRIPT=file with lines
//   wait MS | move X Y | click X Y | rclick X Y | key DIK | keydown DIK | keyup DIK | shot
extern int rt_time_scale;
static void gdelay(unsigned ms) { SDL_Delay(ms / (unsigned)rt_time_scale + 1); }
static void* script_thread(void* arg) {
  FILE* f = fopen((const char*)arg, "r");
  if (!f) return NULL;
  char line[256];
  while (fgets(line, sizeof line, f)) {
    char cmd[32] = "";
    int x = 0, y = 0;
    sscanf(line, "%31s %d %d", cmd, &x, &y);
    if (!strcmp(cmd, "wait")) gdelay((Uint32)x);
    else if (!strcmp(cmd, "move") || !strcmp(cmd, "click") || !strcmp(cmd, "rclick")) {
      g_mouse_dx -= 3000; g_mouse_dy -= 3000; gdelay(60);   // pin to the corner, then move relative
      for (int i = 0; i < 4; i++) { g_mouse_dx += x / 4; g_mouse_dy += y / 4; gdelay(30); }
      g_mouse_dx += x - 4 * (x / 4); g_mouse_dy += y - 4 * (y / 4);
      g_mouse_x = x; g_mouse_y = y;
      gdelay(120);
      if (strcmp(cmd, "move")) {
        int b = !strcmp(cmd, "rclick") ? 1 : 0;
        g_mouse_btn[b] = 1; gdelay(120); g_mouse_btn[b] = 0; gdelay(120);
      }
    } else if (!strcmp(cmd, "down")) g_mouse_btn[0] = 1;
    else if (!strcmp(cmd, "up")) g_mouse_btn[0] = 0;
    else if (!strcmp(cmd, "key")) { g_dik_down[x & 255] = 1; gdelay(120); g_dik_down[x & 255] = 0; gdelay(80); }
    else if (!strcmp(cmd, "keydown")) g_dik_down[x & 255] = 1;
    else if (!strcmp(cmd, "keyup")) g_dik_down[x & 255] = 0;
    else if (!strcmp(cmd, "quit")) { fflush(NULL); _exit(0); }
    else if (!strcmp(cmd, "shot")) { shot_index++; shot_pending = 1; gdelay(400); }
  }
  fclose(f);
  return NULL;
}
void backend_start_script(void) {
  const char* path = getenv("RT_SCRIPT");
  if (!path) return;
  static char saved[512];
  snprintf(saved, sizeof saved, "%s", path);
  pthread_t t;
  pthread_create(&t, NULL, script_thread, saved);
}
