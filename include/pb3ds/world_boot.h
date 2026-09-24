#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pb3ds/input.h"
#include "pb3ds/o2r.h"
#include "pb3ds/texture.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PB_WORLD_BOOT_MAP_ID "mac_00"
#define PB_WORLD_BOOT_BACKGROUND_ID "nok_bg"
#define PB_WORLD_BOOT_ENTRY_ID 6U

typedef enum {
    PB_WORLD_BOOT_NOT_ATTEMPTED = 0,
    PB_WORLD_BOOT_READY,
    PB_WORLD_BOOT_ARCHIVE_MISSING,
    PB_WORLD_BOOT_RESOURCE_MISSING,
    PB_WORLD_BOOT_ARCHIVE_ERROR,
    PB_WORLD_BOOT_SHAPE_INVALID,
    PB_WORLD_BOOT_COLLISION_INVALID,
    PB_WORLD_BOOT_VERTEX_INVALID,
    PB_WORLD_BOOT_DISPLAY_LIST_INVALID,
    PB_WORLD_BOOT_BACKGROUND_INVALID,
    PB_WORLD_BOOT_OUT_OF_MEMORY,
} PBWorldBootResult;

typedef struct {
    uint32_t shape_nodes;
    uint32_t shape_display_lists;
    uint32_t vertex_count;
    uint32_t collision_colliders;
    uint32_t collision_vertices;
    uint32_t collision_triangles;
    uint32_t zone_colliders;
    uint32_t zone_vertices;
    uint32_t zone_triangles;
    uint32_t display_list_commands;
} PBWorldBootStats;

typedef struct {
    PBWorldBootResult result;
    PBO2RResult archive_result;
    PBO2RStats archive_stats;
    PBWorldBootStats world_stats;
    PBDecodedTexture background;
} PBWorldBoot;

typedef enum {
    PB_WORLD_FLOW_FILE_SELECT = 0,
    PB_WORLD_FLOW_LOADING,
    PB_WORLD_FLOW_ACTIVE,
    PB_WORLD_FLOW_PAUSED,
    PB_WORLD_FLOW_FAILED,
} PBWorldFlowState;

typedef enum {
    PB_WORLD_FLOW_EVENT_NONE = 0,
    PB_WORLD_FLOW_EVENT_LOAD_REQUESTED,
    PB_WORLD_FLOW_EVENT_ENTERED_WORLD,
    PB_WORLD_FLOW_EVENT_LOAD_FAILED,
    PB_WORLD_FLOW_EVENT_PAUSED,
    PB_WORLD_FLOW_EVENT_RESUMED,
} PBWorldFlowEvent;

typedef struct {
    PBWorldFlowState state;
    PBWorldFlowEvent last_event;
    uint64_t frame_index;
    uint32_t pause_count;
    uint8_t selected_slot;
} PBWorldFlow;

void pb_world_boot_init(PBWorldBoot *boot);
PBWorldBootResult pb_world_boot_load(PBWorldBoot *boot,
                                     PBArchive *game_archive,
                                     PBMemoryMonitor *memory);
void pb_world_boot_release(PBWorldBoot *boot, PBMemoryMonitor *memory);
const char *pb_world_boot_result_name(PBWorldBootResult result);

void pb_world_flow_init(PBWorldFlow *flow);
PBWorldFlowEvent pb_world_flow_request(PBWorldFlow *flow,
                                       uint8_t selected_slot);
PBWorldFlowEvent pb_world_flow_begin_transition(PBWorldFlow *flow);
PBWorldFlowEvent pb_world_flow_finish(PBWorldFlow *flow, bool loaded);
PBWorldFlowEvent pb_world_flow_update(PBWorldFlow *flow,
                                      const PBInputState *input);
const char *pb_world_flow_state_name(PBWorldFlowState state);
const char *pb_world_flow_event_name(PBWorldFlowEvent event);

#ifdef __cplusplus
}
#endif
