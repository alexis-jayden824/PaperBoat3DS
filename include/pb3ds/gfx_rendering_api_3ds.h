#pragma once

#include "pb3ds/gfx_bridge.h"
#include "pb3ds/runtime_resource_types.h"

typedef struct PBTitleAssets PBTitleAssets;
typedef struct PBTitleFlow PBTitleFlow;
typedef struct PBWorldScene PBWorldScene;

typedef struct {
    uint64_t frames_started;
    uint64_t frames_rendered;
    uint64_t commands;
    uint64_t display_lists;
    uint64_t texture_fallbacks;
    uint64_t texture_evictions;
    uint64_t semantic_combiner_batches;
    uint64_t semantic_two_cycle_batches;
    uint64_t semantic_fog_batches;
    uint64_t semantic_key_convert_batches;
    uint64_t legacy_combiner_fallbacks;
    uint64_t legacy_fog_fallbacks;
    uint64_t legacy_key_convert_fallbacks;
    uint64_t legacy_unsafe_modulate_batches;
    uint64_t depth_target_clears;
    uint64_t copy_rectangles;
    uint32_t unknown_commands;
    uint32_t missing_resources;
    uint32_t malformed_lists;
    uint32_t max_call_depth;
    uint32_t commands_last_frame;
    uint32_t commands_peak_frame;
    uint8_t last_unknown_opcode;
    /* Diagnostics for the "sprites/geometry cut off" class of report: the
     * most recently established CPU-side game viewport (used to convert
     * transformed vertices to screen pixels in LoadVertices) and the most
     * recently established GPU scissor rect (from G_SETSCISSOR, or the
     * full-screen default applied at the start of every frame), both in
     * logical 400x240 top-screen pixel space. A scissor or viewport height
     * smaller than PB_RENDER_TOP_HEIGHT (240), or a nonzero y, here at the
     * moment a cut-off sprite is on screen pinpoints whether the clipping
     * comes from one of these versus somewhere else (e.g. vertex position
     * math). Populated every time either is set; read at any moment (e.g.
     * from the on-screen debug HUD) to see the value active for that frame. */
    int32_t game_viewport_x;
    int32_t game_viewport_y;
    uint32_t game_viewport_w;
    uint32_t game_viewport_h;
    int32_t scissor_x;
    int32_t scissor_y;
    uint32_t scissor_w;
    uint32_t scissor_h;
} PBRuntimeGfxStats;

#ifdef __cplusplus

#include <cstddef>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <utility>

#include <fast/backends/gfx_rendering_api.h>

namespace PB3DS {

class RuntimeDisplayListRenderer;

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
    bool ShaderIsSupported(const Fast::ShaderProgram *program) const;
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
    void ConfigureRuntimePipeline(bool depthTest, bool depthWrite,
                                  bool decal, int8_t cullKeepSign,
                                  bool useAlpha, bool alphaTest,
                                  uint8_t alphaReference);
    void ConfigureRuntimeFog(bool enabled, uint8_t red, uint8_t green,
                             uint8_t blue, int16_t fogMultiply,
                             int16_t fogOffset);
    void DrawTriangles(float bufVbo[], size_t bufVboLen,
                       size_t bufVboNumTris) override;
    void Init() override;
    void OnResize() override;
    void StartFrame() override;
    void PreserveColorOnNextFrame(bool preserve);
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
    bool RenderDisplayList(const PBRuntimeGfx *displayList);
    void InvalidateRuntimeTexture(const void *address);
    void ClearRuntimeDepth();
    void SetActive(bool active);
    const void *GetBridgeStats() const;
    const PBRuntimeGfxStats *GetRuntimeStats() const;

  private:
    void DestroyRuntimeRenderer();
    struct Impl;
    Impl *mImpl;
    RuntimeDisplayListRenderer *mRuntimeRenderer = nullptr;
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
bool pb_gfx_api_3ds_render_display_list(PBGfxApi3DS *api,
                                        const PBRuntimeGfx *display_list);
void pb_gfx_api_3ds_invalidate_texture(PBGfxApi3DS *api,
                                       const void *address);
void pb_gfx_api_3ds_clear_depth(PBGfxApi3DS *api);
void pb_gfx_api_3ds_set_active(PBGfxApi3DS *api, bool active);
const PBGfxBridgeStats *pb_gfx_api_3ds_stats(const PBGfxApi3DS *api);
const PBRuntimeGfxStats *pb_gfx_api_3ds_runtime_stats(
    const PBGfxApi3DS *api);
void pb_gfx_api_3ds_destroy(PBGfxApi3DS *api);

#ifdef __cplusplus
}
#endif
