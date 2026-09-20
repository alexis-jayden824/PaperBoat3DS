#include "pb3ds/gfx_rendering_api_3ds.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <new>

#include "pb3ds/renderer.h"

namespace Fast {

struct ShaderProgram {
    uint64_t shaderId0 = 0;
    uint64_t shaderId1 = 0;
    PBGfxCombinerPlan plan = {};
    bool allocated = false;
};

} // namespace Fast

namespace {

constexpr uint64_t PackFormula(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                               unsigned int shift) {
    return (static_cast<uint64_t>(a) << shift) |
           (static_cast<uint64_t>(b) << (shift + 4U)) |
           (static_cast<uint64_t>(c) << (shift + 8U)) |
           (static_cast<uint64_t>(d) << (shift + 12U));
}

constexpr uint64_t kShadeShader =
    PackFormula(0, 0, 0, PB_GFX_SHADER_SHADE, 0) |
    PackFormula(0, 0, 0, PB_GFX_SHADER_SHADE, 16);
constexpr uint64_t kTextureShadeShader =
    PackFormula(PB_GFX_SHADER_TEXEL0, 0, PB_GFX_SHADER_SHADE, 0, 0) |
    PackFormula(PB_GFX_SHADER_TEXEL0_ALPHA, 0, PB_GFX_SHADER_SHADE, 0, 16);
constexpr uint64_t kAlphaOption = uint64_t{1} << PB_GFX_OPT_ALPHA;

PBTextureWrap TranslateWrap(uint32_t mode) {
    const bool mirror = (mode & 1U) != 0;
    const bool clamp = (mode & 2U) != 0;
    if (clamp) {
        return PB_WRAP_CLAMP_TO_EDGE;
    }
    return mirror ? PB_WRAP_MIRRORED_REPEAT : PB_WRAP_REPEAT;
}

#ifdef __3DS__
bool NativeBegin(PBRenderer3DS *renderer) {
    return pb_renderer_3ds_begin_frame(renderer);
}
bool NativeEnd(PBRenderer3DS *renderer) {
    return pb_renderer_3ds_end_frame(renderer);
}
void NativeFinish(PBRenderer3DS *renderer) {
    pb_renderer_3ds_finish(renderer);
}
bool NativeClear(PBRenderer3DS *renderer, bool color, bool depth) {
    return pb_renderer_3ds_clear(renderer, color, depth);
}
bool NativeViewport(PBRenderer3DS *renderer, const PBViewport *viewport) {
    return pb_renderer_3ds_set_viewport(renderer, viewport);
}
bool NativeScissor(PBRenderer3DS *renderer, const PBViewport *scissor) {
    return pb_renderer_3ds_set_scissor(renderer, scissor);
}
bool NativePipeline(PBRenderer3DS *renderer,
                    const PBRenderPipeline *pipeline) {
    return pb_renderer_3ds_set_pipeline(renderer, pipeline);
}
bool NativeUpload(PBRenderer3DS *renderer, uint32_t id, const uint8_t *rgba,
                  uint16_t width, uint16_t height) {
    return pb_renderer_3ds_upload_texture(renderer, id, rgba, width, height);
}
bool NativeBind(PBRenderer3DS *renderer, int tile, uint32_t id) {
    return pb_renderer_3ds_bind_texture(renderer, tile, id);
}
bool NativeSampler(PBRenderer3DS *renderer, uint32_t id,
                   PBTextureFilter filter, PBTextureWrap wrapS,
                   PBTextureWrap wrapT) {
    return pb_renderer_3ds_set_sampler(renderer, id, filter, wrapS, wrapT);
}
void NativeDelete(PBRenderer3DS *renderer, uint32_t id) {
    pb_renderer_3ds_delete_texture(renderer, id);
}
bool NativeCombiner(PBRenderer3DS *renderer, PBGfxCombinerMode mode) {
    return pb_renderer_3ds_set_combiner(renderer, static_cast<int>(mode));
}
bool NativeDraw(PBRenderer3DS *renderer, const float *vertices,
                size_t floatCount, size_t triangleCount,
                const PBGfxCombinerPlan &plan) {
    return pb_renderer_3ds_draw_stream(
        renderer, vertices, floatCount, triangleCount,
        plan.vertex_stride_floats, plan.used_textures[0],
        plan.used_textures[1], plan.uses_shade, plan.uses_alpha);
}
#else
bool NativeBegin(PBRenderer3DS *) {
    return true;
}
bool NativeEnd(PBRenderer3DS *) {
    return true;
}
void NativeFinish(PBRenderer3DS *) {}
bool NativeClear(PBRenderer3DS *, bool, bool) {
    return true;
}
bool NativeViewport(PBRenderer3DS *, const PBViewport *) {
    return true;
}
bool NativeScissor(PBRenderer3DS *, const PBViewport *) {
    return true;
}
bool NativePipeline(PBRenderer3DS *, const PBRenderPipeline *) {
    return true;
}
bool NativeUpload(PBRenderer3DS *, uint32_t, const uint8_t *, uint16_t,
                  uint16_t) {
    return true;
}
bool NativeBind(PBRenderer3DS *, int, uint32_t) {
    return true;
}
bool NativeSampler(PBRenderer3DS *, uint32_t, PBTextureFilter,
                   PBTextureWrap, PBTextureWrap) {
    return true;
}
void NativeDelete(PBRenderer3DS *, uint32_t) {}
bool NativeCombiner(PBRenderer3DS *, PBGfxCombinerMode) {
    return true;
}
bool NativeDraw(PBRenderer3DS *, const float *, size_t, size_t,
                const PBGfxCombinerPlan &) {
    return true;
}
#endif

void MakeChecker(std::array<uint8_t, 8U * 8U * 4U> &pixels) {
    for (unsigned int y = 0; y < 8U; y++) {
        for (unsigned int x = 0; x < 8U; x++) {
            const size_t offset = (y * 8U + x) * 4U;
            const bool alternate = (((x / 2U) ^ (y / 2U)) & 1U) != 0;
            pixels[offset + 0U] = alternate ? 232U : 28U;
            pixels[offset + 1U] = alternate ? 245U : 107U;
            pixels[offset + 2U] = alternate ? 244U : 117U;
            pixels[offset + 3U] = 255U;
        }
    }
}

float kPanelVertices[] = {
    34.0f,  30.0f,  0.45f, 1.0f, 0.0f, 0.0f, 0.0f, 0.95f, 0.98f, 1.0f, 1.0f,
    366.0f, 30.0f,  0.45f, 1.0f, 0.0f, 6.0f, 0.0f, 0.95f, 0.98f, 1.0f, 1.0f,
    366.0f, 210.0f, 0.45f, 1.0f, 0.0f, 6.0f, 4.0f, 0.95f, 0.98f, 1.0f, 1.0f,
    366.0f, 210.0f, 0.45f, 1.0f, 0.0f, 6.0f, 4.0f, 0.95f, 0.98f, 1.0f, 1.0f,
    34.0f,  210.0f, 0.45f, 1.0f, 0.0f, 0.0f, 4.0f, 0.95f, 0.98f, 1.0f, 1.0f,
    34.0f,  30.0f,  0.45f, 1.0f, 0.0f, 0.0f, 0.0f, 0.95f, 0.98f, 1.0f, 1.0f,
};

float kSailVertices[] = {
    112.0f, 62.0f,  0.70f, 1.0f, 0.0f, 1.00f, 0.24f, 0.10f, 0.88f,
    292.0f, 62.0f,  0.70f, 1.0f, 0.0f, 1.00f, 0.86f, 0.12f, 0.88f,
    202.0f, 196.0f, 0.70f, 1.0f, 0.0f, 0.16f, 0.95f, 0.78f, 0.88f,
};

} // namespace

namespace PB3DS {

struct GfxRenderingAPI3DS::Impl {
    explicit Impl(PBRenderer3DS *nativeRenderer) : renderer(nativeRenderer) {
        pb_gfx_bridge_init(&bridge);
    }

    PBRenderer3DS *renderer = nullptr;
    PBGfxBridge bridge = {};
    std::array<Fast::ShaderProgram, PB_GFX_MAX_SHADERS> shaders = {};
    Fast::ShaderProgram *currentShader = nullptr;
    Fast::ShaderProgram *diagnosticTextureShader = nullptr;
    Fast::ShaderProgram *diagnosticShadeShader = nullptr;
    Fast::FilteringMode filterMode = Fast::FILTER_THREE_POINT;
    PBRenderPipeline pipeline = {
        PB_CULL_NONE, false, false, PB_COMPARE_GREATER_EQUAL,
        PB_BLEND_DISABLED, PB_FILTER_NEAREST, PB_FILTER_NEAREST,
        PB_WRAP_REPEAT, PB_WRAP_REPEAT,
    };
    uint32_t diagnosticTexture = 0;
    int currentTile = 0;
    bool zmodeDecal = false;
    bool strictDecal = false;
    bool initialized = false;
    bool nativeFrameOpen = false;

    void Reject() {
        bridge.stats.rejected_commands++;
    }

    bool ApplyPipeline() {
        if (!NativePipeline(renderer, &pipeline)) {
            Reject();
            return false;
        }
        return true;
    }
};

GfxRenderingAPI3DS::GfxRenderingAPI3DS(PBRenderer3DS *renderer)
    : mImpl(new (std::nothrow) Impl(renderer)) {}

GfxRenderingAPI3DS::~GfxRenderingAPI3DS() {
    if (mImpl != nullptr) {
        for (const PBGfxTextureRecord &texture : mImpl->bridge.textures) {
            if (texture.allocated) {
                NativeDelete(mImpl->renderer, texture.id);
            }
        }
    }
    delete mImpl;
}

const char *GfxRenderingAPI3DS::GetName() {
    return "PICA200 (citro3d)";
}

int GfxRenderingAPI3DS::GetMaxTextureSize() {
    return PB_RENDER_TEXTURE_MAX_DIMENSION;
}

Fast::GfxClipParameters GfxRenderingAPI3DS::GetClipParameters() {
    return { true, false };
}

void GfxRenderingAPI3DS::UnloadShader(Fast::ShaderProgram *oldPrg) {
    if (mImpl != nullptr && mImpl->currentShader == oldPrg) {
        mImpl->currentShader = nullptr;
    }
}

void GfxRenderingAPI3DS::LoadShader(Fast::ShaderProgram *newPrg) {
    if (mImpl == nullptr || newPrg == nullptr || !newPrg->allocated) {
        if (mImpl != nullptr && newPrg != nullptr) {
            mImpl->Reject();
        }
        return;
    }
    mImpl->currentShader = newPrg;
    if (!NativeCombiner(mImpl->renderer, newPrg->plan.mode)) {
        mImpl->Reject();
    }
}

void GfxRenderingAPI3DS::ClearShaderCache() {
    if (mImpl == nullptr) {
        return;
    }
    mImpl->currentShader = nullptr;
    mImpl->diagnosticTextureShader = nullptr;
    mImpl->diagnosticShadeShader = nullptr;
    for (Fast::ShaderProgram &shader : mImpl->shaders) {
        shader = {};
    }
    pb_gfx_bridge_clear_shaders(&mImpl->bridge);
}

Fast::ShaderProgram *GfxRenderingAPI3DS::CreateAndLoadNewShader(
    uint64_t shaderId0, uint64_t shaderId1) {
    if (mImpl == nullptr) {
        return nullptr;
    }
    Fast::ShaderProgram *existing = LookupShader(shaderId0, shaderId1);
    if (existing != nullptr) {
        LoadShader(existing);
        return existing;
    }
    for (Fast::ShaderProgram &shader : mImpl->shaders) {
        if (!shader.allocated) {
            shader.shaderId0 = shaderId0;
            shader.shaderId1 = shaderId1;
            shader.allocated = true;
            (void)pb_gfx_combiner_decode(&shader.plan, shaderId0, shaderId1);
            if (!pb_gfx_bridge_record_shader(&mImpl->bridge, &shader.plan)) {
                shader = {};
                return nullptr;
            }
            LoadShader(&shader);
            return &shader;
        }
    }
    mImpl->Reject();
    return nullptr;
}

Fast::ShaderProgram *GfxRenderingAPI3DS::LookupShader(uint64_t shaderId0,
                                                      uint64_t shaderId1) {
    if (mImpl == nullptr) {
        return nullptr;
    }
    for (Fast::ShaderProgram &shader : mImpl->shaders) {
        if (shader.allocated && shader.shaderId0 == shaderId0 &&
            shader.shaderId1 == shaderId1) {
            return &shader;
        }
    }
    return nullptr;
}

void GfxRenderingAPI3DS::ShaderGetInfo(Fast::ShaderProgram *prg,
                                       uint8_t *numInputs,
                                       bool usedTextures[2]) {
    if (numInputs != nullptr) {
        *numInputs = prg != nullptr ? prg->plan.num_inputs : 0;
    }
    if (usedTextures != nullptr) {
        usedTextures[0] = prg != nullptr && prg->plan.used_textures[0];
        usedTextures[1] = prg != nullptr && prg->plan.used_textures[1];
    }
}

uint32_t GfxRenderingAPI3DS::NewTexture() {
    return mImpl != nullptr ? pb_gfx_bridge_new_texture(&mImpl->bridge) : 0;
}

void GfxRenderingAPI3DS::SelectTexture(int tile, uint32_t textureId) {
    if (mImpl == nullptr ||
        !pb_gfx_bridge_select_texture(&mImpl->bridge, tile, textureId)) {
        return;
    }
    mImpl->currentTile = tile;
    const PBGfxTextureRecord *texture =
        pb_gfx_bridge_find_texture(&mImpl->bridge, textureId);
    if (texture != nullptr && texture->uploaded &&
        !NativeBind(mImpl->renderer, tile, textureId)) {
        mImpl->Reject();
    }
}

void GfxRenderingAPI3DS::UploadTexture(const uint8_t *rgba32Buf,
                                       uint32_t width, uint32_t height) {
    if (mImpl == nullptr || rgba32Buf == nullptr || mImpl->currentTile < 0 ||
        mImpl->currentTile >= static_cast<int>(PB_GFX_TEXTURE_UNITS) ||
        width > UINT16_MAX || height > UINT16_MAX) {
        if (mImpl != nullptr) {
            mImpl->Reject();
        }
        return;
    }
    const uint32_t textureId =
        mImpl->bridge.selected_textures[mImpl->currentTile];
    if (!NativeUpload(mImpl->renderer, textureId, rgba32Buf,
                      static_cast<uint16_t>(width),
                      static_cast<uint16_t>(height)) ||
        !pb_gfx_bridge_upload_texture(&mImpl->bridge, textureId, width,
                                      height) ||
        !NativeBind(mImpl->renderer, mImpl->currentTile, textureId)) {
        mImpl->Reject();
    }
}

void GfxRenderingAPI3DS::SetSamplerParameters(int sampler, bool linearFilter,
                                               uint32_t cms, uint32_t cmt) {
    if (mImpl == nullptr || sampler < 0 ||
        sampler >= static_cast<int>(PB_GFX_TEXTURE_UNITS)) {
        if (mImpl != nullptr) {
            mImpl->Reject();
        }
        return;
    }
    const uint32_t textureId = mImpl->bridge.selected_textures[sampler];
    const PBTextureFilter filter =
        linearFilter && mImpl->filterMode == Fast::FILTER_LINEAR
            ? PB_FILTER_LINEAR
            : PB_FILTER_NEAREST;
    if (textureId == 0 ||
        !NativeSampler(mImpl->renderer, textureId, filter, TranslateWrap(cms),
                       TranslateWrap(cmt))) {
        mImpl->Reject();
    }
}

void GfxRenderingAPI3DS::SetDepthTestAndMask(bool depthTest, bool zUpd) {
    if (mImpl == nullptr) {
        return;
    }
    mImpl->pipeline.depth_test_enabled = depthTest;
    mImpl->pipeline.depth_write_enabled = zUpd;
    mImpl->pipeline.depth_function = mImpl->zmodeDecal
        ? (mImpl->strictDecal ? PB_COMPARE_EQUAL : PB_COMPARE_GREATER_EQUAL)
        : PB_COMPARE_GREATER_EQUAL;
    (void)mImpl->ApplyPipeline();
}

void GfxRenderingAPI3DS::SetZmodeDecal(bool decal) {
    if (mImpl == nullptr) {
        return;
    }
    mImpl->zmodeDecal = decal;
    SetDepthTestAndMask(mImpl->pipeline.depth_test_enabled,
                        mImpl->pipeline.depth_write_enabled);
}

void GfxRenderingAPI3DS::SetStrictDecal(bool on) {
    if (mImpl == nullptr) {
        return;
    }
    mImpl->strictDecal = on;
    SetDepthTestAndMask(mImpl->pipeline.depth_test_enabled,
                        mImpl->pipeline.depth_write_enabled);
}

void GfxRenderingAPI3DS::SetViewport(int x, int y, int width, int height) {
    if (mImpl == nullptr ||
        !pb_gfx_bridge_set_viewport(&mImpl->bridge, x, y, width, height) ||
        !NativeViewport(mImpl->renderer, &mImpl->bridge.viewport)) {
        return;
    }
}

void GfxRenderingAPI3DS::SetScissor(int x, int y, int width, int height) {
    if (mImpl == nullptr ||
        !pb_gfx_bridge_set_scissor(&mImpl->bridge, x, y, width, height) ||
        !NativeScissor(mImpl->renderer, &mImpl->bridge.scissor)) {
        return;
    }
}

void GfxRenderingAPI3DS::SetUseAlpha(bool useAlpha) {
    if (mImpl == nullptr) {
        return;
    }
    mImpl->pipeline.blend_mode =
        useAlpha ? PB_BLEND_ALPHA : PB_BLEND_DISABLED;
    (void)mImpl->ApplyPipeline();
}

void GfxRenderingAPI3DS::DrawTriangles(float bufVbo[], size_t bufVboLen,
                                       size_t bufVboNumTris) {
    if (mImpl == nullptr || mImpl->currentShader == nullptr ||
        !pb_gfx_bridge_record_draw(&mImpl->bridge,
                                   &mImpl->currentShader->plan, bufVbo,
                                   bufVboLen, bufVboNumTris)) {
        return;
    }
    if (!NativeCombiner(mImpl->renderer, mImpl->currentShader->plan.mode) ||
        !NativeDraw(mImpl->renderer, bufVbo, bufVboLen, bufVboNumTris,
                    mImpl->currentShader->plan)) {
        mImpl->Reject();
    }
}

void GfxRenderingAPI3DS::Init() {
    if (mImpl == nullptr || mImpl->initialized) {
        return;
    }
    mImpl->initialized = true;
    OnResize();
    (void)mImpl->ApplyPipeline();
}

void GfxRenderingAPI3DS::OnResize() {
    SetViewport(0, 0, PB_RENDER_TOP_WIDTH, PB_RENDER_TOP_HEIGHT);
    SetScissor(0, 0, PB_RENDER_TOP_WIDTH, PB_RENDER_TOP_HEIGHT);
}

void GfxRenderingAPI3DS::StartFrame() {
    if (mImpl == nullptr || !pb_gfx_bridge_start_frame(&mImpl->bridge)) {
        return;
    }
    if (!NativeBegin(mImpl->renderer)) {
        (void)pb_gfx_bridge_end_frame(&mImpl->bridge, false);
        return;
    }
    mImpl->nativeFrameOpen = true;
}

void GfxRenderingAPI3DS::EndFrame() {
    if (mImpl == nullptr || !mImpl->bridge.frame_open) {
        return;
    }
    const bool presented = mImpl->nativeFrameOpen && NativeEnd(mImpl->renderer);
    mImpl->nativeFrameOpen = false;
    (void)pb_gfx_bridge_end_frame(&mImpl->bridge, presented);
}

void GfxRenderingAPI3DS::FinishRender() {
    if (mImpl != nullptr) {
        NativeFinish(mImpl->renderer);
    }
}

int GfxRenderingAPI3DS::CreateFramebuffer() {
    if (mImpl != nullptr) {
        mImpl->Reject();
    }
    return -1;
}

void GfxRenderingAPI3DS::UpdateFramebufferParameters(
    int fbId, uint32_t width, uint32_t height, uint32_t msaaLevel,
    bool openglInvertY, bool renderTarget, bool hasDepthBuffer,
    bool canExtractDepth) {
    (void)openglInvertY;
    (void)renderTarget;
    (void)hasDepthBuffer;
    (void)canExtractDepth;
    if (mImpl == nullptr) {
        return;
    }
    if (fbId != 0 || width != PB_RENDER_TOP_WIDTH ||
        height != PB_RENDER_TOP_HEIGHT || msaaLevel != 1U) {
        mImpl->Reject();
        return;
    }
    OnResize();
}

void GfxRenderingAPI3DS::StartDrawToFramebuffer(int fbId, float noiseScale) {
    (void)noiseScale;
    if (mImpl != nullptr && fbId != 0) {
        mImpl->Reject();
    }
}

void GfxRenderingAPI3DS::CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0,
                                         int srcY0, int srcX1, int srcY1,
                                         int dstX0, int dstY0, int dstX1,
                                         int dstY1) {
    (void)fbDstId;
    (void)fbSrcId;
    (void)srcX0;
    (void)srcY0;
    (void)srcX1;
    (void)srcY1;
    (void)dstX0;
    (void)dstY0;
    (void)dstX1;
    (void)dstY1;
    if (mImpl != nullptr) {
        mImpl->Reject();
    }
}

void GfxRenderingAPI3DS::ClearFramebuffer(bool color, bool depth) {
    if (mImpl != nullptr && !NativeClear(mImpl->renderer, color, depth)) {
        mImpl->Reject();
    }
}

void GfxRenderingAPI3DS::ReadFramebufferToCPU(int fbId, uint32_t width,
                                               uint32_t height,
                                               uint16_t *rgba16Buf) {
    (void)fbId;
    (void)width;
    (void)height;
    (void)rgba16Buf;
    if (mImpl != nullptr) {
        mImpl->Reject();
    }
}

void GfxRenderingAPI3DS::ResolveMSAAColorBuffer(int fbIdTarget, int fbIdSrc) {
    (void)fbIdTarget;
    (void)fbIdSrc;
    if (mImpl != nullptr) {
        mImpl->Reject();
    }
}

std::unordered_map<std::pair<float, float>, uint16_t, Fast::hash_pair_ff>
GfxRenderingAPI3DS::GetPixelDepth(
    int fbId, const std::set<std::pair<float, float>> &coordinates) {
    (void)fbId;
    (void)coordinates;
    if (mImpl != nullptr) {
        mImpl->Reject();
    }
    return {};
}

void *GfxRenderingAPI3DS::GetFramebufferTextureId(int fbId) {
    (void)fbId;
    if (mImpl != nullptr) {
        mImpl->Reject();
    }
    return nullptr;
}

void GfxRenderingAPI3DS::SelectTextureFb(int fbId, int tile) {
    (void)fbId;
    (void)tile;
    if (mImpl != nullptr) {
        mImpl->Reject();
    }
}

void GfxRenderingAPI3DS::DeleteTexture(uint32_t texId) {
    if (mImpl == nullptr ||
        !pb_gfx_bridge_delete_texture(&mImpl->bridge, texId)) {
        return;
    }
    NativeDelete(mImpl->renderer, texId);
}

void GfxRenderingAPI3DS::SetTextureFilter(Fast::FilteringMode mode) {
    if (mImpl == nullptr || mode < Fast::FILTER_THREE_POINT ||
        mode > Fast::FILTER_NONE) {
        if (mImpl != nullptr) {
            mImpl->Reject();
        }
        return;
    }
    mImpl->filterMode = mode;
}

Fast::FilteringMode GfxRenderingAPI3DS::GetTextureFilter() {
    return mImpl != nullptr ? mImpl->filterMode : Fast::FILTER_NONE;
}

ImTextureID GfxRenderingAPI3DS::GetTextureById(int id) {
    if (mImpl == nullptr || id <= 0 ||
        pb_gfx_bridge_find_texture(&mImpl->bridge,
                                   static_cast<uint32_t>(id)) == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(id));
}

void GfxRenderingAPI3DS::SetCurrentPrimDepth(float depth) {
    if (depth != mCurrentPrimDepth) {
        mCurrentPrimDepth = depth;
        mPrimDepthDirty = true;
    }
}

void GfxRenderingAPI3DS::SetCullMode(int8_t keepSign) {
    if (mImpl == nullptr) {
        return;
    }
    mCurrentCullKeepSign = keepSign;
    mImpl->pipeline.cull_mode = keepSign > 0
                                    ? PB_CULL_BACK_CCW
                                    : (keepSign < 0 ? PB_CULL_FRONT_CCW
                                                    : PB_CULL_NONE);
    (void)mImpl->ApplyPipeline();
}

bool GfxRenderingAPI3DS::PrepareDiagnostic() {
    if (mImpl == nullptr) {
        return false;
    }
    Init();
    mImpl->diagnosticTextureShader =
        CreateAndLoadNewShader(kTextureShadeShader, kAlphaOption);
    mImpl->diagnosticShadeShader =
        CreateAndLoadNewShader(kShadeShader, kAlphaOption);
    if (mImpl->diagnosticTextureShader == nullptr ||
        mImpl->diagnosticShadeShader == nullptr ||
        !mImpl->diagnosticTextureShader->plan.supported ||
        !mImpl->diagnosticShadeShader->plan.supported) {
        return false;
    }

    mImpl->diagnosticTexture = NewTexture();
    if (mImpl->diagnosticTexture == 0) {
        return false;
    }
    SelectTexture(0, mImpl->diagnosticTexture);
    std::array<uint8_t, 8U * 8U * 4U> checker = {};
    MakeChecker(checker);
    UploadTexture(checker.data(), 8, 8);
    SetTextureFilter(Fast::FILTER_LINEAR);
    SetSamplerParameters(0, true, 0, 0);
    const PBGfxTextureRecord *record = pb_gfx_bridge_find_texture(
        &mImpl->bridge, mImpl->diagnosticTexture);
    return record != nullptr && record->uploaded;
}

bool GfxRenderingAPI3DS::RenderDiagnostic() {
    if (mImpl == nullptr || mImpl->diagnosticTextureShader == nullptr ||
        mImpl->diagnosticShadeShader == nullptr) {
        return false;
    }
    const uint64_t previousFrames = mImpl->bridge.stats.frames_presented;
    StartFrame();
    if (!mImpl->bridge.frame_open) {
        return false;
    }
    SetViewport(0, 0, PB_RENDER_TOP_WIDTH, PB_RENDER_TOP_HEIGHT);
    SetScissor(0, 0, PB_RENDER_TOP_WIDTH, PB_RENDER_TOP_HEIGHT);
    SetDepthTestAndMask(false, false);
    SetCullMode(0);

    LoadShader(mImpl->diagnosticTextureShader);
    SelectTexture(0, mImpl->diagnosticTexture);
    SetUseAlpha(false);
    DrawTriangles(kPanelVertices,
                  sizeof(kPanelVertices) / sizeof(kPanelVertices[0]), 2);

    LoadShader(mImpl->diagnosticShadeShader);
    SetScissor(70, 42, 264, 166);
    SetUseAlpha(true);
    DrawTriangles(kSailVertices,
                  sizeof(kSailVertices) / sizeof(kSailVertices[0]), 1);
    EndFrame();
    return mImpl->bridge.stats.frames_presented == previousFrames + 1U;
}

void GfxRenderingAPI3DS::SetActive(bool active) {
    if (mImpl == nullptr) {
        return;
    }
    if (!active && mImpl->bridge.frame_open) {
        const bool presented =
            mImpl->nativeFrameOpen && NativeEnd(mImpl->renderer);
        mImpl->nativeFrameOpen = false;
        (void)pb_gfx_bridge_end_frame(&mImpl->bridge, presented);
    }
    pb_gfx_bridge_set_active(&mImpl->bridge, active);
}

const void *GfxRenderingAPI3DS::GetBridgeStats() const {
    return mImpl != nullptr ? pb_gfx_bridge_stats(&mImpl->bridge) : nullptr;
}

} // namespace PB3DS

struct PBGfxApi3DS {
    PB3DS::GfxRenderingAPI3DS *implementation;
};

extern "C" PBGfxApiInitResult pb_gfx_api_3ds_create(
    PBGfxApi3DS **apiOut, PBRenderer3DS *renderer) {
    if (apiOut == nullptr) {
        return PB_GFX_API_INIT_INVALID_ARGUMENT;
    }
    *apiOut = nullptr;
#ifdef __3DS__
    if (renderer == nullptr) {
        return PB_GFX_API_INIT_INVALID_ARGUMENT;
    }
#endif
    PBGfxApi3DS *api = new (std::nothrow) PBGfxApi3DS{};
    if (api == nullptr) {
        return PB_GFX_API_INIT_OUT_OF_MEMORY;
    }
    api->implementation =
        new (std::nothrow) PB3DS::GfxRenderingAPI3DS(renderer);
    if (api->implementation == nullptr) {
        delete api;
        return PB_GFX_API_INIT_OUT_OF_MEMORY;
    }
    *apiOut = api;
    return PB_GFX_API_INIT_OK;
}

extern "C" const char *pb_gfx_api_init_result_name(
    PBGfxApiInitResult result) {
    switch (result) {
        case PB_GFX_API_INIT_OK:
            return "ready";
        case PB_GFX_API_INIT_INVALID_ARGUMENT:
            return "invalid argument";
        case PB_GFX_API_INIT_OUT_OF_MEMORY:
            return "out of memory";
        case PB_GFX_API_INIT_DIAGNOSTIC:
            return "diagnostic setup failed";
        default:
            return "unknown";
    }
}

extern "C" bool pb_gfx_api_3ds_prepare_diagnostic(PBGfxApi3DS *api) {
    return api != nullptr && api->implementation != nullptr &&
           api->implementation->PrepareDiagnostic();
}

extern "C" bool pb_gfx_api_3ds_render_diagnostic(PBGfxApi3DS *api) {
    return api != nullptr && api->implementation != nullptr &&
           api->implementation->RenderDiagnostic();
}

extern "C" void pb_gfx_api_3ds_set_active(PBGfxApi3DS *api, bool active) {
    if (api != nullptr && api->implementation != nullptr) {
        api->implementation->SetActive(active);
    }
}

extern "C" const PBGfxBridgeStats *pb_gfx_api_3ds_stats(
    const PBGfxApi3DS *api) {
    if (api == nullptr || api->implementation == nullptr) {
        return nullptr;
    }
    return static_cast<const PBGfxBridgeStats *>(
        api->implementation->GetBridgeStats());
}

extern "C" void pb_gfx_api_3ds_destroy(PBGfxApi3DS *api) {
    if (api == nullptr) {
        return;
    }
    delete api->implementation;
    delete api;
}
