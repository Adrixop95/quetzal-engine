// IDirectInput / IDirectInputDevice (keyboard + mouse) on the SDL backend.
#include <stdlib.h>
#include <string.h>

#include "aztec_rt.h"
#include "backend.h"

enum { DI_OK = 0, DIERR_NOINTERFACE = 0x80004002u };

typedef struct { int is_mouse; uint8_t prev_keys[256]; uint8_t prev_btn[3]; uint32_t seq; } DiDev;

static uint32_t com_addref(CPU* c, uint32_t* a) { (void)c; (void)a; return 2; }
static uint32_t com_release(CPU* c, uint32_t* a) { (void)c; (void)a; return 1; }
static uint32_t com_qi(CPU* c, uint32_t* a) { (void)c; GU32(a[2]) = a[0]; return DI_OK; }
static uint32_t di_ok(CPU* c, uint32_t* a) { (void)c; (void)a; return DI_OK; }

static uint32_t dev_getstate(CPU* c, uint32_t* a) {  // size, buf
  (void)c;
  DiDev* d = com_host(a[0]);
  rt_pump_events();
  if (!d->is_mouse) {
    uint32_t n = a[1] < 256 ? a[1] : 256;
    for (uint32_t i = 0; i < n; i++) GU8(a[2] + i) = g_dik_down[i] ? 0x80 : 0;
  } else {
    memset(GPTR(a[2]), 0, a[1]);
    GU32(a[2]) = (uint32_t)g_mouse_dx; GU32(a[2] + 4) = (uint32_t)g_mouse_dy;
    for (int b = 0; b < 3; b++) if (a[1] > 12u + b) GU8(a[2] + 12 + b) = g_mouse_btn[b] ? 0x80 : 0;
    g_mouse_dx = g_mouse_dy = 0;
  }
  return DI_OK;
}

static uint32_t dev_getdata(CPU* c, uint32_t* a) {  // objdatasize, data, *numelements, flags
  (void)c;
  DiDev* d = com_host(a[0]);
  rt_pump_events();
  uint32_t max = GU32(a[3]), n = 0;
  uint32_t stride = a[1] ? a[1] : 16;
  if (!d->is_mouse) {
    for (int k = 0; k < 256 && n < max; k++) {
      if (g_dik_down[k] != d->prev_keys[k]) {
        uint32_t p = a[2] + n * stride;
        GU32(p) = (uint32_t)k; GU32(p + 4) = g_dik_down[k] ? 0x80 : 0; GU32(p + 8) = 0; GU32(p + 12) = d->seq++;
        d->prev_keys[k] = g_dik_down[k];
        n++;
      }
    }
  } else {
    if ((g_mouse_dx || g_mouse_dy) && n + 2 <= max) {
      uint32_t p = a[2] + n * stride;
      GU32(p) = 0; GU32(p + 4) = (uint32_t)g_mouse_dx; GU32(p + 8) = 0; GU32(p + 12) = d->seq++; n++;
      p = a[2] + n * stride;
      GU32(p) = 4; GU32(p + 4) = (uint32_t)g_mouse_dy; GU32(p + 8) = 0; GU32(p + 12) = d->seq++; n++;
      g_mouse_dx = g_mouse_dy = 0;
    }
    for (int b = 0; b < 3 && n < max; b++) {
      if (g_mouse_btn[b] != d->prev_btn[b]) {
        uint32_t p = a[2] + n * stride;
        GU32(p) = 12 + (uint32_t)b; GU32(p + 4) = g_mouse_btn[b] ? 0x80 : 0; GU32(p + 8) = 0; GU32(p + 12) = d->seq++;
        d->prev_btn[b] = g_mouse_btn[b];
        n++;
      }
    }
  }
  GU32(a[3]) = n;
  return DI_OK;
}

static uint32_t dev_getcaps(CPU* c, uint32_t* a) {
  (void)c;
  DiDev* d = com_host(a[0]);
  memset(GPTR(a[1] + 4), 0, GU32(a[1]) - 4);
  GU32(a[1] + 4) = 1;  // DIDC_ATTACHED
  GU32(a[1] + 8) = d->is_mouse ? 0x2 : 0x3;
  return DI_OK;
}

static const ComMethod dev_methods[] = {
  {"QueryInterface", 2, com_qi}, {"AddRef", 0, com_addref}, {"Release", 0, com_release}, {"GetCapabilities", 1, dev_getcaps},
  {"EnumObjects", 3, NULL}, {"GetProperty", 2, NULL}, {"SetProperty", 2, di_ok}, {"Acquire", 0, di_ok}, {"Unacquire", 0, di_ok},
  {"GetDeviceState", 2, dev_getstate}, {"GetDeviceData", 4, dev_getdata}, {"SetDataFormat", 1, di_ok},
  {"SetEventNotification", 1, di_ok}, {"SetCooperativeLevel", 2, di_ok}, {"GetObjectInfo", 3, NULL},
  {"GetDeviceInfo", 1, NULL}, {"RunControlPanel", 2, di_ok}, {"Initialize", 3, di_ok},
};

static uint32_t di_createdevice(CPU* c, uint32_t* a) {  // guid*, **dev, outer
  (void)c;
  DiDev* d = calloc(1, sizeof *d);
  d->is_mouse = GU32(a[1]) == 0x6F1D2B60u;  // GUID_SysMouse data1; keyboard is ...61
  GU32(a[2]) = com_create(dev_methods, 18, d);
  return DI_OK;
}
static uint32_t di_enumdevices(CPU* c, uint32_t* a) { (void)c; (void)a; return DI_OK; }  // report none

static const ComMethod di_methods[] = {
  {"QueryInterface", 2, com_qi}, {"AddRef", 0, com_addref}, {"Release", 0, com_release},
  {"CreateDevice", 3, di_createdevice}, {"EnumDevices", 4, di_enumdevices}, {"GetDeviceStatus", 1, di_ok},
  {"RunControlPanel", 2, di_ok}, {"Initialize", 3, di_ok},
};

static uint32_t sh_DirectInputCreateA(CPU* c, uint32_t* a) {  // hinst, version, **ppDI, outer
  (void)c;
  GU32(a[2]) = com_create(di_methods, 8, NULL);
  return DI_OK;
}

void rt_register_dinput_shims(void) {
  static const Shim s[] = {{"DINPUT.dll", "DirectInputCreateA", 4, sh_DirectInputCreateA}};
  rt_register_shims(s, 1);
}
