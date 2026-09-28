#!/usr/bin/env python3
"""Generate the deliberately narrow M13 PaperBoat runtime registry sources."""

from __future__ import annotations

import argparse
from pathlib import Path


WORLD_SUFFIX = r'''
#define AREA(area, jp_name) { ARRAY_COUNT(area##_maps), area##_maps, "area_" #area, jp_name }
#define MAP(map) .id = #map, .settings = &map##_settings, .dmaStart = map##_ROM_START, .dmaEnd = map##_ROM_END, .dmaDest = map##_VRAM

#include "world/area_mac/mac.h"

/* AREA_MAC must remain index 1, matching the original gAreas table.  The
 * unavailable index-zero entry prevents this recovery build from silently
 * claiming chapter coverage that M13 has not accepted. */
MapConfig pb3ds_unavailable_maps[1] = { { 0 } };
MapConfig mac_maps[] = {
    { .id = "machi_unavailable", .settings = &mac_00_settings },
    { MAP(mac_00), .bgName = "nok_bg" },
    { MAP(mac_01), .bgName = "nok_bg" },
};

AreaConfig gAreas[] = {
    { 0, pb3ds_unavailable_maps, "area_kmr", "" },
    AREA(mac, "Toad Town"),
    {},
};
'''


GAME_MODES = r'''#include "common.h"
#include "game_modes.h"

extern void PB3DS_RuntimeUnsupportedMode(s32 modeID);
extern void PB3DS_RuntimeRestartToTitle(void);

enum GameModeFlags {
    MODE_FLAG_NONE = 0,
    MODE_FLAG_INITIALIZED = 1,
    MODE_FLAG_NEEDS_STEP = 2,
    MODE_FLAG_HAS_FRONT_UI = 4,
};

typedef struct GameModeData {
    u16 flags;
    void (*init)(void);
    void (*step)(void);
    void (*renderBackUI)(void);
    void (*renderFrontUI)(void);
} GameModeData;

static void game_mode_nop(void) {}
static void game_mode_restart(void) { PB3DS_RuntimeRestartToTitle(); }

#define MODE(initfn, stepfn, drawfn) \
    { .init = initfn, .step = stepfn, .renderBackUI = drawfn }

const GameModeData GameModeTemplates[] = {
    [GAME_MODE_STARTUP] = MODE(game_mode_restart, game_mode_nop, game_mode_nop),
    [GAME_MODE_LOGOS] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
    [GAME_MODE_TITLE_SCREEN] = MODE(state_init_title_screen, state_step_title_screen, state_drawUI_title_screen),
    [GAME_MODE_ENTER_DEMO_WORLD] = MODE(state_init_enter_demo, state_step_enter_world, state_drawUI_enter_world),
    [GAME_MODE_ENTER_WORLD] = MODE(state_init_enter_world, state_step_enter_world, state_drawUI_enter_world),
    [GAME_MODE_WORLD] = MODE(state_init_world, state_step_world, state_drawUI_world),
    [GAME_MODE_CHANGE_MAP] = MODE(state_init_change_map, state_step_change_map, state_drawUI_change_map),
    [GAME_MODE_GAME_OVER] = MODE(state_init_game_over, state_step_game_over, state_drawUI_game_over),
    [GAME_MODE_BATTLE] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
    [GAME_MODE_END_BATTLE] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
    [GAME_MODE_PAUSE] = MODE(state_init_pause, state_step_pause, state_drawUI_pause),
    [GAME_MODE_UNPAUSE] = MODE(state_init_unpause, state_step_unpause, state_drawUI_unpause),
    [GAME_MODE_FILE_SELECT] = MODE(state_init_file_select, state_step_file_select, state_drawUI_file_select),
    [GAME_MODE_END_FILE_SELECT] = MODE(state_init_exit_file_select, state_step_exit_file_select, state_drawUI_exit_file_select),
    [GAME_MODE_INTRO] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
    [GAME_MODE_DEMO] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
};

BSS s32 CurGameModeID;
BSS GameModeData CurGameMode;

static b32 mode_supported(s32 modeID) {
    return modeID == GAME_MODE_STARTUP || modeID == GAME_MODE_TITLE_SCREEN ||
           (modeID >= GAME_MODE_ENTER_DEMO_WORLD &&
            modeID <= GAME_MODE_GAME_OVER) ||
           modeID == GAME_MODE_PAUSE || modeID == GAME_MODE_UNPAUSE ||
           modeID == GAME_MODE_FILE_SELECT ||
           modeID == GAME_MODE_END_FILE_SELECT;
}

s32 get_game_mode(void) { return CurGameModeID; }

void set_game_mode(s32 modeID) {
    if (modeID < 0 || modeID >= ARRAY_COUNT(GameModeTemplates)) {
        PB3DS_RuntimeUnsupportedMode(modeID);
        return;
    }
    if (!mode_supported(modeID)) {
        PB3DS_RuntimeUnsupportedMode(modeID);
        return;
    }
    /* M13 deliberately contains only the accepted Toad Town maps. Preserve
     * the real title and file menu, then route a confirmed slot into mac_00
     * instead of following a save into an unlinked chapter map. */
    if (modeID == GAME_MODE_ENTER_WORLD &&
        CurGameModeID == GAME_MODE_END_FILE_SELECT) {
        gGameStatusPtr->areaID = 1;
        gGameStatusPtr->mapID = 1;
        gGameStatusPtr->entryID = 1;
        gGameStatusPtr->prevArea = 1;
        gGameStatusPtr->demoState = DEMO_STATE_NONE;
    }
    const GameModeData *template = &GameModeTemplates[modeID];
    CurGameModeID = modeID;
    CurGameMode = *template;
    CurGameMode.flags = MODE_FLAG_INITIALIZED | MODE_FLAG_NEEDS_STEP;
    if (CurGameMode.init == nullptr) CurGameMode.init = game_mode_nop;
    if (CurGameMode.step == nullptr) CurGameMode.step = game_mode_nop;
    if (CurGameMode.renderBackUI == nullptr) CurGameMode.renderBackUI = game_mode_nop;
    CurGameMode.renderFrontUI = game_mode_nop;
    CurGameMode.init();
}

void clear_game_mode(void) { CurGameMode.flags = MODE_FLAG_NONE; }

void set_game_mode_render_frontUI(void (*fn)(void)) {
    CurGameMode.renderFrontUI = fn != nullptr ? fn : game_mode_nop;
    CurGameMode.flags |= MODE_FLAG_HAS_FRONT_UI;
}

void step_game_mode(void) {
    if (CurGameMode.flags == MODE_FLAG_NONE) return;
    CurGameMode.flags &= ~MODE_FLAG_NEEDS_STEP;
    CurGameMode.step();
}

void render_game_mode_backUI(void) {
    if (CurGameMode.flags != MODE_FLAG_NONE) CurGameMode.renderBackUI();
}

void render_game_mode_frontUI(void) {
    if (CurGameMode.flags != MODE_FLAG_NONE &&
        !(CurGameMode.flags & MODE_FLAG_NEEDS_STEP) &&
        (CurGameMode.flags & MODE_FLAG_HAS_FRONT_UI)) {
        CurGameMode.renderFrontUI();
    }
}
'''


NUSYS_ABI_SIGNATURES = {
    "void osInvalICache(void* vaddr, s32 size)":
        "void osInvalICache(void* vaddr, int32_t size)",
    "void osInvalDCache(void* vaddr, s32 size)":
        "void osInvalDCache(void* vaddr, int32_t size)",
    "void osWritebackDCache(void* vaddr, s32 size)":
        "void osWritebackDCache(void* vaddr, int32_t size)",
}


NUSYS_FLASH_STATE = r'''// Flash emulation state
#define FLASH_PAGE_BYTES PB3DS_FLASH_PAGE_BYTES
#define FLASH_SECTOR_BYTES PB3DS_FLASH_SECTOR_BYTES
#define FLASH_TOTAL_SIZE PB3DS_FLASH_TOTAL_BYTES
static u8 sFlashWriteBuf[FLASH_PAGE_BYTES];
static char sSaveFilePath[512];
static s32 sSaveFilePathValid = 0;
static OSPiHandle sFlashHandle;
'''


NUSYS_PI_READ = r'''void nuPiReadRom(u32 romAddr, void* ramAddr, u32 len) {
    /* The accepted runtime has no N64 ROM address space.  Keep diagnostic
     * callers deterministic, while all gameplay assets use the O2R service. */
    (void) romAddr;
    if (ramAddr != NULL && len != 0U) {
        memset(ramAddr, 0, len);
    }
}
'''


NUSYS_CONTROLLER_SERVICES = r'''u8 nuContInit(void) {
    /* The 3DS platform service exposes one logical N64 controller backed by
     * HID; osContInit reports that same port through the libultra ABI. */
    OSMesgQueue dummyMq = { 0 };
    OSContStatus dummyStatus = { 0 };
    u8 controllerBits = 0;
    if (osContInit(&dummyMq, &controllerBits, &dummyStatus) != 0) {
        return 0;
    }
    return controllerBits & 1U;
}

u8 nuSiMgrInit(void) {
    return 1;
}

u8 nuContMgrInit(void) {
    return 1;
}

void nuContPakMgrInit(void) {
    /* Controller Pak hardware is absent; every PFS operation reports NOPACK. */
}

void nuContRmbMgrInit(void) {
    /* Rumble is outside M13 and no 3DS backend is registered. */
}

void nuContRmbForceStop(void) {}
void nuContRmbForceStopEnd(void) {}

s32 nuContRmbCheck(u32 port) {
    (void) port;
    return -1;
}

void nuContRmbModeSet(u32 port, u8 mode) {
    (void) port;
    (void) mode;
}

void nuContRmbStart(u32 port, u16 freq, u16 frame) {
    (void) port;
    (void) freq;
    (void) frame;
}
'''


NUSYS_FLASH_INIT = r'''OSPiHandle* osFlashInit(void) {
    flash_ensure_path();
    return &sFlashHandle;
}
'''


NUSYS_PFS_INIT = r'''s32 osPfsInitPak(OSMesgQueue* mq, OSPfs* pfs, int channel) {
    (void) mq;
    (void) pfs;
    (void) channel;
    return PFS_ERR_NOPACK;
}
'''


NUSYS_PFS_ABSENT = r'''s32 osPfsRepairId(OSPfs* pfs) {
    (void) pfs;
    return PFS_ERR_NOPACK;
}

s32 osPfsAllocateFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName, int size, s32* fileNo) {
    (void) pfs;
    (void) companyCode;
    (void) gameCode;
    (void) gameName;
    (void) extName;
    (void) size;
    if (fileNo != NULL) *fileNo = -1;
    return PFS_ERR_NOPACK;
}

s32 osPfsFindFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName, s32* fileNo) {
    (void) pfs;
    (void) companyCode;
    (void) gameCode;
    (void) gameName;
    (void) extName;
    if (fileNo != NULL) *fileNo = -1;
    return PFS_ERR_NOPACK;
}

s32 osPfsDeleteFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName) {
    (void) pfs;
    (void) companyCode;
    (void) gameCode;
    (void) gameName;
    (void) extName;
    return PFS_ERR_NOPACK;
}

s32 osPfsReadWriteFile(OSPfs* pfs, s32 fileNo, u8 flag, int offset, int size, u8* data) {
    (void) pfs;
    (void) fileNo;
    (void) flag;
    (void) offset;
    (void) size;
    (void) data;
    return PFS_ERR_NOPACK;
}

s32 osPfsFileState(OSPfs* pfs, s32 fileNo, OSPfsState* state) {
    (void) pfs;
    (void) fileNo;
    if (state != NULL) memset(state, 0, sizeof(*state));
    return PFS_ERR_NOPACK;
}

s32 osPfsFreeBlocks(OSPfs* pfs, s32* bytes) {
    (void) pfs;
    if (bytes != NULL) *bytes = 0;
    return PFS_ERR_NOPACK;
}

s32 osPfsNumFiles(OSPfs* pfs, s32* maxFiles, s32* filesUsed) {
    (void) pfs;
    if (maxFiles != NULL) *maxFiles = 0;
    if (filesUsed != NULL) *filesUsed = 0;
    return PFS_ERR_NOPACK;
}
'''

NUSYS_FLASH_ERASE = r'''s32 osFlashSectorErase(u32 page_num) {
    const u32 pages_per_sector = FLASH_SECTOR_BYTES / FLASH_PAGE_BYTES;
    const u32 total_pages = FLASH_TOTAL_SIZE / FLASH_PAGE_BYTES;
    flash_ensure_path();
    if (!sSaveFilePathValid || page_num % pages_per_sector != 0 ||
        page_num > total_pages - pages_per_sector) {
        return -1;
    }
    return pb_flash_store_erase_sector(
        sSaveFilePath, page_num * FLASH_PAGE_BYTES) ? 0 : -1;
}
'''

NUSYS_FLASH_READ = r'''s32 osFlashReadArray(OSIoMesg* mb, s32 priority, u32 page_num, void* dramAddr, u32 n_pages, OSMesgQueue* mq) {
    (void) mb;
    (void) priority;
    (void) mq;

    const u32 total_pages = FLASH_TOTAL_SIZE / FLASH_PAGE_BYTES;
    flash_ensure_path();
    if (!sSaveFilePathValid || dramAddr == NULL || n_pages > total_pages ||
        page_num > total_pages - n_pages) {
        return -1;
    }
    return pb_flash_store_read(sSaveFilePath, page_num * FLASH_PAGE_BYTES,
                               dramAddr, n_pages * FLASH_PAGE_BYTES) ? 0 : -1;
}
'''

NUSYS_FLASH_BUFFER = r'''s32 osFlashWriteBuffer(OSIoMesg* mb, s32 priority, void* dramAddr, OSMesgQueue* mq) {
    (void) mb;
    (void) priority;
    (void) mq;

    if (dramAddr == NULL) {
        return -1;
    }
    memcpy(sFlashWriteBuf, dramAddr, FLASH_PAGE_BYTES);
    return 0;
}
'''

NUSYS_FLASH_WRITE = r'''s32 osFlashWriteArray(u32 page_num) {
    const u32 total_pages = FLASH_TOTAL_SIZE / FLASH_PAGE_BYTES;
    flash_ensure_path();
    if (!sSaveFilePathValid || page_num >= total_pages) {
        return -1;
    }
    return pb_flash_store_write_page(
        sSaveFilePath, page_num * FLASH_PAGE_BYTES, sFlashWriteBuf) ? 0 : -1;
}
'''


HEAP_STORAGE = r'''#include "common.h"

/*
 * PaperBoat's allocator aligns the address passed to _heap_create(), but its
 * malloc/free entry points subsequently use the storage symbol itself as the
 * first HeapNode.  The original N64 linker and the desktop toolchains place
 * these arrays on a 16-byte boundary.  devkitARM only promises byte alignment
 * for u8 arrays, so make that allocator precondition explicit here.
 */
BSS u8 heap_generalHead[GENERAL_HEAP_SIZE] ALIGNED(16);
BSS u8 heap_spriteHead[SPRITE_HEAP_SIZE] ALIGNED(16);
BSS u16 gFrameBuf0[FRAME_BUFFER_SIZE / 2];
BSS u16 gFrameBuf1[FRAME_BUFFER_SIZE / 2];
BSS u16 gFrameBuf2[FRAME_BUFFER_SIZE / 2];

#ifdef SHIFT
BSS u8 WorldEntityHeapBottom[WORLD_ENTITY_HEAP_SIZE];
#endif
BSS u8 WorldEntityHeapBase[0x10];
BSS u8 heap_collisionHead[COLLISION_HEAP_SIZE] ALIGNED(16);
BSS u8 heap_battleHead[BATTLE_HEAP_SIZE] ALIGNED(16);

b32 PB3DS_RuntimeHeapStorageAligned(void) {
    return (((uintptr_t) heap_generalHead & 0xFU) == 0U) &&
           (((uintptr_t) heap_spriteHead & 0xFU) == 0U) &&
           (((uintptr_t) heap_collisionHead & 0xFU) == 0U) &&
           (((uintptr_t) heap_battleHead & 0xFU) == 0U);
}
'''


CRASH_SLEEP = r'''void crash_screen_sleep(s32 ms) {
    PB3DS_RuntimeSleepMs(ms);
}'''


NUSYS_EFFECT_WRAPPERS = r'''extern struct EffectInstance* small_gold_sparkle_main(
    s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4);

void* fx_small_gold_sparkle(s32 arg0, f32 arg1, f32 arg2, f32 arg3,
                            f32 arg4, s32 arg5) {
    /* The ROM ABI carried a sixth argument which this effect never consumes. */
    (void) arg5;
    return small_gold_sparkle_main(arg0, arg1, arg2, arg3, arg4);
}
'''


NUSYS_RESIDENT_OVERLAYS = r'''extern NUPiOverlaySegment D_8007798C;
extern NUPiOverlaySegment PauseOverlaySegment;

void nuPiReadRomOverlay(NUPiOverlaySegment* seg) {
    /* Both accepted overlays are part of the linked M13 image.  There is no
     * ROM address space to copy from, so validate identity and retain their
     * already-resident code/data instead of silently accepting any segment. */
    if (seg == &D_8007798C || seg == &PauseOverlaySegment) {
        return;
    }
    is_debug_panic("unsupported ROM overlay requested");
}
'''


MAC_00_EXIT_TRIGGERS = r'''EvtScript N(EVS_BindExitTriggers) = {
    BindTrigger(Ref(N(EVS_ExitWalk_mac_01_0)), TRIGGER_FLOOR_ABOVE, COLLIDER_deilie, 1, 0)
    Return
    End
};'''


MAC_01_EXIT_TRIGGERS = r'''EvtScript N(EVS_BindExitTriggers) = {
    BindTrigger(Ref(N(EVS_ExitWalk_mac_00_1)), TRIGGER_FLOOR_ABOVE, COLLIDER_deiliw, 1, 0)
    Return
    End
};'''


MAP_NAMED_LEVELS = r'''typedef struct NamedLevel {
    const char* base;
    char suffix[16];
    char path[160];
} NamedLevel;

static NamedLevel sNamedLevels[512];
static s32 sNamedLevelCount = 0;

IMG_PTR port_tex_named_level(IMG_PTR raster, const char* suffix) {
    const char* base = (const char*) raster;
    s32 i;
    s32 written;

    ASSERT_MSG(base != NULL && suffix != NULL, "null map texture path");
    for (i = 0; i < sNamedLevelCount; i++) {
        if (sNamedLevels[i].base == base
            && strcmp(sNamedLevels[i].suffix, suffix) == 0) {
            return (IMG_PTR) sNamedLevels[i].path;
        }
    }

    ASSERT_MSG(sNamedLevelCount < (s32) ARRAY_COUNT(sNamedLevels),
               "map texture path cache full");
    if (sNamedLevelCount >= (s32) ARRAY_COUNT(sNamedLevels)) {
        return NULL;
    }
    written = snprintf(sNamedLevels[sNamedLevelCount].suffix,
                       sizeof(sNamedLevels[sNamedLevelCount].suffix),
                       "%s", suffix);
    ASSERT_MSG(written >= 0
                   && written < (s32) sizeof(sNamedLevels[0].suffix),
               "map texture suffix too long");
    written = snprintf(sNamedLevels[sNamedLevelCount].path,
                       sizeof(sNamedLevels[sNamedLevelCount].path),
                       "%s%s", base, suffix);
    ASSERT_MSG(written >= 0
                   && written < (s32) sizeof(sNamedLevels[0].path),
               "map texture path too long");
    if (written < 0 || written >= (s32) sizeof(sNamedLevels[0].path)) {
        return NULL;
    }
    sNamedLevels[sNamedLevelCount].base = base;
    return (IMG_PTR) sNamedLevels[sNamedLevelCount++].path;
}'''


MAP_TEXTURE_VALIDATION = r'''static u8 sPortTextureOwned[128];

static u32 port_expected_texture_type(u8 format, u8 depth) {
    if (format == G_IM_FMT_RGBA && depth == G_IM_SIZ_32b) {
        return PB_RESOURCE_TEXTURE_RGBA32;
    }
    if (format == G_IM_FMT_RGBA && depth == G_IM_SIZ_16b) {
        return PB_RESOURCE_TEXTURE_RGBA16;
    }
    if (format == G_IM_FMT_CI && depth == G_IM_SIZ_4b) {
        return PB_RESOURCE_TEXTURE_CI4;
    }
    if (format == G_IM_FMT_CI && depth == G_IM_SIZ_8b) {
        return PB_RESOURCE_TEXTURE_CI8;
    }
    if (format == G_IM_FMT_IA && depth == G_IM_SIZ_4b) {
        return PB_RESOURCE_TEXTURE_IA4;
    }
    if (format == G_IM_FMT_IA && depth == G_IM_SIZ_8b) {
        return PB_RESOURCE_TEXTURE_IA8;
    }
    if (format == G_IM_FMT_IA && depth == G_IM_SIZ_16b) {
        return PB_RESOURCE_TEXTURE_IA16;
    }
    if (format == G_IM_FMT_I && depth == G_IM_SIZ_4b) {
        return PB_RESOURCE_TEXTURE_I4;
    }
    if (format == G_IM_FMT_I && depth == G_IM_SIZ_8b) {
        return PB_RESOURCE_TEXTURE_I8;
    }
    return PB_RESOURCE_TEXTURE_ERROR;
}

static b32 port_require_texture(const char* path, u8 format, u8 depth,
                                u16 width, u16 height) {
    const u32 expected = port_expected_texture_type(format, depth);
    const b32 valid = path != NULL && expected != PB_RESOURCE_TEXTURE_ERROR
        && GameEngine_GetDataExact(path) != NULL
        && pb_runtime_resource_type(path) == 0x4F544558U
        && pb_runtime_resource_texture_type(path) == expected
        && GameEngine_GetTexWidthExact(path) == width
        && GameEngine_GetTexHeightExact(path) == height;
    if (!valid) {
        GameEngine_LogInfo("[maptex] invalid resource %s fmt=%u depth=%u %ux%u",
                           path != NULL ? path : "(null)", format, depth,
                           width, height);
        ASSERT_MSG(valid, "invalid map texture resource");
        return false;
    }
    return true;
}

static const char* port_require_level(const char* base, const char* suffix,
                                      u8 format, u8 depth,
                                      u16 width, u16 height) {
    const char* path = (const char*) port_tex_named_level(
        (IMG_PTR) base, suffix);
    if (!port_require_texture(path, format, depth, width, height)) {
        return NULL;
    }
    return path;
}

static s32 port_mip_level_count(const MapTexMeta* m) {
    const s32 bits = 4 << m->mainDepth;
    s32 divisor = 1;
    s32 levels = 0;
    while (m->mainW / divisor * bits >= 64
           && m->mainH / divisor != 0) {
        levels++;
        divisor *= 2;
    }
    return levels;
}

static b32 port_validate_texture_meta(const MapTexMeta* m) {
    s32 lod;
    char suffix[16];

    ASSERT_MSG(m != NULL && m->mainW != 0 && m->mainH != 0,
               "invalid map texture metadata");
    ASSERT_MSG(m->extraTiles <= EXTRA_TILE_AUX_INDEPENDENT,
               "unsupported map texture mode");
    ASSERT_MSG((m->extraTiles == EXTRA_TILE_MIPMAPS) == !!m->hasMipmaps,
               "map mip metadata mismatch");
    if (!port_require_texture(m->otrPath, m->mainFmt, m->mainDepth,
                              m->mainW, m->mainH)) {
        return false;
    }
    if (m->mainFmt == G_IM_FMT_CI
        && port_require_level(m->otrPath, "_tlut", G_IM_FMT_RGBA,
                              G_IM_SIZ_16b,
                              m->mainDepth == G_IM_SIZ_4b ? 16 : 256,
                              1) == NULL) {
        return false;
    }
    if (m->extraTiles == EXTRA_TILE_AUX_SAME_AS_MAIN
        && port_require_level(m->otrPath, "_aux", m->mainFmt,
                              m->mainDepth, m->mainW,
                              m->mainH / 2) == NULL) {
        return false;
    }
    if (m->extraTiles == EXTRA_TILE_AUX_INDEPENDENT) {
        if (!port_require_texture(m->auxOtrPath, m->auxFmt, m->auxDepth,
                                  m->auxW, m->auxH)) {
            return false;
        }
        if (m->auxFmt == G_IM_FMT_CI
            && port_require_level(m->otrPath, "_aux_tlut", G_IM_FMT_RGBA,
                                  G_IM_SIZ_16b,
                                  m->auxDepth == G_IM_SIZ_4b ? 16 : 256,
                                  1) == NULL) {
            return false;
        }
    }
    if (m->extraTiles == EXTRA_TILE_MIPMAPS) {
        const s32 levels = port_mip_level_count(m);
        ASSERT_MSG(levels > 0 && levels <= 8, "invalid map mip count");
        for (lod = 1; lod < levels; lod++) {
            snprintf(suffix, sizeof(suffix), "_mm%d", lod);
            if (port_require_level(m->otrPath, suffix, m->mainFmt,
                                   m->mainDepth, m->mainW >> lod,
                                   m->mainH >> lod) == NULL) {
                return false;
            }
        }
    }
    return true;
}

void port_release_map_textures(void) {
    u32 i;
    for (i = 0; i < ARRAY_COUNT(TextureHandles); i++) {
        TextureHandle* handle = &TextureHandles[i];
        if (handle->gfx != NULL && !sPortTextureOwned[i]) {
            GameEngine_LogInfo("[maptex] foreign TextureHandles[%u]", i);
            ASSERT_MSG(false, "foreign map texture handle");
        }
        if (sPortTextureOwned[i] && handle->gfx != NULL) {
            free(handle->gfx);
        }
        memset(handle, 0, sizeof(*handle));
        sPortTextureOwned[i] = false;
    }
    sNamedLevelCount = 0;
}'''


MAP_LOAD_ONE = r'''static void port_load_one_texture(const char* archive, const MapTexMeta* meta, s32 metaIdx, s32 textureID) {
    TextureHandle* handle;
    const MapTexMeta* m;
    TextureHeader header;
    Gfx* gfxCursor;
    IMG_PTR raster;
    PAL_PTR palette;
    IMG_PTR auxRaster;
    PAL_PTR auxPalette;
    b32 mainIsCI;
    b32 auxIsCI;

    (void) archive;
    ASSERT_MSG(meta != NULL && metaIdx >= 0,
               "invalid map texture index");
    ASSERT_MSG(textureID > 0
                   && textureID < (s32) ARRAY_COUNT(TextureHandles),
               "map texture handle overflow");
    handle = &TextureHandles[textureID];
    m = &meta[metaIdx];
    ASSERT_MSG(handle->gfx == NULL && !sPortTextureOwned[textureID],
               "map texture handle reused");
    ASSERT_MSG(port_validate_texture_meta(m),
               "map texture validation failed");

    port_build_texture_header(&header, m);
    mainIsCI = (m->mainFmt == G_IM_FMT_CI);
    auxIsCI = (m->auxFmt == G_IM_FMT_CI);
    raster = (IMG_PTR) m->otrPath;
    palette = mainIsCI
        ? (PAL_PTR) port_tex_named_level(raster, "_tlut") : NULL;
    if (m->extraTiles == EXTRA_TILE_AUX_INDEPENDENT) {
        auxRaster = (IMG_PTR) m->auxOtrPath;
        auxPalette = auxIsCI
            ? (PAL_PTR) port_tex_named_level(raster, "_aux_tlut") : NULL;
    } else {
        auxRaster = NULL;
        auxPalette = NULL;
    }

    handle->raster = raster;
    handle->palette = palette;
    handle->auxRaster = auxRaster;
    handle->auxPalette = auxPalette;
    handle->combinedPalette = NULL;
    handle->gfx = (Gfx*) malloc(MAX_TEXTURE_GFX_CMDS * sizeof(Gfx));
    ASSERT_MSG(handle->gfx != NULL, "out of map texture gfx memory");
    if (handle->gfx == NULL) {
        return;
    }
    sPortTextureOwned[textureID] = true;
    gfxCursor = handle->gfx;
    memcpy(&handle->header, &header, sizeof(header));

    make_texture_gfx(
        &header, &gfxCursor, handle->raster, handle->palette,
        handle->auxRaster, handle->auxPalette, 0, 0, 0, 0,
        handle->combinedPalette
    );
    ASSERT_MSG(gfxCursor >= handle->gfx
                   && gfxCursor - handle->gfx < MAX_TEXTURE_GFX_CMDS,
               "map texture gfx overflow");
    gSPEndDisplayList(gfxCursor++);
}'''


MAP_LOAD_FOR_NODE = r'''static void
port_load_texture_for_node(const char* archive, const MapTexMeta* meta, u32 count, ModelNodeProperty* propTextureName) {
    const char* textureName = (const char*) propTextureName->data.p;
    s32 idx;
    s32 textureID;

    if (textureName == NULL) {
        (*gCurrentModelTreeNodeInfo)[TreeIterPos].textureID = 0;
        return;
    }

    idx = -1;
    {
        u32 i;
        for (i = 0; i < count; i++) {
            if (strcmp(textureName, meta[i].name) == 0) {
                idx = (s32) i;
                break;
            }
        }
    }

    if (idx < 0) {
        GameEngine_LogInfo("[maptex] no metadata arc=%s name=%s",
                           archive, textureName);
        ASSERT_MSG(idx >= 0, "map texture metadata missing");
        (*gCurrentModelTreeNodeInfo)[TreeIterPos].textureID = 0;
        return;
    }

    textureID = idx + 1;
    ASSERT_MSG(textureID < (s32) ARRAY_COUNT(TextureHandles),
               "map texture handle overflow");
    (*gCurrentModelTreeNodeInfo)[TreeIterPos].textureID = textureID;

    if (TextureHandles[textureID].gfx == NULL) {
        s32 j;
        port_load_one_texture(archive, meta, idx, textureID);
        for (j = idx + 1; j < (s32) count; j++) {
            if (!meta[j].isVariant) {
                break;
            }
            port_load_one_texture(archive, meta, j, j + 1);
        }
    } else {
        ASSERT_MSG(sPortTextureOwned[textureID],
                   "unowned map texture handle");
    }
}'''


MAP_LOAD_ALL = r'''void port_load_map_textures(ModelNode* rootModel, const char* archiveName) {
    const MapTexArchive* arc = NULL;
    u32 i;

    ASSERT_MSG(rootModel != NULL && archiveName != NULL,
               "invalid map texture load");
    if (rootModel == NULL || archiveName == NULL) {
        return;
    }

    for (i = 0; i < gMapTexArchiveCount; i++) {
        if (strcmp(gMapTexArchives[i].archive, archiveName) == 0) {
            arc = &gMapTexArchives[i];
            break;
        }
    }

    if (arc == NULL) {
        GameEngine_LogInfo("[maptex] no metadata for archive %s", archiveName);
        ASSERT_MSG(arc != NULL, "map texture archive missing");
        return;
    }
    ASSERT_MSG(arc->count < ARRAY_COUNT(TextureHandles),
               "map texture archive too large");

    port_release_map_textures();
    TreeIterPos = 0;
    port_load_next_model_textures(arc->archive, arc->textures, arc->count,
                                  rootModel);
}'''


BACKGROUND_RESOURCE_LOADER = r'''static const char* sBgRasterPath = NULL;
static const char* sBgPalettePath = NULL;

static const char* port_require_background_texture(
    const char* path, u32 textureType, u16 width, u16 height
) {
    const char* stablePath;
    const b32 valid = path != NULL
        && GameEngine_GetDataExact(path) != NULL
        && pb_runtime_resource_type(path) == 0x4F544558U
        && pb_runtime_resource_texture_type(path) == textureType
        && GameEngine_GetTexWidthExact(path) == width
        && GameEngine_GetTexHeightExact(path) == height;

    if (!valid) {
        GameEngine_LogInfo("[background] invalid resource %s type=%u %ux%u",
                           path != NULL ? path : "(null)", textureType,
                           width, height);
        ASSERT_MSG(valid, "invalid background resource");
        return NULL;
    }
    stablePath = pb_runtime_resource_otr_name(path);
    ASSERT_MSG(stablePath != NULL, "background path ownership failed");
    return stablePath;
}

void port_release_background_resource(void) {
    sBgRasterPath = NULL;
    sBgPalettePath = NULL;
    memset(&gBackgroundImage, 0, sizeof(gBackgroundImage));
}

void port_load_map_bg(char* optAssetName) {
    char rasterPath[64];
    char palettePath[64];
    char* assetName;
    s32 rasterLength;
    s32 paletteLength;

    ASSERT_MSG(optAssetName != NULL, "null background name");
    if (optAssetName == NULL) {
        return;
    }
    assetName = optAssetName;

    if (evt_get_variable(NULL, GB_StoryProgress)
            >= STORY_CH6_DESTROYED_PUFF_PUFF_MACHINE
        && strcmp(assetName, gCloudyFlowerFieldsBg) == 0) {
        assetName = gSunnyFlowerFieldsBg;
    }

    rasterLength = snprintf(rasterPath, sizeof(rasterPath),
                            "__OTR__backgrounds/%s", assetName);
    paletteLength = snprintf(palettePath, sizeof(palettePath),
                             "__OTR__backgrounds/%s_pal0", assetName);
    ASSERT_MSG(rasterLength >= 0
                   && rasterLength < (s32) sizeof(rasterPath)
                   && paletteLength >= 0
                   && paletteLength < (s32) sizeof(palettePath),
               "background resource path too long");
    if (rasterLength < 0 || rasterLength >= (s32) sizeof(rasterPath)
        || paletteLength < 0
        || paletteLength >= (s32) sizeof(palettePath)) {
        return;
    }

    sBgRasterPath = port_require_background_texture(
        rasterPath, PB_RESOURCE_TEXTURE_CI8, 296, 200);
    sBgPalettePath = port_require_background_texture(
        palettePath, PB_RESOURCE_TEXTURE_RGBA16, 256, 1);
    if (sBgRasterPath == NULL || sBgPalettePath == NULL) {
        port_release_background_resource();
        return;
    }

    gBackgroundImage.raster = (IMG_PTR) sBgRasterPath;
    gBackgroundImage.palette =
        (PAL_PTR) GameEngine_GetDataExact(sBgPalettePath);
    ASSERT_MSG(gBackgroundImage.palette != NULL,
               "background palette data unavailable");
    gBackgroundImage.width = 296;
    gBackgroundImage.height = 200;
    gBackgroundImage.startX = 12;
    gBackgroundImage.startY = 20;
}'''


MESSAGE_RESOURCE_RESOLUTION = r'''typedef struct GlyphSheet {
    const u8* base;
    u32 size;
    u32 glyphBytes;
    u16 width;
    u16 height;
    const char* prefix;
    b32 hasExactCompanions;
} GlyphSheet;

static GlyphSheet sSheets[] = {
    { (const u8*) MsgCharImgNormal, 0x5100, 128, 16, 16,
      "__OTR__charset/charset_standard_", true },
    /* The pinned Torch exporter advances title companions by 12*15/2 bytes,
     * while Paper Mario's source sheet advances each glyph by 96 bytes.  The
     * full required OBLB therefore remains the only exact title source. */
    { (const u8*) MsgCharImgTitle, 0xF60, 96, 12, 15,
      "__OTR__charset/charset_title_", false },
    { (const u8*) MsgCharImgSubtitle, 0xB88, 72, 12, 12,
      "__OTR__charset/charset_subtitle_", true },
};

static const char* sPalettePrefix =
    "__OTR__charset/charset_standard_palette_";

static const char* port_optional_message_texture(
    const char* prefix, u32 index, u32 textureType, u16 width, u16 height
) {
    char path[96];
    s32 length = snprintf(path, sizeof(path), "%s%u", prefix, index);
    const char* stablePath;
    b32 valid;

    ASSERT_MSG(length >= 0 && length < (s32) sizeof(path),
               "message resource path too long");
    if (length < 0 || length >= (s32) sizeof(path)
        || !pb_runtime_resource_exists(path)) {
        /* Split textures are an optional acceleration.  The required OBLB
         * was copied into the upstream sheet and is the authentic fallback. */
        return NULL;
    }
    valid = GameEngine_GetDataExact(path) != NULL
        && pb_runtime_resource_type(path) == 0x4F544558U
        && pb_runtime_resource_texture_type(path) == textureType
        && GameEngine_GetTexWidthExact(path) == width
        && GameEngine_GetTexHeightExact(path) == height;
    if (!valid) {
        GameEngine_LogInfo("[message] malformed companion %s", path);
        ASSERT_MSG(valid, "invalid message texture companion");
        return NULL;
    }
    stablePath = pb_runtime_resource_otr_name(path);
    ASSERT_MSG(stablePath != NULL, "message path ownership failed");
    return stablePath;
}

void port_msg_font_loaded(s32 font) {
    sPalettePrefix = font == 1
        ? "__OTR__charset/charset_subtitle_palette_"
        : "__OTR__charset/charset_standard_palette_";
}

IMG_PTR port_msg_glyph_raster(IMG_PTR glyph) {
    const uintptr_t address = (uintptr_t) glyph;
    s32 i;

    for (i = 0; i < (s32) ARRAY_COUNT(sSheets); i++) {
        GlyphSheet* sheet = &sSheets[i];
        const uintptr_t base = (uintptr_t) sheet->base;
        u32 offset;
        u32 index;
        const char* path;

        if (address < base || address >= base + sheet->size) {
            continue;
        }
        offset = (u32) (address - base);
        if (offset % sheet->glyphBytes != 0 || !sheet->hasExactCompanions) {
            return glyph;
        }
        index = offset / sheet->glyphBytes;
        path = port_optional_message_texture(
            sheet->prefix, index, PB_RESOURCE_TEXTURE_CI4,
            sheet->width, sheet->height);
        return path != NULL ? (IMG_PTR) path : glyph;
    }
    return glyph;
}

PAL_PTR port_msg_glyph_palette(PAL_PTR palette) {
    const uintptr_t address = (uintptr_t) palette;
    const uintptr_t base = (uintptr_t) D_802F4560;
    const u32 stride = sizeof(D_802F4560[0]);
    u32 offset;
    u32 index;
    const char* path;

    if (address < base || address >= base + sizeof(D_802F4560)) {
        return palette;
    }
    offset = (u32) (address - base);
    if (offset % stride != 0) {
        return palette;
    }
    index = offset / stride;
    path = port_optional_message_texture(
        sPalettePrefix, index, PB_RESOURCE_TEXTURE_RGBA16, 16, 1);
    return path != NULL ? (PAL_PTR) path : palette;
}'''


def replace_script(source: str, script_name: str, replacement: str) -> str:
    """Replace one pinned EVT script without accepting an upstream drift."""
    marker = f"EvtScript N({script_name}) = {{"
    start = source.find(marker)
    if start < 0 or source.find(marker, start + 1) >= 0:
        raise SystemExit(f"pinned script marker changed: {script_name}")
    end_marker = "\n};"
    end = source.find(end_marker, start)
    if end < 0:
        raise SystemExit(f"pinned script terminator changed: {script_name}")
    end += len(end_marker)
    return source[:start] + replacement + source[end:]


def replace_region(source: str, start_marker: str, end_marker: str,
                   replacement: str, label: str) -> str:
    """Replace a pinned source region while retaining its end marker."""
    start = source.find(start_marker)
    if start < 0 or source.find(start_marker, start + 1) >= 0:
        raise SystemExit(f"pinned {label} start changed")
    end = source.find(end_marker, start + len(start_marker))
    if end < 0:
        raise SystemExit(f"pinned {label} end changed")
    return source[:start] + replacement.rstrip() + "\n\n" + source[end:]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("upstream", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    world_source = args.upstream / "src/world/world.c"
    source = world_source.read_text(encoding="utf-8")
    rom_heap_setup = r'''#if !VERSION_IQUE
    load_obfuscation_shims();
#endif
    shim_general_heap_create_obfuscated();'''
    if source.count(rom_heap_setup) != 2:
        raise SystemExit("pinned world ROM/heap setup changed")
    source = source.replace(
        rom_heap_setup,
        "    /* The O2R-backed 3DS image already links these routines; the "
        "original ROM obfuscation copy has no valid source range here. */\n"
        "    general_heap_create();",
    )
    marker = "#define AREA(area, jp_name)"
    if source.count(marker) != 1:
        raise SystemExit("pinned world.c registry marker changed")

    nusys_source = args.upstream / "src/port/nusys_overrides.c"
    nusys = nusys_source.read_text(encoding="utf-8")
    for upstream_signature, platform_signature in NUSYS_ABI_SIGNATURES.items():
        if nusys.count(upstream_signature) != 1:
            raise SystemExit(
                f"pinned nusys_overrides.c signature changed: {upstream_signature}"
            )
        nusys = nusys.replace(upstream_signature, platform_signature)

    include_marker = '#include "nu/nusys.h"\n'
    if nusys.count(include_marker) != 1:
        raise SystemExit("pinned nusys include block changed")
    nusys = nusys.replace(
        include_marker,
        include_marker + '#include "pb3ds/runtime_flash.h"\n',
    )

    pi_read = r'''void nuPiReadRom(u32 romAddr, void* ramAddr, u32 len) {
    return;
}
'''
    if nusys.count(pi_read) != 1:
        raise SystemExit("pinned nusys ROM read implementation changed")
    nusys = nusys.replace(pi_read, NUSYS_PI_READ)

    nusys = replace_region(
        nusys,
        "u8 nuContInit(void) {",
        "// Static pad buffer shared across nuContDataGet/nuContDataGetAll",
        NUSYS_CONTROLLER_SERVICES,
        "nusys controller services",
    )

    flash_state = r'''// Flash emulation state
#define FLASH_PAGE_BYTES 128
#define FLASH_TOTAL_SIZE 0x20000 // 128KB, matches N64 Flash chip
static u8 sFlashWriteBuf[FLASH_PAGE_BYTES];
static char sSaveFilePath[512];
static s32 sSaveFilePathValid = 0;
'''
    flash_erase = r'''s32 osFlashSectorErase(u32 page_num) {
    (void) page_num;
    // No-op: game always writes immediately after erase, and
    // osFlashWriteArray does a full read-modify-write.
    return 0;
}
'''
    flash_read = r'''s32 osFlashReadArray(OSIoMesg* mb, s32 priority, u32 page_num, void* dramAddr, u32 n_pages, OSMesgQueue* mq) {
    (void) mb;
    (void) priority;
    (void) mq;

    flash_ensure_path();

    u32 offset = page_num * FLASH_PAGE_BYTES;
    u32 size = n_pages * FLASH_PAGE_BYTES;

    // Pre-fill with zeros (like a blank Flash chip)
    memset(dramAddr, 0, size);

    if (!sSaveFilePathValid) {
        return 0;
    }

    FILE* fp = fopen(sSaveFilePath, "rb");
    if (fp == NULL) {
        return 0;
    }

    fseek(fp, offset, SEEK_SET);
    fread(dramAddr, 1, size, fp);
    fclose(fp);
    return 0;
}
'''
    flash_buffer = r'''s32 osFlashWriteBuffer(OSIoMesg* mb, s32 priority, void* dramAddr, OSMesgQueue* mq) {
    (void) mb;
    (void) priority;
    (void) mq;

    memcpy(sFlashWriteBuf, dramAddr, FLASH_PAGE_BYTES);
    return 0;
}
'''
    flash_write = r'''s32 osFlashWriteArray(u32 page_num) {
    flash_ensure_path();
    if (!sSaveFilePathValid) {
        return -1;
    }

    u32 offset = page_num * FLASH_PAGE_BYTES;
    if (offset + FLASH_PAGE_BYTES > FLASH_TOTAL_SIZE) {
        return -1;
    }

    // Read existing save file (or start from zeros)
    u8 flash[FLASH_TOTAL_SIZE];
    memset(flash, 0, sizeof(flash));

    FILE* fp = fopen(sSaveFilePath, "rb");
    if (fp != NULL) {
        fread(flash, 1, FLASH_TOTAL_SIZE, fp);
        fclose(fp);
    }

    // Patch the page with the write buffer contents
    memcpy(flash + offset, sFlashWriteBuf, FLASH_PAGE_BYTES);

    // Write back the full file
    fp = fopen(sSaveFilePath, "wb");
    if (fp == NULL) {
        return -1;
    }
    fwrite(flash, 1, FLASH_TOTAL_SIZE, fp);
    fclose(fp);
    return 0;
}
'''
    flash_replacements = (
        (flash_state, NUSYS_FLASH_STATE),
        (flash_erase, NUSYS_FLASH_ERASE),
        (flash_read, NUSYS_FLASH_READ),
        (flash_buffer, NUSYS_FLASH_BUFFER),
        (flash_write, NUSYS_FLASH_WRITE),
    )
    for upstream_block, replacement in flash_replacements:
        if nusys.count(upstream_block) != 1:
            raise SystemExit("pinned nusys flash implementation changed")
        nusys = nusys.replace(upstream_block, replacement)

    flash_init = r'''OSPiHandle* osFlashInit(void) {
    flash_ensure_path();
    return NULL;
}
'''
    if nusys.count(flash_init) != 1:
        raise SystemExit("pinned nusys flash init changed")
    nusys = nusys.replace(flash_init, NUSYS_FLASH_INIT)

    pfs_init = r'''s32 osPfsInitPak(OSMesgQueue* mq, OSPfs* pfs, int channel) {
    (void) mq;
    (void) pfs;
    (void) channel;
    return 0;
}
'''
    if nusys.count(pfs_init) != 1:
        raise SystemExit("pinned nusys PFS init changed")
    nusys = nusys.replace(pfs_init, NUSYS_PFS_INIT)

    cont_query = r'''void osContGetQuery(OSContStatus* data) {
    (void) data;
}
'''
    cont_query_3ds = r'''void osContGetQuery(OSContStatus* data) {
    if (data == NULL) return;
    data->type = CONT_TYPE_NORMAL;
    data->status = 0;
    data->err_no = 0;
}
'''
    if nusys.count(cont_query) != 1:
        raise SystemExit("pinned nusys controller query changed")
    nusys = nusys.replace(cont_query, cont_query_3ds)

    pfs_tail = "s32 osPfsRepairId(OSPfs* pfs) {"
    if nusys.count(pfs_tail) != 1 or nusys.count("s32 osPfsNumFiles(") != 1:
        raise SystemExit("pinned nusys PFS tail changed")
    pfs_tail_start = nusys.index(pfs_tail)
    if nusys[pfs_tail_start:].count("return 0;") != 8:
        raise SystemExit("pinned nusys PFS result contract changed")
    nusys = nusys[:pfs_tail_start] + NUSYS_PFS_ABSENT

    effect_stubs = r'''void* fx_small_gold_sparkle(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

void* fx_sun_undeclared(s32 arg0, f32 arg1, f32 arg2, f32 arg3, s32 arg4) {
    return NULL;
}
'''
    if nusys.count(effect_stubs) != 1:
        raise SystemExit("pinned nusys effect stubs changed")
    nusys = nusys.replace(effect_stubs, NUSYS_EFFECT_WRAPPERS)

    overlay_stub = r'''void nuPiReadRomOverlay(NUPiOverlaySegment* seg) {
    // No-op - overlays loaded via OTR
}
'''
    if nusys.count(overlay_stub) != 1:
        raise SystemExit("pinned nusys overlay stub changed")
    nusys = nusys.replace(overlay_stub, NUSYS_RESIDENT_OVERLAYS)

    # These definitions are replaced as one unit so a future upstream storage
    # change cannot silently bypass the 3DS allocator-alignment contract.
    heaps3 = (args.upstream / "src/heaps3.c").read_text(encoding="utf-8")
    heaps2 = (args.upstream / "src/heaps2.c").read_text(encoding="utf-8")
    heap_contract = {
        "heap_generalHead": "BSS u8 heap_generalHead[GENERAL_HEAP_SIZE];",
        "heap_spriteHead": "BSS u8 heap_spriteHead[SPRITE_HEAP_SIZE];",
        "heap_collisionHead": "BSS u8 heap_collisionHead[COLLISION_HEAP_SIZE];",
        "heap_battleHead": "BSS u8 heap_battleHead[BATTLE_HEAP_SIZE];",
    }
    for symbol, declaration in heap_contract.items():
        heap_source = (
            heaps3
            if symbol in {"heap_generalHead", "heap_spriteHead"}
            else heaps2
        )
        if heap_source.count(declaration) != 1:
            raise SystemExit(
                f"pinned heap storage declaration changed: {declaration}"
            )

    # M13 accepts only mac_00 and mac_01.  Keep their authentic shared exit,
    # but do not bind triggers into chapter maps that are intentionally absent
    # from the generated registry.  Leaving those triggers live turns a normal
    # step onto mac_00's sewer pipe into an upstream "Map not found" panic.
    mac_00_main = (
        args.upstream / "src/world/area_mac/mac_00/main.c"
    ).read_text(encoding="utf-8")
    mac_01_main = (
        args.upstream / "src/world/area_mac/mac_01/main.c"
    ).read_text(encoding="utf-8")
    mac_00_main = replace_script(
        mac_00_main, "EVS_BindExitTriggers", MAC_00_EXIT_TRIGGERS
    )
    mac_01_main = replace_script(
        mac_01_main, "EVS_BindExitTriggers", MAC_01_EXIT_TRIGGERS
    )

    title_screen = (
        args.upstream / "src/state_title_screen.c"
    ).read_text(encoding="utf-8")
    title_timeout = "    TitleScreen_TimeLeft = 480;"
    if title_screen.count(title_timeout) != 1:
        raise SystemExit("pinned title timeout changed")
    # M13 does not link the intro/demo maps. Keep the authentic title in its
    # input state until the player opens the authentic file menu.
    title_screen = title_screen.replace(
        title_timeout, "    TitleScreen_TimeLeft = 32767;"
    )

    sprite_loader = (
        args.upstream / "src/101b90_len_8f0.c"
    ).read_text(encoding="utf-8")
    sprite_load_contract = (
        (
            "        Sprite_LoadPlayer(idx, animData, spriteSize);",
            "        ASSERT_MSG(Sprite_LoadPlayer(idx, animData, spriteSize) == animData,\n"
            "                   \"player sprite conversion failed\");",
        ),
        (
            "        Sprite_LoadNPC(idx, animData, spriteSize);",
            "        ASSERT_MSG(Sprite_LoadNPC(idx, animData, spriteSize) == animData,\n"
            "                   \"NPC sprite conversion failed\");",
        ),
    )
    for original, checked in sprite_load_contract:
        if sprite_loader.count(original) != 1:
            raise SystemExit(f"pinned sprite load call changed: {original}")
        sprite_loader = sprite_loader.replace(original, checked)

    filemenu_yesno = (
        args.upstream / "src/filemenu/filemenu_yesno.c"
    ).read_text(encoding="utf-8")
    new_game_lookup = (
        "                        get_map_IDs_by_name_checked(NEW_GAME_MAP_ID, "
        "&gGameStatusPtr->areaID, &gGameStatusPtr->mapID);"
    )
    if filemenu_yesno.count(new_game_lookup) != 1:
        raise SystemExit("pinned new-game map lookup changed")
    # The M13 registry deliberately contains only mac_00/mac_01. Resolve a
    # newly created slot to the same real mac_00 destination used by the
    # end-file-select transition; otherwise kmr_20 panics before that
    # transition can apply the accepted route.
    filemenu_yesno = filemenu_yesno.replace(
        new_game_lookup,
        "                        get_map_IDs_by_name_checked(\"mac_00\", "
        "&gGameStatusPtr->areaID, &gGameStatusPtr->mapID);",
    )

    map_texture = (
        args.upstream / "src/port/patches/MapTexturePatches.c"
    ).read_text(encoding="utf-8")
    map_include = '#include "port/patches/Patches.h"\n'
    if map_texture.count(map_include) != 1:
        raise SystemExit("pinned map texture include block changed")
    map_texture = map_texture.replace(
        map_include,
        map_include
        + '#include "pb3ds/runtime_resources.h"\n'
        + '#include "pb3ds/texture.h"\n'
        + '#include <stdlib.h>\n',
    )
    map_gfx_limit = "#define MAX_TEXTURE_GFX_CMDS 64"
    if map_texture.count(map_gfx_limit) != 1:
        raise SystemExit("pinned map texture Gfx limit changed")
    map_texture = map_texture.replace(
        map_gfx_limit, "#define MAX_TEXTURE_GFX_CMDS 256"
    )
    map_texture = replace_region(
        map_texture,
        "static void* port_get_tex_resource(",
        "IMG_PTR port_named_image(",
        MAP_NAMED_LEVELS,
        "map texture named-level cache",
    )
    validation_marker = "// Load one texture (resolved via MapTexMeta index `metaIdx`) into\n"
    if map_texture.count(validation_marker) != 1:
        raise SystemExit("pinned map texture validation marker changed")
    map_texture = map_texture.replace(
        validation_marker,
        MAP_TEXTURE_VALIDATION.rstrip() + "\n\n" + validation_marker,
    )
    map_texture = replace_region(
        map_texture,
        "static void port_load_one_texture(",
        "// Reimplements load_texture_by_name + load_texture_variants",
        MAP_LOAD_ONE,
        "map texture single loader",
    )
    map_texture = replace_region(
        map_texture,
        "static void\nport_load_texture_for_node(",
        "// Reimplements load_next_model_textures",
        MAP_LOAD_FOR_NODE,
        "map texture node loader",
    )
    map_load_all = "void port_load_map_textures(ModelNode* rootModel, const char* archiveName) {"
    if map_texture.count(map_load_all) != 1:
        raise SystemExit("pinned map texture archive loader changed")
    map_texture = map_texture[:map_texture.index(map_load_all)] + MAP_LOAD_ALL + "\n"

    background = (
        args.upstream / "src/port/patches/BackgroundPatches.c"
    ).read_text(encoding="utf-8")
    background_include = '#include "port/patches/Patches.h"\n'
    if background.count(background_include) != 1:
        raise SystemExit("pinned background patch include block changed")
    background = background.replace(
        background_include,
        background_include
        + '#include "pb3ds/runtime_resources.h"\n'
        + '#include "pb3ds/texture.h"\n',
    )
    background = replace_region(
        background,
        "#define MAX_BG_PATHS 64",
        "void port_appendGfx_background_texture(void) {",
        BACKGROUND_RESOURCE_LOADER,
        "background resource loader",
    )

    message = (
        args.upstream / "src/port/patches/MessagePatches.c"
    ).read_text(encoding="utf-8")
    message_include = '#include "port/Engine.h"\n'
    if message.count(message_include) != 1:
        raise SystemExit("pinned message patch include block changed")
    message = message.replace(
        message_include,
        message_include
        + '#include "pb3ds/runtime_resources.h"\n'
        + '#include "pb3ds/texture.h"\n'
        + '#include <stdint.h>\n',
    )
    message_marker = "typedef struct GlyphSheet {"
    if message.count(message_marker) != 1:
        raise SystemExit("pinned message resource resolver changed")
    message = (
        message[:message.index(message_marker)]
        + MESSAGE_RESOURCE_RESOLUTION
        + "\n"
    )

    shape_loader = (
        args.upstream / "src/port/shape_loader.c"
    ).read_text(encoding="utf-8")
    shape_include = '#include "shape_loader.h"\n'
    if shape_loader.count(shape_include) != 1:
        raise SystemExit("pinned shape loader include block changed")
    shape_loader = shape_loader.replace(
        shape_include,
        shape_include + '#include "pb3ds/world_boot.h"\n',
    )
    shape_alloc_old = r'''static void* shape_alloc(size_t size) {
    size = (size + 15) & ~15; // align to 16 bytes
    if (sArenaPos + size > sArenaSize) {
        GameEngine_LogInfo("[Shape] Arena exhausted! pos=%zu, need=%zu, limit=%zu", sArenaPos, size, sArenaSize);
        return NULL;
    }
    void* ptr = sArenaBase + sArenaPos;
    sArenaPos += size;
    return ptr;
}'''
    shape_alloc_new = r'''static void* shape_alloc(size_t size) {
    void* ptr;
    ASSERT_MSG(size <= SIZE_MAX - 15, "shape allocation overflow");
    size = (size + 15) & ~(size_t) 15;
    if (sArenaBase == NULL || sArenaPos > sArenaSize
        || size > sArenaSize - sArenaPos) {
        GameEngine_LogInfo(
            "[Shape] arena exhausted pos=%zu need=%zu limit=%zu",
            sArenaPos, size, sArenaSize);
        ASSERT_MSG(false, "shape native arena exhausted");
        return NULL;
    }
    ptr = sArenaBase + sArenaPos;
    sArenaPos += size;
    return ptr;
}'''
    if shape_loader.count(shape_alloc_old) != 1:
        raise SystemExit("pinned shape allocator changed")
    shape_loader = shape_loader.replace(shape_alloc_old, shape_alloc_new)
    shape_dl_old = r'''    char path[128];
    snprintf(path, sizeof(path), "__OTR__shapes/%s/dlist_%X", gCurrentShapeName, dlOffset);

    Gfx* dl = (Gfx*) ResourceGetDataByName(path);
    if (dl == NULL) {
        GameEngine_LogInfo("[Shape] WARNING: Could not load display list from %s", path);
    }
    return dl;'''
    shape_dl_new = r'''    char path[128];
    const s32 length = snprintf(
        path, sizeof(path), "__OTR__shapes/%s/dlist_%X",
        gCurrentShapeName, dlOffset);
    ASSERT_MSG(length >= 0 && length < (s32) sizeof(path),
               "shape display-list path too long");
    if (length < 0 || length >= (s32) sizeof(path)) {
        return NULL;
    }

    Gfx* dl = (Gfx*) ResourceGetDataByName(path);
    if (dl == NULL) {
        GameEngine_LogInfo("[Shape] missing display list %s", path);
        ASSERT_MSG(dl != NULL, "shape display list unavailable");
    }
    return dl;'''
    if shape_loader.count(shape_dl_old) != 1:
        raise SystemExit("pinned shape display-list resolver changed")
    shape_loader = shape_loader.replace(shape_dl_old, shape_dl_new)
    shape_load_start = r'''void Shape_LoadFromRawData(ShapeFile* shapeFile, const u8* rawData, size_t rawSize, const char* shapeName) {
    if (rawData == NULL || rawSize == 0) {
        shapeFile->header.root = NULL;
        shapeFile->header.vertexTable = NULL;
        shapeFile->header.modelNames = NULL;
        shapeFile->header.colliderNames = NULL;
        shapeFile->header.zoneNames = NULL;
        return;
    }

    (void) rawSize;
    u8* base = (u8*) rawData;
    RawShapeFileHeader* rawHeader = (RawShapeFileHeader*) base;

    shape_arena_init(shapeFile->data, sizeof(shapeFile->data));'''
    shape_load_safe = r'''void Shape_LoadFromRawData(ShapeFile* shapeFile, const u8* rawData, size_t rawSize, const char* shapeName) {
    PBWorldBootStats validation;
    u8* base;
    RawShapeFileHeader* rawHeader;
    const b32 validArguments = shapeFile != NULL && rawData != NULL
        && rawSize != 0 && shapeName != NULL;
    b32 validPayload;

    memset(&validation, 0, sizeof(validation));
    validPayload = validArguments
        && pb_world_validate_shape_payload(rawData, rawSize, &validation);
    ASSERT_MSG(validPayload,
               "invalid shape payload reached loader");
    if (!validPayload) {
        if (shapeFile != NULL) {
            memset(&shapeFile->header, 0, sizeof(shapeFile->header));
        }
        return;
    }

    memset(shapeFile, 0, sizeof(*shapeFile));
    base = (u8*) rawData;
    rawHeader = (RawShapeFileHeader*) base;
    shape_arena_init(shapeFile->data, sizeof(shapeFile->data));'''
    if shape_loader.count(shape_load_start) != 1:
        raise SystemExit("pinned shape loader entry changed")
    shape_loader = shape_loader.replace(shape_load_start, shape_load_safe)

    crash_screen = (
        args.upstream / "src/crash_screen.c"
    ).read_text(encoding="utf-8")
    crash_sleep_start = "void crash_screen_sleep(s32 ms) {"
    crash_sleep_end = "\n}\n\nvoid crash_screen_draw_rect"
    start = crash_screen.find(crash_sleep_start)
    end = crash_screen.find(crash_sleep_end, start)
    if start < 0 or end < 0 or crash_screen.find(
        crash_sleep_start, start + 1
    ) >= 0:
        raise SystemExit("pinned crash-screen sleep implementation changed")
    crash_screen = (
        crash_screen[:start]
        + "extern void PB3DS_RuntimeSleepMs(s32 ms);\n\n"
        + CRASH_SLEEP
        + crash_screen[end + 2:]
    )

    heaps = (args.upstream / "src/heaps.c").read_text(encoding="utf-8")
    pause_aux = "BSS u8 D_80200000[0x4000] ALIGNED(0x1000);"
    pause_aux_fixed = "BSS u8 D_80200000[0x38000] ALIGNED(0x1000);"
    if heaps.count(pause_aux) != 1:
        raise SystemExit("pinned heaps.c pause aux cache declaration changed")
    heaps = heaps.replace(pause_aux, pause_aux_fixed)

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "runtime_world_mac.c").write_text(
        source.split(marker, 1)[0] + WORLD_SUFFIX.lstrip(), encoding="utf-8"
    )
    (args.output / "runtime_game_modes.c").write_text(
        GAME_MODES, encoding="utf-8"
    )
    (args.output / "runtime_nusys_overrides.c").write_text(
        nusys, encoding="utf-8"
    )
    (args.output / "runtime_heap_storage.c").write_text(
        HEAP_STORAGE, encoding="utf-8"
    )
    (args.output / "runtime_heaps.c").write_text(heaps, encoding="utf-8")
    (args.output / "runtime_title_screen.c").write_text(
        title_screen, encoding="utf-8"
    )
    (args.output / "runtime_sprite_loader.c").write_text(
        sprite_loader, encoding="utf-8"
    )
    (args.output / "runtime_filemenu_yesno.c").write_text(
        filemenu_yesno, encoding="utf-8"
    )
    (args.output / "runtime_map_texture_patches.c").write_text(
        map_texture, encoding="utf-8"
    )
    (args.output / "runtime_background_patches.c").write_text(
        background, encoding="utf-8"
    )
    (args.output / "runtime_message_patches.c").write_text(
        message, encoding="utf-8"
    )
    (args.output / "runtime_shape_loader.c").write_text(
        shape_loader, encoding="utf-8"
    )
    (args.output / "runtime_crash_screen.c").write_text(
        crash_screen, encoding="utf-8"
    )
    (args.output / "runtime_mac_00_main.c").write_text(
        mac_00_main, encoding="utf-8"
    )
    (args.output / "runtime_mac_01_main.c").write_text(
        mac_01_main, encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
