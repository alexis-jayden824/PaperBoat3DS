#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PaperBoat 1.0.1 title-screen OTR names and US TITLE_DATA sizes from
 * assets/yaml/us/title_screen.yml. M12 binds these blobs; it does not
 * invent pixels or compile state_title_screen.c.
 */
#define PB_TITLE_OTR_LOGO "__OTR__title_screen/title_logo"
#define PB_TITLE_OTR_COPYRIGHT "__OTR__title_screen/title_copyright"
#define PB_TITLE_OTR_PRESS_START "__OTR__title_screen/title_press_start"
#define PB_TITLE_BG_NAME "title_bg"

#define PB_TITLE_LOGO_WIDTH 200U
#define PB_TITLE_LOGO_HEIGHT 112U
#define PB_TITLE_LOGO_BYTES 89600U
#define PB_TITLE_COPYRIGHT_WIDTH 144U
#define PB_TITLE_COPYRIGHT_HEIGHT 32U
#define PB_TITLE_COPYRIGHT_BYTES 4608U
#define PB_TITLE_PRESS_WIDTH 128U
#define PB_TITLE_PRESS_HEIGHT 32U
#define PB_TITLE_PRESS_BYTES 4096U

/* PaperBoat US layout in the 320×240 source (state_title_screen.c). */
#define PB_TITLE_LOGO_SOURCE_X 60U
#define PB_TITLE_LOGO_SOURCE_Y 15U

typedef enum {
    PB_TITLE_WAITING_ASSETS = 0,
    PB_TITLE_INIT,
    PB_TITLE_APPEAR,
    PB_TITLE_HOLD,
} PBTitlePhase;

typedef struct {
    PBTitlePhase phase;
    bool logo_bound;
    bool copyright_bound;
    bool press_start_bound;
    bool bg_bound;
    bool resources_bound;
    bool presented;
    uint16_t appear_delay;
    size_t logo_bytes;
    size_t copyright_bytes;
    size_t press_start_bytes;
    const void *logo;
    const void *copyright;
    const void *press_start;
} PBTitleState;

void pb_title_init(void);
void pb_title_shutdown(void);
void pb_title_step(void);
void pb_title_query(PBTitleState *state);
void pb_title_logo_uv(float *s0, float *t0, float *s1, float *t1);
bool pb_title_resources_bound(void);
bool pb_title_presented(void);
const char *pb_title_phase_name(PBTitlePhase phase);

#ifdef __cplusplus
}
#endif
