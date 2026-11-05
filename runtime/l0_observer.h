// Opt-in synchronous L0 observer. Captures never leave the private run directory.
#pragma once
#include <stdint.h>
void rt_l0_observe(uint32_t tick);
void rt_l0_present(const void* pixels, int pitch, int width, int height, int bpp, const uint32_t* palette);
uint32_t rt_l0_clock(uint32_t wall_ms);
