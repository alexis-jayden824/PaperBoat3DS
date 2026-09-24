#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Paper Mario's fixed title artwork is authored for a 320x240 canvas.  The
 * Nintendo 3DS top LCD is 400x240 on every retail model, including New 3DS XL
 * and New 3DS LL.  Keep the source canvas at 1:1 pixels and center it instead
 * of stretching or cropping it to fill the wider panel.
 */
#define PB_TITLE_REFERENCE_WIDTH 320U
#define PB_TITLE_REFERENCE_HEIGHT 240U

#define PB_TITLE_BACKGROUND_WIDTH 296U
#define PB_TITLE_BACKGROUND_HEIGHT 200U
#define PB_TITLE_LOGO_WIDTH 200U
#define PB_TITLE_LOGO_HEIGHT 112U
#define PB_TITLE_PROMPT_WIDTH 128U
#define PB_TITLE_PROMPT_HEIGHT 32U
#define PB_TITLE_COPYRIGHT_WIDTH 144U
#define PB_TITLE_COPYRIGHT_HEIGHT 32U

#define PB_TITLE_PROMPT_TINT_RED 248U
#define PB_TITLE_PROMPT_TINT_GREEN 240U
#define PB_TITLE_PROMPT_TINT_BLUE 152U

typedef struct {
    int16_t left;
    int16_t bottom;
    uint16_t width;
    uint16_t height;
} PBLayoutRect;

typedef struct {
    PBLayoutRect safe_canvas;
    PBLayoutRect background;
    PBLayoutRect logo;
    PBLayoutRect prompt;
    PBLayoutRect copyright;
    uint16_t left_pillar;
    uint16_t right_pillar;
} PBTitleLayout;

bool pb_title_layout_compute(PBTitleLayout *layout, uint16_t target_width,
                             uint16_t target_height);
bool pb_title_layout_rect_inside(const PBLayoutRect *inner,
                                 const PBLayoutRect *outer);
bool pb_title_prompt_bake_tint(uint8_t *rgba, size_t rgba_size);

#ifdef __cplusplus
}
#endif
