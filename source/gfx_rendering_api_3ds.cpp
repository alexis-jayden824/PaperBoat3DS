#include "pb3ds/gfx_rendering_api_3ds.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>

#include "pb3ds/renderer.h"
#include "pb3ds/title_flow.h"
#include "pb3ds/title_layout.h"
#include "pb3ds/world_scene.h"

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
constexpr size_t kWorldVertexCapacity = PB_GFX_MAX_STREAM_TRIANGLES * 3U;
constexpr size_t kWorldFloatCapacity =
    kWorldVertexCapacity * kFirstFrameVertexStride;
constexpr float kWorldFovDegrees = 25.0f;
constexpr float kWorldBoomLength = 500.0f;
constexpr float kWorldBoomPitchDegrees = 15.0f;
constexpr float kWorldNearClip = 8.0f;
constexpr float kWorldFarClip = 4096.0f;
constexpr float kPi = 3.14159265358979323846f;

struct WorldCamera {
    PBWorldVec3 eye = {};
    PBWorldVec3 forward = {};
    PBWorldVec3 right = {};
    PBWorldVec3 up = {};
    float focal = 0.0f;
};

struct WorldProjectedPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float distance = 0.0f;
};

struct WorldGpuTexture {
    uint32_t id = 0U;
    uint16_t textureWidth = 0U;
    uint16_t textureHeight = 0U;
    uint16_t sourceWidth = 0U;
    uint16_t sourceHeight = 0U;
};

PBWorldVec3 Subtract(PBWorldVec3 left, PBWorldVec3 right) {
    return {
        left.x - right.x,
        left.y - right.y,
        left.z - right.z,
    };
}

float Dot(PBWorldVec3 left, PBWorldVec3 right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

PBWorldVec3 Cross(PBWorldVec3 left, PBWorldVec3 right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

PBWorldVec3 Normalize(PBWorldVec3 value) {
    const float length = std::sqrt(Dot(value, value));
    if (length <= 0.0001f) {
        return {};
    }
    return { value.x / length, value.y / length, value.z / length };
}

WorldCamera BuildWorldCamera(const PBWorldScene &scene) {
    const float pitch = kWorldBoomPitchDegrees * (kPi / 180.0f);
    WorldCamera camera;
    camera.eye = {
        scene.camera_target.x,
        scene.camera_target.y + std::sin(pitch) * kWorldBoomLength,
        scene.camera_target.z + std::cos(pitch) * kWorldBoomLength,
    };
    camera.forward = Normalize(Subtract(scene.camera_target, camera.eye));
    camera.right = Normalize(Cross(camera.forward, { 0.0f, 1.0f, 0.0f }));
    camera.up = Normalize(Cross(camera.right, camera.forward));
    camera.focal =
        (static_cast<float>(PB_RENDER_TOP_HEIGHT) * 0.5f) /
        std::tan(kWorldFovDegrees * 0.5f * (kPi / 180.0f));
    return camera;
}

bool ProjectWorldPoint(const WorldCamera &camera, PBWorldVec3 position,
                       WorldProjectedPoint *output) {
    if (output == nullptr) {
        return false;
    }
    const PBWorldVec3 relative = Subtract(position, camera.eye);
    const float distance = Dot(relative, camera.forward);
    if (distance <= kWorldNearClip || distance >= kWorldFarClip) {
        return false;
    }
    output->x = static_cast<float>(PB_RENDER_TOP_WIDTH) * 0.5f +
                Dot(relative, camera.right) * camera.focal / distance;
    output->y = static_cast<float>(PB_RENDER_TOP_HEIGHT) * 0.5f +
                Dot(relative, camera.up) * camera.focal / distance;
    output->z = 1.0f -
                (distance - kWorldNearClip) /
                    (kWorldFarClip - kWorldNearClip);
    output->z = std::max(0.001f, std::min(0.999f, output->z));
    output->distance = distance;
    return true;
}

float WorldFadeAlpha(const PBWorldScene &scene) {
    constexpr float kFadeFrames = 30.0f;
    if (scene.transition_state == PB_WORLD_TRANSITION_FADE_IN) {
        return 1.0f - static_cast<float>(scene.transition_frame) /
                          kFadeFrames;
    }
    if (scene.transition_state == PB_WORLD_TRANSITION_FADE_OUT) {
        return static_cast<float>(scene.transition_frame) / kFadeFrames;
    }
    return scene.transition_state == PB_WORLD_TRANSITION_WAITING ? 1.0f
                                                                 : 0.0f;
}

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
void NativePreserveColor(PBRenderer3DS *renderer, bool preserve) {
    pb_renderer_3ds_preserve_color(renderer, preserve);
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
void NativePreserveColor(PBRenderer3DS *, bool) {}
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

void SetProjectedTexturedVertex(float *vertices, size_t index,
                                const WorldProjectedPoint &point, float u,
                                float v, uint8_t red, uint8_t green,
                                uint8_t blue, uint8_t alpha) {
    const size_t offset = index * kFirstFrameVertexStride;
    /*
     * Preserve the camera-space distance as clip W.  Feeding homogeneous
     * screen coordinates through the orthographic shader leaves the final
     * position unchanged while allowing PICA200 to perspective-correct the
     * texture coordinates across each projected world triangle.
     */
    vertices[offset + 0U] = point.x * point.distance;
    vertices[offset + 1U] = point.y * point.distance;
    vertices[offset + 2U] = point.z * point.distance;
    vertices[offset + 3U] = point.distance;
    vertices[offset + 4U] = 0.0f;
    vertices[offset + 5U] = u;
    vertices[offset + 6U] = v;
    vertices[offset + 7U] = static_cast<float>(red) / 255.0f;
    vertices[offset + 8U] = static_cast<float>(green) / 255.0f;
    vertices[offset + 9U] = static_cast<float>(blue) / 255.0f;
    vertices[offset + 10U] = static_cast<float>(alpha) / 255.0f;
}

void SetProjectedShadeVertex(float *vertices, size_t index,
                             const WorldProjectedPoint &point,
                             uint8_t red, uint8_t green, uint8_t blue,
                             uint8_t alpha) {
    const size_t offset = index * kShadeVertexStride;
    vertices[offset + 0U] = point.x * point.distance;
    vertices[offset + 1U] = point.y * point.distance;
    vertices[offset + 2U] = point.z * point.distance;
    vertices[offset + 3U] = point.distance;
    vertices[offset + 4U] = 0.0f;
    vertices[offset + 5U] = static_cast<float>(red) / 255.0f;
    vertices[offset + 6U] = static_cast<float>(green) / 255.0f;
    vertices[offset + 7U] = static_cast<float>(blue) / 255.0f;
    vertices[offset + 8U] = static_cast<float>(alpha) / 255.0f;
}

bool AppendProjectedTriangle(float *vertices, size_t *vertexCount,
                             const PBWorldTriangle &triangle,
                             const WorldCamera &camera, bool textured) {
    if (vertices == nullptr || vertexCount == nullptr ||
        *vertexCount > kWorldVertexCapacity - 3U) {
        return false;
    }
    WorldProjectedPoint projected[3];
    for (size_t index = 0U; index < 3U; index++) {
        if (!ProjectWorldPoint(camera, triangle.vertices[index].position,
                               &projected[index])) {
            return true;
        }
    }
    for (size_t index = 0U; index < 3U; index++) {
        const PBWorldVertex &source = triangle.vertices[index];
        if (textured) {
            SetProjectedTexturedVertex(vertices, (*vertexCount)++,
                                       projected[index], source.u, source.v,
                                       source.red, source.green, source.blue,
                                       source.alpha);
        } else {
            SetProjectedShadeVertex(vertices, (*vertexCount)++,
                                    projected[index], source.red,
                                    source.green, source.blue, source.alpha);
        }
    }
    return true;
}

bool BuildProjectedBillboard(
    std::array<float, kFirstFrameVertexStride * kFirstFrameVertexCount>
        &vertices,
    const WorldCamera &camera, PBWorldVec3 center, float worldWidth,
    float worldHeight, const WorldGpuTexture &texture, bool flipHorizontal,
    float red = 1.0f, float green = 1.0f, float blue = 1.0f,
    float alpha = 1.0f) {
    WorldProjectedPoint projected;
    if (texture.id == 0U || texture.textureWidth == 0U ||
        texture.textureHeight == 0U || texture.sourceWidth == 0U ||
        texture.sourceHeight == 0U ||
        !ProjectWorldPoint(camera, center, &projected)) {
        return false;
    }
    const float height = worldHeight * camera.focal / projected.distance;
    const float width = worldWidth * camera.focal / projected.distance;
    const float left = projected.x - width * 0.5f;
    const float right = projected.x + width * 0.5f;
    const float bottom = projected.y - height * 0.5f;
    const float top = projected.y + height * 0.5f;
    const float minimumU = 0.0f;
    const float maximumU = static_cast<float>(texture.sourceWidth) /
                           static_cast<float>(texture.textureWidth);
    const float minimumV = 0.0f;
    const float maximumV = static_cast<float>(texture.sourceHeight) /
                           static_cast<float>(texture.textureHeight);
    const float leftU = flipHorizontal ? maximumU : minimumU;
    const float rightU = flipHorizontal ? minimumU : maximumU;
    projected.z = std::min(0.999f, projected.z + 0.0005f);
    const uint8_t redByte = static_cast<uint8_t>(
        std::max(0.0f, std::min(1.0f, red)) * 255.0f);
    const uint8_t greenByte = static_cast<uint8_t>(
        std::max(0.0f, std::min(1.0f, green)) * 255.0f);
    const uint8_t blueByte = static_cast<uint8_t>(
        std::max(0.0f, std::min(1.0f, blue)) * 255.0f);
    const uint8_t alphaByte = static_cast<uint8_t>(
        std::max(0.0f, std::min(1.0f, alpha)) * 255.0f);
    const auto set = [&](size_t index, float x, float y, float u, float v) {
        WorldProjectedPoint point = projected;
        point.x = x;
        point.y = y;
        SetProjectedTexturedVertex(vertices.data(), index, point, u, v,
                                   redByte, greenByte, blueByte, alphaByte);
    };
    set(0U, left, bottom, leftU, minimumV);
    set(1U, right, bottom, rightU, minimumV);
    set(2U, right, top, rightU, maximumV);
    set(3U, right, top, rightU, maximumV);
    set(4U, left, top, leftU, maximumV);
    set(5U, left, bottom, leftU, minimumV);
    return true;
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
    Fast::ShaderProgram *worldTextureShader = nullptr;
    Fast::ShaderProgram *worldShadeShader = nullptr;
    Fast::FilteringMode filterMode = Fast::FILTER_THREE_POINT;
    PBRenderPipeline pipeline = {
        PB_CULL_NONE, false, false, PB_COMPARE_GREATER_EQUAL,
        PB_BLEND_DISABLED, false, PB_COMPARE_GREATER, 0,
        PB_FILTER_NEAREST, PB_FILTER_NEAREST, PB_WRAP_REPEAT, PB_WRAP_REPEAT,
    };
    uint32_t diagnosticTexture = 0;
    uint32_t firstFrameTexture = 0;
    uint32_t titleLogoTexture = 0;
    uint32_t titlePromptTexture = 0;
    uint32_t titleCopyrightTexture = 0;
    uint32_t worldBackgroundTexture = 0;
    std::array<WorldGpuTexture, PB_WORLD_MAX_TEXTURES> worldMapTextures = {};
    size_t worldMapTextureCount = 0U;
    WorldGpuTexture worldPlayerTextures[PB_WORLD_PLAYER_FRAME_COUNT] = {};
    WorldGpuTexture worldStarTexture = {};
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
    std::array<float, kFirstFrameVertexStride * kFirstFrameVertexCount>
        worldBackgroundVertices = {};
    std::array<float, kShadeVertexStride * kFirstFrameVertexCount>
        worldPauseVertices = {};
    std::array<float, kFirstFrameVertexStride * kFirstFrameVertexCount>
        worldSpriteVertices = {};
    std::array<float, kShadeVertexStride * 24U> worldOverlayVertices = {};
    std::unique_ptr<float[]> worldVertices;
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
    DestroyRuntimeRenderer();
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
    mImpl->worldTextureShader = nullptr;
    mImpl->worldShadeShader = nullptr;
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

void GfxRenderingAPI3DS::ConfigureRuntimePipeline(
    bool depthTest, bool depthWrite, bool decal, int8_t cullKeepSign,
    bool useAlpha, bool alphaTest, uint8_t alphaReference) {
    if (mImpl == nullptr) return;
    mImpl->zmodeDecal = decal;
    mImpl->pipeline.depth_test_enabled = depthTest;
    mImpl->pipeline.depth_write_enabled = depthWrite;
    mImpl->pipeline.depth_function = decal
        ? (mImpl->strictDecal ? PB_COMPARE_EQUAL : PB_COMPARE_GREATER_EQUAL)
        : PB_COMPARE_GREATER_EQUAL;
    mCurrentCullKeepSign = cullKeepSign;
    mImpl->pipeline.cull_mode = cullKeepSign > 0
                                    ? PB_CULL_BACK_CCW
                                    : (cullKeepSign < 0
                                           ? PB_CULL_FRONT_CCW
                                           : PB_CULL_NONE);
    mImpl->pipeline.blend_mode =
        useAlpha ? PB_BLEND_ALPHA : PB_BLEND_DISABLED;
    mImpl->pipeline.alpha_test_enabled = alphaTest;
    mImpl->pipeline.alpha_function = PB_COMPARE_GREATER;
    mImpl->pipeline.alpha_reference = alphaReference;
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

void GfxRenderingAPI3DS::PreserveColorOnNextFrame(bool preserve) {
    if (mImpl != nullptr) {
        NativePreserveColor(mImpl->renderer, preserve);
    }
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
    std::array<uint8_t, 128U * 32U * 4U> tintedPrompt = {};
    std::copy_n(assets->prompt.rgba, tintedPrompt.size(),
                tintedPrompt.data());
    if (!pb_title_prompt_bake_tint(tintedPrompt.data(),
                                   tintedPrompt.size())) {
        return false;
    }
    PBDecodedTexture prompt = assets->prompt;
    prompt.rgba = tintedPrompt.data();
    if (!uploadTexture(&mImpl->titleLogoTexture, assets->logo) ||
        !uploadTexture(&mImpl->titlePromptTexture, prompt) ||
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
                           assets->prompt.source_height) ||
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

bool GfxRenderingAPI3DS::PrepareWorldBackground(
    const uint8_t *rgba, uint16_t textureWidth, uint16_t textureHeight,
    uint16_t sourceWidth, uint16_t sourceHeight) {
    if (mImpl == nullptr || rgba == nullptr || sourceWidth == 0U ||
        sourceHeight == 0U || sourceWidth > textureWidth ||
        sourceHeight > textureHeight ||
        sourceWidth != PB_TITLE_BACKGROUND_WIDTH ||
        sourceHeight != PB_TITLE_BACKGROUND_HEIGHT ||
        !pb_title_layout_compute(&mImpl->titleLayout, PB_RENDER_TOP_WIDTH,
                                 PB_RENDER_TOP_HEIGHT)) {
        return false;
    }

    Init();
    mImpl->firstFrameShader =
        CreateAndLoadNewShader(kTextureShadeShader, kAlphaOption);
    mImpl->titleShadeShader =
        CreateAndLoadNewShader(kShadeShader, kAlphaOption);
    if (mImpl->firstFrameShader == nullptr ||
        mImpl->titleShadeShader == nullptr ||
        !mImpl->firstFrameShader->plan.supported ||
        !mImpl->titleShadeShader->plan.supported) {
        return false;
    }

    const uint32_t candidate = NewTexture();
    if (candidate == 0U) {
        return false;
    }
    SelectTexture(0, candidate);
    UploadTexture(rgba, textureWidth, textureHeight);
    SetTextureFilter(Fast::FILTER_THREE_POINT);
    SetSamplerParameters(0, false, 2U, 2U);
    const PBGfxTextureRecord *record =
        pb_gfx_bridge_find_texture(&mImpl->bridge, candidate);
    if (record == nullptr || !record->uploaded ||
        !BuildTexturedQuad(
            mImpl->worldBackgroundVertices,
            mImpl->titleLayout.background.left,
            mImpl->titleLayout.background.bottom,
            mImpl->titleLayout.background.width,
            mImpl->titleLayout.background.height,
            textureWidth, textureHeight, sourceWidth, sourceHeight)) {
        DeleteTexture(candidate);
        return false;
    }

    size_t pauseVertex = 0U;
    AppendShadeQuad(mImpl->worldPauseVertices.data(), &pauseVertex,
                    0.0f, 0.0f,
                    static_cast<float>(PB_RENDER_TOP_WIDTH),
                    static_cast<float>(PB_RENDER_TOP_HEIGHT),
                    0.0f, 0.0f, 0.0f, 0.48f);
    if (mImpl->worldBackgroundTexture != 0U) {
        DeleteTexture(mImpl->worldBackgroundTexture);
    }
    mImpl->worldBackgroundTexture = candidate;
    return true;
}

bool GfxRenderingAPI3DS::RenderWorldBackground(bool paused) {
    if (mImpl == nullptr || mImpl->firstFrameShader == nullptr ||
        mImpl->titleShadeShader == nullptr ||
        mImpl->worldBackgroundTexture == 0U) {
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
    SelectTexture(0, mImpl->worldBackgroundTexture);
    SetUseAlpha(false);
    DrawTriangles(mImpl->worldBackgroundVertices.data(),
                  mImpl->worldBackgroundVertices.size(), 2U);
    if (paused) {
        LoadShader(mImpl->titleShadeShader);
        SetUseAlpha(true);
        DrawTriangles(mImpl->worldPauseVertices.data(),
                      mImpl->worldPauseVertices.size(), 2U);
    }
    EndFrame();
    return mImpl->bridge.stats.frames_presented == previousFrames + 1U;
}

bool GfxRenderingAPI3DS::PrepareWorldScene(const PBWorldScene *scene) {
    if (mImpl == nullptr || scene == nullptr ||
        scene->result != PB_WORLD_SCENE_READY || scene->pixels_released ||
        scene->triangles == nullptr || scene->stats.triangles == 0U ||
        scene->texture_count == 0U ||
        scene->texture_count > PB_WORLD_MAX_TEXTURES ||
        scene->background.rgba == nullptr ||
        scene->player_frames[0].rgba == nullptr ||
        scene->player_frames[1].rgba == nullptr) {
        return false;
    }

    Init();
    mImpl->worldTextureShader =
        CreateAndLoadNewShader(kTextureShadeShader, kAlphaOption);
    mImpl->worldShadeShader =
        CreateAndLoadNewShader(kShadeShader, kAlphaOption);
    if (mImpl->worldTextureShader == nullptr ||
        mImpl->worldShadeShader == nullptr ||
        !mImpl->worldTextureShader->plan.supported ||
        !mImpl->worldShadeShader->plan.supported) {
        return false;
    }
    if (!mImpl->worldVertices) {
        mImpl->worldVertices.reset(
            new (std::nothrow) float[kWorldFloatCapacity]);
        if (!mImpl->worldVertices) {
            return false;
        }
    }

    const auto discardWorldTextures = [this]() {
        if (mImpl->worldBackgroundTexture != 0U) {
            DeleteTexture(mImpl->worldBackgroundTexture);
            mImpl->worldBackgroundTexture = 0U;
        }
        for (WorldGpuTexture &texture : mImpl->worldMapTextures) {
            if (texture.id != 0U) {
                DeleteTexture(texture.id);
            }
            texture = {};
        }
        mImpl->worldMapTextureCount = 0U;
        for (WorldGpuTexture &texture : mImpl->worldPlayerTextures) {
            if (texture.id != 0U) {
                DeleteTexture(texture.id);
            }
            texture = {};
        }
        if (mImpl->worldStarTexture.id != 0U) {
            DeleteTexture(mImpl->worldStarTexture.id);
        }
        mImpl->worldStarTexture = {};
    };
    discardWorldTextures();

    const auto upload = [this](const PBDecodedTexture &decoded,
                               bool clamp,
                               WorldGpuTexture *destination) {
        if (destination == nullptr || decoded.rgba == nullptr ||
            decoded.texture_width == 0U || decoded.texture_height == 0U ||
            decoded.source_width == 0U || decoded.source_height == 0U ||
            decoded.rgba_size != static_cast<size_t>(decoded.texture_width) *
                                     decoded.texture_height * 4U) {
            return false;
        }
        WorldGpuTexture candidate = {};
        candidate.id = NewTexture();
        if (candidate.id == 0U) {
            return false;
        }
        SelectTexture(0, candidate.id);
        UploadTexture(decoded.rgba, decoded.texture_width,
                      decoded.texture_height);
        SetTextureFilter(Fast::FILTER_LINEAR);
        SetSamplerParameters(0, true, clamp ? 2U : 0U,
                             clamp ? 2U : 0U);
        const PBGfxTextureRecord *record = pb_gfx_bridge_find_texture(
            &mImpl->bridge, candidate.id);
        if (record == nullptr || !record->uploaded) {
            DeleteTexture(candidate.id);
            return false;
        }
        candidate.textureWidth = decoded.texture_width;
        candidate.textureHeight = decoded.texture_height;
        candidate.sourceWidth = decoded.source_width;
        candidate.sourceHeight = decoded.source_height;
        *destination = candidate;
        return true;
    };

    WorldGpuTexture background = {};
    if (!upload(scene->background, true, &background)) {
        discardWorldTextures();
        return false;
    }
    mImpl->worldBackgroundTexture = background.id;
    if (!pb_title_layout_compute(&mImpl->titleLayout, PB_RENDER_TOP_WIDTH,
                                 PB_RENDER_TOP_HEIGHT) ||
        !BuildTexturedQuad(
            mImpl->worldBackgroundVertices,
            mImpl->titleLayout.background.left,
            mImpl->titleLayout.background.bottom,
            mImpl->titleLayout.background.width,
            mImpl->titleLayout.background.height,
            background.textureWidth, background.textureHeight,
            background.sourceWidth, background.sourceHeight)) {
        discardWorldTextures();
        return false;
    }

    for (uint16_t index = 0U; index < scene->texture_count; index++) {
        if (!upload(scene->textures[index].decoded, false,
                    &mImpl->worldMapTextures[index])) {
            discardWorldTextures();
            return false;
        }
        mImpl->worldMapTextureCount++;
    }
    for (size_t index = 0U; index < PB_WORLD_PLAYER_FRAME_COUNT; index++) {
        if (!upload(scene->player_frames[index], true,
                    &mImpl->worldPlayerTextures[index])) {
            discardWorldTextures();
            return false;
        }
    }
    if (scene->star_piece.rgba != nullptr &&
        !upload(scene->star_piece, true, &mImpl->worldStarTexture)) {
        discardWorldTextures();
        return false;
    }

    size_t pauseVertex = 0U;
    AppendShadeQuad(mImpl->worldPauseVertices.data(), &pauseVertex,
                    0.0f, 0.0f,
                    static_cast<float>(PB_RENDER_TOP_WIDTH),
                    static_cast<float>(PB_RENDER_TOP_HEIGHT),
                    0.0f, 0.0f, 0.0f, 0.48f);
    return true;
}

bool GfxRenderingAPI3DS::RenderWorldScene(const PBWorldScene *scene,
                                          bool paused) {
    if (mImpl == nullptr || scene == nullptr ||
        scene->result != PB_WORLD_SCENE_READY ||
        scene->triangles == nullptr || mImpl->worldTextureShader == nullptr ||
        mImpl->worldShadeShader == nullptr ||
        mImpl->worldBackgroundTexture == 0U ||
        mImpl->worldMapTextureCount != scene->texture_count ||
        !mImpl->worldVertices) {
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

    LoadShader(mImpl->worldTextureShader);
    SelectTexture(0, mImpl->worldBackgroundTexture);
    SetUseAlpha(false);
    DrawTriangles(mImpl->worldBackgroundVertices.data(),
                  mImpl->worldBackgroundVertices.size(), 2U);

    const WorldCamera camera = BuildWorldCamera(*scene);
    for (uint8_t renderClass = PB_WORLD_RENDER_OPAQUE;
         renderClass <= PB_WORLD_RENDER_TRANSLUCENT; renderClass++) {
        const bool opaque = renderClass == PB_WORLD_RENDER_OPAQUE;
        SetDepthTestAndMask(true, opaque);
        SetUseAlpha(!opaque);
        for (int textureIndex = -1;
             textureIndex < static_cast<int>(scene->texture_count);
             textureIndex++) {
            size_t vertexCount = 0U;
            for (uint32_t triangleIndex = 0U;
                 triangleIndex < scene->stats.triangles; triangleIndex++) {
                const PBWorldTriangle &triangle =
                    scene->triangles[triangleIndex];
                if (triangle.render_class != renderClass ||
                    triangle.texture_index != textureIndex) {
                    continue;
                }
                if (!AppendProjectedTriangle(
                        mImpl->worldVertices.get(), &vertexCount, triangle,
                        camera, textureIndex >= 0)) {
                    EndFrame();
                    return false;
                }
            }
            if (vertexCount == 0U) {
                continue;
            }
            if (textureIndex >= 0) {
                LoadShader(mImpl->worldTextureShader);
                SelectTexture(
                    0, mImpl->worldMapTextures
                           [static_cast<size_t>(textureIndex)]
                               .id);
                DrawTriangles(
                    mImpl->worldVertices.get(),
                    vertexCount * kFirstFrameVertexStride,
                    vertexCount / 3U);
            } else {
                LoadShader(mImpl->worldShadeShader);
                DrawTriangles(mImpl->worldVertices.get(),
                              vertexCount * kShadeVertexStride,
                              vertexCount / 3U);
            }
        }
    }

    /* Actors must remain readable over their contact floor in this 2D pass. */
    SetDepthTestAndMask(false, false);
    SetUseAlpha(true);
    LoadShader(mImpl->worldTextureShader);
    if (scene->star_piece_active && mImpl->worldStarTexture.id != 0U) {
        PBWorldVec3 center = {
            -420.0f,
            36.0f + std::sin(static_cast<float>(scene->frames) * 0.12f) *
                        5.0f,
            410.0f,
        };
        if (BuildProjectedBillboard(mImpl->worldSpriteVertices, camera,
                                    center, 32.0f, 32.0f,
                                    mImpl->worldStarTexture, false)) {
            SelectTexture(0, mImpl->worldStarTexture.id);
            DrawTriangles(mImpl->worldSpriteVertices.data(),
                          mImpl->worldSpriteVertices.size(), 2U);
        }
    }

    const size_t playerFrame =
        scene->player_frame < PB_WORLD_PLAYER_FRAME_COUNT
            ? scene->player_frame
            : 0U;
    PBWorldVec3 playerCenter = scene->player_position;
    playerCenter.y += 28.0f;
    if (BuildProjectedBillboard(
            mImpl->worldSpriteVertices, camera, playerCenter, 32.0f, 56.0f,
            mImpl->worldPlayerTextures[playerFrame],
            scene->player_facing_left)) {
        SelectTexture(0, mImpl->worldPlayerTextures[playerFrame].id);
        DrawTriangles(mImpl->worldSpriteVertices.data(),
                      mImpl->worldSpriteVertices.size(), 2U);
    }

    size_t overlayVertex = 0U;
    if (scene->message_timer != 0U) {
        AppendShadeQuad(mImpl->worldOverlayVertices.data(), &overlayVertex,
                        35.0f, 18.0f, 330.0f, 58.0f,
                        0.95f, 0.82f, 0.45f, 0.98f);
        AppendShadeQuad(mImpl->worldOverlayVertices.data(), &overlayVertex,
                        39.0f, 22.0f, 322.0f, 50.0f,
                        0.03f, 0.05f, 0.12f, 0.96f);
    }
    if (paused) {
        AppendShadeQuad(mImpl->worldOverlayVertices.data(), &overlayVertex,
                        0.0f, 0.0f,
                        static_cast<float>(PB_RENDER_TOP_WIDTH),
                        static_cast<float>(PB_RENDER_TOP_HEIGHT),
                        0.0f, 0.0f, 0.0f, 0.48f);
    }
    const float fade = WorldFadeAlpha(*scene);
    if (fade > 0.001f) {
        AppendShadeQuad(mImpl->worldOverlayVertices.data(), &overlayVertex,
                        0.0f, 0.0f,
                        static_cast<float>(PB_RENDER_TOP_WIDTH),
                        static_cast<float>(PB_RENDER_TOP_HEIGHT),
                        0.0f, 0.0f, 0.0f, std::min(1.0f, fade));
    }
    if (overlayVertex != 0U) {
        SetDepthTestAndMask(false, false);
        SetUseAlpha(true);
        LoadShader(mImpl->worldShadeShader);
        DrawTriangles(mImpl->worldOverlayVertices.data(),
                      overlayVertex * kShadeVertexStride,
                      overlayVertex / 3U);
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

extern "C" bool pb_gfx_api_3ds_prepare_world_background(
    PBGfxApi3DS *api, const uint8_t *rgba, uint16_t textureWidth,
    uint16_t textureHeight, uint16_t sourceWidth, uint16_t sourceHeight) {
    return api != nullptr && api->implementation != nullptr &&
           api->implementation->PrepareWorldBackground(
               rgba, textureWidth, textureHeight, sourceWidth, sourceHeight);
}

extern "C" bool pb_gfx_api_3ds_render_world_background(
    PBGfxApi3DS *api, bool paused) {
    return api != nullptr && api->implementation != nullptr &&
           api->implementation->RenderWorldBackground(paused);
}

extern "C" bool pb_gfx_api_3ds_prepare_world_scene(
    PBGfxApi3DS *api, const PBWorldScene *scene) {
    return api != nullptr && api->implementation != nullptr &&
           api->implementation->PrepareWorldScene(scene);
}

extern "C" bool pb_gfx_api_3ds_render_world_scene(
    PBGfxApi3DS *api, const PBWorldScene *scene, bool paused) {
    return api != nullptr && api->implementation != nullptr &&
           api->implementation->RenderWorldScene(scene, paused);
}

extern "C" bool pb_gfx_api_3ds_render_display_list(
    PBGfxApi3DS *api, const PBRuntimeGfx *displayList) {
    return api != nullptr && api->implementation != nullptr &&
           api->implementation->RenderDisplayList(displayList);
}

extern "C" void pb_gfx_api_3ds_invalidate_texture(
    PBGfxApi3DS *api, const void *address) {
    if (api != nullptr && api->implementation != nullptr) {
        api->implementation->InvalidateRuntimeTexture(address);
    }
}

extern "C" void pb_gfx_api_3ds_clear_depth(PBGfxApi3DS *api) {
    if (api != nullptr && api->implementation != nullptr) {
        api->implementation->ClearRuntimeDepth();
    }
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

extern "C" const PBRuntimeGfxStats *pb_gfx_api_3ds_runtime_stats(
    const PBGfxApi3DS *api) {
    return api != nullptr && api->implementation != nullptr
               ? api->implementation->GetRuntimeStats()
               : nullptr;
}

extern "C" void pb_gfx_api_3ds_destroy(PBGfxApi3DS *api) {
    if (api == nullptr) {
        return;
    }
    delete api->implementation;
    delete api;
}
