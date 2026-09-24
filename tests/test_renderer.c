#include "pb3ds/renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks_run;

#define CHECK(expression)                                                      \
    do {                                                                       \
        checks_run++;                                                          \
        if (!(expression)) {                                                   \
            fprintf(stderr, "renderer contract check failed at %s:%d: %s\n", \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool test_texture_formats(void) {
    static const uint8_t expected_bits[] = {
        32, 24, 16, 16, 16, 16, 16, 8, 8, 8, 4, 4, 4, 8,
    };
    static const char *const expected_names[] = {
        "RGBA8", "RGB8", "RGBA5551", "RGB565", "RGBA4", "LA8",
        "HILO8", "L8", "A8", "LA4", "L4", "A4", "ETC1",
        "ETC1A4",
    };

    for (unsigned int format = 0; format < PB_TEXTURE_FORMAT_COUNT; format++) {
        PBTextureLayout layout;
        CHECK(pb_renderer_texture_layout(&layout, 8, 8,
                                         (PBTextureFormat)format));
        CHECK(layout.width == 8);
        CHECK(layout.height == 8);
        CHECK(layout.format == (PBTextureFormat)format);
        CHECK(layout.bits_per_pixel == expected_bits[format]);
        CHECK(layout.bytes == 64U * expected_bits[format] / 8U);
        CHECK(strcmp(pb_renderer_texture_format_name((PBTextureFormat)format),
                     expected_names[format]) == 0);
    }

    PBTextureLayout layout;
    CHECK(pb_renderer_texture_layout(&layout, 1024, 1024, PB_TEXTURE_RGBA8));
    CHECK(layout.bytes == 4U * 1024U * 1024U);
    CHECK(!pb_renderer_texture_layout(&layout, 7, 8, PB_TEXTURE_RGBA8));
    CHECK(!pb_renderer_texture_layout(&layout, 12, 8, PB_TEXTURE_RGBA8));
    CHECK(!pb_renderer_texture_layout(&layout, 8, 1025, PB_TEXTURE_RGBA8));
    CHECK(!pb_renderer_texture_layout(
        &layout, 8, 8, (PBTextureFormat)PB_TEXTURE_FORMAT_COUNT));
    CHECK(!pb_renderer_texture_layout(NULL, 8, 8, PB_TEXTURE_RGBA8));
    CHECK(strcmp(pb_renderer_texture_format_name((PBTextureFormat)-1),
                 "invalid") == 0);
    return true;
}

static bool test_texture_swizzle(void) {
    CHECK(pb_renderer_swizzled_texel_index(0, 0, 8, 8) == 0);
    CHECK(pb_renderer_swizzled_texel_index(1, 0, 8, 8) == 1);
    CHECK(pb_renderer_swizzled_texel_index(0, 1, 8, 8) == 2);
    CHECK(pb_renderer_swizzled_texel_index(2, 0, 8, 8) == 4);
    CHECK(pb_renderer_swizzled_texel_index(7, 7, 8, 8) == 63);
    CHECK(pb_renderer_swizzled_texel_index(8, 0, 16, 8) == 64);
    CHECK(pb_renderer_swizzled_texel_index(0, 8, 8, 16) == 64);

    bool visited[64] = { false };
    for (uint16_t y = 0; y < 8; y++) {
        for (uint16_t x = 0; x < 8; x++) {
            const size_t index =
                pb_renderer_swizzled_texel_index(x, y, 8, 8);
            CHECK(index < 64);
            CHECK(!visited[index]);
            visited[index] = true;
        }
    }
    CHECK(pb_renderer_swizzled_texel_index(8, 0, 8, 8) == SIZE_MAX);
    CHECK(pb_renderer_swizzled_texel_index(0, 8, 8, 8) == SIZE_MAX);
    CHECK(pb_renderer_swizzled_texel_index(0, 0, 12, 8) == SIZE_MAX);

    uint8_t source[8U * 8U * 4U];
    uint8_t destination[sizeof(source)];
    for (size_t texel = 0; texel < 64; texel++) {
        source[texel * 4U + 0U] = (uint8_t)texel;
        source[texel * 4U + 1U] = (uint8_t)(texel + 1U);
        source[texel * 4U + 2U] = (uint8_t)(texel + 2U);
        source[texel * 4U + 3U] = (uint8_t)(255U - texel);
    }
    memset(destination, 0, sizeof(destination));
    CHECK(pb_renderer_swizzle_rgba8(destination, sizeof(destination), source,
                                    sizeof(source), 8, 8));
    const size_t source_texel = 2U * 8U + 3U;
    const size_t swizzled_texel =
        pb_renderer_swizzled_texel_index(3, 2, 8, 8);
    CHECK(destination[swizzled_texel * 4U + 0U] ==
          source[source_texel * 4U + 3U]);
    CHECK(destination[swizzled_texel * 4U + 1U] ==
          source[source_texel * 4U + 2U]);
    CHECK(destination[swizzled_texel * 4U + 2U] ==
          source[source_texel * 4U + 1U]);
    CHECK(destination[swizzled_texel * 4U + 3U] ==
          source[source_texel * 4U + 0U]);
    CHECK(!pb_renderer_swizzle_rgba8(destination, sizeof(destination) - 1U,
                                     source, sizeof(source), 8, 8));
    CHECK(!pb_renderer_swizzle_rgba8(destination, sizeof(destination), source,
                                     sizeof(source) - 1U, 8, 8));
    CHECK(!pb_renderer_swizzle_rgba8(NULL, sizeof(destination), source,
                                     sizeof(source), 8, 8));
    return true;
}

static bool test_viewport_rotation(void) {
    PBTargetViewport target;
    const PBViewport full = {
        .x = 0,
        .y = 0,
        .width = PB_RENDER_TOP_WIDTH,
        .height = PB_RENDER_TOP_HEIGHT,
    };
    CHECK(pb_renderer_viewport_to_target(&full, &target));
    CHECK(target.x == 0);
    CHECK(target.y == 0);
    CHECK(target.width == PB_RENDER_TARGET_WIDTH);
    CHECK(target.height == PB_RENDER_TARGET_HEIGHT);

    const PBViewport inset = { .x = 40, .y = 20, .width = 100, .height = 50 };
    CHECK(pb_renderer_viewport_to_target(&inset, &target));
    CHECK(target.x == 20);
    CHECK(target.y == 260);
    CHECK(target.width == 50);
    CHECK(target.height == 100);

    const PBViewport empty = { .x = 0, .y = 0, .width = 0, .height = 1 };
    const PBViewport off_right = { .x = 399, .y = 0, .width = 2, .height = 1 };
    const PBViewport off_top = { .x = 0, .y = 239, .width = 1, .height = 2 };
    CHECK(!pb_renderer_viewport_to_target(&empty, &target));
    CHECK(!pb_renderer_viewport_to_target(&off_right, &target));
    CHECK(!pb_renderer_viewport_to_target(&off_top, &target));
    CHECK(!pb_renderer_viewport_to_target(NULL, &target));
    CHECK(!pb_renderer_viewport_to_target(&full, NULL));
    return true;
}

static bool test_textured_quad_orientation(void) {
    PBTexturedQuad quad;
    CHECK(pb_renderer_textured_quad(&quad, 52.0f, 20.0f, 296.0f, 200.0f,
                                    512, 256, 296, 200));
    CHECK(quad.left == 52.0f);
    CHECK(quad.bottom == 20.0f);
    CHECK(quad.right == 348.0f);
    CHECK(quad.top == 220.0f);
    CHECK(quad.left_u == 0.5f / 512.0f);
    CHECK(quad.bottom_v == 0.5f / 256.0f);
    CHECK(quad.right_u == 295.5f / 512.0f);
    CHECK(quad.top_v == 199.5f / 256.0f);

    /* Regression for the M11 Folium capture: bottom gets min V, top max V. */
    CHECK(quad.bottom_v < quad.top_v);
    CHECK(pb_renderer_n64_texture_v(0.0f, 30U, 32U) == 30.0f / 32.0f);
    CHECK(pb_renderer_n64_texture_v(30.0f, 30U, 32U) == 0.0f);
    CHECK(pb_renderer_n64_texture_v(0.0f, 33U, 32U) == 0.0f);
    CHECK(!pb_renderer_textured_quad(NULL, 0.0f, 0.0f, 8.0f, 8.0f,
                                     8, 8, 8, 8));
    CHECK(!pb_renderer_textured_quad(&quad, 0.0f, 0.0f, 0.0f, 8.0f,
                                     8, 8, 8, 8));
    CHECK(!pb_renderer_textured_quad(&quad, 0.0f, 0.0f, 8.0f, 8.0f,
                                     12, 8, 8, 8));
    CHECK(!pb_renderer_textured_quad(&quad, 0.0f, 0.0f, 8.0f, 8.0f,
                                     8, 8, 9, 8));
    return true;
}

static bool nearly_equal(float left, float right) {
    const float difference = left - right;
    return difference > -0.0001f && difference < 0.0001f;
}

static bool test_fast3d_fog_lut(void) {
    float values[PB_RENDER_FOG_LUT_VALUES];
    CHECK(!pb_renderer_fast3d_fog_lut(NULL, 0, 0));
    CHECK(pb_renderer_fast3d_fog_lut(values, 0, 0));
    for (size_t index = 0; index < 128U; index++) {
        CHECK(nearly_equal(values[index], 1.0f));
        CHECK(nearly_equal(values[index + 128U], 0.0f));
    }

    /* factor=clamp((ndcZ*128+128)/255): visibility runs from 1 at
     * NDC -1 to 0 at NDC +1.  The second half stores segment deltas. */
    CHECK(pb_renderer_fast3d_fog_lut(values, 128, 128));
    CHECK(nearly_equal(values[0], 1.0f));
    CHECK(values[64] > 0.49f && values[64] < 0.51f);
    CHECK(values[127] > 0.0f && values[127] < 0.02f);
    CHECK(nearly_equal(values[127] + values[255], 0.0f));
    for (size_t index = 128U; index < PB_RENDER_FOG_LUT_VALUES; index++) {
        CHECK(values[index] <= 0.0f);
    }
    return true;
}

static PBRenderPipeline valid_pipeline(void) {
    const PBRenderPipeline pipeline = {
        .cull_mode = PB_CULL_BACK_CCW,
        .depth_test_enabled = true,
        .depth_write_enabled = true,
        .depth_function = PB_COMPARE_GREATER,
        .blend_mode = PB_BLEND_ALPHA,
        .alpha_test_enabled = true,
        .alpha_function = PB_COMPARE_GREATER,
        .alpha_reference = 0,
        .min_filter = PB_FILTER_NEAREST,
        .mag_filter = PB_FILTER_LINEAR,
        .wrap_s = PB_WRAP_REPEAT,
        .wrap_t = PB_WRAP_CLAMP_TO_EDGE,
    };
    return pipeline;
}

static bool test_pipeline_and_cache(void) {
    PBRenderPipeline pipeline = valid_pipeline();
    CHECK(pb_renderer_pipeline_is_valid(&pipeline));
    pipeline.depth_test_enabled = false;
    CHECK(pb_renderer_pipeline_is_valid(&pipeline));
    pipeline.depth_write_enabled = false;
    CHECK(pb_renderer_pipeline_is_valid(&pipeline));
    pipeline = valid_pipeline();
    pipeline.alpha_function = (PBCompareFunction)PB_COMPARE_COUNT;
    CHECK(!pb_renderer_pipeline_is_valid(&pipeline));
    pipeline = valid_pipeline();
    pipeline.wrap_s = (PBTextureWrap)PB_WRAP_COUNT;
    CHECK(!pb_renderer_pipeline_is_valid(&pipeline));
    CHECK(!pb_renderer_pipeline_is_valid(NULL));

    PBRenderStateCache cache;
    pb_renderer_state_cache_init(&cache);
    CHECK(cache.changes == 0);
    CHECK(cache.deduplicated == 0);
    CHECK(cache.rejected == 0);

    const PBViewport full = {
        .x = 0,
        .y = 0,
        .width = PB_RENDER_TOP_WIDTH,
        .height = PB_RENDER_TOP_HEIGHT,
    };
    CHECK(pb_renderer_bind_viewport(&cache, &full) == PB_BIND_CHANGED);
    CHECK(pb_renderer_bind_viewport(&cache, &full) == PB_BIND_UNCHANGED);
    CHECK(cache.viewport_bound);

    pipeline = valid_pipeline();
    CHECK(pb_renderer_bind_pipeline(&cache, &pipeline) == PB_BIND_CHANGED);
    CHECK(pb_renderer_bind_pipeline(&cache, &pipeline) == PB_BIND_UNCHANGED);
    CHECK(cache.pipeline_bound);

    pipeline.wrap_t = (PBTextureWrap)PB_WRAP_COUNT;
    CHECK(pb_renderer_bind_pipeline(&cache, &pipeline) == PB_BIND_REJECTED);
    CHECK(cache.changes == 2);
    CHECK(cache.deduplicated == 2);
    CHECK(cache.rejected == 1);
    CHECK(pb_renderer_bind_pipeline(NULL, &pipeline) == PB_BIND_REJECTED);
    return true;
}

static bool test_buffer_contract_and_status(void) {
    size_t bytes;
    CHECK(pb_renderer_vertex_buffer_size(36, 9, &bytes));
    CHECK(bytes == 324);
    CHECK(!pb_renderer_vertex_buffer_size(0, 9, &bytes));
    CHECK(!pb_renderer_vertex_buffer_size(36, 0, &bytes));
    CHECK(!pb_renderer_vertex_buffer_size(SIZE_MAX, 2, &bytes));
    CHECK(!pb_renderer_vertex_buffer_size(36, 9, NULL));

    size_t first_vertex = SIZE_MAX;
    size_t next_used_vertices = SIZE_MAX;
    CHECK(pb_renderer_stream_reserve(1152, 0, 6, &first_vertex,
                                     &next_used_vertices));
    CHECK(first_vertex == 0);
    CHECK(next_used_vertices == 6);
    CHECK(pb_renderer_stream_reserve(1152, next_used_vertices, 3,
                                     &first_vertex,
                                     &next_used_vertices));
    CHECK(first_vertex == 6);
    CHECK(next_used_vertices == 9);
    CHECK(pb_renderer_stream_reserve(1152, 1151, 1, &first_vertex,
                                     &next_used_vertices));
    CHECK(first_vertex == 1151);
    CHECK(next_used_vertices == 1152);
    CHECK(!pb_renderer_stream_reserve(1152, 1152, 1, &first_vertex,
                                      &next_used_vertices));
    CHECK(!pb_renderer_stream_reserve(1152, 1153, 1, &first_vertex,
                                      &next_used_vertices));
    CHECK(!pb_renderer_stream_reserve(1152, 0, 0, &first_vertex,
                                      &next_used_vertices));
    CHECK(!pb_renderer_stream_reserve(1152, 0, 1, NULL,
                                      &next_used_vertices));
    CHECK(!pb_renderer_stream_reserve(1152, 0, 1, &first_vertex, NULL));

    CHECK(strcmp(pb_renderer_init_result_name(PB_RENDERER_INIT_OK),
                 "ready") == 0);
    CHECK(strcmp(pb_renderer_init_result_name(PB_RENDERER_INIT_SHADER),
                 "shader load failed") == 0);
    CHECK(strcmp(pb_renderer_init_result_name((PBRendererInitResult)999),
                 "unknown") == 0);
    return true;
}

static bool test_ortho_depth_slack(void) {
    /* PICA clip Z is [-w, 0]. N64 near (-w) maps to -w; N64 far (+w) to 0.
     * Vertices inside the camera frustum stay inside; a second 0..1 ortho
     * window is not applied to Z. */
    const float w = 4.0f;
    const float near_z = pb_renderer_n64_to_pica_clip_z(-w, w);
    const float far_z = pb_renderer_n64_to_pica_clip_z(w, w);
    const float inside_z = pb_renderer_n64_to_pica_clip_z(0.0f, w);
    const float closer_than_near =
        pb_renderer_n64_to_pica_clip_z(-3.0f * w, w);
    CHECK(near_z == -w);
    CHECK(far_z == 0.0f);
    CHECK(inside_z == -0.5f * w);
    CHECK(near_z >= -w && near_z <= 0.0f);
    CHECK(far_z >= -w && far_z <= 0.0f);
    CHECK(inside_z >= -w && inside_z <= 0.0f);
    CHECK(closer_than_near < -w);
    CHECK(PB_RENDER_ORTHO_Z_IDENTITY_ZZ == 1.0f);
    CHECK(PB_RENDER_ORTHO_Z_IDENTITY_ZW == 0.0f);
    const float sprite_near =
        pb_renderer_n64_to_pica_clip_z(
            pb_renderer_screen_depth_to_n64_clip_z(1.0f, 1.0f), 1.0f);
    const float sprite_far =
        pb_renderer_n64_to_pica_clip_z(
            pb_renderer_screen_depth_to_n64_clip_z(0.0f, 1.0f), 1.0f);
    CHECK(sprite_near == -1.0f);
    CHECK(sprite_far == 0.0f);
    CHECK(pb_renderer_screen_depth_to_pica_z(0.45f, 1.0f) == -0.45f);
    CHECK(pb_renderer_screen_depth_to_pica_z(0.55f, 1.0f) == -0.55f);
    CHECK(pb_renderer_screen_depth_to_pica_z(1.0f, 2.0f) == -2.0f);
    CHECK(pb_renderer_clip_w_inside(1.0f));
    CHECK(pb_renderer_clip_w_inside(0.02f));
    CHECK(!pb_renderer_clip_w_inside(0.0f));
    CHECK(!pb_renderer_clip_w_inside(-4.0f));
    CHECK(pb_renderer_clip_w_edge_t(-4.0f, 4.0f) > 0.0f);
    CHECK(pb_renderer_clip_w_edge_t(-4.0f, 4.0f) < 1.0f);
    {
        const float t = pb_renderer_clip_w_edge_t(-1.0f, 1.0f);
        const float w = -1.0f + (1.0f - (-1.0f)) * t;
        CHECK(w > PB_RENDER_CLIP_W_EPS - 0.0001f);
        CHECK(w < PB_RENDER_CLIP_W_EPS + 0.0001f);
    }
    {
        /* A vertex behind the eye must not survive N64 frustum clipping. */
        const PBClipVertex behind[3] = {
            { 0.0f, 0.0f, 0.0f, -4.0f },
            { 1.0f, 0.0f, 0.0f, -2.0f },
            { 0.0f, 1.0f, 0.0f, -2.0f },
        };
        PBClipVertex out[PB_RENDER_CLIP_MAX_VERTS];
        CHECK(pb_renderer_clip_n64_triangle(behind, out) == 0U);
    }
    {
        /* One vertex behind the eye becomes a clipped polygon still inside. */
        const PBClipVertex crossing[3] = {
            { 0.0f, 0.0f, 0.0f, -2.0f },
            { 0.5f, 0.0f, 0.0f, 2.0f },
            { 0.0f, 0.5f, 0.0f, 2.0f },
        };
        PBClipVertex out[PB_RENDER_CLIP_MAX_VERTS];
        const size_t count = pb_renderer_clip_n64_triangle(crossing, out);
        CHECK(count >= 3U);
        CHECK(count <= PB_RENDER_CLIP_MAX_VERTS);
        for (size_t index = 0; index < count; index++) {
            CHECK(out[index].w > PB_RENDER_CLIP_W_EPS - 0.0001f);
            CHECK(out[index].x <= out[index].w + 0.0001f);
            CHECK(-out[index].x <= out[index].w + 0.0001f);
            CHECK(out[index].y <= out[index].w + 0.0001f);
            CHECK(-out[index].y <= out[index].w + 0.0001f);
            CHECK(out[index].z <= out[index].w + 0.0001f);
            CHECK(-out[index].z <= out[index].w + 0.0001f);
        }
    }
    {
        /* A close billboard vertex (small positive W) must remain inside. */
        const PBClipVertex near_sprite[3] = {
            { 0.0f, 0.0f, 0.0f, 0.02f },
            { 0.01f, 0.0f, 0.0f, 1.0f },
            { 0.0f, 0.01f, 0.0f, 1.0f },
        };
        PBClipVertex out[PB_RENDER_CLIP_MAX_VERTS];
        CHECK(pb_renderer_clip_n64_triangle(near_sprite, out) >= 3U);
    }
    {
        /* Off-axis XY that would span the screen is clipped to |x|,|y| <= w. */
        const PBClipVertex huge[3] = {
            { 40.0f, 0.0f, 0.0f, 1.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f },
            { 0.0f, 40.0f, 0.0f, 1.0f },
        };
        PBClipVertex out[PB_RENDER_CLIP_MAX_VERTS];
        const size_t count = pb_renderer_clip_n64_triangle(huge, out);
        CHECK(count >= 3U);
        for (size_t index = 0; index < count; index++) {
            CHECK(out[index].x <= out[index].w + 0.001f);
            CHECK(out[index].y <= out[index].w + 0.001f);
        }
    }
    {
        const float backFacing = pb_renderer_clip_face_cross(
            0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f);
        CHECK(backFacing < 0.0f);
        CHECK(pb_renderer_clip_keep_face(backFacing, -1));
        CHECK(!pb_renderer_clip_keep_face(backFacing, 1));
        CHECK(pb_renderer_clip_keep_face(backFacing, 0));
    }
    return true;
}

int main(void) {
    if (!test_texture_formats() || !test_texture_swizzle() ||
        !test_viewport_rotation() || !test_textured_quad_orientation() ||
        !test_fast3d_fog_lut() || !test_pipeline_and_cache() ||
        !test_buffer_contract_and_status() || !test_ortho_depth_slack()) {
        return EXIT_FAILURE;
    }

    printf("M9 renderer contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
