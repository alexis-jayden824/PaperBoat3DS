#include "pb3ds/time.h"

static bool g_loop_ready;
static bool g_paused;
static bool g_have_last;
static uint64_t g_origin_raw;
static uint64_t g_paused_total;
static uint64_t g_pause_raw;
static uint64_t g_last_ms;
static uint64_t g_accum_ms;
static uint32_t g_game_ticks;
static uint32_t g_display_frames;
static uint32_t g_steps_this_frame;

static uint64_t game_now_ms(void) {
    uint64_t raw;

    if (!g_loop_ready) {
        return 0U;
    }
    raw = pb_time_raw_ms();
    if (g_paused) {
        if (raw < g_origin_raw + g_paused_total) {
            return 0U;
        }
        return g_pause_raw - g_origin_raw - g_paused_total;
    }
    if (raw < g_origin_raw + g_paused_total) {
        return 0U;
    }
    return raw - g_origin_raw - g_paused_total;
}

uint64_t pb_time_ms(void) {
    return game_now_ms();
}

void pb_loop_init(void) {
    g_paused = false;
    g_have_last = false;
    g_paused_total = 0U;
    g_pause_raw = 0U;
    g_last_ms = 0U;
    g_accum_ms = 0U;
    g_game_ticks = 0U;
    g_display_frames = 0U;
    g_steps_this_frame = 0U;
    g_origin_raw = pb_time_raw_ms();
    g_loop_ready = true;
}

void pb_loop_shutdown(void) {
    g_loop_ready = false;
    g_paused = false;
    g_have_last = false;
}

void pb_loop_on_apt(PBAptEvent event) {
    uint64_t raw;

    if (!g_loop_ready) {
        return;
    }
    raw = pb_time_raw_ms();
    switch (event) {
        case PB_APT_SUSPEND:
        case PB_APT_SLEEP:
            if (!g_paused) {
                g_pause_raw = raw;
                g_paused = true;
            }
            break;
        case PB_APT_RESTORE:
        case PB_APT_WAKEUP:
            if (g_paused) {
                if (raw > g_pause_raw) {
                    g_paused_total += raw - g_pause_raw;
                }
                g_paused = false;
                g_have_last = false;
                g_accum_ms = 0U;
            }
            break;
        default:
            break;
    }
}

unsigned pb_loop_begin_frame(void) {
    uint64_t now;
    uint64_t dt;
    unsigned steps;

    g_steps_this_frame = 0U;
    if (!g_loop_ready || g_paused) {
        return 0U;
    }
    now = game_now_ms();
    if (!g_have_last) {
        g_last_ms = now;
        g_have_last = true;
        g_display_frames++;
        return 0U;
    }
    dt = now - g_last_ms;
    g_last_ms = now;
    if (dt > (uint64_t)PB_LOOP_TICK_MS * 8U) {
        dt = PB_LOOP_TICK_MS;
    }
    g_accum_ms += dt;
    steps = 0U;
    while (g_accum_ms >= PB_LOOP_TICK_MS && steps < PB_LOOP_MAX_CATCHUP) {
        g_accum_ms -= PB_LOOP_TICK_MS;
        steps++;
        g_game_ticks++;
    }
    g_steps_this_frame = steps;
    g_display_frames++;
    return steps;
}

void pb_loop_hold_frame(void) {
    pb_time_wait_vblank();
}

void pb_loop_query(PBLoopStatus *status) {
    if (status == NULL) {
        return;
    }
    status->now_ms = game_now_ms();
    status->game_ticks = g_game_ticks;
    status->display_frames = g_display_frames;
    status->steps_this_frame = g_steps_this_frame;
    status->paused = g_paused;
}

bool pb_loop_paused(void) {
    return g_paused;
}

bool pb_loop_desktop_window_allowed(void) {
    return false;
}
