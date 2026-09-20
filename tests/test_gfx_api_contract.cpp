#include "pb3ds/gfx_rendering_api_3ds.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

static unsigned int checksRun;
static uint8_t firstFramePixels[512U * 256U * 4U];

#define CHECK(expression)                                                    \
    do {                                                                     \
        checksRun++;                                                         \
        if (!(expression)) {                                                 \
            std::fprintf(stderr,                                             \
                         "M11 API contract check failed at %s:%d: %s\n",   \
                         __FILE__, __LINE__, #expression);                   \
            return false;                                                    \
        }                                                                    \
    } while (0)

static_assert(std::is_base_of<Fast::GfxRenderingAPI,
                              PB3DS::GfxRenderingAPI3DS>::value,
              "3DS backend must implement the pinned libultraship API");
static_assert(!std::is_abstract<PB3DS::GfxRenderingAPI3DS>::value,
              "every pure virtual graphics method must be implemented");

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

    api.SetActive(false);
    CHECK(!api.RenderDiagnostic());
    api.SetActive(true);
    CHECK(api.RenderDiagnostic());
    CHECK(stats->frames_presented == 4);

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
    const PBGfxBridgeStats *stats = pb_gfx_api_3ds_stats(api);
    CHECK(stats != nullptr);
    CHECK(stats->frames_presented == 2);
    CHECK(stats->draw_calls == 3);
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
    std::printf("M11 exact GfxRenderingAPI contract: %u checks passed\n",
                checksRun);
    return EXIT_SUCCESS;
}
