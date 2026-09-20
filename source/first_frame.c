#include "pb3ds/first_frame.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define PB_FIRST_FRAME_TEXTURE_ENTRY "backgrounds/title_bg"
#define PB_FIRST_FRAME_PALETTE_ENTRY "backgrounds/title_bg_pal0"
#define PB_OTR_HEADER_SIZE 64U
#define PB_TEXTURE_BODY_HEADER_SIZE 16U
#define PB_TEXTURE_RESOURCE_HEADER_SIZE \
    (PB_OTR_HEADER_SIZE + PB_TEXTURE_BODY_HEADER_SIZE)
#define PB_OTR_TEXTURE_TYPE 0x4F544558U
#define PB_TEXTURE_TYPE_RGBA16 2U
#define PB_TEXTURE_TYPE_PALETTE8 4U
#define PB_FIRST_FRAME_PALETTE_COLORS 256U
#define PB_FIRST_FRAME_TEXTURE_RESOURCE_MAX (64U * 1024U)
#define PB_FIRST_FRAME_PALETTE_RESOURCE_MAX 1024U

typedef struct {
    uint32_t type;
    uint32_t width;
    uint32_t height;
    uint32_t image_size;
    const uint8_t *image;
} PBTextureResource;

static uint32_t read_u32(const uint8_t *bytes, bool big_endian) {
    if (big_endian) {
        return ((uint32_t)bytes[0] << 24U) |
               ((uint32_t)bytes[1] << 16U) |
               ((uint32_t)bytes[2] << 8U) | (uint32_t)bytes[3];
    }
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static bool parse_texture_resource(const uint8_t *data, size_t size,
                                   PBTextureResource *resource) {
    if (data == NULL || resource == NULL ||
        size < PB_TEXTURE_RESOURCE_HEADER_SIZE || data[0] > 1U) {
        return false;
    }
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
    return resource->image_size ==
           size - PB_TEXTURE_RESOURCE_HEADER_SIZE;
}

static uint8_t expand_five_bits(uint16_t value) {
    const uint8_t five_bits = (uint8_t)(value & 0x1FU);
    return (uint8_t)((five_bits << 3U) | (five_bits >> 2U));
}

static PBFirstFrameResult map_archive_failure(PBO2RResult result) {
    if (result == PB_O2R_ENTRY_NOT_FOUND) {
        return PB_FIRST_FRAME_RESOURCE_MISSING;
    }
    if (result == PB_O2R_OUT_OF_MEMORY) {
        return PB_FIRST_FRAME_OUT_OF_MEMORY;
    }
    return PB_FIRST_FRAME_ARCHIVE_ERROR;
}

void pb_first_frame_init(PBFirstFrame *frame) {
    if (frame != NULL) {
        memset(frame, 0, sizeof(*frame));
        frame->result = PB_FIRST_FRAME_NOT_ATTEMPTED;
        frame->archive_result = PB_O2R_OK;
    }
}

PBFirstFrameResult pb_first_frame_load(PBFirstFrame *frame,
                                       PBArchive *game_archive,
                                       PBMemoryMonitor *memory) {
    if (frame == NULL || memory == NULL) {
        return PB_FIRST_FRAME_ARCHIVE_ERROR;
    }
    pb_first_frame_init(frame);
    if (game_archive == NULL || game_archive->file == NULL) {
        frame->result = PB_FIRST_FRAME_ARCHIVE_MISSING;
        return frame->result;
    }

    PBO2RRequest requests[2] = {
        { .name = PB_FIRST_FRAME_TEXTURE_ENTRY },
        { .name = PB_FIRST_FRAME_PALETTE_ENTRY },
    };
    frame->archive_result = pb_o2r_find_entries(
        game_archive, requests, 2, &frame->archive_stats);
    if (frame->archive_result != PB_O2R_OK) {
        frame->result = map_archive_failure(frame->archive_result);
        return frame->result;
    }

    uint8_t *texture_data = NULL;
    size_t texture_size = 0;
    uint8_t *palette_data = NULL;
    size_t palette_size = 0;
    frame->archive_result = pb_o2r_extract_entry(
        game_archive, &requests[0].entry,
        PB_FIRST_FRAME_TEXTURE_RESOURCE_MAX, memory, PB_MEMORY_SCENE,
        &texture_data, &texture_size, &frame->archive_stats);
    if (frame->archive_result != PB_O2R_OK) {
        frame->result = map_archive_failure(frame->archive_result);
        goto finish;
    }
    frame->archive_result = pb_o2r_extract_entry(
        game_archive, &requests[1].entry,
        PB_FIRST_FRAME_PALETTE_RESOURCE_MAX, memory, PB_MEMORY_SCENE,
        &palette_data, &palette_size, &frame->archive_stats);
    if (frame->archive_result != PB_O2R_OK) {
        frame->result = map_archive_failure(frame->archive_result);
        goto finish;
    }

    PBTextureResource texture;
    PBTextureResource palette;
    if (!parse_texture_resource(texture_data, texture_size, &texture) ||
        texture.type != PB_TEXTURE_TYPE_PALETTE8 ||
        texture.width != PB_FIRST_FRAME_SOURCE_WIDTH ||
        texture.height != PB_FIRST_FRAME_SOURCE_HEIGHT ||
        texture.image_size != texture.width * texture.height) {
        frame->result = PB_FIRST_FRAME_TEXTURE_INVALID;
        goto finish;
    }
    if (!parse_texture_resource(palette_data, palette_size, &palette) ||
        palette.type != PB_TEXTURE_TYPE_RGBA16 ||
        palette.width != PB_FIRST_FRAME_PALETTE_COLORS ||
        palette.height != 1U ||
        palette.image_size != PB_FIRST_FRAME_PALETTE_COLORS * 2U) {
        frame->result = PB_FIRST_FRAME_PALETTE_INVALID;
        goto finish;
    }

    frame->rgba_size = (size_t)PB_FIRST_FRAME_TEXTURE_WIDTH *
                       PB_FIRST_FRAME_TEXTURE_HEIGHT * 4U;
    frame->rgba = pb_memory_alloc(memory, PB_MEMORY_SCENE,
                                  frame->rgba_size);
    if (frame->rgba == NULL) {
        frame->rgba_size = 0;
        frame->result = PB_FIRST_FRAME_OUT_OF_MEMORY;
        goto finish;
    }
    memset(frame->rgba, 0, frame->rgba_size);
    for (uint32_t y = 0; y < texture.height; y++) {
        for (uint32_t x = 0; x < texture.width; x++) {
            const uint8_t palette_index =
                texture.image[(size_t)y * texture.width + x];
            const size_t palette_offset = (size_t)palette_index * 2U;
            const uint16_t color =
                ((uint16_t)palette.image[palette_offset] << 8U) |
                palette.image[palette_offset + 1U];
            const size_t output_offset =
                ((size_t)y * PB_FIRST_FRAME_TEXTURE_WIDTH + x) * 4U;
            frame->rgba[output_offset + 0U] =
                expand_five_bits((uint16_t)(color >> 11U));
            frame->rgba[output_offset + 1U] =
                expand_five_bits((uint16_t)(color >> 6U));
            frame->rgba[output_offset + 2U] =
                expand_five_bits((uint16_t)(color >> 1U));
            frame->rgba[output_offset + 3U] =
                (color & 1U) != 0 ? 255U : 0U;
        }
    }

    frame->source_width = (uint16_t)texture.width;
    frame->source_height = (uint16_t)texture.height;
    frame->texture_width = PB_FIRST_FRAME_TEXTURE_WIDTH;
    frame->texture_height = PB_FIRST_FRAME_TEXTURE_HEIGHT;
    frame->result = PB_FIRST_FRAME_READY;

finish:
    if (palette_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, palette_data, palette_size);
    }
    if (texture_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, texture_data, texture_size);
    }
    return frame->result;
}

void pb_first_frame_release_pixels(PBFirstFrame *frame,
                                   PBMemoryMonitor *memory) {
    if (frame == NULL || memory == NULL || frame->rgba == NULL) {
        return;
    }
    pb_memory_free(memory, PB_MEMORY_SCENE, frame->rgba, frame->rgba_size);
    frame->rgba = NULL;
}

const char *pb_first_frame_result_name(PBFirstFrameResult result) {
    switch (result) {
        case PB_FIRST_FRAME_NOT_ATTEMPTED:
            return "not attempted";
        case PB_FIRST_FRAME_READY:
            return "title_bg ready";
        case PB_FIRST_FRAME_ARCHIVE_MISSING:
            return "pm64.o2r missing";
        case PB_FIRST_FRAME_RESOURCE_MISSING:
            return "title resource missing";
        case PB_FIRST_FRAME_ARCHIVE_ERROR:
            return "archive rejected";
        case PB_FIRST_FRAME_TEXTURE_INVALID:
            return "title texture invalid";
        case PB_FIRST_FRAME_PALETTE_INVALID:
            return "title palette invalid";
        case PB_FIRST_FRAME_OUT_OF_MEMORY:
            return "memory budget";
        default:
            return "unknown";
    }
}
