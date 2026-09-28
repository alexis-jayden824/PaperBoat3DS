#include "pb3ds/runtime_resources.h"
#include "pb3ds/texture.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int32_t SpriteS32;

#define SPRITE_ARRAY_LIMIT 1000U
#define PLAYER_RASTER_DESCRIPTOR_LIMIT 101U
#define PLAYER_WORLD_RASTER_BYTES 0x700U
#define PLAYER_OTHER_RASTER_BYTES 0x900U

typedef struct __attribute__((packed)) {
    uint32_t rasters_offset;
    uint32_t palettes_offset;
    int32_t max_components;
    int32_t color_variations;
} N64SpriteHeader;

typedef struct __attribute__((packed)) {
    uint32_t image_offset;
    uint8_t width;
    uint8_t height;
    int8_t palette;
    int8_t quad_cache_index;
} N64SpriteRaster;

typedef struct __attribute__((packed)) {
    uint32_t command_offset;
    int16_t command_size;
    int16_t x;
    int16_t y;
    int16_t z;
} N64SpriteComponent;

typedef struct {
    void *image;
    uint8_t width;
    uint8_t height;
    int8_t palette;
    int8_t quad_cache_index;
} NativeSpriteRaster;

typedef struct {
    void *commands;
    int16_t command_size;
    int16_t x;
    int16_t y;
    int16_t z;
} NativeSpriteComponent;

static SpriteS32 sprites_available;
static uint32_t sprite_header[3];

SpriteS32 Sprite_GetDataHeader(int32_t *output);

static uint32_t swap32(uint32_t value) {
    return ((value & UINT32_C(0x000000FF)) << 24U) |
           ((value & UINT32_C(0x0000FF00)) << 8U) |
           ((value & UINT32_C(0x00FF0000)) >> 8U) |
           ((value & UINT32_C(0xFF000000)) >> 24U);
}

static bool range_valid(size_t size, size_t offset, size_t bytes) {
    return offset <= size && bytes <= size - offset;
}

static bool count_offsets(const uint8_t *blob, size_t size, uint32_t offset,
                          size_t *count) {
    if (blob == NULL || count == NULL || (offset & 3U) != 0U ||
        !range_valid(size, offset, sizeof(uint32_t))) {
        return false;
    }
    const uint32_t *values = (const uint32_t *)(blob + offset);
    const size_t available = (size - offset) / sizeof(uint32_t);
    for (size_t index = 0U;
         index < available && index <= SPRITE_ARRAY_LIMIT; index++) {
        if (values[index] == UINT32_MAX) {
            *count = index;
            return true;
        }
    }
    return false;
}

static bool player_raster_range_valid(int32_t sprite_index,
                                      size_t expected_rasters) {
    static const char header_path[] =
        "__OTR__sprites/player_raster_header";
    static const char sets_path[] =
        "__OTR__sprites/player_raster_sets";
    static const char descriptors_path[] =
        "__OTR__sprites/player_raster_load_descriptors";
    static const char image_path[] =
        "__OTR__sprites/player_raster_image_data";
    const uint32_t *header = ResourceGetDataByName(header_path);
    const uint32_t *sets = ResourceGetDataByName(sets_path);
    const uint32_t *descriptors = ResourceGetDataByName(descriptors_path);
    const uint8_t *images = ResourceGetDataByName(image_path);
    const size_t header_size = pb_runtime_resource_payload_size(header_path);
    const size_t sets_size = pb_runtime_resource_payload_size(sets_path);
    const size_t descriptors_size =
        pb_runtime_resource_payload_size(descriptors_path);
    const size_t image_size = pb_runtime_resource_payload_size(image_path);

    if (sprite_index < 0 || header == NULL || sets == NULL ||
        descriptors == NULL || images == NULL || header_size != 12U ||
        sets_size % sizeof(uint32_t) != 0U ||
        descriptors_size % sizeof(uint32_t) != 0U) {
        return false;
    }
    const size_t set_count = sets_size / sizeof(uint32_t);
    const size_t descriptor_count = descriptors_size / sizeof(uint32_t);
    if ((size_t)sprite_index + 1U >= set_count) return false;
    const uint32_t begin = swap32(sets[sprite_index]);
    const uint32_t end = swap32(sets[sprite_index + 1]);
    if (end < begin || end > descriptor_count ||
        end - begin > PLAYER_RASTER_DESCRIPTOR_LIMIT ||
        (expected_rasters != SIZE_MAX &&
         end - begin != expected_rasters)) {
        return false;
    }

    const uint32_t image_base = swap32(header[2]) & UINT32_C(0xFFFFF);
    const uint32_t maximum = sprite_index <= 8
                                 ? PLAYER_WORLD_RASTER_BYTES
                                 : PLAYER_OTHER_RASTER_BYTES;
    for (uint32_t index = begin; index < end; index++) {
        const uint32_t descriptor = swap32(descriptors[index]);
        const uint32_t source = descriptor & UINT32_C(0xFFFFF);
        const uint32_t bytes = (descriptor >> 16U) & UINT32_C(0xFFF0);
        if (bytes == 0U || bytes > maximum || source < image_base ||
            (size_t)(source - image_base) > image_size ||
            bytes > image_size - (size_t)(source - image_base)) {
            return false;
        }
    }
    return true;
}

bool PB3DS_RuntimeValidatePlayerRasterTables(void) {
    static const int32_t m13_players[] = { 0, 1, 5, 6, 7, 8 };
    for (size_t index = 0U;
         index < sizeof(m13_players) / sizeof(m13_players[0]); index++) {
        if (!player_raster_range_valid(m13_players[index], SIZE_MAX)) {
            return false;
        }
    }
    return true;
}

static const char *sprite_image_path(const char *base, const char *kind,
                                     size_t index) {
    if (base == NULL || kind == NULL) return NULL;
    char candidate[PB_O2R_NAME_CAPACITY + 8U];
    const int length = snprintf(candidate, sizeof(candidate), "%s_%s_%lu",
                                base, kind, (unsigned long)index);
    if (length <= 0 || (size_t)length >= sizeof(candidate)) return NULL;
    return pb_runtime_resource_otr_name(candidate);
}

static bool validate_sprite_payload(const uint8_t *source, size_t source_size,
                                    bool player_sprite,
                                    int32_t sprite_index,
                                    const char *asset_path,
                                    size_t raster_count,
                                    size_t palette_count,
                                    size_t animation_count,
                                    const uint32_t *animations,
                                    const N64SpriteHeader *header) {
    if (player_sprite &&
        !player_raster_range_valid(sprite_index, raster_count)) {
        return false;
    }
    const uint32_t *raster_offsets =
        (const uint32_t *)(source + header->rasters_offset);
    for (size_t i = 0U; i < raster_count; i++) {
        if (!range_valid(source_size, raster_offsets[i],
                         sizeof(N64SpriteRaster))) return false;
        const N64SpriteRaster *input =
            (const N64SpriteRaster *)(source + raster_offsets[i]);
        const char *path = asset_path != NULL
                               ? sprite_image_path(asset_path, "raster", i)
                               : NULL;
        if (path != NULL) {
            if (pb_runtime_resource_texture_type(path) !=
                    PB_RESOURCE_TEXTURE_CI4 ||
                ResourceGetTexWidthByName(path) != input->width ||
                ResourceGetTexHeightByName(path) != input->height) {
                return false;
            }
        } else {
            const bool placeholder =
                player_sprite && input->width == UINT8_MAX &&
                input->height == UINT8_MAX;
            const size_t pixels =
                ((size_t)input->width * (size_t)input->height + 1U) / 2U;
            if (!placeholder &&
                !range_valid(source_size, input->image_offset, pixels)) {
                return false;
            }
        }
    }

    const uint32_t *palette_offsets =
        (const uint32_t *)(source + header->palettes_offset);
    for (size_t i = 0U; i < palette_count; i++) {
        const char *path = asset_path != NULL
                               ? sprite_image_path(asset_path, "pal", i)
                               : NULL;
        if (path != NULL) {
            if (pb_runtime_resource_texture_type(path) !=
                    PB_RESOURCE_TEXTURE_RGBA16 ||
                ResourceGetTexWidthByName(path) != 16U ||
                ResourceGetTexHeightByName(path) != 1U) {
                return false;
            }
        } else if ((palette_offsets[i] & 1U) != 0U ||
                   !range_valid(source_size, palette_offsets[i], 32U)) {
            return false;
        }
    }

    for (size_t animation = 0U; animation < animation_count; animation++) {
        size_t component_count = 0U;
        if (!count_offsets(source, source_size, animations[animation],
                           &component_count)) return false;
        const uint32_t *offsets =
            (const uint32_t *)(source + animations[animation]);
        for (size_t i = 0U; i < component_count; i++) {
            if (!range_valid(source_size, offsets[i],
                             sizeof(N64SpriteComponent))) return false;
            const N64SpriteComponent *input =
                (const N64SpriteComponent *)(source + offsets[i]);
            if ((input->command_offset & 1U) != 0U ||
                input->command_size <= 0 ||
                (input->command_size & 1) != 0 ||
                !range_valid(source_size, input->command_offset,
                             (size_t)input->command_size)) {
                return false;
            }
        }
    }
    return true;
}

static bool add_size(size_t *total, size_t count, size_t element) {
    if (total == NULL || (count != 0U && element > SIZE_MAX / count) ||
        count * element > SIZE_MAX - *total) {
        return false;
    }
    *total += count * element;
    return true;
}

static size_t convert_sprite(const uint8_t *source, size_t source_size,
                             uint8_t *destination, size_t destination_size,
                             bool player_sprite, int32_t sprite_index,
                             const char *asset_path) {
    if (source == NULL || source_size < sizeof(N64SpriteHeader) + 4U) return 0U;
    const N64SpriteHeader *header = (const N64SpriteHeader *)source;
    size_t raster_count = 0U, palette_count = 0U, animation_count = 0U;
    if (!count_offsets(source, source_size, header->rasters_offset,
                       &raster_count) ||
        !count_offsets(source, source_size, header->palettes_offset,
                       &palette_count) ||
        !count_offsets(source, source_size, sizeof(*header),
                       &animation_count)) {
        return 0U;
    }

    const uint32_t *animations =
        (const uint32_t *)(source + sizeof(*header));
    size_t total_components = 0U;
    size_t component_arrays_size = 0U;
    for (size_t i = 0U; i < animation_count; i++) {
        size_t count = 0U;
        if (!count_offsets(source, source_size, animations[i], &count) ||
            count > SIZE_MAX - total_components ||
            !add_size(&component_arrays_size, count + 1U, sizeof(void *))) {
            return 0U;
        }
        total_components += count;
    }

    size_t header_size = sizeof(void *) * 2U + sizeof(int32_t) * 2U;
    if (!add_size(&header_size, animation_count + 1U, sizeof(void *))) return 0U;
    size_t total = header_size;
    if (!add_size(&total, raster_count + 1U, sizeof(void *)) ||
        !add_size(&total, raster_count, sizeof(NativeSpriteRaster)) ||
        !add_size(&total, palette_count + 1U, sizeof(void *)) ||
        !add_size(&total, 1U, component_arrays_size) ||
        !add_size(&total, total_components, sizeof(NativeSpriteComponent)) ||
        !add_size(&total, 1U, source_size) || total > SIZE_MAX - 15U) {
        return 0U;
    }
    total = (total + 15U) & ~(size_t)15U;
    if (!validate_sprite_payload(source, source_size, player_sprite,
                                 sprite_index, asset_path, raster_count, palette_count,
                                 animation_count, animations, header)) {
        return 0U;
    }
    if (destination == NULL) return total;
    if (destination_size < total) return 0U;
    memset(destination, 0, destination_size);

    uint8_t *cursor = destination;
    void **native_header = (void **)cursor;
    int32_t *native_scalars =
        (int32_t *)(cursor + sizeof(void *) * 2U);
    void ***native_animations =
        (void ***)(cursor + sizeof(void *) * 2U + sizeof(int32_t) * 2U);
    cursor += header_size;

    void **raster_array = (void **)cursor;
    native_header[0] = raster_array;
    cursor += sizeof(void *) * (raster_count + 1U);
    NativeSpriteRaster *rasters = (NativeSpriteRaster *)cursor;
    cursor += sizeof(*rasters) * raster_count;

    void **palette_array = (void **)cursor;
    native_header[1] = palette_array;
    cursor += sizeof(void *) * (palette_count + 1U);
    native_scalars[0] = header->max_components;
    native_scalars[1] = header->color_variations;

    uint8_t *component_arrays = cursor;
    cursor += component_arrays_size;
    uint8_t *components = cursor;
    cursor += sizeof(NativeSpriteComponent) * total_components;
    uint8_t *raw = cursor;
    memcpy(raw, source, source_size);

    const uint32_t *raster_offsets =
        (const uint32_t *)(source + header->rasters_offset);
    for (size_t i = 0U; i < raster_count; i++) {
        if (!range_valid(source_size, raster_offsets[i],
                         sizeof(N64SpriteRaster))) return 0U;
        const N64SpriteRaster *input =
            (const N64SpriteRaster *)(source + raster_offsets[i]);
        const char *path = sprite_image_path(asset_path, "raster", i);
        if (path != NULL) {
            if (pb_runtime_resource_texture_type(path) !=
                    PB_RESOURCE_TEXTURE_CI4 ||
                ResourceGetTexWidthByName(path) != input->width ||
                ResourceGetTexHeightByName(path) != input->height) {
                return 0U;
            }
            rasters[i].image = (void *)path;
        } else {
            /* Player images normally live in split companion resources.  A
             * 255x255 entry is Torch's intentional no-image placeholder; a
             * normal-sized missing companion is an incomplete archive, not
             * permission to retain a pointer outside this allocation. */
            const bool placeholder =
                player_sprite && input->width == UINT8_MAX &&
                input->height == UINT8_MAX;
            const size_t pixels =
                ((size_t)input->width * (size_t)input->height + 1U) / 2U;
            if (placeholder) {
                rasters[i].image = NULL;
            } else if (!range_valid(source_size, input->image_offset,
                                    pixels)) {
                return 0U;
            } else {
                rasters[i].image = raw + input->image_offset;
            }
        }
        rasters[i].width = input->width;
        rasters[i].height = input->height;
        rasters[i].palette = input->palette;
        rasters[i].quad_cache_index = input->quad_cache_index;
        raster_array[i] = &rasters[i];
    }
    raster_array[raster_count] = (void *)(uintptr_t)UINTPTR_MAX;

    const uint32_t *palette_offsets =
        (const uint32_t *)(source + header->palettes_offset);
    for (size_t i = 0U; i < palette_count; i++) {
        const char *path = sprite_image_path(asset_path, "pal", i);
        if (path != NULL) {
            if (pb_runtime_resource_texture_type(path) !=
                    PB_RESOURCE_TEXTURE_RGBA16 ||
                ResourceGetTexWidthByName(path) != 16U ||
                ResourceGetTexHeightByName(path) != 1U) {
                return 0U;
            }
            palette_array[i] = (void *)path;
        } else {
            if (!range_valid(source_size, palette_offsets[i], 32U)) {
                return 0U;
            }
            palette_array[i] = raw + palette_offsets[i];
        }
    }
    palette_array[palette_count] = (void *)(uintptr_t)UINTPTR_MAX;

    uint8_t *array_cursor = component_arrays;
    uint8_t *component_cursor = components;
    for (size_t animation = 0U; animation < animation_count; animation++) {
        size_t component_count = 0U;
        if (!count_offsets(source, source_size, animations[animation],
                           &component_count)) return 0U;
        const uint32_t *offsets =
            (const uint32_t *)(source + animations[animation]);
        void **array = (void **)array_cursor;
        native_animations[animation] = array;
        array_cursor += sizeof(void *) * (component_count + 1U);
        for (size_t i = 0U; i < component_count; i++) {
            if (!range_valid(source_size, offsets[i],
                             sizeof(N64SpriteComponent))) return 0U;
            const N64SpriteComponent *input =
                (const N64SpriteComponent *)(source + offsets[i]);
            if (input->command_size <= 0 ||
                (input->command_size & 1) != 0 ||
                !range_valid(source_size, input->command_offset,
                             (size_t)input->command_size)) {
                return 0U;
            }
            NativeSpriteComponent *output =
                (NativeSpriteComponent *)component_cursor;
            output->commands = raw + input->command_offset;
            output->command_size = input->command_size;
            output->x = input->x;
            output->y = input->y;
            output->z = input->z;
            array[i] = output;
            component_cursor += sizeof(*output);
        }
        array[component_count] = (void *)(uintptr_t)UINTPTR_MAX;
    }
    native_animations[animation_count] =
        (void **)(uintptr_t)UINTPTR_MAX;
    return total;
}

static size_t sprite_blob_size(const char *path) {
    if (ResourceGetDataByName(path) == NULL) return 0U;
    return pb_runtime_resource_payload_size(path);
}

void Sprite_Init(void) {
    int32_t values[3];
    sprites_available = 0;
    if (Sprite_GetDataHeader(values)) sprites_available = 1;
}

void Sprite_LoadHeader(void) {
    int32_t values[3];
    (void)Sprite_GetDataHeader(values);
}

SpriteS32 Sprite_AssetsAvailable(void) { return sprites_available; }

SpriteS32 Sprite_GetDataHeader(int32_t *output) {
    if (output == NULL) return 0;
    const char *path = "__OTR__sprites/sprite_data_header";
    uint32_t *data = ResourceGetDataByName(path);
    if (data == NULL || pb_runtime_resource_payload_size(path) < 12U) return 0;
    for (size_t i = 0U; i < 3U; i++) {
        sprite_header[i] = swap32(data[i]);
        output[i] = (int32_t)sprite_header[i];
    }
    sprites_available = 1;
    return 1;
}

size_t Sprite_GetNPCSize(SpriteS32 index) {
    char path[64];
    snprintf(path, sizeof(path), "__OTR__sprites/npc_sprite_%03ld", (long)index);
    const size_t size = sprite_blob_size(path);
    return size != 0U ? convert_sprite(ResourceGetDataByName(path), size,
                                       NULL, 0U, false, index, path) : 0U;
}

void *Sprite_LoadNPC(SpriteS32 index, void *destination, size_t size) {
    char path[64];
    snprintf(path, sizeof(path), "__OTR__sprites/npc_sprite_%03ld", (long)index);
    const size_t blob_size = sprite_blob_size(path);
    return destination != NULL &&
                   convert_sprite(ResourceGetDataByName(path), blob_size,
                                  destination, size, false, index, path) != 0U
               ? destination
               : NULL;
}

SpriteS32 Sprite_GetPlayerRasterHeader(int32_t *output) {
    const char *path = "__OTR__sprites/player_raster_header";
    uint32_t *data = ResourceGetDataByName(path);
    if (output == NULL || data == NULL ||
        pb_runtime_resource_payload_size(path) != 12U) return 0;
    for (size_t i = 0U; i < 3U; i++) output[i] = (int32_t)swap32(data[i]);
    return 1;
}

SpriteS32 Sprite_GetPlayerRasterSets(int32_t *output, SpriteS32 maximum) {
    const char *path = "__OTR__sprites/player_raster_sets";
    uint32_t *data = ResourceGetDataByName(path);
    if (output == NULL || maximum <= 0 || data == NULL) return 0;
    size_t count = pb_runtime_resource_payload_size(path) / sizeof(uint32_t);
    if (count > (size_t)maximum) count = (size_t)maximum;
    for (size_t i = 0U; i < count; i++) output[i] = (int32_t)swap32(data[i]);
    return (SpriteS32)count;
}

SpriteS32 Sprite_GetPlayerSpriteIndexEntry(SpriteS32 index,
                                            int32_t *output) {
    const char *path = "__OTR__sprites/player_sprite_index";
    uint32_t *data = ResourceGetDataByName(path);
    const size_t count = pb_runtime_resource_payload_size(path) / 4U;
    if (output == NULL || data == NULL || index < 0 ||
        (size_t)index + 1U >= count) return 0;
    output[0] = (int32_t)swap32(data[index]);
    output[1] = (int32_t)swap32(data[index + 1]);
    return 1;
}

size_t Sprite_GetPlayerSize(SpriteS32 index) {
    char path[64];
    snprintf(path, sizeof(path), "__OTR__sprites/player_sprite_%ld", (long)index);
    const size_t size = sprite_blob_size(path);
    return size != 0U ? convert_sprite(ResourceGetDataByName(path), size,
                                       NULL, 0U, true, index, path) : 0U;
}

void *Sprite_LoadPlayer(SpriteS32 index, void *destination, size_t size) {
    char path[64];
    snprintf(path, sizeof(path), "__OTR__sprites/player_sprite_%ld", (long)index);
    const size_t blob_size = sprite_blob_size(path);
    return destination != NULL &&
                   convert_sprite(ResourceGetDataByName(path), blob_size,
                                  destination, size, true, index, path) != 0U
               ? destination
               : NULL;
}

SpriteS32 Sprite_GetPlayerRasterLoadDescriptors(SpriteS32 sprite_index,
                                                SpriteS32 start,
                                                int32_t *output,
                                                SpriteS32 count) {
    const char *path = "__OTR__sprites/player_raster_load_descriptors";
    const char *sets_path = "__OTR__sprites/player_raster_sets";
    uint32_t *data = ResourceGetDataByName(path);
    uint32_t *sets = ResourceGetDataByName(sets_path);
    const size_t available = pb_runtime_resource_payload_size(path) / 4U;
    const size_t set_count =
        pb_runtime_resource_payload_size(sets_path) / sizeof(uint32_t);
    if (output == NULL || data == NULL || sets == NULL ||
        sprite_index < 0 || start < 0 ||
        count <= 0 || count > (SpriteS32)PLAYER_RASTER_DESCRIPTOR_LIMIT ||
        (size_t)sprite_index + 1U >= set_count ||
        (size_t)start > available || (size_t)count > available - (size_t)start) {
        return 0;
    }
    const uint32_t begin = swap32(sets[sprite_index]);
    const uint32_t end = swap32(sets[sprite_index + 1]);
    if (begin != (uint32_t)start || end < begin ||
        end - begin != (uint32_t)count || end > available ||
        !player_raster_range_valid(sprite_index, (size_t)count)) {
        return 0;
    }
    for (SpriteS32 i = 0; i < count; i++) {
        output[i] = (int32_t)swap32(data[start + i]);
    }
    return 1;
}

void *Sprite_GetPlayerRasterPath(SpriteS32 sprite_index,
                                 SpriteS32 raster_index) {
    if (sprite_index < 0 || raster_index < 0) return NULL;
    char path[64];
    snprintf(path, sizeof(path), "__OTR__sprites/player_sprite_%ld",
             (long)sprite_index);
    return (void *)sprite_image_path(path, "raster", (size_t)raster_index);
}

SpriteS32 Sprite_LoadPlayerRaster(SpriteS32 offset, void *destination,
                                  SpriteS32 size) {
    const char *path = "__OTR__sprites/player_raster_image_data";
    uint8_t *data = ResourceGetDataByName(path);
    const size_t available = pb_runtime_resource_payload_size(path);
    if (data == NULL || destination == NULL || offset < 0 || size <= 0 ||
        (size_t)offset > available ||
        (size_t)size > available - (size_t)offset) return 0;
    memcpy(destination, data + offset, (size_t)size);
    return 1;
}

uint16_t *port_sprite_palette_data(uint16_t *palette) {
    if (palette == NULL || palette == (uint16_t *)(uintptr_t)UINTPTR_MAX ||
        !GameEngine_OTRSigCheck((const char *)palette)) return palette;
    void *data = ResourceGetDataByName((const char *)palette);
    return data != NULL ? data : palette;
}
