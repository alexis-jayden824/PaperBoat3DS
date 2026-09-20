# M8 Nintendo 3DS Input Backend

M8 establishes a small libctru-facing input boundary that emits the N64 button
and stick contract consumed by PaperBoat. It does not depend on SDL, ImGui, or
the desktop controller stack, and its mapping logic is host-testable without a
3DS SDK.

## Default mapping

Old Nintendo 3DS remains the baseline. New Nintendo 3DS controls are optional
duplicates rather than requirements.

| Nintendo 3DS input | Paper Mario / N64 input | Notes |
|---|---|---|
| Circle Pad | Analog stick | Per-axis deadzone of 15 raw units; scaled and clamped to -80..80 |
| A | A | Confirm / jump |
| B | B | Cancel / hammer |
| X | Z | Spin and Z-trigger actions; required on Old 3DS |
| Y | C-Down | Convenient partner-action duplicate |
| D-Pad | C-Up/Down/Left/Right | Default layer for frequently used C-button menus/actions |
| L | L | Direct when no D-Pad direction is held |
| L + D-Pad | D-Up/Down/Left/Right | Shift layer; L is suppressed while the chord is active |
| R | R | Direct |
| START | START | Exits the M8 diagnostic shell; becomes the game START input after integration |
| SELECT | PaperBoat menu request | Reserved and never emitted to the game |
| ZL (New 3DS) | Z | Optional duplicate |
| ZR (New 3DS) | R | Optional duplicate |
| C-Stick (New 3DS) | C buttons | Optional direct C-button input |
| Touch screen | Raw 320x240 touch state | Coordinates and edges only; M16 owns menu widgets and gesture policy |

Opposing physical D-Pad directions resolve to neutral. Circle-Pad direction
bits reported by libctru are not also mapped as buttons, so analog movement does
not accidentally open a C-button or D-Pad action.

## Conflict audit and menu key

The pinned PaperBoat source uses every N64 button family. A raw symbol audit
shows substantially more C-button use than D-Pad use, with C buttons appearing
through partner actions, pause tabs, item/status menus, messages, and battle
flows. N64 D-Pad use is narrower and includes battle action-command and debug
paths. The default layer therefore favors C buttons while retaining all four
D-Pad directions through the L shift layer.

PaperBoat/libultraship toggles its desktop menu with F1, Escape, or the modern
controller Back button. `SELECT` is the closest 3DS equivalent and has no N64
button bit, so reserving it does not steal A, B, Z, START, R, a C button, or an
analog direction from gameplay. M8 records a menu request only; M16 will attach
that request to the bottom-screen configuration frontend.

## Frame and lifecycle contract

`PBInputState` records:

- native held, pressed, and released masks;
- N64 held, pressed, and released masks;
- normalized analog-stick values;
- touch held, pressed, released, and clamped coordinates;
- the one-frame PaperBoat-menu request;
- suspension state and a resume neutral gate.

Edges are derived from consecutive held samples rather than relying on a
backend-specific repeat policy. Suspending or sleeping clears all live input.
After restore/wakeup, samples remain blocked until buttons, touch, and Circle
Pad return to neutral. This prevents a key held across HOME/lid transitions
from becoming a fresh gameplay or menu action.

## Reproducible validation

Run the host policy suite on any system with a C11 compiler:

```sh
sh tools/test_input_backend.sh
```

The test suite covers the complete default mapping, New 3DS duplicates, the
Old 3DS D-Pad shift layer, opposing directions, analog endpoints and deadzone,
button edges, menu isolation, touch edges/bounds, lifecycle neutralization, and
the libctru polling adapter.

The M8 diagnostic build displays the live N64 mask, edge masks, analog values,
touch coordinates, and whether SELECT generated a menu request. Software
acceptance requires the host suite and all three package builds to pass.
Folium can provide a fast controller-overlay check; real Circle Pad, touch, lid,
and key behavior remain subject to project-owner hardware validation.
