#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pb3ds/bootstrap.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Paper Mario 64 is a 30 Hz game. 3DS vblank is 60 Hz. */
#define PB_LOOP_TICK_HZ 30U
#define PB_LOOP_TICK_MS 33U
#define PB_LOOP_MAX_CATCHUP 2U

uint64_t pb_time_raw_ms(void);
uint64_t pb_time_ms(void);
void pb_time_wait_vblank(void);

typedef struct {
    uint64_t now_ms;
    uint32_t game_ticks;
    uint32_t display_frames;
    uint32_t steps_this_frame;
    bool paused;
} PBLoopStatus;

void pb_loop_init(void);
void pb_loop_shutdown(void);
void pb_loop_on_apt(PBAptEvent event);
unsigned pb_loop_begin_frame(void);
void pb_loop_hold_frame(void);
void pb_loop_query(PBLoopStatus *status);
bool pb_loop_paused(void);
bool pb_loop_desktop_window_allowed(void);

#ifndef __3DS__
void pb_time_host_set(uint64_t ms);
void pb_time_host_advance(uint64_t ms);
#endif

#ifdef __cplusplus
}
#endif
