#!/usr/bin/env python3
"""Regression checks for M13's deliberately bounded upstream map closure."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_m13_runtime_generation.py GENERATOR UPSTREAM")
    generator = Path(sys.argv[1]).resolve()
    upstream = Path(sys.argv[2]).resolve()

    with tempfile.TemporaryDirectory(prefix="pb3ds-m13-generation-") as temp:
        output = Path(temp)
        subprocess.run(
            [sys.executable, str(generator), str(upstream), str(output)],
            check=True,
        )
        mac_00 = (output / "runtime_mac_00_main.c").read_text(encoding="utf-8")
        mac_01 = (output / "runtime_mac_01_main.c").read_text(encoding="utf-8")
        world = (output / "runtime_world_mac.c").read_text(encoding="utf-8")
        heap = (output / "runtime_heaps.c").read_text(encoding="utf-8")
        modes = (output / "runtime_game_modes.c").read_text(encoding="utf-8")
        title = (output / "runtime_title_screen.c").read_text(encoding="utf-8")
        crash = (output / "runtime_crash_screen.c").read_text(encoding="utf-8")
        nusys = (output / "runtime_nusys_overrides.c").read_text(
            encoding="utf-8"
        )
        sprite_loader = (output / "runtime_sprite_loader.c").read_text(
            encoding="utf-8"
        )
        filemenu_yesno = (output / "runtime_filemenu_yesno.c").read_text(
            encoding="utf-8"
        )
        map_texture = (
            output / "runtime_map_texture_patches.c"
        ).read_text(encoding="utf-8")
        background = (
            output / "runtime_background_patches.c"
        ).read_text(encoding="utf-8")
        message = (
            output / "runtime_message_patches.c"
        ).read_text(encoding="utf-8")
        shape_loader = (
            output / "runtime_shape_loader.c"
        ).read_text(encoding="utf-8")

    mac_00_binds = mac_00.split("EvtScript N(EVS_BindExitTriggers) = {", 1)[1]
    mac_00_binds = mac_00_binds.split("\n};", 1)[0]
    check("EVS_ExitWalk_mac_01_0" in mac_00_binds,
          "mac_00 must retain its accepted mac_01 exit")
    for blocked in ("kmr_10", "kmr_20", "tik_19"):
        check(blocked not in mac_00_binds,
              f"mac_00 bound an out-of-scope {blocked} exit")

    mac_01_binds = mac_01.split("EvtScript N(EVS_BindExitTriggers) = {", 1)[1]
    mac_01_binds = mac_01_binds.split("\n};", 1)[0]
    check("EVS_ExitWalk_mac_00_1" in mac_01_binds,
          "mac_01 must retain its accepted mac_00 exit")
    for blocked in ("nok_11", "osr_01", "mac_02", "EVS_ExitFlowerGate"):
        check(blocked not in mac_01_binds,
              f"mac_01 bound an out-of-scope {blocked} exit")

    check("load_obfuscation_shims();" not in world,
          "map loading must not read obfuscation shims from a nonexistent ROM")
    check("shim_general_heap_create_obfuscated();" not in world,
          "map loading must not invoke a ROM-relocated heap shim")
    check(world.count("general_heap_create();") == 2,
          "both accepted map-load paths must create the linked general heap")

    check("BSS u8 D_80200000[0x38000] ALIGNED(0x1000);" in heap,
          "generated heaps.c must size the pause aux cache to 0x38000")
    check("BSS u8 D_80200000[0x4000]" not in heap,
          "generated heaps.c must not keep the 16 KiB overlay placeholder")

    check("MODE(state_init_title_screen, state_step_title_screen" in modes,
          "M13 must enter the real upstream title mode")
    check("[GAME_MODE_STARTUP] = MODE(game_mode_restart" in modes,
          "reachable soft reset must re-enter staged runtime startup")
    check("PB3DS_RuntimeRestartToTitle();" in modes,
          "startup mode must request a safe restart rather than stall")
    check("MODE(state_init_file_select, state_step_file_select" in modes,
          "M13 must use the real upstream file-select mode")
    check("MODE(state_init_exit_file_select, state_step_exit_file_select" in modes,
          "M13 must use the real upstream file-select exit mode")
    check("CurGameModeID == GAME_MODE_END_FILE_SELECT" in modes,
          "file selection must route into the bounded M13 overworld")
    check("PB3DS_RuntimeUnsupportedMode(modeID);\n        return;" in modes,
          "unsupported in-range modes must stop before changing state")
    check("TitleScreen_TimeLeft = 32767;" in title,
          "title must not time out into an unlinked demo")
    check("TitleScreen_TimeLeft = 480;" not in title,
          "generated title must replace the upstream demo timeout")
    check("PB3DS_RuntimeSleepMs(ms);" in crash,
          "crash-screen delay must yield through the 3DS platform")
    check("while (osGetTime()" not in crash,
          "crash-screen delay must not busy-spin the homebrew process")

    check('#include "pb3ds/runtime_flash.h"' in nusys,
          "NuSystem flash wrappers must use the bounded persistence service")
    check("pb_flash_store_read" in nusys,
          "generated flash reads must use the persistence service")
    check("pb_flash_store_write_page" in nusys,
          "generated flash writes must be direct page writes")
    check("pb_flash_store_erase_sector" in nusys,
          "generated sector erase must alter persistent storage")
    check("u8 flash[FLASH_TOTAL_SIZE]" not in nusys,
          "flash writes must not allocate 128 KiB on the ARM stack")
    check("(void) page_num;\n    // No-op" not in nusys,
          "flash erase must not silently succeed without erasing")
    check("page_num > total_pages - n_pages" in nusys,
          "flash reads must reject out-of-range and overflowed requests")
    check("dramAddr == NULL" in nusys,
          "flash wrappers must reject null transfer buffers")
    check("static OSPiHandle sFlashHandle;" in nusys,
          "flash initialization must expose stable handle storage")
    check("return &sFlashHandle;" in nusys,
          "flash initialization must not report a null device")
    check("memset(ramAddr, 0, len);" in nusys,
          "diagnostic ROM reads must initialize their destination")
    check("void nuPiReadRom(u32 romAddr" in nusys and
          "void nuPiReadRom(u32 romAddr, void* ramAddr, u32 len) {\n    return;"
          not in nusys,
          "ROM reads must not leave caller memory indeterminate")
    check("loads SDL gamepad support" not in nusys,
          "generated controller documentation must describe the 3DS backend")
    check("return controllerBits & 1U;" in nusys,
          "controller initialization must return the detected logical port")
    check("data->err_no = 0;" in nusys,
          "controller query must use the effective libultraship ABI field")
    check(nusys.count("return PFS_ERR_NOPACK;") == 9,
          "every unsupported Controller Pak operation must fail explicitly")
    check("*fileNo = -1;" in nusys,
          "failed Controller Pak searches must not expose a valid file index")

    check("return small_gold_sparkle_main" in nusys,
          "reachable small-gold-sparkle wrapper must call the linked effect")
    check("fx_sun_undeclared" not in nusys,
          "dead sun placeholder must not advertise a successful null effect")
    check("seg == &D_8007798C || seg == &PauseOverlaySegment" in nusys,
          "file-menu and pause overlays must be explicitly resident")
    check("unsupported ROM overlay requested" in nusys,
          "an unknown overlay request must fail instead of silently succeeding")
    check("return -1;\n}" in nusys.split("s32 nuContRmbCheck", 1)[1].split(
              "void nuContRmbModeSet", 1)[0],
          "absent rumble hardware must be reported as unavailable")
    check("TODO: Call libultraship rumble" not in nusys,
          "rumble calls must be explicit absent-device operations")
    check("Sprite_LoadPlayer(idx, animData, spriteSize) == animData" in
          sprite_loader,
          "player sprite conversion failure must stop upstream startup")
    check("Sprite_LoadNPC(idx, animData, spriteSize) == animData" in
          sprite_loader,
          "NPC sprite conversion failure must stop map initialization")
    check('get_map_IDs_by_name_checked("mac_00"' in filemenu_yesno,
          "new slots must resolve inside the accepted M13 world registry")
    check("get_map_IDs_by_name_checked(NEW_GAME_MAP_ID" not in
          filemenu_yesno,
          "new-slot creation must not panic on the unlinked kmr_20 map")

    check("void port_release_map_textures(void)" in map_texture,
          "generated map textures must expose an explicit release point")
    check("free(handle->gfx);" in map_texture and
          "sPortTextureOwned[i]" in map_texture,
          "only owned map display lists may be freed on transition")
    check("memset(handle, 0, sizeof(*handle));" in map_texture,
          "released map handles must not retain stale resource pointers")
    check("MAX_TEXTURE_GFX_CMDS 256" in map_texture and
          "map texture gfx overflow" in map_texture,
          "generated texture command storage must be bounded and checked")
    check("pb_runtime_resource_texture_type(path) == expected" in map_texture,
          "map textures must match their pinned Fast3D pixel format")
    check("GameEngine_GetTexWidthExact(path) == width" in map_texture and
          "GameEngine_GetTexHeightExact(path) == height" in map_texture,
          "map texture dimensions must match metadata")
    check('"_aux_tlut"' in map_texture and '"_aux"' in map_texture,
          "auxiliary map rasters and palettes must be validated")
    check('"_mm%d"' in map_texture and "port_mip_level_count" in map_texture,
          "every consumed mip level must be validated")
    check("ASSERT_MSG(idx >= 0, \"map texture metadata missing\")" in
          map_texture,
          "unknown map texture names must fail instead of using texture 0")
    check("MISSING resource" not in map_texture and "-> textureID=0" not in
          map_texture,
          "missing map art must not be hidden behind texture 0")
    check("char path[160];" in map_texture and
          "sNamedLevels[sNamedLevelCount].path = (char*) malloc" not in
          map_texture,
          "stable derived texture paths must not leak heap allocations")
    check("port_release_map_textures();\n    TreeIterPos = 0;" in map_texture,
          "each map load must release the previous map texture commands")

    check("void port_release_background_resource(void)" in background,
          "background resources must expose an explicit release point")
    check("pb_runtime_resource_otr_name(path)" in background,
          "backgrounds must retain resource-owned OTR names")
    check("PB_RESOURCE_TEXTURE_CI8, 296, 200" in background and
          "PB_RESOURCE_TEXTURE_RGBA16, 256, 1" in background,
          "background raster and palette metadata must be exact")
    check("ASSERT_MSG(valid, \"invalid background resource\")" in background,
          "missing or malformed backgrounds must fail at their source")
    check("malloc(" not in background,
          "background path changes must not leak general-heap storage")

    check("pb_runtime_resource_otr_name(path)" in message,
          "message companions must use resource-owned stable names")
    check("pb_runtime_resource_exists(path)" in message,
          "optional message companions must distinguish absence from damage")
    check("invalid message texture companion" in message,
          "present malformed message companions must fail loudly")
    check("hasExactCompanions" in message and
          "charset_title_\", false" in message,
          "the pinned title-glyph stride mismatch must use the exact OBLB")
    check("malloc(" not in message and "sStandardPaths" not in message,
          "message drawing must not allocate one path per glyph")
    check("return path != NULL ? (IMG_PTR) path : glyph;" in message and
          "return path != NULL ? (PAL_PTR) path : palette;" in message,
          "missing optional companions must retain authentic loaded sheets")

    check("pb_world_validate_shape_payload(rawData, rawSize" in shape_loader,
          "the native shape converter must revalidate its bounded input")
    check("invalid shape payload reached loader" in shape_loader,
          "invalid shape input must stop before native pointer conversion")
    check("size > sArenaSize - sArenaPos" in shape_loader,
          "shape arena accounting must not wrap")
    check("shape native arena exhausted" in shape_loader,
          "shape arena exhaustion must fail instead of dereferencing null")
    check("shape display list unavailable" in shape_loader and
          "shape display-list path too long" in shape_loader,
          "shape display-list resolution must fail at the missing resource")
    check("(void) rawSize" not in shape_loader,
          "shape conversion must consume the supplied payload bound")

    print("M13 runtime generation checks passed: 82")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
