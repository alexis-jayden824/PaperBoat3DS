#include "pb3ds/texture.h"

#include <limits.h>
#include <string.h>

#include "pb3ds/renderer.h"

#define PB_OTR_HEADER_SIZE 64U
#define PB_TEXTURE_BODY_HEADER_SIZE 16U
#define PB_TEXTURE_RESOURCE_HEADER_SIZE \
    (PB_OTR_HEADER_SIZE + PB_TEXTURE_BODY_HEADER_SIZE)
#define PB_OTR_TEXTURE_TYPE 0x4F544558U

static uint32_t read_u32(const uint8_t *bytes, bool big_endian) {
    if (big_endian) {
        return ((uint32_t)bytes[0] << 24U) |
               ((uint32_t)bytes[1] << 16U) |
               ((uint32_t)bytes[2] << 8U) | (uint32_t)bytes[3];
    }
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static uint8_t expand_five_bits(uint16_t value) {
    const uint8_t five_bits = (uint8_t)(value & 0x1FU);
    return (uint8_t)((five_bits << 3U) | (five_bits >> 2U));
}

static uint16_t next_texture_dimension(uint32_t dimension) {
    uint32_t result = PB_RENDER_TEXTURE_MIN_DIMENSION;
    if (dimension == 0 || dimension > PB_RENDER_TEXTURE_MAX_DIMENSION) {
        return 0;
    }
    while (result < dimension) {
        result <<= 1U;
    }
    return (uint16_t)result;
}

static bool expected_image_size(const PBTextureResourceView *resource,
                                size_t *expected) {
    if (resource == NULL || expected == NULL || resource->width == 0 ||
        resource->height == 0 ||
        resource->width > SIZE_MAX / resource->height) {
        return false;
    }
    const size_t texels = (size_t)resource->width * resource->height;
    size_t bytes_per_texel = 0;
    switch ((PBResourceTextureType)resource->type) {
        case PB_RESOURCE_TEXTURE_RGBA32:
            bytes_per_texel = 4U;
            break;
        case PB_RESOURCE_TEXTURE_RGBA16:
            bytes_per_texel = 2U;
            break;
        case PB_RESOURCE_TEXTURE_CI8:
        case PB_RESOURCE_TEXTURE_IA8:
            bytes_per_texel = 1U;
            break;
        default:
            return false;
    }
    if (texels > SIZE_MAX / bytes_per_texel) {
        return false;
    }
    *expected = texels * bytes_per_texel;
    return true;
}

bool pb_texture_resource_parse(const uint8_t *data, size_t size,
                               PBTextureResourceView *resource) {
    if (data == NULL || resource == NULL ||
        size < PB_TEXTURE_RESOURCE_HEADER_SIZE || data[0] > 1U) {
        return false;
    }
    memset(resource, 0, sizeof(*resource));
    const bool big_endian = data[0] == 1U;
    if (read_u32(&data[4], big_endian) != PB_OTR_TEXTURE_TYPE ||
        read_u32(&data[8], big_endian) != 0U) {
        return false;
    }

    const uint8_t *body = &data[PB_OTR_HEADER_SIZE];
    resource->type = read_u32(&body[0], big_endian);
    resource->width = read_u32(&body[4], big_endian);
    resource->height = read_u32(&body[8], big_endian);
    resource->image_size = read_u32(&body[12], big_endian);
    resource->image = &data[PB_TEXTURE_RESOURCE_HEADER_SIZE];
    return resource->image_size == size - PB_TEXTURE_RESOURCE_HEADER_SIZE;
}

bool pb_texture_resource_matches(const PBTextureResourceView *resource,
                                 PBResourceTextureType type, uint16_t width,
                                 uint16_t height) {
    size_t expected = 0;
    return resource != NULL && resource->type == (uint32_t)type &&
           resource->width == width && resource->height == height &&
           expected_image_size(resource, &expected) &&
           resource->image_size == expected;
}

PBTextureDecodeResult pb_texture_decode_rgba8(
    PBDecodedTexture *decoded, const PBTextureResourceView *resource,
    const PBTextureResourceView *palette, PBMemoryMonitor *memory) {
    if (decoded == NULL || resource == NULL || memory == NULL) {
        return PB_TEXTURE_DECODE_INVALID_ARGUMENT;
    }
    memset(decoded, 0, sizeof(*decoded));

    size_t expected = 0;
    if (resource->width > UINT16_MAX || resource->height > UINT16_MAX ||
        !expected_image_size(resource, &expected)) {
        switch ((PBResourceTextureType)resource->type) {
            case PB_RESOURCE_TEXTURE_RGBA32:
            case PB_RESOURCE_TEXTURE_RGBA16:
            case PB_RESOURCE_TEXTURE_CI8:
            case PB_RESOURCE_TEXTURE_IA8:
                return PB_TEXTURE_DECODE_INVALID_RESOURCE;
            default:
                return PB_TEXTURE_DECODE_UNSUPPORTED_FORMAT;
        }
    }
    if (resource->image_size != expected || resource->image == NULL) {
        return PB_TEXTURE_DECODE_INVALID_RESOURCE;
    }
    if (resource->type == PB_RESOURCE_TEXTURE_CI8 &&
        !pb_texture_resource_matches(palette, PB_RESOURCE_TEXTURE_RGBA16,
                                     256, 1)) {
        return PB_TEXTURE_DECODE_INVALID_PALETTE;
    }

    const uint16_t texture_width = next_texture_dimension(resource->width);
    const uint16_t texture_height = next_texture_dimension(resource->height);
    if (texture_width == 0 || texture_height == 0 ||
        (size_t)texture_width > SIZE_MAX / texture_height ||
        (size_t)texture_width * texture_height > SIZE_MAX / 4U) {
        return PB_TEXTURE_DECODE_INVALID_RESOURCE;
    }
    const size_t rgba_size =
        (size_t)texture_width * texture_height * 4U;
    uint8_t *rgba = pb_memory_alloc(memory, PB_MEMORY_SCENE, rgba_size);
    if (rgba == NULL) {
        return PB_TEXTURE_DECODE_OUT_OF_MEMORY;
    }
    memset(rgba, 0, rgba_size);

    for (uint32_t y = 0; y < resource->height; y++) {
        for (uint32_t x = 0; x < resource->width; x++) {
            const size_t texel = (size_t)y * resource->width + x;
            const size_t output =
                ((size_t)y * texture_width + x) * 4U;
            switch ((PBResourceTextureType)resource->type) {
                case PB_RESOURCE_TEXTURE_RGBA32: {
                    const size_t source = texel * 4U;
                    memcpy(&rgba[output], &resource->image[source], 4U);
                    break;
                }
                case PB_RESOURCE_TEXTURE_RGBA16: {
                    const size_t source = texel * 2U;
                    const uint16_t color =
                        ((uint16_t)resource->image[source] << 8U) |
                        resource->image[source + 1U];
                    rgba[output + 0U] =
                        expand_five_bits((uint16_t)(color >> 11U));
                    rgba[output + 1U] =
                        expand_five_bits((uint16_t)(color >> 6U));
                    rgba[output + 2U] =
                        expand_five_bits((uint16_t)(color >> 1U));
                    rgba[output + 3U] =
                        (color & 1U) != 0 ? 255U : 0U;
                    break;
                }
                case PB_RESOURCE_TEXTURE_CI8: {
                    const size_t source =
                        (size_t)resource->image[texel] * 2U;
                    const uint16_t color =
                        ((uint16_t)palette->image[source] << 8U) |
                        palette->image[source + 1U];
                    rgba[output + 0U] =
                        expand_five_bits((uint16_t)(color >> 11U));
                    rgba[output + 1U] =
                        expand_five_bits((uint16_t)(color >> 6U));
                    rgba[output + 2U] =
                        expand_five_bits((uint16_t)(color >> 1U));
                    rgba[output + 3U] =
                        (color & 1U) != 0 ? 255U : 0U;
                    break;
                }
                case PB_RESOURCE_TEXTURE_IA8: {
                    const uint8_t packed = resource->image[texel];
                    const uint8_t intensity =
                        (uint8_t)((packed >> 4U) * 17U);
                    rgba[output + 0U] = intensity;
                    rgba[output + 1U] = intensity;
                    rgba[output + 2U] = intensity;
                    rgba[output + 3U] = (uint8_t)((packed & 0x0FU) * 17U);
                    break;
                }
                default:
                    pb_memory_free(memory, PB_MEMORY_SCENE, rgba, rgba_size);
                    return PB_TEXTURE_DECODE_UNSUPPORTED_FORMAT;
            }
        }
    }

    decoded->rgba = rgba;
    decoded->rgba_size = rgba_size;
    decoded->source_width = (uint16_t)resource->width;
    decoded->source_height = (uint16_t)resource->height;
    decoded->texture_width = texture_width;
    decoded->texture_height = texture_height;
    decoded->source_type = resource->type;
    return PB_TEXTURE_DECODE_OK;
}

void pb_decoded_texture_release(PBDecodedTexture *decoded,
                                PBMemoryMonitor *memory) {
    if (decoded == NULL || memory == NULL || decoded->rgba == NULL) {
        return;
    }
    pb_memory_free(memory, PB_MEMORY_SCENE, decoded->rgba,
                   decoded->rgba_size);
    memset(decoded, 0, sizeof(*decoded));
}

const char *pb_texture_decode_result_name(PBTextureDecodeResult result) {
    switch (result) {
        case PB_TEXTURE_DECODE_OK:
            return "ready";
        case PB_TEXTURE_DECODE_INVALID_ARGUMENT:
            return "invalid argument";
        case PB_TEXTURE_DECODE_INVALID_RESOURCE:
            return "invalid texture";
        case PB_TEXTURE_DECODE_UNSUPPORTED_FORMAT:
            return "unsupported format";
        case PB_TEXTURE_DECODE_INVALID_PALETTE:
            return "invalid palette";
        case PB_TEXTURE_DECODE_OUT_OF_MEMORY:
            return "memory budget";
        default:
            return "unknown";
    }
}
