// Synthetic guest memory and renderer: never loads an executable or game assets.
#define _POSIX_C_SOURCE 200809L
#include "../runtime/aztec_rt.h"
#include "../runtime/l0_observer.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

uint8_t* g_mem;
static _Atomic int done;

static void pool_fixture(uint32_t pool, uint32_t base, uint32_t stride, uint32_t capacity,
                         uint32_t count, uint32_t sentinel) {
  GU32(pool + 4) = base; GU32(pool + 8) = count < capacity ? base + count * stride : 0;
  GU32(pool + 0x10) = stride; GU32(pool + 0x18) = count;
  GU32(pool + 0x1c) = capacity * stride; GU32(pool + 0x24) = sentinel;
  GU32(sentinel) = count ? base : sentinel;
  GU32(sentinel + 4) = count ? base + (count - 1) * stride : sentinel;
  for (uint32_t i = 0; i < capacity; ++i) {
    uint32_t a = base + i * stride;
    GU32(a) = i + 1 < capacity ? a + stride : 0;
    if (i < count) {
      GU32(a) = i + 1 < count ? a + stride : sentinel;
      GU32(a + 4) = i ? a - stride : sentinel;
    }
  }
}
void rt_fatal(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(2);
}
static void* renderer(void* unused) {
  (void)unused;
  uint32_t* pixels = calloc(800 * 600, 4);
  if (!pixels) abort();
  struct timespec delay = {0, 1000000};
  while (!atomic_load(&done)) {
    rt_l0_present(pixels, 800 * 4, 800, 600, 32, NULL);
    nanosleep(&delay, NULL);
  }
  free(pixels);
  return NULL;
}
int main(int argc, char** argv) {
  if (argc != 4) return 2;
  setenv("RT_L0_SCHEDULE", argv[1], 1);
  setenv("RT_L0_OUTPUT", argv[2], 1);
  setenv("RT_DUMP_DIR", argv[3], 1);
  g_mem = calloc(0x700000, 1);
  if (!g_mem) return 2;
  pool_fixture(0x4cc3d0, 0x600000, 0x100, 133, 63, 0x610000);
  pool_fixture(0x5156f8, 0x620000, 0x200, 2, 2, 0x610008);
  GU32(0x4dd2f4) = 0x630000;
  GU32(0x640008) = 0x630000; GU32(0x640108) = 0x63069c;
  GU32(0x4933f8 + 3 * 8) = 6;
  GU32(0x4933f8 + 20 * 8) = 2;
  for (uint32_t i = 0; i < 133; ++i) {
    uint32_t o = 0x600008 + i * 0x100;
    GU32(o) = 0x480000;
    GU32(o + 0x50) = i == 0 ? 3 : i < 63 ? 27 + i % 2 : 20;
    GU32(o + 0x30) = i < 63 ? 0x640000 + (i % 2) * 0x100 : 0;
    GU32(o + 0x24) = i < 63 ? 72 + i * 48 : (uint32_t)-1000;
    GU32(o + 0x28) = i < 63 ? 72 : (uint32_t)-1000;
    GU32(o + 0x38) = i < 63 ? 100 : 0;
    GU8(o + 0x5c) = i < 63;
    GU32(o + 0x64) = 1;
    unsigned pos = i == 0 ? 0x70 : 0x6c;
    GU32(o + pos) = GU32(o + 0x24) * 256;
    GU32(o + pos + 4) = GU32(o + 0x28) * 256;
  }
  // A free slot with entirely plausible live payload must still be excluded.
  GU8(0x604008 + 0x5c) = 1;
  GU32(0x604008 + 0x30) = 0x640000;
  GU32(0x604008 + 0x24) = 100; GU32(0x604008 + 0x28) = 200;
  GU32(0x604008 + 0x38) = 300;
  GU32(0x620008 + 8) = 0x630000;
  // Two embedded rings; the second town proves an invalid first record is local.
  const uint32_t q = 0x620008 + 0xf8;
  GU32(q) = 0x48ff84; GU32(q + 4) = 0x651000; GU32(q + 0x10) = 50;
  GU32(0x620208 + 8) = 0x63069c;
  GU32(q + 0x200) = 0x48ff84; GU32(q + 0x204) = 0x652000; GU32(q + 0x210) = 50;
  GU32(q + 0x20c) = 1; GU32(q + 0x214) = 1; GU32(0x652000) = 0x600308;
  // Plausible image addresses at the vtable must never become queue metadata.
  GU32(0x48ff88) = 0x411d90; GU32(0x48ff8c) = 0x411f90;
  GU32(0x48ff94) = 0x411dd0; GU32(0x48ff98) = 0x411d40;
  GU32(0x630000 + 0x698) = 123;
  const char* test_case = getenv("L0_TEST_CASE");
  if (test_case && (!strcmp(test_case, "pool_padding") || !strcmp(test_case, "pool_tail"))) {
    // 4096-byte pages can end partway through a stride. The last full slot
    // remains usable, and a pointer into the partial slot must be rejected.
    pool_fixture(0x4cc3d0, 0x600000, 184, 155, 63, 0x610000);
    GU32(0x4cc3d0 + 0x1c) = 28672;
    for (uint32_t i = 0; i < 63; ++i) {
      uint32_t o = 0x600008 + i * 184;
      GU32(o) = 0x480000; GU32(o + 0x50) = 27; GU32(o + 0x30) = 0x640000;
      GU8(o + 0x5c) = 1; GU32(o + 0x38) = 100;
      GU32(o + 0x6c) = (i + 1) * 12288; GU32(o + 0x70) = 18432;
    }
    if (!strcmp(test_case, "pool_tail")) GU32(0x4cc3d0 + 8) = 0x600000 + 155 * 184;
  }
  if (test_case && !strcmp(test_case, "unit_steps")) {
    for (uint32_t state = 0; state < 6; ++state) {
      uint32_t o = 0x600108 + state * 0x100;
      GU32(o + 0x64) = state;
      // Deliberately different tile, walking step and attack/firing targets.
      GU32(o + 0x44) = 2; GU32(o + 0x48) = 3;
      GU32(o + 0x74) = 456; GU32(o + 0x78) = 789;
      GU32(o + 0x90) = 0x600008; GU32(o + 0x94) = 0x600208;
      GU32(o + 0x8c) = 0x600308;
      GU32(o + 0x98) = 2371; GU32(o + 0x9c) = (uint32_t)-512;
    }
  }
  if (test_case && !strcmp(test_case, "inactive")) GU8(0x600108 + 0x5c) = 0;
  if (test_case && !strcmp(test_case, "sentinel_live")) {
    GU32(0x600008 + 0x38) = 0; GU32(0x600008 + 0x30) = 0;
    GU32(0x600008 + 0x24) = (uint32_t)-1000; GU32(0x600008 + 0x28) = (uint32_t)-1000;
    GU32(0x600008 + 0x70) = (uint32_t)-256000; GU32(0x600008 + 0x74) = (uint32_t)-256000;
  }
  if (test_case && !strcmp(test_case, "cycle")) GU32(0x604000) = 0x604000;
  if (test_case && !strcmp(test_case, "active_count")) GU32(0x4cc3e8) = 62;
  if (test_case && !strcmp(test_case, "queue")) {
    GU32(q + 8) = 49; GU32(q + 0xc) = 1; GU32(q + 0x14) = 2;
    GU32(0x651000 + 49 * 4) = 0x600108; GU32(0x651000) = 0x600208;
  }
  if (test_case && !strcmp(test_case, "invalid_queue")) GU32(q + 0x14) = 51;
  if (test_case && !strcmp(test_case, "queue_vtable")) GU32(q) = 0;
  if (test_case && !strcmp(test_case, "queue_data")) GU32(q + 4) = 0xfffffff0;
  if (test_case && !strcmp(test_case, "queue_head")) GU32(q + 8) = 50;
  if (test_case && !strcmp(test_case, "queue_tail")) GU32(q + 0xc) = 50;
  if (test_case && !strcmp(test_case, "queue_capacity")) GU32(q + 0x10) = 0;
  if (test_case && !strcmp(test_case, "queue_null")) GU32(q + 0x14) = 1;
  if (test_case && !strcmp(test_case, "queue_object_span")) {
    GU32(q + 0x14) = 1; GU32(0x651000) = 0xfffffff0;
  }
  if (test_case && !strcmp(test_case, "town_pool")) GU32(0x515710) = 3;
  if (test_case && !strcmp(test_case, "town_chain")) GU32(0x620004) = 0;
  if (test_case && !strcmp(test_case, "short_town")) {
    pool_fixture(0x5156f8, 0x620000, 0x100, 1, 1, 0x610008);
  }
  if (test_case && !strcmp(test_case, "invalid_order")) {
    GU8(0x600108 + 0x5c) = 0;
    GU32(0x600108 + 0x50) = 90;
  }
  if (test_case && !strcmp(test_case, "order_tactic")) {
    GU8(0x600108 + 0x5c) = 0;
    GU32(0x600108 + 0x30) = 0xfffffff0;
  }
  if (test_case && !strcmp(test_case, "classes")) {
    const unsigned types[] = {0, 30, 10, 7, 83, 33, 3, 17};
    const unsigned positions[] = {0x6c, 0x78, 0, 0x68, 0x6c, 0x68, 0x70, 0};
    for (unsigned i = 0; i < 8; ++i) {
      uint32_t o = 0x600008 + i * 0x100;
      GU32(o + 0x50) = types[i]; GU32(0x4933f8 + types[i] * 8) = i;
      if (positions[i]) {
        GU32(o + positions[i]) = (72 + i * 48) * 256 + 64;
        GU32(o + positions[i] + 4) = 72 * 256 + 128;
      }
    }
  }
  rt_l0_clock(100000);
  pthread_t thread;
  if (pthread_create(&thread, NULL, renderer, NULL)) return 2;
  for (uint32_t tick = 1; tick <= 12; ++tick) {
    rt_l0_observe(tick);
    GU32(0x600008 + 0x38) -= 1;
    GU32(0x600008 + 0x70) += 64;
  }
  atomic_store(&done, 1);
  pthread_join(thread, NULL);
  free(g_mem);
  return 0;
}
