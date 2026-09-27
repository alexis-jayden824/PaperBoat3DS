#include "pb3ds/platform.h"
#include "pb3ds/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks_run;

#define CHECK(expression)                                                      \
    do {                                                                       \
        checks_run++;                                                          \
        if (!(expression)) {                                                   \
            fprintf(stderr, "M11 gfx check failed at %s:%d: %s\n",             \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool nearly(float value, float expected) {
    float delta = value - expected;
    if (delta < 0.0f) {
        delta = -delta;
    }
    return delta < 0.01f;
}

static bool test_clip_invert_pillars_and_submit(void) {
    PBBootstrap bootstrap;
    PBCompatState compat;
    PBGfxDiag diag;
    PBGfxVertex inside;
    PBGfxVertex outside;
    PBGfxVertex origin;
    PBGfxTexDesc tex;
    unsigned char pixels[16];
    float top_x = 0.0f;
    float top_y = 0.0f;
    unsigned kept;
    unsigned int dummy_dl[2] = {0xDF000000U, 0U};

    CHECK(strcmp(PB3DS_VERSION, "0.11.0-m11") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M11") != NULL);
    CHECK(PB_GFX_SOURCE_WIDTH == 320U);
    CHECK(PB_GFX_SOURCE_HEIGHT == 240U);
    CHECK(PB_GFX_PILLAR_PX == 40U);
    CHECK(PB_GFX_TOP_WIDTH == 400U);
    CHECK(PB_GFX_PILLAR_PX * 2U + PB_GFX_SOURCE_WIDTH == PB_GFX_TOP_WIDTH);
    CHECK(pb_gfx_invert_y(1.0f) == -1.0f);
    CHECK(pb_gfx_invert_y(-0.5f) == 0.5f);

    memset(&inside, 0, sizeof(inside));
    inside.x = 0.25f;
    inside.y = -0.5f;
    inside.z = 0.0f;
    inside.w = 1.0f;
    memset(&outside, 0, sizeof(outside));
    outside.x = 2.0f;
    outside.w = 1.0f;
    memset(&origin, 0, sizeof(origin));
    origin.w = 1.0f;
    CHECK(pb_gfx_vertex_in_clip(&inside));
    CHECK(!pb_gfx_vertex_in_clip(&outside));
    CHECK(pb_gfx_vertex_in_clip(&origin));
    CHECK(pb_gfx_project_vertex(&origin, &top_x, &top_y));
    CHECK(nearly(top_x, 200.0f));
    CHECK(nearly(top_y, 120.0f));
    pb_gfx_map_source_to_top(0.0f, 0.0f, &top_x, &top_y);
    CHECK(nearly(top_x, 40.0f));
    CHECK(nearly(top_y, 0.0f));
    pb_gfx_map_source_to_top(320.0f, 240.0f, &top_x, &top_y);
    CHECK(nearly(top_x, 360.0f));
    CHECK(nearly(top_y, 240.0f));

    pb_bootstrap_init(&bootstrap);
    CHECK(pb_system_init(&bootstrap));
    CHECK(pb_gfx_ready());
    CHECK(pb_gfx_invert_y_enabled());
    pb_compat_init();
    pb_compat_query(&compat);
    CHECK(compat.gfx == PB_COMPAT_READY);
    CHECK(strcmp(pb_compat_status_name(PB_COMPAT_DEFERRED_M11), "deferred-m11") ==
          0);

    pb_gfx_set_depth_enabled(true);
    kept = pb_gfx_submit_vertices(&inside, 1U);
    CHECK(kept == 1U);
    kept = pb_gfx_submit_vertices(&outside, 1U);
    CHECK(kept == 0U);
    pb_gfx_submit_dl(dummy_dl, sizeof(dummy_dl));
    Graphics_PushFrame(dummy_dl);

    memset(pixels, 0x7F, sizeof(pixels));
    memset(&tex, 0, sizeof(tex));
    tex.width = 2U;
    tex.height = 2U;
    tex.format = PB_GFX_FMT_RGBA8888;
    tex.slot = 0U;
    CHECK(pb_gfx_tex_upload(&tex, pixels, sizeof(pixels)));
    tex.width = 3U;
    CHECK(!pb_gfx_tex_upload(&tex, pixels, sizeof(pixels)));

    pb_gfx_clear_top(26U, 51U, 68U);
    pb_gfx_present();
    pb_gfx_query(&diag);
    CHECK(diag.dl_submits == 2U);
    CHECK(diag.last_dl == dummy_dl);
    CHECK(diag.last_dl_bytes == 0U);
    CHECK(diag.verts_in == 2U);
    CHECK(diag.verts_kept == 1U);
    CHECK(diag.verts_clipped_out == 1U);
    CHECK(diag.tex_uploads == 1U);
    CHECK(diag.tex_bytes == 16U);
    CHECK(diag.depth_enabled);
    CHECK(diag.invert_y);
    CHECK(diag.frames >= 1U);
    CHECK(!diag.pica_ready);
    CHECK(diag.last_clear_rgba == 0x1A3344FFU);

    pb_system_shutdown(&bootstrap);
    CHECK(!pb_gfx_ready());
    return true;
}

int main(void) {
    if (!test_clip_invert_pillars_and_submit()) {
        return EXIT_FAILURE;
    }
    printf("M11 gfx contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
