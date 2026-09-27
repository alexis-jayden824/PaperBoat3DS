#include "pb3ds/platform.h"

#include <stdio.h>
#include <string.h>

#ifdef __3DS__
#include <3ds.h>
#endif

#ifndef __3DS__
static uint32_t host_held;
static uint32_t host_down;
static uint64_t host_time_ms;
static bool host_running = true;
static bool host_gfx_ready;
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
static bool g_gfx_ready;

static void apt_hook(APT_HookType hook, void *param) {
    PBBootstrap *bootstrap = (PBBootstrap *)param;

    switch (hook) {
        case APTHOOK_ONSUSPEND:
            pb_bootstrap_apt_event(bootstrap, PB_APT_SUSPEND);
            pb_breadcrumb("apt suspend");
            break;
        case APTHOOK_ONSLEEP:
            pb_bootstrap_apt_event(bootstrap, PB_APT_SLEEP);
            pb_breadcrumb("apt sleep");
            break;
        case APTHOOK_ONRESTORE:
            pb_bootstrap_apt_event(bootstrap, PB_APT_RESTORE);
            pb_breadcrumb("apt restore");
            break;
        case APTHOOK_ONWAKEUP:
            pb_bootstrap_apt_event(bootstrap, PB_APT_WAKEUP);
            pb_breadcrumb("apt wakeup");
            break;
        case APTHOOK_ONEXIT:
            pb_bootstrap_apt_event(bootstrap, PB_APT_EXIT);
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

PBFsStatus pb_fs_status(void) {
    return PB_FS_DEFERRED_M9;
}

const char *pb_fs_sdmc_root(void) {
    return PB_FS_SDMC_ROOT;
}

#ifdef __3DS__
bool pb_gfx_ready(void) {
    return g_gfx_ready;
}

void pb_gfx_clear_top(uint8_t red, uint8_t green, uint8_t blue) {
    u16 width = 0;
    u16 height = 0;
    u8 *framebuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height);
    size_t pixels;
    size_t index;

    if (framebuffer == NULL || width == 0 || height == 0) {
        return;
    }
    pixels = (size_t)width * (size_t)height;
    for (index = 0; index < pixels; index++) {
        framebuffer[index * 3U + 0U] = blue;
        framebuffer[index * 3U + 1U] = green;
        framebuffer[index * 3U + 2U] = red;
    }
}

void pb_gfx_present(void) {
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

void pb_input_poll(PBInputSample *sample) {
    circlePosition stick;

    if (sample == NULL) {
        return;
    }
    memset(sample, 0, sizeof(*sample));
    hidScanInput();
    sample->held = hidKeysHeld();
    sample->down = hidKeysDown();
    hidCircleRead(&stick);
    sample->stick_x = stick.dx;
    sample->stick_y = stick.dy;
}

uint64_t pb_time_ms(void) {
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
    g_gfx_ready = true;
    bootstrap->gfx_ready = true;
    (void)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &top_width, &top_height);
    bootstrap->top_ready = top_width != 0 && top_height != 0;
    bootstrap->bottom_ready = true;
    pb_breadcrumb("system init");
    pb_log(PB_LOG_INFO, "hw", pb_hw_model_name(g_hw_model));
    return true;
}

void pb_system_shutdown(PBBootstrap *bootstrap) {
    pb_breadcrumb("system shutdown");
    if (g_apt_hooked) {
        aptUnhook(&g_apt_cookie);
        g_apt_hooked = false;
    }
    gfxExit();
    g_gfx_ready = false;
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

bool pb_gfx_ready(void) {
    return host_gfx_ready;
}

void pb_gfx_clear_top(uint8_t red, uint8_t green, uint8_t blue) {
    (void)red;
    (void)green;
    (void)blue;
}

void pb_gfx_present(void) {
}

void pb_input_poll(PBInputSample *sample) {
    if (sample == NULL) {
        return;
    }
    memset(sample, 0, sizeof(*sample));
    sample->held = host_held;
    sample->down = host_down;
    host_down = 0U;
}

void pb_input_host_set(uint32_t held, uint32_t down) {
    host_held = held;
    host_down = down;
}

uint64_t pb_time_ms(void) {
    return host_time_ms++;
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
    host_gfx_ready = true;
    pb_diag_init();
    if (bootstrap != NULL) {
        bootstrap->gfx_ready = true;
        bootstrap->top_ready = true;
        bootstrap->bottom_ready = true;
    }
    pb_breadcrumb("system init");
    return bootstrap != NULL;
}

void pb_system_shutdown(PBBootstrap *bootstrap) {
    pb_breadcrumb("system shutdown");
    host_gfx_ready = false;
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
