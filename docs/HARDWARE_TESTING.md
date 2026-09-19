# PaperBoat3DS Hardware Testing

## M1 bootstrap checkpoint

This checkpoint validates only the native ARM11 application shell. It does not
load Paper Mario or PaperBoat assets, and no game files are required.

Reference build:

- Branch: `port/3ds`
- Commit: `25cd3cd2dba14192c049cbeb6d42dd2839f227e2`
- Workflow: Nintendo 3DS build run 4
- Toolchain: devkitARM 16.1.0

## Install

1. Download the `PaperBoat3DS-25cd3cd...` artifact from workflow run 4.
2. Extract `PaperBoat3DS.3dsx` from the artifact.
3. Create `/3ds/PaperBoat3DS/` on the 3DS SD card.
4. Copy the file to `/3ds/PaperBoat3DS/PaperBoat3DS.3dsx`.
5. Start it from the Homebrew Launcher.

## Expected result

- The top display shows `PaperBoat3DS`, version `0.0.1-m0`, the M0 toolchain
  stage, and the legal-assets notice.
- The bottom display shows the reserved PaperBoat configuration area and notes
  that the touch menu and physical menu keybind are deferred.
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
