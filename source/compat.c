#include "pb3ds/compat.h"
#include "pb3ds/f3d.h"
#include "pb3ds/fs.h"
#include "pb3ds/gfx.h"
#include "pb3ds/input.h"
#include "pb3ds/log.h"
#include "pb3ds/time.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static PBCompatState g_compat;
static bool g_ready;

static void format_log(PBLogLevel level, const char *fmt, va_list args) {
    char message[192];

    vsnprintf(message, sizeof(message), fmt != NULL ? fmt : "", args);
    pb_log(level, "compat", message);
}

void pb_compat_init(void) {
    memset(&g_compat, 0, sizeof(g_compat));
    g_compat.resources = PB_COMPAT_READY;
    g_compat.logging = PB_COMPAT_READY;
    g_compat.config = PB_COMPAT_DEFERRED_M15;
    g_compat.controller = PB_COMPAT_READY;
    g_compat.time = PB_COMPAT_READY;
    g_compat.gfx = PB_COMPAT_READY;
    g_compat.audio = PB_COMPAT_DEFERRED_M14;
    g_ready = true;
}

void pb_compat_query(PBCompatState *state) {
    if (!g_ready) {
        pb_compat_init();
    }
    if (state == NULL) {
        return;
    }
    *state = g_compat;
}

const char *pb_compat_status_name(PBCompatStatus status) {
    switch (status) {
        case PB_COMPAT_READY:
            return "ready";
        case PB_COMPAT_UNSUPPORTED:
            return "unsupported";
        case PB_COMPAT_DEFERRED_M8:
            return "deferred-m8";
        case PB_COMPAT_DEFERRED_M9:
            return "deferred-m9";
        case PB_COMPAT_DEFERRED_M10:
            return "deferred-m10";
        case PB_COMPAT_DEFERRED_M11:
            return "deferred-m11";
        case PB_COMPAT_DEFERRED_M14:
            return "deferred-m14";
        case PB_COMPAT_DEFERRED_M15:
            return "deferred-m15";
        default:
            return "unknown";
    }
}

PBCompatStatus pb_compat_unsupported(const char *symbol) {
    if (!g_ready) {
        pb_compat_init();
    }
    snprintf(g_compat.last_unsupported, sizeof(g_compat.last_unsupported), "%s",
             symbol != NULL ? symbol : "unknown");
    pb_log(PB_LOG_ERROR, "compat", g_compat.last_unsupported);
    return PB_COMPAT_UNSUPPORTED;
}

bool pb_compat_desktop_engine_allowed(void) {
    return false;
}

void pb_compat_poll_controller(PBOSContPad *pad) {
    PBInputSample sample;

    if (pad == NULL) {
        return;
    }
    pb_input_last(&sample);
    pb_input_map_n64(&sample, pad);
}

uint64_t pb_compat_tick_ms(void) {
    return pb_time_ms();
}

void *ResourceGetDataByName(const char *name) {
    if (!g_ready) {
        pb_compat_init();
    }
    return pb_fs_lookup(name, NULL);
}

void *GameEngine_GetDataExact(const char *name) {
    return ResourceGetDataByName(name);
}

void GameEngine_LogInfo(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    format_log(PB_LOG_INFO, fmt, args);
    va_end(args);
}

void GameEngine_LogWarn(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    format_log(PB_LOG_WARNING, fmt, args);
    va_end(args);
}

void GameEngine_LogError(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    format_log(PB_LOG_ERROR, fmt, args);
    va_end(args);
}

void Graphics_PushFrame(void *display_list) {
    pb_gfx_submit_dl(display_list, 0);
    if (display_list != NULL) {
        pb_f3d_execute(display_list, 0);
    }
}

void GameEngine_StartAudioFrame(void) {
}

void GameEngine_EndAudioFrame(void) {
}

void GameEngine_HoldFrame(void) {
    pb_loop_hold_frame();
}

int GameEngine_GetSaveFilePath(char *dst, unsigned dst_size) {
    if (dst == NULL || dst_size == 0U) {
        return -1;
    }
    snprintf(dst, dst_size, "%s", PB_FS_SDMC_ROOT);
    return 0;
}

int CVarGetInteger(const char *name, int default_value) {
    (void)name;
    return default_value;
}

float CVarGetFloat(const char *name, float default_value) {
    (void)name;
    return default_value;
}

void CVarSetInteger(const char *name, int value) {
    (void)name;
    (void)value;
}
