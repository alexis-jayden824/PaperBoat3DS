#include "pb3ds/gfx_rendering_api_3ds.h"
#include "pb3ds/runtime_resources.h"
#include "pb3ds/title_flow.h"
#include "pb3ds/world_scene.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

static unsigned int checksRun;
static uint8_t firstFramePixels[512U * 256U * 4U];
static uint8_t titleLogoPixels[256U * 128U * 4U];
static uint8_t titlePromptPixels[128U * 32U * 4U];
static uint8_t titleCopyrightPixels[256U * 32U * 4U];
static uint8_t worldMapPixels[8U * 8U * 4U];
static uint8_t worldActorPixels[8U * 8U * 4U];
static PBWorldTriangle worldTriangle;

extern "C" void *ResourceGetDataByName(const char *) { return nullptr; }
extern "C" void *ResourceGetDataByCrc(uint64_t) { return nullptr; }
extern "C" const char *ResourceGetNameByCrc(uint64_t) { return nullptr; }
extern "C" size_t pb_runtime_resource_payload_size(const char *) { return 0U; }
extern "C" uint32_t pb_runtime_resource_texture_type(const char *) {
    return PB_RESOURCE_TEXTURE_ERROR;
}
extern "C" uint16_t ResourceGetTexWidthByName(const char *) { return 0U; }
extern "C" uint16_t ResourceGetTexHeightByName(const char *) { return 0U; }
extern "C" uint8_t GameEngine_OTRSigCheck(const char *) { return 0U; }

#define CHECK(expression)                                                    \
    do {                                                                     \
        checksRun++;                                                         \
        if (!(expression)) {                                                 \
            std::fprintf(stderr,                                             \
                         "M13 graphics contract check failed at %s:%d: %s\n", \
                         __FILE__, __LINE__, #expression);                   \
            return false;                                                    \
        }                                                                    \
    } while (0)

static_assert(std::is_base_of<Fast::GfxRenderingAPI,
                              PB3DS::GfxRenderingAPI3DS>::value,
              "3DS backend must implement the pinned libultraship API");
static_assert(!std::is_abstract<PB3DS::GfxRenderingAPI3DS>::value,
              "every pure virtual graphics method must be implemented");

static PBTitleAssets makeTitleAssets() {
    PBTitleAssets assets = {};
    assets.result = PB_TITLE_ASSETS_READY;
    assets.logo = { titleLogoPixels, sizeof(titleLogoPixels),
                    PB_TITLE_LOGO_WIDTH, PB_TITLE_LOGO_HEIGHT, 256, 128,
                    PB_RESOURCE_TEXTURE_RGBA32 };
    assets.prompt = { titlePromptPixels, sizeof(titlePromptPixels),
                      PB_TITLE_PROMPT_WIDTH, PB_TITLE_PROMPT_HEIGHT, 128, 32,
                      PB_RESOURCE_TEXTURE_IA8 };
    assets.copyright = {
        titleCopyrightPixels, sizeof(titleCopyrightPixels),
        PB_TITLE_COPYRIGHT_WIDTH, PB_TITLE_COPYRIGHT_HEIGHT, 256, 32,
        PB_RESOURCE_TEXTURE_IA8
    };
    return assets;
}

static PBWorldScene makeWorldScene() {
    PBWorldScene scene = {};
    scene.result = PB_WORLD_SCENE_READY;
    std::strcpy(scene.map_id, "mac_00");
    scene.entry_id = 6U;
    scene.triangles = &worldTriangle;
    scene.stats.triangles = 1U;
    scene.stats.textures = 1U;
    scene.texture_count = 1U;
    scene.background = { firstFramePixels, sizeof(firstFramePixels),
                         296U, 200U, 512U, 256U,
                         PB_RESOURCE_TEXTURE_CI8 };
    scene.textures[0].decoded = {
        worldMapPixels, sizeof(worldMapPixels), 8U, 8U, 8U, 8U,
        PB_RESOURCE_TEXTURE_I8
    };
    for (size_t index = 0U; index < PB_WORLD_PLAYER_FRAME_COUNT; index++) {
        scene.player_frames[index] = {
            worldActorPixels, sizeof(worldActorPixels), 8U, 8U, 8U, 8U,
            PB_RESOURCE_TEXTURE_CI4
        };
    }
    scene.star_piece = {
        worldActorPixels, sizeof(worldActorPixels), 8U, 8U, 8U, 8U,
        PB_RESOURCE_TEXTURE_CI4
    };
    scene.player_position = { 0.0f, 0.0f, 0.0f };
    scene.camera_target = { 0.0f, 35.0f, 0.0f };
    scene.star_piece_active = true;
    scene.transition_state = PB_WORLD_TRANSITION_NONE;
    worldTriangle = {};
    worldTriangle.texture_index = 0;
    worldTriangle.render_class = PB_WORLD_RENDER_OPAQUE;
    worldTriangle.vertices[0] = {
        { -100.0f, 0.0f, -100.0f }, 0.0f, 0.0f,
        255U, 255U, 255U, 255U
    };
    worldTriangle.vertices[1] = {
        { 100.0f, 0.0f, -100.0f }, 1.0f, 0.0f,
        255U, 255U, 255U, 255U
    };
    worldTriangle.vertices[2] = {
        { 0.0f, 0.0f, 100.0f }, 0.5f, 1.0f,
        255U, 255U, 255U, 255U
    };
    return scene;
}

static bool testRuntimeDisplayList(PBGfxApi3DS *api) {
    struct TestVertex {
        int16_t position[3];
        uint16_t flag;
        int16_t texture[2];
        uint8_t color[4];
    };
    static_assert(sizeof(TestVertex) == 16U, "test vertex ABI changed");
    TestVertex vertices[3] = {
        { { -1, -1, 0 }, 0U, { 0, 0 }, { 255U, 0U, 0U, 255U } },
        { { 1, -1, 0 }, 0U, { 0, 0 }, { 0U, 255U, 0U, 255U } },
        { { 0, 1, 0 }, 0U, { 0, 0 }, { 0U, 0U, 255U, 255U } },
    };
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0x01003006),
                     reinterpret_cast<uintptr_t>(vertices) } },
        { .words = { UINT32_C(0x05000204), 0U } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    const PBGfxBridgeStats before = *pb_gfx_api_3ds_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBGfxBridgeStats *after = pb_gfx_api_3ds_stats(api);
    CHECK(after->frames_presented == before.frames_presented + 1U);
    CHECK(after->draw_calls == before.draw_calls + 1U);
    CHECK(after->triangles == before.triangles + 1U);
    CHECK(after->rejected_commands == before.rejected_commands);
    const PBRuntimeGfxStats *runtimeStats =
        pb_gfx_api_3ds_runtime_stats(api);
    CHECK(runtimeStats != nullptr);
    CHECK(runtimeStats->frames_rendered == 1U);
    CHECK(runtimeStats->commands == 3U);
    CHECK(runtimeStats->unknown_commands == 0U);
    CHECK(runtimeStats->missing_resources == 0U);
    CHECK(runtimeStats->malformed_lists == 0U);
    return true;
}

static bool testRuntimeMovememViewportUsesBottomLeftOrigin(PBGfxApi3DS *api) {
    /* An N64 Vp_t as delivered by G_MOVEMEM/G_MV_VIEWPORT: scale/translate
     * are int16_t[4], quarter-pixel translate, half-pixel*2 scale. This uses
     * a deliberately asymmetric partial-height viewport, where the old and
     * corrected origin conversions do not coincidentally agree. */
    struct N64ViewportTest {
        int16_t scale[4];
        int16_t translate[4];
    };
    static const N64ViewportTest viewport = {
        { 640, 200, 0, 0 },
        { 640, 200, 0, 0 },
    };
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0xDC000008),
                     reinterpret_cast<uintptr_t>(&viewport) } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBRuntimeGfxStats *after = pb_gfx_api_3ds_runtime_stats(api);
    CHECK(after != nullptr);
    CHECK(after->game_viewport_x == 40);
    CHECK(after->game_viewport_y == 140);
    CHECK(after->game_viewport_w == 320U);
    CHECK(after->game_viewport_h == 100U);
    return true;
}

static bool testRuntimeDepthTargetAndCopyRectangle(PBGfxApi3DS *api) {
    static uint8_t texture[8U * 8U * 2U] = {};
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0xFE000000), UINT32_C(0x12340000) } },
        { .words = { UINT32_C(0xFF100007), UINT32_C(0x12340000) } },
        { .words = { UINT32_C(0xF7000000), UINT32_C(0xFFFFFFFF) } },
        { .words = { UINT32_C(0xF6008008), 0U } },
        { .words = { UINT32_C(0xFF100007), UINT32_C(0x56780000) } },
        { .words = { UINT32_C(0xF6008008), 0U } },
        { .words = { UINT32_C(0xEF200000), 0U } },
        { .words = { UINT32_C(0xFD100000),
                     reinterpret_cast<uintptr_t>(texture) } },
        { .words = { UINT32_C(0xF5100000), 0U } },
        { .words = { UINT32_C(0xF3000000), 0U } },
        { .words = { UINT32_C(0xF2000000), UINT32_C(0x0001C01C) } },
        { .words = { UINT32_C(0xE4020020), 0U } },
        { .words = { UINT32_C(0xE1000000), 0U } },
        { .words = { UINT32_C(0xF1000000), UINT32_C(0x10000400) } },
        { .words = { UINT32_C(0x3C000000), UINT32_C(0x00080008) } },
        { .words = { 0U, 0U } },
        { .words = { UINT32_C(0x00200020), UINT32_C(0x00080008) } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    const PBGfxBridgeStats beforeBridge = *pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats beforeRuntime =
        *pb_gfx_api_3ds_runtime_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBGfxBridgeStats *afterBridge = pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats *afterRuntime =
        pb_gfx_api_3ds_runtime_stats(api);
    CHECK(afterBridge->draw_calls == beforeBridge.draw_calls + 3U);
    CHECK(afterBridge->triangles == beforeBridge.triangles + 6U);
    CHECK(afterRuntime->depth_target_clears ==
          beforeRuntime.depth_target_clears + 1U);
    CHECK(afterRuntime->copy_rectangles ==
          beforeRuntime.copy_rectangles + 1U);
    CHECK(afterRuntime->commands_last_frame ==
          sizeof(displayList) / sizeof(displayList[0]));
    CHECK(afterRuntime->commands_peak_frame >=
          afterRuntime->commands_last_frame);
    CHECK(afterRuntime->texture_fallbacks ==
          beforeRuntime.texture_fallbacks);
    return true;
}

static bool testRuntimeLoadTileSubregion(PBGfxApi3DS *api) {
    static uint8_t texture[16U * 16U * 2U] = {};
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0xFD10000F),
                     reinterpret_cast<uintptr_t>(texture) } },
        { .words = { UINT32_C(0xF5100400), UINT32_C(0x07000000) } },
        { .words = { UINT32_C(0xF4010010), UINT32_C(0x0702C02C) } },
        { .words = { UINT32_C(0xF5100400), 0U } },
        { .words = { UINT32_C(0xF2010010), UINT32_C(0x0002C02C) } },
        { .words = { UINT32_C(0xEF200000), 0U } },
        { .words = { UINT32_C(0xE4020020), 0U } },
        { .words = { UINT32_C(0xE1000000), UINT32_C(0x00800080) } },
        { .words = { UINT32_C(0xF1000000), UINT32_C(0x10000400) } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    const PBGfxBridgeStats beforeBridge = *pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats beforeRuntime =
        *pb_gfx_api_3ds_runtime_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBGfxBridgeStats *afterBridge = pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats *afterRuntime =
        pb_gfx_api_3ds_runtime_stats(api);
    CHECK(afterBridge->draw_calls == beforeBridge.draw_calls + 1U);
    CHECK(afterBridge->triangles == beforeBridge.triangles + 2U);
    CHECK(afterBridge->textures_live == beforeBridge.textures_live + 1U);
    CHECK(afterBridge->texture_bytes == beforeBridge.texture_bytes + 256U);
    CHECK(afterRuntime->texture_fallbacks ==
          beforeRuntime.texture_fallbacks);
    return true;
}

static bool testRuntimeSplitCi8Palette(PBGfxApi3DS *api) {
    static uint8_t texture[8U * 8U];
    static uint8_t paletteLow[128U * 2U];
    static uint8_t paletteHigh[128U * 2U];
    std::memset(texture, 200, sizeof(texture));
    for (size_t entry = 0U; entry < 128U; entry++) {
        paletteLow[entry * 2U] = 0xFFU;
        paletteLow[entry * 2U + 1U] = 0xFFU;
        paletteHigh[entry * 2U] = 0x07U;
        paletteHigh[entry * 2U + 1U] = 0xC1U;
    }
    /* CI8 TLUTs may be loaded as two unrelated 128-entry buffers. The runtime
     * must stage both halves in TMEM instead of reading 512 bytes from the
     * first pointer when a texel selects index 128..255. */
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0xFD10007F),
                     reinterpret_cast<uintptr_t>(paletteLow) } },
        { .words = { UINT32_C(0xF5100100), UINT32_C(0x07000000) } },
        { .words = { UINT32_C(0xF0000000), UINT32_C(0x071FC000) } },
        { .words = { UINT32_C(0xFD10007F),
                     reinterpret_cast<uintptr_t>(paletteHigh) } },
        { .words = { UINT32_C(0xF5100180), UINT32_C(0x07000000) } },
        { .words = { UINT32_C(0xF0000000), UINT32_C(0x071FC000) } },
        { .words = { UINT32_C(0xFD480007),
                     reinterpret_cast<uintptr_t>(texture) } },
        { .words = { UINT32_C(0xF5480000), UINT32_C(0x07000000) } },
        { .words = { UINT32_C(0xF3000000), UINT32_C(0x0703F000) } },
        { .words = { UINT32_C(0xF5480200), 0U } },
        { .words = { UINT32_C(0xF2000000), UINT32_C(0x0001C01C) } },
        { .words = { UINT32_C(0xEF200000), 0U } },
        { .words = { UINT32_C(0xFCFFFFFF), UINT32_C(0xFFFCF33C) } },
        { .words = { UINT32_C(0xE4020020), 0U } },
        { .words = { UINT32_C(0xE1000000), 0U } },
        { .words = { UINT32_C(0xF1000000), UINT32_C(0x04000400) } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    const PBGfxBridgeStats beforeBridge = *pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats beforeRuntime =
        *pb_gfx_api_3ds_runtime_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBGfxBridgeStats *afterBridge = pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats *afterRuntime =
        pb_gfx_api_3ds_runtime_stats(api);
    CHECK(afterBridge->draw_calls == beforeBridge.draw_calls + 1U);
    CHECK(afterBridge->textures_live == beforeBridge.textures_live + 1U);
    CHECK(afterBridge->texture_bytes == beforeBridge.texture_bytes + 256U);
    CHECK(afterRuntime->texture_fallbacks ==
          beforeRuntime.texture_fallbacks);
    return true;
}

static bool testRuntimeOneCycleUsesPaperBoatCombiner(PBGfxApi3DS *api) {
    static uint8_t texture[8U * 8U * 2U] = {};
    /* Cycle 0 is TEXEL0; cycle 1 is SHADE. PaperBoat/Fast selects cycle 0
     * for one-cycle rendering, so this rectangle must upload a texture. */
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0xFD100007),
                     reinterpret_cast<uintptr_t>(texture) } },
        { .words = { UINT32_C(0xF5100000), 0U } },
        { .words = { UINT32_C(0xF3000000), 0U } },
        { .words = { UINT32_C(0xF2000000), UINT32_C(0x0001C01C) } },
        { .words = { UINT32_C(0xFCFFFFFF), UINT32_C(0xFFFCF33C) } },
        { .words = { UINT32_C(0xE4020020), 0U } },
        { .words = { UINT32_C(0xE1000000), 0U } },
        { .words = { UINT32_C(0xF1000000), UINT32_C(0x04000400) } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    const PBGfxBridgeStats beforeBridge = *pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats beforeRuntime =
        *pb_gfx_api_3ds_runtime_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBGfxBridgeStats *afterBridge = pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats *afterRuntime =
        pb_gfx_api_3ds_runtime_stats(api);
    CHECK(afterBridge->draw_calls == beforeBridge.draw_calls + 1U);
    CHECK(afterBridge->triangles == beforeBridge.triangles + 2U);
    CHECK(afterBridge->textures_live == beforeBridge.textures_live + 1U);
    CHECK(afterRuntime->texture_fallbacks ==
          beforeRuntime.texture_fallbacks);
    CHECK(afterRuntime->semantic_combiner_batches ==
          beforeRuntime.semantic_combiner_batches + 1U);
    CHECK(afterRuntime->legacy_combiner_fallbacks ==
          beforeRuntime.legacy_combiner_fallbacks);
    return true;
}

static bool testRuntimeDepthFogUsesSemanticCombiner(PBGfxApi3DS *api) {
    static uint8_t texture[8U * 8U * 2U] = {};
    /* The fog blender used to reject every G_FOG draw from the semantic
     * compiler.  This is a normal textured one-cycle material plus the exact
     * Fast3D depth-fog state that must now reach PICA's fog LUT. */
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0xD9000000), UINT32_C(0x00010000) } },
        { .words = { UINT32_C(0xDB080000), UINT32_C(0x00800080) } },
        { .words = { UINT32_C(0xF8000000), UINT32_C(0x406080FF) } },
        { .words = { UINT32_C(0xEF000000), UINT32_C(0xC0000000) } },
        { .words = { UINT32_C(0xFD100007),
                     reinterpret_cast<uintptr_t>(texture) } },
        { .words = { UINT32_C(0xF5100000), 0U } },
        { .words = { UINT32_C(0xF3000000), 0U } },
        { .words = { UINT32_C(0xF2000000), UINT32_C(0x0001C01C) } },
        { .words = { UINT32_C(0xFCFFFFFF), UINT32_C(0xFFFCF33C) } },
        { .words = { UINT32_C(0xE4020020), 0U } },
        { .words = { UINT32_C(0xE1000000), 0U } },
        { .words = { UINT32_C(0xF1000000), UINT32_C(0x04000400) } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    const PBRuntimeGfxStats before = *pb_gfx_api_3ds_runtime_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBRuntimeGfxStats *after = pb_gfx_api_3ds_runtime_stats(api);
    CHECK(after->semantic_combiner_batches ==
          before.semantic_combiner_batches + 1U);
    CHECK(after->semantic_fog_batches == before.semantic_fog_batches + 1U);
    CHECK(after->legacy_combiner_fallbacks ==
          before.legacy_combiner_fallbacks);
    CHECK(after->legacy_fog_fallbacks == before.legacy_fog_fallbacks);

    /* Blend-color fog uses blend RGB with the fog register's alpha as a
     * constant factor.  It is semantic even without the G_FOG geometry bit. */
    const PBRuntimeGfx constantFogDisplayList[] = {
        { .words = { UINT32_C(0xD9000000), 0U } },
        { .words = { UINT32_C(0xF8000000), UINT32_C(0x00000080) } },
        { .words = { UINT32_C(0xF9000000), UINT32_C(0x204060FF) } },
        { .words = { UINT32_C(0xEF000000), UINT32_C(0x80000000) } },
        { .words = { UINT32_C(0xFD100007),
                     reinterpret_cast<uintptr_t>(texture) } },
        { .words = { UINT32_C(0xF5100000), 0U } },
        { .words = { UINT32_C(0xF3000000), 0U } },
        { .words = { UINT32_C(0xF2000000), UINT32_C(0x0001C01C) } },
        { .words = { UINT32_C(0xFCFFFFFF), UINT32_C(0xFFFCF33C) } },
        { .words = { UINT32_C(0xE4020020), 0U } },
        { .words = { UINT32_C(0xE1000000), 0U } },
        { .words = { UINT32_C(0xF1000000), UINT32_C(0x04000400) } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    const PBRuntimeGfxStats beforeConstant =
        *pb_gfx_api_3ds_runtime_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, constantFogDisplayList));
    after = pb_gfx_api_3ds_runtime_stats(api);
    CHECK(after->semantic_combiner_batches ==
          beforeConstant.semantic_combiner_batches + 1U);
    CHECK(after->semantic_fog_batches ==
          beforeConstant.semantic_fog_batches + 1U);
    CHECK(after->legacy_combiner_fallbacks ==
          beforeConstant.legacy_combiner_fallbacks);
    CHECK(after->legacy_fog_fallbacks ==
          beforeConstant.legacy_fog_fallbacks);
    return true;
}

static bool testRuntimeKeyConvertUsesSemanticCombiner(PBGfxApi3DS *api) {
    /* Cycle 0: (SHADE - CENTER) * SCALE + ENVIRONMENT.
     * Cycle 1: (COMBINED - K4) * K5 + PRIMITIVE.
     * This consumes all four key/convert inputs through the pinned Fast3D
     * mapping and deliberately includes a negative signed-nine-bit K5. */
    constexpr uint32_t combineWord0 =
        UINT32_C(0xFC000000) | (UINT32_C(4) << 20U) |
        (UINT32_C(6) << 15U) | (UINT32_C(7) << 12U) |
        (UINT32_C(7) << 9U) | UINT32_C(15);
    constexpr uint32_t combineWord1 =
        (UINT32_C(6) << 28U) | (UINT32_C(7) << 24U) |
        (UINT32_C(7) << 21U) | (UINT32_C(7) << 18U) |
        (UINT32_C(5) << 15U) | (UINT32_C(7) << 12U) |
        (UINT32_C(6) << 9U) | (UINT32_C(3) << 6U) |
        (UINT32_C(7) << 3U) | UINT32_C(6);
    constexpr uint32_t negativeK5 = UINT32_C(0x1E0); /* -32 in s9. */
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0xEB000000), UINT32_C(0x00002080) } },
        { .words = { UINT32_C(0xEA000000), UINT32_C(0x406080A0) } },
        { .words = { UINT32_C(0xEC000000),
                     (UINT32_C(92) << 9U) | negativeK5 } },
        { .words = { UINT32_C(0xFA000000), UINT32_C(0x204060FF) } },
        { .words = { UINT32_C(0xFB000000), UINT32_C(0x8090A0FF) } },
        { .words = { UINT32_C(0xEF100000), 0U } },
        { .words = { combineWord0, combineWord1 } },
        { .words = { UINT32_C(0xE4020020), 0U } },
        { .words = { UINT32_C(0xE1000000), 0U } },
        { .words = { UINT32_C(0xF1000000), UINT32_C(0x04000400) } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    const PBRuntimeGfxStats before = *pb_gfx_api_3ds_runtime_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBRuntimeGfxStats *after = pb_gfx_api_3ds_runtime_stats(api);
    CHECK(after->unknown_commands == before.unknown_commands);
    CHECK(after->semantic_combiner_batches ==
          before.semantic_combiner_batches + 1U);
    CHECK(after->semantic_two_cycle_batches ==
          before.semantic_two_cycle_batches + 1U);
    CHECK(after->semantic_key_convert_batches ==
          before.semantic_key_convert_batches + 1U);
    CHECK(after->legacy_combiner_fallbacks ==
          before.legacy_combiner_fallbacks);
    CHECK(after->legacy_key_convert_fallbacks ==
          before.legacy_key_convert_fallbacks);
    return true;
}

static bool testRuntimeLegacyUnsafeModulateSkipsTexture(PBGfxApi3DS *api) {
    static uint8_t texture[8U * 8U * 2U] = {};
    /* Vertex-alpha fog forces the legacy path. This one-cycle formula is
     * (TEXEL0 - ENVIRONMENT) * SHADE + ENVIRONMENT, which cannot be rebuilt
     * by multiplying the CPU's white-substituted result by TEXEL0. */
    constexpr uint32_t combineWord0 =
        UINT32_C(0xFC000000) | (UINT32_C(1) << 20U) |
        (UINT32_C(4) << 15U);
    constexpr uint32_t combineWord1 =
        (UINT32_C(5) << 28U) | (UINT32_C(5) << 15U);
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0xEF000000), UINT32_C(0xC0000000) } },
        { .words = { UINT32_C(0xFB000000), UINT32_C(0x804020FF) } },
        { .words = { UINT32_C(0xFD100007),
                     reinterpret_cast<uintptr_t>(texture) } },
        { .words = { UINT32_C(0xF5100000), 0U } },
        { .words = { UINT32_C(0xF3000000), 0U } },
        { .words = { UINT32_C(0xF2000000), UINT32_C(0x0001C01C) } },
        { .words = { combineWord0, combineWord1 } },
        { .words = { UINT32_C(0xE4020020), 0U } },
        { .words = { UINT32_C(0xE1000000), 0U } },
        { .words = { UINT32_C(0xF1000000), UINT32_C(0x04000400) } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    const PBGfxBridgeStats beforeBridge = *pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats before = *pb_gfx_api_3ds_runtime_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBGfxBridgeStats *afterBridge = pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats *after = pb_gfx_api_3ds_runtime_stats(api);
    CHECK(after->semantic_combiner_batches ==
          before.semantic_combiner_batches);
    CHECK(after->legacy_combiner_fallbacks ==
          before.legacy_combiner_fallbacks + 1U);
    CHECK(after->legacy_fog_fallbacks ==
          before.legacy_fog_fallbacks + 1U);
    CHECK(after->legacy_unsafe_modulate_batches ==
          before.legacy_unsafe_modulate_batches + 1U);
    CHECK(afterBridge->textures_live == beforeBridge.textures_live);
    CHECK(afterBridge->texture_bytes == beforeBridge.texture_bytes);
    return true;
}

static bool testRuntimeTwoCycleBindsBothTiles(PBGfxApi3DS *api) {
    static uint8_t texture0[8U * 8U * 2U] = {};
    static uint8_t texture1[8U * 8U * 2U] = {};
    /* Cycle 0 replaces with TEXEL0. Cycle 1 multiplies COMBINED by
     * TEXEL0, which Fast3D maps to physical texture unit 1 in that cycle.
     * Render tiles 0 and 1 point at independent TMEM loads. */
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0xFD100007),
                     reinterpret_cast<uintptr_t>(texture0) } },
        { .words = { UINT32_C(0xF5100000), 0U } },
        { .words = { UINT32_C(0xF3000000), 0U } },
        { .words = { UINT32_C(0xF2000000), UINT32_C(0x0001C01C) } },
        { .words = { UINT32_C(0xFD100007),
                     reinterpret_cast<uintptr_t>(texture1) } },
        { .words = { UINT32_C(0xF5100020), UINT32_C(0x01000000) } },
        { .words = { UINT32_C(0xF3000000), UINT32_C(0x01000000) } },
        { .words = { UINT32_C(0xF2000000), UINT32_C(0x0101C01C) } },
        { .words = { UINT32_C(0xEF100000), 0U } },
        { .words = { UINT32_C(0xFCFFFE01), UINT32_C(0xFF04F3FF) } },
        { .words = { UINT32_C(0xE4020020), 0U } },
        { .words = { UINT32_C(0xE1000000), 0U } },
        { .words = { UINT32_C(0xF1000000), UINT32_C(0x04000400) } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    const PBGfxBridgeStats beforeBridge = *pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats beforeRuntime =
        *pb_gfx_api_3ds_runtime_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBGfxBridgeStats *afterBridge = pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats *afterRuntime =
        pb_gfx_api_3ds_runtime_stats(api);
    CHECK(afterBridge->draw_calls == beforeBridge.draw_calls + 1U);
    CHECK(afterBridge->triangles == beforeBridge.triangles + 2U);
    CHECK(afterBridge->textures_live == beforeBridge.textures_live + 2U);
    CHECK(afterBridge->texture_bytes ==
          beforeBridge.texture_bytes + 2U * 8U * 8U * 4U);
    CHECK(afterBridge->rejected_commands == beforeBridge.rejected_commands);
    CHECK(afterRuntime->texture_fallbacks ==
          beforeRuntime.texture_fallbacks);
    CHECK(afterRuntime->semantic_combiner_batches ==
          beforeRuntime.semantic_combiner_batches + 1U);
    CHECK(afterRuntime->semantic_two_cycle_batches ==
          beforeRuntime.semantic_two_cycle_batches + 1U);
    CHECK(afterRuntime->legacy_combiner_fallbacks ==
          beforeRuntime.legacy_combiner_fallbacks);
    return true;
}

static bool testRuntimeTwoCycleUsesBaseTileWithoutLod(PBGfxApi3DS *api) {
    static uint8_t texture[8U * 8U * 2U] = {};
    /* Pinned Fast3D reuses base tiles 2..7 for unit 1 when no LOD path is
     * available. Both TEV texture units must therefore bind this one upload. */
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0xFD100007),
                     reinterpret_cast<uintptr_t>(texture) } },
        { .words = { UINT32_C(0xF5100040), UINT32_C(0x02000000) } },
        { .words = { UINT32_C(0xF3000000), UINT32_C(0x02000000) } },
        { .words = { UINT32_C(0xF2000000), UINT32_C(0x0201C01C) } },
        { .words = { UINT32_C(0xEF100000), 0U } },
        { .words = { UINT32_C(0xFCFFFE01), UINT32_C(0xFF04F3FF) } },
        { .words = { UINT32_C(0xE4020020), UINT32_C(0x02000000) } },
        { .words = { UINT32_C(0xE1000000), 0U } },
        { .words = { UINT32_C(0xF1000000), UINT32_C(0x04000400) } },
        { .words = { UINT32_C(0xDF000000), 0U } },
    };
    const PBGfxBridgeStats beforeBridge = *pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats beforeRuntime =
        *pb_gfx_api_3ds_runtime_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBGfxBridgeStats *afterBridge = pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats *afterRuntime =
        pb_gfx_api_3ds_runtime_stats(api);
    CHECK(afterBridge->draw_calls == beforeBridge.draw_calls + 1U);
    CHECK(afterBridge->triangles == beforeBridge.triangles + 2U);
    CHECK(afterBridge->textures_live == beforeBridge.textures_live + 1U);
    CHECK(afterBridge->texture_bytes ==
          beforeBridge.texture_bytes + 8U * 8U * 4U);
    CHECK(afterBridge->rejected_commands == beforeBridge.rejected_commands);
    CHECK(afterRuntime->texture_fallbacks ==
          beforeRuntime.texture_fallbacks);
    CHECK(afterRuntime->semantic_combiner_batches ==
          beforeRuntime.semantic_combiner_batches + 1U);
    CHECK(afterRuntime->semantic_two_cycle_batches ==
          beforeRuntime.semantic_two_cycle_batches + 1U);
    CHECK(afterRuntime->legacy_combiner_fallbacks ==
          beforeRuntime.legacy_combiner_fallbacks);
    return true;
}

static bool testRuntimeTextureEvictionFrame(PBGfxApi3DS *api) {
    constexpr size_t textureCount = PB_GFX_MAX_TEXTURES;
    constexpr size_t commandsPerTexture = 8U;
    static uint8_t textures[textureCount][8U * 8U * 2U] = {};
    static PBRuntimeGfx displayList[2U +
                                    textureCount * commandsPerTexture + 1U];
    size_t command = 0U;
    displayList[command++].words = { UINT32_C(0xEF200000), 0U };
    displayList[command++].words = {
        UINT32_C(0xFCFFFFFF), UINT32_C(0xFFFCF33C)
    };
    for (size_t texture = 0U; texture < textureCount; texture++) {
        textures[texture][0] = static_cast<uint8_t>(texture);
        displayList[command++].words = {
            UINT32_C(0xFD100007),
            reinterpret_cast<uintptr_t>(textures[texture])
        };
        displayList[command++].words = { UINT32_C(0xF5100000), 0U };
        displayList[command++].words = { UINT32_C(0xF3000000), 0U };
        displayList[command++].words = { UINT32_C(0xF5100000), 0U };
        displayList[command++].words = {
            UINT32_C(0xF2000000), UINT32_C(0x0001C01C)
        };
        displayList[command++].words = { UINT32_C(0xE4020020), 0U };
        displayList[command++].words = { UINT32_C(0xE1000000), 0U };
        displayList[command++].words = {
            UINT32_C(0xF1000000), UINT32_C(0x04000400)
        };
    }
    displayList[command++].words = { UINT32_C(0xDF000000), 0U };
    CHECK(command == sizeof(displayList) / sizeof(displayList[0]));

    const PBGfxBridgeStats beforeBridge = *pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats beforeRuntime =
        *pb_gfx_api_3ds_runtime_stats(api);
    CHECK(pb_gfx_api_3ds_render_display_list(api, displayList));
    const PBGfxBridgeStats *afterBridge = pb_gfx_api_3ds_stats(api);
    const PBRuntimeGfxStats *afterRuntime =
        pb_gfx_api_3ds_runtime_stats(api);
    CHECK(afterBridge->draw_calls ==
          beforeBridge.draw_calls + textureCount);
    CHECK(afterBridge->triangles ==
          beforeBridge.triangles + textureCount * 2U);
    CHECK(afterBridge->rejected_commands == beforeBridge.rejected_commands);
    CHECK(afterRuntime->texture_fallbacks ==
          beforeRuntime.texture_fallbacks);
    CHECK(afterRuntime->texture_evictions >
          beforeRuntime.texture_evictions);
    return true;
}

static bool testExactInterface() {
    PB3DS::GfxRenderingAPI3DS api(nullptr);
    CHECK(std::strcmp(api.GetName(), "PICA200 (citro3d)") == 0);
    CHECK(api.GetMaxTextureSize() == 1024);
    const Fast::GfxClipParameters clip = api.GetClipParameters();
    CHECK(clip.z_is_from_0_to_1);
    CHECK(!clip.invertY);
    CHECK(api.GetTextureFilter() == Fast::FILTER_THREE_POINT);
    api.SetTextureFilter(Fast::FILTER_LINEAR);
    CHECK(api.GetTextureFilter() == Fast::FILTER_LINEAR);

    CHECK(api.PrepareDiagnostic());
    CHECK(api.RenderDiagnostic());
    CHECK(api.RenderDiagnostic());
    const auto *stats = static_cast<const PBGfxBridgeStats *>(
        api.GetBridgeStats());
    CHECK(stats != nullptr);
    CHECK(stats->frames_started == 2);
    CHECK(stats->frames_presented == 2);
    CHECK(stats->draw_calls == 4);
    CHECK(stats->triangles == 6);
    CHECK(stats->vertices == 18);
    CHECK(stats->textures_live == 1);
    CHECK(stats->texture_bytes == 256);
    CHECK(stats->shaders_live == 2);
    CHECK(stats->unsupported_shaders == 0);
    CHECK(stats->stream_peak_bytes == 264);

    CHECK(api.PrepareFirstFrame(firstFramePixels, 512, 256, 296, 200));
    CHECK(api.RenderFirstFrame());
    CHECK(stats->frames_presented == 3);
    CHECK(stats->draw_calls == 5);
    CHECK(stats->triangles == 8);
    CHECK(stats->vertices == 24);
    CHECK(stats->textures_live == 2);
    CHECK(stats->texture_bytes == 512U * 256U * 4U + 256U);
    CHECK(stats->shaders_live == 2);
    CHECK(!api.PrepareFirstFrame(firstFramePixels, 256, 256, 296, 200));

    PBTitleAssets assets = makeTitleAssets();
    CHECK(api.PrepareTitleFlow(&assets));
    CHECK(stats->textures_live == 5);
    CHECK(stats->texture_bytes ==
          512U * 256U * 4U + 256U + sizeof(titleLogoPixels) +
              sizeof(titlePromptPixels) + sizeof(titleCopyrightPixels));
    PBTitleFlow flow = {};
    flow.screen = PB_TITLE_FLOW_TITLE;
    flow.prompt_alpha = 255;
    CHECK(api.RenderTitleFlow(&flow));
    CHECK(stats->frames_presented == 4);
    CHECK(stats->draw_calls == 9);
    CHECK(stats->triangles == 16);
    CHECK(stats->vertices == 48);
    flow.screen = PB_TITLE_FLOW_FILE_SELECT;
    flow.selected_slot = 3;
    flow.slot_confirmed = true;
    CHECK(api.RenderTitleFlow(&flow));
    CHECK(stats->frames_presented == 5);
    CHECK(stats->draw_calls == 11);
    CHECK(stats->triangles == 36);
    CHECK(stats->vertices == 108);
    flow.selected_slot = PB_FILE_SELECT_SLOT_COUNT;
    CHECK(!api.RenderTitleFlow(&flow));
    CHECK(!api.PrepareTitleFlow(nullptr));

    CHECK(api.PrepareWorldBackground(firstFramePixels, 512, 256, 296, 200));
    CHECK(stats->textures_live == 6);
    CHECK(api.RenderWorldBackground(false));
    CHECK(stats->frames_presented == 6);
    CHECK(stats->draw_calls == 12);
    CHECK(stats->triangles == 38);
    CHECK(api.RenderWorldBackground(true));
    CHECK(stats->frames_presented == 7);
    CHECK(stats->draw_calls == 14);
    CHECK(stats->triangles == 42);
    CHECK(!api.PrepareWorldBackground(firstFramePixels, 256, 256, 296, 200));
    flow.screen = PB_TITLE_FLOW_TITLE;
    flow.selected_slot = 0;
    CHECK(api.RenderTitleFlow(&flow));
    CHECK(stats->frames_presented == 8);

    PBWorldScene scene = makeWorldScene();
    CHECK(api.PrepareWorldScene(&scene));
    CHECK(stats->textures_live == 10U);
    const uint64_t drawsBeforeWorld = stats->draw_calls;
    const uint64_t trianglesBeforeWorld = stats->triangles;
    CHECK(api.RenderWorldScene(&scene, false));
    CHECK(stats->frames_presented == 9U);
    CHECK(stats->draw_calls == drawsBeforeWorld + 4U);
    CHECK(stats->triangles == trianglesBeforeWorld + 7U);
    scene.message_timer = 1U;
    scene.transition_state = PB_WORLD_TRANSITION_FADE_OUT;
    scene.transition_frame = 15U;
    CHECK(api.RenderWorldScene(&scene, true));
    CHECK(stats->frames_presented == 10U);
    CHECK(stats->draw_calls == drawsBeforeWorld + 9U);
    CHECK(stats->triangles == trianglesBeforeWorld + 22U);
    CHECK(!api.PrepareWorldScene(nullptr));

    api.SetActive(false);
    CHECK(!api.RenderDiagnostic());
    api.SetActive(true);
    CHECK(api.RenderDiagnostic());
    CHECK(stats->frames_presented == 11);

    CHECK(api.CreateFramebuffer() == -1);
    CHECK(api.GetFramebufferTextureId(0) == nullptr);
    CHECK(api.GetPixelDepth(0, {}).empty());
    CHECK(stats->rejected_commands >= 3);
    return true;
}

static bool testCBoundary() {
    PBGfxApi3DS *api = nullptr;
    CHECK(pb_gfx_api_3ds_create(&api, nullptr) == PB_GFX_API_INIT_OK);
    CHECK(api != nullptr);
    CHECK(pb_gfx_api_3ds_prepare_diagnostic(api));
    CHECK(pb_gfx_api_3ds_render_diagnostic(api));
    CHECK(pb_gfx_api_3ds_prepare_first_frame(api, firstFramePixels, 512, 256,
                                             296, 200));
    CHECK(pb_gfx_api_3ds_render_first_frame(api));
    PBTitleAssets assets = makeTitleAssets();
    CHECK(pb_gfx_api_3ds_prepare_title_flow(api, &assets));
    PBTitleFlow flow = {};
    flow.screen = PB_TITLE_FLOW_TITLE;
    flow.prompt_alpha = 128;
    CHECK(pb_gfx_api_3ds_render_title_flow(api, &flow));
    flow.screen = PB_TITLE_FLOW_FILE_SELECT;
    flow.selected_slot = 2;
    CHECK(pb_gfx_api_3ds_render_title_flow(api, &flow));
    CHECK(pb_gfx_api_3ds_prepare_world_background(
        api, firstFramePixels, 512, 256, 296, 200));
    CHECK(pb_gfx_api_3ds_render_world_background(api, false));
    CHECK(pb_gfx_api_3ds_render_world_background(api, true));
    PBWorldScene scene = makeWorldScene();
    CHECK(pb_gfx_api_3ds_prepare_world_scene(api, &scene));
    CHECK(pb_gfx_api_3ds_render_world_scene(api, &scene, false));
    const PBGfxBridgeStats *stats = pb_gfx_api_3ds_stats(api);
    CHECK(stats != nullptr);
    CHECK(stats->frames_presented == 7);
    CHECK(stats->draw_calls == 16);
    CHECK(testRuntimeDisplayList(api));
    CHECK(testRuntimeMovememViewportUsesBottomLeftOrigin(api));
    CHECK(testRuntimeDepthTargetAndCopyRectangle(api));
    CHECK(testRuntimeLoadTileSubregion(api));
    CHECK(testRuntimeSplitCi8Palette(api));
    CHECK(testRuntimeOneCycleUsesPaperBoatCombiner(api));
    CHECK(testRuntimeDepthFogUsesSemanticCombiner(api));
    CHECK(testRuntimeKeyConvertUsesSemanticCombiner(api));
    CHECK(testRuntimeLegacyUnsafeModulateSkipsTexture(api));
    CHECK(testRuntimeTwoCycleBindsBothTiles(api));
    CHECK(testRuntimeTwoCycleUsesBaseTileWithoutLod(api));
    CHECK(testRuntimeTextureEvictionFrame(api));
    CHECK(!pb_gfx_api_3ds_prepare_title_flow(nullptr, &assets));
    CHECK(!pb_gfx_api_3ds_render_title_flow(api, nullptr));
    CHECK(!pb_gfx_api_3ds_prepare_world_background(
        nullptr, firstFramePixels, 512, 256, 296, 200));
    CHECK(!pb_gfx_api_3ds_render_world_background(nullptr, false));
    CHECK(!pb_gfx_api_3ds_prepare_world_scene(nullptr, &scene));
    CHECK(!pb_gfx_api_3ds_render_world_scene(api, nullptr, false));
    pb_gfx_api_3ds_set_active(api, false);
    CHECK(!pb_gfx_api_3ds_render_diagnostic(api));
    pb_gfx_api_3ds_destroy(api);

    CHECK(pb_gfx_api_3ds_create(nullptr, nullptr) ==
          PB_GFX_API_INIT_INVALID_ARGUMENT);
    CHECK(std::strcmp(pb_gfx_api_init_result_name(PB_GFX_API_INIT_OK),
                      "ready") == 0);
    CHECK(std::strcmp(pb_gfx_api_init_result_name(
                          static_cast<PBGfxApiInitResult>(999)),
                      "unknown") == 0);
    return true;
}

int main() {
    if (!testExactInterface() || !testCBoundary()) {
        return EXIT_FAILURE;
    }
    std::printf("M13 exact GfxRenderingAPI contract: %u checks passed\n",
                checksRun);
    return EXIT_SUCCESS;
}
