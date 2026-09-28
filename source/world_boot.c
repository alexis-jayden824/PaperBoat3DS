#include "pb3ds/world_boot.h"
#include "pb3ds/gbi_command_span.h"

#include <limits.h>
#include <string.h>

#define PB_OTR_HEADER_SIZE 64U
#define PB_OTR_BLOB_TYPE 0x4F424C42U
#define PB_OTR_DISPLAY_LIST_TYPE 0x4F444C54U
#define PB_OTR_VERTEX_TYPE 0x4F565458U
#define PB_WORLD_SHAPE_ENTRY "shapes/mac_00_shape"
#define PB_WORLD_COLLISION_ENTRY "collisions/mac_00_hit"
#define PB_WORLD_VERTEX_ENTRY "shapes/mac_00_shape/vtx"
#define PB_WORLD_DISPLAY_LIST_ENTRY "shapes/mac_00_shape/dlist_20"
#define PB_WORLD_BACKGROUND_ENTRY "backgrounds/nok_bg"
#define PB_WORLD_BACKGROUND_PALETTE_ENTRY "backgrounds/nok_bg_pal0"
#define PB_WORLD_SHAPE_MAX (128U * 1024U)
#define PB_WORLD_COLLISION_MAX (32U * 1024U)
#define PB_WORLD_VERTEX_MAX (64U * 1024U)
#define PB_WORLD_DISPLAY_LIST_MAX (8U * 1024U)
#define PB_WORLD_BACKGROUND_MAX (64U * 1024U)
#define PB_WORLD_PALETTE_MAX 1024U
#define PB_WORLD_MAX_SHAPE_NODES 512U
#define PB_WORLD_MAX_SHAPE_DEPTH 32U
#define PB_WORLD_MAX_NAME_ENTRIES 512U
#define PB_WORLD_MAX_PROPERTIES 4096U
#define PB_WORLD_MAX_CHILD_REFS 2048U
/* Leave allocator metadata/headroom inside the upstream 0x40000-byte heap. */
#define PB_WORLD_COLLISION_HEAP_BYTES 0x3E000U
#define PB_WORLD_BACKGROUND_WIDTH 296U
#define PB_WORLD_BACKGROUND_HEIGHT 200U
#define PB_WORLD_BACKGROUND_TEXTURE_WIDTH 512U
#define PB_WORLD_BACKGROUND_TEXTURE_HEIGHT 256U
#define PB_F3DEX2_UCODE 4U
#define PB_F3DEX2_END_DL 0xDFU

typedef struct {
    const uint8_t *body;
    size_t body_size;
    bool big_endian;
} PBWorldResource;

typedef struct {
    const uint8_t *data;
    size_t size;
} PBWorldBlob;

typedef struct {
    uint32_t offsets[PB_WORLD_MAX_SHAPE_NODES];
    uint32_t count;
    uint32_t display_lists;
    uint32_t properties;
    uint32_t child_refs;
} PBShapeValidation;

static uint16_t read_u16(const uint8_t *bytes, bool big_endian) {
    if (big_endian) {
        return (uint16_t)(((uint16_t)bytes[0] << 8U) | bytes[1]);
    }
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes, bool big_endian) {
    if (big_endian) {
        return ((uint32_t)bytes[0] << 24U) |
               ((uint32_t)bytes[1] << 16U) |
               ((uint32_t)bytes[2] << 8U) | bytes[3];
    }
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static int32_t read_s32(const uint8_t *bytes, bool big_endian) {
    return (int32_t)read_u32(bytes, big_endian);
}

static int16_t read_s16(const uint8_t *bytes, bool big_endian) {
    return (int16_t)read_u16(bytes, big_endian);
}

static bool span_is_valid(size_t size, uint32_t offset, size_t length) {
    return (size_t)offset <= size && length <= size - (size_t)offset;
}

static bool parse_resource(const uint8_t *data, size_t size,
                           uint32_t expected_type,
                           PBWorldResource *resource) {
    if (data == NULL || resource == NULL || size < PB_OTR_HEADER_SIZE ||
        data[0] > 1U) {
        return false;
    }
    const bool big_endian = data[0] == 1U;
    if (read_u32(&data[4], big_endian) != expected_type ||
        read_u32(&data[8], big_endian) != 0U) {
        return false;
    }
    resource->body = &data[PB_OTR_HEADER_SIZE];
    resource->body_size = size - PB_OTR_HEADER_SIZE;
    resource->big_endian = big_endian;
    return true;
}

static bool parse_blob(const uint8_t *data, size_t size, PBWorldBlob *blob) {
    PBWorldResource resource;
    if (blob == NULL ||
        !parse_resource(data, size, PB_OTR_BLOB_TYPE, &resource) ||
        resource.body_size < sizeof(uint32_t)) {
        return false;
    }
    const uint32_t payload_size =
        read_u32(resource.body, resource.big_endian);
    if (payload_size != resource.body_size - sizeof(uint32_t)) {
        return false;
    }
    blob->data = &resource.body[sizeof(uint32_t)];
    blob->size = payload_size;
    return true;
}

static bool validate_name_table(const uint8_t *data, size_t size,
                                uint32_t offset, bool big_endian) {
    for (uint32_t index = 0; index < PB_WORLD_MAX_NAME_ENTRIES; index++) {
        const size_t table_length =
            ((size_t)index + 1U) * sizeof(uint32_t);
        if (!span_is_valid(size, offset, table_length)) {
            return false;
        }
        const size_t entry_offset =
            (size_t)offset + (size_t)index * sizeof(uint32_t);
        const uint32_t string_offset =
            read_u32(&data[entry_offset], big_endian);
        if (string_offset == 0U || string_offset >= size) {
            return false;
        }
        const uint8_t *text = &data[string_offset];
        const size_t available = size - string_offset;
        const uint8_t *terminator = memchr(text, '\0', available);
        if (terminator == NULL) {
            return false;
        }
        if ((size_t)(terminator - text) == 2U && text[0] == 'd' &&
            text[1] == 'b') {
            return true;
        }
    }
    return false;
}

static bool shape_offset_seen(const PBShapeValidation *validation,
                              uint32_t offset) {
    for (uint32_t index = 0; index < validation->count; index++) {
        if (validation->offsets[index] == offset) {
            return true;
        }
    }
    return false;
}

static bool validate_shape_node(const uint8_t *data, size_t size,
                                uint32_t offset, uint32_t depth,
                                bool big_endian,
                                PBShapeValidation *validation) {
    if (depth > PB_WORLD_MAX_SHAPE_DEPTH ||
        validation->count >= PB_WORLD_MAX_SHAPE_NODES ||
        shape_offset_seen(validation, offset) ||
        !span_is_valid(size, offset, 20U)) {
        return false;
    }
    validation->offsets[validation->count++] = offset;
    const uint8_t *node = &data[offset];
    const int32_t type = read_s32(&node[0], big_endian);
    if (type != 2 && type != 5 && type != 7 && type != 10) {
        return false;
    }

    const uint32_t display_offset = read_u32(&node[4], big_endian);
    if (display_offset != 0U) {
        if (!span_is_valid(size, display_offset, 8U) ||
            read_u32(&data[display_offset], big_endian) == 0U) {
            return false;
        }
        validation->display_lists++;
    }

    const int32_t property_count = read_s32(&node[8], big_endian);
    const uint32_t property_offset = read_u32(&node[12], big_endian);
    if (property_count < 0 || property_count > 1024 ||
        (uint32_t)property_count >
            PB_WORLD_MAX_PROPERTIES - validation->properties ||
        (property_count > 0 &&
         (!span_is_valid(size, property_offset,
                         (size_t)property_count * 12U)))) {
        return false;
    }
    validation->properties += (uint32_t)property_count;
    for (int32_t index = 0; index < property_count; index++) {
        const uint8_t *property =
            &data[(size_t)property_offset + (size_t)index * 12U];
        const int32_t key = read_s32(property, big_endian);
        const uint32_t value = read_u32(&property[8], big_endian);
        if (key == 0x5E && value != 0U) {
            if (value >= size ||
                memchr(&data[value], '\0', size - value) == NULL) {
                return false;
            }
        }
    }

    const uint32_t group_offset = read_u32(&node[16], big_endian);
    if (group_offset == 0U) {
        return true;
    }
    if (!span_is_valid(size, group_offset, 20U)) {
        return false;
    }
    const uint8_t *group = &data[group_offset];
    const uint32_t matrix_offset = read_u32(&group[0], big_endian);
    const uint32_t lights_offset = read_u32(&group[4], big_endian);
    const int32_t light_count = read_s32(&group[8], big_endian);
    const int32_t child_count = read_s32(&group[12], big_endian);
    const uint32_t child_offset = read_u32(&group[16], big_endian);
    if (light_count < 0 || light_count > 7 || child_count < 0 ||
        child_count > (int32_t)PB_WORLD_MAX_SHAPE_NODES ||
        (uint32_t)child_count >
            PB_WORLD_MAX_CHILD_REFS - validation->child_refs ||
        (matrix_offset != 0U &&
         !span_is_valid(size, matrix_offset, 64U)) ||
        (lights_offset != 0U &&
         !span_is_valid(size, lights_offset,
                        8U + (size_t)light_count * 16U)) ||
        (child_count > 0 &&
         !span_is_valid(size, child_offset,
                        (size_t)child_count * sizeof(uint32_t)))) {
        return false;
    }
    validation->child_refs += (uint32_t)child_count;
    for (int32_t index = 0; index < child_count; index++) {
        const uint32_t child = read_u32(
            &data[(size_t)child_offset + (size_t)index * sizeof(uint32_t)],
            big_endian);
        if (child == 0U ||
            !validate_shape_node(data, size, child, depth + 1U,
                                 big_endian, validation)) {
            return false;
        }
    }
    return true;
}

bool pb_world_validate_shape_payload(const uint8_t *data, size_t size,
                                     PBWorldBootStats *stats) {
    if (data == NULL || stats == NULL || size < 32U) {
        return false;
    }
    /* Torch's PM64 blob payloads preserve the little-endian port layout. */
    const bool big_endian = false;
    const uint32_t root_offset = read_u32(&data[0], big_endian);
    const uint32_t vertex_offset = read_u32(&data[4], big_endian);
    const uint32_t model_names = read_u32(&data[8], big_endian);
    const uint32_t collider_names = read_u32(&data[12], big_endian);
    const uint32_t zone_names = read_u32(&data[16], big_endian);
    if (root_offset == 0U || vertex_offset == 0U ||
        !span_is_valid(size, root_offset, 20U) ||
        read_s32(&data[root_offset], big_endian) != 7 ||
        !span_is_valid(size, vertex_offset, 16U) ||
        !validate_name_table(data, size, model_names,
                             big_endian) ||
        !validate_name_table(data, size, collider_names,
                             big_endian) ||
        !validate_name_table(data, size, zone_names,
                             big_endian)) {
        return false;
    }
    PBShapeValidation validation;
    memset(&validation, 0, sizeof(validation));
    if (!validate_shape_node(data, size, root_offset, 0U,
                             big_endian, &validation)) {
        return false;
    }
    stats->shape_nodes = validation.count;
    stats->shape_display_lists = validation.display_lists;
    return validation.count > 0U && validation.display_lists > 0U;
}

static bool validate_hit_section(const uint8_t *data, size_t size,
                                 uint32_t offset, uint32_t *colliders,
                                 uint32_t *vertices, uint32_t *triangles,
                                 size_t *heap_bytes) {
    if (!span_is_valid(size, offset, 24U)) {
        return false;
    }
    const uint8_t *header = &data[offset];
    const uint16_t collider_count = read_u16(&header[0], false);
    const uint32_t collider_offset = read_u32(&header[4], false);
    const uint16_t vertex_count = read_u16(&header[8], false);
    const uint32_t vertex_offset = read_u32(&header[12], false);
    const uint16_t bounds_words = read_u16(&header[16], false);
    const uint32_t bounds_offset = read_u32(&header[20], false);
    if (collider_count == 0U || collider_count > 1024U ||
        vertex_count == 0U || vertex_count > 1024U ||
        !span_is_valid(size, collider_offset,
                       (size_t)collider_count * 12U) ||
        !span_is_valid(size, vertex_offset, (size_t)vertex_count * 6U) ||
        !span_is_valid(size, bounds_offset, (size_t)bounds_words * 4U)) {
        return false;
    }
    uint32_t triangle_count = 0U;
    const size_t fixed_heap = (size_t)bounds_words * 4U +
                              (size_t)vertex_count * 12U +
                              (size_t)collider_count * 28U;
    if (fixed_heap > PB_WORLD_COLLISION_HEAP_BYTES - *heap_bytes) {
        return false;
    }
    *heap_bytes += fixed_heap;
    for (uint16_t index = 0; index < collider_count; index++) {
        const uint8_t *collider =
            &data[(size_t)collider_offset + (size_t)index * 12U];
        const int16_t bounds_index = read_s16(&collider[0], false);
        const int16_t next_sibling = read_s16(&collider[2], false);
        const int16_t first_child = read_s16(&collider[4], false);
        const int16_t signed_count = read_s16(&collider[6], false);
        if ((bounds_index >= 0 &&
             ((uint32_t)bounds_index > bounds_words ||
              7U > (uint32_t)bounds_words - (uint32_t)bounds_index)) ||
            next_sibling < -1 || next_sibling >= (int16_t)collider_count ||
            first_child < -1 || first_child >= (int16_t)collider_count ||
            signed_count < 0) {
            return false;
        }
        const uint16_t count = (uint16_t)signed_count;
        const uint32_t triangle_offset = read_u32(&collider[8], false);
        if (count > 0U &&
            !span_is_valid(size, triangle_offset, (size_t)count * 4U)) {
            return false;
        }
        if (UINT32_MAX - triangle_count < count) {
            return false;
        }
        const size_t triangle_heap = (size_t)count * 64U;
        if (triangle_heap > PB_WORLD_COLLISION_HEAP_BYTES - *heap_bytes) {
            return false;
        }
        *heap_bytes += triangle_heap;
        for (uint16_t triangle = 0; triangle < count; triangle++) {
            const uint32_t packed = read_u32(
                &data[(size_t)triangle_offset + (size_t)triangle * 4U],
                false);
            if ((packed & 0x3FFU) >= vertex_count ||
                ((packed >> 10U) & 0x3FFU) >= vertex_count ||
                ((packed >> 20U) & 0x3FFU) >= vertex_count) {
                return false;
            }
        }
        triangle_count += count;
    }
    *colliders = collider_count;
    *vertices = vertex_count;
    *triangles = triangle_count;
    return true;
}

bool pb_world_validate_collision_payload(const uint8_t *data, size_t size,
                                         PBWorldBootStats *stats) {
    if (data == NULL || stats == NULL || size < 8U) {
        return false;
    }
    const uint32_t collision_offset = read_u32(&data[0], false);
    const uint32_t zone_offset = read_u32(&data[4], false);
    size_t heap_bytes = 0U;
    return collision_offset != 0U && zone_offset != 0U &&
           validate_hit_section(
               data, size, collision_offset,
               &stats->collision_colliders, &stats->collision_vertices,
               &stats->collision_triangles, &heap_bytes) &&
           validate_hit_section(data, size, zone_offset,
                                &stats->zone_colliders,
                                &stats->zone_vertices,
                                &stats->zone_triangles, &heap_bytes);
}

static bool validate_vertices(const uint8_t *data, size_t size,
                              uint32_t *vertex_count) {
    PBWorldResource resource;
    if (vertex_count == NULL ||
        !parse_resource(data, size, PB_OTR_VERTEX_TYPE, &resource) ||
        resource.body_size < sizeof(uint32_t)) {
        return false;
    }
    const uint32_t count = read_u32(resource.body, resource.big_endian);
    if (count == 0U || count > 65535U ||
        resource.body_size - sizeof(uint32_t) != (size_t)count * 16U) {
        return false;
    }
    *vertex_count = count;
    return true;
}

static bool validate_display_list(const uint8_t *data, size_t size,
                                  uint32_t *command_count) {
    PBWorldResource resource;
    if (command_count == NULL ||
        !parse_resource(data, size, PB_OTR_DISPLAY_LIST_TYPE, &resource) ||
        resource.body_size < 16U || resource.body[0] != PB_F3DEX2_UCODE) {
        return false;
    }
    size_t offset = 8U;
    uint32_t count = 0U;
    while (offset <= resource.body_size - 8U && count < 1024U) {
        const uint32_t word0 =
            read_u32(&resource.body[offset], resource.big_endian);
        const uint8_t opcode = (uint8_t)(word0 >> 24U);
        const size_t span = pb_gbi_command_span(opcode);
        if (span == 0U || span > (resource.body_size - offset) / 8U ||
            span > UINT32_MAX - count) return false;
        offset += span * 8U;
        count += (uint32_t)span;
        if (opcode == PB_F3DEX2_END_DL) {
            if (offset != resource.body_size) return false;
            *command_count = count;
            return true;
        }
    }
    return false;
}

static PBWorldBootResult map_archive_failure(PBO2RResult result) {
    if (result == PB_O2R_ENTRY_NOT_FOUND) {
        return PB_WORLD_BOOT_RESOURCE_MISSING;
    }
    if (result == PB_O2R_OUT_OF_MEMORY) {
        return PB_WORLD_BOOT_OUT_OF_MEMORY;
    }
    return PB_WORLD_BOOT_ARCHIVE_ERROR;
}

static bool extract(PBWorldBoot *boot, PBArchive *archive,
                    const PBO2REntry *entry, size_t maximum_size,
                    PBMemoryMonitor *memory, uint8_t **data, size_t *size) {
    boot->archive_result = pb_o2r_extract_entry(
        archive, entry, maximum_size, memory, PB_MEMORY_SCENE, data, size,
        &boot->archive_stats);
    return boot->archive_result == PB_O2R_OK;
}

void pb_world_boot_init(PBWorldBoot *boot) {
    if (boot != NULL) {
        memset(boot, 0, sizeof(*boot));
        boot->result = PB_WORLD_BOOT_NOT_ATTEMPTED;
        boot->archive_result = PB_O2R_OK;
    }
}

PBWorldBootResult pb_world_boot_load(PBWorldBoot *boot,
                                     PBArchive *game_archive,
                                     PBMemoryMonitor *memory) {
    if (boot == NULL || memory == NULL) {
        return PB_WORLD_BOOT_ARCHIVE_ERROR;
    }
    pb_world_boot_init(boot);
    if (game_archive == NULL || game_archive->file == NULL) {
        boot->result = PB_WORLD_BOOT_ARCHIVE_MISSING;
        return boot->result;
    }

    PBO2RRequest requests[6] = {
        { .name = PB_WORLD_SHAPE_ENTRY },
        { .name = PB_WORLD_COLLISION_ENTRY },
        { .name = PB_WORLD_VERTEX_ENTRY },
        { .name = PB_WORLD_DISPLAY_LIST_ENTRY },
        { .name = PB_WORLD_BACKGROUND_ENTRY },
        { .name = PB_WORLD_BACKGROUND_PALETTE_ENTRY },
    };
    boot->archive_result = pb_o2r_find_entries(
        game_archive, requests, 6U, &boot->archive_stats);
    if (boot->archive_result != PB_O2R_OK) {
        boot->result = map_archive_failure(boot->archive_result);
        return boot->result;
    }

    uint8_t *shape_data = NULL;
    size_t shape_size = 0U;
    uint8_t *collision_data = NULL;
    size_t collision_size = 0U;
    uint8_t *vertex_data = NULL;
    size_t vertex_size = 0U;
    uint8_t *display_list_data = NULL;
    size_t display_list_size = 0U;
    uint8_t *background_data = NULL;
    size_t background_size = 0U;
    uint8_t *palette_data = NULL;
    size_t palette_size = 0U;

    if (!extract(boot, game_archive, &requests[0].entry,
                 PB_WORLD_SHAPE_MAX, memory, &shape_data, &shape_size)) {
        boot->result = map_archive_failure(boot->archive_result);
        goto finish;
    }
    PBWorldBlob shape_blob;
    if (!parse_blob(shape_data, shape_size, &shape_blob) ||
        !pb_world_validate_shape_payload(shape_blob.data, shape_blob.size,
                                         &boot->world_stats)) {
        boot->result = PB_WORLD_BOOT_SHAPE_INVALID;
        goto finish;
    }
    if (!extract(boot, game_archive, &requests[1].entry,
                 PB_WORLD_COLLISION_MAX, memory, &collision_data,
                 &collision_size)) {
        boot->result = map_archive_failure(boot->archive_result);
        goto finish;
    }
    PBWorldBlob collision_blob;
    if (!parse_blob(collision_data, collision_size, &collision_blob) ||
        !pb_world_validate_collision_payload(collision_blob.data,
                                             collision_blob.size,
                                             &boot->world_stats)) {
        boot->result = PB_WORLD_BOOT_COLLISION_INVALID;
        goto finish;
    }

    if (!extract(boot, game_archive, &requests[2].entry,
                 PB_WORLD_VERTEX_MAX, memory, &vertex_data, &vertex_size)) {
        boot->result = map_archive_failure(boot->archive_result);
        goto finish;
    }
    if (!validate_vertices(vertex_data, vertex_size,
                           &boot->world_stats.vertex_count)) {
        boot->result = PB_WORLD_BOOT_VERTEX_INVALID;
        goto finish;
    }

    if (!extract(boot, game_archive, &requests[3].entry,
                 PB_WORLD_DISPLAY_LIST_MAX, memory, &display_list_data,
                 &display_list_size)) {
        boot->result = map_archive_failure(boot->archive_result);
        goto finish;
    }
    if (!validate_display_list(display_list_data, display_list_size,
                               &boot->world_stats.display_list_commands)) {
        boot->result = PB_WORLD_BOOT_DISPLAY_LIST_INVALID;
        goto finish;
    }

    if (!extract(boot, game_archive, &requests[4].entry,
                 PB_WORLD_BACKGROUND_MAX, memory, &background_data,
                 &background_size) ||
        !extract(boot, game_archive, &requests[5].entry,
                 PB_WORLD_PALETTE_MAX, memory, &palette_data,
                 &palette_size)) {
        boot->result = map_archive_failure(boot->archive_result);
        goto finish;
    }
    PBTextureResourceView background_resource;
    PBTextureResourceView palette_resource;
    if (!pb_texture_resource_parse(background_data, background_size,
                                   &background_resource) ||
        !pb_texture_resource_matches(&background_resource,
                                     PB_RESOURCE_TEXTURE_CI8,
                                     PB_WORLD_BACKGROUND_WIDTH,
                                     PB_WORLD_BACKGROUND_HEIGHT) ||
        !pb_texture_resource_parse(palette_data, palette_size,
                                   &palette_resource) ||
        !pb_texture_resource_matches(&palette_resource,
                                     PB_RESOURCE_TEXTURE_RGBA16, 256U, 1U)) {
        boot->result = PB_WORLD_BOOT_BACKGROUND_INVALID;
        goto finish;
    }
    const PBTextureDecodeResult decode = pb_texture_decode_rgba8(
        &boot->background, &background_resource, &palette_resource, memory);
    if (decode == PB_TEXTURE_DECODE_OUT_OF_MEMORY) {
        boot->result = PB_WORLD_BOOT_OUT_OF_MEMORY;
        goto finish;
    }
    if (decode != PB_TEXTURE_DECODE_OK ||
        boot->background.texture_width !=
            PB_WORLD_BACKGROUND_TEXTURE_WIDTH ||
        boot->background.texture_height !=
            PB_WORLD_BACKGROUND_TEXTURE_HEIGHT) {
        pb_decoded_texture_release(&boot->background, memory);
        boot->result = PB_WORLD_BOOT_BACKGROUND_INVALID;
        goto finish;
    }
    boot->result = PB_WORLD_BOOT_READY;

finish:
    if (palette_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, palette_data, palette_size);
    }
    if (background_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, background_data,
                       background_size);
    }
    if (display_list_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, display_list_data,
                       display_list_size);
    }
    if (vertex_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, vertex_data, vertex_size);
    }
    if (collision_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, collision_data,
                       collision_size);
    }
    if (shape_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, shape_data, shape_size);
    }
    return boot->result;
}

void pb_world_boot_release(PBWorldBoot *boot, PBMemoryMonitor *memory) {
    if (boot == NULL || memory == NULL) {
        return;
    }
    pb_decoded_texture_release(&boot->background, memory);
}

const char *pb_world_boot_result_name(PBWorldBootResult result) {
    switch (result) {
        case PB_WORLD_BOOT_NOT_ATTEMPTED:
            return "not attempted";
        case PB_WORLD_BOOT_READY:
            return "mac_00 ready";
        case PB_WORLD_BOOT_ARCHIVE_MISSING:
            return "pm64.o2r missing";
        case PB_WORLD_BOOT_RESOURCE_MISSING:
            return "map resource missing";
        case PB_WORLD_BOOT_ARCHIVE_ERROR:
            return "archive rejected";
        case PB_WORLD_BOOT_SHAPE_INVALID:
            return "shape invalid";
        case PB_WORLD_BOOT_COLLISION_INVALID:
            return "collision invalid";
        case PB_WORLD_BOOT_VERTEX_INVALID:
            return "vertices invalid";
        case PB_WORLD_BOOT_DISPLAY_LIST_INVALID:
            return "display list invalid";
        case PB_WORLD_BOOT_BACKGROUND_INVALID:
            return "nok_bg invalid";
        case PB_WORLD_BOOT_OUT_OF_MEMORY:
            return "memory budget";
        default:
            return "unknown";
    }
}

void pb_world_flow_init(PBWorldFlow *flow) {
    if (flow != NULL) {
        memset(flow, 0, sizeof(*flow));
        flow->state = PB_WORLD_FLOW_FILE_SELECT;
    }
}

PBWorldFlowEvent pb_world_flow_request(PBWorldFlow *flow,
                                       uint8_t selected_slot) {
    if (flow == NULL ||
        (flow->state != PB_WORLD_FLOW_FILE_SELECT &&
         flow->state != PB_WORLD_FLOW_FAILED) ||
        selected_slot >= 4U) {
        return PB_WORLD_FLOW_EVENT_NONE;
    }
    flow->selected_slot = selected_slot;
    flow->state = PB_WORLD_FLOW_LOADING;
    flow->last_event = PB_WORLD_FLOW_EVENT_LOAD_REQUESTED;
    return flow->last_event;
}

PBWorldFlowEvent pb_world_flow_begin_transition(PBWorldFlow *flow) {
    if (flow == NULL || flow->state != PB_WORLD_FLOW_ACTIVE) {
        return PB_WORLD_FLOW_EVENT_NONE;
    }
    flow->state = PB_WORLD_FLOW_LOADING;
    flow->last_event = PB_WORLD_FLOW_EVENT_LOAD_REQUESTED;
    return flow->last_event;
}

PBWorldFlowEvent pb_world_flow_finish(PBWorldFlow *flow, bool loaded) {
    if (flow == NULL || flow->state != PB_WORLD_FLOW_LOADING) {
        return PB_WORLD_FLOW_EVENT_NONE;
    }
    flow->state = loaded ? PB_WORLD_FLOW_ACTIVE : PB_WORLD_FLOW_FAILED;
    flow->last_event = loaded ? PB_WORLD_FLOW_EVENT_ENTERED_WORLD
                              : PB_WORLD_FLOW_EVENT_LOAD_FAILED;
    return flow->last_event;
}

PBWorldFlowEvent pb_world_flow_update(PBWorldFlow *flow,
                                      const PBInputState *input) {
    if (flow == NULL || input == NULL) {
        return PB_WORLD_FLOW_EVENT_NONE;
    }
    flow->frame_index++;
    flow->last_event = PB_WORLD_FLOW_EVENT_NONE;
    if ((input->n64_pressed & PB_N64_START) == 0U) {
        return flow->last_event;
    }
    if (flow->state == PB_WORLD_FLOW_ACTIVE) {
        flow->state = PB_WORLD_FLOW_PAUSED;
        flow->pause_count++;
        flow->last_event = PB_WORLD_FLOW_EVENT_PAUSED;
    } else if (flow->state == PB_WORLD_FLOW_PAUSED) {
        flow->state = PB_WORLD_FLOW_ACTIVE;
        flow->last_event = PB_WORLD_FLOW_EVENT_RESUMED;
    }
    return flow->last_event;
}

const char *pb_world_flow_state_name(PBWorldFlowState state) {
    switch (state) {
        case PB_WORLD_FLOW_FILE_SELECT:
            return "file select";
        case PB_WORLD_FLOW_LOADING:
            return "loading";
        case PB_WORLD_FLOW_ACTIVE:
            return "world active";
        case PB_WORLD_FLOW_PAUSED:
            return "paused";
        case PB_WORLD_FLOW_FAILED:
            return "load failed";
        default:
            return "unknown";
    }
}

const char *pb_world_flow_event_name(PBWorldFlowEvent event) {
    switch (event) {
        case PB_WORLD_FLOW_EVENT_NONE:
            return "idle";
        case PB_WORLD_FLOW_EVENT_LOAD_REQUESTED:
            return "load requested";
        case PB_WORLD_FLOW_EVENT_ENTERED_WORLD:
            return "entered world";
        case PB_WORLD_FLOW_EVENT_LOAD_FAILED:
            return "load failed";
        case PB_WORLD_FLOW_EVENT_PAUSED:
            return "paused";
        case PB_WORLD_FLOW_EVENT_RESUMED:
            return "resumed";
        default:
            return "unknown";
    }
}
