#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int16_t s16;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define BIT(n) (1U << (n))

#define KEY_A BIT(0)
#define KEY_B BIT(1)
#define KEY_SELECT BIT(2)
#define KEY_START BIT(3)
#define KEY_DRIGHT BIT(4)
#define KEY_DLEFT BIT(5)
#define KEY_DUP BIT(6)
#define KEY_DDOWN BIT(7)
#define KEY_R BIT(8)
#define KEY_L BIT(9)
#define KEY_X BIT(10)
#define KEY_Y BIT(11)
#define KEY_ZL BIT(14)
#define KEY_ZR BIT(15)
#define KEY_TOUCH BIT(20)
#define KEY_CSTICK_RIGHT BIT(24)
#define KEY_CSTICK_LEFT BIT(25)
#define KEY_CSTICK_UP BIT(26)
#define KEY_CSTICK_DOWN BIT(27)
#define KEY_CPAD_RIGHT BIT(28)
#define KEY_CPAD_LEFT BIT(29)
#define KEY_CPAD_UP BIT(30)
#define KEY_CPAD_DOWN BIT(31)

typedef struct {
    u16 px;
    u16 py;
} touchPosition;

typedef struct {
    s16 dx;
    s16 dy;
} circlePosition;

typedef int Result;

typedef enum {
    APTHOOK_ONSUSPEND,
    APTHOOK_ONSLEEP,
    APTHOOK_ONRESTORE,
    APTHOOK_ONWAKEUP,
    APTHOOK_ONEXIT,
} APT_HookType;

typedef struct {
    int unused;
} aptHookCookie;

typedef struct {
    int unused;
} PrintConsole;

#define GFX_TOP 0
#define GFX_BOTTOM 1
#define R_SUCCEEDED(result) ((result) >= 0)

#define MEMREGION_APPLICATION 0

u32 osGetMemRegionFree(int region);
u32 linearSpaceFree(void);
void *linearAlloc(size_t size);
void linearFree(void *memory);

void hidScanInput(void);
u32 hidKeysHeld(void);
void hidCircleRead(circlePosition *position);
void hidTouchRead(touchPosition *position);

u32 osGetKernelVersion(void);
Result APT_CheckNew3DS(bool *is_new_3ds);
void gfxInitDefault(void);
void gfxExit(void);
PrintConsole *consoleInit(int screen, PrintConsole *console);
PrintConsole *consoleSelect(PrintConsole *console);
void aptHook(aptHookCookie *cookie, void (*callback)(APT_HookType, void *),
             void *param);
void aptUnhook(aptHookCookie *cookie);
bool aptMainLoop(void);
void gfxFlushBuffers(void);
void gfxSwapBuffers(void);
void gspWaitForVBlank(void);
