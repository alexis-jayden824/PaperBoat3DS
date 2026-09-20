#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pb3ds/input.h"
#include "pb3ds/o2r.h"
#include "pb3ds/texture.h"
#include "pb3ds/title_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PB_FILE_SELECT_SLOT_COUNT 4U

typedef enum {
    PB_TITLE_ASSETS_NOT_ATTEMPTED = 0,
    PB_TITLE_ASSETS_READY,
    PB_TITLE_ASSETS_ARCHIVE_MISSING,
    PB_TITLE_ASSETS_RESOURCE_MISSING,
    PB_TITLE_ASSETS_ARCHIVE_ERROR,
    PB_TITLE_ASSETS_TEXTURE_INVALID,
    PB_TITLE_ASSETS_OUT_OF_MEMORY,
} PBTitleAssetsResult;

typedef struct PBTitleAssets {
    PBTitleAssetsResult result;
    PBO2RResult archive_result;
    PBTextureDecodeResult decode_result;
    PBO2RStats archive_stats;
    PBDecodedTexture logo;
    PBDecodedTexture prompt;
    PBDecodedTexture copyright;
} PBTitleAssets;

typedef enum {
    PB_TITLE_FLOW_TITLE = 0,
    PB_TITLE_FLOW_FILE_SELECT,
} PBTitleFlowScreen;

typedef enum {
    PB_TITLE_FLOW_EVENT_NONE = 0,
    PB_TITLE_FLOW_EVENT_ENTER_FILE_SELECT,
    PB_TITLE_FLOW_EVENT_MOVE_SLOT,
    PB_TITLE_FLOW_EVENT_CONFIRM_SLOT,
    PB_TITLE_FLOW_EVENT_RETURN_TITLE,
} PBTitleFlowEvent;

typedef struct PBTitleFlow {
    PBTitleFlowScreen screen;
    PBTitleFlowEvent last_event;
    uint64_t frame_index;
    uint32_t transition_count;
    uint32_t confirmation_count;
    uint8_t selected_slot;
    uint8_t prompt_alpha;
    int8_t previous_stick_x_direction;
    int8_t previous_stick_y_direction;
    bool slot_confirmed;
} PBTitleFlow;

void pb_title_assets_init(PBTitleAssets *assets);
PBTitleAssetsResult pb_title_assets_load(PBTitleAssets *assets,
                                         PBArchive *game_archive,
                                         PBMemoryMonitor *memory);
void pb_title_assets_release_pixels(PBTitleAssets *assets,
                                    PBMemoryMonitor *memory);
const char *pb_title_assets_result_name(PBTitleAssetsResult result);

void pb_title_flow_init(PBTitleFlow *flow);
PBTitleFlowEvent pb_title_flow_update(PBTitleFlow *flow,
                                      const PBInputState *input);
const char *pb_title_flow_screen_name(PBTitleFlowScreen screen);
const char *pb_title_flow_event_name(PBTitleFlowEvent event);

#ifdef __cplusplus
}
#endif
