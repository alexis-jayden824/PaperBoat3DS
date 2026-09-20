#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PB_RENDER_TOP_WIDTH 400U
#define PB_RENDER_TOP_HEIGHT 240U
#define PB_RENDER_TARGET_WIDTH 240U
#define PB_RENDER_TARGET_HEIGHT 400U
#define PB_RENDER_TEXTURE_MIN_DIMENSION 8U
#define PB_RENDER_TEXTURE_MAX_DIMENSION 1024U

typedef enum {
    PB_TEXTURE_RGBA8 = 0x0,
    PB_TEXTURE_RGB8 = 0x1,
    PB_TEXTURE_RGBA5551 = 0x2,
    PB_TEXTURE_RGB565 = 0x3,
    PB_TEXTURE_RGBA4 = 0x4,
    PB_TEXTURE_LA8 = 0x5,
    PB_TEXTURE_HILO8 = 0x6,
    PB_TEXTURE_L8 = 0x7,
    PB_TEXTURE_A8 = 0x8,
    PB_TEXTURE_LA4 = 0x9,
    PB_TEXTURE_L4 = 0xA,
    PB_TEXTURE_A4 = 0xB,
    PB_TEXTURE_ETC1 = 0xC,
    PB_TEXTURE_ETC1A4 = 0xD,
    PB_TEXTURE_FORMAT_COUNT,
} PBTextureFormat;

typedef enum {
    PB_FILTER_NEAREST = 0,
    PB_FILTER_LINEAR = 1,
    PB_FILTER_COUNT,
} PBTextureFilter;

typedef enum {
    PB_WRAP_CLAMP_TO_EDGE = 0,
    PB_WRAP_CLAMP_TO_BORDER = 1,
    PB_WRAP_REPEAT = 2,
    PB_WRAP_MIRRORED_REPEAT = 3,
    PB_WRAP_COUNT,
} PBTextureWrap;

typedef enum {
    PB_CULL_NONE = 0,
    PB_CULL_FRONT_CCW = 1,
    PB_CULL_BACK_CCW = 2,
    PB_CULL_COUNT,
} PBCullMode;

typedef enum {
    PB_COMPARE_NEVER = 0,
    PB_COMPARE_ALWAYS = 1,
    PB_COMPARE_EQUAL = 2,
    PB_COMPARE_NOT_EQUAL = 3,
    PB_COMPARE_LESS = 4,
    PB_COMPARE_LESS_EQUAL = 5,
    PB_COMPARE_GREATER = 6,
    PB_COMPARE_GREATER_EQUAL = 7,
    PB_COMPARE_COUNT,
} PBCompareFunction;

typedef enum {
    PB_BLEND_DISABLED,
    PB_BLEND_ALPHA,
    PB_BLEND_ADDITIVE,
    PB_BLEND_COUNT,
} PBBlendMode;

typedef struct {
    uint16_t width;
    uint16_t height;
    PBTextureFormat format;
    uint8_t bits_per_pixel;
    size_t bytes;
} PBTextureLayout;

/* Logical top-screen coordinates use a bottom-left 400x240 origin. */
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} PBViewport;

/* Native PICA framebuffer coordinates are rotated to 240x400. */
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} PBTargetViewport;

typedef struct {
    PBCullMode cull_mode;
    bool depth_test_enabled;
    bool depth_write_enabled;
    PBCompareFunction depth_function;
    PBBlendMode blend_mode;
    PBTextureFilter min_filter;
    PBTextureFilter mag_filter;
    PBTextureWrap wrap_s;
    PBTextureWrap wrap_t;
} PBRenderPipeline;

typedef enum {
    PB_BIND_REJECTED = -1,
    PB_BIND_UNCHANGED = 0,
    PB_BIND_CHANGED = 1,
} PBBindResult;

typedef struct {
    PBViewport viewport;
    PBRenderPipeline pipeline;
    uint32_t changes;
    uint32_t deduplicated;
    uint32_t rejected;
    bool viewport_bound;
    bool pipeline_bound;
} PBRenderStateCache;

typedef struct {
    uint64_t frames;
    uint64_t draw_calls;
    uint64_t vertices;
    uint32_t frame_failures;
    uint32_t state_changes;
    uint32_t state_deduplicated;
    uint32_t rejected_commands;
    size_t vertex_buffer_bytes;
    size_t texture_bytes;
    float command_buffer_peak;
} PBRendererStats;

typedef enum {
    PB_RENDERER_INIT_OK = 0,
    PB_RENDERER_INIT_INVALID_ARGUMENT,
    PB_RENDERER_INIT_OUT_OF_MEMORY,
    PB_RENDERER_INIT_CITRO3D,
    PB_RENDERER_INIT_TARGET,
    PB_RENDERER_INIT_SHADER,
    PB_RENDERER_INIT_UNIFORM,
    PB_RENDERER_INIT_VERTEX_BUFFER,
    PB_RENDERER_INIT_TEXTURE,
} PBRendererInitResult;

typedef struct PBRenderer3DS PBRenderer3DS;

bool pb_renderer_texture_layout(PBTextureLayout *layout, uint16_t width,
                                uint16_t height, PBTextureFormat format);
const char *pb_renderer_texture_format_name(PBTextureFormat format);
size_t pb_renderer_swizzled_texel_index(uint16_t x, uint16_t y,
                                        uint16_t width, uint16_t height);
bool pb_renderer_swizzle_rgba8(uint8_t *destination, size_t destination_size,
                               const uint8_t *source, size_t source_size,
                               uint16_t width, uint16_t height);
bool pb_renderer_vertex_buffer_size(size_t stride, size_t vertex_count,
                                    size_t *bytes);

bool pb_renderer_viewport_to_target(const PBViewport *logical,
                                    PBTargetViewport *target);
bool pb_renderer_pipeline_is_valid(const PBRenderPipeline *pipeline);
void pb_renderer_state_cache_init(PBRenderStateCache *cache);
PBBindResult pb_renderer_bind_viewport(PBRenderStateCache *cache,
                                       const PBViewport *viewport);
PBBindResult pb_renderer_bind_pipeline(PBRenderStateCache *cache,
                                       const PBRenderPipeline *pipeline);

const char *pb_renderer_init_result_name(PBRendererInitResult result);
PBRendererInitResult pb_renderer_3ds_create(PBRenderer3DS **renderer);
bool pb_renderer_3ds_render(PBRenderer3DS *renderer);
const PBRendererStats *pb_renderer_3ds_stats(const PBRenderer3DS *renderer);
void pb_renderer_3ds_destroy(PBRenderer3DS *renderer);
