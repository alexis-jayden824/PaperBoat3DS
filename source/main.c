#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "pb3ds/bootstrap.h"
#include "pb3ds/version.h"

#define TOP_FILL_R 26U
#define TOP_FILL_G 51U
#define TOP_FILL_B 68U

typedef struct {
    PBBootstrap bootstrap;
    aptHookCookie apt_cookie;
    PrintConsole bottom;
    bool apt_hooked;
} AppState;

static AppState *g_app;

static void apt_hook(APT_HookType hook, void *param) {
    PBBootstrap *bootstrap = (PBBootstrap *)param;

    switch (hook) {
        case APTHOOK_ONSUSPEND:
            pb_bootstrap_apt_event(bootstrap, PB_APT_SUSPEND);
            break;
        case APTHOOK_ONSLEEP:
            pb_bootstrap_apt_event(bootstrap, PB_APT_SLEEP);
            break;
        case APTHOOK_ONRESTORE:
            pb_bootstrap_apt_event(bootstrap, PB_APT_RESTORE);
            break;
        case APTHOOK_ONWAKEUP:
            pb_bootstrap_apt_event(bootstrap, PB_APT_WAKEUP);
            break;
        case APTHOOK_ONEXIT:
            pb_bootstrap_apt_event(bootstrap, PB_APT_EXIT);
            break;
        default:
            break;
    }
}

static void fill_top_screen(u8 red, u8 green, u8 blue) {
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

static void draw_bottom_status(AppState *app) {
    size_t index;
    size_t start;

    consoleSelect(&app->bottom);
    consoleClear();
    printf("%s\n", PB3DS_PROJECT_NAME);
    printf("%s  %s\n", PB3DS_VERSION, PB3DS_ROADMAP_STAGE);
    printf("sha %s\n", PB3DS_BUILD_SHA);
    printf("%s\n\n", pb_bootstrap_status_line(&app->bootstrap));
    printf("Top %ux%u  Bottom %ux%u\n", PB_BOOTSTRAP_TOP_WIDTH,
           PB_BOOTSTRAP_TOP_HEIGHT, PB_BOOTSTRAP_BOTTOM_WIDTH,
           PB_BOOTSTRAP_BOTTOM_HEIGHT);
    printf("frames %lu\n", (unsigned long)app->bootstrap.frames);
    printf("START exits. This is not PaperBoat.\n\n");
    printf("log:\n");
    start = app->bootstrap.log.count < PB_BOOTSTRAP_LOG_CAPACITY
                ? 0U
                : app->bootstrap.log.next;
    for (index = 0; index < app->bootstrap.log.count; index++) {
        const size_t line =
            (start + index) % PB_BOOTSTRAP_LOG_CAPACITY;
        printf("  %s\n", app->bootstrap.log.lines[line]);
    }
}

static void app_shutdown(AppState *app) {
    pb_bootstrap_log(&app->bootstrap, "shutdown");
    if (app->apt_hooked) {
        aptUnhook(&app->apt_cookie);
        app->apt_hooked = false;
    }
    gfxExit();
    app->bootstrap.gfx_ready = false;
    app->bootstrap.top_ready = false;
    app->bootstrap.bottom_ready = false;
}

int main(int argc, char **argv) {
    AppState app = {0};
    u16 top_width = 0;
    u16 top_height = 0;

    (void)argc;
    (void)argv;
    g_app = &app;
    pb_bootstrap_init(&app.bootstrap);

    gfxInitDefault();
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);
    consoleInit(GFX_BOTTOM, &app.bottom);
    aptHook(&app.apt_cookie, apt_hook, &app.bootstrap);
    app.apt_hooked = true;
    app.bootstrap.gfx_ready = true;

    (void)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &top_width, &top_height);
    app.bootstrap.top_ready = top_width != 0 && top_height != 0;
    app.bootstrap.bottom_ready = true;
    pb_bootstrap_log(&app.bootstrap, "gfx + consoles ready");
    pb_bootstrap_log(&app.bootstrap, "press START to exit");

    while (aptMainLoop() && pb_bootstrap_is_running(&app.bootstrap)) {
        hidScanInput();
        if ((hidKeysDown() & KEY_START) != 0U) {
            pb_bootstrap_on_start(&app.bootstrap);
            break;
        }

        pb_bootstrap_tick(&app.bootstrap);
        fill_top_screen(TOP_FILL_R, TOP_FILL_G, TOP_FILL_B);
        draw_bottom_status(&app);
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    app_shutdown(&app);
    g_app = NULL;
    return 0;
}
