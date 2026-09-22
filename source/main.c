#include <3ds.h>
#include <stdio.h>

#include "pb3ds/compat.h"
#include "pb3ds/diagnostics.h"
#include "pb3ds/first_frame.h"
#include "pb3ds/gfx_rendering_api_3ds.h"
#include "pb3ds/input.h"
#include "pb3ds/log.h"
#include "pb3ds/memory.h"
#include "pb3ds/renderer.h"
#include "pb3ds/runtime.h"
#include "pb3ds/title_flow.h"
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
                                const PBGfxBridgeStats *graphics_stats,
                                const PBRuntimeGfxStats *runtime_gfx_stats,
                                const PBFirstFrame *first_frame,
                                bool first_frame_ready,
                                const PBTitleAssets *title_assets,
                                bool title_flow_ready,
                                const PBTitleFlow *title_flow,
                                const PBRuntime *runtime) {
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
    printf("\x1b[1;2HM13 Runtime Recovery\n");
    printf("\x1b[3;2HVersion: %s\n", PB3DS_VERSION);
    printf("\x1b[4;2HBuild: %.12s\n", PB3DS_BUILD_SHA);
    if (runtime->state == PB_RUNTIME_LOADING) {
        printf("\x1b[6;2HStarting PaperBoat...\n");
        printf("\x1b[8;2HStage %lu: %.27s\n",
               (unsigned long)runtime->startup_step,
               runtime->startup_stage != NULL
                   ? runtime->startup_stage : "preparing runtime");
        printf("\x1b[10;2HIndex:%lu Resources:%lu Hits:%lu\n",
               (unsigned long)runtime->resources.index_count,
               (unsigned long)runtime->resources.count,
               (unsigned long)runtime->resources.hits);
        printf("\x1b[12;2HThe app remains responsive while\n");
        printf("\x1b[13;2Hupstream systems initialize.\n");
        printf("\x1b[26;2HSD log: %s\n",
               pb_log_is_persistent(log) ? "ACTIVE" : "unavailable");
        printf("\x1b[30;2HL+R+START exits checkpoint\n");
        return;
    }
    if (runtime->state != PB_RUNTIME_INACTIVE) {
        printf("\x1b[6;2HMap: mac_%02ld entry:%ld slot:%u\n",
               (long)(runtime->stats.map_id > 0
                          ? runtime->stats.map_id - 1
                          : runtime->stats.map_id),
               (long)runtime->stats.entry_id,
               (unsigned int)title_flow->selected_slot + 1U);
        printf("\x1b[7;2HUpstream: %s\n",
               pb_runtime_state_name(runtime->state));
        printf("\x1b[8;2Hboot_main linked; game loop active\n");
        printf("\x1b[9;2HRes:%lu hit:%lu probe:%lu\n",
               (unsigned long)runtime->resources.count,
               (unsigned long)runtime->resources.hits,
               (unsigned long)runtime->resources.lookup_probes);
        printf("\x1b[10;2HUpdates:%llu frames:%llu held:%llu\n",
               (unsigned long long)runtime->stats.updates,
               (unsigned long long)runtime->stats.frames_submitted,
               (unsigned long long)runtime->stats.held_frames);
        printf("\x1b[11;2HMode:%ld block:%lu warn:%lu pause:%d/%d\n",
               (long)runtime->stats.game_mode,
               (unsigned long)runtime->stats.unsupported_mode,
               (unsigned long)runtime->stats.platform_warnings,
               (int)runtime->stats.pause_step,
               (int)runtime->stats.pause_delay);
        printf("\x1b[12;2HPos:%5.0f,%4.0f,%5.0f spd:%3.1f\n",
               (double)runtime->stats.player_x,
               (double)runtime->stats.player_y,
               (double)runtime->stats.player_z,
               (double)runtime->stats.player_speed);
        printf("\x1b[13;2HPlayer action:%d\n",
               (int)runtime->stats.player_action);
        printf("\x1b[14;2HStep:%llums max:%llums slow:%lu\n",
               (unsigned long long)runtime->stats.last_update_ms,
               (unsigned long long)runtime->stats.max_update_ms,
               (unsigned long)runtime->stats.slow_updates);
        if (runtime->state == PB_RUNTIME_FAILED) {
            printf("\x1b[12;2HError: %.30s\n",
                   runtime->error != NULL ? runtime->error : "unknown");
            printf("\x1b[13;2HResource: %.27s\n",
                   runtime->resources.error != NULL
                       ? runtime->resources.error : "none");
        }
        if (graphics_ready) {
            printf("\x1b[15;2HGPU:%llu Draws:%llu Tris:%llu\n",
                   (unsigned long long)graphics_stats->frames_presented,
                   (unsigned long long)graphics_stats->draw_calls,
                   (unsigned long long)graphics_stats->triangles);
            printf("\x1b[16;2HTex:%lu Fall:%llu Reject:%lu\n",
                   (unsigned long)graphics_stats->textures_live,
                   (unsigned long long)(runtime_gfx_stats != NULL
                       ? runtime_gfx_stats->texture_fallbacks : 0U),
                   (unsigned long)graphics_stats->rejected_commands);
        }
        if (runtime_gfx_stats != NULL) {
            printf("\x1b[17;2HDL cmd:%llu unk:%lu miss:%lu\n",
                   (unsigned long long)runtime_gfx_stats->commands,
                   (unsigned long)runtime_gfx_stats->unknown_commands,
                   (unsigned long)runtime_gfx_stats->missing_resources);
            printf("\x1b[18;2HLists:%llu depth:%lu bad:%lu\n",
                   (unsigned long long)runtime_gfx_stats->display_lists,
                   (unsigned long)runtime_gfx_stats->max_call_depth,
                   (unsigned long)runtime_gfx_stats->malformed_lists);
            printf("\x1b[27;2HCmd/f:%lu pk:%lu CC:%llu/%llu 2C:%llu\n",
                   (unsigned long)runtime_gfx_stats->commands_last_frame,
                   (unsigned long)runtime_gfx_stats->commands_peak_frame,
                   (unsigned long long)
                       runtime_gfx_stats->semantic_combiner_batches,
                   (unsigned long long)
                       runtime_gfx_stats->legacy_combiner_fallbacks,
                   (unsigned long long)
                       runtime_gfx_stats->semantic_two_cycle_batches);
        }
        printf("\x1b[19;2HSystem: %s  %s\n",
               state->model_query_ok
                   ? (state->is_new_3ds ? "New 3DS" : "Old 3DS")
                   : "unknown",
               lifecycle_name(state->lifecycle));
        printf("\x1b[20;2HKernel: %08lX\n",
               (unsigned long)state->kernel_version);
        printf("\x1b[21;2HInput: %s N64:%04X\n",
               input->waiting_for_neutral ? "WAIT" : "active",
               (unsigned int)input->n64_held);
        printf("\x1b[22;2HStick:%4d,%4d\n",
               input->stick_x, input->stick_y);
        printf("\x1b[23;2HBudgets: %s fail:%lu\n",
               memory->pressure ? "PRESSURE" : "OK",
               (unsigned long)memory->allocation_failures);
        printf("\x1b[24;2HLinear free: %6lu KiB\n",
               (unsigned long)(memory->linear_free / 1024));
        printf("\x1b[25;2HScene peak: %6lu KiB\n",
               (unsigned long)(memory->class_peak[PB_MEMORY_SCENE] / 1024));
        printf("\x1b[26;2HSD log: %s\n",
               pb_log_is_persistent(log) ? "ACTIVE" : "unavailable");
        printf("\x1b[28;2HUpstream controls and pause active\n");
        printf("\x1b[30;2HL+R+START exits checkpoint\n");
        return;
    }
    if (title_flow_ready) {
        printf("\x1b[6;2HScene: %-11s slot:%u%s\n",
               pb_title_flow_screen_name(title_flow->screen),
               (unsigned int)title_flow->selected_slot + 1U,
               title_flow->slot_confirmed ? " SELECTED" : "");
        printf("\x1b[7;2HAssets: BG+logo+prompt+copy OK\n");
        printf("\x1b[8;2HSafe:320@x40 UV:OK A:%3u\n",
               (unsigned int)title_flow->prompt_alpha);
    } else if (first_frame_ready) {
        printf("\x1b[6;2HScene fallback: title_bg only\n");
        printf("\x1b[7;2HTitle assets: %s\n",
               pb_title_assets_result_name(title_assets->result));
        printf("\x1b[8;2HUV: flip fix applied; no flow\n");
    } else {
        printf("\x1b[6;2HFallback: %s\n",
               first_frame->result == PB_FIRST_FRAME_READY
                   ? "GPU upload failed"
                   : pb_first_frame_result_name(first_frame->result));
        printf("\x1b[7;2HO2R: %s scan:%lu\n",
               pb_o2r_result_name(first_frame->archive_result),
               (unsigned long)first_frame->archive_stats.entries_scanned);
        printf("\x1b[8;2HDiagnostic: checker + sail\n");
    }
    printf("\x1b[9;2HAPI: %s  PICA:%s\n",
           pb_gfx_api_init_result_name(graphics_result),
           renderer_ready ? "ready" : "unavailable");
    if (graphics_ready) {
        printf("\x1b[10;2HTEV shaders:%lu unsupported:%lu\n",
               (unsigned long)graphics_stats->shaders_live,
               (unsigned long)graphics_stats->unsupported_shaders);
        printf("\x1b[11;2HTextures:%lu %lu KiB\n",
               (unsigned long)graphics_stats->textures_live,
               (unsigned long)(graphics_stats->texture_bytes / 1024U));
        printf("\x1b[12;2HFrames:%llu Draws:%llu Tris:%llu\n",
               (unsigned long long)graphics_stats->frames_presented,
               (unsigned long long)graphics_stats->draw_calls,
               (unsigned long long)graphics_stats->triangles);
        printf("\x1b[13;2HVertices:%llu Reject:%lu fail:%lu\n",
               (unsigned long long)graphics_stats->vertices,
               (unsigned long)graphics_stats->rejected_commands,
               (unsigned long)graphics_stats->frame_failures);
    }
    if (renderer_ready) {
        printf("\x1b[14;2HStream:%lu/%lu vtx ovf:%lu\n",
               (unsigned long)renderer_stats->stream_peak_vertices,
               (unsigned long)renderer_stats->stream_capacity_vertices,
               (unsigned long)renderer_stats->stream_overflows);
        printf("\x1b[15;2HCmd peak:%u.%u%%  VBO:%lu KiB\n",
               command_permille / 10U, command_permille % 10U,
               (unsigned long)(renderer_stats->vertex_buffer_bytes / 1024U));
    }
    printf("\x1b[17;2HSystem: %s\n",
           state->model_query_ok
               ? (state->is_new_3ds ? "New 3DS" : "Old 3DS")
               : "unknown");
    printf("\x1b[18;2HKernel: %08lX  %s\n",
           (unsigned long)state->kernel_version,
           lifecycle_name(state->lifecycle));
    printf("\x1b[19;2HInput gate: %s\n",
           input->waiting_for_neutral ? "WAIT NEUTRAL" : "active");
    printf("\x1b[20;2HN64:%04X  Stick:%4d,%4d\n",
           (unsigned int)input->n64_held, input->stick_x, input->stick_y);
    printf("\x1b[21;2HMenu SELECT: %s\n",
           menu_request_seen ? "REQUESTED" : "ready");

    printf("\x1b[23;2HBudgets: %s fail:%lu\n",
           memory->pressure ? "PRESSURE" : "OK",
           (unsigned long)memory->allocation_failures);
    if (!memory->application_measurement_available) {
        printf("\x1b[24;2HApp free: unavailable\n");
    } else {
        printf("\x1b[24;2HApp free: %6lu KiB\n",
               (unsigned long)(memory->application_free / 1024));
    }
    printf("\x1b[25;2HLinear free: %6lu KiB\n",
           (unsigned long)(memory->linear_free / 1024));
    printf("\x1b[26;2HSD log: %s\n",
           pb_log_is_persistent(log) ? "ACTIVE" : "unavailable");
    printf("\x1b[28;2HConfig:%lu Engine:%s Game:%s\n",
           (unsigned long)config->count,
           engine_archive_available ? "OK" : "--",
           game_archive_available ? "OK" : "--");
    if (title_flow_ready && title_flow->screen == PB_TITLE_FLOW_FILE_SELECT) {
        printf("\x1b[29;2HMove: Pad  A launch  B title\n");
    } else {
        printf("\x1b[29;2HA/START opens file select\n");
    }
    printf("\x1b[30;2HL+R+START exits checkpoint\n");
}

static void sample_memory(PBMemoryMonitor *monitor) {
    const uintptr_t stack_marker = (uintptr_t)&monitor;
    pb_memory_monitor_sample(monitor, stack_marker);
}

static void show_boot_checkpoint(const char *stage) {
    printf("\x1b[2J");
    printf("\x1b[2;2HM13 Runtime Recovery\n");
    printf("\x1b[4;2HVersion: %s\n", PB3DS_VERSION);
    printf("\x1b[6;2HStarting PaperBoat...\n");
    printf("\x1b[8;2H%s\n", stage != NULL ? stage : "booting");
    printf("\x1b[10;2HIf startup stops here, record this stage.\n");
    fflush(stdout);
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
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
    PBFirstFrame first_frame;
    pb_first_frame_init(&first_frame);
    bool first_frame_ready = false;
    PBTitleAssets title_assets;
    pb_title_assets_init(&title_assets);
    PBTitleFlow title_flow;
    pb_title_flow_init(&title_flow);
    bool title_flow_ready = false;
    PBRuntime runtime;
    const uintptr_t stack_anchor = (uintptr_t)&memory_monitor;

    state.model_query_ok = R_SUCCEEDED(APT_CheckNew3DS(&state.is_new_3ds));

    gfxInitDefault();
    pb_memory_monitor_init(&memory_monitor, stack_anchor);
    consoleInit(GFX_BOTTOM, &bottom_console);
    aptHook(&apt_cookie, apt_hook, &state);
    show_boot_checkpoint("1/5 graphics + console ready");

    show_boot_checkpoint("2/5 opening SD resources");
    (void)pb_log_init(&log);
    pb_config_init(&config);
    const bool config_loaded =
        pb_config_load(&config, "sdmc:/3ds/PaperBoat3DS/config.ini");
    const bool engine_archive_available =
        pb_archive_open(&engine_archive, "sdmc:/3ds/PaperBoat3DS/paperboat.o2r");
    const bool game_archive_available =
        pb_archive_open(&game_archive, "sdmc:/3ds/PaperBoat3DS/pm64.o2r");
    show_boot_checkpoint("3/5 creating PICA renderer");
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
    show_boot_checkpoint("4/5 loading title assets");
    if (graphics != NULL) {
        if (pb_first_frame_load(&first_frame, &game_archive,
                                &memory_monitor) == PB_FIRST_FRAME_READY) {
            first_frame_ready = pb_gfx_api_3ds_prepare_first_frame(
                graphics, first_frame.rgba, first_frame.texture_width,
                first_frame.texture_height, first_frame.source_width,
                first_frame.source_height);
        }
        if (first_frame_ready &&
            pb_title_assets_load(&title_assets, &game_archive,
                                 &memory_monitor) == PB_TITLE_ASSETS_READY) {
            title_flow_ready =
                pb_gfx_api_3ds_prepare_title_flow(graphics, &title_assets);
        }
        pb_title_assets_release_pixels(&title_assets, &memory_monitor);
        pb_first_frame_release_pixels(&first_frame, &memory_monitor);
    } else if (!game_archive_available) {
        first_frame.result = PB_FIRST_FRAME_ARCHIVE_MISSING;
    }
    show_boot_checkpoint("5/5 entering title loop");
    state.graphics = graphics;
    pb_runtime_init(&runtime, &game_archive, &memory_monitor, &input,
                    graphics, &log);

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
    pb_log_write(&log, first_frame_ready ? PB_LOG_INFO : PB_LOG_WARNING,
                 "first-frame",
                 "status=\"%s\" archive=\"%s\" uploaded=%s source=%ux%u "
                 "texture=%ux%u scan=%lu compressed=%lu decoded=%lu",
                 pb_first_frame_result_name(first_frame.result),
                 pb_o2r_result_name(first_frame.archive_result),
                 first_frame_ready ? "yes" : "no",
                 first_frame.source_width, first_frame.source_height,
                 first_frame.texture_width, first_frame.texture_height,
                 (unsigned long)first_frame.archive_stats.entries_scanned,
                 (unsigned long)first_frame.archive_stats.compressed_bytes,
                 (unsigned long)first_frame.archive_stats.uncompressed_bytes);
    pb_log_write(&log, title_flow_ready ? PB_LOG_INFO : PB_LOG_WARNING,
                 "title-flow",
                 "assets=\"%s\" uploaded=%s screen=%s orientation=fix-applied",
                 pb_title_assets_result_name(title_assets.result),
                 title_flow_ready ? "yes" : "no",
                 pb_title_flow_screen_name(title_flow.screen));

    print_bottom_screen(&bottom_console, &state, &log, &config,
                        engine_archive_available, game_archive_available,
                        memory, &input, false, renderer_result,
                        pb_renderer_3ds_stats(renderer), graphics_result,
                        pb_gfx_api_3ds_stats(graphics),
                        pb_gfx_api_3ds_runtime_stats(graphics), &first_frame,
                        first_frame_ready, &title_assets, title_flow_ready,
                        &title_flow, &runtime);

    u32 memory_sample_frames = 0;
    u32 diagnostics_refresh_frames = 0;
    u32 active_input_refresh_frames = 0;
    bool menu_request_seen = false;
    bool renderer_frame_failed = false;
    bool runtime_failure_logged = false;
    while (aptMainLoop()) {
        pb_input_poll(&input);
        if ((input.native_pressed & KEY_START) != 0 &&
            (input.native_held & (KEY_L | KEY_R)) == (KEY_L | KEY_R)) {
            break;
        }
        if (runtime.state == PB_RUNTIME_LOADING &&
            state.lifecycle == LIFECYCLE_ACTIVE) {
            if (!pb_runtime_continue_startup(&runtime) &&
                !runtime_failure_logged) {
                runtime_failure_logged = true;
                pb_log_write(&log, PB_LOG_ERROR, "runtime-launch",
                             "startup failed stage=\"%s\" error=%s "
                             "resource=%s",
                             runtime.startup_stage != NULL
                                 ? runtime.startup_stage : "unknown",
                             runtime.error != NULL
                                 ? runtime.error : "unknown",
                             runtime.resources.error != NULL
                                 ? runtime.resources.error : "none");
            }
            state.redraw_bottom = true;
        } else if (runtime.state == PB_RUNTIME_ACTIVE &&
            state.lifecycle == LIFECYCLE_ACTIVE) {
            if (!pb_runtime_update(&runtime)) {
                state.redraw_bottom = true;
                if (!runtime_failure_logged) {
                    runtime_failure_logged = true;
                    pb_log_write(&log, PB_LOG_ERROR, "runtime",
                                 "update failed: %s resource=%s",
                                 runtime.error != NULL
                                     ? runtime.error : "unknown",
                                 runtime.resources.error != NULL
                                     ? runtime.resources.error : "none");
                }
            }
        } else if (title_flow_ready &&
                   runtime.state == PB_RUNTIME_INACTIVE &&
                   state.lifecycle == LIFECYCLE_ACTIVE) {
            const PBTitleFlowEvent event =
                pb_title_flow_update(&title_flow, &input);
            if (event != PB_TITLE_FLOW_EVENT_NONE) {
                state.redraw_bottom = true;
                pb_log_write(&log, PB_LOG_INFO, "title-flow",
                             "event=\"%s\" screen=\"%s\" slot=%u",
                             pb_title_flow_event_name(event),
                             pb_title_flow_screen_name(title_flow.screen),
                             (unsigned int)title_flow.selected_slot + 1U);
                if (event == PB_TITLE_FLOW_EVENT_CONFIRM_SLOT) {
                    state.redraw_bottom = true;
                    show_boot_checkpoint(
                        "Loading indexed upstream runtime");
                    const bool started =
                        pb_runtime_begin_toad_town(&runtime);
                    pb_log_write(&log,
                                 started ? PB_LOG_INFO : PB_LOG_ERROR,
                                 "runtime-launch",
                                 "slot=%u state=%s error=%s",
                                 (unsigned int)title_flow.selected_slot + 1U,
                                 pb_runtime_state_name(runtime.state),
                                 runtime.error != NULL
                                     ? runtime.error : "none");
                }
            }
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
                                pb_gfx_api_3ds_stats(graphics),
                                pb_gfx_api_3ds_runtime_stats(graphics),
                                &first_frame,
                                first_frame_ready, &title_assets,
                                title_flow_ready, &title_flow, &runtime);
        }

        if (graphics != NULL && state.lifecycle == LIFECYCLE_ACTIVE) {
            if (runtime.state != PB_RUNTIME_INACTIVE) {
                if (!runtime.frame_submitted) {
                    gspWaitForVBlank();
                }
                continue;
            }
            const bool rendered = title_flow_ready
                ? pb_gfx_api_3ds_render_title_flow(graphics, &title_flow)
                : (first_frame_ready
                       ? pb_gfx_api_3ds_render_first_frame(graphics)
                       : pb_gfx_api_3ds_render_diagnostic(graphics));
            if (!rendered &&
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
    const PBRuntimeGfxStats *runtime_gfx_stats =
        pb_gfx_api_3ds_runtime_stats(graphics);
    if (runtime_gfx_stats != NULL) {
        pb_log_write(&log, PB_LOG_INFO, "runtime-gfx-shutdown",
                     "commands=%llu lists=%llu semantic_combiner_batches=%llu "
                     "semantic_two_cycle_batches=%llu "
                     "legacy_combiner_fallbacks=%llu texture_fallbacks=%llu "
                     "unknown=%lu missing=%lu malformed=%lu",
                     (unsigned long long)runtime_gfx_stats->commands,
                     (unsigned long long)runtime_gfx_stats->display_lists,
                     (unsigned long long)
                         runtime_gfx_stats->semantic_combiner_batches,
                     (unsigned long long)
                         runtime_gfx_stats->semantic_two_cycle_batches,
                     (unsigned long long)
                         runtime_gfx_stats->legacy_combiner_fallbacks,
                     (unsigned long long)runtime_gfx_stats->texture_fallbacks,
                     (unsigned long)runtime_gfx_stats->unknown_commands,
                     (unsigned long)runtime_gfx_stats->missing_resources,
                     (unsigned long)runtime_gfx_stats->malformed_lists);
    }
    if (renderer_stats != NULL) {
        pb_log_write(&log, PB_LOG_INFO, "renderer-shutdown",
                     "frames=%llu draws=%llu vertices=%llu frame_failures=%lu "
                     "command_peak_permille=%lu state_changes=%lu cached=%lu "
                     "rejected=%lu stream_peak_vertices=%lu "
                     "stream_overflows=%lu",
                     (unsigned long long)renderer_stats->frames,
                     (unsigned long long)renderer_stats->draw_calls,
                     (unsigned long long)renderer_stats->vertices,
                     (unsigned long)renderer_stats->frame_failures,
                     (unsigned long)(renderer_stats->command_buffer_peak *
                                     1000.0f),
                     (unsigned long)renderer_stats->state_changes,
                     (unsigned long)renderer_stats->state_deduplicated,
                     (unsigned long)renderer_stats->rejected_commands,
                     (unsigned long)renderer_stats->stream_peak_vertices,
                     (unsigned long)renderer_stats->stream_overflows);
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

    pb_runtime_shutdown(&runtime);
    pb_archive_close(&game_archive);
    pb_archive_close(&engine_archive);
    pb_title_assets_release_pixels(&title_assets, &memory_monitor);
    pb_first_frame_release_pixels(&first_frame, &memory_monitor);
    pb_gfx_api_3ds_destroy(graphics);
    state.graphics = NULL;
    pb_renderer_3ds_destroy(renderer);
    pb_log_close(&log);
    aptUnhook(&apt_cookie);
    gfxExit();
    return 0;
}
