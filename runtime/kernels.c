/* Native replacements for hot blit kernels of the recompiled game.
 * Same stdcall-free (cdecl, caller cleans) convention as the original. */
#include "aztec_rt.h"
#include <string.h>

static inline uint32_t karg(CPU* c, int n) { return GU32(GREG(c, 0x10) + 4 + 4 * n); }
static inline void kret(CPU* c) {
  *(uint32_t*)(c->r + 0x284) = GU32(GREG(c, 0x10));
  GREG(c, 0x10) += 4;
}

/* f_004805e2(w, h, dst, src, dstStride, srcStride, u16 table[256]) */
void f_004805e2(CPU* c) {
  int32_t w = (int32_t)karg(c, 0), h = (int32_t)karg(c, 1);
  uint32_t dst = karg(c, 2), src = karg(c, 3);
  int32_t ds = (int32_t)karg(c, 4), ss = (int32_t)karg(c, 5);
  const uint16_t* tab = (const uint16_t*)(g_mem + karg(c, 6));
  if (w > 0 && h > 0) {
    for (; h; h--, src += ss, dst += ds) {
      const uint8_t* s = g_mem + src;
      uint16_t* d = (uint16_t*)(g_mem + dst);
      for (int x = 0; x < w; x++) d[x] = tab[s[x]];
    }
  }
  kret(c);
}

/* f_00480a36(w, h, dst, src, dstStride, srcStride, u16 table[65536], aux, auxStride) */
void f_00480a36(CPU* c) {
  int32_t w = (int32_t)karg(c, 0), h = (int32_t)karg(c, 1);
  uint32_t dst = karg(c, 2), src = karg(c, 3);
  int32_t ds = (int32_t)karg(c, 4), ss = (int32_t)karg(c, 5);
  const uint16_t* tab = (const uint16_t*)(g_mem + karg(c, 6));
  uint32_t aux = karg(c, 7);
  int32_t as = (int32_t)karg(c, 8);
  if (w > 0 && h > 0) {
    for (; h; h--, src += ss, dst += ds, aux += as) {
      const uint8_t* s = g_mem + src;
      const uint8_t* a = g_mem + aux;
      uint16_t* d = (uint16_t*)(g_mem + dst);
      for (int x = 0; x < w; x++) d[x] = tab[s[x] | (a[x] << 8)];
    }
  }
  kret(c);
}
