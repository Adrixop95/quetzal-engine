#define _GNU_SOURCE
#include "dbg.h"

#include <execinfo.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

static FILE* logf;
static int enabled;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct timespec t0;

#define RING 512
static struct { char name[40]; uint32_t a[4]; long tid; uint64_t ms; } ring[RING];
static unsigned ring_pos;

#define MAX_DUMPERS 16
static struct { const char* name; void (*fn)(FILE*); } dumpers[MAX_DUMPERS];
static int n_dumpers;

int dbg_enabled(void) { return enabled; }

static uint64_t now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  int64_t ms = (int64_t)(t.tv_sec - t0.tv_sec) * 1000 + (int64_t)(t.tv_nsec - t0.tv_nsec) / 1000000;
  return ms < 0 ? 0 : (uint64_t)ms;
}

void dbg_log(const char* cat, const char* fmt, ...) {
  if (!enabled) return;
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  pthread_mutex_lock(&lock);
  fprintf(logf, "[%8llu][t%-6ld][%-7s] %s\n", (unsigned long long)now_ms(), (long)syscall(SYS_gettid), cat, buf);
  fflush(logf);
  pthread_mutex_unlock(&lock);
}

void dbg_api_record(const char* name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
  if (!enabled) return;
  unsigned i = __sync_fetch_and_add(&ring_pos, 1) % RING;
  strncpy(ring[i].name, name, sizeof ring[i].name - 1);
  ring[i].name[sizeof ring[i].name - 1] = 0;
  ring[i].a[0] = a0; ring[i].a[1] = a1; ring[i].a[2] = a2; ring[i].a[3] = a3;
  ring[i].tid = (long)syscall(SYS_gettid);
  ring[i].ms = now_ms();
}

void dbg_register_dumper(const char* name, void (*fn)(FILE*)) {
  if (n_dumpers < MAX_DUMPERS) { dumpers[n_dumpers].name = name; dumpers[n_dumpers].fn = fn; n_dumpers++; }
}

void dbg_dump_context(const char* why) {
  if (!enabled) return;
  pthread_mutex_lock(&lock);
  fprintf(logf, "\n================ CONTEXT DUMP: %s (t=%llu ms) ================\n", why, (unsigned long long)now_ms());
  for (int i = 0; i < n_dumpers; i++) {
    fprintf(logf, "---- %s ----\n", dumpers[i].name);
    dumpers[i].fn(logf);
  }
  fprintf(logf, "---- last %d API calls (oldest first) ----\n", RING);
  unsigned p = ring_pos;
  for (unsigned k = 0; k < RING; k++) {
    unsigned i = (p + k) % RING;
    if (!ring[i].name[0]) continue;
    fprintf(logf, "  [%8llu][t%ld] %s(%08x,%08x,%08x,%08x)\n", (unsigned long long)ring[i].ms, ring[i].tid, ring[i].name,
            ring[i].a[0], ring[i].a[1], ring[i].a[2], ring[i].a[3]);
  }
  void* bt[48];
  int n = backtrace(bt, 48);
  fprintf(logf, "---- host backtrace (%d frames) ----\n", n);
  fflush(logf);
  backtrace_symbols_fd(bt, n, fileno(logf));
  fprintf(logf, "================ END CONTEXT DUMP ================\n");
  fflush(logf);
  pthread_mutex_unlock(&lock);
}

void dbg_fatal_hook(const char* msg) {
  if (!enabled) return;
  dbg_log("FATAL", "%s", msg);
  dbg_dump_context("fatal");
}

static void on_signal(int sig) {
  char m[64];
  snprintf(m, sizeof m, "signal %d (%s)", sig, strsignal(sig));
  if (enabled) { dbg_log("CRASH", "%s", m); dbg_dump_context(m); }
  signal(sig, SIG_DFL);
  raise(sig);
}
static void on_exit_hook(void) { if (enabled) dbg_log("exit", "process exiting normally (atexit hook)"); }

void dbg_init(void) {
  const char* on = getenv("RT_DEBUG");
  const char* path = getenv("RT_DEBUG_FILE");
  if (!on && !path) return;
  if (!path) path = "aztec_debug.log";
  logf = fopen(path, "w");
  if (!logf) { fprintf(stderr, "cannot open debug log %s\n", path); return; }
  clock_gettime(CLOCK_MONOTONIC, &t0);
  enabled = 1;
  struct utsname u;
  uname(&u);
  dbg_log("init", "debug log started, pid=%d, host=%s %s %s", (int)getpid(), u.sysname, u.release, u.machine);
  char cwd[1024];
  if (getcwd(cwd, sizeof cwd)) dbg_log("init", "cwd=%s", cwd);
  const char* keys[] = {"XDG_SESSION_TYPE", "DISPLAY", "WAYLAND_DISPLAY", "SDL_VIDEODRIVER", "SDL_RENDER_DRIVER", "XDG_CURRENT_DESKTOP", "GDK_SCALE", "QT_SCALE_FACTOR", "LANG", NULL};
  for (int i = 0; keys[i]; i++) dbg_log("init", "env %s=%s", keys[i], getenv(keys[i]) ? getenv(keys[i]) : "(unset)");
  extern char** environ;
  for (char** e = environ; *e; e++) if (!strncmp(*e, "RT_", 3)) dbg_log("init", "env %s", *e);
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = on_signal;
  int sigs[] = {SIGSEGV, SIGABRT, SIGILL, SIGFPE, SIGBUS};
  for (unsigned i = 0; i < sizeof sigs / sizeof *sigs; i++) sigaction(sigs[i], &sa, NULL);
  atexit(on_exit_hook);
}
