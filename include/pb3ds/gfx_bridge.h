#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pb3ds/renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PB_GFX_TEXTURE_UNITS 2U
#define PB_GFX_MAX_TEXTURES 192U
#define PB_GFX_MAX_SHADERS 64U
#define PB_GFX_MAX_STREAM_BYTES (576U * 1024U)
#define PB_GFX_MAX_STREAM_TRIANGLES 4096U
#define PB_GFX_TEV_STAGE_COUNT 6U

/* Values are fixed by libultraship's pinned Fast3D interpreter contract. */
typedef enum {
    PB_GFX_SHADER_ZERO = 0,
    PB_GFX_SHADER_INPUT_1 = 1,
    PB_GFX_SHADER_INPUT_2 = 2,
    PB_GFX_SHADER_INPUT_3 = 3,
    PB_GFX_SHADER_INPUT_4 = 4,
    PB_GFX_SHADER_INPUT_5 = 5,
    PB_GFX_SHADER_INPUT_6 = 6,
    PB_GFX_SHADER_SHADE = 7,
    PB_GFX_SHADER_TEXEL0 = 8,
    PB_GFX_SHADER_TEXEL0_ALPHA = 9,
    PB_GFX_SHADER_TEXEL1 = 10,
    PB_GFX_SHADER_TEXEL1_ALPHA = 11,
    PB_GFX_SHADER_ONE = 12,
    PB_GFX_SHADER_COMBINED = 13,
    PB_GFX_SHADER_NOISE = 14,
    PB_GFX_SHADER_LOD_FRACTION = 15,
} PBGfxShaderOperand;

typedef enum {
    PB_GFX_OPT_ALPHA = 0,
    PB_GFX_OPT_FOG = 1,
    PB_GFX_OPT_TEXTURE_EDGE = 2,
    PB_GFX_OPT_NOISE = 3,
    PB_GFX_OPT_TWO_CYCLE = 4,
    PB_GFX_OPT_ALPHA_THRESHOLD = 5,
    PB_GFX_OPT_INVISIBLE = 6,
    PB_GFX_OPT_GRAYSCALE = 7,
    PB_GFX_OPT_TEXEL0_CLAMP_S = 8,
    PB_GFX_OPT_TEXEL0_CLAMP_T = 9,
    PB_GFX_OPT_TEXEL1_CLAMP_S = 10,
    PB_GFX_OPT_TEXEL1_CLAMP_T = 11,
    PB_GFX_OPT_TEXEL0_MASK = 12,
    PB_GFX_OPT_TEXEL1_MASK = 13,
    PB_GFX_OPT_TEXEL0_BLEND = 14,
    PB_GFX_OPT_TEXEL1_BLEND = 15,
    PB_GFX_OPT_PRIM_DEPTH = 16,
    PB_GFX_OPT_TEX_LOD = 17,
    PB_GFX_OPT_MIP_LOD = 18,
    PB_GFX_OPT_LIGHTING = 19,
    PB_GFX_OPT_POINT_LIGHTING = 20,
    PB_GFX_OPT_TEXGEN = 21,
    PB_GFX_OPT_TEXGEN_LINEAR = 22,
    PB_GFX_OPT_TEXEL0_PALETTE = 23,
    PB_GFX_OPT_TEXEL1_PALETTE = 24,
    PB_GFX_OPT_PRISM_SHADER = 25,
} PBGfxShaderOption;

typedef enum {
    PB_GFX_COMBINER_SHADE,
    PB_GFX_COMBINER_TEXTURE0,
    PB_GFX_COMBINER_TEXTURE0_SHADE,
    PB_GFX_COMBINER_FALLBACK,
} PBGfxCombinerMode;

typedef enum {
    PB_GFX_TEV_SHADE,
    PB_GFX_TEV_TEXTURE0,
    PB_GFX_TEV_TEXTURE1,
    PB_GFX_TEV_PREVIOUS,
    PB_GFX_TEV_CONSTANT,
} PBGfxTevSource;

typedef enum {
    PB_GFX_TEV_RGB_COLOR,
    PB_GFX_TEV_RGB_ALPHA,
} PBGfxTevRgbOperand;

typedef enum {
    PB_GFX_TEV_REPLACE,
    PB_GFX_TEV_MODULATE,
    PB_GFX_TEV_ADD,
    PB_GFX_TEV_SUBTRACT,
    PB_GFX_TEV_INTERPOLATE,
    PB_GFX_TEV_MULTIPLY_ADD,
} PBGfxTevFunction;

typedef struct {
    PBGfxTevSource rgb_sources[3];
    PBGfxTevSource alpha_sources[3];
    PBGfxTevRgbOperand rgb_operands[3];
    PBGfxTevFunction rgb_function;
    PBGfxTevFunction alpha_function;
    float constant[4];
} PBGfxTevStage;

typedef struct PBGfxTevProgram {
    PBGfxTevStage stages[PB_GFX_TEV_STAGE_COUNT];
    uint8_t stage_count;
} PBGfxTevProgram;

typedef enum {
    PB_GFX_REJECT_NONE = 0,
    PB_GFX_REJECT_OPTION = 1U << 0,
    PB_GFX_REJECT_TWO_CYCLE = 1U << 1,
    PB_GFX_REJECT_TEXTURE1 = 1U << 2,
    PB_GFX_REJECT_FORMULA = 1U << 3,
    PB_GFX_REJECT_VERTEX_LAYOUT = 1U << 4,
    PB_GFX_REJECT_CAPACITY = 1U << 5,
    PB_GFX_REJECT_STATE = 1U << 6,
} PBGfxRejectReason;

typedef struct {
    uint64_t shader_id0;
    uint64_t shader_id1;
    uint8_t operands[2][2][4];
    uint8_t num_inputs;
    bool used_textures[PB_GFX_TEXTURE_UNITS];
    bool uses_shade;
    bool uses_alpha;
    bool two_cycle;
    bool supported;
    PBGfxCombinerMode mode;
    uint32_t reject_reasons;
    size_t vertex_stride_floats;
} PBGfxCombinerPlan;

typedef struct {
    uint32_t id;
    uint16_t width;
    uint16_t height;
    size_t bytes;
    uint64_t last_use;
    bool allocated;
    bool uploaded;
} PBGfxTextureRecord;

typedef struct {
    uint64_t frames_started;
    uint64_t frames_presented;
    uint64_t draw_calls;
    uint64_t triangles;
    uint64_t vertices;
    uint64_t streamed_bytes;
    size_t stream_peak_bytes;
    size_t texture_bytes;
    uint32_t textures_live;
    uint32_t shaders_live;
    uint32_t rejected_commands;
    uint32_t unsupported_shaders;
    uint32_t frame_failures;
} PBGfxBridgeStats;

typedef struct {
    PBGfxTextureRecord textures[PB_GFX_MAX_TEXTURES];
    uint32_t selected_textures[PB_GFX_TEXTURE_UNITS];
    uint32_t next_texture_id;
    uint64_t use_clock;
    PBGfxBridgeStats stats;
    PBViewport viewport;
    PBViewport scissor;
    bool viewport_valid;
    bool scissor_valid;
    bool frame_open;
    bool active;
} PBGfxBridge;

uint64_t pb_gfx_shader_option(PBGfxShaderOption option);
bool pb_gfx_combiner_decode(PBGfxCombinerPlan *plan, uint64_t shader_id0,
                            uint64_t shader_id1);
bool pb_gfx_combiner_compile_tev(const PBGfxCombinerPlan *plan,
                                 const float inputs[6][4],
                                 PBGfxTevProgram *program);
bool pb_gfx_validate_draw(const PBGfxCombinerPlan *plan,
                          const float *vertices, size_t float_count,
                          size_t triangle_count, size_t *stream_bytes);

void pb_gfx_bridge_init(PBGfxBridge *bridge);
void pb_gfx_bridge_set_active(PBGfxBridge *bridge, bool active);
bool pb_gfx_bridge_start_frame(PBGfxBridge *bridge);
bool pb_gfx_bridge_end_frame(PBGfxBridge *bridge, bool presented);
bool pb_gfx_bridge_set_viewport(PBGfxBridge *bridge, int x, int y,
                                int width, int height);
bool pb_gfx_bridge_set_scissor(PBGfxBridge *bridge, int x, int y,
                               int width, int height);
uint32_t pb_gfx_bridge_new_texture(PBGfxBridge *bridge);
bool pb_gfx_bridge_select_texture(PBGfxBridge *bridge, int tile,
                                  uint32_t texture_id);
bool pb_gfx_bridge_upload_texture(PBGfxBridge *bridge, uint32_t texture_id,
                                  uint32_t width, uint32_t height);
bool pb_gfx_bridge_delete_texture(PBGfxBridge *bridge, uint32_t texture_id);
bool pb_gfx_bridge_record_shader(PBGfxBridge *bridge,
                                 const PBGfxCombinerPlan *plan);
bool pb_gfx_bridge_record_draw(PBGfxBridge *bridge,
                               const PBGfxCombinerPlan *plan,
                               const float *vertices, size_t float_count,
                               size_t triangle_count);
void pb_gfx_bridge_clear_shaders(PBGfxBridge *bridge);
const PBGfxTextureRecord *pb_gfx_bridge_find_texture(
    const PBGfxBridge *bridge, uint32_t texture_id);
const PBGfxBridgeStats *pb_gfx_bridge_stats(const PBGfxBridge *bridge);

#ifdef __cplusplus
}
#endif
