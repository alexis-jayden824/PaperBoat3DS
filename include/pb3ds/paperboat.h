#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * M5 links a small PaperBoat 1.0.1 slice (libc shims + Yay0). M13 steps
 * `step_game_loop` / `gfx_draw_frame` only when `PB3DS_GAME_OBJECTS` is
 * defined. `boot_main` is never linked (infinite nuGfx wait). Desktop
 * Engine.cpp, SDL, libultraship, and Torch stay out of the ARM11 binary.
 */
#ifndef PB3DS_PAPERBOAT_COMMIT
#define PB3DS_PAPERBOAT_COMMIT "unfetched"
#endif

#ifndef PB3DS_PAPERBOAT_RELEASE
#define PB3DS_PAPERBOAT_RELEASE "1.0.1"
#endif

bool pb_paperboat_slice_linked(void);
const char *pb_paperboat_commit(void);
const char *pb_paperboat_release(void);
void pb_paperboat_decode_yay0(void *src, void *dst);
int pb_paperboat_printf_shim(char *dst, size_t dst_size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif
