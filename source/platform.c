#include "pb3ds/platform.h"

#include <stdio.h>
#include <string.h>

#ifdef __3DS__
#include <3ds.h>
#endif

static PBInputSample g_input_last;

#ifndef __3DS__
static uint32_t host_held;
static uint32_t host_down;
static int16_t host_stick_x;
static int16_t host_stick_y;
static int16_t host_cstick_x;
static int16_t host_cstick_y;
static uint64_t host_time_ms;
static bool host_running = true;
static bool host_memory_measured;
static uint32_t host_application_free;
static uint32_t host_linear_free;
static uint32_t host_application_size;
static PBHardwareModel host_hw_model;
#endif

#ifdef __3DS__
static PBHardwareModel g_hw_model = PB_HW_UNKNOWN;
#endif

#ifdef __3DS__
static aptHookCookie g_apt_cookie;
static bool g_apt_hooked;
static PBBootstrap *g_bootstrap;
static PrintConsole g_bottom;
static void apt_hook(APT_HookType hook, void *param) {
    PBBootstrap *bootstrap = (PBBootstrap *)param;

    switch (hook) {
        case APTHOOK_ONSUSPEND:
            pb_bootstrap_apt_event(bootstrap, PB_APT_SUSPEND);
            pb_loop_on_apt(PB_APT_SUSPEND);
            pb_breadcrumb("apt suspend");
            break;
        case APTHOOK_ONSLEEP:
            pb_bootstrap_apt_event(bootstrap, PB_APT_SLEEP);
            pb_loop_on_apt(PB_APT_SLEEP);
            pb_breadcrumb("apt sleep");
            break;
        case APTHOOK_ONRESTORE:
            pb_bootstrap_apt_event(bootstrap, PB_APT_RESTORE);
            pb_loop_on_apt(PB_APT_RESTORE);
            pb_breadcrumb("apt restore");
            break;
        case APTHOOK_ONWAKEUP:
            pb_bootstrap_apt_event(bootstrap, PB_APT_WAKEUP);
            pb_loop_on_apt(PB_APT_WAKEUP);
            pb_breadcrumb("apt wakeup");
            break;
        case APTHOOK_ONEXIT:
            pb_bootstrap_apt_event(bootstrap, PB_APT_EXIT);
            pb_loop_on_apt(PB_APT_EXIT);
            pb_breadcrumb("apt exit");
            break;
        default:
            break;
    }
}
#endif

PBHardwareModel pb_hw_model(void) {
#ifdef __3DS__
    return g_hw_model;
#else
    return host_hw_model;
#endif
}

const char *pb_hw_model_name(PBHardwareModel model) {
    switch (model) {
        case PB_HW_OLD_3DS:
            return "old3ds";
        case PB_HW_NEW_3DS:
            return "new3ds";
        case PB_HW_UNKNOWN:
        default:
            return "unknown";
    }
}

bool pb_hw_new_3ds_features_enabled(void) {
    return false;
}

#ifndef __3DS__
void pb_hw_host_set(PBHardwareModel model) {
    host_hw_model = model;
}

void pb_memory_host_set(uint32_t application_free, uint32_t linear_free,
                        uint32_t application_size, bool measured) {
    host_application_free = application_free;
    host_linear_free = linear_free;
    host_application_size = application_size;
    host_memory_measured = measured;
}
#endif

PBAudioStatus pb_audio_status(void) {
    return PB_AUDIO_DEFERRED_M14;
}

bool pb_thread_extra_workers_allowed(void) {
    return false;
}

#ifdef __3DS__
void pb_input_poll(PBInputSample *sample) {
    circlePosition stick;

    memset(&g_input_last, 0, sizeof(g_input_last));
    hidScanInput();
    g_input_last.held = hidKeysHeld();
    g_input_last.down = hidKeysDown();
    hidCircleRead(&stick);
    g_input_last.stick_x = stick.dx;
    g_input_last.stick_y = stick.dy;
    if (sample != NULL) {
        *sample = g_input_last;
    }
}

void pb_input_last(PBInputSample *sample) {
    if (sample != NULL) {
        *sample = g_input_last;
    }
}

uint64_t pb_time_raw_ms(void) {
    return osGetTime();
}

void pb_time_wait_vblank(void) {
    gspWaitForVBlank();
}

void pb_memory_query(PBMemoryStatus *status) {
    if (status == NULL) {
        return;
    }
    memset(status, 0, sizeof(*status));
    status->application_size = osGetMemRegionSize(MEMREGION_APPLICATION);
    status->application_free = osGetMemRegionFree(MEMREGION_APPLICATION);
    status->linear_free = linearSpaceFree();
    status->measured = true;
    status->pressure = pb_memory_classify(status);
}

bool pb_system_init(PBBootstrap *bootstrap) {
    u16 top_width = 0;
    u16 top_height = 0;

    if (bootstrap == NULL) {
        return false;
    }
    pb_diag_init();
    {
        bool is_new = false;
        if (R_SUCCEEDED(APT_CheckNew3DS(&is_new))) {
            g_hw_model = is_new ? PB_HW_NEW_3DS : PB_HW_OLD_3DS;
        } else {
            g_hw_model = PB_HW_UNKNOWN;
        }
    }
    gfxInitDefault();
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);
    consoleInit(GFX_BOTTOM, &g_bottom);
    g_bootstrap = bootstrap;
    aptHook(&g_apt_cookie, apt_hook, bootstrap);
    g_apt_hooked = true;
    pb_gfx_init();
    bootstrap->gfx_ready = pb_gfx_ready();
    (void)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &top_width, &top_height);
    bootstrap->top_ready = top_width != 0 && top_height != 0;
    bootstrap->bottom_ready = true;
    pb_fs_init();
    pb_loop_init();
    pb_breadcrumb("system init");
    pb_log(PB_LOG_INFO, "hw", pb_hw_model_name(g_hw_model));
    return true;
}

void pb_system_shutdown(PBBootstrap *bootstrap) {
    pb_breadcrumb("system shutdown");
    pb_loop_shutdown();
    pb_fs_shutdown();
    pb_gfx_shutdown();
    if (g_apt_hooked) {
        aptUnhook(&g_apt_cookie);
        g_apt_hooked = false;
    }
    gfxExit();
    if (bootstrap != NULL) {
        bootstrap->gfx_ready = false;
        bootstrap->top_ready = false;
        bootstrap->bottom_ready = false;
    }
    g_bootstrap = NULL;
}

bool pb_system_pump(void) {
    return aptMainLoop();
}

void pb_console_clear(void) {
    consoleSelect(&g_bottom);
    consoleClear();
}

void pb_console_print(const char *text) {
    if (text != NULL) {
        printf("%s", text);
    }
}

#else

void pb_input_poll(PBInputSample *sample) {
    memset(&g_input_last, 0, sizeof(g_input_last));
    g_input_last.held = host_held;
    g_input_last.down = host_down;
    g_input_last.stick_x = host_stick_x;
    g_input_last.stick_y = host_stick_y;
    g_input_last.cstick_x = host_cstick_x;
    g_input_last.cstick_y = host_cstick_y;
    host_down = 0U;
    if (sample != NULL) {
        *sample = g_input_last;
    }
}

void pb_input_last(PBInputSample *sample) {
    if (sample != NULL) {
        *sample = g_input_last;
    }
}

void pb_input_host_set(uint32_t held, uint32_t down) {
    host_held = held;
    host_down = down;
}

void pb_input_host_set_stick(int16_t x, int16_t y) {
    host_stick_x = x;
    host_stick_y = y;
}

void pb_input_host_set_cstick(int16_t x, int16_t y) {
    host_cstick_x = x;
    host_cstick_y = y;
}

uint64_t pb_time_raw_ms(void) {
    return host_time_ms;
}

void pb_time_host_set(uint64_t ms) {
    host_time_ms = ms;
}

void pb_time_host_advance(uint64_t ms) {
    host_time_ms += ms;
}

void pb_time_wait_vblank(void) {
}

void pb_memory_query(PBMemoryStatus *status) {
    if (status == NULL) {
        return;
    }
    memset(status, 0, sizeof(*status));
    status->application_size = host_application_size;
    status->application_free = host_application_free;
    status->linear_free = host_linear_free;
    status->measured = host_memory_measured;
    status->pressure = pb_memory_classify(status);
}

bool pb_system_init(PBBootstrap *bootstrap) {
    host_running = true;
    pb_diag_init();
    pb_gfx_init();
    if (bootstrap != NULL) {
        bootstrap->gfx_ready = pb_gfx_ready();
        bootstrap->top_ready = true;
        bootstrap->bottom_ready = true;
    }
    pb_breadcrumb("system init");
    pb_fs_init();
    pb_loop_init();
    return bootstrap != NULL;
}

void pb_system_shutdown(PBBootstrap *bootstrap) {
    pb_breadcrumb("system shutdown");
    pb_loop_shutdown();
    pb_fs_shutdown();
    pb_gfx_shutdown();
    host_running = false;
    if (bootstrap != NULL) {
        bootstrap->gfx_ready = false;
        bootstrap->top_ready = false;
        bootstrap->bottom_ready = false;
    }
}

bool pb_system_pump(void) {
    return host_running;
}

void pb_console_clear(void) {
}

void pb_console_print(const char *text) {
    (void)text;
}

#endif
