// Win32 API shims (KERNEL32, USER32, GDI32, ADVAPI32, WINMM) for the recompiled game.
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "aztec_rt.h"
#include "backend.h"

// ------------------------------------------------------------ thread state
typedef struct ThreadInfo {
  uint32_t tls[64];
  uint32_t last_error;
  uint32_t tid;
  uint32_t teb;
} ThreadInfo;

static ThreadInfo main_ti;
static __thread ThreadInfo* cur_ti;
static uint32_t tls_next;
static uint32_t next_tid = 1000;
static char game_dir[1024];
static char exe_path_g[1024];
static struct timespec t_start;

static ThreadInfo* TI(CPU* c) { return (ThreadInfo*)c->thread; }
static void set_err(CPU* c, uint32_t e) { TI(c)->last_error = e; }

// ------------------------------------------------------------ path mapping
// Windows path -> host path: backslashes to '/', drive letters dropped, case-insensitive match.
static int ci_resolve(const char* in, char* out, size_t outsz) {
  char tmp[1024];
  size_t n = 0;
  const char* p = in;
  if (isalpha((unsigned char)p[0]) && p[1] == ':') p += 2;
  for (; *p && n < sizeof tmp - 1; p++) tmp[n++] = *p == '\\' ? '/' : *p;
  tmp[n] = 0;
  char cur[1024];
  size_t cl = 0;
  const char* s = tmp;
  while (*s == '/') s++;                      // guest paths are Windows paths: \\x or D:\\x means "relative to the game directory"
  cur[0] = '.'; cl = 1;
  cur[cl] = 0;
  while (*s) {
    char comp[256];
    size_t k = 0;
    while (*s && *s != '/' && k < sizeof comp - 1) comp[k++] = *s++;
    comp[k] = 0;
    while (*s == '/') s++;
    if (!k || !strcmp(comp, ".")) continue;
    char cand[1100];
    snprintf(cand, sizeof cand, "%s/%s", cur, comp);
    struct stat st;
    if (stat(cand, &st) != 0) {
      DIR* d = opendir(cur);
      int found = 0;
      if (d) {
        struct dirent* e;
        while ((e = readdir(d))) if (!strcasecmp(e->d_name, comp)) { snprintf(cand, sizeof cand, "%s/%s", cur, e->d_name); found = 1; break; }
        closedir(d);
      }
      if (!found) { snprintf(out, outsz, "%s/%s", cur, comp); if (*s) { strncat(out, "/", outsz - strlen(out) - 1); strncat(out, s, outsz - strlen(out) - 1); } return 0; }
    }
    snprintf(cur, sizeof cur, "%s", cand);
  }
  snprintf(out, outsz, "%s", cur);
  return 1;
}

// ----------------------------------------------------------- kernel objects
typedef enum { H_NONE, H_FILE, H_EVENT, H_MUTEX, H_SEM, H_THREAD, H_MAP, H_FIND } HType;
typedef struct {
  HType t;
  int fd;                       // file / mapping
  int signaled, manual;         // event
  unsigned pulse_gen;           // PulseEvent generation: wakes threads that were already waiting
  int count, max;               // semaphore
  int owner_depth;              // mutex
  pthread_t th; int done; uint32_t exit_code; int suspended;  // thread
  uint64_t size;
  DIR* dir; char pattern[260]; char dirpath[1024];  // find
} Obj;
#define MAX_OBJ 4096
static Obj objs[MAX_OBJ];
static pthread_mutex_t objlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t objcv = PTHREAD_COND_INITIALIZER;

static uint32_t obj_new(HType t) {
  pthread_mutex_lock(&objlock);
  for (int i = 8; i < MAX_OBJ; i++)
    if (objs[i].t == H_NONE) { memset(&objs[i], 0, sizeof objs[i]); objs[i].t = t; objs[i].fd = -1; if (getenv("RT_TRACE_WAIT")) rt_log("obj_new type=%d -> %08x", (int)t, 4u * i); pthread_mutex_unlock(&objlock); return 4u * i; }
  pthread_mutex_unlock(&objlock);
  return 0;
}
static Obj* obj_get(uint32_t h) { uint32_t i = h / 4; return (h % 4 == 0 && i >= 8 && i < MAX_OBJ && objs[i].t != H_NONE) ? &objs[i] : NULL; }

// -------------------------------------------------------------- critical sections
static pthread_mutex_t cs_pool[1024];
static int cs_used = 1;
static pthread_mutex_t* cs_get(uint32_t cs) {
  uint32_t idx = GU32(cs + 4);  // LockSemaphore field reused as pool index
  return idx ? &cs_pool[idx] : NULL;
}

// ============================================================= KERNEL32
#define SH(n) static uint32_t sh_##n(CPU* c, uint32_t* a)
#define UNUSED (void)c; (void)a

SH(GetLastError) { UNUSED; return TI(c)->last_error; }
SH(SetLastError) { UNUSED; TI(c)->last_error = a[0]; return 0; }
SH(GetVersion) { UNUSED; return 0x0A280105; }
SH(GetVersionExA) {
  UNUSED;
  uint32_t p = a[0], sz = GU32(p);
  if (sz != 148 && sz != 156 && sz != 276) { set_err(c, 87); return 0; }  // ERROR_INVALID_PARAMETER like Windows
  memset(GPTR(p + 4), 0, sz - 4);
  GU32(p + 4) = 5; GU32(p + 8) = 1; GU32(p + 12) = 2600; GU32(p + 16) = 2;
  return 1;
}
SH(GetCommandLineA) { UNUSED; static uint32_t s; if (!s) s = guest_cstring("Aztec.exe"); return s; }
SH(GetStartupInfoA) { UNUSED; memset(GPTR(a[0]), 0, 68); GU32(a[0]) = 68; return 0; }
SH(GetModuleHandleA) { UNUSED; return a[0] ? 0 : 0x400000; }
SH(GetModuleFileNameA) {
  UNUSED;
  char buf[1100];
  snprintf(buf, sizeof buf, "C:\\Aztec\\Aztec.exe");
  uint32_t n = (uint32_t)strlen(buf);
  if (n + 1 > a[2]) n = a[2] - 1;
  memcpy(GPTR(a[1]), buf, n); GU8(a[1] + n) = 0;
  return n;
}
SH(GetProcAddress) {
  UNUSED;
  const char* name = (const char*)GPTR(a[1]);
  if (a[1] < 0x10000) return 0;
  uint32_t t = rt_shim_thunk("KERNEL32.dll", name);
  return t;
}
SH(LoadLibraryA) { UNUSED; rt_log("LoadLibraryA(%s) -> fail", (char*)GPTR(a[0])); set_err(c, 126); return 0; }
SH(LoadLibraryExA) { UNUSED; rt_log("LoadLibraryExA(%s,%u) -> fail", (char*)GPTR(a[0]), a[2]); set_err(c, 126); return 0; }
SH(FreeLibrary) { UNUSED; return 1; }
SH(ExitProcess) { UNUSED; fflush(NULL); _exit((int)a[0]); }
SH(TerminateProcess) { UNUSED; fflush(NULL); _exit((int)a[1]); }
SH(GetCurrentProcess) { UNUSED; return 0xFFFFFFFF; }
SH(GetCurrentThread) { UNUSED; return 0xFFFFFFFE; }
SH(GetCurrentThreadId) { UNUSED; return TI(c)->tid; }
SH(SetThreadPriority) { UNUSED; return 1; }
SH(GetACP) { UNUSED; return 1252; }
SH(GetOEMCP) { UNUSED; return 437; }
SH(GetCPInfo) { UNUSED; memset(GPTR(a[1]), 0, 20); GU32(a[1]) = 1; GU8(a[1] + 4) = '?'; return 1; }
SH(IsBadReadPtr) { UNUSED; return 0; }
SH(IsBadWritePtr) { UNUSED; return 0; }
SH(IsBadCodePtr) { UNUSED; return 0; }
SH(SetUnhandledExceptionFilter) { UNUSED; return 0; }
SH(UnhandledExceptionFilter) { UNUSED; rt_fatal("UnhandledExceptionFilter"); }
SH(RaiseException) { UNUSED; rt_fatal("RaiseException %08x", a[0]); }
SH(RtlUnwind) { UNUSED; rt_fatal("RtlUnwind"); }
SH(InterlockedIncrement) { UNUSED; return ++GU32(a[0]); }
SH(InterlockedDecrement) { UNUSED; return --GU32(a[0]); }

SH(GetSystemInfo) {
  UNUSED;
  memset(GPTR(a[0]), 0, 36);
  GU32(a[0] + 4) = 4096; GU32(a[0] + 8) = 0x10000; GU32(a[0] + 12) = 0x7ffeffff;
  GU32(a[0] + 20) = 1; GU32(a[0] + 24) = 586; GU32(a[0] + 28) = 0x10000;
  return 0;
}
SH(GetComputerNameA) { UNUSED; strcpy((char*)GPTR(a[0]), "AZTEC"); GU32(a[1]) = 5; return 1; }
SH(GetUserNameA) { UNUSED; strcpy((char*)GPTR(a[0]), "player"); GU32(a[1]) = 7; return 1; }

// time
int rt_time_scale = 1;   // RT_TIMESCALE: run game time faster than real time (tests only)
static uint32_t tick_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t ms = (uint64_t)(ts.tv_sec - t_start.tv_sec) * 1000 + (uint64_t)(ts.tv_nsec - t_start.tv_nsec) / 1000000;
  return (uint32_t)(ms * (uint64_t)rt_time_scale) + 100000;
}
uint32_t rt_game_ms(void) { return tick_ms(); }
SH(GetTickCount) { UNUSED; return tick_ms(); }
SH(timeGetTime) { UNUSED; return tick_ms(); }
SH(Sleep) { UNUSED; rt_pump_events(); usleep(a[0] * 1000u / (uint32_t)rt_time_scale); return 0; }
static void fill_systemtime(uint32_t p, int local) {
  time_t t = time(NULL);
  struct tm tm;
  if (local) localtime_r(&t, &tm); else gmtime_r(&t, &tm);
  GU16(p) = (uint16_t)(tm.tm_year + 1900); GU16(p + 2) = (uint16_t)(tm.tm_mon + 1); GU16(p + 4) = (uint16_t)tm.tm_wday;
  GU16(p + 6) = (uint16_t)tm.tm_mday; GU16(p + 8) = (uint16_t)tm.tm_hour; GU16(p + 10) = (uint16_t)tm.tm_min;
  GU16(p + 12) = (uint16_t)tm.tm_sec; GU16(p + 14) = 0;
}
SH(GetSystemTime) { UNUSED; fill_systemtime(a[0], 0); return 0; }
SH(GetLocalTime) { UNUSED; fill_systemtime(a[0], 1); return 0; }
SH(GetFileTime) { UNUSED; for (int i = 1; i < 4; i++) if (a[i]) { GU32(a[i]) = 0; GU32(a[i] + 4) = 0; } return 1; }

// heap / memory
SH(HeapCreate) { UNUSED; return 0x00010000; }
SH(HeapDestroy) { UNUSED; return 1; }
SH(GetProcessHeap) { UNUSED; return 0x00010000; }
SH(HeapAlloc) {
  UNUSED;
  uint32_t p = gmalloc(a[2]);
  if (getenv("RT_TRACE_HEAP")) rt_log("HeapAlloc(%u) -> %08x", a[2], p);
  if (p && (a[1] & 8)) memset(GPTR(p), 0, a[2]);
  return p;
}
SH(HeapFree) { UNUSED; gfree(a[2]); return 1; }
SH(HeapSize) { UNUSED; return gmsize(a[2]); }
SH(HeapReAlloc) { UNUSED; return gmrealloc(a[2], a[3]); }
SH(GlobalAlloc) {
  UNUSED;
  uint32_t p = gmalloc(a[1]);
  if (p && (a[0] & 0x40)) memset(GPTR(p), 0, a[1]);
  return p;
}
SH(GlobalLock) { UNUSED; return a[0]; }
SH(GlobalUnlock) { UNUSED; return 0; }
static uint32_t vtop = 0x50000000u;
SH(VirtualAlloc) {
  UNUSED;
  uint32_t size = (a[1] + 0xfff) & ~0xfffu;
  uint32_t addr = a[0];
  if (!addr) { addr = vtop; vtop += size; }
  if (getenv("RT_TRACE_API")) rt_log("  VirtualAlloc(%08x, %u, type=%08x) -> %08x", a[0], a[1], a[2], addr);
  return addr;  // guest memory is already zero-filled and committed
}
SH(VirtualFree) { UNUSED; return 1; }
SH(GetEnvironmentStrings) { UNUSED; static uint32_t s; if (!s) { s = gmalloc(8); memset(GPTR(s), 0, 8); } return s; }
SH(GetEnvironmentStringsW) { UNUSED; static uint32_t s; if (!s) { s = gmalloc(8); memset(GPTR(s), 0, 8); } return s; }
SH(FreeEnvironmentStringsA) { UNUSED; return 1; }
SH(FreeEnvironmentStringsW) { UNUSED; return 1; }
SH(SetHandleCount) { UNUSED; return a[0]; }
SH(GetStdHandle) { UNUSED; return 0x10 + (a[0] & 7); }
SH(SetStdHandle) { UNUSED; return 1; }
SH(GetFileType) { UNUSED; return a[0] >= 0x10 && a[0] < 0x18 ? 2 : 1; }

// TLS
SH(TlsAlloc) { UNUSED; return tls_next < 64 ? tls_next++ : 0xFFFFFFFF; }
SH(TlsSetValue) { UNUSED; TI(c)->tls[a[0] & 63] = a[1]; return 1; }
SH(TlsGetValue) { UNUSED; return TI(c)->tls[a[0] & 63]; }

// critical sections
SH(InitializeCriticalSection) {
  UNUSED;
  if (cs_used >= 1024) rt_fatal("too many critical sections");
  pthread_mutexattr_t at;
  pthread_mutexattr_init(&at);
  pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&cs_pool[cs_used], &at);
  GU32(a[0] + 4) = cs_used++;
  return 0;
}
SH(DeleteCriticalSection) { UNUSED; return 0; }
SH(EnterCriticalSection) { UNUSED; pthread_mutex_t* m = cs_get(a[0]); if (m) pthread_mutex_lock(m); return 0; }
SH(LeaveCriticalSection) { UNUSED; pthread_mutex_t* m = cs_get(a[0]); if (m) pthread_mutex_unlock(m); return 0; }

// events / mutexes / semaphores / waiting
SH(CreateEventA) {
  UNUSED;
  uint32_t h = obj_new(H_EVENT);
  Obj* o = obj_get(h);
  o->manual = a[1]; o->signaled = a[2];
  return h;
}
SH(OpenEventA) { UNUSED; return 0; }
SH(SetEvent) { UNUSED; if (getenv("RT_TRACE_WAIT")) rt_log("SetEvent tid=%u h=%08x", TI(c)->tid, a[0]); Obj* o = obj_get(a[0]); if (!o) return 0; pthread_mutex_lock(&objlock); o->signaled = 1; pthread_cond_broadcast(&objcv); pthread_mutex_unlock(&objlock); return 1; }
SH(ResetEvent) { UNUSED; Obj* o = obj_get(a[0]); if (!o) return 0; pthread_mutex_lock(&objlock); o->signaled = 0; pthread_mutex_unlock(&objlock); return 1; }
SH(PulseEvent) { UNUSED; Obj* o = obj_get(a[0]); if (!o) return 0; pthread_mutex_lock(&objlock); o->pulse_gen++; pthread_cond_broadcast(&objcv); pthread_mutex_unlock(&objlock); return 1; }
SH(CreateMutexA) { UNUSED; uint32_t h = obj_new(H_MUTEX); return h; }
SH(ReleaseMutex) { UNUSED; return 1; }
SH(CreateSemaphoreA) { UNUSED; uint32_t h = obj_new(H_SEM); Obj* o = obj_get(h); o->count = (int)a[1]; o->max = (int)a[2]; return h; }
SH(ReleaseSemaphore) {
  UNUSED;
  Obj* o = obj_get(a[0]);
  if (!o) return 0;
  pthread_mutex_lock(&objlock);
  if (a[2]) GU32(a[2]) = (uint32_t)o->count;
  o->count += (int)a[1];
  pthread_cond_broadcast(&objcv);
  pthread_mutex_unlock(&objlock);
  return 1;
}
static int obj_ready_locked(Obj* o) {  // consume if ready
  switch (o->t) {
    case H_EVENT: if (o->signaled) { if (!o->manual) o->signaled = 0; return 1; } return 0;
    case H_SEM: if (o->count > 0) { o->count--; return 1; } return 0;
    case H_THREAD: return o->done;
    case H_MUTEX: return 1;
    default: return 1;
  }
}
void rt_signal_event(uint32_t h) {
  Obj* o = obj_get(h);
  if (!o) return;
  pthread_mutex_lock(&objlock);
  o->signaled = 1;
  pthread_cond_broadcast(&objcv);
  pthread_mutex_unlock(&objlock);
}
static uint32_t wait_objs(uint32_t n, const uint32_t* hs, int all, uint32_t ms, int msgs) {
  if (ms != 0xFFFFFFFFu && rt_time_scale > 1) ms = ms / (uint32_t)rt_time_scale;
  struct timespec dl;
  clock_gettime(CLOCK_REALTIME, &dl);
  uint64_t add = ms == 0xFFFFFFFFu ? 0 : ms;
  dl.tv_sec += add / 1000; dl.tv_nsec += (add % 1000) * 1000000; if (dl.tv_nsec >= 1000000000) { dl.tv_sec++; dl.tv_nsec -= 1000000000; }
  uint32_t result = 0x102;
  unsigned gens[64];
  for (uint32_t i = 0; i < n && i < 64; i++) { Obj* o = obj_get(hs[i]); gens[i] = o ? o->pulse_gen : 0; }
  for (;;) {
    int pending = msgs ? rt_events_pending() : 0;   // pumps SDL events and may signal DirectInput events
    pthread_mutex_lock(&objlock);
    for (uint32_t i = 0; i < n; i++) {
      Obj* o = obj_get(hs[i]);
      if (!o) continue;
      if (i < 64 && o->t == H_EVENT && o->pulse_gen != gens[i]) { result = i; goto done; }   // pulsed while we waited
      if (obj_ready_locked(o)) { result = i; goto done; }
    }
    (void)all;
    if (pending) { result = n; goto done; }
    if (ms == 0) goto done;
    struct timespec slice;
    clock_gettime(CLOCK_REALTIME, &slice);
    slice.tv_nsec += 5 * 1000000; if (slice.tv_nsec >= 1000000000) { slice.tv_sec++; slice.tv_nsec -= 1000000000; }
    if (ms != 0xFFFFFFFFu && (slice.tv_sec > dl.tv_sec || (slice.tv_sec == dl.tv_sec && slice.tv_nsec > dl.tv_nsec))) slice = dl;
    pthread_cond_timedwait(&objcv, &objlock, &slice);
    if (ms != 0xFFFFFFFFu) {
      struct timespec now;
      clock_gettime(CLOCK_REALTIME, &now);
      if (now.tv_sec > dl.tv_sec || (now.tv_sec == dl.tv_sec && now.tv_nsec >= dl.tv_nsec)) goto done;
    }
    pthread_mutex_unlock(&objlock);
    continue;
  done:
    pthread_mutex_unlock(&objlock);
    return result;
  }
}
SH(WaitForSingleObject) { UNUSED; uint32_t h = a[0]; return wait_objs(1, &h, 0, a[1], 0); }
SH(WaitForMultipleObjects) {
  UNUSED;
  if (getenv("RT_TRACE_WAIT")) { rt_log("WaitMulti tid=%u n=%u all=%u ms=%u", TI(c)->tid, a[0], a[2], a[3]); for (uint32_t i = 0; i < a[0]; i++) rt_log("   h=%08x", GU32(a[1] + 4 * i)); }
  return wait_objs(a[0], (uint32_t*)GPTR(a[1]), (int)a[2], a[3], 0);
}
SH(MsgWaitForMultipleObjects) {
  UNUSED;
  if (getenv("RT_TRACE_WAIT")) rt_log("MsgWait n=%u h0=%08x ms=%u mask=%08x", a[0], a[0] ? GU32(a[1]) : 0, a[3], a[4]);
  return wait_objs(a[0], (uint32_t*)GPTR(a[1]), (int)a[2], a[3], 1);
}
SH(CloseHandle) {
  UNUSED;
  Obj* o = obj_get(a[0]);
  if (!o) return 1;
  if (o->fd >= 0) close(o->fd);
  if (o->dir) closedir(o->dir);
  if (o->t == H_THREAD) { pthread_detach(o->th); }
  pthread_mutex_lock(&objlock);
  o->t = H_NONE;
  pthread_mutex_unlock(&objlock);
  return 1;
}
SH(DuplicateHandle) { UNUSED; if (a[3]) GU32(a[3]) = a[1]; return 1; }

// threads
typedef struct { uint32_t start, param, h; } ThreadStart;
static uint32_t stack_next = 0x78000000u;
static void* thread_main(void* arg) {
  ThreadStart* ts = arg;
  CPU* cpu = calloc(1, sizeof(CPU));
  ThreadInfo* ti = calloc(1, sizeof *ti);
  cur_ti = ti;
  ti->tid = __sync_fetch_and_add(&next_tid, 1);
  cpu->thread = ti;
  uint32_t stack_top = __sync_fetch_and_add(&stack_next, 0x100000u) + 0x100000u - 16;
  GREG(cpu, R_ESP) = stack_top;
  ti->teb = gmalloc(0x100);
  memset(GPTR(ti->teb), 0, 0x100);
  GU32(ti->teb + 0x18) = ti->teb; GU32(ti->teb + 4) = stack_top; GU32(ti->teb + 8) = stack_top - 0x100000 + 16;
  GREG(cpu, R_FS_OFFSET) = ti->teb;
  Obj* o = obj_get(ts->h);
  if (o) { pthread_mutex_lock(&objlock); while (o->suspended) pthread_cond_wait(&objcv, &objlock); pthread_mutex_unlock(&objlock); }
  uint32_t r = rt_call_guest(cpu, ts->start, 1, ts->param);
  o = obj_get(ts->h);
  if (o) { pthread_mutex_lock(&objlock); o->done = 1; o->exit_code = r; pthread_cond_broadcast(&objcv); pthread_mutex_unlock(&objlock); }
  free(ts);
  return NULL;
}
SH(CreateThread) {
  UNUSED;
  uint32_t h = obj_new(H_THREAD);
  Obj* o = obj_get(h);
  o->suspended = (a[4] & 4) != 0;
  ThreadStart* ts = malloc(sizeof *ts);
  ts->start = a[2]; ts->param = a[3]; ts->h = h;
  pthread_create(&o->th, NULL, thread_main, ts);
  if (a[5]) GU32(a[5]) = 1000;
  return h;
}
SH(ResumeThread) { UNUSED; Obj* o = obj_get(a[0]); if (!o) return 0; pthread_mutex_lock(&objlock); int was = o->suspended; o->suspended = 0; pthread_cond_broadcast(&objcv); pthread_mutex_unlock(&objlock); return was; }
SH(SuspendThread) { UNUSED; return 0; }
SH(GetExitCodeThread) { UNUSED; Obj* o = obj_get(a[0]); GU32(a[1]) = o && o->done ? o->exit_code : 259; return 1; }

// files
SH(CreateFileA) {
  UNUSED;
  char path[1100];
  const char* name = (const char*)GPTR(a[0]);
  int exists = ci_resolve(name, path, sizeof path);
  if (getenv("RT_TRACE_API")) rt_log("  CreateFileA \"%s\" -> %s", name, exists ? path : "(missing)");
  uint32_t access = a[1], disp = a[4];
  int flags = (access & 0x40000000) ? ((access & 0x80000000) ? O_RDWR : O_WRONLY) : O_RDONLY;
  switch (disp) {
    case 1: flags |= O_CREAT | O_EXCL; break;
    case 2: flags |= O_CREAT | O_TRUNC; break;
    case 3: break;
    case 4: flags |= O_CREAT; break;
    case 5: flags |= O_TRUNC; break;
  }
  if (!exists && !(flags & O_CREAT)) { set_err(c, 2); return 0xFFFFFFFF; }
  int fd = open(path, flags, 0644);
  if (fd < 0) { set_err(c, errno == ENOENT ? 2 : 5); return 0xFFFFFFFF; }
  uint32_t h = obj_new(H_FILE);
  obj_get(h)->fd = fd;
  return h;
}
SH(ReadFile) {
  UNUSED;
  Obj* o = obj_get(a[0]);
  if (!o) { uint32_t h = a[0]; if (h >= 0x10 && h < 0x18) { if (a[3]) GU32(a[3]) = 0; return 1; } return 0; }
  ssize_t n = read(o->fd, GPTR(a[1]), a[2]);
  if (n < 0) return 0;
  if (a[3]) GU32(a[3]) = (uint32_t)n;
  return 1;
}
SH(WriteFile) {
  UNUSED;
  Obj* o = obj_get(a[0]);
  if (!o) {
    uint32_t h = a[0];
    if (h >= 0x10 && h < 0x18) { fwrite(GPTR(a[1]), 1, a[2], h == 0x12 ? stderr : stdout); if (a[3]) GU32(a[3]) = a[2]; return 1; }
    return 0;
  }
  ssize_t n = write(o->fd, GPTR(a[1]), a[2]);
  if (n < 0) return 0;
  if (a[3]) GU32(a[3]) = (uint32_t)n;
  return 1;
}
SH(SetFilePointer) {
  UNUSED;
  Obj* o = obj_get(a[0]);
  if (!o) return 0xFFFFFFFF;
  off_t r = lseek(o->fd, (int32_t)a[1], a[3] == 0 ? SEEK_SET : a[3] == 1 ? SEEK_CUR : SEEK_END);
  return r < 0 ? 0xFFFFFFFF : (uint32_t)r;
}
SH(SetEndOfFile) { UNUSED; Obj* o = obj_get(a[0]); if (!o) return 0; off_t p = lseek(o->fd, 0, SEEK_CUR); return ftruncate(o->fd, p) == 0; }
SH(FlushFileBuffers) { UNUSED; return 1; }
SH(GetFileSize) {
  UNUSED;
  Obj* o = obj_get(a[0]);
  if (!o) return 0xFFFFFFFF;
  struct stat st;
  fstat(o->fd, &st);
  if (a[1]) GU32(a[1]) = (uint32_t)((uint64_t)st.st_size >> 32);
  return (uint32_t)st.st_size;
}
SH(DeleteFileA) { UNUSED; char p[1100]; return ci_resolve((char*)GPTR(a[0]), p, sizeof p) && unlink(p) == 0; }
SH(CreateDirectoryA) { UNUSED; char p[1100]; ci_resolve((char*)GPTR(a[0]), p, sizeof p); return mkdir(p, 0755) == 0; }
SH(RemoveDirectoryA) { UNUSED; char p[1100]; return ci_resolve((char*)GPTR(a[0]), p, sizeof p) && rmdir(p) == 0; }
SH(GetCurrentDirectoryA) { UNUSED; const char* s = "C:\\Aztec"; uint32_t n = (uint32_t)strlen(s); if (n + 1 > a[0]) return n + 1; strcpy((char*)GPTR(a[1]), s); return n; }
SH(SetCurrentDirectoryA) { UNUSED; return 1; }
SH(SearchPathA) { UNUSED; set_err(c, 2); return 0; }
SH(GetVolumeInformationA) {
  UNUSED;
  if (a[1] && a[2]) { strncpy((char*)GPTR(a[1]), "AZTEC", a[2]); }
  if (a[3]) GU32(a[3]) = 0x12345678;
  if (a[4]) GU32(a[4]) = 255;
  if (a[5]) GU32(a[5]) = 0;
  if (a[6] && a[7]) strncpy((char*)GPTR(a[6]), "CDFS", a[7]);
  return 1;
}
static int wild_match(const char* pat, const char* s) {
  if (!*pat) return !*s;
  if (*pat == '*') return wild_match(pat + 1, s) || (*s && wild_match(pat, s + 1));
  if (*s && (*pat == '?' || tolower((unsigned char)*pat) == tolower((unsigned char)*s))) return wild_match(pat + 1, s + 1);
  return 0;
}
static int find_fill(Obj* o, uint32_t out) {
  struct dirent* e;
  while ((e = readdir(o->dir))) {
    if (!wild_match(o->pattern, e->d_name)) continue;
    char full[1400];
    snprintf(full, sizeof full, "%s/%s", o->dirpath, e->d_name);
    struct stat st;
    if (stat(full, &st) != 0) continue;
    memset(GPTR(out), 0, 318);
    GU32(out) = S_ISDIR(st.st_mode) ? 0x10 : 0x80;
    GU32(out + 32) = (uint32_t)((uint64_t)st.st_size >> 32);
    GU32(out + 36) = (uint32_t)st.st_size;
    strncpy((char*)GPTR(out + 44), e->d_name, 259);
    return 1;
  }
  return 0;
}
SH(FindFirstFileA) {
  UNUSED;
  char path[1100];
  const char* name = (const char*)GPTR(a[0]);
  char dirpart[1100], pat[260];
  const char* slash = strrchr(name, '\\');
  const char* slash2 = strrchr(name, '/');
  if (slash2 > slash) slash = slash2;
  if (slash) { snprintf(dirpart, sizeof dirpart, "%.*s", (int)(slash - name), name); snprintf(pat, sizeof pat, "%s", slash + 1); }
  else { strcpy(dirpart, "."); snprintf(pat, sizeof pat, "%s", name); }
  if (!dirpart[0]) strcpy(dirpart, "/");
  ci_resolve(dirpart, path, sizeof path);
  DIR* d = opendir(path);
  if (!d) { set_err(c, 3); return 0xFFFFFFFF; }
  uint32_t h = obj_new(H_FIND);
  Obj* o = obj_get(h);
  o->dir = d; snprintf(o->pattern, sizeof o->pattern, "%s", pat); snprintf(o->dirpath, sizeof o->dirpath, "%s", path);
  if (!find_fill(o, a[1])) { closedir(d); o->dir = NULL; o->t = H_NONE; set_err(c, 2); return 0xFFFFFFFF; }
  return h;
}
SH(FindNextFileA) { UNUSED; Obj* o = obj_get(a[0]); if (!o || !o->dir) return 0; if (find_fill(o, a[1])) return 1; set_err(c, 18); return 0; }
SH(FindClose) { UNUSED; Obj* o = obj_get(a[0]); if (o) { if (o->dir) closedir(o->dir); o->dir = NULL; o->t = H_NONE; } return 1; }

// file mapping: map host file straight into the guest address space
SH(CreateFileMappingA) {
  UNUSED;
  Obj* f = obj_get(a[0]);
  if (!f) return 0;
  uint32_t h = obj_new(H_MAP);
  Obj* o = obj_get(h);
  o->fd = dup(f->fd);
  struct stat st;
  fstat(o->fd, &st);
  o->size = (uint64_t)st.st_size;
  return h;
}
static uint32_t map_next = 0x80000000u;
SH(MapViewOfFile) {
  UNUSED;
  Obj* o = obj_get(a[0]);
  if (!o) return 0;
  uint32_t len = a[4] ? a[4] : (uint32_t)o->size;
  uint32_t addr = map_next;
  map_next += (len + 0xffff) & ~0xffffu;
  if (map_next > 0xF0000000u) rt_fatal("out of mapping space");
  void* p = mmap(g_mem + addr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, o->fd, a[3]);
  if (p == MAP_FAILED) { rt_log("MapViewOfFile: mmap failed: %s", strerror(errno)); return 0; }
  if (getenv("RT_TRACE_API")) rt_log("  MapViewOfFile -> %08x len=%u first=%08x", addr, len, GU32(addr));
  return addr;
}
SH(UnmapViewOfFile) { UNUSED; return 1; }

// resources of the EXE module: not needed by the game's own archives
SH(FindResourceA) { UNUSED; set_err(c, 1813); return 0; }
SH(LoadResource) { UNUSED; return 0; }
SH(LockResource) { UNUSED; return 0; }
SH(SizeofResource) { UNUSED; return 0; }

// strings
SH(lstrlenA) { UNUSED; return (uint32_t)strlen((char*)GPTR(a[0])); }
SH(lstrcpyA) { UNUSED; strcpy((char*)GPTR(a[0]), (char*)GPTR(a[1])); return a[0]; }
SH(lstrcpynA) { UNUSED; if (a[2]) { strncpy((char*)GPTR(a[0]), (char*)GPTR(a[1]), a[2] - 1); GU8(a[0] + a[2] - 1) = 0; } return a[0]; }
SH(lstrcatA) { UNUSED; strcat((char*)GPTR(a[0]), (char*)GPTR(a[1])); return a[0]; }
SH(lstrcmpA) { UNUSED; int r = strcmp((char*)GPTR(a[0]), (char*)GPTR(a[1])); return (uint32_t)(r < 0 ? -1 : r > 0); }
SH(MultiByteToWideChar) {
  UNUSED;
  const uint8_t* s = (const uint8_t*)GPTR(a[2]);
  int n = (int)a[3];
  if (n < 0) n = (int)strlen((const char*)s) + 1;
  if (a[5] == 0) return (uint32_t)n;
  for (int i = 0; i < n && i < (int)a[5]; i++) GU16(a[4] + 2 * i) = s[i];
  return (uint32_t)n;
}
SH(WideCharToMultiByte) {
  UNUSED;
  int n = (int)a[3];
  if (n < 0) { n = 0; while (GU16(a[2] + 2 * n)) n++; n++; }
  if (a[5] == 0) return (uint32_t)n;
  for (int i = 0; i < n && i < (int)a[5]; i++) GU8(a[4] + i) = (uint8_t)GU16(a[2] + 2 * i);
  return (uint32_t)n;
}
SH(LCMapStringA) {
  UNUSED;
  int n = (int)a[3];
  if (n < 0) n = (int)strlen((char*)GPTR(a[2])) + 1;
  if (a[5] == 0) return (uint32_t)n;
  for (int i = 0; i < n && i < (int)a[5]; i++) {
    uint8_t ch = GU8(a[2] + i);
    GU8(a[4] + i) = (a[1] & 0x200) ? (uint8_t)toupper(ch) : (a[1] & 0x100) ? (uint8_t)tolower(ch) : ch;
  }
  return (uint32_t)n;
}
SH(LCMapStringW) {
  UNUSED;
  int n = (int)a[3];
  if (n < 0) { n = 0; while (GU16(a[2] + 2 * n)) n++; n++; }
  if (a[5] == 0) return (uint32_t)n;
  for (int i = 0; i < n && i < (int)a[5]; i++) {
    uint16_t ch = GU16(a[2] + 2 * i);
    GU16(a[4] + 2 * i) = (a[1] & 0x200) ? (uint16_t)toupper(ch) : (a[1] & 0x100) ? (uint16_t)tolower(ch) : ch;
  }
  return (uint32_t)n;
}
static uint16_t ctype1(unsigned ch) {
  uint16_t t = 0;
  if (ch < 128) {
    if (isupper((int)ch)) t |= 0x1;
    if (islower((int)ch)) t |= 0x2;
    if (isdigit((int)ch)) t |= 0x4;
    if (isspace((int)ch)) t |= 0x8;
    if (ispunct((int)ch)) t |= 0x10;
    if (iscntrl((int)ch)) t |= 0x20;
    if (ch == ' ') t |= 0x40;
    if (isxdigit((int)ch)) t |= 0x80;
    if (isalpha((int)ch)) t |= 0x100;
  }
  return t;
}
SH(GetStringTypeA) {
  UNUSED;
  int n = (int)a[3];
  if (n < 0) n = (int)strlen((char*)GPTR(a[2])) + 1;
  for (int i = 0; i < n; i++) GU16(a[4] + 2 * i) = ctype1(GU8(a[2] + i));
  return 1;
}
SH(GetStringTypeW) {
  UNUSED;
  int n = (int)a[2];
  if (n < 0) { n = 0; while (GU16(a[1] + 2 * n)) n++; n++; }
  for (int i = 0; i < n; i++) GU16(a[3] + 2 * i) = ctype1(GU16(a[1] + 2 * i));
  return 1;
}
static int fmt_guest(char* out, size_t outsz, const char* fmt, const uint32_t* args) {
  size_t n = 0;
  int ai = 0;
  for (; *fmt && n + 1 < outsz; fmt++) {
    if (*fmt != '%') { out[n++] = *fmt; continue; }
    char spec[32];
    int k = 0;
    spec[k++] = *fmt++;
    while (*fmt && strchr("-+ #0123456789.l", *fmt) && k < 28) spec[k++] = *fmt++;
    char conv = *fmt;
    char tmp[512];
    if (conv == '%') { out[n++] = '%'; continue; }
    spec[k] = 0;
    char fs[40];
    int len;
    if (conv == 's') {
      snprintf(fs, sizeof fs, "%ss", spec);
      uint32_t p = args[ai++];
      len = snprintf(tmp, sizeof tmp, fs, p ? (char*)GPTR(p) : "(null)");
    } else if (conv == 'c') {
      snprintf(fs, sizeof fs, "%sc", spec);
      len = snprintf(tmp, sizeof tmp, fs, (int)args[ai++]);
    } else if (conv == 'd' || conv == 'i' || conv == 'u' || conv == 'x' || conv == 'X') {
      snprintf(fs, sizeof fs, "%s%c", spec, conv);
      len = snprintf(tmp, sizeof tmp, fs, args[ai++]);
    } else { tmp[0] = 0; len = 0; }
    for (int i = 0; i < len && n + 1 < outsz; i++) out[n++] = tmp[i];
  }
  out[n] = 0;
  return (int)n;
}
SH(wsprintfA) {
  UNUSED;
  char buf[2048];
  int n = fmt_guest(buf, sizeof buf, (char*)GPTR(a[1]), a + 2);
  memcpy(GPTR(a[0]), buf, (size_t)n + 1);
  return (uint32_t)n;
}

// profile (INI) API
static int ini_lookup(const char* file, const char* section, const char* key, char* out, size_t outsz) {
  char path[1100];
  if (!ci_resolve(file, path, sizeof path)) return 0;
  FILE* f = fopen(path, "r");
  if (!f) return 0;
  char line[1024], cur[128] = "";
  int found = 0;
  while (fgets(line, sizeof line, f)) {
    size_t l = strlen(line);
    while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
    if (line[0] == '[') { snprintf(cur, sizeof cur, "%.*s", (int)strcspn(line + 1, "]"), line + 1); continue; }
    char* eq = strchr(line, '=');
    if (!eq || strcasecmp(cur, section)) continue;
    *eq = 0;
    if (!strcasecmp(line, key)) { snprintf(out, outsz, "%s", eq + 1); found = 1; break; }
  }
  fclose(f);
  return found;
}
SH(GetPrivateProfileStringA) {
  UNUSED;
  char val[1024];
  const char* def = a[2] ? (const char*)GPTR(a[2]) : "";
  if (!a[0] || !a[1] || !ini_lookup((char*)GPTR(a[5]), (char*)GPTR(a[0]), (char*)GPTR(a[1]), val, sizeof val))
    snprintf(val, sizeof val, "%s", def);
  uint32_t n = (uint32_t)strlen(val);
  if (n + 1 > a[4]) n = a[4] ? a[4] - 1 : 0;
  if (a[4]) { memcpy(GPTR(a[3]), val, n); GU8(a[3] + n) = 0; }
  return n;
}
SH(GetPrivateProfileIntA) {
  UNUSED;
  char val[64];
  if (ini_lookup((char*)GPTR(a[3]), (char*)GPTR(a[0]), (char*)GPTR(a[1]), val, sizeof val)) return (uint32_t)atoi(val);
  return a[2];
}

// ============================================================ ADVAPI32: read-only virtual registry
// The game's installer wrote its settings to HKLM\\Software\\New Media Generation\\The Aztec; we serve the same values.
#define AZ "Software\\New Media Generation\\The Aztec"
static const struct { const char* key; const char* name; int is_dword; uint32_t dw; const char* sz; } reg_defaults[] = {
  {AZ, "Language", 1, 0, 0}, {AZ, "Difficulty", 1, 0, 0}, {AZ, "MouseSpeed", 1, 3, 0},
  {AZ, "Path", 0, 0, "C:\\Aztec\\Aztec.exe"}, {AZ, "CampaignPath", 0, 0, "D:\\Maps"},
  {AZ "\\Net", "DefaultProvider", 1, 1, 0}, {AZ "\\Net", "Phone", 0, 0, ""}, {AZ "\\Net", "IPHost", 0, 0, ""},
  {AZ "\\Release", "", 0, 0, ""},
  {AZ "\\Sound", "MusicPath", 0, 0, "D:\\Music"}, {AZ "\\Sound", "Scheme", 0, 0, "default"},
  {AZ "\\Sound", "SFXVolume", 1, 0, 0}, {AZ "\\Sound", "MusicVolume", 1, 0, 0}, {AZ "\\Sound", "SpeechVolume", 1, 0, 0},
  {AZ "\\Sound", "Frequency", 1, 22050, 0}, {AZ "\\Sound", "Acknowledgement", 1, 1, 0},
  {AZ "\\Video", "Path", 0, 0, "D:\\Video"},
  {AZ "\\View", "DisplayMode", 1, 0, 0}, {AZ "\\View", "GameSpeed", 1, 83, 0}, {AZ "\\View", "ScrollSpeed", 1, 48, 0},
  {AZ "\\View", "Gamma", 1, 1, 0}, {AZ "\\View", "ViewAll", 1, 0, 0},
};
#define N_REG (sizeof reg_defaults / sizeof reg_defaults[0])
static char reg_paths[64][256];
static int reg_n = 1;  // handle 0 unused; handle value = 0x1000 + index

static int reg_key_exists(const char* path) {
  size_t n = strlen(path);
  for (size_t i = 0; i < N_REG; i++)
    if (!strncasecmp(reg_defaults[i].key, path, n) && (reg_defaults[i].key[n] == 0 || reg_defaults[i].key[n] == '\\')) return 1;
  return 0;
}
static uint32_t reg_open(uint32_t parent, const char* sub, uint32_t out) {
  char path[256] = "";
  if (parent >= 0x1000 && parent < 0x1000 + 64) snprintf(path, sizeof path, "%s", reg_paths[parent - 0x1000]);
  else if (parent != 0x80000002u) return 2;  // only HKEY_LOCAL_MACHINE is populated
  if (*sub) snprintf(path + strlen(path), sizeof path - strlen(path), "%s%s", *path ? "\\" : "", sub);
  if (*path && !reg_key_exists(path)) return 2;
  if (reg_n >= 64) return 8;
  snprintf(reg_paths[reg_n], sizeof reg_paths[0], "%s", path);
  GU32(out) = 0x1000u + (uint32_t)reg_n++;
  return 0;
}
SH(RegOpenKeyA) { UNUSED; return reg_open(a[0], (const char*)GPTR(a[1]), a[2]); }
SH(RegOpenKeyExA) { UNUSED; return reg_open(a[0], (const char*)GPTR(a[1]), a[4]); }
SH(RegQueryValueExA) {  // hkey, name, reserved, *type, data, *cb
  UNUSED;
  if (a[0] < 0x1000 || a[0] >= 0x1000 + 64) return 6;
  const char* path = reg_paths[a[0] - 0x1000];
  const char* name = a[1] ? (const char*)GPTR(a[1]) : "";
  for (size_t i = 0; i < N_REG; i++) {
    if (strcasecmp(reg_defaults[i].key, path) || strcasecmp(reg_defaults[i].name, name)) continue;
    uint32_t type = reg_defaults[i].is_dword ? 4 : 1;
    uint32_t len = reg_defaults[i].is_dword ? 4 : (uint32_t)strlen(reg_defaults[i].sz) + 1;
    if (a[3]) GU32(a[3]) = type;
    if (a[4]) {
      if (a[5] && GU32(a[5]) < len) { GU32(a[5]) = len; return 234; }
      if (reg_defaults[i].is_dword) GU32(a[4]) = reg_defaults[i].dw; else memcpy(GPTR(a[4]), reg_defaults[i].sz, len);
    }
    if (a[5]) GU32(a[5]) = len;
    return 0;
  }
  return 2;
}

// ============================================================ USER32 + message queue
typedef struct { uint32_t msg, wparam, lparam; } QMsg;
static QMsg mq[256];
static int mq_head, mq_tail;
static pthread_mutex_t mq_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t wndproc_addr;
static const uint32_t HWND_MAIN = 0x10001;

void rt_post_message(uint32_t msg, uint32_t w, uint32_t l) {
  pthread_mutex_lock(&mq_lock);
  int next = (mq_tail + 1) & 255;
  if (next != mq_head) { mq[mq_tail] = (QMsg){msg, w, l}; mq_tail = next; }
  pthread_mutex_unlock(&mq_lock);
}
int rt_events_pending(void) {
  rt_pump_events();
  pthread_mutex_lock(&mq_lock);
  int r = mq_head != mq_tail;
  pthread_mutex_unlock(&mq_lock);
  return r;
}

SH(RegisterClassA) { UNUSED; wndproc_addr = GU32(a[0] + 4); return 0xC001; }
SH(CreateWindowExA) {
  UNUSED;
  // CreateWindowExA(exstyle, class, title, style, x, y, w, h, parent, menu, inst, param)
  backend_create_window((const char*)GPTR(a[2]), (int32_t)a[6], (int32_t)a[7]);
  if (wndproc_addr) {
    uint32_t cs = gmalloc(48);
    memset(GPTR(cs), 0, 48);
    GU32(cs + 12) = a[7]; GU32(cs + 16) = a[6]; GU32(cs + 20) = a[5]; GU32(cs + 24) = a[4];
    rt_call_guest(c, wndproc_addr, 4, HWND_MAIN, 0x81, 0, cs);   // WM_NCCREATE
    rt_call_guest(c, wndproc_addr, 4, HWND_MAIN, 0x01, 0, cs);   // WM_CREATE
    gfree(cs);
  }
  rt_post_message(0x06 /*WM_ACTIVATE*/, 1, 0);
  rt_post_message(0x1C /*WM_ACTIVATEAPP*/, 1, 0);
  rt_post_message(0x07 /*WM_SETFOCUS*/, 0, 0);
  return HWND_MAIN;
}
SH(DestroyWindow) { UNUSED; if (wndproc_addr) rt_call_guest(c, wndproc_addr, 4, HWND_MAIN, 0x02, 0, 0); return 1; }
SH(DefWindowProcA) { UNUSED; if (a[1] == 0x10) rt_post_message(0x2 /*WM_DESTROY*/, 0, 0); return 0; }
SH(PostQuitMessage) { UNUSED; rt_post_message(0x12, a[0], 0); return 0; }
SH(PeekMessageA) {
  UNUSED;
  rt_pump_events();
  pthread_mutex_lock(&mq_lock);
  if (mq_head == mq_tail) { pthread_mutex_unlock(&mq_lock); return 0; }
  QMsg m = mq[mq_head];
  if (a[4] & 1) mq_head = (mq_head + 1) & 255;  // PM_REMOVE
  pthread_mutex_unlock(&mq_lock);
  uint32_t p = a[0];
  GU32(p) = HWND_MAIN; GU32(p + 4) = m.msg; GU32(p + 8) = m.wparam; GU32(p + 12) = m.lparam;
  GU32(p + 16) = tick_ms(); GU32(p + 20) = 0; GU32(p + 24) = 0;
  return 1;
}
SH(TranslateMessage) { UNUSED; return 1; }
SH(DispatchMessageA) {
  UNUSED;
  uint32_t p = a[0];
  if (GU32(p + 4) == 0x12) return 0;
  if (!wndproc_addr) return 0;
  return rt_call_guest(c, wndproc_addr, 4, GU32(p), GU32(p + 4), GU32(p + 8), GU32(p + 12));
}
SH(BeginPaint) { UNUSED; memset(GPTR(a[1]), 0, 64); return 0x20001; }
SH(EndPaint) { UNUSED; return 1; }
SH(GetWindowLongA) { UNUSED; return 0; }
SH(LoadCursorA) { UNUSED; return 0x30001; }
SH(LoadIconA) { UNUSED; return 0x30002; }
static int cursor_count = 0;  // Win32 display counter: cursor visible while >= 0
SH(ShowCursor) { UNUSED; cursor_count += a[0] ? 1 : -1; backend_show_cursor(cursor_count >= 0); return (uint32_t)cursor_count; }
SH(GetStockObject) { UNUSED; return 0x40000 + a[0]; }
SH(MessageBoxA) { UNUSED; rt_log("MessageBox: [%s] %s", (char*)GPTR(a[2]), (char*)GPTR(a[1])); return 1; }
SH(CharLowerA) { UNUSED; if (a[0] < 0x10000) return (uint32_t)tolower((int)a[0]); char* s = (char*)GPTR(a[0]); for (; *s; s++) *s = (char)tolower((unsigned char)*s); return a[0]; }
SH(IsCharAlphaNumericA) { UNUSED; return isalnum((int)(a[0] & 0xff)) != 0; }
SH(OpenClipboard) { UNUSED; return 0; }
SH(CloseClipboard) { UNUSED; return 1; }
SH(EmptyClipboard) { UNUSED; return 1; }
SH(SetClipboardData) { UNUSED; return 0; }
SH(GetKeyState) { UNUSED; return (uint16_t)backend_vk_state((int)a[0]); }
SH(GetAsyncKeyState) { UNUSED; return (uint16_t)backend_vk_state((int)a[0]); }
// ---- MCI digital video (AVI) as used by the game's movie class; playback is done by video.c.
typedef struct { int used; char path[512]; uint32_t w, h, frames, us_per_frame; int playing, played; } MciDev;
static MciDev mci_devs[16];
static void avi_info(MciDev* d) {
  char host[1100];
  d->w = 640; d->h = 480; d->frames = 1; d->us_per_frame = 83333;
  if (!ci_resolve(d->path, host, sizeof host)) return;
  FILE* f = fopen(host, "rb");
  if (!f) return;
  uint8_t h[0x60];
  if (fread(h, 1, sizeof h, f) == sizeof h && !memcmp(h, "RIFF", 4) && !memcmp(h + 0x18, "avih", 4)) {
    uint32_t v[14];
    memcpy(v, h + 0x20, sizeof v);  // dwMicroSecPerFrame .. dwHeight
    d->us_per_frame = v[0]; d->frames = v[4]; d->w = v[8]; d->h = v[9];
  }
  fclose(f);
}
extern int rt_video_play(const char* host_path, uint32_t w, uint32_t h, uint32_t device, uint32_t notify_hwnd);
SH(mciSendCommandA) {  // id, cmd, flags, parms
  UNUSED;
  uint32_t id = a[0], cmd = a[1], flags = a[2], p = a[3];
  MciDev* d = (id >= 1 && id <= 16) ? &mci_devs[id - 1] : NULL;
  switch (cmd) {
    case 0x803: {  // MCI_OPEN
      for (int i = 0; i < 16; i++) if (!mci_devs[i].used) {
        mci_devs[i].used = 1; mci_devs[i].played = 0; mci_devs[i].playing = 0;
        snprintf(mci_devs[i].path, sizeof mci_devs[i].path, "%s", GU32(p + 12) ? (const char*)GPTR(GU32(p + 12)) : "");
        avi_info(&mci_devs[i]);
        GU32(p + 4) = (uint32_t)i + 1;
        return 0;
      }
      return 0x122;
    }
    case 0x804: if (d) d->used = 0; return 0;  // MCI_CLOSE
    case 0x843:  // MCI_WHERE: source/destination rectangle
      if (d) { GU32(p + 4) = 0; GU32(p + 8) = 0; GU32(p + 12) = d->w; GU32(p + 16) = d->h; }
      return 0;
    case 0x841: case 0x842: case 0x807: return 0;  // WINDOW / PUT / SEEK
    case 0x814: {  // MCI_STATUS
      uint32_t item = GU32(p + 8), ret = 0;
      switch (item) {
        case 1: ret = d ? d->frames : 0; break;                 // length (frames)
        case 2: ret = (d && (d->playing || d->played)) ? d->frames : 0; break;  // position
        case 4: ret = (d && d->playing) ? 526 : 525; break;     // mode: playing / stopped
        case 6: ret = 3; break;                                  // time format: frames
        default: ret = 1; break;
      }
      GU32(p + 4) = ret;
      return 0;
    }
    case 0x806: {  // MCI_PLAY
      if (!d) return 0x122;
      char host[1100];
      ci_resolve(d->path, host, sizeof host);
      d->playing = 1;
      int ok = rt_video_play(host, d->w, d->h, id, GU32(p));
      d->playing = 0;
      d->played = 1;
      if (flags & 1) rt_post_message(0x3B9 /*MM_MCINOTIFY*/, ok ? 1u : 4u, id);
      return 0;
    }
    default: rt_log("mciSendCommand: unhandled cmd %04x", cmd); return 0x122;
  }
}

// ============================================================ registration
#define K "KERNEL32.dll"
#define U "USER32.dll"
#define E(dll, n, na) {dll, #n, na, sh_##n}
static const Shim base_shims[] = {
  E(K, GetLastError, 0), E(K, SetLastError, 1), E(K, GetVersion, 0), E(K, GetVersionExA, 1), E(K, GetCommandLineA, 0),
  E(K, GetStartupInfoA, 1), E(K, GetModuleHandleA, 1), E(K, GetModuleFileNameA, 3), E(K, GetProcAddress, 2),
  E(K, LoadLibraryA, 1), E(K, LoadLibraryExA, 3), E(K, FreeLibrary, 1), E(K, ExitProcess, 1), E(K, TerminateProcess, 2),
  E(K, GetCurrentProcess, 0), E(K, GetCurrentThread, 0), E(K, GetCurrentThreadId, 0), E(K, SetThreadPriority, 2),
  E(K, GetACP, 0), E(K, GetOEMCP, 0), E(K, GetCPInfo, 2), E(K, IsBadReadPtr, 2), E(K, IsBadWritePtr, 2), E(K, IsBadCodePtr, 1),
  E(K, SetUnhandledExceptionFilter, 1), E(K, UnhandledExceptionFilter, 1), E(K, RaiseException, 4), E(K, RtlUnwind, 4),
  E(K, InterlockedIncrement, 1), E(K, InterlockedDecrement, 1), E(K, GetSystemInfo, 1), E(K, GetComputerNameA, 2),
  E("ADVAPI32.dll", GetUserNameA, 2), E(K, GetTickCount, 0), E("WINMM.dll", timeGetTime, 0), E(K, Sleep, 1),
  E(K, GetSystemTime, 1), E(K, GetLocalTime, 1), E(K, GetFileTime, 4), E(K, HeapCreate, 3), E(K, HeapDestroy, 1),
  E(K, GetProcessHeap, 0), E(K, HeapAlloc, 3), E(K, HeapFree, 3), E(K, HeapSize, 3), E(K, HeapReAlloc, 4),
  E(K, GlobalAlloc, 2), E(K, GlobalLock, 1), E(K, GlobalUnlock, 1), E(K, VirtualAlloc, 4), E(K, VirtualFree, 3),
  E(K, GetEnvironmentStrings, 0), E(K, GetEnvironmentStringsW, 0), E(K, FreeEnvironmentStringsA, 1),
  E(K, FreeEnvironmentStringsW, 1), E(K, SetHandleCount, 1), E(K, GetStdHandle, 1), E(K, SetStdHandle, 2), E(K, GetFileType, 1),
  E(K, TlsAlloc, 0), E(K, TlsSetValue, 2), E(K, TlsGetValue, 1), E(K, InitializeCriticalSection, 1),
  E(K, DeleteCriticalSection, 1), E(K, EnterCriticalSection, 1), E(K, LeaveCriticalSection, 1),
  E(K, CreateEventA, 4), E(K, OpenEventA, 3), E(K, SetEvent, 1), E(K, ResetEvent, 1), E(K, PulseEvent, 1),
  E(K, CreateMutexA, 3), E(K, ReleaseMutex, 1), E(K, CreateSemaphoreA, 4), E(K, ReleaseSemaphore, 3),
  E(K, WaitForSingleObject, 2), E(K, WaitForMultipleObjects, 4), E(U, MsgWaitForMultipleObjects, 5), E(K, CloseHandle, 1),
  E(K, DuplicateHandle, 7), E(K, CreateThread, 6), E(K, ResumeThread, 1), E(K, SuspendThread, 1), E(K, GetExitCodeThread, 2),
  E(K, CreateFileA, 7), E(K, ReadFile, 5), E(K, WriteFile, 5), E(K, SetFilePointer, 4), E(K, SetEndOfFile, 1),
  E(K, FlushFileBuffers, 1), E(K, GetFileSize, 2), E(K, DeleteFileA, 1), E(K, CreateDirectoryA, 2), E(K, RemoveDirectoryA, 1),
  E(K, GetCurrentDirectoryA, 2), E(K, SetCurrentDirectoryA, 1), E(K, SearchPathA, 6), E(K, GetVolumeInformationA, 8),
  E(K, FindFirstFileA, 2), E(K, FindNextFileA, 2), E(K, FindClose, 1), E(K, CreateFileMappingA, 6), E(K, MapViewOfFile, 5),
  E(K, UnmapViewOfFile, 1), E(K, FindResourceA, 3), E(K, LoadResource, 2), E(K, LockResource, 1), E(K, SizeofResource, 2),
  E(K, lstrlenA, 1), E(K, lstrcpyA, 2), E(K, lstrcpynA, 3), E(K, lstrcatA, 2), E(K, lstrcmpA, 2),
  E(K, MultiByteToWideChar, 6), E(K, WideCharToMultiByte, 8), E(K, LCMapStringA, 6), E(K, LCMapStringW, 6),
  E(K, GetStringTypeA, 5), E(K, GetStringTypeW, 4), E(U, wsprintfA, -1),
  E(K, GetPrivateProfileStringA, 6), E(K, GetPrivateProfileIntA, 4),
  E("ADVAPI32.dll", RegOpenKeyExA, 5), E("ADVAPI32.dll", RegOpenKeyA, 3), E("ADVAPI32.dll", RegQueryValueExA, 6),
  E(U, RegisterClassA, 1), E(U, CreateWindowExA, 12), E(U, DestroyWindow, 1), E(U, DefWindowProcA, 4),
  E(U, PostQuitMessage, 1), E(U, PeekMessageA, 5), E(U, TranslateMessage, 1), E(U, DispatchMessageA, 1),
  E(U, BeginPaint, 2), E(U, EndPaint, 2), E(U, GetWindowLongA, 2), E(U, LoadCursorA, 2), E(U, LoadIconA, 2),
  E(U, ShowCursor, 1), E("GDI32.dll", GetStockObject, 1), E(U, MessageBoxA, 4), E(U, CharLowerA, 1),
  E(U, IsCharAlphaNumericA, 1), E(U, OpenClipboard, 1), E(U, CloseClipboard, 0), E(U, EmptyClipboard, 0),
  E(U, SetClipboardData, 2), E(U, GetKeyState, 1), E(U, GetAsyncKeyState, 1), E("WINMM.dll", mciSendCommandA, 4),
};

extern void rt_register_directx_shims(void);

void rt_register_all_shims(void) {
  rt_register_shims(base_shims, (int)(sizeof base_shims / sizeof base_shims[0]));
  rt_register_directx_shims();
}

void rt_win32_init(CPU* cpu, const char* exe_path) {
  clock_gettime(CLOCK_MONOTONIC, &t_start);
  if (getenv("RT_TIMESCALE")) { rt_time_scale = atoi(getenv("RT_TIMESCALE")); if (rt_time_scale < 1) rt_time_scale = 1; }
  snprintf(exe_path_g, sizeof exe_path_g, "%s", exe_path);
  snprintf(game_dir, sizeof game_dir, "%s", exe_path);
  char* sl = strrchr(game_dir, '/');
  if (sl) { *sl = 0; if (chdir(game_dir) != 0) rt_fatal("cannot chdir to %s", game_dir); }
  main_ti.tid = 1;
  cpu->thread = &main_ti;
  cur_ti = &main_ti;
  // TEB / PEB
  uint32_t teb = 0x7ffde000u;
  memset(GPTR(teb), 0, 0x1000);
  GU32(teb + 0x18) = teb;
  GU32(teb + 4) = 0x7f000000u; GU32(teb + 8) = 0x7e800000u;
  GU32(teb + 0x30) = 0x7ffdf000u;
  main_ti.teb = teb;
  GREG(cpu, R_FS_OFFSET) = teb;
  GREG(cpu, R_ESP) = 0x7f000000u - 16;
}
