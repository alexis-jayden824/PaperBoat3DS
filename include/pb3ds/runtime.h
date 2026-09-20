#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pb3ds/gfx_rendering_api_3ds.h"
#include "pb3ds/input.h"
#include "pb3ds/log.h"
#include "pb3ds/runtime_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PB_RUNTIME_INACTIVE = 0,
    PB_RUNTIME_LOADING,
    PB_RUNTIME_ACTIVE,
    PB_RUNTIME_FAILED,
} PBRuntimeState;

typedef struct {
    uint64_t updates;
    uint64_t frames_submitted;
    uint64_t held_frames;
    uint32_t unsupported_mode;
    uint32_t platform_warnings;
    int32_t game_mode;
    int32_t area_id;
    int32_t map_id;
    int32_t entry_id;
    float player_x;
    float player_y;
    float player_z;
    float player_speed;
    int8_t player_action;
} PBRuntimeStats;

typedef struct {
    PBRuntimeResources resources;
    PBInputState *input;
    PBGfxApi3DS *graphics;
    PBLog *log;
    PBRuntimeState state;
    PBRuntimeStats stats;
    const char *error;
    bool frame_submitted;
} PBRuntime;

void pb_runtime_init(PBRuntime *runtime, PBArchive *archive,
                     PBMemoryMonitor *memory, PBInputState *input,
                     PBGfxApi3DS *graphics, PBLog *log);
bool pb_runtime_start_toad_town(PBRuntime *runtime);
bool pb_runtime_update(PBRuntime *runtime);
void pb_runtime_shutdown(PBRuntime *runtime);
const char *pb_runtime_state_name(PBRuntimeState state);

#ifdef __cplusplus
}
#endif
