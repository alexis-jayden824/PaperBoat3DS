#include "pb3ds/tev.h"

#include "pb3ds/log.h"

#include <stdio.h>
#include <string.h>

static PBTevDiag g_tev;

static uint8_t mux_c(unsigned value) {
    if (value > 15U) {
        return PB_TEV_ZERO;
    }
    return (uint8_t)value;
}

static uint8_t mux_a(unsigned value) {
    return (uint8_t)(value & 7U);
}

static uint8_t classify(const PBTevCycle *c) {
    if (c->a == PB_TEV_TEXEL0 && c->c == PB_TEV_SHADE) {
        return PB_TEV_MODE_MODULATE;
    }
    if (c->a == PB_TEV_TEXEL0 && c->c == 0U) {
        return PB_TEV_MODE_REPLACE;
    }
    if (c->a == PB_TEV_PRIMITIVE || c->d == PB_TEV_PRIMITIVE) {
        return PB_TEV_MODE_PRIMITIVE;
    }
    if (c->a == PB_TEV_SHADE || c->d == PB_TEV_SHADE) {
        return PB_TEV_MODE_SHADE;
    }
    return PB_TEV_MODE_FALLBACK_MODULATE;
}

void pb_tev_reset(void) {
    memset(&g_tev, 0, sizeof(g_tev));
    g_tev.mode = PB_TEV_MODE_MODULATE;
    g_tev.supported = true;
}

void pb_tev_set_combine(uint32_t w0, uint32_t w1) {
    PBTevCycle *c0 = &g_tev.cycle[0];
    PBTevCycle *c1 = &g_tev.cycle[1];

    g_tev.raw0 = w0;
    g_tev.raw1 = w1;
    g_tev.configures++;

    c0->a = mux_c((w0 >> 20) & 0x0FU);
    c0->c = mux_c((w0 >> 15) & 0x1FU);
    c0->aa = mux_a((w0 >> 12) & 0x07U);
    c0->ac = mux_a((w0 >> 9) & 0x07U);
    c1->a = mux_c((w0 >> 5) & 0x0FU);
    c1->c = mux_c(w0 & 0x1FU);

    c0->b = mux_c((w1 >> 28) & 0x0FU);
    c1->b = mux_c((w1 >> 24) & 0x0FU);
    c1->aa = mux_a((w1 >> 21) & 0x07U);
    c1->ac = mux_a((w1 >> 18) & 0x07U);
    c0->d = mux_c((w1 >> 15) & 0x07U);
    c0->ab = mux_a((w1 >> 12) & 0x07U);
    c0->ad = mux_a((w1 >> 9) & 0x07U);
    c1->d = mux_c((w1 >> 6) & 0x07U);
    c1->ab = mux_a((w1 >> 3) & 0x07U);
    c1->ad = mux_a(w1 & 0x07U);

    g_tev.mode = classify(c0);
    g_tev.supported = g_tev.mode != PB_TEV_MODE_FALLBACK_MODULATE;
    if (!g_tev.supported) {
        char line[80];

        g_tev.fallbacks++;
        if (g_tev.logged_unknown < 8U) {
            snprintf(line, sizeof(line), "combine %08lx %08lx",
                     (unsigned long)w0, (unsigned long)w1);
            pb_log(PB_LOG_WARNING, "combiner", line);
            g_tev.logged_unknown++;
        }
        g_tev.mode = PB_TEV_MODE_FALLBACK_MODULATE;
    }
}

void pb_tev_query(PBTevDiag *diag) {
    if (diag == NULL) {
        return;
    }
    *diag = g_tev;
}

uint8_t pb_tev_mode(void) {
    return g_tev.mode;
}
