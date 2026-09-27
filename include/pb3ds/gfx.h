#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Logical top-screen presentation size. Physical PICA layout is M11. */
#define PB_GFX_TOP_WIDTH 400U
#define PB_GFX_TOP_HEIGHT 240U
#define PB_GFX_BOTTOM_WIDTH 320U
#define PB_GFX_BOTTOM_HEIGHT 240U

bool pb_gfx_ready(void);
void pb_gfx_clear_top(uint8_t red, uint8_t green, uint8_t blue);
void pb_gfx_present(void);

#ifdef __cplusplus
}
#endif
