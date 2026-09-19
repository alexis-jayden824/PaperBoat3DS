#include <3ds.h>
#include <stdio.h>

#include "pb3ds/version.h"

typedef enum {
    LIFECYCLE_ACTIVE,
    LIFECYCLE_SUSPENDED,
    LIFECYCLE_SLEEPING,
    LIFECYCLE_EXITING,
} LifecycleState;

typedef struct {
    bool model_query_ok;
    bool is_new_3ds;
    u32 kernel_version;
    volatile LifecycleState lifecycle;
    volatile bool redraw_bottom;
} BootstrapState;

static const char *lifecycle_name(LifecycleState state) {
    switch (state) {
        case LIFECYCLE_SUSPENDED:
            return "suspended";
        case LIFECYCLE_SLEEPING:
            return "sleeping";
        case LIFECYCLE_EXITING:
            return "exiting";
        case LIFECYCLE_ACTIVE:
        default:
            return "active";
    }
}

static void apt_hook(APT_HookType hook, void *param) {
    BootstrapState *state = (BootstrapState *)param;

    switch (hook) {
        case APTHOOK_ONSUSPEND:
            state->lifecycle = LIFECYCLE_SUSPENDED;
            break;
        case APTHOOK_ONSLEEP:
            state->lifecycle = LIFECYCLE_SLEEPING;
            break;
        case APTHOOK_ONRESTORE:
        case APTHOOK_ONWAKEUP:
            state->lifecycle = LIFECYCLE_ACTIVE;
            break;
        case APTHOOK_ONEXIT:
            state->lifecycle = LIFECYCLE_EXITING;
            break;
        default:
            return;
    }

    state->redraw_bottom = true;
}

static void print_top_screen(PrintConsole *console) {
    consoleSelect(console);
    printf("\x1b[2J");
    printf("\x1b[2;2H%s\n", PB3DS_PROJECT_NAME);
    printf("\x1b[4;2HVersion: %s\n", PB3DS_VERSION);
    printf("\x1b[5;2HStage:   %s\n", PB3DS_ROADMAP_STAGE);
    printf("\x1b[8;2HNative ARM11 application shell\n");
    printf("\x1b[10;2HNo game assets are bundled.\n");
    printf("\x1b[13;2HPress START to exit.\n");
}

static void print_bottom_screen(PrintConsole *console, const BootstrapState *state) {
    consoleSelect(console);
    printf("\x1b[2J");
    printf("\x1b[2;2HM1 Service Diagnostics\n");
    printf("\x1b[4;2HGFX displays      OK\n");
    printf("\x1b[5;2HAPT lifecycle    OK\n");
    printf("\x1b[6;2HHID input        OK\n");
    printf("\x1b[8;2HSystem: %s\n",
           state->model_query_ok ? (state->is_new_3ds ? "New 3DS" : "Old 3DS") : "unknown");
    printf("\x1b[9;2HKernel: %08lX\n", (unsigned long)state->kernel_version);
    printf("\x1b[11;2HLifecycle: %-10s\n", lifecycle_name(state->lifecycle));
    printf("\x1b[14;2HConfiguration UI remains M16.\n");
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    BootstrapState state = {
        .model_query_ok = false,
        .is_new_3ds = false,
        .kernel_version = osGetKernelVersion(),
        .lifecycle = LIFECYCLE_ACTIVE,
        .redraw_bottom = false,
    };
    aptHookCookie apt_cookie;
    PrintConsole top_console;
    PrintConsole bottom_console;

    state.model_query_ok = R_SUCCEEDED(APT_CheckNew3DS(&state.is_new_3ds));

    gfxInitDefault();
    consoleInit(GFX_TOP, &top_console);
    consoleInit(GFX_BOTTOM, &bottom_console);
    aptHook(&apt_cookie, apt_hook, &state);

    print_top_screen(&top_console);
    print_bottom_screen(&bottom_console, &state);

    while (aptMainLoop()) {
        hidScanInput();
        if ((hidKeysDown() & KEY_START) != 0) {
            break;
        }
        if (state.redraw_bottom) {
            state.redraw_bottom = false;
            print_bottom_screen(&bottom_console, &state);
        }
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    aptUnhook(&apt_cookie);
    gfxExit();
    return 0;
}
