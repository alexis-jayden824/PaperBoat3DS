#include "pb3ds/first_frame.h"

#include <string.h>

#include "pb3ds/texture.h"

#define PB_FIRST_FRAME_TEXTURE_ENTRY "backgrounds/title_bg"
#define PB_FIRST_FRAME_PALETTE_ENTRY "backgrounds/title_bg_pal0"
#define PB_FIRST_FRAME_TEXTURE_RESOURCE_MAX (64U * 1024U)
#define PB_FIRST_FRAME_PALETTE_RESOURCE_MAX 1024U

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

    PBTextureResourceView texture;
    PBTextureResourceView palette;
    if (!pb_texture_resource_parse(texture_data, texture_size, &texture) ||
        !pb_texture_resource_matches(&texture, PB_RESOURCE_TEXTURE_CI8,
                                     PB_FIRST_FRAME_SOURCE_WIDTH,
                                     PB_FIRST_FRAME_SOURCE_HEIGHT)) {
        frame->result = PB_FIRST_FRAME_TEXTURE_INVALID;
        goto finish;
    }
    if (!pb_texture_resource_parse(palette_data, palette_size, &palette) ||
        !pb_texture_resource_matches(&palette, PB_RESOURCE_TEXTURE_RGBA16,
                                     256, 1)) {
        frame->result = PB_FIRST_FRAME_PALETTE_INVALID;
        goto finish;
    }

    PBDecodedTexture decoded;
    const PBTextureDecodeResult decode_result = pb_texture_decode_rgba8(
        &decoded, &texture, &palette, memory);
    if (decode_result == PB_TEXTURE_DECODE_OUT_OF_MEMORY) {
        frame->result = PB_FIRST_FRAME_OUT_OF_MEMORY;
        goto finish;
    }
    if (decode_result != PB_TEXTURE_DECODE_OK ||
        decoded.texture_width != PB_FIRST_FRAME_TEXTURE_WIDTH ||
        decoded.texture_height != PB_FIRST_FRAME_TEXTURE_HEIGHT) {
        pb_decoded_texture_release(&decoded, memory);
        frame->result = PB_FIRST_FRAME_TEXTURE_INVALID;
        goto finish;
    }

    frame->rgba = decoded.rgba;
    frame->rgba_size = decoded.rgba_size;
    frame->source_width = decoded.source_width;
    frame->source_height = decoded.source_height;
    frame->texture_width = decoded.texture_width;
    frame->texture_height = decoded.texture_height;
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
