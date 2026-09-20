#include <3ds.h>
#include <stdio.h>

#include "pb3ds/compat.h"
#include "pb3ds/diagnostics.h"
#include "pb3ds/input.h"
#include "pb3ds/log.h"
#include "pb3ds/memory.h"
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
    PBInputState *input;
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
            pb_input_suspend(state->input);
            break;
        case APTHOOK_ONSLEEP:
            state->lifecycle = LIFECYCLE_SLEEPING;
            pb_input_suspend(state->input);
            break;
        case APTHOOK_ONRESTORE:
        case APTHOOK_ONWAKEUP:
            state->lifecycle = LIFECYCLE_ACTIVE;
            pb_input_resume(state->input);
            break;
        case APTHOOK_ONEXIT:
            state->lifecycle = LIFECYCLE_EXITING;
            pb_input_suspend(state->input);
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
                                bool engine_archive_available,
                                bool game_archive_available,
                                const PBMemorySnapshot *memory,
                                const PBInputState *input,
                                bool menu_request_seen) {
    consoleSelect(console);
    printf("\x1b[2J");
    printf("\x1b[2;2HM8 Input Backend\n");
    printf("\x1b[4;2HGFX displays      OK\n");
    printf("\x1b[5;2HAPT lifecycle    OK\n");
    printf("\x1b[6;2HHID input        OK\n");
    printf("\x1b[8;2HSystem: %s\n",
           state->model_query_ok ? (state->is_new_3ds ? "New 3DS" : "Old 3DS") : "unknown");
    printf("\x1b[9;2HKernel: %08lX\n", (unsigned long)state->kernel_version);
    printf("\x1b[10;2HLifecycle: %-10s\n", lifecycle_name(state->lifecycle));
    printf("\x1b[11;2HInput gate: %s\n",
           input->waiting_for_neutral ? "WAIT NEUTRAL" : "active");
    printf("\x1b[13;2HN64 held:%04X press:%04X\n",
           (unsigned int)input->n64_held,
           (unsigned int)input->n64_pressed);
    printf("\x1b[14;2HN64 release: %04X\n",
           (unsigned int)input->n64_released);
    printf("\x1b[15;2HStick: %4d,%4d\n", input->stick_x, input->stick_y);
    if (input->touch_held) {
        printf("\x1b[16;2HTouch: %3u,%3u DOWN\n",
               (unsigned int)input->touch_x,
               (unsigned int)input->touch_y);
    } else {
        printf("\x1b[16;2HTouch: up\n");
    }
    printf("\x1b[17;2HMenu SELECT: %s\n",
           menu_request_seen ? "REQUESTED" : "ready");
    printf("\x1b[19;2HBudgets: %s fail:%lu\n",
           memory->pressure ? "PRESSURE" : "OK",
           (unsigned long)memory->allocation_failures);
    if (!memory->application_measurement_available) {
        printf("\x1b[20;2HApp free: unavailable\n");
    } else {
        printf("\x1b[20;2HApp free: %6lu KiB\n",
               (unsigned long)(memory->application_free / 1024));
    }
    printf("\x1b[21;2HLinear free: %6lu KiB\n",
           (unsigned long)(memory->linear_free / 1024));
    printf("\x1b[22;2HSD log: %s\n",
           pb_log_is_persistent(log) ? "ACTIVE" : "unavailable (continuing)");
    printf("\x1b[24;2HConfig: %lu\n", (unsigned long)config->count);
    printf("\x1b[25;2HEngine:%s Game:%s\n",
           engine_archive_available ? "FOUND" : "missing",
           game_archive_available ? "FOUND" : "missing");
    printf("\x1b[26;2HStream chunk: %lu KiB\n",
           (unsigned long)(PB_ARCHIVE_STREAM_CHUNK / 1024));
}

static void sample_memory(PBMemoryMonitor *monitor) {
    const uintptr_t stack_marker = (uintptr_t)&monitor;
    pb_memory_monitor_sample(monitor, stack_marker);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    PBInputState input;
    pb_input_init(&input);
    BootstrapState state = {
        .model_query_ok = false,
        .is_new_3ds = false,
        .kernel_version = osGetKernelVersion(),
        .lifecycle = LIFECYCLE_ACTIVE,
        .redraw_bottom = false,
        .input = &input,
    };
    aptHookCookie apt_cookie;
    PrintConsole top_console;
    PrintConsole bottom_console;
    PBLog log;
    PBConfig config;
    PBArchive engine_archive;
    PBArchive game_archive;
    PBMemoryMonitor memory_monitor;
    const uintptr_t stack_anchor = (uintptr_t)&memory_monitor;

    state.model_query_ok = R_SUCCEEDED(APT_CheckNew3DS(&state.is_new_3ds));

    gfxInitDefault();
    pb_memory_monitor_init(&memory_monitor, stack_anchor);
    consoleInit(GFX_TOP, &top_console);
    consoleInit(GFX_BOTTOM, &bottom_console);
    aptHook(&apt_cookie, apt_hook, &state);

    (void)pb_log_init(&log);
    pb_config_init(&config);
    const bool config_loaded =
        pb_config_load(&config, "sdmc:/3ds/PaperBoat3DS/config.ini");
    const bool engine_archive_available =
        pb_archive_open(&engine_archive, "sdmc:/3ds/PaperBoat3DS/paperboat.o2r");
    const bool game_archive_available =
        pb_archive_open(&game_archive, "sdmc:/3ds/PaperBoat3DS/pm64.o2r");
    sample_memory(&memory_monitor);
    const PBMemorySnapshot *memory =
        pb_memory_monitor_snapshot(&memory_monitor);
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
                 (unsigned long)memory->application_free,
                 (unsigned long)memory->linear_free);
    pb_log_write(&log, PB_LOG_INFO, "memory-budget",
                 "app_reserve=%lu linear_reserve=%lu archive=%lu scene=%lu "
                 "transient=%lu linear=%lu stack=%lu stream_chunk=%lu",
                 (unsigned long)PB_MEMORY_APPLICATION_RESERVE,
                 (unsigned long)PB_MEMORY_LINEAR_RESERVE,
                 (unsigned long)PB_MEMORY_ARCHIVE_LIMIT,
                 (unsigned long)PB_MEMORY_SCENE_LIMIT,
                 (unsigned long)PB_MEMORY_TRANSIENT_LIMIT,
                 (unsigned long)PB_MEMORY_LINEAR_LIMIT,
                 (unsigned long)PB_MEMORY_STACK_LIMIT,
                 (unsigned long)PB_ARCHIVE_STREAM_CHUNK);
    pb_log_write(&log, PB_LOG_INFO, "compat",
                 "config=%s entries=%lu engine_archive=%s engine_size=%lu "
                 "game_archive=%s game_size=%lu time_ms=%llu",
                 config_loaded ? "loaded" : "defaults",
                 (unsigned long)config.count,
                 engine_archive_available ? "found" : "missing",
                 (unsigned long)engine_archive.size,
                 game_archive_available ? "found" : "missing",
                 (unsigned long)game_archive.size,
                 (unsigned long long)pb_platform_time_ms());
    print_top_screen(&top_console);
    print_bottom_screen(&bottom_console, &state, &log, &config,
                        engine_archive_available, game_archive_available,
                        memory, &input, false);

    u32 memory_sample_frames = 0;
    u32 active_input_refresh_frames = 0;
    bool menu_request_seen = false;
    while (aptMainLoop()) {
        pb_input_poll(&input);
        if ((input.n64_pressed & PB_N64_START) != 0) {
            break;
        }
        if (input.menu_requested) {
            menu_request_seen = true;
            state.redraw_bottom = true;
            pb_log_write(&log, PB_LOG_INFO, "input",
                         "PaperBoat menu requested through SELECT");
        }
        if (input.native_pressed != 0 || input.native_released != 0 ||
            input.touch_pressed || input.touch_released) {
            state.redraw_bottom = true;
        }
        if (input.native_held != 0 || input.touch_held || input.stick_x != 0 ||
            input.stick_y != 0) {
            active_input_refresh_frames++;
            if (active_input_refresh_frames >= 4) {
                active_input_refresh_frames = 0;
                state.redraw_bottom = true;
            }
        } else {
            active_input_refresh_frames = 0;
        }
        if (state.redraw_bottom) {
            state.redraw_bottom = false;
            pb_log_write(&log, PB_LOG_INFO, "lifecycle", "state=%s",
                         lifecycle_name(state.lifecycle));
            sample_memory(&memory_monitor);
            memory = pb_memory_monitor_snapshot(&memory_monitor);
            print_bottom_screen(&bottom_console, &state, &log, &config,
                                engine_archive_available,
                                game_archive_available, memory, &input,
                                menu_request_seen);
        }
        memory_sample_frames++;
        if (memory_sample_frames >= 60) {
            memory_sample_frames = 0;
            const bool was_under_pressure = memory->pressure;
            sample_memory(&memory_monitor);
            memory = pb_memory_monitor_snapshot(&memory_monitor);
            if (memory->pressure != was_under_pressure) {
                print_bottom_screen(&bottom_console, &state, &log, &config,
                                    engine_archive_available,
                                    game_archive_available, memory, &input,
                                    menu_request_seen);
            }
        }
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    sample_memory(&memory_monitor);
    memory = pb_memory_monitor_snapshot(&memory_monitor);
    pb_log_write(&log, PB_LOG_INFO, "shutdown",
                 "application_free=%lu linear_free=%lu peak_application=%lu "
                 "peak_linear=%lu peak_stack=%lu allocation_failures=%lu pressure=%s",
                 (unsigned long)memory->application_free,
                 (unsigned long)memory->linear_free,
                 (unsigned long)memory->peak_application_used,
                 (unsigned long)memory->peak_linear_used,
                 (unsigned long)memory->peak_stack_used,
                 (unsigned long)memory->allocation_failures,
                 memory->pressure ? "yes" : "no");
    pb_log_write(&log, PB_LOG_INFO, "input",
                 "frames=%llu menu_request_seen=%s",
                 (unsigned long long)input.frame_index,
                 menu_request_seen ? "yes" : "no");
    pb_archive_close(&game_archive);
    pb_archive_close(&engine_archive);
    pb_log_close(&log);
    aptUnhook(&apt_cookie);
    gfxExit();
    return 0;
}
