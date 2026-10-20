// Movement instrumentation (additive, env-driven):
//   RT_PATH_LOG=file  one line per wsr_WaveSearch (0x46c470) call: frame, args, result length, path triplets
//   RT_POS_LOG=file   per tick, one line per live mobile unit: frame obj type x y tilex tiley state alive
//   RT_SPEED_UP=1     (unused)
#include "aztec_rt.h"
#include "dbg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void f_0046c470_orig(CPU* c);
static FILE *pathf, *posf; static int inited;
static void init(void) { inited = 1; const char* p = getenv("RT_PATH_LOG"); if (p) pathf = fopen(p, "w"); p = getenv("RT_POS_LOG"); if (p) posf = fopen(p, "w"); }

void f_0046c470(CPU* c) {   // int __cdecl wsr_WaveSearch(sx,sy,gx,gy,out,fogmap,class,flag,goallist,radius)
  if (!inited) init();
  uint32_t sp = GREG(c, 0x10), a[10];
  for (int i = 0; i < 10; i++) a[i] = GU32(sp + 4 + 4 * i);
  f_0046c470_orig(c);
  if (pathf) {
    int n = (int)GREG(c, 0x00);
    fprintf(pathf, "%u s=%d,%d g=%d,%d cls=%u flag=%u list=%u rad=%d n=%d", GU32(0x4c5730), (int)a[0], (int)a[1], (int)a[2], (int)a[3], a[6], a[7] & 0xff, a[8], (int)a[9], n);
    for (int i = 0; i < n && i < 400; i++) fprintf(pathf, " %d,%d,%d", (int)GU32(a[4] + 12 * i), (int)GU32(a[4] + 12 * i + 4), (int)GU32(a[4] + 12 * i + 8));
    fputc('\n', pathf); fflush(pathf);
  }
}

static void dumpf(const char* base, const char* ext, const void* p, size_t n) {
  char path[600]; snprintf(path, sizeof path, "%s.%s", base, ext); FILE* f = fopen(path, "wb"); if (f) { fwrite(p, 1, n, f); fclose(f); }
}
// RT_DUMP_FRAME=N RT_DUMP_BASE=path: at tick N dump occupancy map (u32 grid), passability layers (bytes), side-0 fog map ((w+1)*h bytes)
static void dump_maps(void) {
  const char* base = getenv("RT_DUMP_BASE"); if (!base) return;
  uint32_t occ = GU32(0x4dd2f8), W = GU32(occ + 4) & 0xff, H = GU32(occ + 8) & 0xff, data = GU32(occ + 0xc);
  dumpf(base, "occ", GPTR(data), (size_t)W * H * 4);
  uint32_t w = GU32(0x515a54), h = GU32(0x515a50);
  static const char* nm[6] = {"l74", "l78", "l7c", "l80", "l84", "l88"}; static const uint32_t ad[6] = {0x515a74, 0x515a78, 0x515a7c, 0x515a80, 0x515a84, 0x515a88};
  for (int i = 0; i < 6; i++) dumpf(base, nm[i], GPTR(GU32(ad[i])), (size_t)w * h);
  uint32_t side0 = GU32(0x4dd2f4), fog = GU32(side0 + 0x690);
  if (fog) dumpf(base, "fog", GPTR(GU32(fog + 0x14)), (size_t)(w + 1) * h);
  { char path[600]; snprintf(path, sizeof path, "%s.desc", base); FILE* f = fopen(path, "w");
    if (f) { uint32_t pb = 0x4cc3d0, b = GU32(pb + 4), hw = GU32(pb + 8), st = GU32(pb + 0x10);
      for (uint32_t a = b; a + st <= hw; a += st) { uint32_t o = a + 8, vt = GU32(o); if (vt < 0x480000 || vt >= 0x4c4000) continue;
        fprintf(f, "type=%d vt=%x", (int)GU32(o + 0x50), vt);
        for (int k = 0; k < 3; k++) { uint32_t dp = GU32(o + 0x64 + 4 * k); fprintf(f, " | +%x=%x:", 0x64 + 4 * k, dp); if (dp > 0x400000 && dp < 0x600000) for (int j = 0; j < 17; j++) fprintf(f, " %x", GU32(dp + 4 * j)); }
        fputc('\n', f); }
      fclose(f); } }
  char hdr[100]; snprintf(hdr, sizeof hdr, "w=%u h=%u occW=%u occH=%u\n", w, h, W, H); dumpf(base, "hdr", hdr, strlen(hdr));
}

void pathlog_tick(uint32_t frame) {
  if (!inited) init();
  { static int dumped; const char* fr = getenv("RT_DUMP_FRAME"); if (fr && !dumped && frame >= (uint32_t)atoi(fr)) { dumped = 1; dump_maps(); } }
  if (!posf) return;
  uint32_t pb = 0x4cc3d0, base = GU32(pb + 4), hwm = GU32(pb + 8), stride = GU32(pb + 0x10);
  if (!base || hwm <= base || stride < 16) return;
  for (uint32_t a = base; a + stride <= hwm; a += stride) {
    uint32_t o = a + 8, vt = GU32(o);
    if (vt < 0x480000 || vt >= 0x4c4000) continue;
    uint32_t alive = GU8(o + 0x5c);
    fprintf(posf, "%u %x %u %d %d %d %d %d %u %d\n", frame, o, vt, (int)GU32(o + 0x50), (int)GU32(o + 0x24), (int)GU32(o + 0x28), (int)GU32(o + 0x44), (int)GU32(o + 0x48), GU32(o + 0x64), (int)alive);
  }
  fflush(posf);
}
