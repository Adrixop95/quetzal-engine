// Guest memory, allocator, dispatcher, PE loader, x87-ish float helpers.
#define _GNU_SOURCE
#include "aztec_rt.h"
#include "dbg.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <pthread.h>
#include "backend.h"

uint8_t* g_mem;

// ------------------------------------------------------------------ logging
void rt_log(const char* fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  fprintf(stderr, "%s\n", buf);
  DBG("log", "%s", buf);
}
void rt_fatal(const char* fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  fprintf(stderr, "FATAL: %s\n", buf);
  fflush(stderr);
  dbg_fatal_hook(buf);
  abort();
}
void rt_unlifted(CPU* c, uint32_t addr) { (void)c; rt_fatal("unlifted function %08x", addr); }

// --------------------------------------------------------- guest allocator
#define HEAP_BASE 0x10000000u
#define HEAP_END 0x6e000000u
static uint32_t heap_top = HEAP_BASE;
static uint32_t free_lists[40];  // by log2 class; large blocks use class 39 list (unused)
typedef struct { uint32_t cls; uint32_t size; } BlockHdr;  // 8 bytes before the payload

static int size_class(uint32_t n) {
  int k = 4;
  while (((uint32_t)1 << k) < n) k++;
  return k;
}

#ifdef RT_HEAPDEBUG
static struct { uint32_t p, n; int live; } dbg_blocks[400000];
static int dbg_n;
static void dbg_alloc(uint32_t p, uint32_t n) {
  for (int i = 0; i < dbg_n; i++)
    if (dbg_blocks[i].live && p < dbg_blocks[i].p + dbg_blocks[i].n && dbg_blocks[i].p < p + n)
      rt_fatal("heap overlap: new %08x+%u overlaps live %08x+%u", p, n, dbg_blocks[i].p, dbg_blocks[i].n);
  dbg_blocks[dbg_n].p = p; dbg_blocks[dbg_n].n = n; dbg_blocks[dbg_n++].live = 1;
}
static void dbg_free(uint32_t p) {
  for (int i = 0; i < dbg_n; i++) if (dbg_blocks[i].live && dbg_blocks[i].p == p) { dbg_blocks[i].live = 0; return; }
  rt_fatal("free of unknown/dead block %08x", p);
}
#else
#define dbg_alloc(p, n) ((void)0)
#define dbg_free(p) ((void)0)
#endif

uint32_t gmalloc(uint32_t size) {
  if (size == 0) size = 1;
  int k = size_class(size + 8);
  uint32_t blk;
  if (free_lists[k]) {
    blk = free_lists[k];
    free_lists[k] = GU32(blk + 8);
  } else {
    uint32_t sz = (uint32_t)1 << k;
    if ((uint64_t)heap_top + sz >= HEAP_END) return 0;
    blk = heap_top;
    heap_top += sz;
  }
  BlockHdr* h = (BlockHdr*)GPTR(blk);
  h->cls = (uint32_t)k | 0xA1100000u;   // live-block magic: lets gfree reject double frees
  h->size = size;
  memset(g_mem + blk + 8, 0, size);  // the game assumes fresh heap memory is zeroed
  dbg_alloc(blk + 8, size);
  return blk + 8;
}
void gfree(uint32_t ptr) {
  if (!ptr) return;
  dbg_free(ptr);
  uint32_t blk = ptr - 8;
  BlockHdr* h = (BlockHdr*)GPTR(blk);
  if ((h->cls & 0xFFFF0000u) != 0xA1100000u) {   // Windows tolerated this; a naive free list would hand the block out twice
    static int warned;
    if (warned++ < 8) rt_log("gfree: ignoring double or invalid free of %08x", ptr);
    return;
  }
  uint32_t k = h->cls & 0xFFFFu;
  h->cls = k | 0xDEAD0000u;
  GU32(blk + 8) = free_lists[k];
  free_lists[k] = blk;
}
uint32_t gmsize(uint32_t ptr) { return ptr ? ((BlockHdr*)GPTR(ptr - 8))->size : 0; }
uint32_t gmrealloc(uint32_t ptr, uint32_t size) {
  if (!ptr) return gmalloc(size);
  uint32_t old = gmsize(ptr);
  uint32_t n = gmalloc(size);
  if (!n) return 0;
  memcpy(GPTR(n), GPTR(ptr), old < size ? old : size);
  gfree(ptr);
  return n;
}
uint32_t guest_cstring(const char* s) {
  uint32_t n = (uint32_t)strlen(s) + 1;
  uint32_t p = gmalloc(n);
  memcpy(GPTR(p), s, n);
  return p;
}

// ------------------------------------------------------------- dispatcher
static const FuncEntry** fn_hash;
static uint32_t fn_hash_size;

static void build_fn_hash(void) {
  fn_hash_size = 1;
  while (fn_hash_size < g_func_count * 3) fn_hash_size <<= 1;
  fn_hash = calloc(fn_hash_size, sizeof *fn_hash);
  for (unsigned i = 0; i < g_func_count; i++) {
    uint32_t h = (g_func_table[i].addr * 2654435761u) & (fn_hash_size - 1);
    while (fn_hash[h]) h = (h + 1) & (fn_hash_size - 1);
    fn_hash[h] = &g_func_table[i];
  }
}
static void (*lookup_fn(uint32_t addr))(CPU*) {
  uint32_t h = (addr * 2654435761u) & (fn_hash_size - 1);
  while (fn_hash[h]) {
    if (fn_hash[h]->addr == addr) return fn_hash[h]->fn;
    h = (h + 1) & (fn_hash_size - 1);
  }
  return NULL;
}

// ------------------------------------------------------------------- shims
#define SHIM_BASE 0xF0000000u
#define COM_BASE 0xF1000000u
static Shim* shims;
static int n_shims;
static int cap_shims;

void rt_register_shims(const Shim* s, int n) {
  if (n_shims + n > cap_shims) {
    cap_shims = (n_shims + n) * 2;
    shims = realloc(shims, cap_shims * sizeof *shims);
  }
  memcpy(shims + n_shims, s, n * sizeof *s);
  n_shims += n;
}
uint32_t rt_shim_thunk(const char* dll, const char* name) {
  for (int i = 0; i < n_shims; i++)
    if (!strcasecmp(shims[i].dll, dll) && !strcmp(shims[i].name, name)) return SHIM_BASE + 4u * i;
  return 0;
}

typedef struct { const ComMethod* m; } ComThunk;
static ComThunk* com_thunks;
static int n_com_thunks, cap_com_thunks;
typedef struct { const ComMethod* methods; uint32_t vtable; } ComClass;
static ComClass com_classes[32];
static int n_com_classes;
static void* com_hosts[4096];
static int n_com_hosts = 1;

uint32_t com_create(const ComMethod* methods, int count, void* host) {
  ComClass* cl = NULL;
  for (int i = 0; i < n_com_classes; i++) if (com_classes[i].methods == methods) cl = &com_classes[i];
  if (!cl) {
    cl = &com_classes[n_com_classes++];
    cl->methods = methods;
    cl->vtable = gmalloc(4 * (uint32_t)count);
    for (int i = 0; i < count; i++) {
      if (n_com_thunks == cap_com_thunks) {
        cap_com_thunks = cap_com_thunks ? cap_com_thunks * 2 : 256;
        com_thunks = realloc(com_thunks, cap_com_thunks * sizeof *com_thunks);
      }
      com_thunks[n_com_thunks].m = &methods[i];
      GU32(cl->vtable + 4 * i) = COM_BASE + 4u * n_com_thunks++;
    }
  }
  uint32_t obj = gmalloc(8);
  GU32(obj) = cl->vtable;
  GU32(obj + 4) = n_com_hosts;
  com_hosts[n_com_hosts++] = host;
  return obj;
}
void* com_host(uint32_t obj) { return obj ? com_hosts[GU32(obj + 4)] : NULL; }

static void shim_call(CPU* c, uint32_t addr) {
  uint32_t esp = GREG(c, R_ESP);
  uint32_t* a = (uint32_t*)GPTR(esp + 4);
  const char* name;
  ShimFn fn;
  int nargs;
  if (addr < COM_BASE) {
    uint32_t id = (addr - SHIM_BASE) / 4;
    if (id >= (uint32_t)n_shims) rt_fatal("bad shim thunk %08x", addr);
    fn = shims[id].fn; nargs = shims[id].nargs; name = shims[id].name;
  } else {
    uint32_t id = (addr - COM_BASE) / 4;
    if (id >= (uint32_t)n_com_thunks) rt_fatal("bad COM thunk %08x", addr);
    fn = com_thunks[id].m->fn; nargs = com_thunks[id].m->nargs < 0 ? -1 : com_thunks[id].m->nargs + 1;
    name = com_thunks[id].m->name;
  }
  if (!fn) rt_fatal("unimplemented API: %s (esp=%08x)", name, esp);
  dbg_api_record(name, nargs > 0 ? a[0] : 0, nargs > 1 ? a[1] : 0, nargs > 2 ? a[2] : 0, nargs > 3 ? a[3] : 0);
  if (dbg_enabled()) {
    // chatty calls are sampled, everything else is logged in full
    static const char* const noisy[] = {"Lock", "Unlock", "GetProcessHeap", "HeapAlloc", "HeapFree", "lstrcmpA", "lstrcpynA", "lstrlenA",
      "lstrcpyA", "lstrcatA", "GetDeviceState", "WaitForSingleObject", "MsgWaitForMultipleObjects", "timeGetTime", "EnterCriticalSection",
      "LeaveCriticalSection", "ReleaseMutex", "PeekMessageA", "BltFast", "Blt", "GetAsyncKeyState", "TlsGetValue", "SetLastError", "GetLastError",
      "GetTickCount", "Sleep", "PulseEvent", "SetEvent", "GetKeyState", "GetDeviceData", NULL};
    int is_noisy = 0;
    for (int i = 0; noisy[i]; i++) if (!strcmp(name, noisy[i])) { is_noisy = 1; break; }
    static unsigned long cnt;
    if (!is_noisy || (++cnt % 1000) == 0)
      dbg_log("api", "%s(%08x,%08x,%08x,%08x)%s", name, nargs > 0 ? a[0] : 0, nargs > 1 ? a[1] : 0, nargs > 2 ? a[2] : 0, nargs > 3 ? a[3] : 0, is_noisy ? " [sampled]" : "");
  }
  static int trace = -1;
  if (trace < 0) trace = getenv("RT_TRACE_API") != NULL;
  if (trace) rt_log("API[%u] %s(%08x,%08x,%08x,%08x)", c->thread ? *(uint32_t*)((char*)c->thread + 64 * 4 + 4) : 0, name, nargs > 0 ? a[0] : 0, nargs > 1 ? a[1] : 0, nargs > 2 ? a[2] : 0, nargs > 3 ? a[3] : 0);
  uint32_t r = fn(c, a);
  {
    static int watch = -1;
    static uint32_t last = 0;
    if (watch < 0) watch = getenv("RT_WATCH_ADDR") != NULL;
    if (watch) {
      uint32_t wa = (uint32_t)strtoul(getenv("RT_WATCH_ADDR"), NULL, 16);
      uint32_t v = GU32(wa);
      if (v != last) { rt_log("WATCH %08x: %08x -> %08x after %s", wa, last, v, name); last = v; }
    }
  }
  GREG(c, R_EAX) = r;
  GREG(c, R_ESP) = esp + 4 + (nargs > 0 ? 4u * nargs : 0);
}

void f_dispatch(CPU* c, uint32_t addr) {
  if (addr >= SHIM_BASE) { shim_call(c, addr); return; }
  void (*fn)(CPU*) = lookup_fn(addr);
  if (!fn && getenv("RT_IATDUMP")) { for (uint32_t q = 0x48f040; q < 0x48f070; q += 4) rt_log("IAT[%08x]=%08x", q, GU32(q)); }
  if (!fn && dbg_enabled()) {
    dbg_log("guest", "unlifted target %08x; guest stack at esp=%08x:", addr, GREG(c, R_ESP));
    for (int k = 0; k < 24; k++) dbg_log("guest", "  [esp+%02x] = %08x", 4 * k, GU32(GREG(c, R_ESP) + 4 * k));
    FILE* mf = fopen("rt_missing_entries.txt", "a");   // feed tools/extra_entries.txt
    if (mf) { fprintf(mf, "%08x\n", addr); fclose(mf); }
  }
  if (!fn) rt_fatal("call to unlifted guest address %08x (return address %08x, esp %08x, ecx %08x, edi %08x [edi]=%08x)", addr, GU32(GREG(c, R_ESP)), GREG(c, R_ESP), GREG(c, R_ECX), GREG(c, R_EDI), GU32(GREG(c, R_EDI)));
  fn(c);
}

void rt_push(CPU* c, uint32_t v) {
  GREG(c, R_ESP) -= 4;
  GU32(GREG(c, R_ESP)) = v;
}
uint32_t rt_call_guest(CPU* c, uint32_t addr, int nargs, ...) {
  uint32_t args[16];
  va_list ap;
  va_start(ap, nargs);
  for (int i = 0; i < nargs; i++) args[i] = va_arg(ap, uint32_t);
  va_end(ap);
  uint32_t esp0 = GREG(c, R_ESP);
  for (int i = nargs - 1; i >= 0; i--) rt_push(c, args[i]);
  rt_push(c, 0xF0FFFFF0u);  // return address (never executed: callee RET pops it)
  f_dispatch(c, addr);
  GREG(c, R_ESP) = esp0;     // tolerate cdecl callees
  return GREG(c, R_EAX);
}

// -------------------------------------------------------------- float ops
static long double fget(int sz, const void* p) {
  if (sz == 4) { float f; memcpy(&f, p, 4); return f; }
  if (sz == 8) { double d; memcpy(&d, p, 8); return d; }
  if (sz == 10) { long double l = 0; memcpy(&l, p, 10); return l; }
  rt_fatal("float size %d", sz);
}
static void fput(int sz, void* p, long double v) {
  if (sz == 4) { float f = (float)v; memcpy(p, &f, 4); }
  else if (sz == 8) { double d = (double)v; memcpy(p, &d, 8); }
  else if (sz == 10) {
    static int fpu53 = -1;   // MSVC's runtime runs the x87 with 53-bit precision control; RT_FPU53=0 keeps full 64-bit precision
    if (fpu53 < 0) fpu53 = getenv("RT_FPU53") ? atoi(getenv("RT_FPU53")) : 1;
    if (fpu53) v = (long double)(double)v;
    memset(p, 0, 10); memcpy(p, &v, 10);
  }
  else rt_fatal("float size %d", sz);
}
static void bput(int osz, void* o, int v) { memset(o, 0, osz); *(uint8_t*)o = (uint8_t)v; }

#define FCMP(name, expr) \
  void rt_float_##name(int osz, void* o, int asz, const void* a, int bsz, const void* b) { \
    long double x = fget(asz, a), y = fget(bsz, b); bput(osz, o, (expr)); }
FCMP(equal, x == y)
FCMP(notequal, x != y)
FCMP(less, x < y)
FCMP(lessequal, x <= y)
#define FBIN(name, expr) \
  void rt_float_##name(int osz, void* o, int asz, const void* a, int bsz, const void* b) { \
    long double x = fget(asz, a), y = fget(bsz, b); fput(osz, o, (expr)); }
FBIN(add, x + y)
FBIN(sub, x - y)
FBIN(mult, x * y)
FBIN(div, x / y)
void rt_float_nan(int osz, void* o, int asz, const void* a) { bput(osz, o, isnan(fget(asz, a))); }
void rt_float_neg(int osz, void* o, int asz, const void* a) { fput(osz, o, -fget(asz, a)); }
void rt_float_abs(int osz, void* o, int asz, const void* a) { fput(osz, o, fabsl(fget(asz, a))); }
void rt_float_sqrt(int osz, void* o, int asz, const void* a) { fput(osz, o, sqrtl(fget(asz, a))); }
void rt_float2float(int osz, void* o, int asz, const void* a) { fput(osz, o, fget(asz, a)); }
void rt_int2float(int osz, void* o, int asz, const void* a) {
  int64_t v = 0;
  switch (asz) {
    case 1: v = *(const int8_t*)a; break;
    case 2: v = *(const int16_t*)a; break;
    case 4: v = *(const int32_t*)a; break;
    case 8: v = *(const int64_t*)a; break;
  }
  fput(osz, o, (long double)v);
}
static void iput(int osz, void* o, int64_t v) { memcpy(o, &v, osz); }  // little-endian truncation
void rt_trunc(int osz, void* o, int asz, const void* a) { iput(osz, o, (int64_t)truncl(fget(asz, a))); }
void rt_ceil(int osz, void* o, int asz, const void* a) { fput(osz, o, ceill(fget(asz, a))); }
void rt_floor(int osz, void* o, int asz, const void* a) { fput(osz, o, floorl(fget(asz, a))); }
void rt_round(int osz, void* o, int asz, const void* a) { fput(osz, o, nearbyintl(fget(asz, a))); }

void rt_userop(CPU* c, const char* name, void* out, int outsz, const void** in, const int* insz, int nin) {
  (void)c; (void)insz;
  if (!strcmp(name, "LOCK") || !strcmp(name, "UNLOCK")) return;
  if (!strcmp(name, "rdtsc")) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t t = (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
    if (out) memcpy(out, &t, outsz < 8 ? outsz : 8);
    return;
  }
  if (!strcmp(name, "paddusb") && nin == 2 && out) {
    const uint8_t* x = in[0]; const uint8_t* y = in[1]; uint8_t* r = out;
    for (int i = 0; i < outsz; i++) { unsigned s = x[i] + y[i]; r[i] = s > 255 ? 255 : s; }
    return;
  }
  rt_fatal("unimplemented userop %s", name);
}

// ------------------------------------------------------------------ loader
static uint16_t r16(const uint8_t* p) { return p[0] | p[1] << 8; }
static uint32_t r32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

int rt_load_pe(const char* path, uint32_t* entry_out) {
  FILE* f = fopen(path, "rb");
  if (!f) return 0;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t* d = malloc(n);
  if (fread(d, 1, n, f) != (size_t)n) { fclose(f); free(d); return 0; }
  fclose(f);
  uint32_t pe = r32(d + 0x3c);
  if (memcmp(d + pe, "PE\0\0", 4)) return 0;
  int nsec = r16(d + pe + 6);
  int opt = r16(d + pe + 20);
  uint32_t base = r32(d + pe + 52);
  *entry_out = base + r32(d + pe + 40);
  memcpy(g_mem + base, d, r32(d + pe + 84) < 0x1000 ? r32(d + pe + 84) : 0x1000);  // headers
  for (int i = 0; i < nsec; i++) {
    const uint8_t* s = d + pe + 24 + opt + 40 * i;
    uint32_t vs = r32(s + 8), va = r32(s + 12), rs = r32(s + 16), ro = r32(s + 20);
    memcpy(g_mem + base + va, d + ro, rs < vs ? rs : vs);
  }
  // imports
  uint32_t imp = r32(d + pe + 24 + 96 + 8);
  if (imp) {
    for (const uint8_t* e = g_mem + base + imp; r32(e + 12); e += 20) {
      const char* dll = (const char*)(g_mem + base + r32(e + 12));
      uint32_t ol = r32(e), ft = r32(e + 16);
      for (uint32_t k = 0;; k++) {
        uint32_t thunk_rva = ol ? ol : ft;
        uint32_t v = r32(g_mem + base + thunk_rva + 4 * k);
        if (!v) break;
        char namebuf[96];
        const char* name;
        if (v & 0x80000000u) { snprintf(namebuf, sizeof namebuf, "#%u", v & 0xffff); name = namebuf; }
        else name = (const char*)(g_mem + base + v + 2);
        uint32_t t = rt_shim_thunk(dll, name);
        if (!t) {
          rt_log("import not implemented: %s!%s", dll, name);
          Shim missing = {strdup(dll), strdup(name), 0, NULL};  // fatal with its name when called
          rt_register_shims(&missing, 1);
          t = rt_shim_thunk(dll, name);
        }
        GU32(base + ft + 4 * k) = t;
      }
    }
  }
  free(d);
  return 1;
}

extern void rt_win32_init(CPU* main_cpu, const char* exe_path);

char** rt_saved_argv;
static CPU* main_cpu_ptr;
static void dump_main_cpu(FILE* f) {
  if (!main_cpu_ptr) return;
  CPU* c = main_cpu_ptr;
  fprintf(f, "eax=%08x ecx=%08x edx=%08x ebx=%08x esp=%08x ebp=%08x esi=%08x edi=%08x fs_base=%08x\n", GREG(c, R_EAX), GREG(c, R_ECX), GREG(c, R_EDX), GREG(c, R_EBX), GREG(c, R_ESP), GREG(c, R_EBP), GREG(c, R_ESI), GREG(c, R_EDI), GREG(c, R_FS_OFFSET));
  uint32_t sp = GREG(c, R_ESP);
  if (sp > 0x1000 && sp < 0xF0000000u) for (int k = 0; k < 16; k++) fprintf(f, "  [esp+%02x] = %08x\n", 4 * k, GU32(sp + 4 * k));
  fprintf(f, "heap_top=%08x\n", heap_top);
}
extern void settings_init(const char* exe_path);
extern void overlay_set_argv0(const char* a);
static void* game_thread_main(void* arg) {
  struct { const char* exe; uint32_t entry; }* gs = arg;
  rt_win32_init(main_cpu_ptr, gs->exe);
  rt_call_guest(main_cpu_ptr, gs->entry, 0);
  fflush(NULL);
  exit(0);
  return NULL;
}
void rt_run_main(const char* exe_path, int argc, char** argv) {
  (void)argc;
  rt_saved_argv = argv;
  overlay_set_argv0(argv[0]);
  settings_init(exe_path);
  dbg_init();
  g_mem = mmap(NULL, 0x100000000ull, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (g_mem == MAP_FAILED) rt_fatal("cannot reserve 4 GiB of guest address space");
  build_fn_hash();
  extern void rt_register_all_shims(void);
  rt_register_all_shims();
  uint32_t entry;
  if (!rt_load_pe(exe_path, &entry)) rt_fatal("cannot load %s", exe_path);
  static CPU cpu;
  main_cpu_ptr = &cpu;
  dbg_register_dumper("guest main CPU", dump_main_cpu);
  DBG("init", "exe=%s guest image mapped, entry=%08x, heap base=%08x", exe_path, entry, HEAP_BASE);
  if (getenv("RT_SELFTEST")) {
    rt_win32_init(&cpu, exe_path);
    uint32_t str = gmalloc(64);
    strcpy((char*)GPTR(str), "Hello player\nsecond line");
    for (int k = 0; k < 8; k++) {   // misaligned starts exercise the word-at-a-time path
      uint32_t r = rt_call_guest(&cpu, 0x4818e0, 2, str + k, 10);
      rt_log("strchr(str+%d) = %08x, str=%08x, expected %08x", k, r, str, str + 12);
    }
    return;
  }
  // The process main thread owns the window, input and presentation (smooth pointer, vsync); the game runs on its own thread.
  static struct { const char* exe; uint32_t entry; } gs;
  gs.exe = exe_path; gs.entry = entry;
  backend_main_init();
  pthread_attr_t at;
  pthread_attr_init(&at);
  pthread_attr_setstacksize(&at, 512u << 20);
  pthread_t gt;
  if (pthread_create(&gt, &at, game_thread_main, &gs) != 0) rt_fatal("cannot start the game thread");
  backend_main_loop();
}

#ifdef RT_TRACE_CALLS
#include <pthread.h>
#include <unistd.h>
static uint32_t call_counts[0x140000];
static pthread_t trace_thread;
static int trace_started;
static void* trace_dump_loop(void* arg) {
  (void)arg;
  const char* path = getenv("RT_CALLS_FILE");
  if (!path) path = "calls.txt";
  for (;;) {
    sleep(1);
    FILE* f = fopen(path, "w");
    if (!f) continue;
    for (uint32_t i = 0; i < sizeof call_counts / sizeof *call_counts; i++)
      if (call_counts[i]) fprintf(f, "%08x %u\n", 0x400000u + i, call_counts[i]);
    fclose(f);
  }
  return NULL;
}
void rt_trace_enter(uint32_t addr) {
  if (!trace_started) { trace_started = 1; pthread_create(&trace_thread, NULL, trace_dump_loop, NULL); }
  uint32_t i = addr - 0x400000u;
  if (i < sizeof call_counts / sizeof *call_counts) __sync_fetch_and_add(&call_counts[i], 1);
}
#endif

// Debug helper (script command "scan N"): list guest addresses holding 32-bit value N, tagged with the side struct
// array (base at guest 0x4dd2f4, stride 0x69c) when they fall inside it.
uint32_t rt_heap_top(void) { return heap_top; }
#include <sys/mman.h>
void rt_objscan(const char* vtfile, const char* outfile) {
  static uint8_t is_vt[0x100000];   // vtable addresses are in 0x48f000..0x4c4000: index (addr - 0x400000) >> 2 would be big; use a hash-free bitmap
  memset(is_vt, 0, sizeof is_vt);
  FILE* f = fopen(vtfile, "r");
  if (!f) return;
  unsigned v;
  while (fscanf(f, "%x", &v) == 1) if (v >= 0x400000 && v < 0x800000) is_vt[(v - 0x400000) >> 2 >> 3] |= 1u << (((v - 0x400000) >> 2) & 7);
  fclose(f);
  FILE* o = fopen(outfile, "w");
  if (!o) return;
  struct { uint32_t lo, hi; } rg[2] = {{0x10000000u, heap_top}, {0x50000000u, 0x6e000000u}};
  for (int r = 0; r < 2; r++) {
    for (uint64_t pg = rg[r].lo & ~0xfffu; pg < rg[r].hi; pg += 0x1000) {
      unsigned char vec;
      if (mincore(g_mem + pg, 0x1000, &vec) != 0 || !(vec & 1)) continue;
      for (uint32_t a = (uint32_t)pg; a < pg + 0x1000; a += 4) {
        uint32_t x = GU32(a);
        if (x >= 0x400000 && x < 0x800000 && (is_vt[(x - 0x400000) >> 2 >> 3] >> (((x - 0x400000) >> 2) & 7) & 1)) { fprintf(o, "%08x %08x", a, x); for (int k = 1; k < 48; k++) fprintf(o, " %08x", GU32(a + 4 * k)); fputc('\n', o); }
      }
    }
  }
  fclose(o);
}
void rt_debug_scan(uint32_t value) {
  uint32_t side_base = GU32(0x4dd2f4);
  rt_log("scan %u: sides base=%08x", value, side_base);
  int found = 0;
  for (uint32_t a = 0x49b000; a < 0x531000; a += 4)
    if (GU32(a) == value) { rt_log("  data  %08x", a); found++; }
  for (uint32_t a = 0x10000000; a < heap_top && found < 400; a += 4)
    if (GU32(a) == value) {
      if (side_base && a >= side_base && a < side_base + 0x69c * 8)
        rt_log("  SIDE[%u]+%#x  (%08x)", (a - side_base) / 0x69c, (a - side_base) % 0x69c, a);
      else { found++; rt_log("  heap  %08x", a); }
    }
  rt_log("scan done (%d other hits)", found);
}

static uint32_t snap_addrs[100000];
static int snap_n;
static uint32_t snap_value;
void rt_debug_snap(uint32_t value) {   // remember every address currently holding `value`
  snap_n = 0; snap_value = value;
  for (uint32_t a = 0x49b000; a < 0x531000 && snap_n < 100000; a += 4) if (GU32(a) == value) snap_addrs[snap_n++] = a;
  for (uint32_t a = 0x10000000; a < heap_top && snap_n < 100000; a += 4) if (GU32(a) == value) snap_addrs[snap_n++] = a;
  rt_log("snap: %d addresses hold %u", snap_n, value);
}
void rt_debug_diff(void) {              // report remembered addresses whose value changed
  uint32_t side_base = GU32(0x4dd2f4);
  int shown = 0;
  for (int i = 0; i < snap_n; i++) {
    uint32_t v = GU32(snap_addrs[i]);
    if (v != snap_value && shown < 60) {
      uint32_t a = snap_addrs[i];
      if (side_base && a >= side_base && a < side_base + 0x69c * 8) rt_log("  changed SIDE[%u]+%#x: %u -> %u", (a - side_base) / 0x69c, (a - side_base) % 0x69c, snap_value, v);
      else rt_log("  changed %08x: %u -> %u", a, snap_value, v);
      shown++;
    }
  }
  rt_log("diff done, %d shown", shown);
}

void rt_debug_scan16(uint32_t value) {   // 16-bit aligned scan inside the side array and the heap (first hits only)
  uint32_t side_base = GU32(0x4dd2f4);
  rt_log("scan16 %u: sides base=%08x", value, side_base);
  int n = 0;
  if (side_base)
    for (uint32_t a = side_base; a < side_base + 0x69c * 4; a += 2)
      if (GU16(a) == (value & 0xffff)) { rt_log("  SIDE[%u]+%#x", (a - side_base) / 0x69c, (a - side_base) % 0x69c); n++; }
  rt_log("scan16 done (%d)", n);
}

static uint8_t* fsnap_data;
static uint32_t fsnap_len;
void rt_debug_fsnap(void) {   // copy of the whole guest heap
  free(fsnap_data);
  fsnap_len = heap_top - 0x10000000u;
  fsnap_data = malloc(fsnap_len);
  memcpy(fsnap_data, g_mem + 0x10000000u, fsnap_len);
  rt_log("fsnap: %u bytes", fsnap_len);
}
void rt_debug_fdiff(int delta) {   // addresses whose 32-bit value dropped by exactly `delta`
  int shown = 0;
  uint32_t side_base = GU32(0x4dd2f4);
  for (uint32_t off = 0; off + 4 <= fsnap_len && shown < 40; off += 4) {
    uint32_t old = *(uint32_t*)(fsnap_data + off), now = GU32(0x10000000u + off);
    if (old - now == (uint32_t)delta && old > 100 && old < 100000) {
      uint32_t a = 0x10000000u + off;
      rt_log("  %08x: %u -> %u%s", a, old, now, (side_base && a >= side_base && a < side_base + 0x69c * 8) ? "  (in SIDE array)" : "");
      shown++;
    }
  }
  rt_log("fdiff done (%d)", shown);
}

void rt_debug_fdiffd(void) {   // 8-byte doubles in 1000..2000 that decreased since fsnap
  int shown = 0;
  uint32_t side_base = GU32(0x4dd2f4);
  for (uint32_t off = 0; off + 8 <= fsnap_len && shown < 40; off += 4) {
    double o, n;
    memcpy(&o, fsnap_data + off, 8);
    memcpy(&n, g_mem + 0x10000000u + off, 8);
    if (o > 900 && o < 2000 && n < o && o - n < 300) {
      uint32_t a = 0x10000000u + off;
      rt_log("  double %08x: %.3f -> %.3f%s", a, o, n, (side_base && a >= side_base && a < side_base + 0x69c * 8) ? "  (in SIDE array)" : "");
      shown++;
    }
  }
  rt_log("fdiffd done (%d)", shown);
}

void rt_debug_fdifff(void) {   // 32-bit floats in 900..2000 that decreased since fsnap
  int shown = 0;
  uint32_t side_base = GU32(0x4dd2f4);
  for (uint32_t off = 0; off + 4 <= fsnap_len && shown < 40; off += 4) {
    float o, n;
    memcpy(&o, fsnap_data + off, 4);
    memcpy(&n, g_mem + 0x10000000u + off, 4);
    if (o > 900 && o < 2000 && n < o && o - n < 300) {
      uint32_t a = 0x10000000u + off;
      rt_log("  float %08x: %.3f -> %.3f%s", a, o, n, (side_base && a >= side_base && a < side_base + 0x69c * 8) ? "  (in SIDE array)" : "");
      shown++;
    }
  }
  rt_log("fdifff done (%d)", shown);
}

static uint8_t dsnap_data[0x531000 - 0x49b000];
void rt_debug_dsnap(void) { memcpy(dsnap_data, g_mem + 0x49b000, sizeof dsnap_data); rt_log("dsnap done"); }
void rt_debug_ddiff(void) {   // globals (.data/.bss) that decreased as int / float / double since dsnap, money-like ranges
  int shown = 0;
  for (uint32_t off = 0; off + 8 <= sizeof dsnap_data && shown < 60; off += 4) {
    uint32_t a = 0x49b000 + off;
    int32_t oi = *(int32_t*)(dsnap_data + off), ni = (int32_t)GU32(a);
    float of, nf; double od, nd;
    memcpy(&of, dsnap_data + off, 4); memcpy(&nf, g_mem + a, 4);
    memcpy(&od, dsnap_data + off, 8); memcpy(&nd, g_mem + a, 8);
    if (oi > 900 && oi < 100000000 && ni < oi && oi - ni < oi / 5) { rt_log("  int    %08x: %d -> %d", a, oi, ni); shown++; }
    else if (of > 900 && of < 2000 && nf < of && of - nf < 300) { rt_log("  float  %08x: %.3f -> %.3f", a, of, nf); shown++; }
    else if (od > 900 && od < 2000 && nd < od && od - nd < 300) { rt_log("  double %08x: %.3f -> %.3f", a, od, nd); shown++; }
  }
  rt_log("ddiff done (%d)", shown);
}
