// SDL2 backend: window, presentation, keyboard/mouse state, event pump.
#include "backend.h"

#include <SDL2/SDL.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aztec_rt.h"
#include "settings.h"
#include "dbg.h"

extern int overlay_is_open(void);
extern void overlay_toggle(void);
extern void overlay_event(const SDL_Event* e, SDL_Renderer* ren);
extern void overlay_draw(SDL_Renderer* ren, int lw, int lh);
extern void overlay_request(int action);
static int tex_reset;
static void mouse_resync(void);
static int cur_lw = 800, cur_lh = 600;   // logical size currently used for presentation

static SDL_Window* win;
static SDL_Renderer* ren;
static SDL_Texture* tex;
static int tex_w, tex_h, tex_is16;
static int mode_w = 640, mode_h = 480;
static pthread_t main_thread;
static int main_thread_set;

uint8_t g_dik_down[256];
int g_mouse_dx, g_mouse_dy, g_mouse_x, g_mouse_y;
uint8_t g_mouse_btn[3];

void backend_start_script(void);
static void backend_dump(FILE* f);
static void ensure_sdl(void) {
  static int dumper_registered;
  if (!dumper_registered) { dumper_registered = 1; dbg_register_dumper("backend (window/render/mouse)", backend_dump); }
  if (!main_thread_set) { main_thread = pthread_self(); main_thread_set = 1; DBG("sdl", "main (render/event) thread registered"); }
  if (!SDL_WasInit(SDL_INIT_VIDEO)) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) rt_fatal("SDL_Init: %s", SDL_GetError());
    SDL_version cv, lv;
    SDL_VERSION(&cv);
    SDL_GetVersion(&lv);
    DBG("sdl", "SDL compiled=%d.%d.%d linked=%d.%d.%d revision=%s", cv.major, cv.minor, cv.patch, lv.major, lv.minor, lv.patch, SDL_GetRevision());
    DBG("sdl", "video driver=%s, %d displays", SDL_GetCurrentVideoDriver(), SDL_GetNumVideoDisplays());
    for (int d = 0; d < SDL_GetNumVideoDisplays(); d++) {
      SDL_Rect b;
      SDL_DisplayMode dm;
      float ddpi = 0, hdpi = 0, vdpi = 0;
      SDL_GetDisplayBounds(d, &b);
      SDL_GetDesktopDisplayMode(d, &dm);
      SDL_GetDisplayDPI(d, &ddpi, &hdpi, &vdpi);
      DBG("sdl", "display %d '%s': bounds=%d,%d %dx%d desktop mode=%dx%d@%dHz dpi=%.1f/%.1f/%.1f", d, SDL_GetDisplayName(d), b.x, b.y, b.w, b.h, dm.w, dm.h, dm.refresh_rate, ddpi, hdpi, vdpi);
    }
    for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
      SDL_RendererInfo ri;
      if (SDL_GetRenderDriverInfo(i, &ri) == 0) DBG("sdl", "render driver %d: %s flags=%#x max_tex=%dx%d", i, ri.name, ri.flags, ri.max_texture_width, ri.max_texture_height);
    }
  }
}

typedef struct { const char* title; int w, h; } WinReq;
static void create_window_impl(void* arg);
static void run_on_main(void (*fn)(void*), void* arg);
void backend_create_window(const char* title, int w, int h) {
  WinReq r = {title, w, h};
  run_on_main(create_window_impl, &r);
}
static void create_window_impl(void* arg) {
  const char* title = ((WinReq*)arg)->title; int w = ((WinReq*)arg)->w, h = ((WinReq*)arg)->h;
  ensure_sdl();
  if (win) return;
  backend_start_script();
  if (w < 320) w = 640;
  if (h < 200) h = 480;
  if (g_settings.res_w > 0 && g_settings.window_mode != 2) { w = g_settings.res_w; h = g_settings.res_h; }
  Uint32 flags = SDL_WINDOW_RESIZABLE;
  if (g_settings.window_mode == 1) flags |= SDL_WINDOW_BORDERLESS;
  if (g_settings.window_mode == 2) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  DBG("window", "create: title='%s' requested %dx%d flags=%#x (settings: res=%dx%d mode=%d smooth=%d video=%d)", title ? title : "", w, h, flags, g_settings.res_w, g_settings.res_h, g_settings.window_mode, g_settings.smooth, g_settings.video);
  win = SDL_CreateWindow(title && *title ? title : "Aztec Empire", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
  if (!win) rt_fatal("SDL_CreateWindow: %s", SDL_GetError());
  ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | (getenv("RT_NO_VSYNC") ? 0 : SDL_RENDERER_PRESENTVSYNC));
  if (!ren) { DBG("window", "accelerated renderer failed (%s), falling back to software", SDL_GetError()); ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE); }
  if (!ren) rt_fatal("SDL_CreateRenderer: %s", SDL_GetError());
  {
    int ww, wh, ow, oh, wx, wy;
    SDL_RendererInfo ri;
    SDL_GetWindowSize(win, &ww, &wh);
    SDL_GetWindowPosition(win, &wx, &wy);
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    SDL_GetRendererInfo(ren, &ri);
    DBG("window", "created: window size=%dx%d pos=%d,%d renderer output=%dx%d (scale %.2f) display index=%d flags=%#x", ww, wh, wx, wy, ow, oh, ww ? (double)ow / ww : 0.0, SDL_GetWindowDisplayIndex(win), SDL_GetWindowFlags(win));
    DBG("window", "renderer '%s' flags=%#x max texture=%dx%d", ri.name, ri.flags, ri.max_texture_width, ri.max_texture_height);
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, g_settings.smooth ? "linear" : "nearest");
  if (!getenv("RT_NO_GRAB")) { int rc = SDL_SetRelativeMouseMode(SDL_TRUE); DBG("mouse", "relative mouse mode ON (initial) rc=%d %s", rc, rc ? SDL_GetError() : ""); }
  if (getenv("RT_MENU")) overlay_toggle();
}

void backend_set_mode(int w, int h, int bpp) {
  ensure_sdl();
  if (!win) backend_create_window("Aztec Empire", w, h);
  mode_w = w; mode_h = h;
  cur_lw = w; cur_lh = h;   // the render thread applies the logical size when the next frame arrives
  DBG("render", "set_mode %dx%d bpp=%d -> logical size %dx%d", w, h, bpp, cur_lw, cur_lh);
}

static int game_cursor_visible = 1;   // what the game asked for (informational: we always draw the game's own cursor)
void backend_show_cursor(int show) {
  ensure_sdl();
  game_cursor_visible = show;
  DBG("cursor", "game requested system cursor %s (ignored: the pointer is locked and the game draws its own cursor)", show ? "visible" : "hidden");
}
void backend_overlay_cursor(int open) {
  // Pointer stays locked (relative mode) with a virtual cursor; the overlay draws its own arrow, so nothing to switch.
  DBG("cursor", "overlay %s: virtual cursor continues", open ? "opened" : "closed");
  if (!open) mouse_resync();
}

static volatile unsigned guest_presents;
static volatile int shot_pending;
static volatile int shot_index;
static void dump_frame(const void* pixels, int pitch, int w, int h, int bpp, const uint32_t* palette) {
  static int n;
  const char* dir = getenv("RT_DUMP_DIR");
  if (!dir) return;
  n++;
  char path[512];
  if (n == 1 || n % 500 == 0) snprintf(path, sizeof path, "%s/frame_%05d.ppm", dir, n);
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
static uint32_t* stage;       // ARGB staging buffer (8/15/32 bpp sources, or the RGB565 fallback)
static uint16_t* stage16;     // raw RGB565 frame, uploaded as-is when the renderer supports RGB565 textures
static int stage_is16;
static int rgb565_ok = -1;    // -1 unknown, 1 usable, 0 renderer refused RGB565
static int stage_w, stage_h;
static volatile int stage_dirty;
static pthread_mutex_t stage_lock = PTHREAD_MUTEX_INITIALIZER;

// Window coordinates (as delivered in mouse events) -> picture coordinates, accounting for the aspect-preserving
// letterbox. Done by hand because SDL's renderer helper misbehaves on some Wayland/sdl2-compat setups.
void backend_window_to_logical(int wx, int wy, int* lx, int* ly) {
  int ww = 1, wh = 1;
  if (win) SDL_GetWindowSize(win, &ww, &wh);
  if (ww < 1) ww = 1;
  if (wh < 1) wh = 1;
  double sc = (double)ww / cur_lw < (double)wh / cur_lh ? (double)ww / cur_lw : (double)wh / cur_lh;
  double ox = (ww - cur_lw * sc) / 2, oy = (wh - cur_lh * sc) / 2;
  int x = (int)((wx - ox) / sc), y = (int)((wy - oy) / sc);
  *lx = x < 0 ? 0 : x >= cur_lw ? cur_lw - 1 : x;
  *ly = y < 0 ? 0 : y >= cur_lh ? cur_lh - 1 : y;
}

// ---- virtual cursor ---------------------------------------------------------------------------------------
// The OS pointer is locked to the window (SDL relative mode, no escaping on any compositor). We integrate the
// relative motion into a virtual cursor expressed in picture coordinates, so pointer speed is independent of the
// window size / letterboxing and can be tuned in the F9 menu.
static double vcx = 400, vcy = 300;
static double window_scale(void) {
  int ww = 1, wh = 1;
  if (win) SDL_GetWindowSize(win, &ww, &wh);
  double sc = (double)ww / cur_lw < (double)wh / cur_lh ? (double)ww / cur_lw : (double)wh / cur_lh;
  return sc > 0.01 ? sc : 1.0;
}
static void vcursor_move(int dx, int dy) {
  double k = g_settings.mouse_speed / 100.0 / window_scale();
  vcx += dx * k; vcy += dy * k;
  if (vcx < 0) vcx = 0;
  if (vcy < 0) vcy = 0;
  if (vcx > cur_lw - 1) vcx = cur_lw - 1;
  if (vcy > cur_lh - 1) vcy = cur_lh - 1;
}
void backend_virtual_cursor(int* x, int* y) { *x = (int)vcx; *y = (int)vcy; }
void backend_set_virtual_cursor(int x, int y) { vcx = x; vcy = y; }

// The game keeps its own cursor: position = previous position (guest 0x4c5468/0x4c546c) + the delta read from
// DirectInput, clamped to the screen size (0x4dd368/0x4dd36c). Instead of faking deltas (which depend on the game's
// internal state) we write the wanted absolute position into those variables and poke the device event so the game
// processes it. This runs on the game's main thread, the same thread that evaluates the cursor, so it cannot race.
volatile int g_mouse_poke;
static int tgt_x, tgt_y, tgt_valid;
static void mouse_follow(void) {
  if (!tgt_valid) return;
  tgt_valid = 0;
  uint32_t sw = GU32(0x4dd368), sh = GU32(0x4dd36c);
  int x = tgt_x, y = tgt_y;
  if (sw && x >= (int)sw) x = (int)sw - 1;
  if (sh && y >= (int)sh) y = (int)sh - 1;
  GU32(0x4c5468) = (uint32_t)x; GU32(0x4c546c) = (uint32_t)y;
  GU32(0x4c4f50) = (uint32_t)x; GU32(0x4c4f54) = (uint32_t)y;
  g_mouse_poke = 1;
  DBG_EVERY(25, "mouse", "follow: placed game cursor at (%d,%d) [target (%d,%d), game screen %ux%u], poke event", x, y, tgt_x, tgt_y, sw, sh);
}
static void mouse_resync(void) { tgt_valid = 1; }   // re-send the last known position (after focus / overlay changes)

// ---- software cursor: drawn here every display frame from the virtual cursor position -------------------------
static struct { uint32_t px[96 * 96]; int w, h, ox, oy, valid; unsigned ver; } cur_img;
static pthread_mutex_t cur_lock = PTHREAD_MUTEX_INITIALIZER;
static SDL_Texture* cur_tex;
static unsigned cur_tex_ver;
void backend_cursor_set(const uint32_t* argb, int w, int h, int off_x, int off_y) {
  if (w <= 0 || h <= 0 || w > 96 || h > 96) return;
  pthread_mutex_lock(&cur_lock);
  if (!cur_img.valid || cur_img.w != w || cur_img.h != h || memcmp(cur_img.px, argb, (size_t)w * h * 4)) {
    memcpy(cur_img.px, argb, (size_t)w * h * 4);
    cur_img.w = w; cur_img.h = h; cur_img.ver++;
  }
  cur_img.ox = off_x; cur_img.oy = off_y; cur_img.valid = 1;
  pthread_mutex_unlock(&cur_lock);
}
static void draw_game_cursor(void) {
  if (!g_mem || overlay_is_open() || getenv("RT_NO_SWCURSOR")) return;
  if (!GU8(0x4dd32d) || !GU8(0x4dd32e) || !GU32(0x5159c4)) return;   // the game's own "cursor active" state
  pthread_mutex_lock(&cur_lock);
  if (cur_img.valid) {
    if (!cur_tex || cur_tex_ver != cur_img.ver) {
      if (cur_tex) SDL_DestroyTexture(cur_tex);
      cur_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, cur_img.w, cur_img.h);
      if (cur_tex) { SDL_UpdateTexture(cur_tex, NULL, cur_img.px, cur_img.w * 4); SDL_SetTextureBlendMode(cur_tex, SDL_BLENDMODE_BLEND); }
      cur_tex_ver = cur_img.ver;
    }
    if (cur_tex) {
      SDL_Rect dst = {(int)vcx + cur_img.ox, (int)vcy + cur_img.oy, cur_img.w, cur_img.h};
      DBG_EVERY(120, "cursor", "sw cursor at (%d,%d) v=(%.1f,%.1f) off=(%d,%d) game=(%d,%d) logical=%dx%d", dst.x, dst.y, vcx, vcy, cur_img.ox, cur_img.oy, (int)GU32(0x4c4f50), (int)GU32(0x4c4f54), cur_lw, cur_lh);
      SDL_RenderCopy(ren, cur_tex, NULL, &dst);
    }
  }
  pthread_mutex_unlock(&cur_lock);
}

static int on_main_thread(void);
static void consume_guest_request(void);
static void sdl_pump(void);

static void present_stage(void) {
  consume_guest_request();
  pthread_mutex_lock(&stage_lock);
  if (ren && (stage || stage16)) {
    int upload = stage_dirty;
    int want16 = stage_is16 && rgb565_ok != 0;
    if (!tex || tex_w != stage_w || tex_h != stage_h || tex_reset || tex_is16 != want16) {
      tex_reset = 0;
      if (tex) SDL_DestroyTexture(tex);
      tex = SDL_CreateTexture(ren, want16 ? SDL_PIXELFORMAT_RGB565 : SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, stage_w, stage_h);
      if (!tex && want16) {   // renderer cannot do RGB565: remember it and convert on this (main) thread instead
        rgb565_ok = 0; want16 = 0;
        DBG("render", "RGB565 texture refused (%s): falling back to ARGB conversion", SDL_GetError());
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, stage_w, stage_h);
      } else if (tex && want16) rgb565_ok = 1;
      tex_is16 = want16;
      if (!tex) rt_log("SDL_CreateTexture %dx%d failed: %s", stage_w, stage_h, SDL_GetError());
      DBG("render", "texture (re)created %dx%d -> %s (smooth=%d)", stage_w, stage_h, tex ? "ok" : "FAILED", g_settings.smooth);
      tex_w = stage_w; tex_h = stage_h; upload = 1;
    }
    if (tex) {
      static int lw, lh;
      if (lw != stage_w || lh != stage_h) {
        int rc = SDL_RenderSetLogicalSize(ren, stage_w, stage_h);
        int ww, wh;
        SDL_GetWindowSize(win, &ww, &wh);
        DBG("render", "logical size %dx%d -> %dx%d (rc=%d); window=%dx%d", lw, lh, stage_w, stage_h, rc, ww, wh);
        lw = stage_w; lh = stage_h; cur_lw = stage_w; cur_lh = stage_h;
      }
      uint64_t t_start = SDL_GetPerformanceCounter();
      int urc = 0;
      if (!upload) {}
      else if (tex_is16) urc = SDL_UpdateTexture(tex, NULL, stage16, stage_w * 2);
      else if (stage_is16) {   // RGB565 frame but ARGB texture: convert here, off the game's threads
        static uint32_t* conv; static size_t conv_n;
        size_t n = (size_t)stage_w * stage_h;
        if (conv_n < n) { free(conv); conv = malloc(n * 4); conv_n = n; }
        for (size_t i = 0; i < n; i++) {
          uint32_t v = stage16[i], r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
          conv[i] = 0xFF000000u | ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
        }
        urc = SDL_UpdateTexture(tex, NULL, conv, stage_w * 4);
      } else urc = SDL_UpdateTexture(tex, NULL, stage, stage_w * 4);
      if (urc != 0) DBG_EVERY(100, "render", "SDL_UpdateTexture failed: %s", SDL_GetError());
      SDL_RenderClear(ren);
      if (SDL_RenderCopy(ren, tex, NULL, NULL) != 0) DBG_EVERY(100, "render", "SDL_RenderCopy failed: %s", SDL_GetError());
      sdl_pump();          // freshest pointer position right before the cursor is drawn (lowest latency)
      draw_game_cursor();
      overlay_draw(ren, stage_w, stage_h);
      if (shot_pending) {   // final composed window image (game + overlay)
        shot_pending = 0;
        const char* dir = getenv("RT_DUMP_DIR");
        int ow, oh;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        uint32_t* rb = malloc((size_t)ow * oh * 4);
        if (dir && rb && SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, rb, ow * 4) == 0) {
          char path[512];
          snprintf(path, sizeof path, "%s/shot_%02d.ppm", dir, shot_index);
          FILE* f = fopen(path, "wb");
          if (f) {
            fprintf(f, "P6\n%d %d\n255\n", ow, oh);
            for (int i = 0; i < ow * oh; i++) { uint8_t rgb[3] = {rb[i] >> 16, rb[i] >> 8, rb[i]}; fwrite(rgb, 1, 3, f); }
            fclose(f);
          }
        }
        free(rb);
      }
      SDL_RenderPresent(ren);
      if (dbg_enabled()) {   // per-second presentation statistics
        static unsigned frames;
        static double acc_ms;
        static uint64_t last_report;
        uint64_t now = SDL_GetTicks64();
        frames++;
        acc_ms += (double)(SDL_GetPerformanceCounter() - t_start) * 1000.0 / (double)SDL_GetPerformanceFrequency();
        if (now - last_report >= 1000) {
          DBG("render", "guest present calls=%u; presented %u frames in last %llu ms, avg present cost %.2f ms, stage=%dx%d logical=%dx%d overlay=%d", guest_presents, frames, (unsigned long long)(now - last_report), frames ? acc_ms / frames : 0.0, stage_w, stage_h, cur_lw, cur_lh, overlay_is_open());
          if (g_mem) {
            int ww, wh, mxp, myp;
            SDL_GetWindowSize(win, &ww, &wh);
            SDL_GetMouseState(&mxp, &myp);
            DBG("state", "window=%dx%d grab=%d focus=%d | system mouse=(%d,%d) -> logical target=(%d,%d) | game cursor cur=(%d,%d) prev=(%d,%d) game screen=%ux%u | btn=%d%d%d",
                ww, wh, SDL_GetWindowGrab(win), SDL_GetKeyboardFocus() == win, mxp, myp, tgt_x, tgt_y, (int)GU32(0x4c4f50), (int)GU32(0x4c4f54), (int)GU32(0x4c5468), (int)GU32(0x4c546c), GU32(0x4dd368), GU32(0x4dd36c), g_mouse_btn[0], g_mouse_btn[1], g_mouse_btn[2]);
          }
          if (g_mem)   // lockstep / network state of the game (guest globals)
            DBG("netstate", "session=%02x frame=%u hostid=%08x flags 396..399=%02x%02x%02x%02x slot0..3 active=%02x %02x %02x %02x pause=%02x 515570=%08x",
                GU8(0x4cc394), GU32(0x4c5730), GU32(0x4cc38c), GU8(0x4cc396), GU8(0x4cc395), GU8(0x4cc398), GU8(0x4cc399),
                GU8(0x4cc248 ? GU32(0x4cc248) + 0x10 : 0), GU8(GU32(0x4cc248) + 0x40), GU8(GU32(0x4cc248) + 0x70), GU8(GU32(0x4cc248) + 0xa0), GU8(0x515400), GU32(0x515570));
          guest_presents = 0; frames = 0; acc_ms = 0; last_report = now;
        }
      }
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

static void backend_present_copy_impl(const void* pixels, int pitch, int w, int h, int bpp, const uint32_t* palette);
void backend_present(const void* pixels, int pitch, int w, int h, int bpp, const uint32_t* palette) {
  dump_frame(pixels, pitch, w, h, bpp, palette);
  if (ren) backend_present_copy_impl(pixels, pitch, w, h, bpp, palette);
}
static void backend_present_copy_impl(const void* pixels, int pitch, int w, int h, int bpp, const uint32_t* palette) {
  {
    static int last_crop = -1, last_w, last_h;
    int crop = outside_800x600_is_black(pixels, pitch, w, h, bpp);
    if (crop != last_crop || w != last_w || h != last_h) { DBG("render", "game frame %dx%d bpp=%d: menu-crop=%d (was %d)", w, h, bpp, crop, last_crop); last_crop = crop; last_w = w; last_h = h; }
    if (crop) { w = 800; h = 600; }   // pitch stays, rows are cropped
  }
  if (!ren) return;
  if (bpp == 16) {   // fast path: copy rows as they are, no per-pixel work on the game's thread
    pthread_mutex_lock(&stage_lock);
    if (!stage16 || stage_w != w || stage_h != h || !stage_is16) { free(stage16); stage16 = malloc((size_t)w * h * 2); }
    stage_w = w; stage_h = h; stage_is16 = 1;
    for (int y = 0; y < h; y++) memcpy(stage16 + (size_t)y * w, (const uint8_t*)pixels + (size_t)y * pitch, (size_t)w * 2);
    stage_dirty = 1;
    pthread_mutex_unlock(&stage_lock);
    return;
  }
  pthread_mutex_lock(&stage_lock);
  stage_is16 = 0;
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
}

// DirectDraw primary surface presentation: the game draws straight into its primary surface (like it did on real
// hardware), so we only note where it lives; the render thread copies it when it builds the next frame.
typedef struct { const void* px; int pitch, w, h, bpp; uint32_t pal[256]; int has_pal; } GReq;
static GReq greq;
static volatile int greq_dirty;
static pthread_mutex_t greq_lock = PTHREAD_MUTEX_INITIALIZER;
void backend_present_guest(const void* pixels, int pitch, int w, int h, int bpp, const uint32_t* palette) {
  guest_presents++;
  dump_frame(pixels, pitch, w, h, bpp, palette);
  pthread_mutex_lock(&greq_lock);
  greq.px = pixels; greq.pitch = pitch; greq.w = w; greq.h = h; greq.bpp = bpp;
  greq.has_pal = palette != NULL;
  if (palette) memcpy(greq.pal, palette, sizeof greq.pal);
  greq_dirty = 1;
  pthread_mutex_unlock(&greq_lock);
}
static void consume_guest_request(void) {
  if (!greq_dirty) return;
  pthread_mutex_lock(&greq_lock);
  GReq r = greq;
  greq_dirty = 0;
  pthread_mutex_unlock(&greq_lock);
  backend_present_copy_impl(r.px, r.pitch, r.w, r.h, r.bpp, r.has_pal ? r.pal : NULL);
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
static const char* win_event_name(int ev) {
  switch (ev) {
    case SDL_WINDOWEVENT_SHOWN: return "shown"; case SDL_WINDOWEVENT_HIDDEN: return "hidden"; case SDL_WINDOWEVENT_EXPOSED: return "exposed";
    case SDL_WINDOWEVENT_MOVED: return "moved"; case SDL_WINDOWEVENT_RESIZED: return "resized"; case SDL_WINDOWEVENT_SIZE_CHANGED: return "size_changed";
    case SDL_WINDOWEVENT_MINIMIZED: return "minimized"; case SDL_WINDOWEVENT_MAXIMIZED: return "maximized"; case SDL_WINDOWEVENT_RESTORED: return "restored";
    case SDL_WINDOWEVENT_ENTER: return "mouse_enter"; case SDL_WINDOWEVENT_LEAVE: return "mouse_leave"; case SDL_WINDOWEVENT_FOCUS_GAINED: return "focus_gained";
    case SDL_WINDOWEVENT_FOCUS_LOST: return "focus_lost"; case SDL_WINDOWEVENT_CLOSE: return "close"; case SDL_WINDOWEVENT_TAKE_FOCUS: return "take_focus";
    case SDL_WINDOWEVENT_DISPLAY_CHANGED: return "display_changed"; default: return "other";
  }
}

// Runs on the guest's thread(s): hand the latest pointer position to the game and wake DirectInput waiters.
void rt_pump_events(void) {
  rt_dinput_poll();
  if (!overlay_is_open()) mouse_follow();
  rt_dinput_poll();
}

// Runs on the main (SDL) thread: turns SDL events into input state.
static void sdl_pump(void) {
  static unsigned long n_motion;
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F9 && !e.key.repeat) {
      DBG("input", "F9 pressed: toggling overlay (was open=%d)", overlay_is_open());
      overlay_toggle(); memset(g_dik_down, 0, sizeof g_dik_down); memset(g_mouse_btn, 0, sizeof g_mouse_btn);
      continue;
    }
    if (overlay_is_open() && e.type == SDL_MOUSEMOTION) {
      if (SDL_GetRelativeMouseMode()) vcursor_move(e.motion.xrel, e.motion.yrel);
      else { int lx2, ly2; backend_window_to_logical(e.motion.x, e.motion.y, &lx2, &ly2); backend_set_virtual_cursor(lx2, ly2); }
    }
    if (overlay_is_open() && e.type != SDL_QUIT && e.type != SDL_WINDOWEVENT) {
      if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) DBG("overlay", "mouse button event %s button=%d window=(%d,%d)", e.type == SDL_MOUSEBUTTONDOWN ? "down" : "up", e.button.button, e.button.x, e.button.y);
      else if (e.type == SDL_MOUSEMOTION) DBG_EVERY(40, "overlay", "mouse motion window=(%d,%d)", e.motion.x, e.motion.y);
      overlay_event(&e, ren); stage_dirty = 1; continue;
    }
    switch (e.type) {
      case SDL_QUIT: DBG("input", "SDL_QUIT -> posting WM_CLOSE to the game"); rt_post_message(0x10 /*WM_CLOSE*/, 0, 0); break;
      case SDL_KEYDOWN: case SDL_KEYUP: {
        int d = dik_of(e.key.keysym.scancode);
        DBG("input", "key %s scancode=%d sym=%d -> DIK %d", e.type == SDL_KEYDOWN ? "down" : "up", e.key.keysym.scancode, e.key.keysym.sym, d);
        if (d) g_dik_down[d] = e.type == SDL_KEYDOWN;
        break;
      }
      case SDL_MOUSEMOTION: {
        int lx, ly;
        if (SDL_GetRelativeMouseMode()) vcursor_move(e.motion.xrel, e.motion.yrel);
        else backend_window_to_logical(e.motion.x, e.motion.y, &lx, &ly), backend_set_virtual_cursor(lx, ly);
        lx = (int)vcx; ly = (int)vcy;
        g_mouse_x = lx; g_mouse_y = ly;
        tgt_x = lx; tgt_y = ly; tgt_valid = 1;
        n_motion++;
        if (n_motion <= 60 || n_motion % 40 == 0) {
          int ww, wh;
          SDL_GetWindowSize(win, &ww, &wh);
          DBG("mouse", "motion #%lu: rel=(%d,%d) abs=(%d,%d) relmode=%d speed=%d%% window=%dx%d logical=%dx%d scale=%.3f -> virtual cursor=(%.1f,%.1f)", n_motion, e.motion.xrel, e.motion.yrel, e.motion.x, e.motion.y, SDL_GetRelativeMouseMode(), g_settings.mouse_speed, ww, wh, cur_lw, cur_lh, window_scale(), vcx, vcy);
        }
        break;
      }
      case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: {
        int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 1 : e.button.button == SDL_BUTTON_MIDDLE ? 2 : -1;
        DBG("mouse", "button %s sdl_button=%d -> game button %d window=(%d,%d) clicks=%d", e.type == SDL_MOUSEBUTTONDOWN ? "DOWN" : "UP", e.button.button, b, e.button.x, e.button.y, e.button.clicks);
        if (b >= 0) g_mouse_btn[b] = e.type == SDL_MOUSEBUTTONDOWN;
        break;
      }
      case SDL_MOUSEWHEEL: DBG("mouse", "wheel x=%d y=%d (ignored)", e.wheel.x, e.wheel.y); break;
      case SDL_WINDOWEVENT: {
        int ww, wh;
        SDL_GetWindowSize(win, &ww, &wh);
        DBG("window", "event %s data=(%d,%d) -> window size now %dx%d, overlay open=%d", win_event_name(e.window.event), e.window.data1, e.window.data2, ww, wh, overlay_is_open());
        if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
          mouse_resync();
          if (!getenv("RT_NO_GRAB")) { int rc = SDL_SetRelativeMouseMode(SDL_TRUE); DBG("mouse", "relative mode ON (focus gained) rc=%d", rc); }
        }
        if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) { SDL_SetRelativeMouseMode(SDL_FALSE); DBG("mouse", "relative mode OFF (focus lost)"); }
        break;
      }
      default: break;
    }
  }
}

static void backend_dump(FILE* f) {
  fprintf(f, "window=%p renderer=%p texture=%p\n", (void*)win, (void*)ren, (void*)tex);
  if (win) {
    int ww, wh, wx, wy, ow = 0, oh = 0, mxp, myp, gx, gy;
    SDL_GetWindowSize(win, &ww, &wh);
    SDL_GetWindowPosition(win, &wx, &wy);
    if (ren) SDL_GetRendererOutputSize(ren, &ow, &oh);
    SDL_GetMouseState(&mxp, &myp);
    SDL_GetGlobalMouseState(&gx, &gy);
    fprintf(f, "window size=%dx%d pos=%d,%d flags=%#x grab=%d renderer output=%dx%d\n", ww, wh, wx, wy, SDL_GetWindowFlags(win), SDL_GetWindowGrab(win), ow, oh);
    fprintf(f, "mouse in window=(%d,%d) global=(%d,%d) focus_window_is_ours=%d mouse_focus_is_ours=%d\n", mxp, myp, gx, gy, SDL_GetKeyboardFocus() == win, SDL_GetMouseFocus() == win);
  }
  fprintf(f, "logical=%dx%d mode=%dx%d stage=%dx%d tex=%dx%d stage_dirty=%d overlay_open=%d game_cursor_visible=%d\n", cur_lw, cur_lh, mode_w, mode_h, stage_w, stage_h, tex_w, tex_h, (int)stage_dirty, overlay_is_open(), game_cursor_visible);
  fprintf(f, "virtual cursor=(%.1f,%.1f) speed=%d%% relative_mode=%d\n", vcx, vcy, g_settings.mouse_speed, SDL_GetRelativeMouseMode());
  fprintf(f, "g_mouse xy=(%d,%d) pending dxdy=(%d,%d) btn=%d%d%d target=(%d,%d) valid=%d poke=%d\n", g_mouse_x, g_mouse_y, g_mouse_dx, g_mouse_dy, g_mouse_btn[0], g_mouse_btn[1], g_mouse_btn[2], tgt_x, tgt_y, tgt_valid, (int)g_mouse_poke);
  if (g_mem) {
    fprintf(f, "game cursor: cur=(%d,%d) prev=(%d,%d) game screen=%ux%u buttons=%02x%02x%02x\n", (int)GU32(0x4c4f50), (int)GU32(0x4c4f54), (int)GU32(0x4c5468), (int)GU32(0x4c546c), GU32(0x4dd368), GU32(0x4dd36c), GU8(0x4c4f4a), GU8(0x4c4f49), GU8(0x4c4f48));
  }
}

// ---- scripted input for automated tests: RT_SCRIPT=file with lines
//   wait MS | move X Y | click X Y | rclick X Y | key DIK | keydown DIK | keyup DIK | shot
extern int rt_time_scale;
static void gdelay(unsigned ms) { SDL_Delay(ms / (unsigned)rt_time_scale + 1); }
static int script_x, script_y;
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
      g_mouse_x = x; g_mouse_y = y; script_x = x; script_y = y;
      backend_set_virtual_cursor(x, y);
      gdelay(120);
      if (strcmp(cmd, "move")) {
        int b = !strcmp(cmd, "rclick") ? 1 : 0;
        g_mouse_btn[b] = 1; gdelay(120); g_mouse_btn[b] = 0; gdelay(120);
      }
    } else if (!strcmp(cmd, "dragto")) {   // relative move from the last scripted position (keeps buttons held)
      int steps = 8;
      for (int i = 1; i <= steps; i++) { g_mouse_dx += (x - script_x) * i / steps - (x - script_x) * (i - 1) / steps; g_mouse_dy += (y - script_y) * i / steps - (y - script_y) * (i - 1) / steps; gdelay(60); }
      script_x = x; script_y = y; backend_set_virtual_cursor(x, y); gdelay(150);
    }
    else if (!strcmp(cmd, "down")) g_mouse_btn[0] = 1;
    else if (!strcmp(cmd, "up")) g_mouse_btn[0] = 0;
    else if (!strcmp(cmd, "key")) { g_dik_down[x & 255] = 1; gdelay(120); g_dik_down[x & 255] = 0; gdelay(80); }
    else if (!strcmp(cmd, "keydown")) g_dik_down[x & 255] = 1;
    else if (!strcmp(cmd, "keyup")) g_dik_down[x & 255] = 0;
    else if (!strcmp(cmd, "pokeside")) { extern uint8_t* g_mem; int v = 0; sscanf(line, "%*s %d %d", &x, &v); uint32_t base = *(uint32_t*)(g_mem + 0x4dd2f4); *(uint32_t*)(g_mem + base + (uint32_t)x) = (uint32_t)v; }
    else if (!strcmp(cmd, "scan16")) { extern void rt_debug_scan16(uint32_t); rt_debug_scan16((uint32_t)x); }
    else if (!strcmp(cmd, "dumpside")) { extern uint8_t* g_mem; uint32_t base = *(uint32_t*)(g_mem + 0x4dd2f4); FILE* df = fopen("/tmp/side0.bin", "wb"); if (df) { fwrite(g_mem + base, 1, 0x69c * 2, df); fclose(df); } }
    else if (!strcmp(cmd, "fsnap")) { extern void rt_debug_fsnap(void); rt_debug_fsnap(); }
    else if (!strcmp(cmd, "fdiff")) { extern void rt_debug_fdiff(int); rt_debug_fdiff(x); }
    else if (!strcmp(cmd, "fdiffd")) { extern void rt_debug_fdiffd(void); rt_debug_fdiffd(); }
    else if (!strcmp(cmd, "fdifff")) { extern void rt_debug_fdifff(void); rt_debug_fdifff(); }
    else if (!strcmp(cmd, "dsnap")) { extern void rt_debug_dsnap(void); rt_debug_dsnap(); }
    else if (!strcmp(cmd, "ddiff")) { extern void rt_debug_ddiff(void); rt_debug_ddiff(); }
    else if (!strcmp(cmd, "pokeb")) { extern uint8_t* g_mem; unsigned ad = 0, v = 0; sscanf(line, "%*s %x %u", &ad, &v); g_mem[ad] = (uint8_t)v; }
    else if (!strcmp(cmd, "poke")) { extern uint8_t* g_mem; unsigned ad = 0, v = 0; sscanf(line, "%*s %x %u", &ad, &v); *(uint32_t*)(g_mem + ad) = v; }
    else if (!strcmp(cmd, "snap")) { extern void rt_debug_snap(uint32_t); rt_debug_snap((uint32_t)x); }
    else if (!strcmp(cmd, "diff")) { extern void rt_debug_diff(void); rt_debug_diff(); }
    else if (!strcmp(cmd, "close")) { SDL_Event ev; memset(&ev, 0, sizeof ev); ev.type = SDL_QUIT; SDL_PushEvent(&ev); }
    else if (!strcmp(cmd, "f9")) { SDL_Event ev; memset(&ev, 0, sizeof ev); ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_F9; SDL_PushEvent(&ev); }
    else if (!strcmp(cmd, "winmove")) { int lx, ly; backend_window_to_logical(x, y, &lx, &ly); backend_set_virtual_cursor(lx, ly); SDL_Event ev; memset(&ev, 0, sizeof ev); ev.type = SDL_MOUSEMOTION; ev.motion.x = x; ev.motion.y = y; SDL_PushEvent(&ev); gdelay(120); }
    else if (!strcmp(cmd, "ovclick")) {
      SDL_Event ev; memset(&ev, 0, sizeof ev);
      { int lx, ly; backend_window_to_logical(x, y, &lx, &ly); backend_set_virtual_cursor(lx, ly); }
      ev.type = SDL_MOUSEMOTION; ev.motion.x = x; ev.motion.y = y; SDL_PushEvent(&ev); gdelay(150);
      memset(&ev, 0, sizeof ev); ev.type = SDL_MOUSEBUTTONDOWN; ev.button.button = SDL_BUTTON_LEFT; ev.button.x = x; ev.button.y = y; SDL_PushEvent(&ev); gdelay(200);
    }
    else if (!strcmp(cmd, "cheat")) { char nm[32] = ""; sscanf(line, "%*s %31s", nm); overlay_request(!strcmp(nm, "unlock") ? 1 : !strcmp(nm, "win") ? 2 : !strcmp(nm, "gold") ? 3 : 0); }
    else if (!strcmp(cmd, "scan")) { extern void rt_debug_scan(uint32_t); rt_debug_scan((uint32_t)x); }
    else if (!strcmp(cmd, "objscan")) {   // objscan VTABLES_FILE OUT_FILE: list live objects (dword at a == known vtable) in heap and VirtualAlloc regions
      extern void rt_objscan(const char*, const char*); char a1[300] = "", a2[300] = ""; sscanf(line, "%*s %299s %299s", a1, a2); rt_objscan(a1, a2);
    }
    else if (!strcmp(cmd, "heapdump")) {   // raw guest heap + data section for offline analysis: heapdump PATH
      extern uint32_t rt_heap_top(void); char path[400] = ""; sscanf(line, "%*s %399s", path);
      FILE* hf = fopen(path, "wb");
      if (hf) { uint32_t top = rt_heap_top(); fwrite(&top, 4, 1, hf); fwrite(g_mem + 0x49b000, 1, 0x531000 - 0x49b000, hf); fwrite(g_mem + 0x10000000u, 1, top - 0x10000000u, hf); fclose(hf); }
    }
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

void backend_apply_window_mode(int mode) {
  if (!win) return;
  if (mode == 2) SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
  else {
    SDL_SetWindowFullscreen(win, 0);
    SDL_SetWindowBordered(win, mode == 1 ? SDL_FALSE : SDL_TRUE);
  }
}
void backend_set_smooth(int smooth) {
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, smooth ? "linear" : "nearest");
  tex_reset = 1;   // the hint applies to textures created afterwards
}

// ---- main (SDL) thread: the game runs on its own thread; this thread owns the window, input and presentation ----
typedef struct RpcJob { void (*fn)(void*); void* arg; int done; struct RpcJob* next; } RpcJob;
static pthread_mutex_t rpc_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t rpc_cv = PTHREAD_COND_INITIALIZER;
static RpcJob* rpc_head;
static void run_on_main(void (*fn)(void*), void* arg) {
  if (!main_thread_set || on_main_thread()) { fn(arg); return; }
  RpcJob j = {fn, arg, 0, NULL};
  pthread_mutex_lock(&rpc_lock);
  RpcJob** pp = &rpc_head;
  while (*pp) pp = &(*pp)->next;
  *pp = &j;
  while (!j.done) pthread_cond_wait(&rpc_cv, &rpc_lock);
  pthread_mutex_unlock(&rpc_lock);
}
static void rpc_drain(void) {
  pthread_mutex_lock(&rpc_lock);
  while (rpc_head) {
    RpcJob* j = rpc_head;
    rpc_head = j->next;
    pthread_mutex_unlock(&rpc_lock);
    j->fn(j->arg);
    pthread_mutex_lock(&rpc_lock);
    j->done = 1;
    pthread_cond_broadcast(&rpc_cv);
  }
  pthread_mutex_unlock(&rpc_lock);
}
// RT_STATE_LOG=file: the game's own per-tick state checksums (used for lockstep desync detection) written once per tick.
static void state_log_poll(void) {
  static FILE* f; static int init; static uint32_t last_frame = ~0u; static uint64_t seen_at;
  if (!init) { init = 1; const char* p = getenv("RT_STATE_LOG_UNSYNCED"); if (p) f = fopen(p, "w"); }
  if (!f || !g_mem) return;
  uint32_t fr = GU32(0x4c5730);
  uint64_t now = SDL_GetTicks64();
  if (fr != last_frame) { last_frame = fr; seen_at = now; return; }
  if (seen_at && now - seen_at >= 35) {   // the tick thread sleeps most of each 83 ms: sample when it is idle
    seen_at = 0;
    fprintf(f, "%u", fr);
    static const uint32_t pools[4] = {0x4cc3d0, 0x4cd698, 0x5156f8, 0x4c1978};   // object pools iterated by the game's tick
    for (int pi = 0; pi < 4; pi++) {
      uint32_t pb = pools[pi], base = GU32(pb + 4), hwm = GU32(pb + 8), stride = GU32(pb + 0x10);
      uint64_t h = 1469598103934665603ull; unsigned live = 0;
      if (base && hwm > base && stride >= 16 && stride <= 0x400)
        for (uint32_t a = base; a + stride <= hwm; a += stride) {
          uint32_t vt = GU32(a + 8);
          if (vt < 0x480000 || vt >= 0x4c4000) continue;   // free slot
          live++;
          for (uint32_t k = 8; k < stride; k += 4) {
            uint32_t v = GU32(a + k);
            if (v >= 0x10000000u && v < 0x70000000u) v = 0xdeadbeef;   // host-layout dependent pointers are not part of the game state
            h = (h ^ v) * 1099511628211ull;
          }
        }
      fprintf(f, " %u:%016llx", live, (unsigned long long)h);
    }
    fputc('\n', f); fflush(f);
  }
}
void backend_main_init(void) {   // call on the process main thread before starting the game thread
  main_thread = pthread_self(); main_thread_set = 1;
  ensure_sdl();
}
void backend_main_loop(void) {   // never returns: process exits through the game's ExitProcess
  for (;;) {
    rpc_drain();
    if (!win || !ren) { SDL_Delay(1); continue; }
    uint64_t t0 = SDL_GetPerformanceCounter();
    sdl_pump();
    state_log_poll();
    present_stage();
    double ms = (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
    if (ms < 3.0) SDL_Delay(4);   // no working vsync (minimized window, software renderer): do not spin
  }
}
