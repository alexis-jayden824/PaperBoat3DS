#include "pb3ds/renderer.h"

#include <limits.h>
#include <string.h>

static bool dimension_is_valid(uint16_t dimension) {
    return dimension >= PB_RENDER_TEXTURE_MIN_DIMENSION &&
           dimension <= PB_RENDER_TEXTURE_MAX_DIMENSION &&
           (dimension & (uint16_t)(dimension - 1U)) == 0;
}

static uint8_t texture_bits_per_pixel(PBTextureFormat format) {
    switch (format) {
        case PB_TEXTURE_RGBA8:
            return 32;
        case PB_TEXTURE_RGB8:
            return 24;
        case PB_TEXTURE_RGBA5551:
        case PB_TEXTURE_RGB565:
        case PB_TEXTURE_RGBA4:
        case PB_TEXTURE_LA8:
        case PB_TEXTURE_HILO8:
            return 16;
        case PB_TEXTURE_L8:
        case PB_TEXTURE_A8:
        case PB_TEXTURE_LA4:
        case PB_TEXTURE_ETC1A4:
            return 8;
        case PB_TEXTURE_L4:
        case PB_TEXTURE_A4:
        case PB_TEXTURE_ETC1:
            return 4;
        case PB_TEXTURE_FORMAT_COUNT:
        default:
            return 0;
    }
}

bool pb_renderer_texture_layout(PBTextureLayout *layout, uint16_t width,
                                uint16_t height, PBTextureFormat format) {
    if (layout == NULL) {
        return false;
    }
    memset(layout, 0, sizeof(*layout));

    const uint8_t bits_per_pixel = texture_bits_per_pixel(format);
    if (!dimension_is_valid(width) || !dimension_is_valid(height) ||
        bits_per_pixel == 0) {
        return false;
    }

    const size_t texels = (size_t)width * height;
    if (texels > SIZE_MAX / bits_per_pixel) {
        return false;
    }

    layout->width = width;
    layout->height = height;
    layout->format = format;
    layout->bits_per_pixel = bits_per_pixel;
    layout->bytes = texels * bits_per_pixel / CHAR_BIT;
    return true;
}

const char *pb_renderer_texture_format_name(PBTextureFormat format) {
    static const char *const names[] = {
        "RGBA8", "RGB8", "RGBA5551", "RGB565", "RGBA4", "LA8",
        "HILO8", "L8", "A8", "LA4", "L4", "A4", "ETC1",
        "ETC1A4",
    };
    _Static_assert(sizeof(names) / sizeof(names[0]) == PB_TEXTURE_FORMAT_COUNT,
                   "every texture format needs a diagnostic name");
    if ((unsigned int)format >= (unsigned int)PB_TEXTURE_FORMAT_COUNT) {
        return "invalid";
    }
    return names[format];
}

static size_t morton_3bit(uint16_t x, uint16_t y) {
    size_t result = 0;
    for (unsigned int bit = 0; bit < 3; bit++) {
        result |= (size_t)((x >> bit) & 1U) << (bit * 2U);
        result |= (size_t)((y >> bit) & 1U) << (bit * 2U + 1U);
    }
    return result;
}

size_t pb_renderer_swizzled_texel_index(uint16_t x, uint16_t y,
                                        uint16_t width, uint16_t height) {
    if (!dimension_is_valid(width) || !dimension_is_valid(height) ||
        x >= width || y >= height) {
        return SIZE_MAX;
    }

    const size_t tile_origin =
        (size_t)(y & (uint16_t)~7U) * width +
        (size_t)(x & (uint16_t)~7U) * 8U;
    return tile_origin + morton_3bit((uint16_t)(x & 7U),
                                    (uint16_t)(y & 7U));
}

bool pb_renderer_swizzle_rgba8(uint8_t *destination, size_t destination_size,
                               const uint8_t *source, size_t source_size,
                               uint16_t width, uint16_t height) {
    PBTextureLayout layout;
    if (destination == NULL || source == NULL ||
        !pb_renderer_texture_layout(&layout, width, height,
                                    PB_TEXTURE_RGBA8) ||
        destination_size < layout.bytes || source_size < layout.bytes) {
        return false;
    }

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            const size_t source_offset = ((size_t)y * width + x) * 4U;
            const size_t destination_offset =
                pb_renderer_swizzled_texel_index(x, y, width, height) * 4U;

            /* PICA RGBA8 texture bytes are stored in ABGR order. */
            destination[destination_offset + 0U] = source[source_offset + 3U];
            destination[destination_offset + 1U] = source[source_offset + 2U];
            destination[destination_offset + 2U] = source[source_offset + 1U];
            destination[destination_offset + 3U] = source[source_offset + 0U];
        }
    }
    return true;
}

bool pb_renderer_vertex_buffer_size(size_t stride, size_t vertex_count,
                                    size_t *bytes) {
    if (bytes == NULL) {
        return false;
    }
    *bytes = 0;
    if (stride == 0 || vertex_count == 0 || vertex_count > SIZE_MAX / stride) {
        return false;
    }
    *bytes = stride * vertex_count;
    return true;
}

bool pb_renderer_stream_reserve(size_t capacity_vertices,
                                size_t used_vertices,
                                size_t requested_vertices,
                                size_t *first_vertex,
                                size_t *next_used_vertices) {
    if (first_vertex == NULL || next_used_vertices == NULL ||
        requested_vertices == 0 || used_vertices > capacity_vertices ||
        requested_vertices > capacity_vertices - used_vertices) {
        return false;
    }

    *first_vertex = used_vertices;
    *next_used_vertices = used_vertices + requested_vertices;
    return true;
}

bool pb_renderer_viewport_to_target(const PBViewport *logical,
                                    PBTargetViewport *target) {
    if (logical == NULL || target == NULL || logical->width == 0 ||
        logical->height == 0 || logical->x >= PB_RENDER_TOP_WIDTH ||
        logical->y >= PB_RENDER_TOP_HEIGHT ||
        (uint32_t)logical->x + logical->width > PB_RENDER_TOP_WIDTH ||
        (uint32_t)logical->y + logical->height > PB_RENDER_TOP_HEIGHT) {
        return false;
    }

    target->x = logical->y;
    target->y = (uint16_t)(PB_RENDER_TOP_WIDTH - logical->x - logical->width);
    target->width = logical->height;
    target->height = logical->width;
    return true;
}

bool pb_renderer_pipeline_is_valid(const PBRenderPipeline *pipeline) {
    if (pipeline == NULL ||
        (unsigned int)pipeline->cull_mode >= (unsigned int)PB_CULL_COUNT ||
        (unsigned int)pipeline->depth_function >=
            (unsigned int)PB_COMPARE_COUNT ||
        (unsigned int)pipeline->blend_mode >= (unsigned int)PB_BLEND_COUNT ||
        (unsigned int)pipeline->min_filter >= (unsigned int)PB_FILTER_COUNT ||
        (unsigned int)pipeline->mag_filter >= (unsigned int)PB_FILTER_COUNT ||
        (unsigned int)pipeline->wrap_s >= (unsigned int)PB_WRAP_COUNT ||
        (unsigned int)pipeline->wrap_t >= (unsigned int)PB_WRAP_COUNT) {
        return false;
    }
    return true;
}

void pb_renderer_state_cache_init(PBRenderStateCache *cache) {
    if (cache != NULL) {
        memset(cache, 0, sizeof(*cache));
    }
}

static bool viewport_equals(const PBViewport *left, const PBViewport *right) {
    return left->x == right->x && left->y == right->y &&
           left->width == right->width && left->height == right->height;
}

static bool pipeline_equals(const PBRenderPipeline *left,
                            const PBRenderPipeline *right) {
    return left->cull_mode == right->cull_mode &&
           left->depth_test_enabled == right->depth_test_enabled &&
           left->depth_write_enabled == right->depth_write_enabled &&
           left->depth_function == right->depth_function &&
           left->blend_mode == right->blend_mode &&
           left->min_filter == right->min_filter &&
           left->mag_filter == right->mag_filter &&
           left->wrap_s == right->wrap_s && left->wrap_t == right->wrap_t;
}

PBBindResult pb_renderer_bind_viewport(PBRenderStateCache *cache,
                                       const PBViewport *viewport) {
    PBTargetViewport unused_target;
    if (cache == NULL ||
        !pb_renderer_viewport_to_target(viewport, &unused_target)) {
        if (cache != NULL) {
            cache->rejected++;
        }
        return PB_BIND_REJECTED;
    }
    if (cache->viewport_bound && viewport_equals(&cache->viewport, viewport)) {
        cache->deduplicated++;
        return PB_BIND_UNCHANGED;
    }

    cache->viewport = *viewport;
    cache->viewport_bound = true;
    cache->changes++;
    return PB_BIND_CHANGED;
}

PBBindResult pb_renderer_bind_pipeline(PBRenderStateCache *cache,
                                       const PBRenderPipeline *pipeline) {
    if (cache == NULL || !pb_renderer_pipeline_is_valid(pipeline)) {
        if (cache != NULL) {
            cache->rejected++;
        }
        return PB_BIND_REJECTED;
    }
    if (cache->pipeline_bound && pipeline_equals(&cache->pipeline, pipeline)) {
        cache->deduplicated++;
        return PB_BIND_UNCHANGED;
    }

    cache->pipeline = *pipeline;
    cache->pipeline_bound = true;
    cache->changes++;
    return PB_BIND_CHANGED;
}

const char *pb_renderer_init_result_name(PBRendererInitResult result) {
    switch (result) {
        case PB_RENDERER_INIT_OK:
            return "ready";
        case PB_RENDERER_INIT_INVALID_ARGUMENT:
            return "invalid argument";
        case PB_RENDERER_INIT_OUT_OF_MEMORY:
            return "out of memory";
        case PB_RENDERER_INIT_CITRO3D:
            return "citro3d init failed";
        case PB_RENDERER_INIT_TARGET:
            return "render target failed";
        case PB_RENDERER_INIT_SHADER:
            return "shader load failed";
        case PB_RENDERER_INIT_UNIFORM:
            return "shader uniform missing";
        case PB_RENDERER_INIT_VERTEX_BUFFER:
            return "vertex buffer failed";
        case PB_RENDERER_INIT_TEXTURE:
            return "texture upload failed";
        default:
            return "unknown";
    }
}
