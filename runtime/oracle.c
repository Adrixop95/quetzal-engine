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
void pathlog_tick(uint32_t frame);

#define CMD_DWORDS 53
typedef struct { uint32_t frame, side; uint32_t d[CMD_DWORDS]; } Rec;
static Rec* recs; static size_t nrecs, caprecs;
static FILE* logf_, *statef_, *unitf_; static int mode_init, replay;

// RT_STATE_LOG=file: per tick, at the first command fetch of the tick (same thread as the logic, so no race with updates):
// frame number, then per object pool: live count and an FNV hash of the object bytes (host pointers masked).
// RT_UNIT_LOG=file (RT_UNIT_EVERY=n ticks): per tick lines "U frame addr type side x y hp maxhp state" for pool 0x4cc3d0 objects and "S frame side money spent earned".
static void log_units(uint32_t frame) {
  static uint32_t lastu = ~0u; static int every = 0;
  if (!unitf_ || frame == lastu) return;
  if (!every) { const char* e = getenv("RT_UNIT_EVERY"); every = e ? atoi(e) : 1; if (every < 1) every = 1; }
  if (frame % every) return;
  lastu = frame;
  uint32_t pb = 0x4cc3d0, base = GU32(pb + 4), hwm = GU32(pb + 8), stride = GU32(pb + 0x10), sb = GU32(0x4dd2f4);
  for (uint32_t a = base; base && hwm > base && stride >= 16 && a + stride <= hwm; a += stride) {
    uint32_t vt = GU32(a + 8);
    if (vt < 0x480000 || vt >= 0x4c4000) continue;
    uint32_t o = a + 8, sp = GU32(o + 0x30);
    int side = (sb && sp >= sb && sp < sb + 16 * 0x69c) ? (int)((sp - sb) / 0x69c) : -1;
    fprintf(unitf_, "U %u %x %d %d %d %d %d %d %d\n", frame, a, (int)GU32(o + 0x50), side, (int)GU32(o + 0x24), (int)GU32(o + 0x28), (int)GU32(o + 0x38), (int)GU32(o + 0x3c), (int)GU32(o + 0x64));
  }
  for (int i = 0; i < 4 && sb; i++) fprintf(unitf_, "S %u %d %d %d %d\n", frame, i, (int)GU32(sb + i * 0x69c + 0x698), (int)GU32(sb + i * 0x69c + 0x640), (int)GU32(sb + i * 0x69c + 0x63c));
  fflush(unitf_);
}

static void log_state(uint32_t frame) {
  static uint32_t last = ~0u;
  log_units(frame);
  if (!statef_ || frame == last) return;
  last = frame;
  fprintf(statef_, "%u", frame);
  static const uint32_t pools[4] = {0x4cc3d0, 0x4cd698, 0x5156f8, 0x4c1978};
  for (int pi = 0; pi < 4; pi++) {
    uint32_t pb = pools[pi], base = GU32(pb + 4), hwm = GU32(pb + 8), stride = GU32(pb + 0x10);
    uint64_t h = 1469598103934665603ull; unsigned live = 0;
    if (base && hwm > base && stride >= 16 && stride <= 0x400)
      for (uint32_t a = base; a + stride <= hwm; a += stride) {
        uint32_t vt = GU32(a + 8);
        if (vt < 0x480000 || vt >= 0x4c4000) continue;
        live++;
        for (uint32_t k = 8; k < stride; k += 4) {
          uint32_t v = GU32(a + k);
          if (v >= 0x10000000u && v < 0x70000000u) v = 0xdeadbeef;
          h = (h ^ v) * 1099511628211ull;
        }
      }
    fprintf(statef_, " %u:%016llx", live, (unsigned long long)h);
  }
  fputc('\n', statef_); fflush(statef_);
}

static void init_mode(void) {
  mode_init = 1;
  const char* sl = getenv("RT_STATE_LOG");
  if (sl) statef_ = fopen(sl, "w");
  const char* ul = getenv("RT_UNIT_LOG");
  if (ul) unitf_ = fopen(ul, "w");
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
  if (frame != 0 && GU8(0x4cc394)) { log_state(frame); pathlog_tick(frame); }
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
