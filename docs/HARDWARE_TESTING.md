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

## M11 first archive-frame checkpoint

Use the newest private `port/3ds` `.3dsx` bundle marked `0.11.0-m11`. Copy its
two O2R files beside the executable under `/3ds/PaperBoat3DS/`; do not share or
upload those user-generated archives.

1. Cold-boot and confirm the top screen shows the centered 296x200 title
   background with correct orientation, colors, and both triangles intact.
2. Confirm the bottom screen reports `Frame: title_bg`, `O2R: ready`, one draw
   and two triangles per frame, zero rejects/failures, and zero stream
   overflows.
3. Leave it active for 60 seconds and check for corruption, flicker, memory
   pressure, or counter stalls.
4. Close the lid for at least five seconds, reopen it, and confirm rendering
   and the input neutral gate recover.
5. Exit with START and preserve `PaperBoat3DS.log`.
6. As a separate negative test, rename `pm64.o2r`, relaunch, and confirm the
   complete checker rectangle plus sail appears with an explicit missing-
   archive fallback; restore the filename afterward.

Report the exact build SHA, New 3DS XL/LL model, firmware, launch environment,
success/fallback photos, lifecycle result, and log. This checkpoint validates a
legal archive-backed static frame, not title input, game-state execution, audio,
or playability. Old 3DS measurements remain part of the performance baseline.

## M12 title and file-select checkpoint

Use the newest private `port/3ds` `.3dsx` bundle marked `0.12.1-m12.1`. Copy the
owner-generated `paperboat.o2r` and `pm64.o2r` beside the executable under
`/3ds/PaperBoat3DS/`; never share or upload those archives.

1. Cold-boot and verify the background, logo, prompt, and copyright are upright,
   correctly colored, centered, and stable for 60 seconds. On New 3DS XL/LL,
   the bottom diagnostic must report `Safe:320@x40`; the fixed title canvas has
   equal black 40-pixel pillars and must not be stretched or cropped.
2. Capture the title once while the bottom diagnostic reports prompt `A:255`.
   PRESS START must be visible with PaperBoat's pale-yellow tint. `A:0` is an
   intentional blink-off frame, not a missing asset.
3. Press A and START separately from fresh title visits; each must enter the
   four-panel file-select checkpoint.
4. Visit all four slots with Circle Pad and D-Pad/C-direction input. Focus must
   move once per new direction and stay within the 2x2 grid.
5. Confirm each slot with A or START. The active border must turn green. Press B
   to return to title; no save or world load is expected yet.
6. Close the lid for at least five seconds, reopen it, release all controls to
   clear the neutral gate, and repeat one title/file-select round trip.
7. Confirm zero renderer rejects, failures, and stream overflows, then exit with
   L+R+START and preserve `PaperBoat3DS.log`.

Report the exact build SHA, New 3DS XL/LL model, firmware, launch environment,
photos of both screens on title and file select, lifecycle result, input result,
and complete log. This closes the owner's target-hardware check only; Old 3DS
performance/memory evidence remains outstanding.

## M13 core overworld gameplay

Use a private `.3dsx` bundle marked `0.13.8-m13r8` with the owner-generated O2R
files under `/3ds/PaperBoat3DS/`. Follow the M13 Folium procedure first, then
repeat it on the New 3DS XL/LL when the console is available. In addition:

1. Verify file confirmation advances through the numbered startup stages,
   reaches upstream `mac_00` without a freeze, and reports `Upstream: active`,
   then `Mode:5`.
2. Compare Mario movement, collision, camera, sprites, textures, palettes,
   transparency, fog, UI, fades, and framing with the same PaperBoat/N64 route.
3. Pause and resume at least ten times and verify upstream `Mode:10`, fixed
   player position while paused, and clean return to `Mode:5`.
4. Traverse `mac_00` to `mac_01` and back if reachable, checking upstream
   scripts, fades, entry motion, and loading-zone guards.
5. Close the lid while active and once while paused. On wake, release all
   controls and verify rendering resumes in the prior world state.
6. Confirm `unk`, `miss`, `Fall`, `bad`, renderer rejects, frame failures,
   stream overflows, and memory failures remain zero, then preserve the exact
   build SHA, both-screen captures, and `PaperBoat3DS.log`.

For r8, also photograph the `Step`, `Cmd/frame`, `Zclear`, `probe`, and
`pause` fields before pausing, while paused, and after resuming. Verify that
the world background and complete building surfaces are present, Mario/NPCs
remain whole while crossing other objects, and the warmed-up `last` update
time is normally at or below 40 ms without the slow-update count continually
increasing. All sprite/UI texturing must remain upright and the update count
must continue advancing in pause mode. A static pause frame is not an
acceptance pass even if the first menu labels appear.

This validates only the M13 Toad Town route. Audio, saves, battles, and later
chapter coverage remain blocked until its presentation and movement pass.

If startup stops, preserve the last numbered stage and the complete log. A
`runtime-panic` line includes the upstream assertion text; otherwise the final
`runtime-start` stage identifies the call that did not return.
