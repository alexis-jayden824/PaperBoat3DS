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

#define MODE(initfn, stepfn, drawfn) \
    { .init = initfn, .step = stepfn, .renderBackUI = drawfn }

const GameModeData GameModeTemplates[] = {
    [GAME_MODE_STARTUP] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
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
    if (!mode_supported(modeID)) PB3DS_RuntimeUnsupportedMode(modeID);
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("upstream", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    world_source = args.upstream / "src/world/world.c"
    source = world_source.read_text(encoding="utf-8")
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
    (args.output / "runtime_mac_00_main.c").write_text(
        mac_00_main, encoding="utf-8"
    )
    (args.output / "runtime_mac_01_main.c").write_text(
        mac_01_main, encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
