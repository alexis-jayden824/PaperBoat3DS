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


GAME_MODES = r'''#include "game_modes.h"

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
    [GAME_MODE_TITLE_SCREEN] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
    [GAME_MODE_ENTER_DEMO_WORLD] = MODE(state_init_enter_demo, state_step_enter_world, state_drawUI_enter_world),
    [GAME_MODE_ENTER_WORLD] = MODE(state_init_enter_world, state_step_enter_world, state_drawUI_enter_world),
    [GAME_MODE_WORLD] = MODE(state_init_world, state_step_world, state_drawUI_world),
    [GAME_MODE_CHANGE_MAP] = MODE(state_init_change_map, state_step_change_map, state_drawUI_change_map),
    [GAME_MODE_GAME_OVER] = MODE(state_init_game_over, state_step_game_over, state_drawUI_game_over),
    [GAME_MODE_BATTLE] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
    [GAME_MODE_END_BATTLE] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
    [GAME_MODE_PAUSE] = MODE(state_init_pause, state_step_pause, state_drawUI_pause),
    [GAME_MODE_UNPAUSE] = MODE(state_init_unpause, state_step_unpause, state_drawUI_unpause),
    [GAME_MODE_FILE_SELECT] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
    [GAME_MODE_END_FILE_SELECT] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
    [GAME_MODE_INTRO] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
    [GAME_MODE_DEMO] = MODE(game_mode_nop, game_mode_nop, game_mode_nop),
};

BSS s32 CurGameModeID;
BSS GameModeData CurGameMode;

static b32 mode_supported(s32 modeID) {
    return modeID == GAME_MODE_STARTUP ||
           (modeID >= GAME_MODE_ENTER_DEMO_WORLD &&
            modeID <= GAME_MODE_GAME_OVER) ||
           modeID == GAME_MODE_PAUSE || modeID == GAME_MODE_UNPAUSE;
}

s32 get_game_mode(void) { return CurGameModeID; }

void set_game_mode(s32 modeID) {
    if (modeID < 0 || modeID >= ARRAY_COUNT(GameModeTemplates)) {
        PB3DS_RuntimeUnsupportedMode(modeID);
        return;
    }
    if (!mode_supported(modeID)) PB3DS_RuntimeUnsupportedMode(modeID);
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

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "runtime_world_mac.c").write_text(
        source.split(marker, 1)[0] + WORLD_SUFFIX.lstrip(), encoding="utf-8"
    )
    (args.output / "runtime_game_modes.c").write_text(
        GAME_MODES, encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
