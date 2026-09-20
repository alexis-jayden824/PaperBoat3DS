#include "pb3ds/title_flow.h"

#include <stddef.h>
#include <string.h>

#define PB_TITLE_LOGO_ENTRY "title_screen/title_logo_img"
#define PB_TITLE_PROMPT_ENTRY "title_screen/title_press_start_img"
#define PB_TITLE_COPYRIGHT_ENTRY "title_screen/title_copyright_img"
#define PB_TITLE_LOGO_RESOURCE_MAX (96U * 1024U)
#define PB_TITLE_SMALL_RESOURCE_MAX (8U * 1024U)
#define PB_TITLE_STICK_TRIGGER 40
#define PB_TITLE_STICK_RELEASE 20

static PBTitleAssetsResult map_archive_failure(PBO2RResult result) {
    if (result == PB_O2R_ENTRY_NOT_FOUND) {
        return PB_TITLE_ASSETS_RESOURCE_MISSING;
    }
    if (result == PB_O2R_OUT_OF_MEMORY) {
        return PB_TITLE_ASSETS_OUT_OF_MEMORY;
    }
    return PB_TITLE_ASSETS_ARCHIVE_ERROR;
}

static PBTitleAssetsResult extract_texture(
    PBTitleAssets *assets, PBArchive *archive, const PBO2REntry *entry,
    size_t maximum_size, PBResourceTextureType expected_type,
    uint16_t expected_width, uint16_t expected_height,
    PBMemoryMonitor *memory, PBDecodedTexture *decoded) {
    uint8_t *data = NULL;
    size_t size = 0;
    assets->archive_result = pb_o2r_extract_entry(
        archive, entry, maximum_size, memory, PB_MEMORY_SCENE, &data, &size,
        &assets->archive_stats);
    if (assets->archive_result != PB_O2R_OK) {
        return map_archive_failure(assets->archive_result);
    }

    PBTextureResourceView resource;
    if (!pb_texture_resource_parse(data, size, &resource) ||
        !pb_texture_resource_matches(&resource, expected_type,
                                     expected_width, expected_height)) {
        pb_memory_free(memory, PB_MEMORY_SCENE, data, size);
        return PB_TITLE_ASSETS_TEXTURE_INVALID;
    }

    assets->decode_result =
        pb_texture_decode_rgba8(decoded, &resource, NULL, memory);
    pb_memory_free(memory, PB_MEMORY_SCENE, data, size);
    if (assets->decode_result == PB_TEXTURE_DECODE_OUT_OF_MEMORY) {
        return PB_TITLE_ASSETS_OUT_OF_MEMORY;
    }
    if (assets->decode_result != PB_TEXTURE_DECODE_OK) {
        return PB_TITLE_ASSETS_TEXTURE_INVALID;
    }
    return PB_TITLE_ASSETS_READY;
}

void pb_title_assets_init(PBTitleAssets *assets) {
    if (assets == NULL) {
        return;
    }
    memset(assets, 0, sizeof(*assets));
    assets->result = PB_TITLE_ASSETS_NOT_ATTEMPTED;
    assets->archive_result = PB_O2R_OK;
    assets->decode_result = PB_TEXTURE_DECODE_OK;
}

PBTitleAssetsResult pb_title_assets_load(PBTitleAssets *assets,
                                         PBArchive *game_archive,
                                         PBMemoryMonitor *memory) {
    if (assets == NULL || memory == NULL) {
        return PB_TITLE_ASSETS_ARCHIVE_ERROR;
    }
    pb_title_assets_init(assets);
    if (game_archive == NULL || game_archive->file == NULL) {
        assets->result = PB_TITLE_ASSETS_ARCHIVE_MISSING;
        return assets->result;
    }

    PBO2RRequest requests[3] = {
        { .name = PB_TITLE_LOGO_ENTRY },
        { .name = PB_TITLE_PROMPT_ENTRY },
        { .name = PB_TITLE_COPYRIGHT_ENTRY },
    };
    assets->archive_result = pb_o2r_find_entries(
        game_archive, requests, 3, &assets->archive_stats);
    if (assets->archive_result != PB_O2R_OK) {
        assets->result = map_archive_failure(assets->archive_result);
        return assets->result;
    }

    assets->result = extract_texture(
        assets, game_archive, &requests[0].entry,
        PB_TITLE_LOGO_RESOURCE_MAX, PB_RESOURCE_TEXTURE_RGBA32,
        PB_TITLE_LOGO_WIDTH, PB_TITLE_LOGO_HEIGHT, memory, &assets->logo);
    if (assets->result != PB_TITLE_ASSETS_READY) {
        goto fail;
    }
    assets->result = extract_texture(
        assets, game_archive, &requests[1].entry,
        PB_TITLE_SMALL_RESOURCE_MAX, PB_RESOURCE_TEXTURE_IA8,
        PB_TITLE_PROMPT_WIDTH, PB_TITLE_PROMPT_HEIGHT, memory,
        &assets->prompt);
    if (assets->result != PB_TITLE_ASSETS_READY) {
        goto fail;
    }
    assets->result = extract_texture(
        assets, game_archive, &requests[2].entry,
        PB_TITLE_SMALL_RESOURCE_MAX, PB_RESOURCE_TEXTURE_IA8,
        PB_TITLE_COPYRIGHT_WIDTH, PB_TITLE_COPYRIGHT_HEIGHT, memory,
        &assets->copyright);
    if (assets->result != PB_TITLE_ASSETS_READY) {
        goto fail;
    }
    return assets->result;

fail:
    pb_title_assets_release_pixels(assets, memory);
    return assets->result;
}

void pb_title_assets_release_pixels(PBTitleAssets *assets,
                                    PBMemoryMonitor *memory) {
    if (assets == NULL || memory == NULL) {
        return;
    }
    pb_decoded_texture_release(&assets->copyright, memory);
    pb_decoded_texture_release(&assets->prompt, memory);
    pb_decoded_texture_release(&assets->logo, memory);
}

const char *pb_title_assets_result_name(PBTitleAssetsResult result) {
    switch (result) {
        case PB_TITLE_ASSETS_NOT_ATTEMPTED:
            return "not attempted";
        case PB_TITLE_ASSETS_READY:
            return "title assets ready";
        case PB_TITLE_ASSETS_ARCHIVE_MISSING:
            return "pm64.o2r missing";
        case PB_TITLE_ASSETS_RESOURCE_MISSING:
            return "title asset missing";
        case PB_TITLE_ASSETS_ARCHIVE_ERROR:
            return "archive rejected";
        case PB_TITLE_ASSETS_TEXTURE_INVALID:
            return "title asset invalid";
        case PB_TITLE_ASSETS_OUT_OF_MEMORY:
            return "memory budget";
        default:
            return "unknown";
    }
}

static int8_t stick_direction(int8_t value, int8_t previous) {
    if (previous != 0 && value > -PB_TITLE_STICK_RELEASE &&
        value < PB_TITLE_STICK_RELEASE) {
        return 0;
    }
    if (previous == 0 && value >= PB_TITLE_STICK_TRIGGER) {
        return 1;
    }
    if (previous == 0 && value <= -PB_TITLE_STICK_TRIGGER) {
        return -1;
    }
    return previous;
}

static uint8_t prompt_alpha_for_frame(uint64_t frame) {
    const unsigned int phase = (unsigned int)(frame & 31U);
    if (phase == 0U) {
        return 0U;
    }
    if (phase == 1U || phase == 17U) {
        return 128U;
    }
    return phase < 17U ? 255U : 0U;
}

void pb_title_flow_init(PBTitleFlow *flow) {
    if (flow == NULL) {
        return;
    }
    memset(flow, 0, sizeof(*flow));
    flow->screen = PB_TITLE_FLOW_TITLE;
}

PBTitleFlowEvent pb_title_flow_update(PBTitleFlow *flow,
                                      const PBInputState *input) {
    if (flow == NULL || input == NULL) {
        return PB_TITLE_FLOW_EVENT_NONE;
    }
    flow->frame_index++;
    flow->prompt_alpha = prompt_alpha_for_frame(flow->frame_index);
    flow->last_event = PB_TITLE_FLOW_EVENT_NONE;

    const int8_t old_x = flow->previous_stick_x_direction;
    const int8_t old_y = flow->previous_stick_y_direction;
    flow->previous_stick_x_direction =
        stick_direction(input->stick_x, old_x);
    flow->previous_stick_y_direction =
        stick_direction(input->stick_y, old_y);
    const bool stick_left = old_x == 0 &&
                            flow->previous_stick_x_direction < 0;
    const bool stick_right = old_x == 0 &&
                             flow->previous_stick_x_direction > 0;
    const bool stick_down = old_y == 0 &&
                            flow->previous_stick_y_direction < 0;
    const bool stick_up = old_y == 0 &&
                          flow->previous_stick_y_direction > 0;

    if (flow->screen == PB_TITLE_FLOW_TITLE) {
        if ((input->n64_pressed & (PB_N64_A | PB_N64_START)) != 0) {
            flow->screen = PB_TITLE_FLOW_FILE_SELECT;
            flow->slot_confirmed = false;
            flow->transition_count++;
            flow->last_event = PB_TITLE_FLOW_EVENT_ENTER_FILE_SELECT;
        }
        return flow->last_event;
    }

    if ((input->n64_pressed & PB_N64_B) != 0) {
        flow->screen = PB_TITLE_FLOW_TITLE;
        flow->slot_confirmed = false;
        flow->transition_count++;
        flow->last_event = PB_TITLE_FLOW_EVENT_RETURN_TITLE;
        return flow->last_event;
    }

    uint8_t row = (uint8_t)(flow->selected_slot / 2U);
    uint8_t column = (uint8_t)(flow->selected_slot % 2U);
    const bool left = stick_left ||
        (input->n64_pressed & (PB_N64_C_LEFT | PB_N64_D_LEFT)) != 0;
    const bool right = stick_right ||
        (input->n64_pressed & (PB_N64_C_RIGHT | PB_N64_D_RIGHT)) != 0;
    const bool up = stick_up ||
        (input->n64_pressed & (PB_N64_C_UP | PB_N64_D_UP)) != 0;
    const bool down = stick_down ||
        (input->n64_pressed & (PB_N64_C_DOWN | PB_N64_D_DOWN)) != 0;
    const uint8_t previous_slot = flow->selected_slot;
    if (left && column > 0U) {
        column--;
    } else if (right && column < 1U) {
        column++;
    }
    if (up && row > 0U) {
        row--;
    } else if (down && row < 1U) {
        row++;
    }
    flow->selected_slot = (uint8_t)(row * 2U + column);
    if (flow->selected_slot != previous_slot) {
        flow->slot_confirmed = false;
        flow->last_event = PB_TITLE_FLOW_EVENT_MOVE_SLOT;
    }

    if ((input->n64_pressed & (PB_N64_A | PB_N64_START)) != 0) {
        flow->slot_confirmed = true;
        flow->confirmation_count++;
        flow->last_event = PB_TITLE_FLOW_EVENT_CONFIRM_SLOT;
    }
    return flow->last_event;
}

const char *pb_title_flow_screen_name(PBTitleFlowScreen screen) {
    switch (screen) {
        case PB_TITLE_FLOW_TITLE:
            return "title";
        case PB_TITLE_FLOW_FILE_SELECT:
            return "file select";
        default:
            return "unknown";
    }
}

const char *pb_title_flow_event_name(PBTitleFlowEvent event) {
    switch (event) {
        case PB_TITLE_FLOW_EVENT_NONE:
            return "idle";
        case PB_TITLE_FLOW_EVENT_ENTER_FILE_SELECT:
            return "entered file select";
        case PB_TITLE_FLOW_EVENT_MOVE_SLOT:
            return "moved slot";
        case PB_TITLE_FLOW_EVENT_CONFIRM_SLOT:
            return "slot selected";
        case PB_TITLE_FLOW_EVENT_RETURN_TITLE:
            return "returned to title";
        default:
            return "unknown";
    }
}
