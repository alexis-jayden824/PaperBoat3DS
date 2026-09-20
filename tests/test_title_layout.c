#include "pb3ds/title_layout.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned int checks_run;

#define CHECK(expression)                                                     \
    do {                                                                      \
        checks_run++;                                                         \
        if (!(expression)) {                                                  \
            fprintf(stderr, "M12.1 title-layout check failed at %s:%d: %s\n", \
                    __FILE__, __LINE__, #expression);                         \
            return false;                                                     \
        }                                                                     \
    } while (0)

static bool check_rect(const PBLayoutRect *rect, int16_t left,
                       int16_t bottom, uint16_t width, uint16_t height) {
    CHECK(rect->left == left);
    CHECK(rect->bottom == bottom);
    CHECK(rect->width == width);
    CHECK(rect->height == height);
    return true;
}

static bool test_new_3ds_xl_layout(void) {
    PBTitleLayout layout;
    CHECK(pb_title_layout_compute(&layout, 400, 240));
    CHECK(check_rect(&layout.safe_canvas, 40, 0, 320, 240));
    CHECK(check_rect(&layout.background, 52, 20, 296, 200));
    CHECK(check_rect(&layout.logo, 100, 113, 200, 112));
    CHECK(check_rect(&layout.prompt, 136, 71, 128, 32));
    CHECK(check_rect(&layout.copyright, 129, 17, 144, 32));
    CHECK(layout.left_pillar == 40);
    CHECK(layout.right_pillar == 40);
    CHECK(PB_TITLE_PROMPT_TINT_RED == 248);
    CHECK(PB_TITLE_PROMPT_TINT_GREEN == 240);
    CHECK(PB_TITLE_PROMPT_TINT_BLUE == 152);
    return true;
}

static bool test_reference_and_rejection(void) {
    PBTitleLayout layout;
    CHECK(pb_title_layout_compute(&layout, 320, 240));
    CHECK(check_rect(&layout.safe_canvas, 0, 0, 320, 240));
    CHECK(check_rect(&layout.background, 12, 20, 296, 200));
    CHECK(layout.left_pillar == 0);
    CHECK(layout.right_pillar == 0);
    CHECK(!pb_title_layout_compute(&layout, 319, 240));
    CHECK(!pb_title_layout_compute(&layout, 400, 239));
    CHECK(!pb_title_layout_compute(NULL, 400, 240));

    const PBLayoutRect outer = { 40, 0, 320, 240 };
    const PBLayoutRect inside = { 52, 20, 296, 200 };
    const PBLayoutRect outside = { 39, 20, 296, 200 };
    CHECK(pb_title_layout_rect_inside(&inside, &outer));
    CHECK(!pb_title_layout_rect_inside(&outside, &outer));
    CHECK(!pb_title_layout_rect_inside(NULL, &outer));
    return true;
}

int main(void) {
    if (!test_new_3ds_xl_layout() || !test_reference_and_rejection()) {
        return EXIT_FAILURE;
    }
    printf("M12.1 400x240 title layout: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
