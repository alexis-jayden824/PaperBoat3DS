#ifdef PB3DS_GAME_OBJECTS

/*
 * Link-time stand-ins for PaperBoat / LUS / audio / event symbols that are
 * not part of the 3DS shell. These exist so M13_LINK_GAME can produce an
 * ELF. They are not a substitute for M14 audio or a real LUS engine.
 * Unprototyped definitions match any caller arity at link time.
 */

#include "pb3ds/log.h"

void PB3DS_RuntimeUnsupportedMode(int mode_id) {
    (void)mode_id;
    pb_log(PB_LOG_WARNING, "runtime", "unsupported game mode");
}

void is_debug_panic() {}

int osGetCount() { return 0; }
void osCreateMesgQueue() {}
void osRecvMesg() {}
void osSetEventMesg() {}
void osContInit() {}

void EventSystemCallEvent() {}
void FrameInterpolation_RecordOpenChild() {}
void FrameInterpolation_RecordCloseChild() {}
void FrameInterpolation_RecordMatrixMtxFToMtx() {}

int GameEngine_GetTexHeightExact() { return 0; }
int GameEngine_GetTexWidthExact() { return 0; }
void GameEngine_InvalidateTextureCache() {}
int GameEngine_OTRSigCheck() { return 0; }

void *OTRGetDimensionFromLeftEdge() { return 0; }
void *OTRGetDimensionFromRightEdge() { return 0; }
int OTRGetRectDimensionFromLeftEdge() { return 0; }
int OTRGetRectDimensionFromRightEdge() { return 0; }
int OTRGetScissorCoordX() { return 0; }

void *ResourceGetDataByCrc() { return 0; }
void *ResourceGetNameByCrc() { return 0; }
int ResourceGetSizeByName() { return 0; }

void *GetActorPos() { return 0; }
void *Sprite_GetDataHeader() { return 0; }
void *Sprite_GetNPCSize() { return 0; }
void *Sprite_GetPlayerRasterHeader() { return 0; }
void *Sprite_GetPlayerRasterLoadDescriptors() { return 0; }
void *Sprite_GetPlayerRasterPath() { return 0; }
void *Sprite_GetPlayerRasterSets() { return 0; }
void *Sprite_GetPlayerSize() { return 0; }
void Sprite_LoadNPC() {}
void Sprite_LoadPlayer() {}
void Sprite_LoadPlayerRaster() {}

void gDPSetTextureImageOTR() {}
void gSPDisplayListOTR() {}
void gSPVertexOTR() {}
void gbi_resolve_vtx_in_static_dl() {}
void gfx_create_framebuffer() {}
void gfx_register_fb_texture() {}

void dx_debug_console_main() {}
void dx_debug_draw_collision() {}
void dx_debug_evt_force_detach() {}
void dx_debug_evt_reset() {}
int dx_debug_is_cheat_enabled() { return 0; }
void dx_debug_menu_main() {}
void dx_debug_set_map_info() {}
int dx_debug_should_hide_models() { return 0; }
void dx_hashed_debug_printf() {}

void load_battle() {}
void reset_battle_status() {}
void set_battle_formation() {}
void set_battle_stage() {}

void bgm_adjust_proximity() {}
void bgm_pop_battle_song() {}
void bgm_pop_song() {}
void bgm_push_battle_song() {}
void bgm_push_song() {}
void bgm_quiet_max_volume() {}
void bgm_reset_max_volume() {}
void bgm_set_battle_song() {}
void bgm_set_song() {}
void bgm_update_music_control() {}
void play_ambient_sounds() {}
void update_ambient_sounds() {}
void snd_stop_sound() {}
void sfx_clear_env_sounds() {}
int sfx_get_reverb_mode() { return 0; }
void sfx_play_sound() {}
void sfx_play_sound_at_npc() {}
void sfx_play_sound_at_player() {}
void sfx_play_sound_at_position() {}
void sfx_play_sound_with_params() {}
void sfx_reset_door_sounds() {}
void sfx_set_reverb_mode() {}
void sfx_stop_env_sounds() {}
void sfx_stop_sound() {}
void sfx_update_env_sound_params() {}

int GameFrameUpdateID;
int HudElementPostDrawID;
int HudElementPreDrawID;
int HudElementUpdateID;
int MessageDrawSetupID;
int MessagePostDrawID;
int MessagePreDrawID;
int MessageUpdateID;
int OnPlayerBPCostCheckID;
int OnSaveFileSaveID;
int ScriptFrameUpdateID;
int ScriptRequestUpdateID;
int WorkerDrawID;
int WorkerUpdateID;

unsigned char gBattleStatus[8192];
unsigned char gMusicControlData[256];
int gCurrentDoorSounds;
int gCurrentRoomDoorSounds;
unsigned char port_sprite_palette_data[256];

#endif
