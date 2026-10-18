// Runtime for statically recompiled Aztec.exe: guest memory image, CPU register file, shims.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint8_t b[10]; } f80;
typedef struct { uint64_t lo, hi; } u128;

// Ghidra x86 register-space offsets used by the lifter.
enum { R_EAX = 0x00, R_ECX = 0x04, R_EDX = 0x08, R_EBX = 0x0c, R_ESP = 0x10, R_EBP = 0x14,
       R_ESI = 0x18, R_EDI = 0x1c, R_FS_OFFSET = 0x110, R_EIP = 0x284 };

typedef struct CPU {
  uint8_t r[0x2000];
  void* thread;  // host thread info
} CPU;

extern uint8_t* g_mem;  // base of the 4 GiB guest address space

#define GREG(c, off) (*(uint32_t*)((c)->r + (off)))
#define GPTR(a) ((void*)(g_mem + (uint32_t)(a)))
#define GU32(a) (*(uint32_t*)(g_mem + (uint32_t)(a)))
#define GU16(a) (*(uint16_t*)(g_mem + (uint32_t)(a)))
#define GU8(a) (*(uint8_t*)(g_mem + (uint32_t)(a)))

#ifdef RT_TRACE_CALLS
void rt_trace_enter(uint32_t addr);
#define RT_ENTER(a) rt_trace_enter(a)
#else
#define RT_ENTER(a) ((void)0)
#endif

typedef struct { uint32_t addr; void (*fn)(CPU*); } FuncEntry;
extern const FuncEntry g_func_table[];
extern const unsigned g_func_count;

void f_dispatch(CPU* c, uint32_t addr);          // call guest/shim code at addr (CALL semantics)
void rt_unlifted(CPU* c, uint32_t addr);
void rt_fatal(const char* fmt, ...) __attribute__((noreturn));
void rt_log(const char* fmt, ...);

// guest -> host helpers used by shims
uint32_t rt_call_guest(CPU* c, uint32_t addr, int nargs, ...);  // stdcall-style call, returns EAX
void rt_push(CPU* c, uint32_t v);

// guest memory allocator (HeapAlloc & friends)
uint32_t gmalloc(uint32_t size);
void gfree(uint32_t ptr);
uint32_t gmsize(uint32_t ptr);
uint32_t gmrealloc(uint32_t ptr, uint32_t size);
uint32_t guest_cstring(const char* s);   // allocate a NUL-terminated copy in guest memory

// shims
typedef uint32_t (*ShimFn)(CPU* c, uint32_t* a);
typedef struct { const char* dll; const char* name; int nargs; ShimFn fn; } Shim;
void rt_register_shims(const Shim* shims, int n);
uint32_t rt_shim_thunk(const char* dll, const char* name);  // thunk address or 0

// COM objects (DirectDraw/DirectSound/DirectInput)
typedef struct { const char* name; int nargs; ShimFn fn; } ComMethod;  // nargs excludes `this`
uint32_t com_create(const ComMethod* methods, int count, void* host);   // returns guest object pointer
void* com_host(uint32_t obj);

// loader
int rt_load_pe(const char* path, uint32_t* entry_out);
void rt_run_main(const char* exe_path, int argc, char** argv);

// helpers referenced by generated code
static inline uint64_t shl64(uint64_t v, uint64_t s, int bits) { return s >= (uint64_t)bits ? 0 : v << s; }
static inline uint64_t shr64(uint64_t v, uint64_t s, int bits) { return s >= (uint64_t)bits ? 0 : v >> s; }
static inline int64_t sar64(int64_t v, uint64_t s, int bits) { return s >= (uint64_t)bits ? (v < 0 ? -1 : 0) : v >> s; }
static inline uint64_t lzcnt64(uint64_t v, int bits) {
  int n = 0;
  for (int i = bits - 1; i >= 0 && !((v >> i) & 1); i--) n++;
  return n;
}
void rt_userop(CPU* c, const char* name, void* out, int outsz, const void** in, const int* insz, int nin);
void rt_float_equal(int osz, void* o, int asz, const void* a, int bsz, const void* b);
void rt_float_notequal(int osz, void* o, int asz, const void* a, int bsz, const void* b);
void rt_float_less(int osz, void* o, int asz, const void* a, int bsz, const void* b);
void rt_float_lessequal(int osz, void* o, int asz, const void* a, int bsz, const void* b);
void rt_float_add(int osz, void* o, int asz, const void* a, int bsz, const void* b);
void rt_float_sub(int osz, void* o, int asz, const void* a, int bsz, const void* b);
void rt_float_mult(int osz, void* o, int asz, const void* a, int bsz, const void* b);
void rt_float_div(int osz, void* o, int asz, const void* a, int bsz, const void* b);
void rt_float_nan(int osz, void* o, int asz, const void* a);
void rt_float_neg(int osz, void* o, int asz, const void* a);
void rt_float_abs(int osz, void* o, int asz, const void* a);
void rt_float_sqrt(int osz, void* o, int asz, const void* a);
void rt_int2float(int osz, void* o, int asz, const void* a);
void rt_float2float(int osz, void* o, int asz, const void* a);
void rt_trunc(int osz, void* o, int asz, const void* a);
void rt_ceil(int osz, void* o, int asz, const void* a);
void rt_floor(int osz, void* o, int asz, const void* a);
void rt_round(int osz, void* o, int asz, const void* a);

#ifdef __cplusplus
}
#endif
