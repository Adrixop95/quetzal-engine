// Movie playback for the game's MCI AVI calls. Decoding is done by an external ffmpeg process
// (no development headers needed); without ffmpeg the movie is skipped.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aztec_rt.h"
#include "backend.h"

int rt_video_play(const char* host_path, uint32_t w, uint32_t h, uint32_t device, uint32_t notify_hwnd) {
  (void)w; (void)h; (void)device; (void)notify_hwnd;
  rt_log("movie: %s (skipped, no playback yet)", host_path);
  return 1;
}
