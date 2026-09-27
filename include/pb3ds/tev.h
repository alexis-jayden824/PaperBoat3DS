#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * N64 combiner → portable TEV description. gfx_pica.c is the only TU that
 * programs PICA Texture Environment registers from this state.
 */
#define PB_TEV_COMBINED 0U
#define PB_TEV_TEXEL0 1U
#define PB_TEV_TEXEL1 2U
#define PB_TEV_PRIMITIVE 3U
#define PB_TEV_SHADE 4U
#define PB_TEV_ENVIRONMENT 5U
#define PB_TEV_ZERO 15U

#define PB_TEV_MODE_MODULATE 0U
#define PB_TEV_MODE_REPLACE 1U
#define PB_TEV_MODE_PRIMITIVE 2U
#define PB_TEV_MODE_SHADE 3U
#define PB_TEV_MODE_FALLBACK_MODULATE 4U

typedef struct {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t aa;
    uint8_t ab;
    uint8_t ac;
    uint8_t ad;
} PBTevCycle;

typedef struct {
    uint32_t raw0;
    uint32_t raw1;
    PBTevCycle cycle[2];
    uint8_t mode;
    uint32_t configures;
    uint32_t fallbacks;
    uint32_t logged_unknown;
    bool supported;
} PBTevDiag;

void pb_tev_reset(void);
void pb_tev_set_combine(uint32_t w0, uint32_t w1);
void pb_tev_query(PBTevDiag *diag);
uint8_t pb_tev_mode(void);

#ifdef __cplusplus
}
#endif
