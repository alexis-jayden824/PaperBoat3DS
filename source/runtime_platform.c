#include "pb3ds/runtime.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "audio/public.h"
#include "battle/battle.h"
#include "game_modes.h"
#include "gbi_custom.h"
#include "port/interpolation/FrameInterpolation.h"

extern void init_game_globals(void);
extern void load_engine_data(void);
extern void Graphics_ThreadUpdate(void);

static PBRuntime *active_runtime;
static uint64_t runtime_time;

const char *pb_runtime_state_name(PBRuntimeState state) {
    switch (state) {
        case PB_RUNTIME_LOADING: return "loading";
        case PB_RUNTIME_ACTIVE: return "active";
        case PB_RUNTIME_FAILED: return "failed";
        case PB_RUNTIME_INACTIVE:
        default: return "inactive";
    }
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

bool pb_runtime_start_toad_town(PBRuntime *runtime) {
    if (runtime == NULL || runtime->resources.archive == NULL ||
        runtime->resources.archive->file == NULL || runtime->input == NULL ||
        runtime->graphics == NULL) {
        if (runtime != NULL) {
            runtime->state = PB_RUNTIME_FAILED;
            runtime->error = "runtime prerequisites unavailable";
        }
        return false;
    }
    active_runtime = runtime;
    pb_runtime_resources_bind(&runtime->resources);
    runtime->state = PB_RUNTIME_LOADING;
    init_game_globals();
    load_engine_data();

    /* Keep the original gAreas numbering: Toad Town is area 1.  Its real map
     * table keeps the original placeholder at index 0, so mac_00 is map 1. */
    gGameStatusPtr->areaID = 1;
    gGameStatusPtr->mapID = 1;
    gGameStatusPtr->entryID = 6;
    gGameStatusPtr->prevArea = 1;
    gGameStatusPtr->demoState = DEMO_STATE_NONE;
    runtime->stats.area_id = gGameStatusPtr->areaID;
    runtime->stats.map_id = gGameStatusPtr->mapID;
    runtime->stats.entry_id = gGameStatusPtr->entryID;
    set_game_mode(GAME_MODE_ENTER_DEMO_WORLD);
    runtime->state = PB_RUNTIME_ACTIVE;
    if (runtime->log != NULL) {
        pb_log_write(runtime->log, PB_LOG_INFO, "runtime",
                     "upstream activated area=mac map=mac_00 entry=6");
    }
    return true;
}

bool pb_runtime_update(PBRuntime *runtime) {
    if (runtime == NULL || runtime != active_runtime ||
        runtime->state != PB_RUNTIME_ACTIVE) return false;
    runtime->frame_submitted = false;
    Graphics_ThreadUpdate();
    runtime->stats.updates++;
    runtime->stats.game_mode = get_game_mode();
    runtime->stats.area_id = gGameStatusPtr->areaID;
    runtime->stats.map_id = gGameStatusPtr->mapID;
    runtime->stats.entry_id = gGameStatusPtr->entryID;
    runtime->stats.player_x = gPlayerStatus.pos.x;
    runtime->stats.player_y = gPlayerStatus.pos.y;
    runtime->stats.player_z = gPlayerStatus.pos.z;
    runtime->stats.player_speed = gPlayerStatus.curSpeed;
    runtime->stats.player_action = gPlayerStatus.actionState;
    if (!runtime->frame_submitted) runtime->stats.held_frames++;
    return runtime->state != PB_RUNTIME_FAILED;
}

void pb_runtime_shutdown(PBRuntime *runtime) {
    if (runtime == NULL) return;
    if (active_runtime == runtime) active_runtime = NULL;
    pb_runtime_resources_clear(&runtime->resources);
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
        active_runtime->state = PB_RUNTIME_FAILED;
        active_runtime->error = "display-list submission failed";
        return;
    }
    active_runtime->frame_submitted = true;
    active_runtime->stats.frames_submitted++;
}

void GameEngine_StartAudioFrame(void) {}
void GameEngine_EndAudioFrame(void) {}
void GameEngine_HoldFrame(void) {}

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
uint64_t osGetTime(void) { return runtime_time++; }
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
    if (displayList == NULL) return;
    for (Gfx *command = displayList;; command++) {
        const unsigned int opcode = command->words.w0 >> 24U;
        if (opcode == G_ENDDL) return;
        if (opcode == G_VTX && command->words.w1 != 0U &&
            GameEngine_OTRSigCheck((const char *)command->words.w1)) {
            void *data = ResourceGetDataByName((const char *)command->words.w1);
            if (data != NULL) command->words.w1 = (uintptr_t)data;
        }
    }
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
