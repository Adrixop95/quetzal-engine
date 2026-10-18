// SDL2 backend shared by the Win32 / DirectX shims.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void backend_create_window(const char* title, int w, int h);
void backend_set_mode(int w, int h, int bpp);
void backend_show_cursor(int show);
// Present a frame. bpp: 8 (uses 256 ARGB palette), 15 (RGB555), 16 (RGB565), 32 (xRGB).
void backend_present(const void* pixels, int pitch, int w, int h, int bpp, const uint32_t* palette);
int backend_vk_state(int vk);
void backend_apply_window_mode(int mode);   // 0 windowed, 1 borderless, 2 fullscreen
void backend_set_smooth(int smooth);

// Input state (DirectInput scan codes)
extern uint8_t g_dik_down[256];
extern int g_mouse_dx, g_mouse_dy, g_mouse_x, g_mouse_y;
extern uint8_t g_mouse_btn[3];

void rt_pump_events(void);
int rt_events_pending(void);
void rt_post_message(uint32_t msg, uint32_t w, uint32_t l);

#ifdef __cplusplus
}
#endif
