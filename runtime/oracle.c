// Command recorder / replayer for the lockstep tick (differential testing oracle).
//   RT_CMD_LOG=file     record every command the tick reads (frame, side, 53 dwords)
//   RT_CMD_REPLAY=file  feed recorded commands to the tick instead of the live ones (all other frames: PCT_FRAME_NOTHING)
// Hook: the tick (0x4577b0) calls 0x43d120(side, out) per side and frame; commands are 0xd4 bytes, type at dword 2.
#include "aztec_rt.h"
#include "dbg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void f_0043d120_orig(CPU* c);

#define CMD_DWORDS 53
typedef struct { uint32_t frame, side; uint32_t d[CMD_DWORDS]; } Rec;
static Rec* recs; static size_t nrecs, caprecs;
static FILE* logf_; static int mode_init, replay;

static void init_mode(void) {
  mode_init = 1;
  const char* l = getenv("RT_CMD_LOG");
  if (l) logf_ = fopen(l, "w");
  const char* r = getenv("RT_CMD_REPLAY");
  if (r) {
    FILE* f = fopen(r, "r");
    if (f) {
      char line[2048];
      while (fgets(line, sizeof line, f)) {
        Rec x; char* p = line; char* e;
        x.frame = (uint32_t)strtoul(p, &e, 10); p = e; x.side = (uint32_t)strtoul(p, &e, 10); p = e;
        for (int i = 0; i < CMD_DWORDS; i++) { x.d[i] = (uint32_t)strtoul(p, &e, 16); p = e; }
        if (nrecs == caprecs) { caprecs = caprecs ? caprecs * 2 : 1024; recs = realloc(recs, caprecs * sizeof *recs); }
        recs[nrecs++] = x;
      }
      fclose(f); replay = 1;
      DBG("oracle", "replaying %zu recorded commands", nrecs);
    }
  }
}

static const Rec* find_rec(uint32_t frame, uint32_t side) {
  size_t lo = 0, hi = nrecs;   // records are in log order = non-decreasing frame
  while (lo < hi) { size_t m = (lo + hi) / 2; if (recs[m].frame < frame) lo = m + 1; else hi = m; }
  for (size_t i = lo; i < nrecs && recs[i].frame == frame; i++) if (recs[i].side == side) return &recs[i];
  return NULL;
}

void f_0043d120(CPU* c) {   // uint __cdecl FUN_0043d120(int side, uint32_t* out)
  if (!mode_init) init_mode();
  uint32_t side = GU32(GREG(c, 0x10) + 4), out = GU32(GREG(c, 0x10) + 8);
  uint32_t frame = GU32(0x4c5730);
  if (replay) {
    uint32_t ret = GU32(GREG(c, 0x10));
    const Rec* r = find_rec(frame, side);
    if (frame != 0 && GU8(0x4cc394)) {
      if (r) memcpy(GPTR(out), r->d, sizeof r->d);
      else { memset(GPTR(out), 0, CMD_DWORDS * 4); GU32(out + 8) = 0x25; GU32(out + 12) = frame; }   // PCT_FRAME_NOTHING
      *(uint32_t*)(c->r + 0x284) = ret; GREG(c, 0x10) += 4; GREG(c, 0x00) = 1;
      return;
    }
  }
  f_0043d120_orig(c);
  if (logf_ && (GREG(c, 0x00) & 0xff) && frame != 0 && GU32(out + 8) != 0x25) {
    fprintf(logf_, "%u %u", frame, side);
    for (int i = 0; i < CMD_DWORDS; i++) fprintf(logf_, " %x", GU32(out + 4 * i));
    fputc('\n', logf_); fflush(logf_);
  }
}
