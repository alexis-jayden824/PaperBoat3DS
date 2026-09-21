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

static bool testRuntimeOneCycleUsesSecondCombiner(PBGfxApi3DS *api) {
    static uint8_t texture[8U * 8U * 2U] = {};
    /* Cycle 0 is SHADE; cycle 1 is TEXEL0.  Real one-cycle RDP semantics
     * select cycle 1, so rendering this rectangle must upload a texture. */
    const PBRuntimeGfx displayList[] = {
        { .words = { UINT32_C(0xFD100007),
                     reinterpret_cast<uintptr_t>(texture) } },
        { .words = { UINT32_C(0xF5100000), 0U } },
        { .words = { UINT32_C(0xF3000000), 0U } },
        { .words = { UINT32_C(0xF2000000), UINT32_C(0x0001C01C) } },
        { .words = { UINT32_C(0xFCFFFFFF), UINT32_C(0xFFFE7879) } },
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
    CHECK(testRuntimeDepthTargetAndCopyRectangle(api));
    CHECK(testRuntimeLoadTileSubregion(api));
    CHECK(testRuntimeOneCycleUsesSecondCombiner(api));
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
