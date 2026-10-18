// In-game helper menu (F9): display settings and a few helpers. Drawn with SDL on top of the game frame.
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aztec_rt.h"
#include "backend.h"
#include "font8x16.h"
#include "settings.h"

static int is_open;
static int tab;                       // 0 settings, 1 helpers
static SDL_Texture* atlas;
static int mx, my, clicked;           // logical mouse position, click edge
static uint32_t click_time;
static char status[160];
static uint32_t status_until;
static int pending_action;
enum { ACT_NONE, ACT_UNLOCK_MISSIONS, ACT_WIN_MISSION };
static const char* saved_argv0;

static const struct { int w, h; const char* label; } res_presets[] = {
  {0, 0, "Domyślna (800x600)"}, {1024, 768, "1024x768"}, {1280, 720, "1280x720"}, {1366, 768, "1366x768"},
  {1600, 900, "1600x900"}, {1920, 1080, "1920x1080"}, {2560, 1440, "2560x1440"}, {-1, -1, "Rozdzielczość ekranu"},
};
#define N_RES ((int)(sizeof res_presets / sizeof *res_presets))
static int res_index = -1;

static const char* const win_modes[] = {"Okno", "Okno bez ramki", "Pełny ekran"};

int overlay_is_open(void) { return is_open; }
void overlay_set_argv0(const char* a) { saved_argv0 = a; }

static int glyph_of(uint32_t cp) {
  for (int i = 0; i < FONT_COUNT; i++) if (font_cps[i] == cp) return i;
  return -1;
}

static void build_atlas(SDL_Renderer* ren) {
  uint32_t px[128 * 128];
  memset(px, 0, sizeof px);
  for (int g = 0; g < FONT_COUNT; g++)
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 8; x++)
        if (font8x16[g][y] & (0x80 >> x)) px[(size_t)((g / 16) * 16 + y) * 128 + (g % 16) * 8 + x] = 0xFFFFFFFFu;
  atlas = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 128, 128);
  SDL_UpdateTexture(atlas, NULL, px, 128 * 4);
  SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);
}

static int text_width(const char* s, int scale) {
  int n = 0;
  for (const unsigned char* p = (const unsigned char*)s; *p; p++) if ((*p & 0xC0) != 0x80) n++;
  return n * 8 * scale;
}

static void draw_text(SDL_Renderer* ren, int x, int y, const char* s, int scale, uint8_t r, uint8_t g, uint8_t b) {
  SDL_SetTextureColorMod(atlas, r, g, b);
  const unsigned char* p = (const unsigned char*)s;
  while (*p) {
    uint32_t cp = *p++;
    if (cp >= 0xC0) {                       // decode UTF-8 (up to 3 bytes)
      int extra = cp >= 0xE0 ? 2 : 1;
      cp &= extra == 2 ? 0x0F : 0x1F;
      while (extra-- && (*p & 0xC0) == 0x80) cp = cp << 6 | (*p++ & 0x3F);
    }
    int gi = glyph_of(cp);
    if (gi >= 0) {
      SDL_Rect src = {(gi % 16) * 8, (gi / 16) * 16, 8, 16}, dst = {x, y, 8 * scale, 16 * scale};
      SDL_RenderCopy(ren, atlas, &src, &dst);
    }
    x += 8 * scale;
  }
}

static void say(const char* msg) { snprintf(status, sizeof status, "%s", msg); status_until = SDL_GetTicks() + 4000; }

static int inside(int x, int y, int w, int h) { return mx >= x && mx < x + w && my >= y && my < y + h; }

static int button(SDL_Renderer* ren, int x, int y, int w, int h, const char* label) {
  int hot = inside(x, y, w, h);
  SDL_SetRenderDrawColor(ren, hot ? 120 : 70, hot ? 80 : 50, 30, 255);
  SDL_Rect r = {x, y, w, h};
  SDL_RenderFillRect(ren, &r);
  SDL_SetRenderDrawColor(ren, 220, 180, 100, 255);
  SDL_RenderDrawRect(ren, &r);
  draw_text(ren, x + (w - text_width(label, 2)) / 2, y + (h - 32) / 2, label, 2, 255, 240, 200);
  if (hot && clicked) { clicked = 0; return 1; }
  return 0;
}

// "label  < value >" row; returns -1 / +1 when the left / right arrow is clicked
static int chooser(SDL_Renderer* ren, int x, int y, const char* label, const char* value) {
  draw_text(ren, x, y + 4, label, 2, 230, 210, 170);
  int vx = x + 300;
  int res = 0;
  if (button(ren, vx, y, 40, 40, "◄")) res = -1;
  SDL_Rect box = {vx + 46, y, 330, 40};
  SDL_SetRenderDrawColor(ren, 30, 25, 20, 255);
  SDL_RenderFillRect(ren, &box);
  draw_text(ren, box.x + (box.w - text_width(value, 2)) / 2, y + 4, value, 2, 255, 255, 255);
  if (button(ren, vx + 382, y, 40, 40, "►")) res = 1;
  return res;
}

static void sync_res_index(void) {
  res_index = 0;
  for (int i = 0; i < N_RES; i++)
    if (res_presets[i].w == g_settings.res_w && res_presets[i].h == g_settings.res_h) res_index = i;
}

static void apply_resolution_choice(int idx) {
  if (res_presets[idx].w == -1) {
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { g_settings.res_w = dm.w; g_settings.res_h = dm.h; }
  } else {
    g_settings.res_w = res_presets[idx].w; g_settings.res_h = res_presets[idx].h;
  }
  res_index = idx;
}

static void restart_game(void) {
  settings_save();
  fflush(NULL);
  char self[1100];
  ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1);
  if (n > 0) { self[n] = 0; } else snprintf(self, sizeof self, "%s", saved_argv0 ? saved_argv0 : "aztec_recomp");
  extern char** rt_saved_argv;
  execv(self, rt_saved_argv);
  say("Nie udało się uruchomić ponownie");
}

extern void backend_overlay_cursor(int open);
void overlay_toggle(void) {
  is_open = !is_open;
  if (is_open) sync_res_index();
  backend_overlay_cursor(is_open);
}

void overlay_event(const SDL_Event* e, SDL_Renderer* ren) {
  if (e->type == SDL_KEYDOWN) {
    if (e->key.keysym.sym == SDLK_ESCAPE || e->key.keysym.sym == SDLK_F9) { is_open = 0; backend_overlay_cursor(0); }
    if (e->key.keysym.sym == SDLK_TAB) tab = !tab;
  } else if (e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEBUTTONDOWN) {
    int x = e->type == SDL_MOUSEMOTION ? e->motion.x : e->button.x, y = e->type == SDL_MOUSEMOTION ? e->motion.y : e->button.y;
    extern void backend_window_to_logical(int wx, int wy, int* lx, int* ly);
    backend_window_to_logical(x, y, &mx, &my);
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) { clicked = 1; click_time = SDL_GetTicks(); }
    if (getenv("RT_TRACE_INPUT") && e->type == SDL_MOUSEBUTTONDOWN) rt_log("overlay click window=(%d,%d) logical=(%d,%d)", x, y, mx, my);
  }
}

void overlay_draw(SDL_Renderer* ren, int lw, int lh) {
  if (!atlas) build_atlas(ren);
  if (!is_open) {
    if (SDL_GetTicks() < status_until) {   // transient message even when the menu is closed
      SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(ren, 0, 0, 0, 170);
      SDL_Rect b = {8, lh - 40, text_width(status, 2) + 16, 34};
      SDL_RenderFillRect(ren, &b);
      draw_text(ren, 16, lh - 32, status, 2, 255, 230, 160);
    }
    return;
  }
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ren, 0, 0, 0, 150);
  SDL_Rect full = {0, 0, lw, lh};
  SDL_RenderFillRect(ren, &full);
  int pw = 720, ph = 470, px = (lw - pw) / 2, py = (lh - ph) / 2;
  if (px < 0) px = 0;
  if (py < 0) py = 0;
  SDL_SetRenderDrawColor(ren, 45, 32, 22, 245);
  SDL_Rect panel = {px, py, pw, ph};
  SDL_RenderFillRect(ren, &panel);
  SDL_SetRenderDrawColor(ren, 220, 180, 100, 255);
  SDL_RenderDrawRect(ren, &panel);
  draw_text(ren, px + 20, py + 12, "Aztec Empire: menu pomocnicze (F9)", 2, 255, 220, 140);
  if (button(ren, px + 20, py + 56, 200, 40, "Ustawienia")) tab = 0;
  if (button(ren, px + 230, py + 56, 200, 40, "Pomoc w grze")) tab = 1;
  int y = py + 120;
  if (tab == 0) {
    if (res_index < 0) sync_res_index();
    int d = chooser(ren, px + 20, y, "Rozdzielczość gry:", res_presets[res_index].label);
    if (d) apply_resolution_choice((res_index + d + N_RES) % N_RES);
    y += 56;
    d = chooser(ren, px + 20, y, "Tryb okna:", win_modes[g_settings.window_mode]);
    if (d) { g_settings.window_mode = (g_settings.window_mode + d + 3) % 3; backend_apply_window_mode(g_settings.window_mode); settings_save(); }
    y += 56;
    d = chooser(ren, px + 20, y, "Skalowanie obrazu:", g_settings.smooth ? "Gładkie" : "Ostre (piksele)");
    if (d) { g_settings.smooth = !g_settings.smooth; backend_set_smooth(g_settings.smooth); settings_save(); }
    y += 56;
    d = chooser(ren, px + 20, y, "Filmy:", g_settings.video ? "Włączone" : "Wyłączone");
    if (d) { g_settings.video = !g_settings.video; settings_save(); }
    y += 62;
    draw_text(ren, px + 20, y, "Rozdzielczość gry zmienia się po ponownym uruchomieniu.", 1, 200, 190, 170);
    if (button(ren, px + 20, py + ph - 56, 440, 40, "Zapisz i uruchom ponownie")) restart_game();
  } else {
    draw_text(ren, px + 20, y, "Działają na bieżącej misji i na zapisie gracza.", 1, 200, 190, 170);
    y += 28;
    if (button(ren, px + 20, y, 680, 44, "Odblokuj wszystkie misje kampanii")) { pending_action = ACT_UNLOCK_MISSIONS; }
    y += 56;
    if (button(ren, px + 20, y, 680, 44, "Natychmiastowe zwycięstwo w misji")) { pending_action = ACT_WIN_MISSION; }
    y += 66;
    draw_text(ren, px + 20, y, "Odblokowane misje widać po ponownym wejściu w ekran Kampania.", 1, 200, 190, 170);
  }
  if (button(ren, px + pw - 190, py + ph - 56, 170, 40, "Zamknij")) { is_open = 0; backend_overlay_cursor(0); }
  if (SDL_GetTicks() < status_until) draw_text(ren, px + 20, py + ph - 88, status, 2, 255, 230, 160);
  if (clicked && SDL_GetTicks() - click_time > 300) clicked = 0;   // a click survives until the next frame that can use it
}

// Runs on the game's main thread (called from PeekMessage), where calling guest code is safe.
void overlay_run_actions(CPU* c) {
  int a = pending_action;
  if (!a) return;
  pending_action = 0;
  if (a == ACT_UNLOCK_MISSIONS) {
    rt_call_guest(c, 0x43f560, 1, 15);   // set "missions reached" = 15 and save the player file
    say("Misje odblokowane");
  } else if (a == ACT_WIN_MISSION) {
    uint32_t sides = GU32(0x4dd2f4);
    if (!sides) { say("Brak aktywnej misji"); return; }
    uint32_t idx = rt_call_guest(c, 0x43d200, 0);   // index of the local side
    GU32(sides + 0x140 + idx * 0x69c) = 1;          // same flag the built-in CheatWin key sets
    say("Zwycięstwo zgłoszone");
  }
}
void overlay_request(int action) { pending_action = action; }
