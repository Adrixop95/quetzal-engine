// Shared, asset-independent transport for the L0 observation schedule.
#ifndef AZTEC_PARITY_SCHEDULE_H
#define AZTEC_PARITY_SCHEDULE_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  int x, y, side;
  uint32_t* ticks;
  size_t count, next;
} L0Schedule;

static inline int l0_schedule_read(const char* path, L0Schedule* out) {
  FILE* f = fopen(path, "r");
  char line[128];
  L0Schedule s = {0, 0, 0, NULL, 0, 0};
  if (!f) return 0;
  if (!fgets(line, sizeof line, f) || strncmp(line, "L0 2 ", 5)) goto fail;
  {
    char* cursor = line + 5;
    int* values[3] = {&s.x, &s.y, &s.side};
    for (int i = 0; i < 3; ++i) {
      char* end;
      if (*cursor < '0' || *cursor > '9') goto fail;
      unsigned long v = strtoul(cursor, &end, 10);
      if (v > INT32_MAX || *end != (i == 2 ? '\n' : ' ') || (i == 2 && end[1])) goto fail;
      *values[i] = (int)v;
      cursor = end + 1;
    }
    if (s.side >= 4) goto fail;
  }
  while (fgets(line, sizeof line, f)) {
    char* end;
    if (s.count >= 100000 || line[0] < '1' || line[0] > '9') goto fail;
    unsigned long tick = strtoul(line, &end, 10);
    if (strcmp(end, "\n") || tick > INT32_MAX || (!s.count && tick != 1) || (s.count && tick <= s.ticks[s.count - 1])) goto fail;
    uint32_t* resized = (uint32_t*)realloc(s.ticks, (s.count + 1) * sizeof *s.ticks);
    if (!resized) goto fail;
    s.ticks = resized;
    s.ticks[s.count++] = (uint32_t)tick;
  }
  if (ferror(f) || !s.count) goto fail;
  fclose(f);
  *out = s;
  return 1;
fail:
  free(s.ticks);
  fclose(f);
  return 0;
}
#endif
