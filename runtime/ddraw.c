// IDirectDraw / IDirectDrawSurface / IDirectDrawPalette (DirectX 3-5 era) on top of the SDL backend.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aztec_rt.h"
#include "backend.h"

enum { DD_OK = 0, DDERR_UNSUPPORTED = 0x80004001u, DDERR_GENERIC = 0x80004005u, E_NOINTERFACE = 0x80004002u,
       DDERR_INVALIDPARAMS = 0x80070057u, DDERR_NOTFOUND = 0x887600FFu };

typedef struct Surface Surface;
typedef struct Palette { uint32_t argb[256]; uint8_t raw[1024]; } Palette;

struct Surface {
  int w, h, bpp, pitch;
  uint32_t pixels;          // guest address
  uint32_t self;            // guest COM object
  int primary;
  Surface* back;            // attached back buffer (flip chain)
  Palette* pal;
  uint32_t pal_obj;
  int has_key;
  uint32_t key_lo, key_hi;
  int refs;
};

static struct { int w, h, bpp; int created; } mode = {640, 480, 16, 0};
static Surface* primary_surf;
static uint32_t dd_obj;

// -------------------------------------------------------------- helpers
static int bytespp(int bpp) { return bpp == 8 ? 1 : (bpp + 7) / 8; }

static void pixel_format(uint32_t p, int bpp) {
  memset(GPTR(p), 0, 32);
  GU32(p) = 32;
  if (bpp == 8) { GU32(p + 4) = 0x20 | 0x40 * 0; GU32(p + 4) = 0x20; GU32(p + 12) = 8; return; }  // DDPF_PALETTEINDEXED8
  GU32(p + 4) = 0x40;  // DDPF_RGB
  GU32(p + 12) = (uint32_t)bpp == 15 ? 16 : (uint32_t)bpp;
  if (bpp == 16) { GU32(p + 16) = 0xF800; GU32(p + 20) = 0x07E0; GU32(p + 24) = 0x001F; }
  else if (bpp == 15) { GU32(p + 16) = 0x7C00; GU32(p + 20) = 0x03E0; GU32(p + 24) = 0x001F; }
  else { GU32(p + 16) = 0xFF0000; GU32(p + 20) = 0x00FF00; GU32(p + 24) = 0x0000FF; }
}

static void present(Surface* s) {
  if (!s) return;
  backend_present(GPTR(s->pixels), s->pitch, s->w, s->h, s->bpp, s->pal ? s->pal->argb : NULL);
  rt_pump_events();
}

static void palette_update_argb(Palette* p) {
  for (int i = 0; i < 256; i++) {
    const uint8_t* e = p->raw + 4 * i;  // PALETTEENTRY: r, g, b, flags
    p->argb[i] = 0xFF000000u | (uint32_t)e[0] << 16 | (uint32_t)e[1] << 8 | e[2];
  }
}

// ----------------------------------------------------------- palette COM
static uint32_t pal_qi(CPU* c, uint32_t* a) { (void)c; GU32(a[2]) = a[0]; return DD_OK; }
static uint32_t com_addref(CPU* c, uint32_t* a) { (void)c; (void)a; return 2; }
static uint32_t com_release(CPU* c, uint32_t* a) { (void)c; (void)a; return 1; }
static uint32_t pal_getcaps(CPU* c, uint32_t* a) { (void)c; GU32(a[1]) = 0x44; return DD_OK; }  // 8bit | allow256
static uint32_t pal_getentries(CPU* c, uint32_t* a) {
  (void)c;
  Palette* p = com_host(a[0]);
  memcpy(GPTR(a[4]), p->raw + 4 * a[2], 4 * a[3]);
  return DD_OK;
}
static uint32_t pal_setentries(CPU* c, uint32_t* a) {
  (void)c;
  Palette* p = com_host(a[0]);
  if (a[2] + a[3] > 256) return DDERR_INVALIDPARAMS;
  memcpy(p->raw + 4 * a[2], GPTR(a[4]), 4 * a[3]);
  palette_update_argb(p);
  if (primary_surf && primary_surf->pal == p) present(primary_surf);
  return DD_OK;
}
static const ComMethod pal_methods[] = {
  {"IDirectDrawPalette::QueryInterface", 2, pal_qi}, {"AddRef", 0, com_addref}, {"Release", 0, com_release},
  {"GetCaps", 1, pal_getcaps}, {"GetEntries", 4, pal_getentries}, {"Initialize", 3, NULL}, {"SetEntries", 4, pal_setentries},
};

// ----------------------------------------------------------- surface COM
static const ComMethod surf_methods[];
#define NSURF_METHODS 36

static Surface* new_surface(int w, int h, int bpp, int is_primary) {
  Surface* s = calloc(1, sizeof *s);
  s->w = w; s->h = h; s->bpp = bpp;
  s->pitch = (w * bytespp(bpp) + 31) & ~31;
  s->pixels = gmalloc((uint32_t)s->pitch * (uint32_t)h);
  memset(GPTR(s->pixels), 0, (size_t)s->pitch * (size_t)h);
  s->primary = is_primary;
  s->refs = 1;
  s->self = com_create(surf_methods, NSURF_METHODS, s);
  return s;
}

static uint32_t surf_qi(CPU* c, uint32_t* a) { (void)c; GU32(a[2]) = a[0]; return DD_OK; }
static uint32_t surf_release(CPU* c, uint32_t* a) { (void)c; (void)a; return 1; }

typedef struct { int l, t, r, b; } Rect;
static Rect get_rect(uint32_t p, const Surface* s) {
  Rect r = {0, 0, s->w, s->h};
  if (p) { r.l = (int32_t)GU32(p); r.t = (int32_t)GU32(p + 4); r.r = (int32_t)GU32(p + 8); r.b = (int32_t)GU32(p + 12); }
  if (r.l < 0) r.l = 0;
  if (r.t < 0) r.t = 0;
  if (r.r > s->w) r.r = s->w;
  if (r.b > s->h) r.b = s->h;
  return r;
}

static uint32_t surf_blt(CPU* c, uint32_t* a) {
  (void)c;
  Surface* d = com_host(a[0]);
  Surface* s = a[3] ? com_host(a[3]) : NULL;
  uint32_t flags = a[5];
  Rect dr = get_rect(a[2], d);
  int bp = bytespp(d->bpp);
  if (flags & 0x400) {  // DDBLT_COLORFILL
    uint32_t fill = a[6] ? GU32(a[6] + 80) : 0;
    for (int y = dr.t; y < dr.b; y++) {
      uint8_t* row = (uint8_t*)GPTR(d->pixels) + (size_t)y * d->pitch + (size_t)dr.l * bp;
      for (int x = dr.l; x < dr.r; x++, row += bp) memcpy(row, &fill, bp);
    }
  } else if (s) {
    Rect sr = get_rect(a[4], s);
    int w = sr.r - sr.l, h = sr.b - sr.t;
    int dw = dr.r - dr.l, dh = dr.b - dr.t;
    if (a[2] && (dw != w || dh != h)) {  // stretch (nearest)
      for (int y = 0; y < dh; y++) {
        int sy = sr.t + y * h / (dh ? dh : 1);
        for (int x = 0; x < dw; x++) {
          int sx = sr.l + x * w / (dw ? dw : 1);
          memcpy((uint8_t*)GPTR(d->pixels) + (size_t)(dr.t + y) * d->pitch + (size_t)(dr.l + x) * bp,
                 (uint8_t*)GPTR(s->pixels) + (size_t)sy * s->pitch + (size_t)sx * bp, bp);
        }
      }
    } else {
      if (dr.l + w > d->w) w = d->w - dr.l;
      if (dr.t + h > d->h) h = d->h - dr.t;
      int keyed = (flags & 0x8000) && s->has_key;  // DDBLT_KEYSRC
      for (int y = 0; y < h; y++) {
        uint8_t* sp = (uint8_t*)GPTR(s->pixels) + (size_t)(sr.t + y) * s->pitch + (size_t)sr.l * bp;
        uint8_t* dp = (uint8_t*)GPTR(d->pixels) + (size_t)(dr.t + y) * d->pitch + (size_t)dr.l * bp;
        if (!keyed) { memmove(dp, sp, (size_t)w * bp); continue; }
        for (int x = 0; x < w; x++) {
          uint32_t v = 0;
          memcpy(&v, sp + x * bp, bp);
          if (v < s->key_lo || v > s->key_hi) memcpy(dp + x * bp, sp + x * bp, bp);
        }
      }
    }
  }
  if (d->primary) present(d);
  return DD_OK;
}

static uint32_t surf_bltfast(CPU* c, uint32_t* a) {  // x, y, src, rect, trans
  (void)c;
  Surface* d = com_host(a[0]);
  Surface* s = com_host(a[3]);
  Rect sr = get_rect(a[4], s);
  int bp = bytespp(d->bpp);
  int w = sr.r - sr.l, h = sr.b - sr.t;
  int x0 = (int)a[1], y0 = (int)a[2];
  int keyed = (a[5] & 0x2) && s->has_key;  // DDBLTFAST_SRCCOLORKEY
  for (int y = 0; y < h && y0 + y < d->h; y++) {
    if (y0 + y < 0) continue;
    for (int x = 0; x < w && x0 + x < d->w; x++) {
      if (x0 + x < 0) continue;
      uint8_t* sp = (uint8_t*)GPTR(s->pixels) + (size_t)(sr.t + y) * s->pitch + (size_t)(sr.l + x) * bp;
      uint8_t* dp = (uint8_t*)GPTR(d->pixels) + (size_t)(y0 + y) * d->pitch + (size_t)(x0 + x) * bp;
      if (keyed) { uint32_t v = 0; memcpy(&v, sp, bp); if (v >= s->key_lo && v <= s->key_hi) continue; }
      memcpy(dp, sp, bp);
    }
  }
  if (d->primary) present(d);
  return DD_OK;
}

static uint32_t surf_flip(CPU* c, uint32_t* a) {
  (void)c;
  Surface* s = com_host(a[0]);
  if (s->primary && s->back) {
    memcpy(GPTR(s->pixels), GPTR(s->back->pixels), (size_t)s->pitch * s->h);
    present(s);
  }
  return DD_OK;
}
static uint32_t surf_getattached(CPU* c, uint32_t* a) {
  (void)c;
  Surface* s = com_host(a[0]);
  if (s->back) { GU32(a[2]) = s->back->self; return DD_OK; }
  return DDERR_NOTFOUND;
}
static uint32_t surf_getcaps(CPU* c, uint32_t* a) { (void)c; Surface* s = com_host(a[0]); GU32(a[1]) = s->primary ? 0x200 : 0x40; return DD_OK; }
static uint32_t surf_getpalette(CPU* c, uint32_t* a) {
  (void)c;
  Surface* s = com_host(a[0]);
  if (!s->pal_obj) return 0x8876017Cu;  // DDERR_NOPALETTEATTACHED
  GU32(a[1]) = s->pal_obj;
  return DD_OK;
}
static uint32_t surf_getpixelformat(CPU* c, uint32_t* a) { (void)c; Surface* s = com_host(a[0]); pixel_format(a[1], s->bpp); return DD_OK; }
static uint32_t surf_getsurfacedesc(CPU* c, uint32_t* a) {
  (void)c;
  Surface* s = com_host(a[0]);
  uint32_t d = a[1];
  memset(GPTR(d + 4), 0, 104);
  GU32(d) = 108;
  GU32(d + 4) = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000;  // CAPS|HEIGHT|WIDTH|PITCH|PIXELFORMAT
  GU32(d + 8) = (uint32_t)s->h; GU32(d + 12) = (uint32_t)s->w; GU32(d + 16) = (uint32_t)s->pitch;
  pixel_format(d + 72, s->bpp);
  GU32(d + 104) = s->primary ? 0x200 : 0x40;
  return DD_OK;
}
static uint32_t surf_islost(CPU* c, uint32_t* a) { (void)c; (void)a; return DD_OK; }
static uint32_t surf_lock(CPU* c, uint32_t* a) {  // rect, desc, flags, event
  (void)c;
  Surface* s = com_host(a[0]);
  uint32_t d = a[2];
  Rect r = get_rect(a[1], s);
  memset(GPTR(d + 4), 0, 104);
  GU32(d) = 108;
  GU32(d + 4) = 0x2 | 0x4 | 0x8 | 0x400 | 0x1000;  // HEIGHT|WIDTH|PITCH|LPSURFACE|PIXELFORMAT
  GU32(d + 8) = (uint32_t)s->h; GU32(d + 12) = (uint32_t)s->w; GU32(d + 16) = (uint32_t)s->pitch;
  GU32(d + 36) = s->pixels + (uint32_t)r.t * (uint32_t)s->pitch + (uint32_t)r.l * (uint32_t)bytespp(s->bpp);
  pixel_format(d + 72, s->bpp);
  return DD_OK;
}
static void dump_surface(Surface* s) {
  const char* dir = getenv("RT_DUMP_SURF");
  if (!dir) return;
  static int count;
  if (++count % 200 != 1) return;
  const uint16_t* px = (const uint16_t*)GPTR(s->pixels);
  int nonzero = 0;
  for (int y = 0; y < s->h && !nonzero; y++)
    for (int x = 0; x < s->w; x++) if (s->bpp == 16 ? px[(size_t)y * s->pitch / 2 + x] : ((uint8_t*)px)[(size_t)y * s->pitch + x]) { nonzero = 1; break; }
  char path[512];
  snprintf(path, sizeof path, "%s/s%08x_%d_%s.ppm", dir, s->self, count, nonzero ? "data" : "empty");
  FILE* f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", s->w, s->h);
  for (int y = 0; y < s->h; y++)
    for (int x = 0; x < s->w; x++) {
      uint8_t rgb[3];
      if (s->bpp == 16) { uint16_t v = px[(size_t)y * s->pitch / 2 + x]; rgb[0] = (v >> 11) << 3; rgb[1] = ((v >> 5) & 63) << 2; rgb[2] = (v & 31) << 3; }
      else { uint8_t v = ((uint8_t*)px)[(size_t)y * s->pitch + x]; uint32_t cc = s->pal ? s->pal->argb[v] : v * 0x010101u; rgb[0] = cc >> 16; rgb[1] = cc >> 8; rgb[2] = cc; }
      fwrite(rgb, 1, 3, f);
    }
  fclose(f);
}
static uint32_t surf_unlock(CPU* c, uint32_t* a) { (void)c; Surface* s = com_host(a[0]); dump_surface(s); if (s->primary) present(s); return DD_OK; }
static uint32_t surf_restore(CPU* c, uint32_t* a) { (void)c; (void)a; return DD_OK; }
static uint32_t surf_setcolorkey(CPU* c, uint32_t* a) {
  (void)c;
  Surface* s = com_host(a[0]);
  if (a[2]) { s->has_key = 1; s->key_lo = GU32(a[2]); s->key_hi = GU32(a[2] + 4); }
  else s->has_key = 0;
  return DD_OK;
}
static uint32_t surf_setpalette(CPU* c, uint32_t* a) {
  (void)c;
  Surface* s = com_host(a[0]);
  s->pal_obj = a[1];
  s->pal = a[1] ? com_host(a[1]) : NULL;
  return DD_OK;
}
static uint32_t surf_addattached(CPU* c, uint32_t* a) { (void)c; Surface* s = com_host(a[0]); s->back = com_host(a[1]); return DD_OK; }
static uint32_t surf_getdc(CPU* c, uint32_t* a) { (void)c; (void)a; return DDERR_UNSUPPORTED; }
static uint32_t surf_ok(CPU* c, uint32_t* a) { (void)c; (void)a; return DD_OK; }

static const ComMethod surf_methods[NSURF_METHODS] = {
  {"QueryInterface", 2, surf_qi}, {"AddRef", 0, com_addref}, {"Release", 0, surf_release},
  {"AddAttachedSurface", 1, surf_addattached}, {"AddOverlayDirtyRect", 1, NULL}, {"Blt", 5, surf_blt},
  {"BltBatch", 3, NULL}, {"BltFast", 5, surf_bltfast}, {"DeleteAttachedSurface", 2, surf_ok},
  {"EnumAttachedSurfaces", 2, NULL}, {"EnumOverlayZOrders", 3, NULL}, {"Flip", 2, surf_flip},
  {"GetAttachedSurface", 2, surf_getattached}, {"GetBltStatus", 1, surf_ok}, {"GetCaps", 1, surf_getcaps},
  {"GetClipper", 1, NULL}, {"GetColorKey", 2, NULL}, {"GetDC", 1, surf_getdc}, {"GetFlipStatus", 1, surf_ok},
  {"GetOverlayPosition", 2, NULL}, {"GetPalette", 1, surf_getpalette}, {"GetPixelFormat", 1, surf_getpixelformat},
  {"GetSurfaceDesc", 1, surf_getsurfacedesc}, {"Initialize", 2, NULL}, {"IsLost", 0, surf_islost},
  {"Lock", 4, surf_lock}, {"ReleaseDC", 1, surf_ok}, {"Restore", 0, surf_restore}, {"SetClipper", 1, surf_ok},
  {"SetColorKey", 2, surf_setcolorkey}, {"SetOverlayPosition", 2, NULL}, {"SetPalette", 1, surf_setpalette},
  {"Unlock", 1, surf_unlock}, {"UpdateOverlay", 5, NULL}, {"UpdateOverlayDisplayList", 2, NULL},
  {"UpdateOverlayZOrder", 2, NULL},
};

// ----------------------------------------------------------- IDirectDraw
static uint32_t dd2_obj;
static const ComMethod dd2_methods[];
static uint32_t dd_qi(CPU* c, uint32_t* a) {
  (void)c;
  uint32_t iid1 = GU32(a[1]);
  if (iid1 == 0xB3A6F3E0u) {  // IID_IDirectDraw2
    if (!dd2_obj) dd2_obj = com_create(dd2_methods, 24, NULL);
    GU32(a[2]) = dd2_obj;
    rt_log("QI DD2 -> %08x vtbl=%08x ppv=%08x", dd2_obj, GU32(dd2_obj), a[2]);
    return DD_OK;
  }
  if (iid1 == 0x6C14DB80u) { GU32(a[2]) = dd_obj ? dd_obj : a[0]; return DD_OK; }  // IID_IDirectDraw
  return E_NOINTERFACE;
}
static uint32_t dd_createpalette(CPU* c, uint32_t* a) {  // flags, entries, **pal, outer
  (void)c;
  Palette* p = calloc(1, sizeof *p);
  if (a[2]) memcpy(p->raw, GPTR(a[2]), 4 * 256);
  palette_update_argb(p);
  GU32(a[3]) = com_create(pal_methods, 7, p);
  return DD_OK;
}
static uint32_t dd_createsurface(CPU* c, uint32_t* a) {  // desc*, **surf, outer
  (void)c;
  uint32_t d = a[1];
  uint32_t flags = GU32(d + 4);
  uint32_t caps = (flags & 1) ? GU32(d + 104) : 0;
  if (caps & 0x200) {  // primary
    Surface* s = new_surface(mode.w, mode.h, mode.bpp, 1);
    primary_surf = s;
    if ((flags & 0x20) && GU32(d + 20) > 0) s->back = new_surface(mode.w, mode.h, mode.bpp, 0);
    GU32(a[2]) = s->self;
    return DD_OK;
  }
  int w = (flags & 4) ? (int)GU32(d + 12) : mode.w;
  int h = (flags & 2) ? (int)GU32(d + 8) : mode.h;
  int bpp = mode.bpp;
  if ((flags & 0x1000) && (GU32(d + 72 + 4) & 0x20)) bpp = 8;
  Surface* s = new_surface(w, h, bpp, 0);
  GU32(a[2]) = s->self;
  return DD_OK;
}
static uint32_t dd_enumdisplaymodes(CPU* c, uint32_t* a) {  // flags, desc*, ctx, callback
  static const struct { int w, h, bpp; } modes[] = {{640, 480, 8}, {640, 480, 16}, {800, 600, 8}, {800, 600, 16}, {1024, 768, 16}};
  uint32_t d = gmalloc(108);
  for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++) {
    memset(GPTR(d), 0, 108);
    GU32(d) = 108;
    GU32(d + 4) = 0x2 | 0x4 | 0x1000 | 0x20000;
    GU32(d + 8) = (uint32_t)modes[i].h; GU32(d + 12) = (uint32_t)modes[i].w;
    pixel_format(d + 72, modes[i].bpp);
    if (!rt_call_guest(c, a[4], 2, d, a[3])) break;  // callback returns DDENUMRET_OK(1) to continue
  }
  gfree(d);
  return DD_OK;
}
static uint32_t dd_getcaps(CPU* c, uint32_t* a) {
  (void)c;
  for (int k = 1; k <= 2; k++) if (a[k]) {
    uint32_t sz = GU32(a[k]);
    if (sz < 64 || sz > 1024) continue;
    memset(GPTR(a[k] + 4), 0, sz - 4);
    GU32(a[k] + 4) = 0x1 | 0x40 | 0x400 | 0x4000 | 0x20000 | 0x80000;   // 3D? no: BLT, BLTCOLORFILL, COLORKEY, ...
    GU32(a[k] + 0x3c) = 64u << 20; GU32(a[k] + 0x40) = 48u << 20;     // video memory total/free
  }
  return DD_OK;
}
static uint32_t dd_getdisplaymode(CPU* c, uint32_t* a) {
  (void)c;
  uint32_t d = a[1];
  memset(GPTR(d + 4), 0, 104);
  GU32(d) = 108; GU32(d + 4) = 0x2 | 0x4 | 0x1000;
  GU32(d + 8) = (uint32_t)mode.h; GU32(d + 12) = (uint32_t)mode.w;
  pixel_format(d + 72, mode.bpp);
  return DD_OK;
}
static uint32_t dd_setcooplevel(CPU* c, uint32_t* a) { (void)c; (void)a; return DD_OK; }
static uint32_t dd_setdisplaymode(CPU* c, uint32_t* a) {
  (void)c;
  mode.w = (int)a[1]; mode.h = (int)a[2]; mode.bpp = (int)a[3];
  if (mode.bpp == 15 || mode.bpp == 24) mode.bpp = 16;
  rt_log("DirectDraw SetDisplayMode %dx%dx%d", mode.w, mode.h, mode.bpp);
  backend_set_mode(mode.w, mode.h, mode.bpp);
  return DD_OK;
}
static uint32_t dd_ok(CPU* c, uint32_t* a) { (void)c; (void)a; return DD_OK; }
static const ComMethod dd_methods[] = {
  {"QueryInterface", 2, dd_qi}, {"AddRef", 0, com_addref}, {"Release", 0, com_release}, {"Compact", 0, dd_ok},
  {"CreateClipper", 3, NULL}, {"CreatePalette", 4, dd_createpalette}, {"CreateSurface", 3, dd_createsurface},
  {"DuplicateSurface", 2, NULL}, {"EnumDisplayModes", 4, dd_enumdisplaymodes}, {"EnumSurfaces", 4, NULL},
  {"FlipToGDISurface", 0, dd_ok}, {"GetCaps", 2, dd_getcaps}, {"GetDisplayMode", 1, dd_getdisplaymode},
  {"GetFourCCCodes", 2, NULL}, {"GetGDISurface", 1, NULL}, {"GetMonitorFrequency", 1, NULL}, {"GetScanLine", 1, NULL},
  {"GetVerticalBlankStatus", 1, NULL}, {"Initialize", 1, NULL}, {"RestoreDisplayMode", 0, dd_ok},
  {"SetCooperativeLevel", 2, dd_setcooplevel}, {"SetDisplayMode", 3, dd_setdisplaymode}, {"WaitForVerticalBlank", 2, dd_ok},
};

static uint32_t dd2_setdisplaymode(CPU* c, uint32_t* a) { return dd_setdisplaymode(c, a); }  // + refresh, flags
static uint32_t dd2_getavailvidmem(CPU* c, uint32_t* a) { (void)c; if (a[2]) GU32(a[2]) = 48u << 20; if (a[3]) GU32(a[3]) = 40u << 20; return DD_OK; }
static const ComMethod dd2_methods[24] = {
  {"QueryInterface", 2, dd_qi}, {"AddRef", 0, com_addref}, {"Release", 0, com_release}, {"Compact", 0, dd_ok},
  {"CreateClipper", 3, NULL}, {"CreatePalette", 4, dd_createpalette}, {"CreateSurface", 3, dd_createsurface},
  {"DuplicateSurface", 2, NULL}, {"EnumDisplayModes", 4, dd_enumdisplaymodes}, {"EnumSurfaces", 4, NULL},
  {"FlipToGDISurface", 0, dd_ok}, {"GetCaps", 2, dd_getcaps}, {"GetDisplayMode", 1, dd_getdisplaymode},
  {"GetFourCCCodes", 2, NULL}, {"GetGDISurface", 1, NULL}, {"GetMonitorFrequency", 1, NULL}, {"GetScanLine", 1, NULL},
  {"GetVerticalBlankStatus", 1, NULL}, {"Initialize", 1, NULL}, {"RestoreDisplayMode", 0, dd_ok},
  {"SetCooperativeLevel", 2, dd_setcooplevel}, {"SetDisplayMode", 5, dd2_setdisplaymode}, {"WaitForVerticalBlank", 2, dd_ok},
  {"GetAvailableVidMem", 3, dd2_getavailvidmem},
};
static uint32_t sh_DirectDrawCreate(CPU* c, uint32_t* a) {
  (void)c;
  if (!dd_obj) dd_obj = com_create(dd_methods, 23, NULL);
  rt_log("DirectDrawCreate: guid=%08x pp=%08x -> obj=%08x", a[0], a[1], dd_obj);
  GU32(a[1]) = dd_obj;
  return DD_OK;
}

void rt_register_ddraw_shims(void) {
  static const Shim s[] = {{"DDRAW.dll", "DirectDrawCreate", 3, sh_DirectDrawCreate}};
  rt_register_shims(s, 1);
}
