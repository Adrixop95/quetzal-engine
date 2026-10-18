// Movie playback for the game's MCI AVI calls. Decoding is done by an external `ffmpeg` process
// (no development headers needed). Without ffmpeg, or with RT_NO_VIDEO set, movies are skipped.
#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "aztec_rt.h"
#include "dbg.h"
#include "backend.h"
#include "settings.h"

extern int rt_time_scale;

static int shell_quote(char* out, size_t n, const char* s) {
  size_t k = 0;
  out[k++] = '\'';
  for (; *s && k + 5 < n; s++) {
    if (*s == '\'') { memcpy(out + k, "'\\''", 4); k += 4; } else out[k++] = *s;
  }
  out[k++] = '\'';
  out[k] = 0;
  return (int)k;
}

typedef struct { FILE* pipe; SDL_AudioDeviceID dev; volatile int stop; } AudioFeed;

static void* audio_feeder(void* arg) {
  AudioFeed* a = arg;
  uint8_t buf[8192];
  size_t n;
  while (!a->stop && (n = fread(buf, 1, sizeof buf, a->pipe)) > 0) {
    SDL_QueueAudio(a->dev, buf, (Uint32)n);
    while (!a->stop && SDL_GetQueuedAudioSize(a->dev) > 44100 * 2) SDL_Delay(20);  // keep ~1 s buffered
  }
  return NULL;
}

static int skip_requested(void) {
  return g_dik_down[1] || g_dik_down[57] || g_dik_down[28] || g_mouse_btn[0] || g_mouse_btn[1];
}

int rt_video_play(const char* host_path, uint32_t w, uint32_t h, uint32_t us_per_frame, uint32_t notify_hwnd) {
  (void)notify_hwnd;
  DBG("video", "play request: %s %ux%u us/frame=%u (video enabled=%d, timescale=%d)", host_path, w, h, us_per_frame, g_settings.video, rt_time_scale);
  if (!g_settings.video || rt_time_scale > 1) { rt_log("movie: %s (skipped)", host_path); return 1; }
  if (us_per_frame < 10000 || us_per_frame > 200000) us_per_frame = 83333;
  char q[2200], cmd[2600];
  shell_quote(q, sizeof q, host_path);
  snprintf(cmd, sizeof cmd, "ffmpeg -v error -nostdin -i %s -an -f rawvideo -pix_fmt bgra - 2>/dev/null", q);
  FILE* vp = popen(cmd, "r");
  if (!vp) { rt_log("movie: cannot start ffmpeg"); return 1; }
  AudioFeed af = {0};
  pthread_t at = 0;
  snprintf(cmd, sizeof cmd, "ffmpeg -v error -nostdin -i %s -vn -f s16le -ar 22050 -ac 1 - 2>/dev/null", q);
  FILE* ap = popen(cmd, "r");
  if (ap) {
    if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_AudioSpec want = {0}, have;
    want.freq = 22050; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024;
    af.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (af.dev) {
      af.pipe = ap;
      SDL_PauseAudioDevice(af.dev, 0);
      pthread_create(&at, NULL, audio_feeder, &af);
    }
  }
  DBG("video", "ffmpeg started (video pipe %s, audio pipe %s)", vp ? "ok" : "FAILED", ap ? "ok" : "FAILED");
  size_t fsz = (size_t)w * h * 4;
  uint8_t* frame = malloc(fsz);
  struct timespec t0;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  uint64_t i = 0;
  int skipped = 0;
  while (fread(frame, 1, fsz, vp) == fsz) {
    backend_present(frame, (int)w * 4, (int)w, (int)h, 32, NULL);
    rt_pump_events();
    if (skip_requested()) { skipped = 1; break; }
    i++;
    struct timespec now;
    do {
      clock_gettime(CLOCK_MONOTONIC, &now);
      int64_t el_us = (int64_t)(now.tv_sec - t0.tv_sec) * 1000000 + (now.tv_nsec - t0.tv_nsec) / 1000;
      int64_t wait = (int64_t)i * us_per_frame - el_us;
      if (wait <= 0) break;
      usleep(wait > 5000 ? 5000 : (useconds_t)wait);
      rt_pump_events();
    } while (!skip_requested());
  }
  DBG("video", "playback finished: %llu frames shown, skipped by user=%d", (unsigned long long)i, skipped);
  free(frame);
  pclose(vp);
  if (at) { af.stop = 1; pthread_join(at, NULL); }
  if (af.dev) { SDL_ClearQueuedAudio(af.dev); SDL_CloseAudioDevice(af.dev); }
  if (ap) pclose(ap);
  if (skipped) { SDL_Delay(150); while (skip_requested()) { rt_pump_events(); SDL_Delay(10); } }   // wait for key release
  return 1;
}
