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
#include "pb3ds/title_flow.h"
#include "pb3ds/version.h"
#include "pb3ds/world_boot.h"
#include "pb3ds/world_scene.h"

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
                                const PBFirstFrame *first_frame,
                                bool first_frame_ready,
                                const PBTitleAssets *title_assets,
                                bool title_flow_ready,
                                const PBTitleFlow *title_flow,
                                const PBWorldScene *world_scene,
                                const PBWorldFlow *world_flow,
                                bool world_frame_ready) {
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
    if (world_flow->state == PB_WORLD_FLOW_ACTIVE ||
        world_flow->state == PB_WORLD_FLOW_PAUSED) {
        printf("\x1b[6;2HMap: %s  entry:%u  slot:%u\n",
               world_scene->map_id, (unsigned int)world_scene->entry_id,
               (unsigned int)world_flow->selected_slot + 1U);
        printf("\x1b[7;2HState: %s\n",
               pb_world_flow_state_name(world_flow->state));
        printf("\x1b[8;2HDiag scene: %s floor:%d\n",
               pb_world_scene_result_name(world_scene->result),
               world_scene->current_floor);
        printf("\x1b[9;2HMesh:%lu tri %lu tex %lu DL\n",
               (unsigned long)world_scene->stats.triangles,
               (unsigned long)world_scene->stats.textures,
               (unsigned long)world_scene->stats.display_lists);
        printf("\x1b[10;2HShape:%lu node %lu leaf %lu vtx\n",
               (unsigned long)world_scene->stats.shape_nodes,
               (unsigned long)world_scene->stats.leaf_models,
               (unsigned long)world_scene->stats.source_vertices);
        printf("\x1b[11;2HHit: %lu col %lu vtx %lu tri\n",
               (unsigned long)world_scene->stats.colliders,
               (unsigned long)world_scene->stats.collision_vertices,
               (unsigned long)world_scene->stats.collision_triangles);
        printf("\x1b[12;2HPos:%5.0f,%4.0f,%5.0f move:%lu\n",
               (double)world_scene->player_position.x,
               (double)world_scene->player_position.y,
               (double)world_scene->player_position.z,
               (unsigned long)world_scene->movement_frames);
        printf("\x1b[13;2HScripts:%lu trans:%lu star:%s\n",
               (unsigned long)world_scene->script_events,
               (unsigned long)world_scene->transition_count,
               world_scene->star_piece_collected ? "GOT" : "ready");
        printf("\x1b[14;2HRuntime: DIAG ONLY %s\n",
               world_frame_ready ? "ready" : "FAILED");
        if (graphics_ready) {
            printf("\x1b[15;2HFrames:%llu Draws:%llu Tris:%llu\n",
                   (unsigned long long)graphics_stats->frames_presented,
                   (unsigned long long)graphics_stats->draw_calls,
                   (unsigned long long)graphics_stats->triangles);
            printf("\x1b[16;2HReject:%lu fail:%lu unsupported:%lu\n",
                   (unsigned long)graphics_stats->rejected_commands,
                   (unsigned long)graphics_stats->frame_failures,
                   (unsigned long)graphics_stats->unsupported_shaders);
        }
        printf("\x1b[18;2HInteract:%s  fade:%u\n",
               pb_world_scene_message_visible(world_scene) ? "SIGN" : "ready",
               (unsigned int)(pb_world_scene_fade_alpha(world_scene) *
                              100.0f));
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
        printf("\x1b[22;2HStick:%4d,%4d  pauses:%lu\n",
               input->stick_x, input->stick_y,
               (unsigned long)world_flow->pause_count);
        printf("\x1b[23;2HBudgets: %s fail:%lu\n",
               memory->pressure ? "PRESSURE" : "OK",
               (unsigned long)memory->allocation_failures);
        printf("\x1b[24;2HLinear free: %6lu KiB\n",
               (unsigned long)(memory->linear_free / 1024));
        printf("\x1b[25;2HScene peak: %6lu KiB\n",
               (unsigned long)(memory->class_peak[PB_MEMORY_SCENE] / 1024));
        printf("\x1b[26;2HSD log: %s\n",
               pb_log_is_persistent(log) ? "ACTIVE" : "unavailable");
        printf("\x1b[28;2HCircle/D-pad move; A sign\n");
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
    if (world_flow->state == PB_WORLD_FLOW_FAILED) {
        printf("\x1b[22;2HWorld: %s%s\n",
               pb_world_scene_result_name(world_scene->result),
               world_scene->result == PB_WORLD_SCENE_READY
                   ? " (GPU failed)" : "");
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
        printf("\x1b[29;2HMove: Pad  A %s  B title\n",
               world_flow->state == PB_WORLD_FLOW_FAILED
                   ? "retry" : "select");
    } else {
        printf("\x1b[29;2HA/START opens file select\n");
    }
    printf("\x1b[30;2HL+R+START exits checkpoint\n");
}

static void sample_memory(PBMemoryMonitor *monitor) {
    const uintptr_t stack_marker = (uintptr_t)&monitor;
    pb_memory_monitor_sample(monitor, stack_marker);
}

static bool load_world_scene(PBWorldScene *scene, PBArchive *archive,
                             const char *map_id, uint8_t entry_id,
                             PBMemoryMonitor *memory,
                             PBGfxApi3DS *graphics) {
    if (scene == NULL || archive == NULL || map_id == NULL ||
        memory == NULL || graphics == NULL) {
        return false;
    }
    pb_world_scene_release(scene, memory);
    if (pb_world_scene_load(scene, archive, map_id, entry_id, memory) !=
        PB_WORLD_SCENE_READY) {
        return false;
    }
    if (!pb_gfx_api_3ds_prepare_world_scene(graphics, scene)) {
        return false;
    }
    pb_world_scene_release_pixels(scene, memory);
    return true;
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
    PBWorldScene world_scene;
    pb_world_scene_init(&world_scene);
    PBWorldFlow world_flow;
    pb_world_flow_init(&world_flow);
    bool world_frame_ready = false;
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
                        pb_gfx_api_3ds_stats(graphics), &first_frame,
                        first_frame_ready, &title_assets, title_flow_ready,
                        &title_flow, &world_scene, &world_flow,
                        world_frame_ready);

    u32 memory_sample_frames = 0;
    u32 diagnostics_refresh_frames = 0;
    u32 active_input_refresh_frames = 0;
    bool menu_request_seen = false;
    bool renderer_frame_failed = false;
    while (aptMainLoop()) {
        pb_input_poll(&input);
        if ((input.native_pressed & KEY_START) != 0 &&
            (input.native_held & (KEY_L | KEY_R)) == (KEY_L | KEY_R)) {
            break;
        }
        if ((world_flow.state == PB_WORLD_FLOW_ACTIVE ||
             world_flow.state == PB_WORLD_FLOW_PAUSED) &&
            state.lifecycle == LIFECYCLE_ACTIVE) {
            const PBWorldFlowEvent event =
                pb_world_flow_update(&world_flow, &input);
            if (event != PB_WORLD_FLOW_EVENT_NONE) {
                state.redraw_bottom = true;
                pb_log_write(&log, PB_LOG_INFO, "world-flow",
                             "event=\"%s\" state=\"%s\" map=%s entry=%u",
                             pb_world_flow_event_name(event),
                             pb_world_flow_state_name(world_flow.state),
                             world_scene.map_id,
                             (unsigned int)world_scene.entry_id);
            } else if (world_flow.state == PB_WORLD_FLOW_ACTIVE) {
                const PBWorldSceneEvent scene_event =
                    pb_world_scene_update(&world_scene, &input);
                if (scene_event != PB_WORLD_SCENE_EVENT_NONE) {
                    state.redraw_bottom = true;
                    pb_log_write(
                        &log, PB_LOG_INFO, "world-scene",
                        "event=\"%s\" map=%s entry=%u pos=%.1f,%.1f,%.1f "
                        "floor=%d scripts=%lu transitions=%lu",
                        pb_world_scene_event_name(scene_event),
                        world_scene.map_id,
                        (unsigned int)world_scene.entry_id,
                        (double)world_scene.player_position.x,
                        (double)world_scene.player_position.y,
                        (double)world_scene.player_position.z,
                        world_scene.current_floor,
                        (unsigned long)world_scene.script_events,
                        (unsigned long)world_scene.transition_count);
                }
                if (scene_event ==
                    PB_WORLD_SCENE_EVENT_TRANSITION_REQUESTED) {
                    char requested_map[PB_WORLD_MAP_ID_CAPACITY];
                    snprintf(requested_map, sizeof(requested_map), "%s",
                             world_scene.requested_map);
                    const uint8_t requested_entry =
                        world_scene.requested_entry;
                    (void)pb_world_flow_begin_transition(&world_flow);
                    world_frame_ready = load_world_scene(
                        &world_scene, &game_archive, requested_map,
                        requested_entry, &memory_monitor, graphics);
                    const PBWorldFlowEvent load_event =
                        pb_world_flow_finish(&world_flow,
                                             world_frame_ready);
                    pb_log_write(
                        &log,
                        world_frame_ready ? PB_LOG_INFO : PB_LOG_ERROR,
                        "world-transition",
                        "event=\"%s\" result=\"%s\" archive=\"%s\" "
                        "map=%s entry=%u tris=%lu textures=%lu "
                        "hit=%lu/%lu/%lu",
                        pb_world_flow_event_name(load_event),
                        pb_world_scene_result_name(world_scene.result),
                        pb_o2r_result_name(world_scene.archive_result),
                        requested_map, (unsigned int)requested_entry,
                        (unsigned long)world_scene.stats.triangles,
                        (unsigned long)world_scene.stats.textures,
                        (unsigned long)world_scene.stats.colliders,
                        (unsigned long)world_scene.stats.collision_vertices,
                        (unsigned long)world_scene.stats.collision_triangles);
                }
            }
        } else if (title_flow_ready &&
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
                if (event == PB_TITLE_FLOW_EVENT_CONFIRM_SLOT &&
                    pb_world_flow_request(&world_flow,
                                          title_flow.selected_slot) ==
                        PB_WORLD_FLOW_EVENT_LOAD_REQUESTED) {
                    state.redraw_bottom = true;
                    world_frame_ready = load_world_scene(
                        &world_scene, &game_archive, "mac_00", 6U,
                        &memory_monitor, graphics);
                    const PBWorldFlowEvent world_event =
                        pb_world_flow_finish(&world_flow,
                                             world_frame_ready);
                    pb_log_write(
                        &log,
                        world_frame_ready ? PB_LOG_INFO : PB_LOG_ERROR,
                        "world-scene",
                        "event=\"%s\" result=\"%s\" map=%s entry=%u "
                        "slot=%u nodes=%lu leaves=%lu dl=%lu vertices=%lu "
                        "triangles=%lu textures=%lu hit=%lu/%lu/%lu "
                        "scene_uploaded=%s",
                        pb_world_flow_event_name(world_event),
                        pb_world_scene_result_name(world_scene.result),
                        world_scene.map_id,
                        (unsigned int)world_scene.entry_id,
                        (unsigned int)world_flow.selected_slot + 1U,
                        (unsigned long)world_scene.stats.shape_nodes,
                        (unsigned long)world_scene.stats.leaf_models,
                        (unsigned long)world_scene.stats.display_lists,
                        (unsigned long)world_scene.stats.source_vertices,
                        (unsigned long)world_scene.stats.triangles,
                        (unsigned long)world_scene.stats.textures,
                        (unsigned long)world_scene.stats.colliders,
                        (unsigned long)world_scene.stats.collision_vertices,
                        (unsigned long)world_scene.stats.collision_triangles,
                        world_frame_ready ? "yes" : "no");
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
                                pb_gfx_api_3ds_stats(graphics), &first_frame,
                                first_frame_ready, &title_assets,
                                title_flow_ready, &title_flow, &world_scene,
                                &world_flow, world_frame_ready);
        }

        if (graphics != NULL && state.lifecycle == LIFECYCLE_ACTIVE) {
            const bool rendered = world_frame_ready
                ? pb_gfx_api_3ds_render_world_scene(
                      graphics, &world_scene,
                      world_flow.state == PB_WORLD_FLOW_PAUSED)
                : (title_flow_ready
                ? pb_gfx_api_3ds_render_title_flow(graphics, &title_flow)
                : (first_frame_ready
                       ? pb_gfx_api_3ds_render_first_frame(graphics)
                       : pb_gfx_api_3ds_render_diagnostic(graphics)));
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

    pb_archive_close(&game_archive);
    pb_archive_close(&engine_archive);
    pb_world_scene_release(&world_scene, &memory_monitor);
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
