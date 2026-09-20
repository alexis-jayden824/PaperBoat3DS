# PaperBoat3DS Hardware Testing

## M1 bootstrap checkpoint

This checkpoint validates only the native ARM11 application shell. It does not
load Paper Mario or PaperBoat assets, and no game files are required.

Reference build:

- Branch: `port/3ds`
- Commit: `662488b4ac72381c27ea8ff312cba42b9acb20cc`
- Workflow: Nintendo 3DS build run `35456778305`
- Toolchain: devkitARM 16.1.0

## Install

1. Download the `PaperBoat3DS-662488b...` artifact from workflow run `35456778305`.
2. Extract `PaperBoat3DS.3dsx` from the artifact.
3. Create `/3ds/PaperBoat3DS/` on the 3DS SD card.
4. Copy the file to `/3ds/PaperBoat3DS/PaperBoat3DS.3dsx`.
5. Start it from the Homebrew Launcher.

## Expected result

- The top display shows `PaperBoat3DS`, version `0.1.0-m1`, the M1 native-app
  stage, and the legal-assets notice.
- The bottom display shows `M1 Service Diagnostics`, reports GFX, APT, and HID
  as `OK`, identifies the console family, prints the kernel version, and shows
  the lifecycle as `active`.
- Closing and reopening the lid returns the lifecycle display to `active`
  without a crash or frozen display.
- Pressing START returns cleanly to the Homebrew Launcher.
- The application does not crash, hang, display an exception screen, or require
  copyrighted assets.

## Test report

Record the following when reporting a result:

- Exact build commit
- Console model (Old/New 3DS, 3DS XL, 2DS, or New 2DS XL)
- System version and Homebrew Launcher environment
- Whether both displays matched the expected result
- Whether START exited cleanly
- A photo or the exact text of any exception/error screen

M1 is complete only after this test passes on real hardware. Emulator results
may supplement the report but do not replace hardware authority.

Pending hardware profile supplied by the project owner:

- Model: New Nintendo 3DS LL, region-converted/modded and functionally treated
  as a New Nintendo 3DS XL
- Status: console not currently available; M1 hardware validation remains pending

## M6 memory checkpoint

Use the newest passing `port/3ds` artifact marked `0.6.0-m6`. The `.3dsx`
build is authoritative; `.cia` may be checked additionally under CFW.

1. Cold-boot the application without a `paperboat.o2r` archive installed.
2. Leave the application idle for 30 seconds.
3. Close the lid for at least five seconds, reopen it, and confirm the lifecycle
   returns to `active`.
4. Photograph the bottom screen, then press START to exit normally.
5. Preserve `/3ds/PaperBoat3DS/PaperBoat3DS.log` before another run appends to
   it.

Report the build SHA, console model, system version, launch method, photo, and
complete log. The shutdown record must include application/linear free space,
peak application/linear/stack usage, zero allocation failures, and
`pressure=no`. A New 3DS XL/LL result is useful secondary evidence; an Old 3DS
result remains required to calibrate and formally close the milestone.

## M8 input checkpoint

Use the newest passing `port/3ds` artifact marked `0.8.0-m8`.

1. Launch the `.3dsx` from `/3ds/PaperBoat3DS/PaperBoat3DS.3dsx`.
2. Press A, B, X, Y, L, R, and every D-Pad direction individually; confirm the
   live N64 held/press masks change and clear on release.
3. Hold L with each D-Pad direction; confirm the shifted N64 D-Pad masks appear
   instead of the default C-button masks.
4. Move the Circle Pad slowly around the center and then to each extreme;
   confirm the deadzone is stable and displayed values approach -80 or 80.
5. Tap and drag across the bottom screen; confirm DOWN and coordinates stay in
   the 320x240 range.
6. Press SELECT; confirm `Menu SELECT: REQUESTED` appears without a gameplay
   button becoming stuck.
7. Hold a button, close the lid for at least five seconds, reopen it while still
   holding the button, then release it. Confirm `WAIT NEUTRAL` clears only after
   release and no fresh press is generated automatically.
8. Press START and confirm a clean return to the Homebrew Launcher.

Report the build SHA, console model, system version, launch method, mapping
results, lifecycle result, a photo of the live diagnostics, and the resulting
`/3ds/PaperBoat3DS/PaperBoat3DS.log`. A New 3DS XL/LL result validates the
owner's target console; Old 3DS remains the performance baseline.

## M9 renderer checkpoint

Use the newest passing `port/3ds` `.3dsx` artifact marked `0.9.0-m9`.

1. Cold-boot the diagnostic and confirm the top display contains the dark navy,
   teal/light checker, and translucent sail scene described in
   `docs/M9_RENDERER.md`.
2. Confirm the bottom display reports `C3D: ready`, advancing frame/draw/vertex
   counters, zero frame failures, and a stable command-buffer peak.
3. Leave it running for 60 seconds and check for corruption, flicker, freezes,
   or memory pressure.
4. Close the lid for at least five seconds, reopen it, and confirm rendering
   resumes without stale input or a crash.
5. Press START to exit, then preserve the SD log.

Report the exact build SHA, model, firmware, launch environment, photographs of
both displays, idle/lifecycle result, and complete log. The owner's New 3DS
XL/LL result is required for the target console. An Old 3DS result remains
required for baseline performance and memory conclusions.

## M10 libultraship graphics checkpoint

Use the newest passing `port/3ds` `.3dsx` artifact marked `0.10.0-m10`.

1. Confirm the top screen shows the repeated checker and an unmistakable bright
   translucent sail; photograph both displays.
2. Confirm `API: ready`, two TEV shaders, one texture, zero unsupported shaders,
   zero rejected normal commands, and zero frame failures.
3. Leave the diagnostic active for 60 seconds. Counters must continue at two
   draws and three triangles per presented frame without flicker or corruption.
4. Close the lid for at least five seconds, reopen it, and confirm rendering
   resumes while frame admission and the input neutral gate recover cleanly.
5. Press START, confirm a clean Homebrew Launcher return, and preserve the SD
   log.

Report the build SHA, model, firmware, launch environment, photos, lifecycle
result, and complete log. The New 3DS XL/LL result validates the owner's target
console. Old 3DS data remains required for the performance and memory baseline.
