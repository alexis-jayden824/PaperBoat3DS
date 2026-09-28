#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pb3ds/o2r.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PB_FIRST_FRAME_SOURCE_WIDTH 296U
#define PB_FIRST_FRAME_SOURCE_HEIGHT 200U
#define PB_FIRST_FRAME_TEXTURE_WIDTH 512U
#define PB_FIRST_FRAME_TEXTURE_HEIGHT 256U

typedef enum {
    PB_FIRST_FRAME_NOT_ATTEMPTED = 0,
    PB_FIRST_FRAME_READY,
    PB_FIRST_FRAME_ARCHIVE_MISSING,
    PB_FIRST_FRAME_RESOURCE_MISSING,
    PB_FIRST_FRAME_ARCHIVE_ERROR,
    PB_FIRST_FRAME_TEXTURE_INVALID,
    PB_FIRST_FRAME_PALETTE_INVALID,
    PB_FIRST_FRAME_OUT_OF_MEMORY,
} PBFirstFrameResult;

typedef struct {
    PBFirstFrameResult result;
    PBO2RResult archive_result;
    PBO2RStats archive_stats;
    uint8_t *rgba;
    size_t rgba_size;
    uint16_t source_width;
    uint16_t source_height;
    uint16_t texture_width;
    uint16_t texture_height;
} PBFirstFrame;

void pb_first_frame_init(PBFirstFrame *frame);
PBFirstFrameResult pb_first_frame_load(PBFirstFrame *frame,
                                       PBArchive *game_archive,
                                       PBMemoryMonitor *memory);
void pb_first_frame_release_pixels(PBFirstFrame *frame,
                                   PBMemoryMonitor *memory);
const char *pb_first_frame_result_name(PBFirstFrameResult result);

#ifdef __cplusplus
}
#endif
