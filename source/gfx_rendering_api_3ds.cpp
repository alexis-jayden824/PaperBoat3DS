#include "pb3ds/gfx_rendering_api_3ds.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <new>

#include "pb3ds/renderer.h"
#include "pb3ds/title_flow.h"
#include "pb3ds/title_layout.h"

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
constexpr size_t kFirstFrameVertexStride = 11U;
constexpr size_t kFirstFrameVertexCount = 6U;
constexpr size_t kShadeVertexStride = 9U;
constexpr size_t kFileSelectQuadCount = 9U;
constexpr size_t kFileSelectVertexCount = kFileSelectQuadCount * 6U;

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

void SetTexturedVertex(float *vertices, size_t index, float x, float y,
                       float u, float v, float red = 1.0f,
                       float green = 1.0f, float blue = 1.0f,
                       float alpha = 1.0f) {
    const size_t offset = index * kFirstFrameVertexStride;
    vertices[offset + 0U] = x;
    vertices[offset + 1U] = y;
    vertices[offset + 2U] = 0.45f;
    vertices[offset + 3U] = 1.0f;
    vertices[offset + 4U] = 0.0f;
    vertices[offset + 5U] = u;
    vertices[offset + 6U] = v;
    vertices[offset + 7U] = red;
    vertices[offset + 8U] = green;
    vertices[offset + 9U] = blue;
    vertices[offset + 10U] = alpha;
}

bool BuildTexturedQuad(
    std::array<float, kFirstFrameVertexStride * kFirstFrameVertexCount>
        &vertices,
    float left, float bottom, float width, float height,
    uint16_t textureWidth, uint16_t textureHeight, uint16_t sourceWidth,
    uint16_t sourceHeight, float red = 1.0f, float green = 1.0f,
    float blue = 1.0f, float alpha = 1.0f) {
    PBTexturedQuad quad;
    if (!pb_renderer_textured_quad(&quad, left, bottom, width, height,
                                   textureWidth, textureHeight, sourceWidth,
                                   sourceHeight)) {
        return false;
    }
    SetTexturedVertex(vertices.data(), 0, quad.left, quad.bottom,
                      quad.left_u, quad.bottom_v, red, green, blue, alpha);
    SetTexturedVertex(vertices.data(), 1, quad.right, quad.bottom,
                      quad.right_u, quad.bottom_v, red, green, blue, alpha);
    SetTexturedVertex(vertices.data(), 2, quad.right, quad.top,
                      quad.right_u, quad.top_v, red, green, blue, alpha);
    SetTexturedVertex(vertices.data(), 3, quad.right, quad.top,
                      quad.right_u, quad.top_v, red, green, blue, alpha);
    SetTexturedVertex(vertices.data(), 4, quad.left, quad.top,
                      quad.left_u, quad.top_v, red, green, blue, alpha);
    SetTexturedVertex(vertices.data(), 5, quad.left, quad.bottom,
                      quad.left_u, quad.bottom_v, red, green, blue, alpha);
    return true;
}

void SetShadeVertex(float *vertices, size_t index, float x, float y,
                    float red, float green, float blue, float alpha) {
    const size_t offset = index * kShadeVertexStride;
    vertices[offset + 0U] = x;
    vertices[offset + 1U] = y;
    vertices[offset + 2U] = 0.55f;
    vertices[offset + 3U] = 1.0f;
    vertices[offset + 4U] = 0.0f;
    vertices[offset + 5U] = red;
    vertices[offset + 6U] = green;
    vertices[offset + 7U] = blue;
    vertices[offset + 8U] = alpha;
}

void AppendShadeQuad(float *vertices, size_t *vertexIndex, float left,
                     float bottom, float width, float height, float red,
                     float green, float blue, float alpha) {
    const float right = left + width;
    const float top = bottom + height;
    SetShadeVertex(vertices, (*vertexIndex)++, left, bottom, red, green, blue,
                   alpha);
    SetShadeVertex(vertices, (*vertexIndex)++, right, bottom, red, green, blue,
                   alpha);
    SetShadeVertex(vertices, (*vertexIndex)++, right, top, red, green, blue,
                   alpha);
    SetShadeVertex(vertices, (*vertexIndex)++, right, top, red, green, blue,
                   alpha);
    SetShadeVertex(vertices, (*vertexIndex)++, left, top, red, green, blue,
                   alpha);
    SetShadeVertex(vertices, (*vertexIndex)++, left, bottom, red, green, blue,
                   alpha);
}

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
    Fast::ShaderProgram *firstFrameShader = nullptr;
    Fast::ShaderProgram *titleTextureShader = nullptr;
    Fast::ShaderProgram *titleShadeShader = nullptr;
    Fast::FilteringMode filterMode = Fast::FILTER_THREE_POINT;
    PBRenderPipeline pipeline = {
        PB_CULL_NONE, false, false, PB_COMPARE_GREATER_EQUAL,
        PB_BLEND_DISABLED, PB_FILTER_NEAREST, PB_FILTER_NEAREST,
        PB_WRAP_REPEAT, PB_WRAP_REPEAT,
    };
    uint32_t diagnosticTexture = 0;
    uint32_t firstFrameTexture = 0;
    uint32_t titleLogoTexture = 0;
    uint32_t titlePromptTexture = 0;
    uint32_t titleCopyrightTexture = 0;
    std::array<float, kFirstFrameVertexStride * kFirstFrameVertexCount>
        firstFrameVertices = {};
    std::array<float, kFirstFrameVertexStride * kFirstFrameVertexCount>
        titleLogoVertices = {};
    std::array<float, kFirstFrameVertexStride * kFirstFrameVertexCount>
        titlePromptVertices = {};
    std::array<float, kFirstFrameVertexStride * kFirstFrameVertexCount>
        titleCopyrightVertices = {};
    std::array<float, kShadeVertexStride * kFileSelectVertexCount>
        fileSelectVertices = {};
    PBTitleLayout titleLayout = {};
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
    mImpl->firstFrameShader = nullptr;
    mImpl->titleTextureShader = nullptr;
    mImpl->titleShadeShader = nullptr;
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

bool GfxRenderingAPI3DS::PrepareFirstFrame(
    const uint8_t *rgba, uint16_t textureWidth, uint16_t textureHeight,
    uint16_t sourceWidth, uint16_t sourceHeight) {
    if (mImpl == nullptr || rgba == nullptr || sourceWidth == 0 ||
        sourceHeight == 0 || sourceWidth > textureWidth ||
        sourceHeight > textureHeight || sourceWidth > PB_RENDER_TOP_WIDTH ||
        sourceHeight > PB_RENDER_TOP_HEIGHT) {
        return false;
    }
    Init();
    mImpl->firstFrameShader =
        CreateAndLoadNewShader(kTextureShadeShader, kAlphaOption);
    if (mImpl->firstFrameShader == nullptr ||
        !mImpl->firstFrameShader->plan.supported) {
        return false;
    }
    if (mImpl->firstFrameTexture != 0) {
        DeleteTexture(mImpl->firstFrameTexture);
        mImpl->firstFrameTexture = 0;
    }
    mImpl->firstFrameTexture = NewTexture();
    if (mImpl->firstFrameTexture == 0) {
        return false;
    }
    SelectTexture(0, mImpl->firstFrameTexture);
    UploadTexture(rgba, textureWidth, textureHeight);
    SetTextureFilter(Fast::FILTER_LINEAR);
    SetSamplerParameters(0, true, 2U, 2U);
    const PBGfxTextureRecord *record = pb_gfx_bridge_find_texture(
        &mImpl->bridge, mImpl->firstFrameTexture);
    if (record == nullptr || !record->uploaded) {
        DeleteTexture(mImpl->firstFrameTexture);
        mImpl->firstFrameTexture = 0;
        return false;
    }

    float left =
        (static_cast<float>(PB_RENDER_TOP_WIDTH) - sourceWidth) * 0.5f;
    float bottom =
        (static_cast<float>(PB_RENDER_TOP_HEIGHT) - sourceHeight) * 0.5f;
    if (sourceWidth == PB_TITLE_BACKGROUND_WIDTH &&
        sourceHeight == PB_TITLE_BACKGROUND_HEIGHT &&
        pb_title_layout_compute(&mImpl->titleLayout, PB_RENDER_TOP_WIDTH,
                                PB_RENDER_TOP_HEIGHT)) {
        left = mImpl->titleLayout.background.left;
        bottom = mImpl->titleLayout.background.bottom;
    }
    return BuildTexturedQuad(mImpl->firstFrameVertices, left, bottom,
                             static_cast<float>(sourceWidth),
                             static_cast<float>(sourceHeight), textureWidth,
                             textureHeight, sourceWidth, sourceHeight);
}

bool GfxRenderingAPI3DS::RenderFirstFrame() {
    if (mImpl == nullptr || mImpl->firstFrameShader == nullptr ||
        mImpl->firstFrameTexture == 0) {
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
    LoadShader(mImpl->firstFrameShader);
    SelectTexture(0, mImpl->firstFrameTexture);
    SetUseAlpha(false);
    DrawTriangles(mImpl->firstFrameVertices.data(),
                  mImpl->firstFrameVertices.size(), 2);
    EndFrame();
    return mImpl->bridge.stats.frames_presented == previousFrames + 1U;
}

bool GfxRenderingAPI3DS::PrepareTitleFlow(const PBTitleAssets *assets) {
    const auto textureMatches = [](const PBDecodedTexture &texture,
                                   uint16_t textureWidth,
                                   uint16_t textureHeight,
                                   uint16_t sourceWidth,
                                   uint16_t sourceHeight) {
        return texture.rgba != nullptr && texture.texture_width == textureWidth &&
               texture.texture_height == textureHeight &&
               texture.source_width == sourceWidth &&
               texture.source_height == sourceHeight &&
               texture.rgba_size ==
                   static_cast<size_t>(textureWidth) * textureHeight * 4U;
    };
    if (mImpl == nullptr || assets == nullptr ||
        assets->result != PB_TITLE_ASSETS_READY ||
        mImpl->firstFrameTexture == 0 || mImpl->firstFrameShader == nullptr ||
        !textureMatches(assets->logo, 256, 128, PB_TITLE_LOGO_WIDTH,
                        PB_TITLE_LOGO_HEIGHT) ||
        !textureMatches(assets->prompt, 128, 32, PB_TITLE_PROMPT_WIDTH,
                        PB_TITLE_PROMPT_HEIGHT) ||
        !textureMatches(assets->copyright, 256, 32,
                        PB_TITLE_COPYRIGHT_WIDTH,
                        PB_TITLE_COPYRIGHT_HEIGHT) ||
        !pb_title_layout_compute(&mImpl->titleLayout, PB_RENDER_TOP_WIDTH,
                                 PB_RENDER_TOP_HEIGHT)) {
        return false;
    }

    Init();
    mImpl->titleTextureShader =
        CreateAndLoadNewShader(kTextureShadeShader, kAlphaOption);
    mImpl->titleShadeShader =
        CreateAndLoadNewShader(kShadeShader, kAlphaOption);
    if (mImpl->titleTextureShader == nullptr ||
        mImpl->titleShadeShader == nullptr ||
        !mImpl->titleTextureShader->plan.supported ||
        !mImpl->titleShadeShader->plan.supported) {
        return false;
    }

    const auto discardTitleTextures = [this]() {
        uint32_t *textures[] = {
            &mImpl->titleLogoTexture,
            &mImpl->titlePromptTexture,
            &mImpl->titleCopyrightTexture,
        };
        for (uint32_t *texture : textures) {
            if (*texture != 0) {
                DeleteTexture(*texture);
                *texture = 0;
            }
        }
    };
    discardTitleTextures();

    const auto uploadTexture = [this](uint32_t *textureId,
                                      const PBDecodedTexture &texture) {
        *textureId = NewTexture();
        if (*textureId == 0) {
            return false;
        }
        SelectTexture(0, *textureId);
        UploadTexture(texture.rgba, texture.texture_width,
                      texture.texture_height);
        SetTextureFilter(Fast::FILTER_LINEAR);
        SetSamplerParameters(0, true, 2U, 2U);
        const PBGfxTextureRecord *record =
            pb_gfx_bridge_find_texture(&mImpl->bridge, *textureId);
        return record != nullptr && record->uploaded;
    };
    if (!uploadTexture(&mImpl->titleLogoTexture, assets->logo) ||
        !uploadTexture(&mImpl->titlePromptTexture, assets->prompt) ||
        !uploadTexture(&mImpl->titleCopyrightTexture, assets->copyright) ||
        !BuildTexturedQuad(mImpl->titleLogoVertices,
                           mImpl->titleLayout.logo.left,
                           mImpl->titleLayout.logo.bottom,
                           mImpl->titleLayout.logo.width,
                           mImpl->titleLayout.logo.height,
                           assets->logo.texture_width,
                           assets->logo.texture_height,
                           assets->logo.source_width,
                           assets->logo.source_height) ||
        !BuildTexturedQuad(mImpl->titlePromptVertices,
                           mImpl->titleLayout.prompt.left,
                           mImpl->titleLayout.prompt.bottom,
                           mImpl->titleLayout.prompt.width,
                           mImpl->titleLayout.prompt.height,
                           assets->prompt.texture_width,
                           assets->prompt.texture_height,
                           assets->prompt.source_width,
                           assets->prompt.source_height,
                           static_cast<float>(PB_TITLE_PROMPT_TINT_RED) /
                               255.0f,
                           static_cast<float>(PB_TITLE_PROMPT_TINT_GREEN) /
                               255.0f,
                           static_cast<float>(PB_TITLE_PROMPT_TINT_BLUE) /
                               255.0f) ||
        !BuildTexturedQuad(mImpl->titleCopyrightVertices,
                           mImpl->titleLayout.copyright.left,
                           mImpl->titleLayout.copyright.bottom,
                           mImpl->titleLayout.copyright.width,
                           mImpl->titleLayout.copyright.height,
                           assets->copyright.texture_width,
                           assets->copyright.texture_height,
                           assets->copyright.source_width,
                           assets->copyright.source_height)) {
        discardTitleTextures();
        return false;
    }
    return true;
}

bool GfxRenderingAPI3DS::RenderTitleFlow(const PBTitleFlow *flow) {
    if (mImpl == nullptr || flow == nullptr ||
        (flow->screen != PB_TITLE_FLOW_TITLE &&
         flow->screen != PB_TITLE_FLOW_FILE_SELECT) ||
        flow->selected_slot >= PB_FILE_SELECT_SLOT_COUNT ||
        mImpl->firstFrameShader == nullptr ||
        mImpl->firstFrameTexture == 0 ||
        mImpl->titleTextureShader == nullptr ||
        mImpl->titleShadeShader == nullptr ||
        mImpl->titleLogoTexture == 0 || mImpl->titlePromptTexture == 0 ||
        mImpl->titleCopyrightTexture == 0) {
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

    LoadShader(mImpl->firstFrameShader);
    SelectTexture(0, mImpl->firstFrameTexture);
    SetUseAlpha(false);
    DrawTriangles(mImpl->firstFrameVertices.data(),
                  mImpl->firstFrameVertices.size(), 2);

    if (flow->screen == PB_TITLE_FLOW_TITLE) {
        const float promptAlpha =
            static_cast<float>(flow->prompt_alpha) / 255.0f;
        for (size_t vertex = 0; vertex < kFirstFrameVertexCount; vertex++) {
            mImpl->titlePromptVertices[
                vertex * kFirstFrameVertexStride + 10U] = promptAlpha;
        }

        LoadShader(mImpl->titleTextureShader);
        SetUseAlpha(true);
        SelectTexture(0, mImpl->titleLogoTexture);
        DrawTriangles(mImpl->titleLogoVertices.data(),
                      mImpl->titleLogoVertices.size(), 2);
        SelectTexture(0, mImpl->titlePromptTexture);
        DrawTriangles(mImpl->titlePromptVertices.data(),
                      mImpl->titlePromptVertices.size(), 2);
        SelectTexture(0, mImpl->titleCopyrightTexture);
        DrawTriangles(mImpl->titleCopyrightVertices.data(),
                      mImpl->titleCopyrightVertices.size(), 2);
    } else {
        size_t vertex = 0;
        AppendShadeQuad(mImpl->fileSelectVertices.data(), &vertex,
                        0.0f, 0.0f, 400.0f, 240.0f,
                        0.02f, 0.04f, 0.12f, 0.72f);
        for (uint8_t slot = 0; slot < PB_FILE_SELECT_SLOT_COUNT; slot++) {
            const float left = slot % 2U == 0U ? 49.0f : 189.0f;
            const float bottom = slot / 2U == 0U ? 145.0f : 76.0f;
            const bool selected = slot == flow->selected_slot;
            const bool confirmed = selected && flow->slot_confirmed;
            AppendShadeQuad(
                mImpl->fileSelectVertices.data(), &vertex,
                left, bottom, 130.0f, 54.0f,
                confirmed ? 0.25f : (selected ? 1.0f : 0.28f),
                confirmed ? 0.95f : (selected ? 0.72f : 0.38f),
                confirmed ? 0.42f : (selected ? 0.12f : 0.62f),
                0.96f);
            AppendShadeQuad(mImpl->fileSelectVertices.data(), &vertex,
                            left + 3.0f, bottom + 3.0f, 124.0f, 48.0f,
                            0.03f, 0.08f, 0.19f, 0.94f);
        }

        LoadShader(mImpl->titleShadeShader);
        SetUseAlpha(true);
        DrawTriangles(mImpl->fileSelectVertices.data(),
                      mImpl->fileSelectVertices.size(),
                      kFileSelectVertexCount / 3U);
    }
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

extern "C" bool pb_gfx_api_3ds_prepare_first_frame(
    PBGfxApi3DS *api, const uint8_t *rgba, uint16_t textureWidth,
    uint16_t textureHeight, uint16_t sourceWidth, uint16_t sourceHeight) {
    return api != nullptr && api->implementation != nullptr &&
           api->implementation->PrepareFirstFrame(
               rgba, textureWidth, textureHeight, sourceWidth, sourceHeight);
}

extern "C" bool pb_gfx_api_3ds_render_first_frame(PBGfxApi3DS *api) {
    return api != nullptr && api->implementation != nullptr &&
           api->implementation->RenderFirstFrame();
}

extern "C" bool pb_gfx_api_3ds_prepare_title_flow(
    PBGfxApi3DS *api, const PBTitleAssets *assets) {
    return api != nullptr && api->implementation != nullptr &&
           api->implementation->PrepareTitleFlow(assets);
}

extern "C" bool pb_gfx_api_3ds_render_title_flow(
    PBGfxApi3DS *api, const PBTitleFlow *flow) {
    return api != nullptr && api->implementation != nullptr &&
           api->implementation->RenderTitleFlow(flow);
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
