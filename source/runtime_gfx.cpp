#include "pb3ds/gfx_rendering_api_3ds.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#ifndef __3DS__
#include <cstdio>
#endif
#include <limits>
#include <new>
#include <vector>

#include "pb3ds/runtime_resources.h"
#include "pb3ds/texture.h"

namespace {

constexpr uint8_t G_VTX = 0x01;
constexpr uint8_t G_MODIFYVTX = 0x02;
constexpr uint8_t G_TRI1 = 0x05;
constexpr uint8_t G_TRI2 = 0x06;
constexpr uint8_t G_QUAD = 0x07;
constexpr uint8_t G_SETTIMG_OTR_HASH = 0x20;
constexpr uint8_t G_VTX_OTR_FILEPATH = 0x24;
constexpr uint8_t G_SETTIMG_OTR_FILEPATH = 0x25;
constexpr uint8_t G_TRI1_OTR = 0x26;
constexpr uint8_t G_DL_OTR_FILEPATH = 0x27;
constexpr uint8_t G_MTX_OTR_FILEPATH = 0x29;
constexpr uint8_t G_INVAL_TEX_BY_PAL = 0x2A;
constexpr uint8_t G_DL_OTR_HASH = 0x31;
constexpr uint8_t G_VTX_OTR_HASH = 0x32;
constexpr uint8_t G_MARKER = 0x33;
constexpr uint8_t G_INVALTEXCACHE = 0x34;
constexpr uint8_t G_BRANCH_Z_OTR = 0x35;
constexpr uint8_t G_MTX_OTR = 0x36;
constexpr uint8_t G_TEXRECT_WIDE = 0x37;
constexpr uint8_t G_FILLWIDERECT = 0x38;
constexpr uint8_t G_COPYFB = 0x3B;
constexpr uint8_t G_IMAGERECT = 0x3C;
constexpr uint8_t G_DL_INDEX = 0x3D;
constexpr uint8_t G_SETTIMG_PAL = 0x41;
constexpr uint8_t G_MOVEMEM_HASH = 0x42;
constexpr uint8_t G_PUSH_SHADER = 0x43;
constexpr uint8_t G_POP_SHADER = 0x44;
constexpr uint8_t G_SETTILESIZE_INTERP = 0x45;
constexpr uint8_t G_SETTARGETINTERPINDEX = 0x46;
constexpr uint8_t G_LOADBLOCK_WIDE = 0x47;
constexpr uint8_t G_VTX_WIDE = 0x48;
constexpr uint8_t G_TRI1_WIDE = 0x49;
constexpr uint8_t G_SETTILESIZE_LERP = 0x4A;
constexpr uint8_t G_SET_STRICT_DECAL = 0x4B;
constexpr uint8_t G_SETUNIFORM = 0x4C;
constexpr uint8_t G_SETTILESCROLL_INTERP = 0x4D;

constexpr uint8_t G_TEXTURE = 0xD7;
constexpr uint8_t G_POPMTX = 0xD8;
constexpr uint8_t G_GEOMETRYMODE = 0xD9;
constexpr uint8_t G_MTX = 0xDA;
constexpr uint8_t G_MOVEWORD = 0xDB;
constexpr uint8_t G_MOVEMEM = 0xDC;
constexpr uint8_t G_DL = 0xDE;
constexpr uint8_t G_ENDDL = 0xDF;
constexpr uint8_t G_SETOTHERMODE_L = 0xE2;
constexpr uint8_t G_SETOTHERMODE_H = 0xE3;
constexpr uint8_t G_TEXRECT = 0xE4;
constexpr uint8_t G_TEXRECTFLIP = 0xE5;
constexpr uint8_t G_RDPLOADSYNC = 0xE6;
constexpr uint8_t G_RDPPIPESYNC = 0xE7;
constexpr uint8_t G_RDPTILESYNC = 0xE8;
constexpr uint8_t G_RDPFULLSYNC = 0xE9;
constexpr uint8_t G_SETSCISSOR = 0xED;
constexpr uint8_t G_SETPRIMDEPTH = 0xEE;
constexpr uint8_t G_RDPSETOTHERMODE = 0xEF;
constexpr uint8_t G_LOADTLUT = 0xF0;
constexpr uint8_t G_SETTILESIZE = 0xF2;
constexpr uint8_t G_LOADBLOCK = 0xF3;
constexpr uint8_t G_LOADTILE = 0xF4;
constexpr uint8_t G_SETTILE = 0xF5;
constexpr uint8_t G_FILLRECT = 0xF6;
constexpr uint8_t G_SETFILLCOLOR = 0xF7;
constexpr uint8_t G_SETFOGCOLOR = 0xF8;
constexpr uint8_t G_SETBLENDCOLOR = 0xF9;
constexpr uint8_t G_SETPRIMCOLOR = 0xFA;
constexpr uint8_t G_SETENVCOLOR = 0xFB;
constexpr uint8_t G_SETCOMBINE = 0xFC;
constexpr uint8_t G_SETTIMG = 0xFD;
constexpr uint8_t G_SETZIMG = 0xFE;
constexpr uint8_t G_SETCIMG = 0xFF;

constexpr uint32_t G_ZBUFFER = 0x00000001U;
constexpr uint32_t G_FOG = 0x00010000U;
constexpr uint32_t G_LIGHTING = 0x00020000U;
constexpr uint32_t G_CULL_FRONT = 0x00000200U;
constexpr uint32_t G_CULL_BACK = 0x00000400U;
constexpr uint32_t G_CULL_BOTH = G_CULL_FRONT | G_CULL_BACK;
constexpr uint32_t Z_CMP = 0x10U;
constexpr uint32_t Z_UPD = 0x20U;
constexpr uint32_t ZMODE_DEC = 0xC00U;
constexpr uint32_t FORCE_BL = 0x4000U;
constexpr uint32_t G_ZS_PRIM = 1U << 2U;
constexpr uint32_t G_CYCLE_TYPE_MASK = 3U << 20U;
constexpr uint32_t G_CYCLE_COPY = 2U << 20U;
constexpr uint32_t G_CYCLE_FILL = 3U << 20U;
constexpr uint8_t G_MW_NUMLIGHT = 0x02;
constexpr uint8_t G_MW_SEGMENT = 0x06;
constexpr uint8_t G_MW_FOG = 0x08;
constexpr uint8_t G_MV_VIEWPORT = 0x08;
constexpr uint8_t G_MV_LIGHT = 0x0A;
constexpr uint8_t G_MWO_POINT_ST = 0x14;

constexpr size_t kMaxVertices = 80U;
constexpr size_t kMaxMatrixStack = 12U;
constexpr size_t kBatchTriangleLimit = 384U;
constexpr size_t kRuntimeTextureLimit = PB_GFX_MAX_TEXTURES - 12U;
/* Normal Toad Town frames are below 10k commands.  Bound malformed/custom
 * lists so a device reports a failed frame instead of appearing frozen. */
constexpr size_t kCommandBudget = 250000U;
constexpr unsigned int kCallDepthLimit = 48U;
constexpr float kScreenInset = 40.0f;

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

struct N64Vertex {
    int16_t position[3];
    uint16_t flag;
    int16_t texture[2];
    uint8_t color[4];
};
static_assert(sizeof(N64Vertex) == 16U, "N64 vertex ABI changed");

struct N64Viewport {
    int16_t scale[4];
    int16_t translate[4];
};

struct N64Light {
    uint8_t color[3];
    uint8_t pad1;
    uint8_t colorCopy[3];
    uint8_t pad2;
    int8_t direction[3];
    uint8_t pad3;
    uint8_t alignment[4];
};
static_assert(sizeof(N64Light) == 16U, "N64 light ABI changed");

struct Matrix {
    float value[4][4] = {};
};

Matrix IdentityMatrix() {
    Matrix matrix = {};
    for (size_t index = 0U; index < 4U; index++) {
        matrix.value[index][index] = 1.0f;
    }
    return matrix;
}

Matrix Multiply(const Matrix &left, const Matrix &right) {
    Matrix output = {};
    for (size_t row = 0U; row < 4U; row++) {
        for (size_t column = 0U; column < 4U; column++) {
            for (size_t inner = 0U; inner < 4U; inner++) {
                output.value[row][column] +=
                    left.value[row][inner] * right.value[inner][column];
            }
        }
    }
    return output;
}

Matrix DecodeMatrix(const int32_t *address) {
    Matrix matrix = {};
    if (address == nullptr) {
        return IdentityMatrix();
    }
    for (size_t row = 0U; row < 4U; row++) {
        for (size_t column = 0U; column < 4U; column += 2U) {
            int32_t integerPart = 0;
            uint32_t fractionalPart = 0;
            std::memcpy(&integerPart,
                        &address[row * 2U + column / 2U],
                        sizeof(integerPart));
            std::memcpy(&fractionalPart,
                        &address[8U + row * 2U + column / 2U],
                        sizeof(fractionalPart));
            const int32_t first = static_cast<int32_t>(
                (static_cast<uint32_t>(integerPart) & 0xFFFF0000U) |
                (fractionalPart >> 16U));
            const int32_t second = static_cast<int32_t>(
                (static_cast<uint32_t>(integerPart) << 16U) |
                (fractionalPart & 0xFFFFU));
            matrix.value[row][column] =
                static_cast<float>(first) / 65536.0f;
            matrix.value[row][column + 1U] =
                static_cast<float>(second) / 65536.0f;
        }
    }
    return matrix;
}

uint16_t NextTextureDimension(uint32_t dimension) {
    if (dimension == 0U || dimension > PB_RENDER_TEXTURE_MAX_DIMENSION) {
        return 0U;
    }
    uint32_t result = PB_RENDER_TEXTURE_MIN_DIMENSION;
    while (result < dimension) {
        result <<= 1U;
    }
    return static_cast<uint16_t>(result);
}

uint8_t ExpandFive(uint16_t value) {
    value &= 0x1FU;
    return static_cast<uint8_t>((value << 3U) | (value >> 2U));
}

uint16_t ReadBig16(const uint8_t *bytes) {
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8U) |
                                 bytes[1]);
}

uint64_t HashCommand(const PBRuntimeGfx &command) {
    return (static_cast<uint64_t>(
                static_cast<uint32_t>(command.words.w0)) << 32U) |
           static_cast<uint32_t>(command.words.w1);
}

float Clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

} // namespace

namespace PB3DS {

class RuntimeDisplayListRenderer {
  public:
    explicit RuntimeDisplayListRenderer(GfxRenderingAPI3DS *renderingApi)
        : api(renderingApi) {
        ResetFrameState();
    }

    ~RuntimeDisplayListRenderer() { InvalidateTexture(nullptr); }

    bool Render(const PBRuntimeGfx *displayList) {
        stats.frames_started++;
        if (api == nullptr || displayList == nullptr || !PrepareShaders()) {
#ifndef __3DS__
            std::fprintf(stderr,
                         "runtime gfx: setup failed api=%p dl=%p shade=%p "
                         "texture=%p\n",
                         static_cast<void *>(api),
                         static_cast<const void *>(displayList),
                         static_cast<void *>(shadeShader),
                         static_cast<void *>(textureShader));
#endif
            return false;
        }
        ResetFrameState();
        const uint64_t previousFrame = frameSerial++;
        (void)previousFrame;
        api->StartFrame();
        api->ClearFramebuffer(true, true);
        depthClearPending = false;
        api->SetViewport(0, 0, PB_RENDER_TOP_WIDTH, PB_RENDER_TOP_HEIGHT);
        api->SetScissor(0, 0, PB_RENDER_TOP_WIDTH, PB_RENDER_TOP_HEIGHT);
        const bool interpreted = RunList(displayList, 0U);
        const bool flushed = Flush();
        api->EndFrame();
        stats.commands += commandCount;
        stats.commands_last_frame = static_cast<uint32_t>(commandCount);
        stats.commands_peak_frame = std::max(
            stats.commands_peak_frame,
            static_cast<uint32_t>(commandCount));
        if (interpreted && flushed && !malformed) {
            stats.frames_rendered++;
        } else if (malformed) {
            stats.malformed_lists++;
        }
#ifndef __3DS__
        if (!interpreted || !flushed || malformed) {
            std::fprintf(stderr,
                         "runtime gfx: rejected interpreted=%u flushed=%u "
                         "malformed=%u commands=%zu last=%02x index=%zu "
                         "depth=%u\n",
                         interpreted ? 1U : 0U, flushed ? 1U : 0U,
                         malformed ? 1U : 0U, commandCount, lastOpcode,
                         lastCommandIndex, lastDepth);
        }
#endif
        return interpreted && flushed && !malformed;
    }

    void InvalidateTexture(const void *address) {
        if (api == nullptr) {
            textures.clear();
            return;
        }
        Flush();
        for (size_t index = 0U; index < textures.size();) {
            const TextureCacheEntry &entry = textures[index];
            if (address == nullptr || address == entry.source ||
                address == entry.palette || address == entry.path) {
                api->DeleteTexture(entry.id);
                textures.erase(textures.begin() +
                               static_cast<std::ptrdiff_t>(index));
            } else {
                index++;
            }
        }
    }

    void RequestDepthClear() { depthClearPending = true; }

    const PBRuntimeGfxStats *GetStats() const { return &stats; }

  private:
    struct Color {
        uint8_t red = 255U;
        uint8_t green = 255U;
        uint8_t blue = 255U;
        uint8_t alpha = 255U;
    };

    struct LoadedVertex {
        float screenX = 0.0f;
        float screenY = 0.0f;
        float depth = 0.5f;
        float clipW = 1.0f;
        float clipZ = 0.0f;
        float objectZ = 0.0f;
        float textureS = 0.0f;
        float textureT = 0.0f;
        Color color = {};
        bool valid = false;
    };

    struct Tile {
        uint8_t format = 0U;
        uint8_t size = 0U;
        uint16_t line = 0U;
        uint16_t tmem = 0U;
        uint8_t palette = 0U;
        uint8_t clampT = 0U;
        uint8_t maskT = 0U;
        uint8_t shiftT = 0U;
        uint8_t clampS = 0U;
        uint8_t maskS = 0U;
        uint8_t shiftS = 0U;
        uint16_t upperS = 0U;
        uint16_t upperT = 0U;
        uint16_t lowerS = 0U;
        uint16_t lowerT = 0U;
    };

    struct TextureSource {
        const uint8_t *data = nullptr;
        const char *path = nullptr;
        uint16_t resourceWidth = 0U;
        uint16_t resourceHeight = 0U;
        uint16_t imageWidth = 0U;
        uint8_t format = 0U;
        uint8_t size = 0U;
        uint32_t resourceType = 0U;
        size_t payloadSize = 0U;
    };

    struct TextureCacheEntry {
        const void *source = nullptr;
        const void *palette = nullptr;
        const void *path = nullptr;
        uint32_t id = 0U;
        uint32_t key = 0U;
        uint16_t sourceWidth = 0U;
        uint16_t sourceHeight = 0U;
        uint16_t textureWidth = 0U;
        uint16_t textureHeight = 0U;
        uint64_t lastUse = 0U;
    };

    struct CombinerUse {
        bool texture = true;
        bool shade = true;
        bool primitive = false;
        bool environment = false;
    };

    bool PrepareShaders() {
        if (shadeShader != nullptr && textureShader != nullptr) {
            return true;
        }
        api->Init();
        textureShader =
            api->CreateAndLoadNewShader(kTextureShadeShader, kAlphaOption);
        shadeShader = api->CreateAndLoadNewShader(kShadeShader, kAlphaOption);
        return shadeShader != nullptr && textureShader != nullptr;
    }

    void ResetFrameState() {
        projection = IdentityMatrix();
        modelView.fill(IdentityMatrix());
        modelViewTop = 0U;
        combined = Multiply(modelView[modelViewTop], projection);
        vertices = {};
        tiles = {};
        textureToLoad = {};
        loadedTextures = {};
        paletteBanks.fill(nullptr);
        paletteFull = nullptr;
        segmentPointers.fill(0U);
        lights = {};
        lightCount = 1U;
        geometryMode = 0U;
        otherModeHigh = 0U;
        otherModeLow = 0U;
        textureScaleS = UINT16_MAX;
        textureScaleT = UINT16_MAX;
        firstTile = 0U;
        viewportX = kScreenInset;
        viewportY = 0.0f;
        viewportWidth = 320.0f;
        viewportHeight = 240.0f;
        primColor = {};
        envColor = {};
        fogColor = { 0U, 0U, 0U, 0U };
        blendColor = {};
        fillColor = 0U;
        primDepth = 0.5f;
        fogMultiply = 0;
        fogOffset = 0;
        combineWord0 = 0U;
        combineWord1 = 0U;
        commandCount = 0U;
        lastOpcode = 0U;
        lastCommandIndex = 0U;
        lastDepth = 0U;
        malformed = false;
        batch.clear();
        batchTriangles = 0U;
        batchTextured = false;
        batchTexture = 0U;
        depthImageAddress = 0U;
        colorImageAddress = 0U;
        colorTargetIsDepth = false;
    }

    const void *Resolve(uintptr_t address) const {
        if ((address & 1U) != 0U && address <= UINT32_MAX) {
            const uint32_t encoded = static_cast<uint32_t>(address);
            const uint32_t segment = encoded >> 24U;
            const uint32_t offset = encoded & 0x00FFFFFEU;
            if (segment < segmentPointers.size() &&
                segmentPointers[segment] != 0U) {
                return reinterpret_cast<const void *>(
                    segmentPointers[segment] + offset);
            }
        }
        return reinterpret_cast<const void *>(address);
    }

    void RebuildCombined() {
        combined = Multiply(modelView[modelViewTop], projection);
    }

    void ApplyMatrix(uint8_t parameters, const int32_t *address) {
        if (address == nullptr) {
            malformed = true;
            return;
        }
        const Matrix decoded = DecodeMatrix(address);
        const bool projectionMatrix = (parameters & 0x04U) != 0U;
        const bool load = (parameters & 0x02U) != 0U;
        const bool push = (parameters & 0x01U) != 0U;
        if (projectionMatrix) {
            projection = load ? decoded : Multiply(decoded, projection);
        } else {
            if (push && modelViewTop + 1U < modelView.size()) {
                modelView[modelViewTop + 1U] = modelView[modelViewTop];
                modelViewTop++;
            }
            modelView[modelViewTop] =
                load ? decoded : Multiply(decoded, modelView[modelViewTop]);
        }
        RebuildCombined();
    }

    Color Illuminate(const N64Vertex &source) const {
        if ((geometryMode & G_LIGHTING) == 0U || lightCount == 0U) {
            return { source.color[0], source.color[1], source.color[2],
                     source.color[3] };
        }
        const size_t ambientIndex = std::min<size_t>(lightCount - 1U,
                                                     lights.size() - 1U);
        float red = lights[ambientIndex].color[0];
        float green = lights[ambientIndex].color[1];
        float blue = lights[ambientIndex].color[2];
        float normal[3] = {
            static_cast<float>(static_cast<int8_t>(source.color[0])) / 127.0f,
            static_cast<float>(static_cast<int8_t>(source.color[1])) / 127.0f,
            static_cast<float>(static_cast<int8_t>(source.color[2])) / 127.0f,
        };
        const float normalLength = std::sqrt(normal[0] * normal[0] +
                                             normal[1] * normal[1] +
                                             normal[2] * normal[2]);
        if (normalLength > 0.0001f) {
            for (float &component : normal) component /= normalLength;
        }
        for (size_t index = 0U; index + 1U < lightCount &&
                               index < lights.size(); index++) {
            float direction[3] = {
                static_cast<float>(lights[index].direction[0]) / 127.0f,
                static_cast<float>(lights[index].direction[1]) / 127.0f,
                static_cast<float>(lights[index].direction[2]) / 127.0f,
            };
            const float contribution = std::max(
                0.0f, normal[0] * direction[0] + normal[1] * direction[1] +
                          normal[2] * direction[2]);
            red += lights[index].color[0] * contribution;
            green += lights[index].color[1] * contribution;
            blue += lights[index].color[2] * contribution;
        }
        return {
            static_cast<uint8_t>(std::min(255.0f, red)),
            static_cast<uint8_t>(std::min(255.0f, green)),
            static_cast<uint8_t>(std::min(255.0f, blue)),
            source.color[3],
        };
    }

    void LoadVertices(const N64Vertex *source, size_t count,
                      size_t destination) {
        if (source == nullptr || destination >= vertices.size() ||
            count > vertices.size() - destination) {
            malformed = true;
            return;
        }
        for (size_t index = 0U; index < count; index++) {
            const N64Vertex &input = source[index];
            float object[4] = {
                static_cast<float>(input.position[0]),
                static_cast<float>(input.position[1]),
                static_cast<float>(input.position[2]), 1.0f,
            };
            float clip[4] = {};
            for (size_t column = 0U; column < 4U; column++) {
                for (size_t row = 0U; row < 4U; row++) {
                    clip[column] +=
                        object[row] * combined.value[row][column];
                }
            }
            LoadedVertex &output = vertices[destination + index];
            output = {};
            output.objectZ = object[2];
            output.clipZ = clip[2];
            output.clipW = clip[3];
            if (!std::isfinite(clip[3]) || std::fabs(clip[3]) < 0.0001f) {
                continue;
            }
            const float reciprocalW = 1.0f / clip[3];
            const float ndcX = clip[0] * reciprocalW;
            const float ndcY = clip[1] * reciprocalW;
            const float ndcZ = clip[2] * reciprocalW;
            output.screenX =
                viewportX + (ndcX + 1.0f) * viewportWidth * 0.5f;
            output.screenY =
                viewportY + (ndcY + 1.0f) * viewportHeight * 0.5f;
            output.depth = Clamp01(1.0f - (ndcZ * 0.5f + 0.5f));
            output.textureS = static_cast<float>(
                (static_cast<int32_t>(input.texture[0]) * textureScaleS) >>
                16);
            output.textureT = static_cast<float>(
                (static_cast<int32_t>(input.texture[1]) * textureScaleT) >>
                16);
            output.color = Illuminate(input);
            output.valid = std::isfinite(output.screenX) &&
                           std::isfinite(output.screenY) && clip[3] > 0.0f;
        }
    }

    static bool FormulaReferences(uint8_t value, uint8_t source) {
        return value == source;
    }

    CombinerUse DecodeCombinerUse() const {
        CombinerUse use = {};
        use.texture = false;
        use.shade = false;
        const uint32_t word0 = combineWord0;
        const uint32_t word1 = combineWord1;
        const uint8_t values[] = {
            static_cast<uint8_t>((word0 >> 20U) & 0xFU),
            static_cast<uint8_t>((word1 >> 28U) & 0xFU),
            static_cast<uint8_t>((word0 >> 15U) & 0x1FU),
            static_cast<uint8_t>((word1 >> 15U) & 0x7U),
            static_cast<uint8_t>((word0 >> 12U) & 0x7U),
            static_cast<uint8_t>((word1 >> 12U) & 0x7U),
            static_cast<uint8_t>((word0 >> 9U) & 0x7U),
            static_cast<uint8_t>((word1 >> 9U) & 0x7U),
            static_cast<uint8_t>((word0 >> 5U) & 0xFU),
            static_cast<uint8_t>((word1 >> 24U) & 0xFU),
            static_cast<uint8_t>(word0 & 0x1FU),
            static_cast<uint8_t>((word1 >> 6U) & 0x7U),
            static_cast<uint8_t>((word1 >> 21U) & 0x7U),
            static_cast<uint8_t>((word1 >> 3U) & 0x7U),
            static_cast<uint8_t>((word1 >> 18U) & 0x7U),
            static_cast<uint8_t>(word1 & 0x7U),
        };
        for (uint8_t value : values) {
            use.texture = use.texture || value == 1U || value == 2U ||
                          value == 8U || value == 9U;
            use.shade = use.shade || FormulaReferences(value, 4U) ||
                        FormulaReferences(value, 11U);
            use.primitive = use.primitive || FormulaReferences(value, 3U) ||
                            FormulaReferences(value, 10U);
            use.environment = use.environment || FormulaReferences(value, 5U);
        }
        return use;
    }

    Color ShadeForVertex(const LoadedVertex &vertex) const {
        const CombinerUse use = DecodeCombinerUse();
        Color output = use.shade ? vertex.color : Color{};
        const auto multiply = [&output](const Color &color) {
            output.red = static_cast<uint8_t>(
                (static_cast<unsigned int>(output.red) * color.red + 127U) /
                255U);
            output.green = static_cast<uint8_t>(
                (static_cast<unsigned int>(output.green) * color.green + 127U) /
                255U);
            output.blue = static_cast<uint8_t>(
                (static_cast<unsigned int>(output.blue) * color.blue + 127U) /
                255U);
            output.alpha = static_cast<uint8_t>(
                (static_cast<unsigned int>(output.alpha) * color.alpha +
                 127U) /
                255U);
        };
        if (use.primitive) multiply(primColor);
        if (use.environment) multiply(envColor);
        if ((geometryMode & G_FOG) != 0U) {
            const float divisor = std::fabs(vertex.clipW) < 0.001f
                                      ? 0.001f
                                      : vertex.clipW;
            const float factor = Clamp01(
                (vertex.clipZ / divisor * fogMultiply + fogOffset) / 255.0f);
            output.red = static_cast<uint8_t>(
                output.red * (1.0f - factor) + fogColor.red * factor);
            output.green = static_cast<uint8_t>(
                output.green * (1.0f - factor) + fogColor.green * factor);
            output.blue = static_cast<uint8_t>(
                output.blue * (1.0f - factor) + fogColor.blue * factor);
        }
        return output;
    }

    float ShiftTextureCoordinate(float coordinate, uint8_t shift) const {
        if (shift == 0U) return coordinate;
        if (shift <= 10U) {
            return coordinate / static_cast<float>(1U << shift);
        }
        return coordinate * static_cast<float>(1U << (16U - shift));
    }

    static uint32_t TextureTypeFor(uint8_t format, uint8_t size) {
        if (format == 0U && size == 3U) return PB_RESOURCE_TEXTURE_RGBA32;
        if (format == 0U && size == 2U) return PB_RESOURCE_TEXTURE_RGBA16;
        if (format == 2U && size == 0U) return PB_RESOURCE_TEXTURE_CI4;
        if (format == 2U && size == 1U) return PB_RESOURCE_TEXTURE_CI8;
        if (format == 3U && size == 0U) return PB_RESOURCE_TEXTURE_IA4;
        if (format == 3U && size == 1U) return PB_RESOURCE_TEXTURE_IA8;
        if (format == 3U && size == 2U) return PB_RESOURCE_TEXTURE_IA16;
        if (format == 4U && size == 0U) return PB_RESOURCE_TEXTURE_I4;
        if (format == 4U && size == 1U) return PB_RESOURCE_TEXTURE_I8;
        return PB_RESOURCE_TEXTURE_ERROR;
    }

    const uint8_t *PaletteFor(const Tile &tile, uint32_t type) const {
        if (type == PB_RESOURCE_TEXTURE_CI4) {
            return paletteBanks[tile.palette & 0xFU];
        }
        if (type == PB_RESOURCE_TEXTURE_CI8) {
            return paletteFull != nullptr ? paletteFull : paletteBanks[0];
        }
        return nullptr;
    }

    bool DecodeTexture(const TextureSource &source, const Tile &tile,
                       const uint8_t *palette, std::vector<uint8_t> *rgba,
                       uint16_t *textureWidth, uint16_t *textureHeight,
                       uint16_t *sourceWidth, uint16_t *sourceHeight,
                       uint32_t *typeOut) const {
        if (source.data == nullptr || rgba == nullptr ||
            textureWidth == nullptr || textureHeight == nullptr ||
            sourceWidth == nullptr || sourceHeight == nullptr ||
            typeOut == nullptr) {
            return false;
        }
        uint32_t width = source.resourceWidth;
        uint32_t height = source.resourceHeight;
        if (width == 0U) {
            width = tile.lowerS >= tile.upperS
                        ? ((tile.lowerS - tile.upperS) >> 2U) + 1U
                        : source.imageWidth;
        }
        if (height == 0U) {
            height = tile.lowerT >= tile.upperT
                         ? ((tile.lowerT - tile.upperT) >> 2U) + 1U
                         : 1U;
        }
        if (width == 0U) width = source.imageWidth;
        if (width == 0U || height == 0U || width > UINT16_MAX ||
            height > UINT16_MAX) {
            return false;
        }
        const uint16_t paddedWidth = NextTextureDimension(width);
        const uint16_t paddedHeight = NextTextureDimension(height);
        if (paddedWidth == 0U || paddedHeight == 0U) return false;
        const uint32_t type = source.resourceType != 0U
                                  ? source.resourceType
                                  : TextureTypeFor(tile.format, tile.size);
        if (type == PB_RESOURCE_TEXTURE_ERROR) return false;
        const size_t texels = static_cast<size_t>(width) * height;
        size_t required = 0U;
        switch (type) {
            case PB_RESOURCE_TEXTURE_RGBA32: required = texels * 4U; break;
            case PB_RESOURCE_TEXTURE_RGBA16:
            case PB_RESOURCE_TEXTURE_IA16: required = texels * 2U; break;
            case PB_RESOURCE_TEXTURE_CI4:
            case PB_RESOURCE_TEXTURE_I4:
            case PB_RESOURCE_TEXTURE_IA4: required = (texels + 1U) / 2U; break;
            case PB_RESOURCE_TEXTURE_CI8:
            case PB_RESOURCE_TEXTURE_I8:
            case PB_RESOURCE_TEXTURE_IA8: required = texels; break;
            default: return false;
        }
        if (source.payloadSize != 0U && required > source.payloadSize) {
            return false;
        }
        if ((type == PB_RESOURCE_TEXTURE_CI4 ||
             type == PB_RESOURCE_TEXTURE_CI8) && palette == nullptr) {
            return false;
        }
        rgba->assign(static_cast<size_t>(paddedWidth) * paddedHeight * 4U,
                     0U);
        const auto nibble = [&source](size_t texel) {
            const uint8_t packed = source.data[texel / 2U];
            return (texel & 1U) == 0U
                       ? static_cast<uint8_t>(packed >> 4U)
                       : static_cast<uint8_t>(packed & 0xFU);
        };
        const auto rgba16 = [](uint8_t *destination, const uint8_t *input) {
            const uint16_t color = ReadBig16(input);
            destination[0] = ExpandFive(color >> 11U);
            destination[1] = ExpandFive(color >> 6U);
            destination[2] = ExpandFive(color >> 1U);
            destination[3] = (color & 1U) != 0U ? 255U : 0U;
        };
        for (uint32_t y = 0U; y < height; y++) {
            for (uint32_t x = 0U; x < width; x++) {
                const size_t texel = static_cast<size_t>(y) * width + x;
                uint8_t *destination =
                    &(*rgba)[(static_cast<size_t>(y) * paddedWidth + x) * 4U];
                switch (type) {
                    case PB_RESOURCE_TEXTURE_RGBA32:
                        std::memcpy(destination, source.data + texel * 4U, 4U);
                        break;
                    case PB_RESOURCE_TEXTURE_RGBA16:
                        rgba16(destination, source.data + texel * 2U);
                        break;
                    case PB_RESOURCE_TEXTURE_CI4:
                        rgba16(destination,
                               palette + static_cast<size_t>(nibble(texel)) *
                                             2U);
                        break;
                    case PB_RESOURCE_TEXTURE_CI8:
                        rgba16(destination,
                               palette + static_cast<size_t>(source.data[texel]) *
                                             2U);
                        break;
                    case PB_RESOURCE_TEXTURE_I4: {
                        const uint8_t intensity =
                            static_cast<uint8_t>(nibble(texel) * 17U);
                        destination[0] = destination[1] = destination[2] =
                            intensity;
                        destination[3] = 255U;
                        break;
                    }
                    case PB_RESOURCE_TEXTURE_I8:
                        destination[0] = destination[1] = destination[2] =
                            source.data[texel];
                        destination[3] = 255U;
                        break;
                    case PB_RESOURCE_TEXTURE_IA4: {
                        const uint8_t packed = nibble(texel);
                        const uint8_t intensity = static_cast<uint8_t>(
                            ((packed >> 1U) * 255U + 3U) / 7U);
                        destination[0] = destination[1] = destination[2] =
                            intensity;
                        destination[3] =
                            (packed & 1U) != 0U ? 255U : 0U;
                        break;
                    }
                    case PB_RESOURCE_TEXTURE_IA8: {
                        const uint8_t packed = source.data[texel];
                        destination[0] = destination[1] = destination[2] =
                            static_cast<uint8_t>((packed >> 4U) * 17U);
                        destination[3] =
                            static_cast<uint8_t>((packed & 0xFU) * 17U);
                        break;
                    }
                    case PB_RESOURCE_TEXTURE_IA16:
                        destination[0] = destination[1] = destination[2] =
                            source.data[texel * 2U];
                        destination[3] = source.data[texel * 2U + 1U];
                        break;
                    default: return false;
                }
            }
        }
        *textureWidth = paddedWidth;
        *textureHeight = paddedHeight;
        *sourceWidth = static_cast<uint16_t>(width);
        *sourceHeight = static_cast<uint16_t>(height);
        *typeOut = type;
        return true;
    }

    uint32_t TextureKey(const TextureSource &source, const Tile &tile,
                        const void *palette, uint16_t width,
                        uint16_t height, uint32_t type) const {
        uint64_t key = reinterpret_cast<uintptr_t>(source.data);
        key ^= reinterpret_cast<uintptr_t>(palette) * UINT64_C(0x9E3779B1);
        key ^= static_cast<uint64_t>(width) << 5U;
        key ^= static_cast<uint64_t>(height) << 17U;
        key ^= static_cast<uint64_t>(type) << 27U;
        key ^= static_cast<uint64_t>(tile.palette) << 2U;
        key ^= key >> 32U;
        return static_cast<uint32_t>(key);
    }

    void EvictOldestTexture() {
        if (textures.empty()) return;
        auto oldest = std::min_element(
            textures.begin(), textures.end(),
            [](const TextureCacheEntry &left,
               const TextureCacheEntry &right) {
                return left.lastUse < right.lastUse;
            });
        api->DeleteTexture(oldest->id);
        textures.erase(oldest);
    }

    TextureCacheEntry *FallbackTexture() {
        for (TextureCacheEntry &entry : textures) {
            if (entry.source == this) {
                entry.lastUse = ++textureUseClock;
                return &entry;
            }
        }
        if (textures.size() >= kRuntimeTextureLimit) EvictOldestTexture();
        const uint32_t id = api->NewTexture();
        if (id == 0U) return nullptr;
        std::array<uint8_t, 8U * 8U * 4U> pixels = {};
        for (size_t y = 0U; y < 8U; y++) {
            for (size_t x = 0U; x < 8U; x++) {
                uint8_t *pixel = &pixels[(y * 8U + x) * 4U];
                const bool alternate = ((x / 2U) ^ (y / 2U)) != 0U;
                pixel[0] = 255U;
                pixel[1] = alternate ? 0U : 64U;
                pixel[2] = 255U;
                pixel[3] = 255U;
            }
        }
        api->SelectTexture(0, id);
        api->UploadTexture(pixels.data(), 8U, 8U);
        textures.push_back({ this, nullptr, nullptr, id, 0U, 8U, 8U, 8U,
                             8U, ++textureUseClock });
        return &textures.back();
    }

    TextureCacheEntry *AcquireTexture(const Tile &tile) {
        const TextureSource &source =
            loadedTextures[tile.tmem != 0U ? 1U : 0U].data != nullptr
                ? loadedTextures[tile.tmem != 0U ? 1U : 0U]
                : textureToLoad;
        uint32_t type = source.resourceType != 0U
                            ? source.resourceType
                            : TextureTypeFor(tile.format, tile.size);
        const uint8_t *palette = PaletteFor(tile, type);
        uint32_t width = source.resourceWidth;
        uint32_t height = source.resourceHeight;
        if (width == 0U && tile.lowerS >= tile.upperS) {
            width = ((tile.lowerS - tile.upperS) >> 2U) + 1U;
        }
        if (height == 0U && tile.lowerT >= tile.upperT) {
            height = ((tile.lowerT - tile.upperT) >> 2U) + 1U;
        }
        if (width == 0U) width = source.imageWidth;
        const uint32_t key = TextureKey(
            source, tile, palette, static_cast<uint16_t>(width),
            static_cast<uint16_t>(height), type);
        for (TextureCacheEntry &entry : textures) {
            if (entry.source == source.data && entry.palette == palette &&
                entry.key == key) {
                entry.lastUse = ++textureUseClock;
                return &entry;
            }
        }

        std::vector<uint8_t> rgba;
        uint16_t textureWidth = 0U, textureHeight = 0U;
        uint16_t sourceWidth = 0U, sourceHeight = 0U;
        if (!DecodeTexture(source, tile, palette, &rgba, &textureWidth,
                           &textureHeight, &sourceWidth, &sourceHeight,
                           &type)) {
            stats.texture_fallbacks++;
            return FallbackTexture();
        }
        if (textures.size() >= kRuntimeTextureLimit) EvictOldestTexture();
        uint32_t id = api->NewTexture();
        if (id == 0U) {
            EvictOldestTexture();
            id = api->NewTexture();
        }
        if (id == 0U) return nullptr;
        api->SelectTexture(0, id);
        api->UploadTexture(rgba.data(), textureWidth, textureHeight);
        textures.push_back({
            source.data, palette, source.path, id,
            TextureKey(source, tile, palette, sourceWidth, sourceHeight, type),
            sourceWidth, sourceHeight, textureWidth, textureHeight,
            ++textureUseClock,
        });
        return &textures.back();
    }

    bool BeginBatch(bool textured) {
        if (batchTriangles != 0U && batchTextured == textured) return true;
        if (!Flush()) return false;
        batchTextured = textured;
        batchTexture = 0U;
        api->SetDepthTestAndMask(
            ((geometryMode & G_ZBUFFER) != 0U ||
             (otherModeLow & G_ZS_PRIM) != 0U) &&
                (otherModeLow & Z_CMP) != 0U,
            (otherModeLow & Z_UPD) != 0U);
        api->SetZmodeDecal((otherModeLow & ZMODE_DEC) == ZMODE_DEC);
        const uint32_t cull = geometryMode & G_CULL_BOTH;
        api->SetCullMode(cull == G_CULL_FRONT
                             ? 1
                             : (cull == G_CULL_BACK ? -1 : 0));
        api->SetUseAlpha(textured || (otherModeLow & FORCE_BL) != 0U ||
                         primColor.alpha != 255U || envColor.alpha != 255U);
        if (textured) {
            Tile &tile = tiles[firstTile & 7U];
            TextureCacheEntry *texture = AcquireTexture(tile);
            if (texture == nullptr) return false;
            batchTexture = texture->id;
            api->LoadShader(textureShader);
            api->SelectTexture(0, texture->id);
            const bool linear = ((otherModeHigh >> 12U) & 3U) != 0U;
            api->SetTextureFilter(linear ? Fast::FILTER_LINEAR
                                         : Fast::FILTER_NONE);
            api->SetSamplerParameters(0, linear, tile.clampS, tile.clampT);
        } else {
            api->LoadShader(shadeShader);
        }
        return true;
    }

    bool Flush() {
        if (batchTriangles == 0U) return true;
        if (batch.empty()) return false;
        api->DrawTriangles(batch.data(), batch.size(), batchTriangles);
        batch.clear();
        batchTriangles = 0U;
        return true;
    }

    void AppendVertex(const LoadedVertex &vertex, const Tile *tile,
                      const TextureCacheEntry *texture) {
        const float clipW = std::max(0.0001f, vertex.clipW);
        batch.push_back(vertex.screenX * clipW);
        batch.push_back(vertex.screenY * clipW);
        batch.push_back(vertex.depth * clipW);
        batch.push_back(clipW);
        batch.push_back(0.0f);
        if (tile != nullptr && texture != nullptr) {
            const float s =
                ShiftTextureCoordinate(vertex.textureS / 32.0f,
                                       tile->shiftS) -
                static_cast<float>(tile->upperS) / 4.0f;
            const float t =
                ShiftTextureCoordinate(vertex.textureT / 32.0f,
                                       tile->shiftT) -
                static_cast<float>(tile->upperT) / 4.0f;
            batch.push_back(s / texture->textureWidth);
            batch.push_back(pb_renderer_n64_texture_v(
                t, texture->sourceHeight, texture->textureHeight));
        }
        const Color color = ShadeForVertex(vertex);
        batch.push_back(static_cast<float>(color.red) / 255.0f);
        batch.push_back(static_cast<float>(color.green) / 255.0f);
        batch.push_back(static_cast<float>(color.blue) / 255.0f);
        batch.push_back(static_cast<float>(color.alpha) / 255.0f);
    }

    bool EmitTriangle(uint8_t first, uint8_t second, uint8_t third) {
        if (first >= vertices.size() || second >= vertices.size() ||
            third >= vertices.size()) {
#ifndef __3DS__
            std::fprintf(stderr,
                         "runtime gfx: invalid triangle %u,%u,%u (max %zu)\n",
                         first, second, third, vertices.size());
#endif
            malformed = true;
            return false;
        }
        const LoadedVertex *triangle[] = {
            &vertices[first], &vertices[second], &vertices[third]
        };
        if (!triangle[0]->valid || !triangle[1]->valid ||
            !triangle[2]->valid) {
            return true;
        }
        const bool textured = DecodeCombinerUse().texture;
        if (!BeginBatch(textured)) return false;
        const Tile *tile = textured ? &tiles[firstTile & 7U] : nullptr;
        TextureCacheEntry *texture = nullptr;
        if (textured) {
            for (TextureCacheEntry &entry : textures) {
                if (entry.id == batchTexture) {
                    texture = &entry;
                    break;
                }
            }
            if (texture == nullptr) return false;
        }
        for (const LoadedVertex *vertex : triangle) {
            AppendVertex(*vertex, tile, texture);
        }
        batchTriangles++;
        if (batchTriangles >= kBatchTriangleLimit) return Flush();
        return true;
    }

    LoadedVertex RectangleVertex(float x, float y, float u, float v,
                                 float depth) const {
        LoadedVertex vertex = {};
        vertex.screenX = x;
        vertex.screenY = y;
        vertex.depth = depth;
        vertex.clipW = 1.0f;
        vertex.clipZ = 0.0f;
        vertex.textureS = u * 32.0f;
        vertex.textureT = v * 32.0f;
        vertex.color = {};
        vertex.valid = true;
        return vertex;
    }

    bool EmitRectangle(float left, float top, float right, float bottom,
                       float upperS, float upperT, float lowerS,
                       float lowerT, bool textured,
                       bool flipTexture = false) {
        if (right <= left || bottom <= top) return true;
        if (!Flush()) return false;
        const float screenLeft = kScreenInset + left;
        const float screenRight = kScreenInset + right;
        const float screenBottom = PB_RENDER_TOP_HEIGHT - bottom;
        const float screenTop = PB_RENDER_TOP_HEIGHT - top;
        const float depth = primDepth;
        LoadedVertex rectangle[6] = {};
        if (flipTexture) {
            rectangle[0] = RectangleVertex(screenLeft, screenBottom,
                                            lowerS, upperT, depth);
            rectangle[1] = RectangleVertex(screenRight, screenBottom,
                                            lowerS, lowerT, depth);
            rectangle[2] = RectangleVertex(screenRight, screenTop,
                                            upperS, lowerT, depth);
            rectangle[3] = rectangle[2];
            rectangle[4] = RectangleVertex(screenLeft, screenTop,
                                            upperS, upperT, depth);
            rectangle[5] = rectangle[0];
        } else {
            rectangle[0] = RectangleVertex(screenLeft, screenBottom,
                                            upperS, lowerT, depth);
            rectangle[1] = RectangleVertex(screenRight, screenBottom,
                                            lowerS, lowerT, depth);
            rectangle[2] = RectangleVertex(screenRight, screenTop,
                                            lowerS, upperT, depth);
            rectangle[3] = rectangle[2];
            rectangle[4] = RectangleVertex(screenLeft, screenTop,
                                            upperS, upperT, depth);
            rectangle[5] = rectangle[0];
        }
        if (!BeginBatch(textured)) return false;
        const Tile *tile = textured ? &tiles[firstTile & 7U] : nullptr;
        TextureCacheEntry *texture = nullptr;
        if (textured) {
            for (TextureCacheEntry &entry : textures) {
                if (entry.id == batchTexture) texture = &entry;
            }
            if (texture == nullptr) return false;
        }
        for (const LoadedVertex &vertex : rectangle) {
            AppendVertex(vertex, tile, texture);
        }
        batchTriangles += 2U;
        return Flush();
    }

    void SetTextureImage(uint32_t word0, uintptr_t address,
                         const char *path = nullptr) {
        TextureSource source = {};
        source.format = static_cast<uint8_t>((word0 >> 21U) & 7U);
        source.size = static_cast<uint8_t>((word0 >> 19U) & 3U);
        source.imageWidth = static_cast<uint16_t>((word0 & 0xFFFU) + 1U);
        if (path != nullptr) {
            source.path = path;
            source.data = static_cast<const uint8_t *>(
                ResourceGetDataByName(path));
            source.resourceWidth = ResourceGetTexWidthByName(path);
            source.resourceHeight = ResourceGetTexHeightByName(path);
            source.resourceType = pb_runtime_resource_texture_type(path);
            source.payloadSize = pb_runtime_resource_payload_size(path);
            if (source.data == nullptr) stats.missing_resources++;
        } else {
            const void *resolved = Resolve(address);
            if (resolved != nullptr &&
                GameEngine_OTRSigCheck(static_cast<const char *>(resolved))) {
                path = static_cast<const char *>(resolved);
                source.path = path;
                source.data = static_cast<const uint8_t *>(
                    ResourceGetDataByName(path));
                source.resourceWidth = ResourceGetTexWidthByName(path);
                source.resourceHeight = ResourceGetTexHeightByName(path);
                source.resourceType = pb_runtime_resource_texture_type(path);
                source.payloadSize = pb_runtime_resource_payload_size(path);
                if (source.data == nullptr) stats.missing_resources++;
            } else {
                source.data = static_cast<const uint8_t *>(resolved);
            }
        }
        textureToLoad = source;
    }

    void SetTile(uint32_t word0, uint32_t word1) {
        const size_t index = (word1 >> 24U) & 7U;
        Tile &tile = tiles[index];
        tile.format = static_cast<uint8_t>((word0 >> 21U) & 7U);
        tile.size = static_cast<uint8_t>((word0 >> 19U) & 3U);
        tile.line = static_cast<uint16_t>((word0 >> 9U) & 0x1FFU);
        tile.tmem = static_cast<uint16_t>(word0 & 0x1FFU);
        tile.palette = static_cast<uint8_t>((word1 >> 20U) & 0xFU);
        tile.clampT = static_cast<uint8_t>((word1 >> 18U) & 3U);
        tile.maskT = static_cast<uint8_t>((word1 >> 14U) & 0xFU);
        tile.shiftT = static_cast<uint8_t>((word1 >> 10U) & 0xFU);
        tile.clampS = static_cast<uint8_t>((word1 >> 8U) & 3U);
        tile.maskS = static_cast<uint8_t>((word1 >> 4U) & 0xFU);
        tile.shiftS = static_cast<uint8_t>(word1 & 0xFU);
        if (tile.clampS == 0U && tile.maskS == 0U) tile.clampS = 2U;
        if (tile.clampT == 0U && tile.maskT == 0U) tile.clampT = 2U;
    }

    void SetTileSize(uint32_t word0, uint32_t word1) {
        Tile &tile = tiles[(word1 >> 24U) & 7U];
        tile.upperS = static_cast<uint16_t>((word0 >> 12U) & 0xFFFU);
        tile.upperT = static_cast<uint16_t>(word0 & 0xFFFU);
        tile.lowerS = static_cast<uint16_t>((word1 >> 12U) & 0xFFFU);
        tile.lowerT = static_cast<uint16_t>(word1 & 0xFFFU);
    }

    void LoadTexture(size_t tileIndex) {
        if (tileIndex >= tiles.size()) return;
        const size_t tmemIndex = tiles[tileIndex].tmem != 0U ? 1U : 0U;
        loadedTextures[tmemIndex] = textureToLoad;
    }

    void LoadPalette(size_t tileIndex, size_t entries) {
        if (tileIndex >= tiles.size() || textureToLoad.data == nullptr) return;
        const Tile &tile = tiles[tileIndex];
        const size_t firstBank =
            tile.tmem >= 256U ? (tile.tmem - 256U) / 16U : 0U;
        if (entries >= 256U && firstBank == 0U) {
            paletteFull = textureToLoad.data;
        }
        for (size_t bank = firstBank;
             bank < paletteBanks.size() &&
             (bank - firstBank) * 16U < entries;
             bank++) {
            paletteBanks[bank] =
                textureToLoad.data + (bank - firstBank) * 32U;
        }
    }

    void ApplyOtherMode(uint32_t *destination, uint32_t word0,
                        uint32_t word1) {
        const uint32_t length = (word0 & 0xFFU) + 1U;
        const uint32_t shift = 31U - ((word0 >> 8U) & 0xFFU) -
                               (word0 & 0xFFU);
        const uint32_t mask = length >= 32U
                                  ? UINT32_MAX
                                  : ((UINT32_C(1) << length) - 1U) << shift;
        *destination = (*destination & ~mask) | (word1 & mask);
    }

    bool RunList(const PBRuntimeGfx *displayList, unsigned int depth) {
        if (displayList == nullptr || depth > kCallDepthLimit) {
            malformed = true;
            return false;
        }
        stats.display_lists++;
        stats.max_call_depth =
            std::max(stats.max_call_depth, static_cast<uint32_t>(depth));
        for (size_t index = 0U; index < kCommandBudget; index++) {
            if (++commandCount > kCommandBudget) {
                malformed = true;
                return false;
            }
            const PBRuntimeGfx &command = displayList[index];
            const uint32_t word0 = static_cast<uint32_t>(command.words.w0);
            const uint32_t word1 = static_cast<uint32_t>(command.words.w1);
            const uint8_t opcode = static_cast<uint8_t>(word0 >> 24U);
            lastOpcode = opcode;
            lastCommandIndex = index;
            lastDepth = depth;
            switch (opcode) {
                case G_VTX: {
                    const size_t count = (word0 >> 12U) & 0xFFU;
                    const size_t end = (word0 >> 1U) & 0x7FU;
                    const size_t destination = end >= count ? end - count : 0U;
                    LoadVertices(static_cast<const N64Vertex *>(
                                     Resolve(command.words.w1)),
                                 count, destination);
                    break;
                }
                case G_VTX_WIDE: {
                    const size_t count = (word0 >> 12U) & 0xFFU;
                    const size_t end = (word0 >> 1U) & 0x7FU;
                    LoadVertices(static_cast<const N64Vertex *>(
                                     Resolve(command.words.w1)),
                                 count, end >= count ? end - count : 0U);
                    break;
                }
                case G_VTX_OTR_FILEPATH: {
                    const char *path =
                        reinterpret_cast<const char *>(command.words.w1);
                    const PBRuntimeGfx &extra = displayList[++index];
                    commandCount++;
                    const size_t count = extra.words.w0;
                    const size_t destination = extra.words.w1 >> 16U;
                    const size_t offset = extra.words.w1 & 0xFFFFU;
                    const N64Vertex *data = static_cast<const N64Vertex *>(
                        ResourceGetDataByName(path));
                    if (data != nullptr) {
                        LoadVertices(data + offset, count, destination);
                    } else {
                        stats.missing_resources++;
                    }
                    break;
                }
                case G_VTX_OTR_HASH: {
                    const uintptr_t offset = command.words.w1;
                    const uint64_t hash = HashCommand(displayList[++index]);
                    commandCount++;
                    const size_t count = (word0 >> 12U) & 0xFFU;
                    const size_t end = (word0 >> 1U) & 0x7FU;
                    const uint8_t *data = offset > 0xFFFFFU
                                              ? reinterpret_cast<const uint8_t *>(
                                                    offset)
                                              : static_cast<const uint8_t *>(
                                                    ResourceGetDataByCrc(hash));
                    if (data != nullptr && offset <= 0xFFFFFU) data += offset;
                    if (data != nullptr) {
                        LoadVertices(reinterpret_cast<const N64Vertex *>(data),
                                     count, end >= count ? end - count : 0U);
                    } else {
                        stats.missing_resources++;
                    }
                    break;
                }
                case G_MODIFYVTX: {
                    const size_t vertex = (word0 >> 1U) & 0x7FFFU;
                    const uint8_t where = static_cast<uint8_t>(word0 >> 16U);
                    if (vertex < vertices.size() && where == G_MWO_POINT_ST) {
                        vertices[vertex].textureS =
                            static_cast<float>(static_cast<int16_t>(word1 >> 16U));
                        vertices[vertex].textureT =
                            static_cast<float>(static_cast<int16_t>(word1));
                    }
                    break;
                }
                case G_TRI1:
                    if (!EmitTriangle(
                            static_cast<uint8_t>((word0 >> 17U) & 0x7FU),
                            static_cast<uint8_t>((word0 >> 9U) & 0x7FU),
                            static_cast<uint8_t>((word0 >> 1U) & 0x7FU))) {
                        return false;
                    }
                    break;
                case G_TRI2:
                    if (!EmitTriangle(static_cast<uint8_t>((word0 >> 17U) &
                                                           0x7FU),
                                      static_cast<uint8_t>((word0 >> 9U) &
                                                           0x7FU),
                                      static_cast<uint8_t>((word0 >> 1U) &
                                                           0x7FU)) ||
                        !EmitTriangle(static_cast<uint8_t>((word1 >> 17U) &
                                                           0x7FU),
                                      static_cast<uint8_t>((word1 >> 9U) &
                                                           0x7FU),
                                      static_cast<uint8_t>((word1 >> 1U) &
                                                           0x7FU))) {
                        return false;
                    }
                    break;
                case G_QUAD:
                    if (!EmitTriangle(
                            static_cast<uint8_t>((word0 >> 17U) & 0x7FU),
                            static_cast<uint8_t>((word0 >> 9U) & 0x7FU),
                            static_cast<uint8_t>((word0 >> 1U) & 0x7FU)) ||
                        !EmitTriangle(
                            static_cast<uint8_t>((word1 >> 17U) & 0x7FU),
                            static_cast<uint8_t>((word1 >> 9U) & 0x7FU),
                            static_cast<uint8_t>((word1 >> 1U) & 0x7FU))) {
                        return false;
                    }
                    break;
                case G_TRI1_OTR:
                    if (!EmitTriangle(static_cast<uint8_t>(word0),
                                      static_cast<uint8_t>(word1 >> 16U),
                                      static_cast<uint8_t>(word1))) {
                        return false;
                    }
                    break;
                case G_TRI1_WIDE:
                    if (!EmitTriangle(static_cast<uint8_t>((word0 >> 16U) &
                                                           0xFFU),
                                      static_cast<uint8_t>((word0 >> 8U) &
                                                           0xFFU),
                                      static_cast<uint8_t>(word0 & 0xFFU))) {
                        return false;
                    }
                    break;
                case G_MTX: {
                    Flush();
                    const uint8_t parameters =
                        static_cast<uint8_t>(word0 & 0xFFU) ^ 0x01U;
                    ApplyMatrix(parameters,
                                static_cast<const int32_t *>(
                                    Resolve(command.words.w1)));
                    break;
                }
                case G_MTX_OTR_FILEPATH:
                    Flush();
                    if (const void *matrix = ResourceGetDataByName(
                            reinterpret_cast<const char *>(
                                command.words.w1));
                        matrix != nullptr) {
                        ApplyMatrix(
                            static_cast<uint8_t>(word0 & 0xFFU) ^ 0x01U,
                            static_cast<const int32_t *>(matrix));
                    } else {
                        stats.missing_resources++;
                    }
                    break;
                case G_MTX_OTR: {
                    Flush();
                    const uint64_t hash = HashCommand(displayList[++index]);
                    commandCount++;
                    if (const void *matrix = ResourceGetDataByCrc(hash);
                        matrix != nullptr) {
                        ApplyMatrix(
                            static_cast<uint8_t>(word0 & 0xFFU) ^ 0x01U,
                            static_cast<const int32_t *>(matrix));
                    } else {
                        stats.missing_resources++;
                    }
                    break;
                }
                case G_POPMTX: {
                    Flush();
                    uint32_t count = word1 / 64U;
                    while (count-- != 0U && modelViewTop != 0U) modelViewTop--;
                    RebuildCombined();
                    break;
                }
                case G_GEOMETRYMODE:
                    Flush();
                    geometryMode &= ~(word0 & 0xFFFFFFU);
                    geometryMode |= word1;
                    break;
                case G_TEXTURE:
                    Flush();
                    textureScaleS = static_cast<uint16_t>(word1 >> 16U);
                    textureScaleT = static_cast<uint16_t>(word1);
                    firstTile = static_cast<uint8_t>((word0 >> 8U) & 7U);
                    break;
                case G_MOVEWORD: {
                    Flush();
                    const uint8_t type = static_cast<uint8_t>(word0 >> 16U);
                    const uint16_t offset = static_cast<uint16_t>(word0);
                    if (type == G_MW_SEGMENT) {
                        const size_t segment = offset / 4U;
                        if (segment < segmentPointers.size()) {
                            segmentPointers[segment] = command.words.w1;
                        }
                    } else if (type == G_MW_NUMLIGHT) {
                        lightCount = std::min<size_t>(word1 / 24U + 1U,
                                                      lights.size());
                    } else if (type == G_MW_FOG) {
                        fogMultiply = static_cast<int16_t>(word1 >> 16U);
                        fogOffset = static_cast<int16_t>(word1);
                    }
                    break;
                }
                case G_MOVEMEM: {
                    Flush();
                    const uint8_t type = static_cast<uint8_t>(word0);
                    const uint8_t offset =
                        static_cast<uint8_t>(word0 >> 8U) * 8U;
                    const void *data = Resolve(command.words.w1);
                    if (type == G_MV_VIEWPORT && data != nullptr) {
                        const N64Viewport *viewport =
                            static_cast<const N64Viewport *>(data);
                        viewportWidth =
                            2.0f * viewport->scale[0] / 4.0f;
                        viewportHeight =
                            2.0f * viewport->scale[1] / 4.0f;
                        viewportX = kScreenInset +
                            viewport->translate[0] / 4.0f -
                            viewportWidth * 0.5f;
                        viewportY = viewport->translate[1] / 4.0f -
                            viewportHeight * 0.5f;
                    } else if (type == G_MV_LIGHT && data != nullptr) {
                        const int light = static_cast<int>(offset) / 24 - 2;
                        if (light >= 0 &&
                            static_cast<size_t>(light) < lights.size()) {
                            std::memcpy(&lights[static_cast<size_t>(light)],
                                        data, sizeof(N64Light));
                        }
                    }
                    break;
                }
                case G_MOVEMEM_HASH: {
                    Flush();
                    const uint8_t type = static_cast<uint8_t>(word0);
                    const uint8_t offset =
                        static_cast<uint8_t>(word0 >> 8U) * 8U;
                    const uint64_t hash = HashCommand(displayList[++index]);
                    commandCount++;
                    const void *data = ResourceGetDataByCrc(hash);
                    if (data == nullptr) stats.missing_resources++;
                    if (type == G_MV_LIGHT && data != nullptr) {
                        const int light = static_cast<int>(offset) / 24 - 2;
                        if (light >= 0 &&
                            static_cast<size_t>(light) < lights.size()) {
                            std::memcpy(&lights[static_cast<size_t>(light)],
                                        data, sizeof(N64Light));
                        }
                    }
                    break;
                }
                case G_DL: {
                    Flush();
                    const PBRuntimeGfx *nested =
                        static_cast<const PBRuntimeGfx *>(
                            Resolve(command.words.w1));
                    if (!RunList(nested, depth + 1U)) return false;
                    if (((word0 >> 16U) & 1U) != 0U) return true;
                    break;
                }
                case G_DL_OTR_FILEPATH: {
                    Flush();
                    const PBRuntimeGfx *nested =
                        static_cast<const PBRuntimeGfx *>(
                            ResourceGetDataByName(
                                reinterpret_cast<const char *>(
                                    command.words.w1)));
                    if (nested != nullptr && !RunList(nested, depth + 1U)) {
                        return false;
                    }
                    if (nested == nullptr) stats.missing_resources++;
                    if (((word0 >> 16U) & 1U) != 0U) return true;
                    break;
                }
                case G_DL_OTR_HASH: {
                    Flush();
                    const uint64_t hash = HashCommand(displayList[++index]);
                    commandCount++;
                    const PBRuntimeGfx *nested =
                        static_cast<const PBRuntimeGfx *>(
                            ResourceGetDataByCrc(hash));
                    if (nested != nullptr && !RunList(nested, depth + 1U)) {
                        return false;
                    }
                    if (nested == nullptr) stats.missing_resources++;
                    if (((word0 >> 16U) & 1U) != 0U) return true;
                    break;
                }
                case G_DL_INDEX: {
                    Flush();
                    const uint32_t segment = word1 >> 24U;
                    const uint32_t listIndex = word1 & 0xFFFFFFU;
                    const uintptr_t encoded =
                        (segment << 24U) |
                        static_cast<uint32_t>(listIndex *
                                              sizeof(PBRuntimeGfx)) |
                        1U;
                    if (!RunList(static_cast<const PBRuntimeGfx *>(
                                     Resolve(encoded)),
                                 depth + 1U)) {
                        return false;
                    }
                    if (((word0 >> 16U) & 1U) != 0U) return true;
                    break;
                }
                case G_BRANCH_Z_OTR: {
                    Flush();
                    const size_t vertex = word0 & 0xFFFU;
                    const uint64_t hash = HashCommand(displayList[++index]);
                    commandCount++;
                    if (vertex < vertices.size() &&
                        vertices[vertex].objectZ <= word1) {
                        const PBRuntimeGfx *branch =
                            static_cast<const PBRuntimeGfx *>(
                                ResourceGetDataByCrc(hash));
                        if (branch == nullptr) {
                            stats.missing_resources++;
                            break;
                        }
                        return RunList(branch, depth + 1U);
                    }
                    break;
                }
                case G_ENDDL:
                    return Flush();
                case G_SETTIMG:
                    Flush();
                    SetTextureImage(word0, command.words.w1);
                    break;
                case G_SETTIMG_OTR_FILEPATH:
                    Flush();
                    SetTextureImage(word0, command.words.w1,
                                    reinterpret_cast<const char *>(
                                        command.words.w1));
                    break;
                case G_SETTIMG_OTR_HASH: {
                    Flush();
                    const uint64_t hash = HashCommand(displayList[++index]);
                    commandCount++;
                    const char *path = ResourceGetNameByCrc(hash);
                    if (path != nullptr) {
                        SetTextureImage(word0, command.words.w1, path);
                    } else {
                        stats.missing_resources++;
                        textureToLoad = {};
                    }
                    break;
                }
                case G_SETTIMG_PAL: {
                    Flush();
                    const size_t palette = word0 & 0xFFU;
                    textureToLoad = {};
                    textureToLoad.data = palette < paletteBanks.size()
                                             ? paletteBanks[palette]
                                             : nullptr;
                    textureToLoad.format = 0U;
                    textureToLoad.size = 2U;
                    break;
                }
                case G_SETTILE:
                    Flush();
                    SetTile(word0, word1);
                    break;
                case G_SETTILESIZE:
                    Flush();
                    SetTileSize(word0, word1);
                    break;
                case G_SETTILESIZE_INTERP:
                case G_SETTILESIZE_LERP:
                    Flush();
                    SetTileSize(word0, word1);
                    index += 2U;
                    commandCount += 2U;
                    break;
                case G_SETTILESCROLL_INTERP:
                    Flush();
                    SetTileSize(word0, word1);
                    index += 1U;
                    commandCount += 1U;
                    break;
                case G_LOADBLOCK:
                case G_LOADTILE:
                    Flush();
                    LoadTexture((word1 >> 24U) & 7U);
                    break;
                case G_LOADBLOCK_WIDE:
                    Flush();
                    LoadTexture(word0 & 7U);
                    index++;
                    commandCount++;
                    break;
                case G_LOADTLUT:
                    Flush();
                    LoadPalette((word1 >> 24U) & 7U,
                                ((word1 >> 14U) & 0x3FFU) + 1U);
                    break;
                case G_SETCOMBINE:
                    Flush();
                    combineWord0 = word0;
                    combineWord1 = word1;
                    break;
                case G_SETOTHERMODE_L:
                    Flush();
                    ApplyOtherMode(&otherModeLow, word0, word1);
                    break;
                case G_SETOTHERMODE_H:
                    Flush();
                    ApplyOtherMode(&otherModeHigh, word0, word1);
                    break;
                case G_RDPSETOTHERMODE:
                    Flush();
                    otherModeHigh = word0 & 0xFFFFFFU;
                    otherModeLow = word1;
                    break;
                case G_SETPRIMCOLOR:
                    Flush();
                    primColor = {
                        static_cast<uint8_t>(word1 >> 24U),
                        static_cast<uint8_t>(word1 >> 16U),
                        static_cast<uint8_t>(word1 >> 8U),
                        static_cast<uint8_t>(word1),
                    };
                    break;
                case G_SETENVCOLOR:
                    Flush();
                    envColor = {
                        static_cast<uint8_t>(word1 >> 24U),
                        static_cast<uint8_t>(word1 >> 16U),
                        static_cast<uint8_t>(word1 >> 8U),
                        static_cast<uint8_t>(word1),
                    };
                    break;
                case G_SETFOGCOLOR:
                    Flush();
                    fogColor = {
                        static_cast<uint8_t>(word1 >> 24U),
                        static_cast<uint8_t>(word1 >> 16U),
                        static_cast<uint8_t>(word1 >> 8U),
                        static_cast<uint8_t>(word1),
                    };
                    break;
                case G_SETBLENDCOLOR:
                    Flush();
                    blendColor = {
                        static_cast<uint8_t>(word1 >> 24U),
                        static_cast<uint8_t>(word1 >> 16U),
                        static_cast<uint8_t>(word1 >> 8U),
                        static_cast<uint8_t>(word1),
                    };
                    break;
                case G_SETFILLCOLOR:
                    Flush();
                    fillColor = word1;
                    break;
                case G_SETSCISSOR: {
                    Flush();
                    const int left =
                        static_cast<int>((word0 >> 12U) & 0xFFFU) / 4;
                    const int top = static_cast<int>(word0 & 0xFFFU) / 4;
                    const int right =
                        static_cast<int>((word1 >> 12U) & 0xFFFU) / 4;
                    const int bottom = static_cast<int>(word1 & 0xFFFU) / 4;
                    api->SetScissor(static_cast<int>(kScreenInset) + left,
                                    static_cast<int>(PB_RENDER_TOP_HEIGHT) -
                                        bottom,
                                    std::max(0, right - left),
                                    std::max(0, bottom - top));
                    break;
                }
                case G_SETPRIMDEPTH:
                    Flush();
                    primDepth = 1.0f -
                        static_cast<float>((word1 >> 16U) & 0x7FFFU) /
                            32767.0f;
                    break;
                case G_TEXRECT:
                case G_TEXRECTFLIP: {
                    Flush();
                    const PBRuntimeGfx &texture = displayList[++index];
                    const PBRuntimeGfx &delta = displayList[++index];
                    commandCount += 2U;
                    float right = ((word0 >> 12U) & 0xFFFU) / 4.0f;
                    float bottom = (word0 & 0xFFFU) / 4.0f;
                    const float left = ((word1 >> 12U) & 0xFFFU) / 4.0f;
                    const float top = (word1 & 0xFFFU) / 4.0f;
                    const uint8_t savedTile = firstTile;
                    firstTile = static_cast<uint8_t>((word1 >> 24U) & 7U);
                    const float upperS =
                        static_cast<int16_t>(texture.words.w1 >> 16U) / 32.0f;
                    const float upperT =
                        static_cast<int16_t>(texture.words.w1) / 32.0f;
                    int16_t deltaSRaw =
                        static_cast<int16_t>(delta.words.w1 >> 16U);
                    int16_t deltaTRaw =
                        static_cast<int16_t>(delta.words.w1);
                    const bool copyCycle =
                        (otherModeHigh & G_CYCLE_TYPE_MASK) == G_CYCLE_COPY;
                    if (copyCycle) {
                        deltaSRaw = static_cast<int16_t>(deltaSRaw >> 2U);
                        right += 1.0f;
                        bottom += 1.0f;
                        stats.copy_rectangles++;
                    }
                    const float deltaS = deltaSRaw / 1024.0f;
                    const float deltaT = deltaTRaw / 1024.0f;
                    const bool flip = opcode == G_TEXRECTFLIP;
                    const float lowerS = upperS +
                        (flip ? -(bottom - top) : (right - left)) * deltaS;
                    const float lowerT = upperT +
                        (flip ? -(right - left) : (bottom - top)) * deltaT;
                    const bool emitted = EmitRectangle(
                        left, top, right, bottom, upperS, upperT,
                        lowerS, lowerT, true, flip);
                    firstTile = savedTile;
                    if (!emitted) {
                        return false;
                    }
                    break;
                }
                case G_TEXRECT_WIDE: {
                    Flush();
                    const auto signed24 = [](uint32_t value) {
                        return static_cast<int32_t>(value << 8U) >> 8U;
                    };
                    float right = signed24(word0 & 0xFFFFFFU) / 4.0f;
                    float bottom = signed24(word1 & 0xFFFFFFU) / 4.0f;
                    const PBRuntimeGfx &corner = displayList[++index];
                    const PBRuntimeGfx &texture = displayList[++index];
                    commandCount += 2U;
                    const float left =
                        signed24(static_cast<uint32_t>(corner.words.w0)) /
                        4.0f;
                    const float top =
                        signed24(static_cast<uint32_t>(corner.words.w1)) /
                        4.0f;
                    const uint8_t savedTile = firstTile;
                    firstTile = static_cast<uint8_t>(
                        (static_cast<uint32_t>(corner.words.w0) >> 24U) & 7U);
                    const float upperS =
                        static_cast<int16_t>(texture.words.w0 >> 16U) / 32.0f;
                    const float upperT =
                        static_cast<int16_t>(texture.words.w0) / 32.0f;
                    int16_t deltaSRaw =
                        static_cast<int16_t>(texture.words.w1 >> 16U);
                    const float deltaT =
                        static_cast<int16_t>(texture.words.w1) / 1024.0f;
                    if ((otherModeHigh & G_CYCLE_TYPE_MASK) == G_CYCLE_COPY) {
                        deltaSRaw = static_cast<int16_t>(deltaSRaw >> 2U);
                        right += 1.0f;
                        bottom += 1.0f;
                        stats.copy_rectangles++;
                    }
                    const float deltaS = deltaSRaw / 1024.0f;
                    const bool emitted = EmitRectangle(
                        left, top, right, bottom, upperS, upperT,
                        upperS + (right - left) * deltaS,
                        upperT + (bottom - top) * deltaT, true);
                    firstTile = savedTile;
                    if (!emitted) {
                        return false;
                    }
                    break;
                }
                case G_FILLRECT: {
                    Flush();
                    if (colorTargetIsDepth) {
                        api->ClearFramebuffer(false, true);
                        stats.depth_target_clears++;
                        break;
                    }
                    const bool inclusiveEdge =
                        (otherModeHigh & G_CYCLE_TYPE_MASK) == G_CYCLE_COPY ||
                        (otherModeHigh & G_CYCLE_TYPE_MASK) == G_CYCLE_FILL;
                    const uint16_t color = static_cast<uint16_t>(fillColor);
                    primColor = {
                        ExpandFive(color >> 11U), ExpandFive(color >> 6U),
                        ExpandFive(color >> 1U),
                        static_cast<uint8_t>((color & 1U) != 0U ? 255U : 0U),
                    };
                    if (!EmitRectangle(((word1 >> 12U) & 0xFFFU) / 4.0f,
                                       (word1 & 0xFFFU) / 4.0f,
                                       ((word0 >> 12U) & 0xFFFU) / 4.0f +
                                           (inclusiveEdge ? 1.0f : 0.0f),
                                       (word0 & 0xFFFU) / 4.0f +
                                           (inclusiveEdge ? 1.0f : 0.0f),
                                       0.0f, 0.0f, 0.0f, 0.0f, false)) {
                        return false;
                    }
                    break;
                }
                case G_FILLWIDERECT: {
                    Flush();
                    if (colorTargetIsDepth) {
                        api->ClearFramebuffer(false, true);
                        stats.depth_target_clears++;
                        index++;
                        commandCount++;
                        break;
                    }
                    const auto signed24 = [](uint32_t value) {
                        return static_cast<int32_t>(value << 8U) >> 8U;
                    };
                    float right = signed24(word0 & 0xFFFFFFU) / 4.0f;
                    float bottom = signed24(word1 & 0xFFFFFFU) / 4.0f;
                    const PBRuntimeGfx &corner = displayList[++index];
                    commandCount++;
                    const uint32_t cycle =
                        otherModeHigh & G_CYCLE_TYPE_MASK;
                    if (cycle == G_CYCLE_COPY || cycle == G_CYCLE_FILL) {
                        right += 1.0f;
                        bottom += 1.0f;
                    }
                    if (!EmitRectangle(
                            signed24(static_cast<uint32_t>(corner.words.w0)) /
                                4.0f,
                            signed24(static_cast<uint32_t>(corner.words.w1)) /
                                4.0f,
                            right, bottom, 0.0f, 0.0f, 0.0f, 0.0f, false)) {
                        return false;
                    }
                    break;
                }
                case G_INVALTEXCACHE:
                    InvalidateTexture(reinterpret_cast<const void *>(
                        command.words.w1));
                    break;
                case G_INVAL_TEX_BY_PAL:
                    InvalidateTexture(reinterpret_cast<const void *>(
                        command.words.w1));
                    break;
                case G_SET_STRICT_DECAL:
                    Flush();
                    api->SetStrictDecal(command.words.w1 != 0U);
                    break;
                case G_MARKER:
                    index++;
                    commandCount++;
                    break;
                case G_IMAGERECT: {
                    const PBRuntimeGfx &upper = displayList[++index];
                    const PBRuntimeGfx &lower = displayList[++index];
                    commandCount += 2U;
                    const uint8_t savedTile = firstTile;
                    firstTile = static_cast<uint8_t>(word0 & 7U);
                    Tile &imageTile = tiles[firstTile];
                    const uint16_t imageWidth =
                        static_cast<uint16_t>(word1 >> 16U);
                    const uint16_t imageHeight =
                        static_cast<uint16_t>(word1);
                    imageTile.upperS = 0U;
                    imageTile.upperT = 0U;
                    imageTile.lowerS = imageWidth == 0U
                        ? 0U
                        : static_cast<uint16_t>((imageWidth - 1U) * 4U);
                    imageTile.lowerT = imageHeight == 0U
                        ? 0U
                        : static_cast<uint16_t>((imageHeight - 1U) * 4U);
                    imageTile.shiftS = 0U;
                    imageTile.shiftT = 0U;
                    imageTile.clampS = 0U;
                    imageTile.clampT = 0U;
                    const float left =
                        static_cast<int16_t>(upper.words.w0 >> 16U) / 4.0f;
                    const float top =
                        static_cast<int16_t>(upper.words.w0) / 4.0f;
                    const float right =
                        static_cast<int16_t>(lower.words.w0 >> 16U) / 4.0f;
                    const float bottom =
                        static_cast<int16_t>(lower.words.w0) / 4.0f;
                    const float upperS =
                        static_cast<int16_t>(upper.words.w1 >> 16U);
                    const float upperT =
                        static_cast<int16_t>(upper.words.w1);
                    const float lowerS =
                        static_cast<int16_t>(lower.words.w1 >> 16U);
                    const float lowerT =
                        static_cast<int16_t>(lower.words.w1);
                    const bool emitted = EmitRectangle(
                        left, top, right, bottom, upperS, upperT,
                        lowerS, lowerT, true);
                    firstTile = savedTile;
                    if (!emitted) return false;
                    break;
                }
                case G_SETZIMG:
                    Flush();
                    depthImageAddress = command.words.w1;
                    colorTargetIsDepth = colorImageAddress != 0U &&
                                         colorImageAddress == depthImageAddress;
                    break;
                case G_SETCIMG:
                    Flush();
                    colorImageAddress = command.words.w1;
                    colorTargetIsDepth = depthImageAddress != 0U &&
                                         colorImageAddress == depthImageAddress;
                    break;
                case G_COPYFB:
                case G_PUSH_SHADER:
                case G_POP_SHADER:
                case G_SETTARGETINTERPINDEX:
                case G_SETUNIFORM:
                case G_RDPLOADSYNC:
                case G_RDPPIPESYNC:
                case G_RDPTILESYNC:
                case G_RDPFULLSYNC:
                case 0x00:
                case 0x21:
                case 0x22:
                case 0x23:
                case 0x28:
                case 0x39:
                case 0x3A:
                case 0x3E:
                case 0x3F:
                case 0x40:
                    break;
                default:
                    if (unknownCommands[opcode]++ == 0U) {
#ifndef __3DS__
                        std::fprintf(stderr,
                                     "runtime gfx: unknown opcode=%02x "
                                     "w0=%08x w1=%08x depth=%u index=%zu\n",
                                     opcode, word0, word1, depth, index);
#endif
                    }
                    stats.unknown_commands++;
                    stats.last_unknown_opcode = opcode;
                    break;
            }
        }
        malformed = true;
        return false;
    }

    GfxRenderingAPI3DS *api = nullptr;
    Fast::ShaderProgram *shadeShader = nullptr;
    Fast::ShaderProgram *textureShader = nullptr;
    Matrix projection = {};
    std::array<Matrix, kMaxMatrixStack> modelView = {};
    size_t modelViewTop = 0U;
    Matrix combined = {};
    std::array<LoadedVertex, kMaxVertices> vertices = {};
    std::array<Tile, 8U> tiles = {};
    TextureSource textureToLoad = {};
    std::array<TextureSource, 2U> loadedTextures = {};
    std::array<const uint8_t *, 16U> paletteBanks = {};
    const uint8_t *paletteFull = nullptr;
    std::array<uintptr_t, 16U> segmentPointers = {};
    std::array<N64Light, 9U> lights = {};
    size_t lightCount = 1U;
    uint32_t geometryMode = 0U;
    uint32_t otherModeHigh = 0U;
    uint32_t otherModeLow = 0U;
    uint16_t textureScaleS = UINT16_MAX;
    uint16_t textureScaleT = UINT16_MAX;
    uint8_t firstTile = 0U;
    float viewportX = kScreenInset;
    float viewportY = 0.0f;
    float viewportWidth = 320.0f;
    float viewportHeight = 240.0f;
    Color primColor = {};
    Color envColor = {};
    Color fogColor = {};
    Color blendColor = {};
    uint32_t fillColor = 0U;
    float primDepth = 0.5f;
    int16_t fogMultiply = 0;
    int16_t fogOffset = 0;
    uint32_t combineWord0 = 0U;
    uint32_t combineWord1 = 0U;
    size_t commandCount = 0U;
    uint8_t lastOpcode = 0U;
    size_t lastCommandIndex = 0U;
    unsigned int lastDepth = 0U;
    std::array<uint32_t, 256U> unknownCommands = {};
    bool malformed = false;
    bool depthClearPending = false;
    uintptr_t depthImageAddress = 0U;
    uintptr_t colorImageAddress = 0U;
    bool colorTargetIsDepth = false;
    std::vector<float> batch;
    size_t batchTriangles = 0U;
    bool batchTextured = false;
    uint32_t batchTexture = 0U;
    std::vector<TextureCacheEntry> textures;
    uint64_t textureUseClock = 0U;
    uint64_t frameSerial = 0U;
    PBRuntimeGfxStats stats = {};
};

bool GfxRenderingAPI3DS::RenderDisplayList(
    const PBRuntimeGfx *displayList) {
    if (mRuntimeRenderer == nullptr) {
        mRuntimeRenderer =
            new (std::nothrow) RuntimeDisplayListRenderer(this);
    }
    return mRuntimeRenderer != nullptr &&
           mRuntimeRenderer->Render(displayList);
}

void GfxRenderingAPI3DS::InvalidateRuntimeTexture(const void *address) {
    if (mRuntimeRenderer != nullptr) {
        mRuntimeRenderer->InvalidateTexture(address);
    }
}

void GfxRenderingAPI3DS::ClearRuntimeDepth() {
    if (mRuntimeRenderer != nullptr) {
        mRuntimeRenderer->RequestDepthClear();
    }
}

const PBRuntimeGfxStats *GfxRenderingAPI3DS::GetRuntimeStats() const {
    return mRuntimeRenderer != nullptr ? mRuntimeRenderer->GetStats()
                                       : nullptr;
}

void GfxRenderingAPI3DS::DestroyRuntimeRenderer() {
    delete mRuntimeRenderer;
    mRuntimeRenderer = nullptr;
}

} // namespace PB3DS
