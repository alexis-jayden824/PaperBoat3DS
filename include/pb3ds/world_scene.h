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

#define PB_WORLD_MAP_ID_CAPACITY 8U
#define PB_WORLD_TEXTURE_NAME_CAPACITY 48U
#define PB_WORLD_COLLIDER_NAME_CAPACITY 32U
#define PB_WORLD_MAX_DISPLAY_LISTS 256U
#define PB_WORLD_MAX_SOURCE_VERTICES 8192U
#define PB_WORLD_MAX_TEXTURES 56U
#define PB_WORLD_MAX_TRIANGLES 4096U
#define PB_WORLD_MAX_COLLIDERS 256U
#define PB_WORLD_MAX_COLLISION_VERTICES 2048U
#define PB_WORLD_MAX_COLLISION_TRIANGLES 4096U
#define PB_WORLD_PLAYER_FRAME_COUNT 2U

typedef struct {
    float x;
    float y;
    float z;
} PBWorldVec3;

typedef struct {
    PBWorldVec3 position;
    float u;
    float v;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} PBWorldVertex;

typedef enum {
    PB_WORLD_RENDER_OPAQUE = 0,
    PB_WORLD_RENDER_CUTOUT,
    PB_WORLD_RENDER_TRANSLUCENT,
} PBWorldRenderClass;

typedef struct {
    PBWorldVertex vertices[3];
    int16_t texture_index;
    uint8_t render_class;
    uint8_t reserved;
} PBWorldTriangle;

typedef struct {
    char name[PB_WORLD_TEXTURE_NAME_CAPACITY];
    PBDecodedTexture decoded;
    uint8_t wrap_s;
    uint8_t wrap_t;
} PBWorldTexture;

typedef struct {
    uint16_t vertex[3];
    uint16_t collider;
    PBWorldVec3 normal;
    bool one_sided;
} PBWorldCollisionTriangle;

typedef struct {
    char name[PB_WORLD_COLLIDER_NAME_CAPACITY];
    uint32_t first_triangle;
    uint16_t triangle_count;
    int16_t next_sibling;
    int16_t first_child;
    PBWorldVec3 minimum;
    PBWorldVec3 maximum;
    uint32_t flags;
    bool has_bounds;
} PBWorldCollider;

typedef struct {
    uint32_t shape_nodes;
    uint32_t leaf_models;
    uint32_t display_lists;
    uint32_t display_list_commands;
    uint32_t source_vertices;
    uint32_t triangles;
    uint32_t textured_triangles;
    uint32_t lit_triangles;
    uint32_t textures;
    uint32_t unsupported_commands;
    uint32_t collision_vertices;
    uint32_t collision_triangles;
    uint32_t colliders;
} PBWorldSceneStats;

typedef enum {
    PB_WORLD_SCENE_NOT_ATTEMPTED = 0,
    PB_WORLD_SCENE_READY,
    PB_WORLD_SCENE_INVALID_ARGUMENT,
    PB_WORLD_SCENE_UNSUPPORTED_MAP,
    PB_WORLD_SCENE_RESOURCE_MISSING,
    PB_WORLD_SCENE_ARCHIVE_ERROR,
    PB_WORLD_SCENE_SHAPE_INVALID,
    PB_WORLD_SCENE_DISPLAY_LIST_INVALID,
    PB_WORLD_SCENE_TEXTURE_INVALID,
    PB_WORLD_SCENE_COLLISION_INVALID,
    PB_WORLD_SCENE_OUT_OF_MEMORY,
    PB_WORLD_SCENE_CAPACITY,
} PBWorldSceneResult;

typedef enum {
    PB_WORLD_SCENE_EVENT_NONE = 0,
    PB_WORLD_SCENE_EVENT_SIGN_OPENED,
    PB_WORLD_SCENE_EVENT_SIGN_CLOSED,
    PB_WORLD_SCENE_EVENT_STAR_PIECE_COLLECTED,
    PB_WORLD_SCENE_EVENT_TRANSITION_STARTED,
    PB_WORLD_SCENE_EVENT_TRANSITION_REQUESTED,
} PBWorldSceneEvent;

typedef enum {
    PB_WORLD_TRANSITION_NONE = 0,
    PB_WORLD_TRANSITION_FADE_IN,
    PB_WORLD_TRANSITION_FADE_OUT,
    PB_WORLD_TRANSITION_WAITING,
} PBWorldTransitionState;

typedef struct PBWorldScene {
    PBWorldSceneResult result;
    PBO2RResult archive_result;
    PBO2RStats archive_stats;
    PBWorldSceneStats stats;
    char map_id[PB_WORLD_MAP_ID_CAPACITY];
    uint8_t entry_id;

    PBWorldTriangle *triangles;
    size_t triangles_allocation;
    PBWorldTexture textures[PB_WORLD_MAX_TEXTURES];
    uint16_t texture_count;
    PBDecodedTexture background;
    PBDecodedTexture player_frames[PB_WORLD_PLAYER_FRAME_COUNT];
    PBDecodedTexture star_piece;

    PBWorldVec3 *collision_vertices;
    size_t collision_vertices_allocation;
    PBWorldCollisionTriangle *collision_triangles;
    size_t collision_triangles_allocation;
    PBWorldCollider *colliders;
    size_t colliders_allocation;
    uint16_t collider_count;

    PBWorldVec3 player_position;
    PBWorldVec3 camera_target;
    float player_yaw;
    float player_speed;
    int16_t current_floor;
    uint32_t frames;
    uint32_t movement_frames;
    uint32_t collision_blocks;
    uint32_t floor_samples;
    uint32_t script_events;
    uint32_t transition_count;
    uint16_t transition_cooldown;
    uint16_t entry_walk_frames;
    uint16_t message_timer;
    uint8_t player_frame;
    bool player_facing_left;
    bool star_piece_active;
    bool star_piece_collected;
    bool pixels_released;

    int16_t sign_collider;
    int16_t exit_collider;
    char exit_map[PB_WORLD_MAP_ID_CAPACITY];
    uint8_t exit_entry;
    PBWorldTransitionState transition_state;
    uint16_t transition_frame;
    char requested_map[PB_WORLD_MAP_ID_CAPACITY];
    uint8_t requested_entry;
} PBWorldScene;

void pb_world_scene_init(PBWorldScene *scene);
PBWorldSceneResult pb_world_scene_load(PBWorldScene *scene,
                                       PBArchive *archive,
                                       const char *map_id,
                                       uint8_t entry_id,
                                       PBMemoryMonitor *memory);
PBWorldSceneEvent pb_world_scene_update(PBWorldScene *scene,
                                        const PBInputState *input);
float pb_world_scene_fade_alpha(const PBWorldScene *scene);
bool pb_world_scene_message_visible(const PBWorldScene *scene);
void pb_world_scene_release_pixels(PBWorldScene *scene,
                                   PBMemoryMonitor *memory);
void pb_world_scene_release(PBWorldScene *scene, PBMemoryMonitor *memory);
const char *pb_world_scene_result_name(PBWorldSceneResult result);
const char *pb_world_scene_event_name(PBWorldSceneEvent event);

#ifdef __cplusplus
}
#endif
