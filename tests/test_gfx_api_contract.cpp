#include "pb3ds/gfx_rendering_api_3ds.h"
#include "pb3ds/title_flow.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

static unsigned int checksRun;
static uint8_t firstFramePixels[512U * 256U * 4U];
static uint8_t titleLogoPixels[256U * 128U * 4U];
static uint8_t titlePromptPixels[128U * 32U * 4U];
static uint8_t titleCopyrightPixels[256U * 32U * 4U];

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

    api.SetActive(false);
    CHECK(!api.RenderDiagnostic());
    api.SetActive(true);
    CHECK(api.RenderDiagnostic());
    CHECK(stats->frames_presented == 9);

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
    const PBGfxBridgeStats *stats = pb_gfx_api_3ds_stats(api);
    CHECK(stats != nullptr);
    CHECK(stats->frames_presented == 6);
    CHECK(stats->draw_calls == 12);
    CHECK(!pb_gfx_api_3ds_prepare_title_flow(nullptr, &assets));
    CHECK(!pb_gfx_api_3ds_render_title_flow(api, nullptr));
    CHECK(!pb_gfx_api_3ds_prepare_world_background(
        nullptr, firstFramePixels, 512, 256, 296, 200));
    CHECK(!pb_gfx_api_3ds_render_world_background(nullptr, false));
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
