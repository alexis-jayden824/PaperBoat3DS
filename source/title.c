#include "pb3ds/title.h"

#include "pb3ds/fs.h"
#include "pb3ds/gfx.h"
#include "pb3ds/log.h"

#include <stdio.h>
#include <string.h>

static PBTitleState g_title;
static bool g_ready;

static void *lookup_title(const char *otr_name, size_t expected, size_t *size_out) {
    const char *names[3];
    char img_name[96];
    char stripped[96];
    size_t index;
    size_t size = 0;
    void *data;

    if (otr_name == NULL) {
        return NULL;
    }
    snprintf(img_name, sizeof(img_name), "%s_img", otr_name);
    if (strncmp(otr_name, "__OTR__", 7) == 0) {
        snprintf(stripped, sizeof(stripped), "%s", otr_name + 7);
    } else {
        stripped[0] = '\0';
    }
    names[0] = otr_name;
    names[1] = img_name;
    names[2] = stripped[0] != '\0' ? stripped : NULL;
    for (index = 0; index < 3U; index++) {
        if (names[index] == NULL) {
            continue;
        }
        data = pb_fs_lookup(names[index], &size);
        if (data == NULL) {
            continue;
        }
        if (size != expected) {
            pb_log(PB_LOG_WARNING, "title", names[index]);
            continue;
        }
        if (size_out != NULL) {
            *size_out = size;
        }
        return data;
    }
    if (size_out != NULL) {
        *size_out = 0;
    }
    return NULL;
}

static void bind_resources(void) {
    size_t size = 0;

    g_title.logo = lookup_title(PB_TITLE_OTR_LOGO, PB_TITLE_LOGO_BYTES, &size);
    g_title.logo_bytes = size;
    g_title.logo_bound = g_title.logo != NULL;

    g_title.copyright =
        lookup_title(PB_TITLE_OTR_COPYRIGHT, PB_TITLE_COPYRIGHT_BYTES, &size);
    g_title.copyright_bytes = size;
    g_title.copyright_bound = g_title.copyright != NULL;

    g_title.press_start =
        lookup_title(PB_TITLE_OTR_PRESS_START, PB_TITLE_PRESS_BYTES, &size);
    g_title.press_start_bytes = size;
    g_title.press_start_bound = g_title.press_start != NULL;

    g_title.bg_bound = pb_fs_lookup(PB_TITLE_BG_NAME, NULL) != NULL;
    g_title.resources_bound =
        g_title.logo_bound && g_title.copyright_bound && g_title.press_start_bound;
    g_title.presented = false;
}

void pb_title_init(void) {
    memset(&g_title, 0, sizeof(g_title));
    g_title.phase = PB_TITLE_WAITING_ASSETS;
    g_ready = true;
    bind_resources();
    if (g_title.resources_bound) {
        g_title.phase = PB_TITLE_INIT;
        pb_log(PB_LOG_INFO, "title", "resources bound");
    } else {
        pb_log(PB_LOG_INFO, "title", "waiting for prepared title OTR");
    }
}

void pb_title_shutdown(void) {
    memset(&g_title, 0, sizeof(g_title));
    g_title.phase = PB_TITLE_WAITING_ASSETS;
    g_ready = false;
}

void pb_title_step(void) {
    if (!g_ready) {
        pb_title_init();
    }
    if (!g_title.resources_bound) {
        bind_resources();
        if (g_title.resources_bound) {
            g_title.phase = PB_TITLE_INIT;
            pb_log(PB_LOG_INFO, "title", "resources bound");
        }
    }
    if (!g_title.resources_bound) {
        g_title.phase = PB_TITLE_WAITING_ASSETS;
        g_title.presented = false;
        return;
    }

    switch (g_title.phase) {
        case PB_TITLE_INIT:
            g_title.appear_delay = 3U;
            g_title.phase = PB_TITLE_APPEAR;
            break;
        case PB_TITLE_APPEAR:
            if (g_title.appear_delay > 0U) {
                g_title.appear_delay--;
            }
            if (g_title.appear_delay == 0U) {
                g_title.phase = PB_TITLE_HOLD;
            }
            break;
        case PB_TITLE_HOLD:
        case PB_TITLE_WAITING_ASSETS:
        default:
            break;
    }
    /* Rasterizing the title images is Fast3D (M13). Orientation is ready. */
    g_title.presented = false;
}

void pb_title_logo_uv(float *s0, float *t0, float *s1, float *t1) {
    if (s0 != NULL) {
        *s0 = 0.0f;
    }
    if (s1 != NULL) {
        *s1 = 1.0f;
    }
    if (t0 != NULL) {
        *t0 = pb_gfx_upright_t(0.0f);
    }
    if (t1 != NULL) {
        *t1 = pb_gfx_upright_t(1.0f);
    }
}

void pb_title_query(PBTitleState *state) {
    if (state == NULL) {
        return;
    }
    if (!g_ready) {
        memset(state, 0, sizeof(*state));
        state->phase = PB_TITLE_WAITING_ASSETS;
        return;
    }
    *state = g_title;
}

bool pb_title_resources_bound(void) {
    return g_ready && g_title.resources_bound;
}

bool pb_title_presented(void) {
    return false;
}

const char *pb_title_phase_name(PBTitlePhase phase) {
    switch (phase) {
        case PB_TITLE_WAITING_ASSETS:
            return "waiting";
        case PB_TITLE_INIT:
            return "init";
        case PB_TITLE_APPEAR:
            return "appear";
        case PB_TITLE_HOLD:
            return "hold";
        default:
            return "unknown";
    }
}
