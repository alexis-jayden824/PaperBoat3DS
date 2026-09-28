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
        heap = (output / "runtime_heaps.c").read_text(encoding="utf-8")
        modes = (output / "runtime_game_modes.c").read_text(encoding="utf-8")
        title = (output / "runtime_title_screen.c").read_text(encoding="utf-8")
        nusys = (output / "runtime_nusys_overrides.c").read_text(
            encoding="utf-8"
        )

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

    check("BSS u8 D_80200000[0x38000] ALIGNED(0x1000);" in heap,
          "generated heaps.c must size the pause aux cache to 0x38000")
    check("BSS u8 D_80200000[0x4000]" not in heap,
          "generated heaps.c must not keep the 16 KiB overlay placeholder")

    check("MODE(state_init_title_screen, state_step_title_screen" in modes,
          "M13 must enter the real upstream title mode")
    check("MODE(state_init_file_select, state_step_file_select" in modes,
          "M13 must use the real upstream file-select mode")
    check("MODE(state_init_exit_file_select, state_step_exit_file_select" in modes,
          "M13 must use the real upstream file-select exit mode")
    check("CurGameModeID == GAME_MODE_END_FILE_SELECT" in modes,
          "file selection must route into the bounded M13 overworld")
    check("TitleScreen_TimeLeft = 32767;" in title,
          "title must not time out into an unlinked demo")
    check("TitleScreen_TimeLeft = 480;" not in title,
          "generated title must replace the upstream demo timeout")

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

    print("M13 runtime generation checks passed: 25")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
