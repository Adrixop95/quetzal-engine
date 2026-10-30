// Host-only playback requests. Buffer handles are process-local, never sound IDs.
#ifndef AZTEC_AUDIO_TRACE_H
#define AZTEC_AUDIO_TRACE_H
#include <stdint.h>
#include <stdio.h>

static int rt_audio_write(FILE* file, uint64_t sequence, uint32_t tick,
                          uint32_t buffer, const char* kind, uint32_t flags) {
  return fprintf(file, "{\"seq\":%llu,\"tick\":%u,\"buffer\":%u,\"kind\":\"%s\",\"flags\":%u,\"sound\":null,\"trigger\":\"unknown\"}\n",
      (unsigned long long)sequence, tick, buffer, kind, flags) >= 0 && fflush(file) == 0;
}
#endif
