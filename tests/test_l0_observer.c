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
  GU32(0x4cc3d4) = 0x600000; GU32(0x4cc3d8) = 0x600100; GU32(0x4cc3e0) = 0x100;
  GU32(0x4dd2f4) = 0x601000;
  GU32(0x600008) = 0x480000;
  GU32(0x600008 + 0x30) = 0x603000;
  GU32(0x603008) = 0x601000;
  GU32(0x600008 + 0x50) = 3;
  GU32(0x600008 + 0x24) = 72; GU32(0x600008 + 0x28) = 72;
  GU32(0x600008 + 0x38) = 100;
  GU32(0x601000 + 0x698) = 123;
  rt_l0_clock(100000);
  pthread_t thread;
  if (pthread_create(&thread, NULL, renderer, NULL)) return 2;
  for (uint32_t tick = 1; tick <= 3; ++tick) {
    rt_l0_observe(tick);
    GU32(0x600008 + 0x38) -= 1;
  }
  atomic_store(&done, 1);
  pthread_join(thread, NULL);
  free(g_mem);
  return 0;
}
