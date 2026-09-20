#include "pb3ds/title_layout.h"

#include <stddef.h>
#include <string.h>

bool pb_title_layout_rect_inside(const PBLayoutRect *inner,
                                 const PBLayoutRect *outer) {
    if (inner == NULL || outer == NULL || inner->width == 0 ||
        inner->height == 0 || outer->width == 0 || outer->height == 0) {
        return false;
    }

    const int32_t inner_right = (int32_t)inner->left + inner->width;
    const int32_t inner_top = (int32_t)inner->bottom + inner->height;
    const int32_t outer_right = (int32_t)outer->left + outer->width;
    const int32_t outer_top = (int32_t)outer->bottom + outer->height;
    return inner->left >= outer->left && inner->bottom >= outer->bottom &&
           inner_right <= outer_right && inner_top <= outer_top;
}

bool pb_title_layout_compute(PBTitleLayout *layout, uint16_t target_width,
                             uint16_t target_height) {
    if (layout == NULL) {
        return false;
    }
    memset(layout, 0, sizeof(*layout));
    if (target_width < PB_TITLE_REFERENCE_WIDTH ||
        target_height < PB_TITLE_REFERENCE_HEIGHT) {
        return false;
    }

    const uint16_t horizontal_extra =
        (uint16_t)(target_width - PB_TITLE_REFERENCE_WIDTH);
    const uint16_t vertical_extra =
        (uint16_t)(target_height - PB_TITLE_REFERENCE_HEIGHT);
    const int16_t safe_left = (int16_t)(horizontal_extra / 2U);
    const int16_t safe_bottom = (int16_t)(vertical_extra / 2U);

    layout->safe_canvas = (PBLayoutRect) {
        safe_left, safe_bottom, PB_TITLE_REFERENCE_WIDTH,
        PB_TITLE_REFERENCE_HEIGHT
    };
    layout->background = (PBLayoutRect) {
        (int16_t)(safe_left + 12), (int16_t)(safe_bottom + 20),
        PB_TITLE_BACKGROUND_WIDTH, PB_TITLE_BACKGROUND_HEIGHT
    };
    layout->logo = (PBLayoutRect) {
        (int16_t)(safe_left + 60), (int16_t)(safe_bottom + 113),
        PB_TITLE_LOGO_WIDTH, PB_TITLE_LOGO_HEIGHT
    };
    layout->prompt = (PBLayoutRect) {
        (int16_t)(safe_left + 96), (int16_t)(safe_bottom + 71),
        PB_TITLE_PROMPT_WIDTH, PB_TITLE_PROMPT_HEIGHT
    };
    layout->copyright = (PBLayoutRect) {
        (int16_t)(safe_left + 89), (int16_t)(safe_bottom + 17),
        PB_TITLE_COPYRIGHT_WIDTH, PB_TITLE_COPYRIGHT_HEIGHT
    };
    layout->left_pillar = (uint16_t)safe_left;
    layout->right_pillar =
        (uint16_t)(horizontal_extra - layout->left_pillar);

    return pb_title_layout_rect_inside(&layout->background,
                                       &layout->safe_canvas) &&
           pb_title_layout_rect_inside(&layout->logo,
                                       &layout->safe_canvas) &&
           pb_title_layout_rect_inside(&layout->prompt,
                                       &layout->safe_canvas) &&
           pb_title_layout_rect_inside(&layout->copyright,
                                       &layout->safe_canvas);
}
