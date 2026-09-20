#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pb3ds/memory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PB_RESOURCE_TEXTURE_ERROR = 0,
    PB_RESOURCE_TEXTURE_RGBA32 = 1,
    PB_RESOURCE_TEXTURE_RGBA16 = 2,
    PB_RESOURCE_TEXTURE_CI4 = 3,
    PB_RESOURCE_TEXTURE_CI8 = 4,
    PB_RESOURCE_TEXTURE_I4 = 5,
    PB_RESOURCE_TEXTURE_I8 = 6,
    PB_RESOURCE_TEXTURE_IA4 = 7,
    PB_RESOURCE_TEXTURE_IA8 = 8,
    PB_RESOURCE_TEXTURE_IA16 = 9,
} PBResourceTextureType;

typedef enum {
    PB_TEXTURE_DECODE_OK = 0,
    PB_TEXTURE_DECODE_INVALID_ARGUMENT,
    PB_TEXTURE_DECODE_INVALID_RESOURCE,
    PB_TEXTURE_DECODE_UNSUPPORTED_FORMAT,
    PB_TEXTURE_DECODE_INVALID_PALETTE,
    PB_TEXTURE_DECODE_OUT_OF_MEMORY,
} PBTextureDecodeResult;

typedef struct {
    uint32_t type;
    uint32_t width;
    uint32_t height;
    uint32_t image_size;
    const uint8_t *image;
} PBTextureResourceView;

typedef struct {
    uint8_t *rgba;
    size_t rgba_size;
    uint16_t source_width;
    uint16_t source_height;
    uint16_t texture_width;
    uint16_t texture_height;
    uint32_t source_type;
} PBDecodedTexture;

bool pb_texture_resource_parse(const uint8_t *data, size_t size,
                               PBTextureResourceView *resource);
bool pb_texture_resource_matches(const PBTextureResourceView *resource,
                                 PBResourceTextureType type, uint16_t width,
                                 uint16_t height);
PBTextureDecodeResult pb_texture_decode_rgba8(
    PBDecodedTexture *decoded, const PBTextureResourceView *resource,
    const PBTextureResourceView *palette, PBMemoryMonitor *memory);
void pb_decoded_texture_release(PBDecodedTexture *decoded,
                                PBMemoryMonitor *memory);
const char *pb_texture_decode_result_name(PBTextureDecodeResult result);

#ifdef __cplusplus
}
#endif
