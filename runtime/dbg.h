// Debug log: enable with RT_DEBUG=1 (file: RT_DEBUG_FILE, default ./aztec_debug.log). Every line carries
// elapsed ms, thread id and a category. Crash/fatal paths dump recent API calls and registered state.
#pragma once
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void dbg_init(void);
int dbg_enabled(void);
void dbg_log(const char* cat, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void dbg_api_record(const char* name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
void dbg_register_dumper(const char* name, void (*fn)(FILE* f));   // module state printed on crash / on demand
void dbg_dump_context(const char* why);
void dbg_fatal_hook(const char* msg);                               // used by rt_fatal

#define DBG(cat, ...) do { if (dbg_enabled()) dbg_log(cat, __VA_ARGS__); } while (0)
// rate limited: logs the 1st, (n+1)th, ... call from this call site
#define DBG_EVERY(n, cat, ...) do { static unsigned _dbg_c; if (dbg_enabled() && (_dbg_c++ % (unsigned)(n)) == 0) dbg_log(cat, __VA_ARGS__); } while (0)

#ifdef __cplusplus
}
#endif
