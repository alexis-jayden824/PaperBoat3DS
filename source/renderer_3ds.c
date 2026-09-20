#include "pb3ds/renderer.h"

#include <3ds.h>
#include <citro3d.h>
#include <stdlib.h>
#include <string.h>

#include "renderer_shbin.h"

#define PB_RENDER_CLEAR_COLOR 0x10243BFFU
#define PB_RENDER_CLEAR_DEPTH 0U
#define PB_RENDER_CHECKER_SIZE 8U

#define PB_DISPLAY_TRANSFER_FLAGS                                             \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |                  \
     GX_TRANSFER_RAW_COPY(0) |                                               \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |                          \
     GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |                          \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct {
    float position[3];
    float texcoord[2];
    float color[4];
} PBRendererVertex;

static const PBRendererVertex diagnostic_vertices[] = {
    /* Textured foundation panel. */
    { { 34.0f, 30.0f, 0.45f }, { 0.0f, 0.0f }, { 0.95f, 0.98f, 1.0f, 0.96f } },
    { { 366.0f, 30.0f, 0.45f }, { 6.0f, 0.0f }, { 0.95f, 0.98f, 1.0f, 0.96f } },
    { { 366.0f, 210.0f, 0.45f }, { 6.0f, 4.0f }, { 0.95f, 0.98f, 1.0f, 0.96f } },
    { { 366.0f, 210.0f, 0.45f }, { 6.0f, 4.0f }, { 0.95f, 0.98f, 1.0f, 0.96f } },
    { { 34.0f, 210.0f, 0.45f }, { 0.0f, 4.0f }, { 0.95f, 0.98f, 1.0f, 0.96f } },
    { { 34.0f, 30.0f, 0.45f }, { 0.0f, 0.0f }, { 0.95f, 0.98f, 1.0f, 0.96f } },

    /* A translucent sail confirms vertex color, depth, and alpha blending. */
    { { 118.0f, 66.0f, 0.70f }, { 0.0f, 0.0f }, { 1.0f, 0.48f, 0.20f, 0.72f } },
    { { 286.0f, 66.0f, 0.70f }, { 3.0f, 0.0f }, { 0.98f, 0.78f, 0.25f, 0.72f } },
    { { 202.0f, 190.0f, 0.70f }, { 1.5f, 3.0f }, { 0.35f, 0.90f, 0.82f, 0.72f } },
};

#define PB_DIAGNOSTIC_VERTEX_COUNT                                           \
    (sizeof(diagnostic_vertices) / sizeof(diagnostic_vertices[0]))
#define PB_PANEL_VERTEX_COUNT 6U
#define PB_SAIL_VERTEX_COUNT 3U

struct PBRenderer3DS {
    C3D_RenderTarget *target;
    DVLB_s *shader_dvlb;
    shaderProgram_s program;
    C3D_Mtx projection;
    C3D_Tex checker_texture;
    void *vertex_buffer;
    int projection_uniform;
    PBTextureLayout texture_layout;
    PBRenderStateCache state_cache;
    PBRendererStats stats;
    bool c3d_ready;
    bool program_ready;
    bool texture_ready;
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

static bool make_checker_texture(uint8_t *swizzled, size_t swizzled_size) {
    uint8_t linear[PB_RENDER_CHECKER_SIZE * PB_RENDER_CHECKER_SIZE * 4U];
    for (uint16_t y = 0; y < PB_RENDER_CHECKER_SIZE; y++) {
        for (uint16_t x = 0; x < PB_RENDER_CHECKER_SIZE; x++) {
            const size_t offset = ((size_t)y * PB_RENDER_CHECKER_SIZE + x) * 4U;
            const bool alternate = (((x / 2U) ^ (y / 2U)) & 1U) != 0;
            linear[offset + 0U] = alternate ? 232U : 28U;
            linear[offset + 1U] = alternate ? 245U : 107U;
            linear[offset + 2U] = alternate ? 244U : 117U;
            linear[offset + 3U] = 255U;
        }
    }
    return pb_renderer_swizzle_rgba8(
        swizzled, swizzled_size, linear, sizeof(linear),
        PB_RENDER_CHECKER_SIZE, PB_RENDER_CHECKER_SIZE);
}

static void apply_pipeline(PBRenderer3DS *renderer) {
    const PBRenderPipeline *pipeline = &renderer->state_cache.pipeline;

    C3D_CullFace((GPU_CULLMODE)pipeline->cull_mode);
    C3D_DepthTest(pipeline->depth_test_enabled,
                  (GPU_TESTFUNC)pipeline->depth_function,
                  pipeline->depth_write_enabled ? GPU_WRITE_ALL
                                                : GPU_WRITE_COLOR);

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

    C3D_TexSetFilter(&renderer->checker_texture,
                     (GPU_TEXTURE_FILTER_PARAM)pipeline->mag_filter,
                     (GPU_TEXTURE_FILTER_PARAM)pipeline->min_filter);
    C3D_TexSetWrap(&renderer->checker_texture,
                   (GPU_TEXTURE_WRAP_PARAM)pipeline->wrap_s,
                   (GPU_TEXTURE_WRAP_PARAM)pipeline->wrap_t);
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
    AttrInfo_AddLoader(attributes, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attributes, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attributes, 2, GPU_FLOAT, 4);

    Mtx_OrthoTilt(&renderer->projection, 0.0f, (float)PB_RENDER_TOP_WIDTH,
                  0.0f, (float)PB_RENDER_TOP_HEIGHT, 0.0f, 1.0f, true);

    result = PB_RENDERER_INIT_VERTEX_BUFFER;
    if (!pb_renderer_vertex_buffer_size(sizeof(PBRendererVertex),
                                        PB_DIAGNOSTIC_VERTEX_COUNT,
                                        &renderer->stats.vertex_buffer_bytes)) {
        goto fail;
    }
    renderer->vertex_buffer =
        linearAlloc(renderer->stats.vertex_buffer_bytes);
    if (renderer->vertex_buffer == NULL) {
        goto fail;
    }
    memcpy(renderer->vertex_buffer, diagnostic_vertices,
           renderer->stats.vertex_buffer_bytes);

    C3D_BufInfo *buffers = C3D_GetBufInfo();
    BufInfo_Init(buffers);
    BufInfo_Add(buffers, renderer->vertex_buffer, sizeof(PBRendererVertex), 3,
                0x210);

    result = PB_RENDERER_INIT_TEXTURE;
    if (!pb_renderer_texture_layout(&renderer->texture_layout,
                                    PB_RENDER_CHECKER_SIZE,
                                    PB_RENDER_CHECKER_SIZE,
                                    PB_TEXTURE_RGBA8) ||
        !C3D_TexInit(&renderer->checker_texture, PB_RENDER_CHECKER_SIZE,
                     PB_RENDER_CHECKER_SIZE, GPU_RGBA8)) {
        goto fail;
    }
    renderer->texture_ready = true;
    renderer->stats.texture_bytes = renderer->texture_layout.bytes;

    uint8_t checker_texels[PB_RENDER_CHECKER_SIZE * PB_RENDER_CHECKER_SIZE * 4U];
    if (!make_checker_texture(checker_texels, sizeof(checker_texels))) {
        goto fail;
    }
    C3D_TexUpload(&renderer->checker_texture, checker_texels);
    C3D_TexBind(0, &renderer->checker_texture);

    C3D_TexEnv *environment = C3D_GetTexEnv(0);
    C3D_TexEnvInit(environment);
    C3D_TexEnvSrc(environment, C3D_Both, GPU_TEXTURE0,
                  GPU_PRIMARY_COLOR, 0);
    C3D_TexEnvFunc(environment, C3D_Both, GPU_MODULATE);

    pb_renderer_state_cache_init(&renderer->state_cache);
    const PBViewport viewport = {
        .x = 0,
        .y = 0,
        .width = PB_RENDER_TOP_WIDTH,
        .height = PB_RENDER_TOP_HEIGHT,
    };
    const PBRenderPipeline pipeline = {
        .cull_mode = PB_CULL_NONE,
        .depth_test_enabled = true,
        .depth_write_enabled = true,
        .depth_function = PB_COMPARE_GREATER,
        .blend_mode = PB_BLEND_ALPHA,
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
    apply_pipeline(renderer);
    sync_stats(renderer);

    *renderer_out = renderer;
    return PB_RENDERER_INIT_OK;

fail:
    pb_renderer_3ds_destroy(renderer);
    return result;
}

bool pb_renderer_3ds_render(PBRenderer3DS *renderer) {
    if (renderer == NULL || !renderer->c3d_ready || renderer->target == NULL ||
        !renderer->program_ready || !renderer->texture_ready ||
        renderer->vertex_buffer == NULL) {
        return false;
    }

    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) {
        renderer->stats.frame_failures++;
        renderer->state_cache.rejected++;
        sync_stats(renderer);
        return false;
    }

    C3D_RenderTargetClear(renderer->target, C3D_CLEAR_ALL,
                          PB_RENDER_CLEAR_COLOR, PB_RENDER_CLEAR_DEPTH);
    if (!C3D_FrameDrawOn(renderer->target)) {
        C3D_FrameEnd(0);
        renderer->stats.frame_failures++;
        renderer->state_cache.rejected++;
        sync_stats(renderer);
        return false;
    }

    PBTargetViewport target_viewport;
    if (!pb_renderer_viewport_to_target(&renderer->state_cache.viewport,
                                        &target_viewport)) {
        C3D_FrameEnd(0);
        renderer->stats.frame_failures++;
        renderer->state_cache.rejected++;
        sync_stats(renderer);
        return false;
    }

    (void)pb_renderer_bind_viewport(&renderer->state_cache,
                                    &renderer->state_cache.viewport);
    (void)pb_renderer_bind_pipeline(&renderer->state_cache,
                                    &renderer->state_cache.pipeline);
    C3D_SetViewport(target_viewport.x, target_viewport.y,
                    target_viewport.width, target_viewport.height);
    C3D_BindProgram(&renderer->program);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, renderer->projection_uniform,
                     &renderer->projection);
    C3D_TexBind(0, &renderer->checker_texture);
    apply_pipeline(renderer);

    C3D_DrawArrays(GPU_TRIANGLES, 0, PB_PANEL_VERTEX_COUNT);
    C3D_DrawArrays(GPU_TRIANGLES, PB_PANEL_VERTEX_COUNT,
                   PB_SAIL_VERTEX_COUNT);

    const float command_usage = C3D_GetCmdBufUsage();
    if (command_usage > renderer->stats.command_buffer_peak) {
        renderer->stats.command_buffer_peak = command_usage;
    }
    C3D_FrameEnd(0);

    renderer->stats.frames++;
    renderer->stats.draw_calls += 2;
    renderer->stats.vertices += PB_DIAGNOSTIC_VERTEX_COUNT;
    sync_stats(renderer);
    return true;
}

const PBRendererStats *pb_renderer_3ds_stats(const PBRenderer3DS *renderer) {
    return renderer != NULL ? &renderer->stats : NULL;
}

void pb_renderer_3ds_destroy(PBRenderer3DS *renderer) {
    if (renderer == NULL) {
        return;
    }

    if (renderer->c3d_ready) {
        C3D_FrameSync();
    }
    if (renderer->texture_ready) {
        C3D_TexDelete(&renderer->checker_texture);
    }
    if (renderer->vertex_buffer != NULL) {
        linearFree(renderer->vertex_buffer);
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
