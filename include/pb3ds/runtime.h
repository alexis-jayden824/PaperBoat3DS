#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * One APT-side gameplay loop. When PB3DS_GAME_OBJECTS is defined and both
 * .o2r archives are present, M13 steps step_game_loop / gfx_draw_frame
 * instead of the M0 teal shell. boot_main's infinite nuGfx wait is not used.
 *
 * pb_runtime_play_query is named separately from M4 pb_runtime_query
 * (PBRuntimeStatus).
 */
typedef struct {
    bool game_linked;
    bool assets_ready;
    bool playing;
    bool started;
    uint32_t steps;
    uint32_t draws;
} PBRuntimePlay;

void pb_runtime_init(void);
void pb_runtime_shutdown(void);
void pb_runtime_frame(unsigned game_ticks);
void pb_runtime_play_query(PBRuntimePlay *state);
bool pb_runtime_playing(void);
bool pb_runtime_game_linked(void);
bool pb_runtime_quit_combo(uint32_t held);

#ifdef __cplusplus
}
#endif
