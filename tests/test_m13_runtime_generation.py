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

    print("M13 runtime generation checks passed: 11")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
