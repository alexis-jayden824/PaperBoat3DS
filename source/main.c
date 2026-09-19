#include <3ds.h>
#include <stdio.h>

#include "pb3ds/compat.h"
#include "pb3ds/diagnostics.h"
#include "pb3ds/log.h"
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
    printf("\x1b[7;2HBuild:   %.12s\n", PB3DS_BUILD_SHA);
    printf("\x1b[9;2HNative ARM11 application shell\n");
    printf("\x1b[11;2HNo game assets are bundled.\n");
    printf("\x1b[14;2HPress START to exit.\n");
}

static void print_bottom_screen(PrintConsole *console, const BootstrapState *state,
                                const PBLog *log, const PBConfig *config,
                                bool archive_available, PBMemorySnapshot memory) {
    consoleSelect(console);
    printf("\x1b[2J");
    printf("\x1b[2;2HM4 Compatibility Core\n");
    printf("\x1b[4;2HGFX displays      OK\n");
    printf("\x1b[5;2HAPT lifecycle    OK\n");
    printf("\x1b[6;2HHID input        OK\n");
    printf("\x1b[8;2HSystem: %s\n",
           state->model_query_ok ? (state->is_new_3ds ? "New 3DS" : "Old 3DS") : "unknown");
    printf("\x1b[9;2HKernel: %08lX\n", (unsigned long)state->kernel_version);
    printf("\x1b[11;2HLifecycle: %-10s\n", lifecycle_name(state->lifecycle));
    printf("\x1b[13;2HApp free:    %6lu KiB\n",
           (unsigned long)(memory.application_free / 1024));
    printf("\x1b[14;2HLinear free: %6lu KiB\n",
           (unsigned long)(memory.linear_free / 1024));
    printf("\x1b[16;2HSD log: %s\n",
           pb_log_is_persistent(log) ? "ACTIVE" : "unavailable (continuing)");
    printf("\x1b[18;2HConfig: %lu entries\n", (unsigned long)config->count);
    printf("\x1b[19;2HArchive: %s\n", archive_available ? "FOUND" : "not installed");
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
    PBLog log;
    PBConfig config;
    PBArchive archive;

    state.model_query_ok = R_SUCCEEDED(APT_CheckNew3DS(&state.is_new_3ds));

    gfxInitDefault();
    consoleInit(GFX_TOP, &top_console);
    consoleInit(GFX_BOTTOM, &bottom_console);
    aptHook(&apt_cookie, apt_hook, &state);

    (void)pb_log_init(&log);
    pb_config_init(&config);
    const bool config_loaded =
        pb_config_load(&config, "sdmc:/3ds/PaperBoat3DS/config.ini");
    const bool archive_available =
        pb_archive_open(&archive, "sdmc:/3ds/PaperBoat3DS/paperboat.o2r");
    PBMemorySnapshot memory = pb_memory_snapshot();
    pb_log_write(&log, PB_LOG_INFO, "bootstrap",
                 "version=%s stage=\"%s\" build_sha=%s build_utc=%s",
                 PB3DS_VERSION, PB3DS_ROADMAP_STAGE, PB3DS_BUILD_SHA,
                 PB3DS_BUILD_UTC);
    pb_log_write(&log, PB_LOG_INFO, "platform",
                 "model=%s kernel=%08lX",
                 state.model_query_ok ? (state.is_new_3ds ? "new3ds" : "old3ds") : "unknown",
                 (unsigned long)state.kernel_version);
    pb_log_write(&log, PB_LOG_INFO, "memory",
                 "application_free=%lu linear_free=%lu",
                 (unsigned long)memory.application_free,
                 (unsigned long)memory.linear_free);
    pb_log_write(&log, PB_LOG_INFO, "compat",
                 "config=%s entries=%lu archive=%s archive_size=%lu time_ms=%llu",
                 config_loaded ? "loaded" : "defaults",
                 (unsigned long)config.count,
                 archive_available ? "found" : "missing",
                 (unsigned long)archive.size,
                 (unsigned long long)pb_platform_time_ms());
    print_top_screen(&top_console);
    print_bottom_screen(&bottom_console, &state, &log, &config,
                        archive_available, memory);

    while (aptMainLoop()) {
        hidScanInput();
        if ((hidKeysDown() & KEY_START) != 0) {
            break;
        }
        if (state.redraw_bottom) {
            state.redraw_bottom = false;
            pb_log_write(&log, PB_LOG_INFO, "lifecycle", "state=%s",
                         lifecycle_name(state.lifecycle));
            memory = pb_memory_snapshot();
            print_bottom_screen(&bottom_console, &state, &log, &config,
                                archive_available, memory);
        }
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    memory = pb_memory_snapshot();
    pb_log_write(&log, PB_LOG_INFO, "shutdown",
                 "application_free=%lu linear_free=%lu",
                 (unsigned long)memory.application_free,
                 (unsigned long)memory.linear_free);
    pb_archive_close(&archive);
    pb_log_close(&log);
    aptUnhook(&apt_cookie);
    gfxExit();
    return 0;
}
