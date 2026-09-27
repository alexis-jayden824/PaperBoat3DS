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
            fprintf(stderr, "M13 runtime check failed at %s:%d: %s\n",         \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool test_fast3d_and_tex(void) {
    PBGfx dl[4];
    PBVtx verts[3];
    PBF3dDiag f3d;
    PBTexCacheDiag tex;
    uint8_t ci[4];
    uint16_t tlut[4];
    uint8_t rgba[16];

    CHECK(strcmp(PB3DS_VERSION, "0.13.0-m13") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M13") != NULL);
    CHECK(PB_F3D_G_VTX == 0x01U);
    CHECK(PB_F3D_G_TRI1 == 0x05U);
    CHECK(PB_F3D_G_ENDDL == 0xdfU);
    CHECK(PB_F3D_G_SETTIMG == 0xfdU);
    CHECK(PB_F3D_G_SETCOMBINE == 0xfcU);

    memset(verts, 0, sizeof(verts));
    verts[0].ob[0] = 0;
    verts[0].ob[1] = 0;
    verts[0].cn[3] = 255;
    verts[1].ob[0] = 1;
    verts[1].ob[1] = 0;
    verts[1].cn[3] = 255;
    verts[2].ob[0] = 0;
    verts[2].ob[1] = 1;
    verts[2].cn[3] = 255;

    pb_f3d_reset();
    pb_f3d_set_segment(1, verts);
    memset(dl, 0, sizeof(dl));
    dl[0].w0 = 0x01003006U;
    dl[0].w1 = 1U;
    dl[1].w0 = 0x05000204U;
    dl[1].w1 = 0U;
    dl[2].w0 = 0xDF000000U;
    pb_f3d_execute(dl, sizeof(dl));
    pb_f3d_query(&f3d);
    CHECK(f3d.triangles == 1U);
    CHECK(f3d.vtx == 3U);
    CHECK(f3d.invert_y_applied);
    CHECK(f3d.depth_test);
    CHECK(f3d.unsupported == 0U);

    dl[0].w0 = 0xCC000000U;
    dl[0].w1 = 0U;
    dl[1].w0 = 0xDF000000U;
    pb_f3d_execute(dl, sizeof(PBGfx) * 2U);
    pb_f3d_query(&f3d);
    CHECK(f3d.unsupported >= 1U);
    CHECK(f3d.last_unsupported == 0xCCU);
    CHECK(strcmp(pb_f3d_opcode_name(PB_F3D_G_TRI1), "G_TRI1") == 0);

    tlut[0] = 0xF801U;
    tlut[1] = 0x07C1U;
    tlut[2] = 0x003FU;
    tlut[3] = 0xFFFFU;
    ci[0] = 0;
    ci[1] = 1;
    ci[2] = 2;
    ci[3] = 3;
    CHECK(pb_tex_decode_rgba8888(PB_F3D_FMT_CI, PB_F3D_SIZ_8B, 2, 2, ci,
                                 sizeof(ci), tlut, 4U, rgba, sizeof(rgba)));
    CHECK(rgba[0] > 8U);
    CHECK(rgba[3] == 0xFFU);
    CHECK(pb_tex_decode_rgba8888(PB_F3D_FMT_CI, PB_F3D_SIZ_8B, 2, 2, ci,
                                 sizeof(ci), NULL, 0U, rgba, sizeof(rgba)));
    CHECK(rgba[0] != 0U || rgba[1] != 0U || rgba[2] != 0U);
    pb_tex_query(&tex);
    CHECK(tex.black_prevented >= 1U);

    pb_f3d_reset();
    memset(dl, 0, sizeof(dl));
    dl[0].w0 = 0xED0A0000U;
    dl[0].w1 = 0x00000010U;
    dl[1].w0 = 0xFC120000U;
    dl[1].w1 = 0U;
    dl[2].w0 = 0xDF000000U;
    pb_f3d_execute(dl, sizeof(dl));
    pb_f3d_query(&f3d);
    CHECK(f3d.scissor_clamps >= 1U);
    CHECK(f3d.combiners >= 1U);
    {
        PBTevDiag tev;
        pb_tev_query(&tev);
        CHECK(tev.configures >= 1U);
        CHECK(tev.mode == PB_TEV_MODE_MODULATE ||
              tev.mode == PB_TEV_MODE_FALLBACK_MODULATE);
    }
    return true;
}

static bool test_runtime_gate(void) {
    PBBootstrap bootstrap;
    PBRuntimePlay play;

    pb_bootstrap_init(&bootstrap);
    CHECK(pb_system_init(&bootstrap));
    pb_compat_init();
    pb_runtime_init();
    pb_runtime_play_query(&play);
    CHECK(!play.game_linked);
    CHECK(!play.playing);
    CHECK(!pb_runtime_playing());
    CHECK(pb_runtime_quit_combo(PB_KEY_L | PB_KEY_R | PB_KEY_START));
    CHECK(!pb_runtime_quit_combo(PB_KEY_START));
    pb_runtime_frame(1U);
    pb_runtime_shutdown();
    pb_system_shutdown(&bootstrap);
    return true;
}

int main(void) {
    if (!test_fast3d_and_tex() || !test_runtime_gate()) {
        return EXIT_FAILURE;
    }
    printf("M13 runtime contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
