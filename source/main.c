#include <3ds.h>
#include <stdio.h>

#include "pb3ds/compat.h"
#include "pb3ds/diagnostics.h"
#include "pb3ds/gfx_rendering_api_3ds.h"
#include "pb3ds/input.h"
#include "pb3ds/log.h"
#include "pb3ds/memory.h"
#include "pb3ds/renderer.h"
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
    PBGfxApi3DS *graphics;
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

    if (state->graphics != NULL) {
        pb_gfx_api_3ds_set_active(state->graphics,
                                  state->lifecycle == LIFECYCLE_ACTIVE);
    }

    state->redraw_bottom = true;
}

static void print_bottom_screen(PrintConsole *console,
                                const BootstrapState *state,
                                const PBLog *log,
                                const PBConfig *config,
                                bool engine_archive_available,
                                bool game_archive_available,
                                const PBMemorySnapshot *memory,
                                const PBInputState *input,
                                bool menu_request_seen,
                                PBRendererInitResult renderer_result,
                                const PBRendererStats *renderer_stats,
                                PBGfxApiInitResult graphics_result,
                                const PBGfxBridgeStats *graphics_stats) {
    const bool renderer_ready =
        renderer_result == PB_RENDERER_INIT_OK && renderer_stats != NULL;
    const bool graphics_ready =
        graphics_result == PB_GFX_API_INIT_OK && graphics_stats != NULL;
    const unsigned int command_permille =
        renderer_ready
            ? (unsigned int)(renderer_stats->command_buffer_peak * 1000.0f)
            : 0;

    consoleSelect(console);
    printf("\x1b[2J");
    printf("\x1b[1;2HM10 libultraship Graphics\n");
    printf("\x1b[3;2HVersion: %s\n", PB3DS_VERSION);
    printf("\x1b[4;2HBuild: %.12s\n", PB3DS_BUILD_SHA);
    printf("\x1b[6;2HAPI: %-20s\n",
           pb_gfx_api_init_result_name(graphics_result));
    printf("\x1b[7;2HContract: Fast::GfxRenderingAPI\n");
    printf("\x1b[8;2HPICA: %s  Z:0..1  Tex:1024\n",
           renderer_ready ? "ready" : "unavailable");
    if (graphics_ready) {
        printf("\x1b[9;2HTEV shaders:%lu unsupported:%lu\n",
               (unsigned long)graphics_stats->shaders_live,
               (unsigned long)graphics_stats->unsupported_shaders);
        printf("\x1b[10;2HTextures:%lu %lu B  Stream:%lu B\n",
               (unsigned long)graphics_stats->textures_live,
               (unsigned long)graphics_stats->texture_bytes,
               (unsigned long)graphics_stats->stream_peak_bytes);
        printf("\x1b[11;2HFrames:%llu Draws:%llu Tris:%llu\n",
               (unsigned long long)graphics_stats->frames_presented,
               (unsigned long long)graphics_stats->draw_calls,
               (unsigned long long)graphics_stats->triangles);
        printf("\x1b[12;2HVertices:%llu Reject:%lu fail:%lu\n",
               (unsigned long long)graphics_stats->vertices,
               (unsigned long)graphics_stats->rejected_commands,
               (unsigned long)graphics_stats->frame_failures);
    }
    if (renderer_ready) {
        printf("\x1b[13;2HCmd peak:%u.%u%%  VBO:%lu KiB\n",
               command_permille / 10U, command_permille % 10U,
               (unsigned long)(renderer_stats->vertex_buffer_bytes / 1024U));
    }

    printf("\x1b[15;2HSystem: %s\n",
           state->model_query_ok
               ? (state->is_new_3ds ? "New 3DS" : "Old 3DS")
               : "unknown");
    printf("\x1b[16;2HKernel: %08lX  %s\n",
           (unsigned long)state->kernel_version,
           lifecycle_name(state->lifecycle));
    printf("\x1b[17;2HInput gate: %s\n",
           input->waiting_for_neutral ? "WAIT NEUTRAL" : "active");
    printf("\x1b[18;2HN64:%04X  Stick:%4d,%4d\n",
           (unsigned int)input->n64_held, input->stick_x, input->stick_y);
    printf("\x1b[19;2HMenu SELECT: %s\n",
           menu_request_seen ? "REQUESTED" : "ready");

    printf("\x1b[21;2HBudgets: %s fail:%lu\n",
           memory->pressure ? "PRESSURE" : "OK",
           (unsigned long)memory->allocation_failures);
    if (!memory->application_measurement_available) {
        printf("\x1b[22;2HApp free: unavailable\n");
    } else {
        printf("\x1b[22;2HApp free: %6lu KiB\n",
               (unsigned long)(memory->application_free / 1024));
    }
    printf("\x1b[23;2HLinear free: %6lu KiB\n",
           (unsigned long)(memory->linear_free / 1024));
    printf("\x1b[24;2HSD log: %s\n",
           pb_log_is_persistent(log) ? "ACTIVE" : "unavailable");
    printf("\x1b[26;2HConfig:%lu Engine:%s Game:%s\n",
           (unsigned long)config->count,
           engine_archive_available ? "OK" : "--",
           game_archive_available ? "OK" : "--");
    printf("\x1b[28;2HSTART exits diagnostic shell\n");
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
        .graphics = NULL,
    };
    aptHookCookie apt_cookie;
    PrintConsole bottom_console;
    PBLog log;
    PBConfig config;
    PBArchive engine_archive;
    PBArchive game_archive;
    PBMemoryMonitor memory_monitor;
    PBRenderer3DS *renderer = NULL;
    PBGfxApi3DS *graphics = NULL;
    const uintptr_t stack_anchor = (uintptr_t)&memory_monitor;

    state.model_query_ok = R_SUCCEEDED(APT_CheckNew3DS(&state.is_new_3ds));

    gfxInitDefault();
    pb_memory_monitor_init(&memory_monitor, stack_anchor);
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
    const PBRendererInitResult renderer_result =
        pb_renderer_3ds_create(&renderer);
    PBGfxApiInitResult graphics_result = PB_GFX_API_INIT_INVALID_ARGUMENT;
    if (renderer_result == PB_RENDERER_INIT_OK) {
        graphics_result = pb_gfx_api_3ds_create(&graphics, renderer);
        if (graphics_result == PB_GFX_API_INIT_OK &&
            !pb_gfx_api_3ds_prepare_diagnostic(graphics)) {
            graphics_result = PB_GFX_API_INIT_DIAGNOSTIC;
            pb_gfx_api_3ds_destroy(graphics);
            graphics = NULL;
        }
    }
    state.graphics = graphics;

    sample_memory(&memory_monitor);
    const PBMemorySnapshot *memory =
        pb_memory_monitor_snapshot(&memory_monitor);
    pb_log_write(&log, PB_LOG_INFO, "bootstrap",
                 "version=%s stage=\"%s\" build_sha=%s build_utc=%s",
                 PB3DS_VERSION, PB3DS_ROADMAP_STAGE, PB3DS_BUILD_SHA,
                 PB3DS_BUILD_UTC);
    pb_log_write(&log, PB_LOG_INFO, "platform",
                 "model=%s kernel=%08lX",
                 state.model_query_ok
                     ? (state.is_new_3ds ? "new3ds" : "old3ds")
                     : "unknown",
                 (unsigned long)state.kernel_version);
    pb_log_write(&log, PB_LOG_INFO, "renderer",
                 "status=\"%s\" logical=400x240 target=240x400",
                 pb_renderer_init_result_name(renderer_result));
    pb_log_write(&log, PB_LOG_INFO, "graphics-api",
                 "status=\"%s\" contract=Fast::GfxRenderingAPI "
                 "backend=\"PICA200 (citro3d)\" max_texture=1024",
                 pb_gfx_api_init_result_name(graphics_result));
    if (renderer != NULL) {
        const PBRendererStats *stats = pb_renderer_3ds_stats(renderer);
        pb_log_write(&log, PB_LOG_INFO, "renderer-resources",
                     "shader=renderer.shbin texture=RGBA8 texture_bytes=%lu "
                     "vertex_bytes=%lu",
                     (unsigned long)stats->texture_bytes,
                     (unsigned long)stats->vertex_buffer_bytes);
    }
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

    print_bottom_screen(&bottom_console, &state, &log, &config,
                        engine_archive_available, game_archive_available,
                        memory, &input, false, renderer_result,
                        pb_renderer_3ds_stats(renderer), graphics_result,
                        pb_gfx_api_3ds_stats(graphics));

    u32 memory_sample_frames = 0;
    u32 diagnostics_refresh_frames = 0;
    u32 active_input_refresh_frames = 0;
    bool menu_request_seen = false;
    bool renderer_frame_failed = false;
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

        memory_sample_frames++;
        if (memory_sample_frames >= 60) {
            memory_sample_frames = 0;
            const bool was_under_pressure = memory->pressure;
            sample_memory(&memory_monitor);
            memory = pb_memory_monitor_snapshot(&memory_monitor);
            if (memory->pressure != was_under_pressure) {
                state.redraw_bottom = true;
            }
        }
        diagnostics_refresh_frames++;
        if (diagnostics_refresh_frames >= 30) {
            diagnostics_refresh_frames = 0;
            state.redraw_bottom = true;
        }

        if (state.redraw_bottom) {
            state.redraw_bottom = false;
            print_bottom_screen(&bottom_console, &state, &log, &config,
                                engine_archive_available,
                                game_archive_available, memory, &input,
                                menu_request_seen, renderer_result,
                                pb_renderer_3ds_stats(renderer),
                                graphics_result,
                                pb_gfx_api_3ds_stats(graphics));
        }

        if (graphics != NULL && state.lifecycle == LIFECYCLE_ACTIVE) {
            if (!pb_gfx_api_3ds_render_diagnostic(graphics) &&
                !renderer_frame_failed) {
                renderer_frame_failed = true;
                state.redraw_bottom = true;
                pb_log_write(&log, PB_LOG_ERROR, "graphics-api",
                             "adapter frame submission failed");
            }
        } else if (graphics == NULL) {
            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank();
        } else {
            gspWaitForVBlank();
        }
    }

    sample_memory(&memory_monitor);
    memory = pb_memory_monitor_snapshot(&memory_monitor);
    const PBRendererStats *renderer_stats = pb_renderer_3ds_stats(renderer);
    const PBGfxBridgeStats *graphics_stats =
        pb_gfx_api_3ds_stats(graphics);
    if (graphics_stats != NULL) {
        pb_log_write(&log, PB_LOG_INFO, "graphics-api-shutdown",
                     "frames=%llu draws=%llu triangles=%llu vertices=%llu "
                     "stream_bytes=%llu stream_peak=%lu textures=%lu "
                     "shaders=%lu unsupported=%lu rejected=%lu failures=%lu",
                     (unsigned long long)graphics_stats->frames_presented,
                     (unsigned long long)graphics_stats->draw_calls,
                     (unsigned long long)graphics_stats->triangles,
                     (unsigned long long)graphics_stats->vertices,
                     (unsigned long long)graphics_stats->streamed_bytes,
                     (unsigned long)graphics_stats->stream_peak_bytes,
                     (unsigned long)graphics_stats->textures_live,
                     (unsigned long)graphics_stats->shaders_live,
                     (unsigned long)graphics_stats->unsupported_shaders,
                     (unsigned long)graphics_stats->rejected_commands,
                     (unsigned long)graphics_stats->frame_failures);
    }
    if (renderer_stats != NULL) {
        pb_log_write(&log, PB_LOG_INFO, "renderer-shutdown",
                     "frames=%llu draws=%llu vertices=%llu frame_failures=%lu "
                     "command_peak_permille=%lu state_changes=%lu cached=%lu "
                     "rejected=%lu",
                     (unsigned long long)renderer_stats->frames,
                     (unsigned long long)renderer_stats->draw_calls,
                     (unsigned long long)renderer_stats->vertices,
                     (unsigned long)renderer_stats->frame_failures,
                     (unsigned long)(renderer_stats->command_buffer_peak *
                                     1000.0f),
                     (unsigned long)renderer_stats->state_changes,
                     (unsigned long)renderer_stats->state_deduplicated,
                     (unsigned long)renderer_stats->rejected_commands);
    }
    pb_log_write(&log, PB_LOG_INFO, "shutdown",
                 "application_free=%lu linear_free=%lu peak_application=%lu "
                 "peak_linear=%lu peak_stack=%lu allocation_failures=%lu "
                 "pressure=%s",
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
    pb_gfx_api_3ds_destroy(graphics);
    state.graphics = NULL;
    pb_renderer_3ds_destroy(renderer);
    pb_log_close(&log);
    aptUnhook(&apt_cookie);
    gfxExit();
    return 0;
}
