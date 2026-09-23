#include "pb3ds/runtime.h"
#include "pb3ds/compat.h"
#include "pb3ds/gbi_command_span.h"
#include "pb3ds/gbi_resolve.h"

#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __3DS__
/* libctru's s32/u32 conflict with libultra types in this translation unit. */
void svcSleepThread(long long nanoseconds);
#endif

#include "common.h"
#include "audio/public.h"
#include "battle/battle.h"
#include "game_modes.h"
#include "gbi_custom.h"
#include "port/interpolation/FrameInterpolation.h"
#include "saved_flag_names.h"
#include "sprite.h"

_Static_assert(sizeof(Gfx) == sizeof(PBGbiPacket),
               "static DL walker packet layout must match Gfx");

extern void init_game_globals(void);
extern void Graphics_ThreadUpdate(void);
extern s8 gGameStepDelayCount;
extern s8 StepPauseDelay;
extern s8 StepPauseState;
extern b32 PB3DS_RuntimeHeapStorageAligned(void);

static PBRuntime *active_runtime;
static uint64_t runtime_time;
static jmp_buf runtime_panic_jump;
static bool runtime_panic_armed;

typedef enum {
    PB_START_RESOURCE_INDEX = 1,
    PB_START_GLOBALS,
    PB_START_DEFAULTS,
    PB_START_FLASH,
    PB_START_INPUT,
    PB_START_GENERAL_HEAP,
    PB_START_RENDER_TASKS,
    PB_START_WORKERS,
    PB_START_SCRIPTS,
    PB_START_CAMERAS,
    PB_START_PLAYER_STATUS,
    PB_START_PLAYER_SPRITES,
    PB_START_ENTITY_MODELS,
    PB_START_ANIMATORS,
    PB_START_MODELS,
    PB_START_SPRITE_SHADING,
    PB_START_BACKGROUND,
    PB_START_CHARACTER_SET,
    PB_START_PRINTERS,
    PB_START_GAME_MODE,
    PB_START_NPCS,
    PB_START_HUD,
    PB_START_TRIGGERS,
    PB_START_ENTITIES,
    PB_START_PLAYER_DATA,
    PB_START_ENCOUNTER,
    PB_START_OVERLAYS,
    PB_START_EFFECTS,
    PB_START_SAVED_VARIABLES,
    PB_START_ITEM_ENTITIES,
    PB_START_MUSIC,
    PB_START_AMBIENT,
    PB_START_SOUNDS,
    PB_START_WINDOWS,
    PB_START_CURTAINS,
    PB_START_RUMBLE,
    PB_START_ENGINE_READY,
    PB_START_TOAD_TOWN,
} PBRuntimeStartupStep;

static const char *runtime_startup_stage(uint32_t step) {
    switch ((PBRuntimeStartupStep)step) {
        case PB_START_RESOURCE_INDEX: return "building resource index";
        case PB_START_GLOBALS: return "initializing upstream globals";
        case PB_START_DEFAULTS: return "setting engine defaults";
        case PB_START_FLASH: return "initializing save flash";
        case PB_START_INPUT: return "clearing upstream input";
        case PB_START_GENERAL_HEAP: return "creating general heap";
        case PB_START_RENDER_TASKS: return "clearing render tasks";
        case PB_START_WORKERS: return "clearing worker list";
        case PB_START_SCRIPTS: return "clearing script list";
        case PB_START_CAMERAS: return "creating upstream cameras";
        case PB_START_PLAYER_STATUS: return "clearing player status";
        case PB_START_PLAYER_SPRITES: return "loading player sprites";
        case PB_START_ENTITY_MODELS: return "clearing entity models";
        case PB_START_ANIMATORS: return "clearing animators";
        case PB_START_MODELS: return "clearing model data";
        case PB_START_SPRITE_SHADING: return "clearing sprite shading";
        case PB_START_BACKGROUND: return "resetting background";
        case PB_START_CHARACTER_SET: return "clearing character set";
        case PB_START_PRINTERS: return "loading message font";
        case PB_START_GAME_MODE: return "clearing game mode";
        case PB_START_NPCS: return "clearing NPCs";
        case PB_START_HUD: return "creating HUD cache";
        case PB_START_TRIGGERS: return "clearing triggers";
        case PB_START_ENTITIES: return "clearing entities";
        case PB_START_PLAYER_DATA: return "clearing player data";
        case PB_START_ENCOUNTER: return "initializing encounters";
        case PB_START_OVERLAYS: return "clearing screen overlays";
        case PB_START_EFFECTS: return "clearing effects";
        case PB_START_SAVED_VARIABLES: return "clearing saved variables";
        case PB_START_ITEM_ENTITIES: return "creating item workers";
        case PB_START_MUSIC: return "resetting music state";
        case PB_START_AMBIENT: return "resetting ambient state";
        case PB_START_SOUNDS: return "clearing sound state";
        case PB_START_WINDOWS: return "clearing windows";
        case PB_START_CURTAINS: return "initializing curtains";
        case PB_START_RUMBLE: return "initializing rumble state";
        case PB_START_ENGINE_READY: return "finalizing engine data";
        case PB_START_TOAD_TOWN: return "activating Toad Town";
        default: return "unknown startup stage";
    }
}

static void runtime_mark_startup_stage(PBRuntime *runtime) {
    runtime->startup_stage = runtime_startup_stage(runtime->startup_step);
    if (runtime->log != NULL) {
        pb_log_write(runtime->log, PB_LOG_INFO, "runtime-start",
                     "step=%lu stage=\"%s\" resources=%lu hits=%lu",
                     (unsigned long)runtime->startup_step,
                     runtime->startup_stage,
                     (unsigned long)runtime->resources.count,
                     (unsigned long)runtime->resources.hits);
    }
}

static void runtime_fail(PBRuntime *runtime, const char *error) {
    runtime->state = PB_RUNTIME_FAILED;
    runtime->error = error;
}

static void runtime_set_engine_defaults(void) {
    gOverrideFlags = 0;
    gGameStatusPtr->unk_79 = 0;
    gGameStatusPtr->backgroundFlags = 0;
    gGameStatusPtr->musicEnabled = true;
    gGameStatusPtr->healthBarsEnabled = true;
    gGameStatusPtr->introPart = INTRO_PART_NONE;
    gGameStatusPtr->demoBattleFlags = 0;
    gGameStatusPtr->multiplayerEnabled = false;
    gGameStatusPtr->altViewportOffset.x = -8;
    gGameStatusPtr->altViewportOffset.y = 4;
    gTimeFreezeMode = TIME_FREEZE_NONE;
    gGameStatusPtr->debugQuizmo = 0;
    gGameStatusPtr->unk_13C = 0;
    gGameStepDelayCount = 5;
    gGameStatusPtr->saveCount = 0;
}

static void runtime_finish_engine_data(void) {
    for (size_t i = 0U;
         i < sizeof(gGameStatusPtr->holdRepeatInterval) /
                 sizeof(gGameStatusPtr->holdRepeatInterval[0]); i++) {
        gGameStatusPtr->holdRepeatInterval[i] = 3;
        gGameStatusPtr->holdDelayTime[i] = 12;
    }
    gOverrideFlags |= GLOBAL_OVERRIDES_DISABLE_DRAW_FRAME;
    set_game_mode(GAME_MODE_STARTUP);
}

static void runtime_activate_toad_town(PBRuntime *runtime) {
    /* Keep the original gAreas numbering: Toad Town is area 1.  Its real map
     * table keeps the original placeholder at index 0, so mac_00 is map 1.
     * Entry 1 is the authentic mac_01 -> mac_00 entrance.  The former debug
     * entry 6 shares the kmr_20 sewer-pipe position and can immediately fire
     * an exit into a map that is deliberately outside the M13 registry. */
    gGameStatusPtr->areaID = 1;
    gGameStatusPtr->mapID = 1;
    gGameStatusPtr->entryID = 1;
    gGameStatusPtr->prevArea = 1;
    gGameStatusPtr->demoState = DEMO_STATE_NONE;
    runtime->stats.area_id = gGameStatusPtr->areaID;
    runtime->stats.map_id = gGameStatusPtr->mapID;
    runtime->stats.entry_id = gGameStatusPtr->entryID;
    set_game_mode(GAME_MODE_ENTER_DEMO_WORLD);
    /* First-pause tutorial intercepts START until a long A/stick sequence
     * finishes. Folium START then looks frozen. Skip the badge tutorial. */
    evt_set_variable(NULL, GF_Tutorial_Badges, 0);
    if (runtime->state == PB_RUNTIME_FAILED) return;
    runtime->startup_stage = "upstream runtime active";
    runtime->state = PB_RUNTIME_ACTIVE;
    if (runtime->log != NULL) {
        pb_log_write(runtime->log, PB_LOG_INFO, "runtime",
                     "upstream activated area=mac map=mac_00 entry=1 "
                     "resources=%lu hits=%lu",
                     (unsigned long)runtime->resources.count,
                     (unsigned long)runtime->resources.hits);
    }
}

const char *pb_runtime_state_name(PBRuntimeState state) {
    switch (state) {
        case PB_RUNTIME_LOADING: return "loading";
        case PB_RUNTIME_ACTIVE: return "active";
        case PB_RUNTIME_FAILED: return "failed";
        case PB_RUNTIME_INACTIVE:
        default: return "inactive";
    }
}

void is_debug_panic(const char *message) {
    if (active_runtime != NULL) {
        runtime_fail(active_runtime, "upstream assertion failed");
        if (active_runtime->log != NULL) {
            pb_log_write(active_runtime->log, PB_LOG_ERROR, "runtime-panic",
                         "stage=\"%s\" message=\"%s\"",
                         active_runtime->startup_stage != NULL
                             ? active_runtime->startup_stage : "runtime update",
                         message != NULL ? message : "panic");
        }
    }
    if (runtime_panic_armed) longjmp(runtime_panic_jump, 1);
    abort();
}

void pb_runtime_init(PBRuntime *runtime, PBArchive *archive,
                     PBMemoryMonitor *memory, PBInputState *input,
                     PBGfxApi3DS *graphics, PBLog *log) {
    if (runtime == NULL) return;
    memset(runtime, 0, sizeof(*runtime));
    pb_runtime_resources_init(&runtime->resources, archive, memory);
    runtime->input = input;
    runtime->graphics = graphics;
    runtime->log = log;
}

bool pb_runtime_begin_toad_town(PBRuntime *runtime) {
    if (runtime == NULL || runtime->resources.archive == NULL ||
        runtime->resources.archive->file == NULL || runtime->input == NULL ||
        runtime->graphics == NULL) {
        if (runtime != NULL) {
            runtime->state = PB_RUNTIME_FAILED;
            runtime->error = "runtime prerequisites unavailable";
        }
        return false;
    }
    if (runtime->state != PB_RUNTIME_INACTIVE) {
        runtime_fail(runtime, "runtime already started");
        return false;
    }
    active_runtime = runtime;
    pb_runtime_resources_bind(&runtime->resources);
    runtime->state = PB_RUNTIME_LOADING;
    runtime->error = NULL;
    runtime->startup_step = PB_START_RESOURCE_INDEX;
    runtime_mark_startup_stage(runtime);
    return true;
}

bool pb_runtime_continue_startup(PBRuntime *runtime) {
    if (runtime == NULL || runtime != active_runtime ||
        runtime->state != PB_RUNTIME_LOADING) return false;

    runtime_panic_armed = true;
    if (setjmp(runtime_panic_jump) != 0) {
        runtime_panic_armed = false;
        if (runtime->state != PB_RUNTIME_FAILED) {
            runtime_fail(runtime, "upstream assertion failed");
        }
        return false;
    }

    switch ((PBRuntimeStartupStep)runtime->startup_step) {
        case PB_START_RESOURCE_INDEX:
            if (!pb_runtime_resources_prepare(&runtime->resources)) {
                runtime_fail(runtime,
                    runtime->resources.error != NULL
                        ? runtime->resources.error
                        : "resource index unavailable");
                if (runtime->log != NULL) {
                    pb_log_write(runtime->log, PB_LOG_ERROR, "runtime",
                                 "resource index failed: %s (%s)",
                                 runtime->error,
                                 pb_o2r_result_name(
                                     runtime->resources.archive_error));
                }
            } else if (runtime->log != NULL) {
                pb_log_write(runtime->log, PB_LOG_INFO, "runtime-start",
                             "resource index ready entries=%lu bytes=%lu",
                             (unsigned long)runtime->resources.index_count,
                             (unsigned long)runtime->resources.index_allocation);
            }
            break;
        case PB_START_GLOBALS: init_game_globals(); break;
        case PB_START_DEFAULTS: runtime_set_engine_defaults(); break;
        case PB_START_FLASH: fio_init_flash(); break;
        case PB_START_INPUT: clear_input(); break;
        case PB_START_GENERAL_HEAP:
            if (!PB3DS_RuntimeHeapStorageAligned()) {
                runtime_fail(runtime, "upstream heap storage is misaligned");
            } else {
                general_heap_create();
            }
            break;
        case PB_START_RENDER_TASKS: clear_render_tasks(); break;
        case PB_START_WORKERS: clear_worker_list(); break;
        case PB_START_SCRIPTS: clear_script_list(); break;
        case PB_START_CAMERAS: create_cameras(); break;
        case PB_START_PLAYER_STATUS: clear_player_status(); break;
        case PB_START_PLAYER_SPRITES:
            spr_init_sprites(PLAYER_SPRITES_MARIO_WORLD);
            break;
        case PB_START_ENTITY_MODELS: clear_entity_models(); break;
        case PB_START_ANIMATORS: clear_animator_list(); break;
        case PB_START_MODELS: clear_model_data(); break;
        case PB_START_SPRITE_SHADING: clear_sprite_shading_data(); break;
        case PB_START_BACKGROUND: reset_background_settings(); break;
        case PB_START_CHARACTER_SET: clear_character_set(); break;
        case PB_START_PRINTERS: clear_printers(); break;
        case PB_START_GAME_MODE: clear_game_mode(); break;
        case PB_START_NPCS: clear_npcs(); break;
        case PB_START_HUD: hud_element_clear_cache(); break;
        case PB_START_TRIGGERS: clear_trigger_data(); break;
        case PB_START_ENTITIES: clear_entity_data(false); break;
        case PB_START_PLAYER_DATA: clear_player_data(); break;
        case PB_START_ENCOUNTER: init_encounter_status(); break;
        case PB_START_OVERLAYS: clear_screen_overlays(); break;
        case PB_START_EFFECTS: clear_effect_data(); break;
        case PB_START_SAVED_VARIABLES: clear_saved_variables(); break;
        case PB_START_ITEM_ENTITIES: clear_item_entity_data(); break;
        case PB_START_MUSIC: bgm_reset_sequence_players(); break;
        case PB_START_AMBIENT: reset_ambient_sounds(); break;
        case PB_START_SOUNDS: sfx_clear_sounds(); break;
        case PB_START_WINDOWS: clear_windows(); break;
        case PB_START_CURTAINS: initialize_curtains(); break;
        case PB_START_RUMBLE: poll_rumble(); break;
        case PB_START_ENGINE_READY: runtime_finish_engine_data(); break;
        case PB_START_TOAD_TOWN: runtime_activate_toad_town(runtime); break;
        default: runtime_fail(runtime, "invalid runtime startup stage"); break;
    }
    runtime_panic_armed = false;

    if (runtime->state == PB_RUNTIME_FAILED) return false;
    if (runtime->state == PB_RUNTIME_ACTIVE) return true;
    runtime->startup_step++;
    runtime_mark_startup_stage(runtime);
    return true;
}

bool pb_runtime_start_toad_town(PBRuntime *runtime) {
    if (!pb_runtime_begin_toad_town(runtime)) return false;
    while (runtime->state == PB_RUNTIME_LOADING) {
        if (!pb_runtime_continue_startup(runtime)) return false;
    }
    return runtime->state == PB_RUNTIME_ACTIVE;
}

bool pb_runtime_update(PBRuntime *runtime) {
    if (runtime == NULL || runtime != active_runtime ||
        runtime->state != PB_RUNTIME_ACTIVE) return false;
    runtime->frame_submitted = false;
    const int32_t mode_before = get_game_mode();
    const int8_t pause_step_before = StepPauseState;
    const int8_t pause_delay_before = StepPauseDelay;
    if (runtime->log != NULL && mode_before == GAME_MODE_PAUSE &&
        (pause_step_before != runtime->stats.pause_step ||
         pause_delay_before != runtime->stats.pause_delay)) {
        pb_log_write(runtime->log, PB_LOG_INFO, "pause-step",
                     "update=%llu step=%d delay=%d",
                     (unsigned long long)runtime->stats.updates + 1U,
                     (int)pause_step_before, (int)pause_delay_before);
    }
    const uint64_t update_started_ms = pb_platform_time_ms();
    runtime_panic_armed = true;
    if (setjmp(runtime_panic_jump) != 0) {
        runtime_panic_armed = false;
        if (runtime->state != PB_RUNTIME_FAILED) {
            runtime_fail(runtime, "upstream assertion failed");
        }
        return false;
    }
    Graphics_ThreadUpdate();
    const uint64_t update_elapsed_ms =
        pb_platform_time_ms() - update_started_ms;
    runtime_panic_armed = false;
    runtime->stats.updates++;
    runtime->stats.last_update_ms = update_elapsed_ms;
    if (update_elapsed_ms > runtime->stats.max_update_ms) {
        runtime->stats.max_update_ms = update_elapsed_ms;
    }
    if (update_elapsed_ms >= 100U) runtime->stats.slow_updates++;
    runtime->stats.game_mode = get_game_mode();
    runtime->stats.area_id = gGameStatusPtr->areaID;
    runtime->stats.map_id = gGameStatusPtr->mapID;
    runtime->stats.entry_id = gGameStatusPtr->entryID;
    runtime->stats.pause_step = StepPauseState;
    runtime->stats.pause_delay = StepPauseDelay;
    runtime->stats.player_x = gPlayerStatus.pos.x;
    runtime->stats.player_y = gPlayerStatus.pos.y;
    runtime->stats.player_z = gPlayerStatus.pos.z;
    runtime->stats.player_speed = gPlayerStatus.curSpeed;
    runtime->stats.player_action = gPlayerStatus.actionState;
    if (!runtime->frame_submitted) runtime->stats.held_frames++;
    if (runtime->log != NULL && update_elapsed_ms >= 500U) {
        pb_log_write(runtime->log, PB_LOG_WARNING, "slow-update",
                     "update=%llu elapsed_ms=%llu mode_before=%ld "
                     "mode_after=%ld pause_step=%d pause_delay=%d "
                     "frame=%s",
                     (unsigned long long)runtime->stats.updates,
                     (unsigned long long)update_elapsed_ms,
                     (long)mode_before, (long)runtime->stats.game_mode,
                     (int)runtime->stats.pause_step,
                     (int)runtime->stats.pause_delay,
                     runtime->frame_submitted ? "submitted" : "held");
    }
    return runtime->state != PB_RUNTIME_FAILED;
}

void pb_runtime_shutdown(PBRuntime *runtime) {
    if (runtime == NULL) return;
    if (active_runtime == runtime) active_runtime = NULL;
    pb_runtime_resources_clear(&runtime->resources);
    runtime->startup_stage = NULL;
    runtime->startup_step = 0U;
    runtime->state = PB_RUNTIME_INACTIVE;
}

void PB3DS_RuntimeUnsupportedMode(s32 modeID) {
    if (active_runtime == NULL) return;
    active_runtime->stats.unsupported_mode = (uint32_t)(modeID + 1);
    active_runtime->state = PB_RUNTIME_FAILED;
    active_runtime->error = "upstream requested a mode outside M13";
    if (active_runtime->log != NULL) {
        pb_log_write(active_runtime->log, PB_LOG_ERROR, "runtime",
                     "blocked out-of-scope game mode=%ld", (long)modeID);
    }
}

void Graphics_PushFrame(Gfx *displayList) {
    if (active_runtime == NULL || active_runtime->graphics == NULL ||
        displayList == NULL) return;
    if (!pb_gfx_api_3ds_render_display_list(active_runtime->graphics,
                                            (const PBRuntimeGfx *)displayList)) {
        active_runtime->stats.platform_warnings++;
        if (active_runtime->log != NULL) {
            pb_log_write(active_runtime->log, PB_LOG_WARNING, "gfx",
                         "display-list submission incomplete");
        }
        return;
    }
    active_runtime->frame_submitted = true;
    active_runtime->stats.frames_submitted++;
}

void GameEngine_StartAudioFrame(void) {}
void GameEngine_EndAudioFrame(void) {}
void GameEngine_HoldFrame(void) {
#ifdef __3DS__
    /* PaperBoat sleeps ~1/30s so DISABLE_DRAW_FRAME (START pause setup)
     * does not spin the CPU. The outer loop already waits for VBlank when
     * no frame was submitted. */
    svcSleepThread(33000000LL); /* ~1/30s, matching PaperBoat HoldFrame */
#endif
}

void GameEngine_ReadController(void *opaquePads) {
    OSContPad *pads = opaquePads;
    if (pads == NULL) return;
    memset(pads, 0, sizeof(*pads) * 4U);
    if (active_runtime == NULL || active_runtime->input == NULL) return;
    pads[0].button = active_runtime->input->n64_held;
    pads[0].stick_x = active_runtime->input->stick_x;
    pads[0].stick_y = active_runtime->input->stick_y;
    pads[0].err_no = 0;
}

void *GameEngine_Malloc(size_t size) { return malloc(size); }

void GameEngine_LogInfo(const char *format, ...) {
    if (format == NULL) return;
    char message[384];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    if (active_runtime != NULL && active_runtime->log != NULL) {
        pb_log_write(active_runtime->log, PB_LOG_INFO, "upstream", "%s",
                     message);
    }
}

void GameEngine_LogStackTrace(const char *label) {
    GameEngine_LogInfo("stack trace unavailable: %s",
                       label != NULL ? label : "unnamed");
}

void GameEngine_InvalidateTextureCache(const void *address) {
    pb_gfx_api_3ds_invalidate_texture(active_runtime != NULL
                                          ? active_runtime->graphics
                                          : NULL,
                                      address);
}

void gfx_texture_cache_clear(void) {
    pb_gfx_api_3ds_invalidate_texture(active_runtime != NULL
                                          ? active_runtime->graphics
                                          : NULL,
                                      NULL);
}

int GameEngine_GetSaveFilePath(char *buffer, int bufferSize) {
    static const char path[] = "sdmc:/3ds/PaperBoat3DS/m13-runtime.sav";
    if (buffer == NULL || bufferSize <= 0 ||
        sizeof(path) > (size_t)bufferSize) return -1;
    memcpy(buffer, path, sizeof(path));
    return 0;
}

void GameEngine_ClearDepthBuffer(void) {
    pb_gfx_api_3ds_clear_depth(active_runtime != NULL
                                   ? active_runtime->graphics
                                   : NULL);
}

float GameEngine_GetAspectRatio(void) { return 4.0f / 3.0f; }
float OTRGetDimensionFromLeftEdgeForcedAspect(float value, float aspect) {
    const float selected = aspect > 0.0f ? aspect : GameEngine_GetAspectRatio();
    return 160.0f - 120.0f * selected + value;
}
float OTRGetDimensionFromRightEdgeForcedAspect(float value, float aspect) {
    const float selected = aspect > 0.0f ? aspect : GameEngine_GetAspectRatio();
    return 160.0f + 120.0f * selected - value;
}
float OTRGetDimensionFromLeftEdge(float value) {
    return OTRGetDimensionFromLeftEdgeForcedAspect(value, 0.0f);
}
float OTRGetDimensionFromRightEdge(float value) {
    return OTRGetDimensionFromRightEdgeForcedAspect(value, 0.0f);
}
int16_t OTRGetRectDimensionFromLeftEdgeForcedAspect(float value,
                                                     float aspect) {
    return (int16_t)floorf(OTRGetDimensionFromLeftEdgeForcedAspect(value,
                                                                   aspect));
}
int16_t OTRGetRectDimensionFromRightEdgeForcedAspect(float value,
                                                      float aspect) {
    return (int16_t)ceilf(OTRGetDimensionFromRightEdgeForcedAspect(value,
                                                                   aspect));
}
int16_t OTRGetRectDimensionFromLeftEdge(float value) {
    return OTRGetRectDimensionFromLeftEdgeForcedAspect(value, 0.0f);
}
int16_t OTRGetRectDimensionFromRightEdge(float value) {
    return OTRGetRectDimensionFromRightEdgeForcedAspect(value, 0.0f);
}
int16_t OTRGetScissorCoordX(float value) { return (int16_t)lroundf(value); }
uint32_t OTRGetGameRenderWidth(void) { return 320U; }
uint32_t OTRGetGameRenderHeight(void) { return 240U; }

void EventSystemCallEvent(int32_t id, void *event, const char *file, int line,
                          const char *key) {
    (void)id; (void)event; (void)file; (void)line; (void)key;
}

int32_t GameFrameUpdateID = -1;
int32_t HudElementPostDrawID = -1;
int32_t HudElementPreDrawID = -1;
int32_t HudElementUpdateID = -1;
int32_t MessageDrawSetupID = -1;
int32_t MessagePostDrawID = -1;
int32_t MessagePreDrawID = -1;
int32_t MessageUpdateID = -1;
int32_t OnPlayerBPCostCheckID = -1;
int32_t OnSaveFileEraseID = -1;
int32_t OnSaveFileLoadID = -1;
int32_t OnSaveFileSaveID = -1;
int32_t ScriptFrameUpdateID = -1;
int32_t ScriptRequestUpdateID = -1;
int32_t WorkerDrawID = -1;
int32_t WorkerUpdateID = -1;

void FrameInterpolation_RecordOpenChild(const void *key, uintptr_t value) {
    (void)key; (void)value;
}
void FrameInterpolation_RecordCloseChild(void) {}
void FrameInterpolation_RecordMatrixMtxFToMtx(MtxF *source, Mtx *destination) {
    (void)source; (void)destination;
}

int32_t osContInit(OSMesgQueue *queue, uint8_t *bits, OSContStatus *status) {
    (void)queue;
    if (bits != NULL) *bits = 1;
    if (status != NULL) memset(status, 0, sizeof(*status));
    return 0;
}
void osCreateMesgQueue(OSMesgQueue *queue, OSMesg *messages, int32_t count) {
    if (queue != NULL) {
        memset(queue, 0, sizeof(*queue));
        queue->msg = messages;
        queue->msgCount = count;
    }
}
int32_t osRecvMesg(OSMesgQueue *queue, OSMesg *message, int32_t flag) {
    (void)queue; (void)flag;
    if (message != NULL) *message = (OSMesg){0};
    return 0;
}
void osSetEventMesg(OSEvent event, OSMesgQueue *queue, OSMesg message) {
    (void)event; (void)queue; (void)message;
}
uint32_t osGetCount(void) { return (uint32_t)(runtime_time++); }
void osSetTime(OSTime time) { runtime_time = time; }

void gSPVertexOTR(Gfx *packet, uintptr_t vertices, int count, int first) {
    if (GameEngine_OTRSigCheck((const char *)vertices)) {
        vertices = (uintptr_t)ResourceGetDataByName((const char *)vertices);
    }
    __gSPVertex(packet, vertices, count, first);
}
void gSPDisplayListOTR(Gfx *packet, const void *displayList) {
    if (GameEngine_OTRSigCheck((const char *)displayList)) {
        displayList = ResourceGetDataByName((const char *)displayList);
    }
    __gSPDisplayList(packet, displayList);
}
void gDPSetTextureImageOTR(Gfx *packet, int format, int size, int width,
                           uintptr_t image) {
    if (GameEngine_OTRSigCheck((const char *)image)) {
        gSetImage(packet, 0x25, format, size, width, image);
    } else {
        gSetImage(packet, G_SETTIMG, format, size, width, image);
    }
}
void gbi_resolve_vtx_in_static_dl(Gfx *displayList) {
    /*
     * Match PaperBoat GBIMiddleware: resolve even G_VTX OTR paths, recurse
     * G_DL (push vs branch), and skip TEXRECT extra words so pause_init
     * cannot hang on HUD lists.
     */
    pb_gbi_resolve_vtx_in_static_dl((PBGbiPacket *)displayList,
                                    GameEngine_OTRSigCheck,
                                    ResourceGetDataByName);
}

int gfx_create_framebuffer(unsigned int width, unsigned int height,
                           unsigned int nativeWidth,
                           unsigned int nativeHeight, unsigned char resize,
                           unsigned char fixedAspect) {
    (void)width; (void)height; (void)nativeWidth; (void)nativeHeight;
    (void)resize; (void)fixedAspect;
    return -1;
}
void gfx_register_fb_texture(const void *address, int framebuffer) {
    (void)address; (void)framebuffer;
}

MusicControlData gMusicControlData[2];
u16 gCurrentDoorSounds;
u16 gCurrentRoomDoorSounds;

void bgm_reset_sequence_players(void) { memset(gMusicControlData, 0, sizeof(gMusicControlData)); }
void bgm_update_music_control(void) {}
s32 bgm_set_song(s32 player, s32 song, s32 variation, s32 fade, s16 volume) {
    (void)player; (void)song; (void)variation; (void)fade; (void)volume; return 1;
}
b32 bgm_fade_in_song(s32 player, s32 song, s32 variation, s32 fade,
                     s16 start, s16 end) {
    (void)player; (void)song; (void)variation; (void)fade; (void)start; (void)end; return true;
}
s32 bgm_adjust_proximity(s32 player, s32 mix, s16 state) {
    (void)player; (void)mix; (void)state; return true;
}
AuResult bgm_set_track_volumes(s32 player, s16 set) {
    (void)player; (void)set; return AU_RESULT_OK;
}
void bgm_quiet_max_volume(void) {}
void bgm_reset_max_volume(void) {}
void bgm_push_song(s32 song, s32 variation) { (void)song; (void)variation; }
void bgm_pop_song(void) {}
void bgm_push_battle_song(void) {}
void bgm_pop_battle_song(void) {}
void bgm_set_battle_song(s32 song, s32 variation) { (void)song; (void)variation; }
void reset_ambient_sounds(void) {}
void update_ambient_sounds(void) {}
s32 play_ambient_sounds(s32 sound, s32 fade) { (void)sound; (void)fade; return 1; }
void sfx_reset_door_sounds(void) { gCurrentDoorSounds = gCurrentRoomDoorSounds = 0; }
void sfx_clear_sounds(void) {}
void sfx_clear_env_sounds(s16 play) { (void)play; }
void sfx_update_env_sound_params(void) {}
void sfx_set_reverb_mode(s32 mode) { (void)mode; }
s32 sfx_get_reverb_mode(void) { return 0; }
void sfx_stop_env_sounds(void) {}
void sfx_stop_tracking_env_sound_pos(s32 sound, s32 keep) { (void)sound; (void)keep; }
void sfx_play_sound_with_params(s32 sound, u8 volume, u8 pan, s16 pitch) { (void)sound; (void)volume; (void)pan; (void)pitch; }
void sfx_stop_sound(s32 sound) { (void)sound; }
void sfx_play_sound(s32 sound) { (void)sound; }
void sfx_play_sound_at_player(s32 sound, s32 flags) { (void)sound; (void)flags; }
void sfx_play_sound_at_npc(s32 sound, s32 flags, s32 npc) { (void)sound; (void)flags; (void)npc; }
void sfx_play_sound_at_position(s32 sound, s32 flags, f32 x, f32 y, f32 z) { (void)sound; (void)flags; (void)x; (void)y; (void)z; }
void snd_song_poll_music_events(u32 **events, s32 *count) { if (count != NULL) *count = 0; (void)events; }
void snd_song_flush_music_events(void) {}
void snd_stop_sound(s32 sound) { (void)sound; }

s32 gBattleState;
BattleStatus gBattleStatus;
s32 gLastDrawBattleState;
s32 gDefeatedBattleSubstate;
s32 gBattleSubState;
s32 gDefeatedBattleState;
s32 gCurrentBattleID;
s32 gCurrentStageID;

void reset_battle_status(void) { memset(&gBattleStatus, 0, sizeof(gBattleStatus)); }
void set_battle_formation(Battle *battle) { (void)battle; }
void set_battle_stage(s32 stage) { gCurrentStageID = stage; }
void load_battle(s32 battle) { gCurrentBattleID = battle; PB3DS_RuntimeUnsupportedMode(GAME_MODE_BATTLE); }
void load_demo_battle(u32 battle) { load_battle((s32)battle); }
API_CALLABLE(GetActorPos) { (void)script; (void)isInitialCall; return ApiStatus_DONE2; }

void dx_debug_console_main(void) {}
void dx_debug_draw_collision(void) {}
void dx_debug_evt_force_detach(Evt *script) { (void)script; }
void dx_debug_evt_reset(void) {}
b32 dx_debug_is_cheat_enabled(DebugCheat cheat) { (void)cheat; return false; }
void dx_debug_menu_main(void) {}
void dx_debug_set_map_info(char *map, s32 entry) { (void)map; (void)entry; }
b32 dx_debug_should_hide_models(void) { return false; }
void dx_hashed_debug_printf(const char *file, s32 line, const char *format, ...) {
    (void)file; (void)line; (void)format;
}
