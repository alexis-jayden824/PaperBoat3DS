#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimum PaperBoat engine surface (M6). This is not desktop libultraship.
 * Unsupported and deferred calls fail explicitly and are logged.
 */
#define PB_COMPAT_NAME_MAX 80U

typedef enum {
    PB_COMPAT_READY = 0,
    PB_COMPAT_UNSUPPORTED,
    PB_COMPAT_DEFERRED_M8,
    PB_COMPAT_DEFERRED_M9,
    PB_COMPAT_DEFERRED_M10,
    PB_COMPAT_DEFERRED_M11,
    PB_COMPAT_DEFERRED_M14,
    PB_COMPAT_DEFERRED_M15,
} PBCompatStatus;

typedef struct {
    uint16_t button;
    int8_t stick_x;
    int8_t stick_y;
    uint8_t errnum;
} PBOSContPad;

typedef struct {
    PBCompatStatus resources;
    PBCompatStatus logging;
    PBCompatStatus config;
    PBCompatStatus controller;
    PBCompatStatus time;
    PBCompatStatus gfx;
    PBCompatStatus audio;
    char last_unsupported[PB_COMPAT_NAME_MAX];
} PBCompatState;

void pb_compat_init(void);
void pb_compat_query(PBCompatState *state);
const char *pb_compat_status_name(PBCompatStatus status);
PBCompatStatus pb_compat_unsupported(const char *symbol);
bool pb_compat_desktop_engine_allowed(void);
void pb_compat_poll_controller(PBOSContPad *pad);
uint64_t pb_compat_tick_ms(void);

void *ResourceGetDataByName(const char *name);
void *GameEngine_GetDataExact(const char *name);
void GameEngine_LogInfo(const char *fmt, ...);
void GameEngine_LogWarn(const char *fmt, ...);
void GameEngine_LogError(const char *fmt, ...);
void Graphics_PushFrame(void *display_list);
void GameEngine_StartAudioFrame(void);
void GameEngine_EndAudioFrame(void);
void GameEngine_HoldFrame(void);
int GameEngine_GetSaveFilePath(char *dst, unsigned dst_size);
int CVarGetInteger(const char *name, int default_value);
float CVarGetFloat(const char *name, float default_value);
void CVarSetInteger(const char *name, int value);

#ifdef __cplusplus
}
#endif
