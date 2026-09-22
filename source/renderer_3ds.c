#include "pb3ds/renderer.h"

#include <3ds.h>
#include <citro3d.h>
#include <stdlib.h>
#include <string.h>

#include "pb3ds/gfx_bridge.h"
#include "renderer_shbin.h"

/* Match PaperBoat's fixed-art presentation and the physical black LCD bezel. */
#define PB_RENDER_CLEAR_COLOR 0x000000FFU
#define PB_RENDER_CLEAR_DEPTH 0U

#define PB_DISPLAY_TRANSFER_FLAGS                                           \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |                  \
     GX_TRANSFER_RAW_COPY(0) |                                             \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |                        \
     GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |                        \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct {
    float position[4];
    float texcoord0[2];
    float texcoord1[2];
    float color[4];
} PBRendererVertex;

typedef struct {
    uint32_t id;
    C3D_Tex texture;
    PBTextureFilter filter;
    PBTextureWrap wrap_s;
    PBTextureWrap wrap_t;
    bool allocated;
    bool sampler_set;
} PBRendererTexture;

typedef struct PBRendererRetiredTexture {
    C3D_Tex texture;
    struct PBRendererRetiredTexture *next;
} PBRendererRetiredTexture;

struct PBRenderer3DS {
    C3D_RenderTarget *target;
    DVLB_s *shader_dvlb;
    shaderProgram_s program;
    C3D_Mtx projection;
    PBRendererTexture textures[PB_GFX_MAX_TEXTURES];
    PBRendererRetiredTexture *retired_textures;
    uint32_t retired_texture_count;
    PBRenderStateCache state_cache;
    PBViewport scissor;
    PBRendererStats stats;
    PBRendererVertex *stream_buffer;
    size_t stream_capacity_vertices;
    size_t stream_used_vertices;
    int projection_uniform;
    uint32_t bound_textures[PB_GFX_TEXTURE_UNITS];
    int combiner_mode;
    PBGfxTevProgram combiner_program;
    bool combiner_program_valid;
    C3D_FogLut fog_lut;
    int16_t fog_multiply;
    int16_t fog_offset;
    uint8_t fog_red;
    uint8_t fog_green;
    uint8_t fog_blue;
    bool fog_enabled;
    bool fog_state_valid;
    bool preserve_color_next_frame;
    bool c3d_ready;
    bool program_ready;
    bool frame_open;
};

_Static_assert((int)PB_TEXTURE_RGBA8 == (int)GPU_RGBA8,
               "texture format contract must match PICA200");
_Static_assert((int)PB_TEXTURE_ETC1A4 == (int)GPU_ETC1A4,
               "texture format contract must match PICA200");
_Static_assert((int)PB_FILTER_NEAREST == (int)GPU_NEAREST,
               "filter contract must match PICA200");
_Static_assert((int)PB_WRAP_REPEAT == (int)GPU_REPEAT,
               "wrap contract must match PICA200");
_Static_assert((int)PB_COMPARE_GREATER == (int)GPU_GREATER,
               "depth contract must match PICA200");

static PBRendererTexture *find_texture(PBRenderer3DS *renderer,
                                       uint32_t texture_id) {
    if (renderer == NULL || texture_id == 0) {
        return NULL;
    }
    for (size_t index = 0; index < PB_GFX_MAX_TEXTURES; index++) {
        PBRendererTexture *entry = &renderer->textures[index];
        if (entry->allocated && entry->id == texture_id) {
            return entry;
        }
    }
    return NULL;
}

static PBRendererTexture *find_free_texture(PBRenderer3DS *renderer) {
    if (renderer == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < PB_GFX_MAX_TEXTURES; index++) {
        if (!renderer->textures[index].allocated) {
            return &renderer->textures[index];
        }
    }
    return NULL;
}

/*
 * Citro3D records texture addresses in commands that execute after the CPU
 * has finished walking PaperBoat's display list.  A runtime-cache eviction
 * must therefore stop exposing the logical texture immediately without
 * releasing its C3D_Tex allocation until the submitted frame is complete.
 */
static bool retire_texture(PBRenderer3DS *renderer, C3D_Tex *texture) {
    if (renderer == NULL || texture == NULL) {
        return false;
    }
    PBRendererRetiredTexture *retired = malloc(sizeof(*retired));
    if (retired == NULL) {
        renderer->stats.texture_retire_failures++;
        return false;
    }
    retired->texture = *texture;
    retired->next = renderer->retired_textures;
    renderer->retired_textures = retired;
    memset(texture, 0, sizeof(*texture));
    renderer->retired_texture_count++;
    renderer->stats.texture_retirements++;
    if (renderer->retired_texture_count >
        renderer->stats.retired_texture_peak) {
        renderer->stats.retired_texture_peak =
            renderer->retired_texture_count;
    }
    return true;
}

static void release_retired_textures(PBRenderer3DS *renderer) {
    if (renderer == NULL) {
        return;
    }
    PBRendererRetiredTexture *retired = renderer->retired_textures;
    while (retired != NULL) {
        PBRendererRetiredTexture *next = retired->next;
        C3D_TexDelete(&retired->texture);
        free(retired);
        retired = next;
    }
    renderer->retired_textures = NULL;
    renderer->retired_texture_count = 0U;
}

static void sync_and_release_retired_textures(PBRenderer3DS *renderer) {
    if (renderer == NULL || renderer->retired_textures == NULL) {
        return;
    }
    C3D_FrameSync();
    release_retired_textures(renderer);
}

static void apply_pipeline(PBRenderer3DS *renderer) {
    const PBRenderPipeline *pipeline = &renderer->state_cache.pipeline;
    const bool depth_enabled = pipeline->depth_test_enabled ||
                               pipeline->depth_write_enabled;
    const GPU_TESTFUNC depth_function = pipeline->depth_test_enabled
                                            ? (GPU_TESTFUNC)pipeline->depth_function
                                            : GPU_ALWAYS;

    C3D_CullFace((GPU_CULLMODE)pipeline->cull_mode);
    C3D_DepthTest(depth_enabled, depth_function,
                  pipeline->depth_write_enabled ? GPU_WRITE_ALL
                                                : GPU_WRITE_COLOR);
    C3D_AlphaTest(pipeline->alpha_test_enabled,
                  (GPU_TESTFUNC)pipeline->alpha_function,
                  pipeline->alpha_reference);

    switch (pipeline->blend_mode) {
        case PB_BLEND_ALPHA:
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                           GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA,
                           GPU_ONE_MINUS_SRC_ALPHA);
            break;
        case PB_BLEND_ADDITIVE:
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                           GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
            break;
        case PB_BLEND_DISABLED:
        default:
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO,
                           GPU_ONE, GPU_ZERO);
            break;
    }
}

static void apply_combiner(int combiner_mode) {
    for (int stage = 0; stage < (int)PB_GFX_TEV_STAGE_COUNT; stage++) {
        C3D_TexEnvInit(C3D_GetTexEnv(stage));
    }
    C3D_TexEnv *environment = C3D_GetTexEnv(0);

    switch ((PBGfxCombinerMode)combiner_mode) {
        case PB_GFX_COMBINER_SHADE:
            C3D_TexEnvSrc(environment, C3D_Both, GPU_PRIMARY_COLOR,
                          GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(environment, C3D_Both, GPU_REPLACE);
            break;
        case PB_GFX_COMBINER_TEXTURE0:
            C3D_TexEnvSrc(environment, C3D_Both, GPU_TEXTURE0,
                          GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(environment, C3D_Both, GPU_REPLACE);
            break;
        case PB_GFX_COMBINER_TEXTURE0_SHADE:
            C3D_TexEnvSrc(environment, C3D_Both, GPU_TEXTURE0,
                          GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(environment, C3D_Both, GPU_MODULATE);
            break;
        case PB_GFX_COMBINER_FALLBACK:
        default:
            C3D_TexEnvColor(environment, 0xFF00FFFFU);
            C3D_TexEnvSrc(environment, C3D_Both, GPU_CONSTANT,
                          GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(environment, C3D_Both, GPU_REPLACE);
            break;
    }
}

static GPU_TEVSRC tev_source(PBGfxTevSource source) {
    switch (source) {
        case PB_GFX_TEV_TEXTURE0: return GPU_TEXTURE0;
        case PB_GFX_TEV_TEXTURE1: return GPU_TEXTURE1;
        case PB_GFX_TEV_PREVIOUS: return GPU_PREVIOUS;
        case PB_GFX_TEV_CONSTANT: return GPU_CONSTANT;
        case PB_GFX_TEV_SHADE:
        default: return GPU_PRIMARY_COLOR;
    }
}

static GPU_TEVOP_RGB tev_rgb_operand(PBGfxTevRgbOperand operand) {
    return operand == PB_GFX_TEV_RGB_ALPHA
               ? GPU_TEVOP_RGB_SRC_ALPHA
               : GPU_TEVOP_RGB_SRC_COLOR;
}

static GPU_COMBINEFUNC tev_function(PBGfxTevFunction function) {
    switch (function) {
        case PB_GFX_TEV_MODULATE: return GPU_MODULATE;
        case PB_GFX_TEV_ADD: return GPU_ADD;
        case PB_GFX_TEV_SUBTRACT: return GPU_SUBTRACT;
        case PB_GFX_TEV_INTERPOLATE: return GPU_INTERPOLATE;
        case PB_GFX_TEV_MULTIPLY_ADD: return GPU_MULTIPLY_ADD;
        case PB_GFX_TEV_REPLACE:
        default: return GPU_REPLACE;
    }
}

static uint8_t tev_color_component(float value) {
    if (value <= 0.0f) return 0U;
    if (value >= 1.0f) return 255U;
    return (uint8_t)(value * 255.0f + 0.5f);
}

static bool apply_combiner_program(const PBGfxTevProgram *program) {
    if (program == NULL || program->stage_count == 0U ||
        program->stage_count > PB_GFX_TEV_STAGE_COUNT) {
        return false;
    }
    for (size_t stage_index = 0; stage_index < PB_GFX_TEV_STAGE_COUNT;
         stage_index++) {
        C3D_TexEnv *environment = C3D_GetTexEnv((int)stage_index);
        C3D_TexEnvInit(environment);
        if (stage_index >= program->stage_count) {
            continue;
        }
        const PBGfxTevStage *stage = &program->stages[stage_index];
        C3D_TexEnvColor(
            environment,
            IVec_Pack(tev_color_component(stage->constant[0]),
                      tev_color_component(stage->constant[1]),
                      tev_color_component(stage->constant[2]),
                      tev_color_component(stage->constant[3])));
        C3D_TexEnvSrc(environment, C3D_RGB,
                      tev_source(stage->rgb_sources[0]),
                      tev_source(stage->rgb_sources[1]),
                      tev_source(stage->rgb_sources[2]));
        C3D_TexEnvSrc(environment, C3D_Alpha,
                      tev_source(stage->alpha_sources[0]),
                      tev_source(stage->alpha_sources[1]),
                      tev_source(stage->alpha_sources[2]));
        C3D_TexEnvOpRgb(environment,
                        tev_rgb_operand(stage->rgb_operands[0]),
                        tev_rgb_operand(stage->rgb_operands[1]),
                        tev_rgb_operand(stage->rgb_operands[2]));
        C3D_TexEnvOpAlpha(environment, GPU_TEVOP_A_SRC_ALPHA,
                          GPU_TEVOP_A_SRC_ALPHA,
                          GPU_TEVOP_A_SRC_ALPHA);
        C3D_TexEnvFunc(environment, C3D_RGB,
                       tev_function(stage->rgb_function));
        C3D_TexEnvFunc(environment, C3D_Alpha,
                       tev_function(stage->alpha_function));
    }
    return true;
}

static void sync_stats(PBRenderer3DS *renderer) {
    renderer->stats.state_changes = renderer->state_cache.changes;
    renderer->stats.state_deduplicated = renderer->state_cache.deduplicated;
    renderer->stats.rejected_commands = renderer->state_cache.rejected;
}

PBRendererInitResult pb_renderer_3ds_create(PBRenderer3DS **renderer_out) {
    if (renderer_out == NULL) {
        return PB_RENDERER_INIT_INVALID_ARGUMENT;
    }
    *renderer_out = NULL;

    PBRenderer3DS *renderer = calloc(1, sizeof(*renderer));
    if (renderer == NULL) {
        return PB_RENDERER_INIT_OUT_OF_MEMORY;
    }

    PBRendererInitResult result = PB_RENDERER_INIT_CITRO3D;
    renderer->combiner_mode = -1;
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        goto fail;
    }
    renderer->c3d_ready = true;

    result = PB_RENDERER_INIT_TARGET;
    renderer->target = C3D_RenderTargetCreate(
        PB_RENDER_TARGET_WIDTH, PB_RENDER_TARGET_HEIGHT, GPU_RB_RGBA8,
        GPU_RB_DEPTH24_STENCIL8);
    if (renderer->target == NULL) {
        goto fail;
    }
    C3D_RenderTargetSetOutput(renderer->target, GFX_TOP, GFX_LEFT,
                              PB_DISPLAY_TRANSFER_FLAGS);

    result = PB_RENDERER_INIT_SHADER;
    renderer->shader_dvlb =
        DVLB_ParseFile((uint32_t *)renderer_shbin, renderer_shbin_size);
    if (renderer->shader_dvlb == NULL) {
        goto fail;
    }
    shaderProgramInit(&renderer->program);
    renderer->program_ready = true;
    shaderProgramSetVsh(&renderer->program, &renderer->shader_dvlb->DVLE[0]);
    C3D_BindProgram(&renderer->program);

    result = PB_RENDERER_INIT_UNIFORM;
    renderer->projection_uniform = shaderInstanceGetUniformLocation(
        renderer->program.vertexShader, "projection");
    if (renderer->projection_uniform < 0) {
        goto fail;
    }

    C3D_AttrInfo *attributes = C3D_GetAttrInfo();
    AttrInfo_Init(attributes);
    AttrInfo_AddLoader(attributes, 0, GPU_FLOAT, 4);
    AttrInfo_AddLoader(attributes, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attributes, 2, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attributes, 3, GPU_FLOAT, 4);

    Mtx_OrthoTilt(&renderer->projection, 0.0f, (float)PB_RENDER_TOP_WIDTH,
                  0.0f, (float)PB_RENDER_TOP_HEIGHT, 0.0f, 1.0f, true);

    result = PB_RENDERER_INIT_VERTEX_BUFFER;
    renderer->stream_capacity_vertices = PB_GFX_MAX_STREAM_TRIANGLES * 3U;
    renderer->stats.stream_capacity_vertices =
        renderer->stream_capacity_vertices;
    if (!pb_renderer_vertex_buffer_size(
            sizeof(PBRendererVertex), renderer->stream_capacity_vertices,
            &renderer->stats.vertex_buffer_bytes)) {
        goto fail;
    }
    renderer->stream_buffer = linearAlloc(renderer->stats.vertex_buffer_bytes);
    if (renderer->stream_buffer == NULL) {
        goto fail;
    }

    pb_renderer_state_cache_init(&renderer->state_cache);
    const PBViewport viewport = {
        .x = 0,
        .y = 0,
        .width = PB_RENDER_TOP_WIDTH,
        .height = PB_RENDER_TOP_HEIGHT,
    };
    const PBRenderPipeline pipeline = {
        .cull_mode = PB_CULL_NONE,
        .depth_test_enabled = false,
        .depth_write_enabled = false,
        .depth_function = PB_COMPARE_GREATER_EQUAL,
        .blend_mode = PB_BLEND_DISABLED,
        .alpha_test_enabled = false,
        .alpha_function = PB_COMPARE_GREATER,
        .alpha_reference = 0,
        .min_filter = PB_FILTER_NEAREST,
        .mag_filter = PB_FILTER_NEAREST,
        .wrap_s = PB_WRAP_REPEAT,
        .wrap_t = PB_WRAP_REPEAT,
    };
    if (pb_renderer_bind_viewport(&renderer->state_cache, &viewport) !=
            PB_BIND_CHANGED ||
        pb_renderer_bind_pipeline(&renderer->state_cache, &pipeline) !=
            PB_BIND_CHANGED) {
        result = PB_RENDERER_INIT_INVALID_ARGUMENT;
        goto fail;
    }
    renderer->scissor = viewport;
    apply_pipeline(renderer);
    apply_combiner(PB_GFX_COMBINER_FALLBACK);
    renderer->combiner_mode = PB_GFX_COMBINER_FALLBACK;
    sync_stats(renderer);

    *renderer_out = renderer;
    return PB_RENDERER_INIT_OK;

fail:
    pb_renderer_3ds_destroy(renderer);
    return result;
}

bool pb_renderer_3ds_begin_frame(PBRenderer3DS *renderer) {
    if (renderer == NULL || !renderer->c3d_ready || renderer->target == NULL ||
        !renderer->program_ready || renderer->stream_buffer == NULL ||
        renderer->frame_open) {
        return false;
    }
    /* Delete resources retired after the previous FrameEnd only after its
     * queued PICA work has completed. */
    sync_and_release_retired_textures(renderer);
    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) {
        renderer->stats.frame_failures++;
        return false;
    }
    renderer->frame_open = true;
    renderer->stream_used_vertices = 0;
    const C3D_ClearBits clear_bits = renderer->preserve_color_next_frame
                                         ? C3D_CLEAR_DEPTH
                                         : C3D_CLEAR_ALL;
    renderer->preserve_color_next_frame = false;
    C3D_RenderTargetClear(renderer->target, clear_bits,
                          PB_RENDER_CLEAR_COLOR, PB_RENDER_CLEAR_DEPTH);
    if (!C3D_FrameDrawOn(renderer->target)) {
        C3D_FrameEnd(0);
        renderer->frame_open = false;
        renderer->stats.frame_failures++;
        return false;
    }
    C3D_BindProgram(&renderer->program);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, renderer->projection_uniform,
                     &renderer->projection);
    return true;
}

void pb_renderer_3ds_preserve_color(PBRenderer3DS *renderer,
                                    bool preserve_color) {
    if (renderer != NULL && !renderer->frame_open) {
        renderer->preserve_color_next_frame = preserve_color;
    }
}

bool pb_renderer_3ds_end_frame(PBRenderer3DS *renderer) {
    if (renderer == NULL || !renderer->frame_open) {
        return false;
    }
    const float command_usage = C3D_GetCmdBufUsage();
    if (command_usage > renderer->stats.command_buffer_peak) {
        renderer->stats.command_buffer_peak = command_usage;
    }
    if (renderer->stream_used_vertices != 0U) {
        GSPGPU_FlushDataCache(
            renderer->stream_buffer,
            renderer->stream_used_vertices * sizeof(PBRendererVertex));
    }
    C3D_FrameEnd(0);
    renderer->frame_open = false;
    /* Evicted textures can still be referenced by this frame's command list.
     * One synchronization releases the whole retirement batch safely. */
    sync_and_release_retired_textures(renderer);
    renderer->stats.frames++;
    return true;
}

void pb_renderer_3ds_finish(PBRenderer3DS *renderer) {
    if (renderer != NULL && renderer->c3d_ready) {
        C3D_FrameSync();
        /* FrameSync cannot cover commands that are still being recorded in
         * an open frame. EndFrame owns that retirement batch. */
        if (!renderer->frame_open) {
            release_retired_textures(renderer);
        }
    }
}

bool pb_renderer_3ds_clear(PBRenderer3DS *renderer, bool color, bool depth) {
    if (renderer == NULL || renderer->target == NULL ||
        (!color && !depth)) {
        return false;
    }
    C3D_ClearBits bits = color && depth
                              ? C3D_CLEAR_ALL
                              : (color ? C3D_CLEAR_COLOR : C3D_CLEAR_DEPTH);
    C3D_RenderTargetClear(renderer->target, bits, PB_RENDER_CLEAR_COLOR,
                          PB_RENDER_CLEAR_DEPTH);
    return true;
}

bool pb_renderer_3ds_set_viewport(PBRenderer3DS *renderer,
                                  const PBViewport *viewport) {
    if (renderer == NULL) {
        return false;
    }
    const PBBindResult bind =
        pb_renderer_bind_viewport(&renderer->state_cache, viewport);
    if (bind == PB_BIND_REJECTED) return false;
    if (bind == PB_BIND_UNCHANGED) {
        sync_stats(renderer);
        return true;
    }
    PBTargetViewport target;
    if (!pb_renderer_viewport_to_target(viewport, &target)) {
        renderer->state_cache.rejected++;
        return false;
    }
    C3D_SetViewport(target.x, target.y, target.width, target.height);
    sync_stats(renderer);
    return true;
}

bool pb_renderer_3ds_set_scissor(PBRenderer3DS *renderer,
                                 const PBViewport *scissor) {
    PBTargetViewport target;
    if (renderer == NULL ||
        !pb_renderer_viewport_to_target(scissor, &target)) {
        if (renderer != NULL) {
            renderer->state_cache.rejected++;
            sync_stats(renderer);
        }
        return false;
    }
    renderer->scissor = *scissor;
    C3D_SetScissor(GPU_SCISSOR_NORMAL, target.x, target.y,
                   target.x + target.width, target.y + target.height);
    return true;
}

bool pb_renderer_3ds_set_pipeline(PBRenderer3DS *renderer,
                                  const PBRenderPipeline *pipeline) {
    if (renderer == NULL) {
        return false;
    }
    const PBBindResult bind =
        pb_renderer_bind_pipeline(&renderer->state_cache, pipeline);
    if (bind == PB_BIND_REJECTED) return false;
    if (bind == PB_BIND_UNCHANGED) {
        sync_stats(renderer);
        return true;
    }
    apply_pipeline(renderer);
    sync_stats(renderer);
    return true;
}

bool pb_renderer_3ds_upload_texture(PBRenderer3DS *renderer,
                                    uint32_t texture_id,
                                    const uint8_t *rgba32,
                                    uint16_t width, uint16_t height) {
    PBTextureLayout layout;
    if (renderer == NULL || texture_id == 0 || rgba32 == NULL ||
        !pb_renderer_texture_layout(&layout, width, height,
                                    PB_TEXTURE_RGBA8)) {
        return false;
    }

    PBRendererTexture *entry = find_texture(renderer, texture_id);
    if (entry == NULL) {
        entry = find_free_texture(renderer);
    }
    if (entry == NULL) {
        return false;
    }

    C3D_Tex new_texture;
    memset(&new_texture, 0, sizeof(new_texture));
    if (!C3D_TexInit(&new_texture, width, height, GPU_RGBA8)) {
        return false;
    }
    u32 native_size = 0;
    uint8_t *native_pixels =
        C3D_Tex2DGetImagePtr(&new_texture, 0, &native_size);
    if (native_pixels == NULL || native_size < layout.bytes ||
        !pb_renderer_swizzle_rgba8(native_pixels, native_size, rgba32,
                                   layout.bytes, width, height)) {
        C3D_TexDelete(&new_texture);
        return false;
    }
    C3D_TexFlush(&new_texture);

    if (entry->allocated) {
        const size_t old_texture_size = entry->texture.size;
        if (!retire_texture(renderer, &entry->texture)) {
            C3D_TexDelete(&new_texture);
            return false;
        }
        renderer->stats.texture_bytes -= old_texture_size;
    }
    entry->id = texture_id;
    entry->texture = new_texture;
    entry->allocated = true;
    entry->sampler_set = false;
    for (size_t tile = 0; tile < PB_GFX_TEXTURE_UNITS; tile++) {
        if (renderer->bound_textures[tile] == texture_id) {
            renderer->bound_textures[tile] = 0U;
        }
    }
    renderer->stats.texture_bytes += layout.bytes;
    return true;
}

bool pb_renderer_3ds_bind_texture(PBRenderer3DS *renderer, int tile,
                                  uint32_t texture_id) {
    PBRendererTexture *entry = find_texture(renderer, texture_id);
    if (entry == NULL || tile < 0 || tile >= (int)PB_GFX_TEXTURE_UNITS) {
        return false;
    }
    if (renderer->bound_textures[tile] == texture_id) return true;
    C3D_TexBind(tile, &entry->texture);
    renderer->bound_textures[tile] = texture_id;
    return true;
}

bool pb_renderer_3ds_set_sampler(PBRenderer3DS *renderer,
                                 uint32_t texture_id,
                                 PBTextureFilter filter,
                                 PBTextureWrap wrap_s,
                                 PBTextureWrap wrap_t) {
    PBRendererTexture *entry = find_texture(renderer, texture_id);
    if (entry == NULL || (unsigned int)filter >= PB_FILTER_COUNT ||
        (unsigned int)wrap_s >= PB_WRAP_COUNT ||
        (unsigned int)wrap_t >= PB_WRAP_COUNT) {
        return false;
    }
    if (entry->sampler_set && entry->filter == filter &&
        entry->wrap_s == wrap_s && entry->wrap_t == wrap_t) {
        return true;
    }
    C3D_TexSetFilter(&entry->texture, (GPU_TEXTURE_FILTER_PARAM)filter,
                     (GPU_TEXTURE_FILTER_PARAM)filter);
    C3D_TexSetWrap(&entry->texture, (GPU_TEXTURE_WRAP_PARAM)wrap_s,
                   (GPU_TEXTURE_WRAP_PARAM)wrap_t);
    entry->filter = filter;
    entry->wrap_s = wrap_s;
    entry->wrap_t = wrap_t;
    entry->sampler_set = true;
    return true;
}

bool pb_renderer_3ds_delete_texture(PBRenderer3DS *renderer,
                                    uint32_t texture_id) {
    if (renderer == NULL) {
        return false;
    }
    PBRendererTexture *entry = find_texture(renderer, texture_id);
    if (entry == NULL) {
        /* Logical textures do not acquire native storage until upload. */
        return true;
    }
    const size_t texture_size = entry->texture.size;
    if (!retire_texture(renderer, &entry->texture)) {
        return false;
    }
    for (size_t tile = 0; tile < PB_GFX_TEXTURE_UNITS; tile++) {
        if (renderer->bound_textures[tile] == texture_id) {
            renderer->bound_textures[tile] = 0U;
        }
    }
    renderer->stats.texture_bytes -= texture_size;
    memset(entry, 0, sizeof(*entry));
    return true;
}

bool pb_renderer_3ds_set_combiner(PBRenderer3DS *renderer,
                                  int combiner_mode) {
    if (renderer == NULL || combiner_mode < PB_GFX_COMBINER_SHADE ||
        combiner_mode > PB_GFX_COMBINER_FALLBACK) {
        return false;
    }
    if (renderer->combiner_mode == combiner_mode) return true;
    apply_combiner(combiner_mode);
    renderer->combiner_mode = combiner_mode;
    renderer->combiner_program_valid = false;
    return true;
}

bool pb_renderer_3ds_set_combiner_program(PBRenderer3DS *renderer,
                                          const PBGfxTevProgram *program) {
    if (renderer == NULL || program == NULL) {
        return false;
    }
    if (renderer->combiner_program_valid &&
        memcmp(&renderer->combiner_program, program,
               sizeof(*program)) == 0) {
        return true;
    }
    if (!apply_combiner_program(program)) return false;
    renderer->combiner_program = *program;
    renderer->combiner_program_valid = true;
    renderer->combiner_mode = -1;
    return true;
}

bool pb_renderer_3ds_set_fog(PBRenderer3DS *renderer, bool enabled,
                             uint8_t red, uint8_t green, uint8_t blue,
                             int16_t fog_multiply, int16_t fog_offset) {
    if (renderer == NULL) {
        return false;
    }
    if (renderer->fog_state_valid && renderer->fog_enabled == enabled &&
        (!enabled ||
         (renderer->fog_red == red && renderer->fog_green == green &&
          renderer->fog_blue == blue &&
          renderer->fog_multiply == fog_multiply &&
          renderer->fog_offset == fog_offset))) {
        return true;
    }

    if (!enabled) {
        C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
        C3D_FogLutBind(NULL);
    } else {
        float values[PB_RENDER_FOG_LUT_VALUES];
        if (!pb_renderer_fast3d_fog_lut(values, fog_multiply, fog_offset)) {
            return false;
        }
        FogLut_FromArray(&renderer->fog_lut, values);
        C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
        C3D_FogColor((uint32_t)red | ((uint32_t)green << 8U) |
                     ((uint32_t)blue << 16U));
        C3D_FogLutBind(&renderer->fog_lut);
    }

    renderer->fog_enabled = enabled;
    renderer->fog_red = red;
    renderer->fog_green = green;
    renderer->fog_blue = blue;
    renderer->fog_multiply = fog_multiply;
    renderer->fog_offset = fog_offset;
    renderer->fog_state_valid = true;
    return true;
}

bool pb_renderer_3ds_draw_stream(PBRenderer3DS *renderer,
                                 const float *vertices,
                                 size_t float_count,
                                 size_t triangle_count,
                                 size_t vertex_stride_floats,
                                 bool uses_texture0,
                                 bool uses_texture1,
                                 bool uses_shade,
                                 bool uses_alpha) {
    if (renderer == NULL || !renderer->frame_open || vertices == NULL ||
        triangle_count == 0 ||
        triangle_count > PB_GFX_MAX_STREAM_TRIANGLES ||
        triangle_count > SIZE_MAX / 3U) {
        return false;
    }
    const size_t vertex_count = triangle_count * 3U;
    if (vertex_stride_floats == 0 ||
        vertex_count > SIZE_MAX / vertex_stride_floats ||
        float_count != vertex_count * vertex_stride_floats) {
        return false;
    }
    size_t first_vertex = 0;
    size_t next_used_vertices = 0;
    if (!pb_renderer_stream_reserve(renderer->stream_capacity_vertices,
                                    renderer->stream_used_vertices,
                                    vertex_count, &first_vertex,
                                    &next_used_vertices)) {
        renderer->stats.stream_overflows++;
        return false;
    }

    for (size_t vertex_index = 0; vertex_index < vertex_count;
         vertex_index++) {
        const float *source =
            &vertices[vertex_index * vertex_stride_floats];
        PBRendererVertex *destination =
            &renderer->stream_buffer[first_vertex + vertex_index];
        size_t offset = 5U;

        destination->position[0] = source[0];
        destination->position[1] = source[1];
        destination->position[2] = source[2];
        destination->position[3] = source[3];
        destination->texcoord0[0] = 0.0f;
        destination->texcoord0[1] = 0.0f;
        destination->texcoord1[0] = 0.0f;
        destination->texcoord1[1] = 0.0f;
        if (uses_texture0) {
            destination->texcoord0[0] = source[offset + 0U];
            destination->texcoord0[1] = source[offset + 1U];
            offset += 2U;
        }
        if (uses_texture1) {
            destination->texcoord1[0] = source[offset + 0U];
            destination->texcoord1[1] = source[offset + 1U];
            offset += 2U;
        }
        if (uses_shade) {
            destination->color[0] = source[offset + 0U];
            destination->color[1] = source[offset + 1U];
            destination->color[2] = source[offset + 2U];
            destination->color[3] = uses_alpha ? source[offset + 3U] : 1.0f;
        } else {
            destination->color[0] = 1.0f;
            destination->color[1] = 1.0f;
            destination->color[2] = 1.0f;
            destination->color[3] = 1.0f;
        }
    }

    C3D_BufInfo *buffers = C3D_GetBufInfo();
    BufInfo_Init(buffers);
    PBRendererVertex *draw_buffer = &renderer->stream_buffer[first_vertex];
    BufInfo_Add(buffers, draw_buffer, sizeof(PBRendererVertex), 4, 0x3210);
    C3D_DrawArrays(GPU_TRIANGLES, 0, (int)vertex_count);
    renderer->stream_used_vertices = next_used_vertices;
    if (renderer->stream_used_vertices >
        renderer->stats.stream_peak_vertices) {
        renderer->stats.stream_peak_vertices =
            renderer->stream_used_vertices;
    }
    renderer->stats.draw_calls++;
    renderer->stats.vertices += vertex_count;
    return true;
}

bool pb_renderer_3ds_render(PBRenderer3DS *renderer) {
    if (!pb_renderer_3ds_begin_frame(renderer)) {
        return false;
    }
    return pb_renderer_3ds_end_frame(renderer);
}

const PBRendererStats *pb_renderer_3ds_stats(const PBRenderer3DS *renderer) {
    return renderer != NULL ? &renderer->stats : NULL;
}

void pb_renderer_3ds_destroy(PBRenderer3DS *renderer) {
    if (renderer == NULL) {
        return;
    }
    if (renderer->frame_open) {
        C3D_FrameEnd(0);
        renderer->frame_open = false;
    }
    if (renderer->c3d_ready) {
        C3D_FrameSync();
        release_retired_textures(renderer);
    }
    for (size_t index = 0; index < PB_GFX_MAX_TEXTURES; index++) {
        if (renderer->textures[index].allocated) {
            C3D_TexDelete(&renderer->textures[index].texture);
        }
    }
    if (renderer->stream_buffer != NULL) {
        linearFree(renderer->stream_buffer);
    }
    if (renderer->program_ready) {
        shaderProgramFree(&renderer->program);
    }
    if (renderer->shader_dvlb != NULL) {
        DVLB_Free(renderer->shader_dvlb);
    }
    if (renderer->target != NULL) {
        C3D_RenderTargetDelete(renderer->target);
    }
    if (renderer->c3d_ready) {
        C3D_Fini();
    }
    free(renderer);
}
