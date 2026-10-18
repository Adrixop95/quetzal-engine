// IDirectInput / IDirectInputDevice (keyboard + mouse) on the SDL backend.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aztec_rt.h"
#include "backend.h"

enum { DI_OK = 0, DIERR_NOINTERFACE = 0x80004002u };

typedef struct { uint32_t ofs; int kind; int index; } DiObj;   // kind: 0 X, 1 Y, 2 Z, 3 button, 4 key
typedef struct { int is_mouse; int n_objs; DiObj objs[300]; uint32_t datasize; uint8_t prev_keys[256]; uint8_t prev_btn[3]; uint32_t seq; uint32_t event; uint8_t sig_keys[256]; uint8_t sig_btn[3]; } DiDev;
static DiDev* devs[8];
static int n_devs;
extern void rt_signal_event(uint32_t h);

// Called regularly from the event pump: signal a device's notification event when its state changed.
void rt_dinput_poll(void) {
  for (int i = 0; i < n_devs; i++) {
    DiDev* d = devs[i];
    if (!d->event) continue;
    int changed = 0;
    if (d->is_mouse) {
      if (g_mouse_dx || g_mouse_dy) changed = 1;
      for (int b = 0; b < 3; b++) if (g_mouse_btn[b] != d->sig_btn[b]) { d->sig_btn[b] = g_mouse_btn[b]; changed = 1; }
    } else {
      for (int k = 0; k < 256; k++) if (g_dik_down[k] != d->sig_keys[k]) { d->sig_keys[k] = g_dik_down[k]; changed = 1; }
    }
    if (changed) rt_signal_event(d->event);
  }
}

static uint32_t dev_seteventnotification(CPU* c, uint32_t* a) { (void)c; DiDev* d = com_host(a[0]); d->event = a[1]; return DI_OK; }

static uint32_t com_addref(CPU* c, uint32_t* a) { (void)c; (void)a; return 2; }
static uint32_t com_release(CPU* c, uint32_t* a) { (void)c; (void)a; return 1; }
static uint32_t com_qi(CPU* c, uint32_t* a) { (void)c; GU32(a[2]) = a[0]; return DI_OK; }
static uint32_t di_ok(CPU* c, uint32_t* a) { (void)c; (void)a; return DI_OK; }

static uint32_t dev_setdataformat(CPU* c, uint32_t* a) {
  (void)c;
  DiDev* d = com_host(a[0]);
  uint32_t df = a[1];
  uint32_t nobj = GU32(df + 16), rg = GU32(df + 20), osz = GU32(df + 4);
  d->datasize = GU32(df + 12);
  d->n_objs = 0;
  if (getenv("RT_TRACE_INPUT")) rt_log("SetDataFormat dev(mouse=%d): size=%u nobj=%u objsz=%u flags=%08x", d->is_mouse, d->datasize, nobj, osz, GU32(df + 8));
  int btn = 0;
  for (uint32_t i = 0; i < nobj && d->n_objs < 300; i++) {
    uint32_t o = rg + i * osz;
    uint32_t pguid = GU32(o), ofs = GU32(o + 4), type = GU32(o + 8);
    DiObj ob = {ofs, 3, 0};
    uint32_t g1 = pguid ? GU32(pguid) : 0;
    uint32_t inst = (type >> 8) & 0xFFFF;
    if (g1 == 0xA36D02E0u) ob.kind = 0;
    else if (g1 == 0xA36D02E1u) ob.kind = 1;
    else if (g1 == 0xA36D02E2u) ob.kind = 2;
    else if (d->is_mouse) { ob.kind = 3; ob.index = inst != 0xFFFF ? (int)inst : btn; btn++; }
    else { ob.kind = 4; ob.index = (int)inst; }
    if (getenv("RT_TRACE_INPUT") && i < 12) rt_log("  obj %u: guid1=%08x ofs=%u type=%08x -> kind=%d idx=%d", i, g1, ofs, type, ob.kind, ob.index);
    d->objs[d->n_objs++] = ob;
  }
  return DI_OK;
}

static uint32_t dev_getstate(CPU* c, uint32_t* a) {  // size, buf
  (void)c;
  DiDev* d = com_host(a[0]);
  rt_pump_events();
  memset(GPTR(a[2]), 0, a[1]);
  if (d->n_objs == 0) {  // no format set: legacy DIMOUSESTATE / 256-byte keyboard
    if (!d->is_mouse) { for (uint32_t i = 0; i < a[1] && i < 256; i++) GU8(a[2] + i) = g_dik_down[i] ? 0x80 : 0; return DI_OK; }
    GU32(a[2]) = (uint32_t)g_mouse_dx; GU32(a[2] + 4) = (uint32_t)g_mouse_dy;
    for (int b = 0; b < 3 && a[1] > 12u + b; b++) GU8(a[2] + 12 + b) = g_mouse_btn[b] ? 0x80 : 0;
    g_mouse_dx = g_mouse_dy = 0;
    return DI_OK;
  }
  for (int i = 0; i < d->n_objs; i++) {
    const DiObj* o = &d->objs[i];
    if (o->ofs + (o->kind <= 2 ? 4u : 1u) > a[1]) continue;
    switch (o->kind) {
      case 0: GU32(a[2] + o->ofs) = (uint32_t)g_mouse_dx; break;
      case 1: GU32(a[2] + o->ofs) = (uint32_t)g_mouse_dy; break;
      case 2: GU32(a[2] + o->ofs) = 0; break;
      case 3: GU8(a[2] + o->ofs) = (o->index < 3 && g_mouse_btn[o->index]) ? 0x80 : 0; break;
      case 4: GU8(a[2] + o->ofs) = (o->index < 256 && g_dik_down[o->index]) ? 0x80 : 0; break;
    }
  }
  if (d->is_mouse) {
    if (getenv("RT_TRACE_INPUT") && (g_mouse_btn[0] || g_mouse_dx || g_mouse_dy)) { rt_log("mouse state size=%u:", a[1]); for (uint32_t q = 0; q < a[1] && q < 24; q += 4) rt_log("   +%u = %08x", q, GU32(a[2] + q)); }
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
  {"GetDeviceState", 2, dev_getstate}, {"GetDeviceData", 4, dev_getdata}, {"SetDataFormat", 1, dev_setdataformat},
  {"SetEventNotification", 1, dev_seteventnotification}, {"SetCooperativeLevel", 2, di_ok}, {"GetObjectInfo", 3, NULL},
  {"GetDeviceInfo", 1, NULL}, {"RunControlPanel", 2, di_ok}, {"Initialize", 3, di_ok},
};

static uint32_t di_createdevice(CPU* c, uint32_t* a) {  // guid*, **dev, outer
  (void)c;
  DiDev* d = calloc(1, sizeof *d);
  d->is_mouse = GU32(a[1]) == 0x6F1D2B60u;  // GUID_SysMouse data1; keyboard is ...61
  if (n_devs < 8) devs[n_devs++] = d;
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
