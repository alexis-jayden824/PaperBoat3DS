#pragma once

#include "pb3ds/gfx_bridge.h"

typedef struct PBTitleAssets PBTitleAssets;
typedef struct PBTitleFlow PBTitleFlow;
typedef struct PBWorldScene PBWorldScene;

#ifdef __cplusplus

#include <cstddef>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <utility>

#include <fast/backends/gfx_rendering_api.h>

namespace PB3DS {

class GfxRenderingAPI3DS final : public Fast::GfxRenderingAPI {
  public:
    explicit GfxRenderingAPI3DS(PBRenderer3DS *renderer);
    ~GfxRenderingAPI3DS() override;

    GfxRenderingAPI3DS(const GfxRenderingAPI3DS &) = delete;
    GfxRenderingAPI3DS &operator=(const GfxRenderingAPI3DS &) = delete;

    const char *GetName() override;
    int GetMaxTextureSize() override;
    Fast::GfxClipParameters GetClipParameters() override;
    void UnloadShader(Fast::ShaderProgram *oldPrg) override;
    void LoadShader(Fast::ShaderProgram *newPrg) override;
    void ClearShaderCache() override;
    Fast::ShaderProgram *CreateAndLoadNewShader(uint64_t shaderId0,
                                                uint64_t shaderId1) override;
    Fast::ShaderProgram *LookupShader(uint64_t shaderId0,
                                     uint64_t shaderId1) override;
    void ShaderGetInfo(Fast::ShaderProgram *prg, uint8_t *numInputs,
                       bool usedTextures[2]) override;
    uint32_t NewTexture() override;
    void SelectTexture(int tile, uint32_t textureId) override;
    void UploadTexture(const uint8_t *rgba32Buf, uint32_t width,
                       uint32_t height) override;
    void SetSamplerParameters(int sampler, bool linearFilter, uint32_t cms,
                              uint32_t cmt) override;
    void SetDepthTestAndMask(bool depthTest, bool zUpd) override;
    void SetZmodeDecal(bool decal) override;
    void SetStrictDecal(bool on) override;
    void SetViewport(int x, int y, int width, int height) override;
    void SetScissor(int x, int y, int width, int height) override;
    void SetUseAlpha(bool useAlpha) override;
    void DrawTriangles(float bufVbo[], size_t bufVboLen,
                       size_t bufVboNumTris) override;
    void Init() override;
    void OnResize() override;
    void StartFrame() override;
    void EndFrame() override;
    void FinishRender() override;
    int CreateFramebuffer() override;
    void UpdateFramebufferParameters(int fbId, uint32_t width,
                                     uint32_t height, uint32_t msaaLevel,
                                     bool openglInvertY, bool renderTarget,
                                     bool hasDepthBuffer,
                                     bool canExtractDepth) override;
    void StartDrawToFramebuffer(int fbId, float noiseScale) override;
    void CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0,
                         int srcX1, int srcY1, int dstX0, int dstY0,
                         int dstX1, int dstY1) override;
    void ClearFramebuffer(bool color, bool depth) override;
    void ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height,
                              uint16_t *rgba16Buf) override;
    void ResolveMSAAColorBuffer(int fbIdTarget, int fbIdSrc) override;
    std::unordered_map<std::pair<float, float>, uint16_t, Fast::hash_pair_ff>
    GetPixelDepth(
        int fbId,
        const std::set<std::pair<float, float>> &coordinates) override;
    void *GetFramebufferTextureId(int fbId) override;
    void SelectTextureFb(int fbId, int tile) override;
    void DeleteTexture(uint32_t texId) override;
    void SetTextureFilter(Fast::FilteringMode mode) override;
    Fast::FilteringMode GetTextureFilter() override;
    ImTextureID GetTextureById(int id) override;
    void SetCurrentPrimDepth(float depth) override;
    void SetCullMode(int8_t keepSign) override;

    bool PrepareDiagnostic();
    bool RenderDiagnostic();
    bool PrepareFirstFrame(const uint8_t *rgba, uint16_t textureWidth,
                           uint16_t textureHeight, uint16_t sourceWidth,
                           uint16_t sourceHeight);
    bool RenderFirstFrame();
    bool PrepareTitleFlow(const PBTitleAssets *assets);
    bool RenderTitleFlow(const PBTitleFlow *flow);
    bool PrepareWorldBackground(const uint8_t *rgba,
                                uint16_t textureWidth,
                                uint16_t textureHeight,
                                uint16_t sourceWidth,
                                uint16_t sourceHeight);
    bool RenderWorldBackground(bool paused);
    bool PrepareWorldScene(const PBWorldScene *scene);
    bool RenderWorldScene(const PBWorldScene *scene, bool paused);
    void SetActive(bool active);
    const void *GetBridgeStats() const;

  private:
    struct Impl;
    Impl *mImpl;
};

} // namespace PB3DS

extern "C" {
#endif

typedef struct PBGfxApi3DS PBGfxApi3DS;

typedef enum {
    PB_GFX_API_INIT_OK = 0,
    PB_GFX_API_INIT_INVALID_ARGUMENT,
    PB_GFX_API_INIT_OUT_OF_MEMORY,
    PB_GFX_API_INIT_DIAGNOSTIC,
} PBGfxApiInitResult;

PBGfxApiInitResult pb_gfx_api_3ds_create(PBGfxApi3DS **api,
                                         PBRenderer3DS *renderer);
const char *pb_gfx_api_init_result_name(PBGfxApiInitResult result);
bool pb_gfx_api_3ds_prepare_diagnostic(PBGfxApi3DS *api);
bool pb_gfx_api_3ds_render_diagnostic(PBGfxApi3DS *api);
bool pb_gfx_api_3ds_prepare_first_frame(PBGfxApi3DS *api,
                                        const uint8_t *rgba,
                                        uint16_t texture_width,
                                        uint16_t texture_height,
                                        uint16_t source_width,
                                        uint16_t source_height);
bool pb_gfx_api_3ds_render_first_frame(PBGfxApi3DS *api);
bool pb_gfx_api_3ds_prepare_title_flow(PBGfxApi3DS *api,
                                       const PBTitleAssets *assets);
bool pb_gfx_api_3ds_render_title_flow(PBGfxApi3DS *api,
                                      const PBTitleFlow *flow);
bool pb_gfx_api_3ds_prepare_world_background(
    PBGfxApi3DS *api, const uint8_t *rgba, uint16_t texture_width,
    uint16_t texture_height, uint16_t source_width, uint16_t source_height);
bool pb_gfx_api_3ds_render_world_background(PBGfxApi3DS *api, bool paused);
bool pb_gfx_api_3ds_prepare_world_scene(PBGfxApi3DS *api,
                                        const PBWorldScene *scene);
bool pb_gfx_api_3ds_render_world_scene(PBGfxApi3DS *api,
                                       const PBWorldScene *scene,
                                       bool paused);
void pb_gfx_api_3ds_set_active(PBGfxApi3DS *api, bool active);
const PBGfxBridgeStats *pb_gfx_api_3ds_stats(const PBGfxApi3DS *api);
void pb_gfx_api_3ds_destroy(PBGfxApi3DS *api);

#ifdef __cplusplus
}
#endif
