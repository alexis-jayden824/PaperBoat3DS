#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PB_RENDER_TOP_WIDTH 400U
#define PB_RENDER_TOP_HEIGHT 240U
#define PB_RENDER_TARGET_WIDTH 240U
#define PB_RENDER_TARGET_HEIGHT 400U
#define PB_RENDER_TEXTURE_MIN_DIMENSION 8U
#define PB_RENDER_TEXTURE_MAX_DIMENSION 1024U
#define PB_RENDER_FOG_LUT_VALUES 256U
/*
 * Mtx_OrthoTilt still needs dummy near/far because it fills the whole
 * matrix, but row 2 is then replaced with identity. N64 clip Z is converted
 * into PICA's [-w, 0] window in the vertex stream instead of remapping
 * depth through a second 0..1 ortho, which sheared near walls and sprite
 * heads onto a horizontal clip line.
 */
#define PB_RENDER_ORTHO_NEAR (0.0f)
#define PB_RENDER_ORTHO_FAR (1.0f)
#define PB_RENDER_ORTHO_Z_IDENTITY_ZZ (1.0f)
#define PB_RENDER_ORTHO_Z_IDENTITY_ZW (0.0f)

/* N64 clip Z is [-w, w] (-w near). PICA requires [-w, 0] (-w near, 0 far). */
static inline float pb_renderer_n64_to_pica_clip_z(float clip_z,
                                                   float clip_w) {
    return 0.5f * clip_z - 0.5f * clip_w;
}

/* Screen-space prim depth is reverse-Z 1=near, 0=far. Recover N64 clip Z. */
static inline float pb_renderer_screen_depth_to_n64_clip_z(float depth,
                                                           float clip_w) {
    return clip_w * (1.0f - 2.0f * depth);
}

/* Reverse-Z 0..1 screen depth to PICA clip Z in [-w, 0] (near = -w). */
static inline float pb_renderer_screen_depth_to_pica_z(float depth,
                                                       float clip_w) {
    return pb_renderer_n64_to_pica_clip_z(
        pb_renderer_screen_depth_to_n64_clip_z(depth, clip_w), clip_w);
}

/*
 * PICA does not clip W<=0 the way Fast3D/OpenGL does. Homogeneous vertices
 * behind the eye become screen-spanning slivers. Clip edges where W crosses
 * this epsilon before submission.
 */
/*
 * Clip only vertices behind the eye. 1/32 was large enough to shave Mario's
 * hat and NPC heads (small but legal W). |x|,|y|,|z| <= w still removes the
 * screen-spanning slivers that negative-W vertices produced on PICA.
 */
#define PB_RENDER_CLIP_W_EPS (1.0f / 1024.0f)

static inline bool pb_renderer_clip_w_inside(float clip_w) {
    return clip_w > PB_RENDER_CLIP_W_EPS;
}

static inline float pb_renderer_clip_w_edge_t(float w0, float w1) {
    const float denom = w1 - w0;
    if (denom == 0.0f) {
        return 0.0f;
    }
    return (PB_RENDER_CLIP_W_EPS - w0) / denom;
}

/*
 * N64 homogeneous clip volume is |x|,|y|,|z| <= w with w > 0. PICA's GPU
 * clipper sees screen-mapped vertices after OrthoTilt, so triangles that
 * only failed the N64 planes become screen-spanning slivers. Clip the
 * Fast3D volume on the CPU, then submit.
 */
#define PB_RENDER_CLIP_PLANE_COUNT 7U
#define PB_RENDER_CLIP_MAX_VERTS 16U

typedef struct {
    float x;
    float y;
    float z;
    float w;
} PBClipVertex;

static inline float pb_renderer_n64_clip_plane(const PBClipVertex *vertex,
                                               unsigned int plane) {
    switch (plane) {
        case 0U:
            return vertex->w - PB_RENDER_CLIP_W_EPS;
        case 1U:
            return vertex->w - vertex->x;
        case 2U:
            return vertex->w + vertex->x;
        case 3U:
            return vertex->w - vertex->y;
        case 4U:
            return vertex->w + vertex->y;
        case 5U:
            return vertex->w - vertex->z;
        case 6U:
            return vertex->w + vertex->z;
        default:
            return 0.0f;
    }
}

static inline PBClipVertex pb_renderer_clip_lerp(const PBClipVertex *a,
                                                 const PBClipVertex *b,
                                                 float t) {
    PBClipVertex out;
    const float s = 1.0f - t;
    out.x = a->x * s + b->x * t;
    out.y = a->y * s + b->y * t;
    out.z = a->z * s + b->z * t;
    out.w = a->w * s + b->w * t;
    return out;
}

static inline size_t pb_renderer_clip_against_plane(const PBClipVertex *in,
                                                    size_t count,
                                                    PBClipVertex *out,
                                                    unsigned int plane) {
    size_t produced = 0U;
    size_t index;

    if (in == NULL || out == NULL || count < 2U) {
        return 0U;
    }
    for (index = 0U; index < count; index++) {
        const PBClipVertex *a = &in[index];
        const PBClipVertex *b = &in[(index + 1U) % count];
        const float fa = pb_renderer_n64_clip_plane(a, plane);
        const float fb = pb_renderer_n64_clip_plane(b, plane);
        const bool a_inside = fa >= 0.0f;
        const bool b_inside = fb >= 0.0f;

        if (a_inside && produced < PB_RENDER_CLIP_MAX_VERTS) {
            out[produced++] = *a;
        }
        if (a_inside != b_inside && produced < PB_RENDER_CLIP_MAX_VERTS) {
            const float denom = fa - fb;
            float t = denom == 0.0f ? 0.0f : fa / denom;
            if (t < 0.0f) {
                t = 0.0f;
            } else if (t > 1.0f) {
                t = 1.0f;
            }
            out[produced++] = pb_renderer_clip_lerp(a, b, t);
        }
    }
    return produced;
}

static inline size_t pb_renderer_clip_n64_triangle(const PBClipVertex *triangle,
                                                   PBClipVertex *out) {
    PBClipVertex current[PB_RENDER_CLIP_MAX_VERTS];
    PBClipVertex next[PB_RENDER_CLIP_MAX_VERTS];
    size_t count;
    unsigned int plane;

    if (triangle == NULL || out == NULL) {
        return 0U;
    }
    current[0] = triangle[0];
    current[1] = triangle[1];
    current[2] = triangle[2];
    count = 3U;
    for (plane = 0U; plane < PB_RENDER_CLIP_PLANE_COUNT; plane++) {
        count = pb_renderer_clip_against_plane(current, count, next, plane);
        if (count < 3U) {
            return 0U;
        }
        {
            size_t index;
            for (index = 0U; index < count; index++) {
                current[index] = next[index];
            }
        }
    }
    {
        size_t index;
        for (index = 0U; index < count; index++) {
            out[index] = current[index];
        }
    }
    return count;
}

/*
 * Fast3D keep-sign in pre-Y-flip clip space: G_CULL_FRONT keeps C > 0,
 * G_CULL_BACK keeps C < 0. Hardware PICA winding after OrthoTilt does not
 * match this, so the interpreter culls here instead of via GPU faces.
 */
static inline float pb_renderer_clip_face_cross(float x0, float y0, float w0,
                                                float x1, float y1, float w1,
                                                float x2, float y2, float w2) {
    const float ax = x0 / w0;
    const float ay = y0 / w0;
    const float bx = x1 / w1;
    const float by = y1 / w1;
    const float cx = x2 / w2;
    const float cy = y2 / w2;
    return (ax - bx) * (cy - by) - (ay - by) * (cx - bx);
}

static inline bool pb_renderer_clip_keep_face(float cross, int8_t keep_sign) {
    if (keep_sign == 0) {
        return true;
    }
    return keep_sign > 0 ? cross > 0.0f : cross < 0.0f;
}

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

/*
 * A textured rectangle in the renderer's bottom-left 400x240 coordinate
 * system. PICA samples the decoded OTR resource correctly when the lower
 * screen edge uses the minimum V coordinate and the upper edge uses the
 * maximum V coordinate. Keeping this mapping in one tested helper prevents
 * individual checkpoint scenes from silently reintroducing vertical flips.
 */
typedef struct {
    float left;
    float bottom;
    float right;
    float top;
    float left_u;
    float bottom_v;
    float right_u;
    float top_v;
} PBTexturedQuad;

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
    bool alpha_test_enabled;
    PBCompareFunction alpha_function;
    uint8_t alpha_reference;
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
    size_t stream_capacity_vertices;
    size_t stream_peak_vertices;
    uint32_t stream_overflows;
    size_t texture_bytes;
    uint64_t texture_retirements;
    uint32_t retired_texture_peak;
    uint32_t texture_retire_failures;
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
typedef struct PBGfxTevProgram PBGfxTevProgram;

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
bool pb_renderer_stream_reserve(size_t capacity_vertices,
                                size_t used_vertices,
                                size_t requested_vertices,
                                size_t *first_vertex,
                                size_t *next_used_vertices);
bool pb_renderer_textured_quad(PBTexturedQuad *quad, float left,
                               float bottom, float width, float height,
                               uint16_t texture_width,
                               uint16_t texture_height,
                               uint16_t source_width,
                               uint16_t source_height);
/* N64 texture rows are top-to-bottom; PICA's logical V axis is bottom-to-top. */
float pb_renderer_n64_texture_v(float n64_v, uint16_t source_height,
                                uint16_t texture_height);
/* Build PICA visibility samples/deltas for Fast3D's clip-Z fog equation. */
bool pb_renderer_fast3d_fog_lut(float values[PB_RENDER_FOG_LUT_VALUES],
                                int16_t fog_multiply, int16_t fog_offset);

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
bool pb_renderer_3ds_begin_frame(PBRenderer3DS *renderer);
void pb_renderer_3ds_preserve_color(PBRenderer3DS *renderer,
                                    bool preserve_color);
bool pb_renderer_3ds_end_frame(PBRenderer3DS *renderer);
void pb_renderer_3ds_finish(PBRenderer3DS *renderer);
bool pb_renderer_3ds_clear(PBRenderer3DS *renderer, bool color, bool depth);
bool pb_renderer_3ds_set_viewport(PBRenderer3DS *renderer,
                                  const PBViewport *viewport);
bool pb_renderer_3ds_set_scissor(PBRenderer3DS *renderer,
                                 const PBViewport *scissor);
bool pb_renderer_3ds_set_pipeline(PBRenderer3DS *renderer,
                                  const PBRenderPipeline *pipeline);
bool pb_renderer_3ds_upload_texture(PBRenderer3DS *renderer,
                                    uint32_t texture_id,
                                    const uint8_t *rgba32,
                                    uint16_t width, uint16_t height);
bool pb_renderer_3ds_bind_texture(PBRenderer3DS *renderer, int tile,
                                  uint32_t texture_id);
bool pb_renderer_3ds_set_sampler(PBRenderer3DS *renderer,
                                 uint32_t texture_id,
                                 PBTextureFilter filter,
                                 PBTextureWrap wrap_s,
                                 PBTextureWrap wrap_t);
bool pb_renderer_3ds_delete_texture(PBRenderer3DS *renderer,
                                    uint32_t texture_id);
bool pb_renderer_3ds_set_combiner(PBRenderer3DS *renderer,
                                  int combiner_mode);
bool pb_renderer_3ds_set_combiner_program(PBRenderer3DS *renderer,
                                          const PBGfxTevProgram *program);
bool pb_renderer_3ds_set_fog(PBRenderer3DS *renderer, bool enabled,
                             uint8_t red, uint8_t green, uint8_t blue,
                             int16_t fog_multiply, int16_t fog_offset);
bool pb_renderer_3ds_draw_stream(PBRenderer3DS *renderer,
                                 const float *vertices,
                                 size_t float_count,
                                 size_t triangle_count,
                                 size_t vertex_stride_floats,
                                 bool uses_texture0,
                                 bool uses_texture1,
                                 bool uses_shade,
                                 bool uses_alpha);
const PBRendererStats *pb_renderer_3ds_stats(const PBRenderer3DS *renderer);
void pb_renderer_3ds_destroy(PBRenderer3DS *renderer);

#ifdef __cplusplus
}
#endif
