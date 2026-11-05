// Hand-written observation plumbing; no lifted functions or generated source.
#include "aztec_rt.h"
#include "l0_observer.h"
#include "../src/parity/schedule.h"
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>

static L0Schedule schedule;
static FILE* output;
static int initialized, pending, captured;
static uint32_t last_tick;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static _Atomic int active;
static _Atomic uint32_t frozen_ms;
static _Atomic uint32_t latest_ms;
static _Atomic uint32_t paused_ms;
static char frame_path[1024];

uint32_t rt_l0_clock(uint32_t wall_ms) {
  wall_ms -= atomic_load(&paused_ms);
  atomic_store(&latest_ms, wall_ms);
  uint32_t frozen = atomic_load(&frozen_ms);
  return frozen ? frozen : wall_ms;
}

void rt_l0_present(const void* pixels, int pitch, int width, int height, int bpp, const uint32_t* palette) {
  if (!atomic_load(&active)) return;
  pthread_mutex_lock(&lock);
  if (pending && !captured && width == 800 && height == 600 &&
      (bpp == 16 || bpp == 32 || (bpp == 8 && palette)) && pitch >= width * (bpp / 8)) {
    FILE* f = fopen(frame_path, "wb");
    if (!f) rt_fatal("L0 cannot open frame");
    fprintf(f, "P6\n800 600\n255\n");
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
      const uint8_t* p = (const uint8_t*)pixels + y * pitch + x * (bpp / 8);
      uint32_t color = 0;
      if (bpp == 32) memcpy(&color, p, 4);
      else if (bpp == 8) color = palette[*p];
      else {
        uint16_t v;
        memcpy(&v, p, 2);
        unsigned r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
        color = ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
      }
      uint8_t rgb[3] = {color >> 16, color >> 8, color};
      if (fwrite(rgb, 1, 3, f) != 3) rt_fatal("L0 cannot write frame");
    }
    if (fclose(f)) rt_fatal("L0 cannot finish frame");
    captured = 1;
    pthread_cond_signal(&ready);
  }
  pthread_mutex_unlock(&lock);
}

void rt_l0_observe(uint32_t tick) {
  if (!initialized) {
    initialized = 1;
    const char* path = getenv("RT_L0_SCHEDULE");
    if (!path) return;
    if (!l0_schedule_read(path, &schedule)) rt_fatal("Invalid L0 schedule");
    path = getenv("RT_L0_OUTPUT");
    if (!path || !(output = fopen(path, "w"))) rt_fatal("Cannot open L0 observations");
    if (!getenv("RT_DUMP_DIR")) rt_fatal("L0 requires RT_DUMP_DIR");
    atomic_store(&active, 1);
    fputs("{\"schema\":2,\"phase\":\"before_commands\",\"position_unit\":\"pixel/256\"}\n", output);
  }
  if (!output || tick == last_tick) return;
  last_tick = tick;
  if (schedule.next == schedule.count) { fflush(NULL); _exit(0); }
  if (tick < schedule.ticks[schedule.next]) return;
  if (tick != schedule.ticks[schedule.next]) rt_fatal("L0 skipped requested tick");
  // Stop the logic thread at its command boundary. The renderer may still present.
  uint32_t clock = atomic_load(&latest_ms);
  if (!clock) rt_fatal("L0 clock has not started");
  atomic_store(&frozen_ms, clock);
  GU32(0x4dd330) = (uint32_t)schedule.x;
  GU32(0x4dd334) = (uint32_t)schedule.y;
  pthread_mutex_lock(&lock);
  int n = snprintf(frame_path, sizeof frame_path, "%s/tick_%u.ppm", getenv("RT_DUMP_DIR"), tick);
  if (n < 0 || (size_t)n >= sizeof frame_path) rt_fatal("L0 frame path too long");
  pending = 1; captured = 0;
  struct timespec until;
  clock_gettime(CLOCK_REALTIME, &until);
  until.tv_sec += 5;
  while (!captured) {
    if (pthread_cond_timedwait(&ready, &lock, &until)) break;
  }
  pending = 0;
  int have_frame = captured;
  pthread_mutex_unlock(&lock);
  fprintf(output, "{\"tick\":%u,\"objects\":[", tick);
  uint32_t base = GU32(0x4cc3d4), end = GU32(0x4cc3d8), stride = GU32(0x4cc3e0), sides = GU32(0x4dd2f4);
  if (!base || end < base || stride < 0x70 || stride > 0x400 || (end - base) % stride || (end - base) / stride > 100000)
    rt_fatal("Invalid L0 object pool");
  int first = 1;
  for (uint32_t a = base; a < end; a += stride) {
    uint32_t o = a + 8, vt = GU32(o);
    if (vt < 0x480000 || vt >= 0x4c4000) continue;
    uint32_t tactic = GU32(o + 0x30), side = tactic ? GU32(tactic + 8) : 0;
    int owner = -1;
    if (sides && side >= sides && side < sides + 4 * 0x69c && (side - sides) % 0x69c == 0)
      owner = (int)((side - sides) / 0x69c);
    fprintf(output, "%s{\"handle\":%u,\"type\":%d,\"owner\":%d,\"x\":%lld,\"y\":%lld,\"hp\":%d,\"orders\":null}",
            first ? "" : ",", a, (int)GU32(o + 0x50), owner, (long long)(int32_t)GU32(o + 0x24) * 256,
            (long long)(int32_t)GU32(o + 0x28) * 256, (int)GU32(o + 0x38));
    first = 0;
  }
  fputs("],\"money\":{", output);
  if (!sides) rt_fatal("Missing L0 side storage");
  for (int i = 0; i < 4; ++i)
    fprintf(output, "%s\"%d\":%d", i ? "," : "", i, (int)GU32(sides + i * 0x69c + 0x698));
  fprintf(output, "},\"queues\":null,\"view\":null,\"frame\":");
  if (have_frame) fprintf(output, "\"reference/tick_%u.ppm\"", tick);
  else fputs("null", output);
  // Raw DirectDraw presents can be partial blits. Never certify their phase or UI.
  fprintf(output, ",\"unavailable\":[\"orders\",\"queues\",\"position_fraction\",\"view\",\"frame_phase\"%s]}\n", have_frame ? "" : ",\"frame\"");
  if (fflush(output) || ferror(output)) rt_fatal("L0 observation write failed");
  ++schedule.next;
  uint32_t frozen = atomic_load(&frozen_ms);
  atomic_fetch_add(&paused_ms, atomic_load(&latest_ms) - frozen);
  atomic_store(&frozen_ms, 0);
}
