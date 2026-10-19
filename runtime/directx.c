#include "aztec_rt.h"
extern void rt_register_ddraw_shims(void);
extern void rt_register_dinput_shims(void);
extern void rt_register_dsound_shims(void);
extern void rt_register_winsock_shims(void);
void rt_register_directx_shims(void) {
  rt_register_ddraw_shims();
  rt_register_dinput_shims();
  rt_register_dsound_shims();
  rt_register_winsock_shims();
}
