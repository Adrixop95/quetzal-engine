// Guest memory, allocator, dispatcher, PE loader, x87-ish float helpers.
#define _GNU_SOURCE
#include "aztec_rt.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

uint8_t* g_mem;

// ------------------------------------------------------------------ logging
void rt_log(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}
void rt_fatal(const char* fmt, ...) {
  va_list ap;
  fputs("FATAL: ", stderr);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  fflush(stderr);
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
  h->cls = k;
  h->size = size;
  return blk + 8;
}
void gfree(uint32_t ptr) {
  if (!ptr) return;
  uint32_t blk = ptr - 8;
  BlockHdr* h = (BlockHdr*)GPTR(blk);
  GU32(blk + 8) = free_lists[h->cls];
  free_lists[h->cls] = blk;
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
  uint32_t r = fn(c, a);
  GREG(c, R_EAX) = r;
  GREG(c, R_ESP) = esp + 4 + (nargs > 0 ? 4u * nargs : 0);
}

void f_dispatch(CPU* c, uint32_t addr) {
  if (addr >= SHIM_BASE) { shim_call(c, addr); return; }
  void (*fn)(CPU*) = lookup_fn(addr);
  if (!fn) rt_fatal("call to unlifted guest address %08x", addr);
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
  else if (sz == 10) { memset(p, 0, 10); memcpy(p, &v, 10); }
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

void rt_run_main(const char* exe_path, int argc, char** argv) {
  (void)argc; (void)argv;
  g_mem = mmap(NULL, 0x100000000ull, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (g_mem == MAP_FAILED) rt_fatal("cannot reserve 4 GiB of guest address space");
  build_fn_hash();
  extern void rt_register_all_shims(void);
  rt_register_all_shims();
  uint32_t entry;
  if (!rt_load_pe(exe_path, &entry)) rt_fatal("cannot load %s", exe_path);
  static CPU cpu;
  rt_win32_init(&cpu, exe_path);
  rt_call_guest(&cpu, entry, 0);
}
