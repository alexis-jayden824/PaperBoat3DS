# S2 — Cocos / renderer shim v0

**Status:** Software complete (2026-11-11). Hardware confirmation: look for **gold rectangle + pale bar** on the navy top screen; bottom stays panel blue.

## What shipped

| Piece | Role |
| --- | --- |
| `ex3/cocos.h` | Director / Scene / Node / Sprite / Label scene graph |
| `ex3/texture.h` | PICA plan: ≤1024, power-of-two pad, reject oversized |
| `ex3_plat_draw_rect` | GLES-like solid quad → citro2d (`C2D_DrawRectSolid`) / host recorder |
| Diagnostic scene | Top: gold 160×80 sprite + pale label bar; bottom: chrome clear |

## Hardware expect

After installing the new `.3dsx`:

1. **Top** — navy background with a **gold** centered panel and a lighter bar inside it  
2. **Bottom** — solid panel blue (unchanged chrome placeholder)  
3. START still exits  

If you still see only flat navy on top with **no gold**, the S2 binary did not install — re-copy the CI artifact.

## Proof

```bash
make -C exvius3ds test
```

Includes `test_cocos` (2 draw_rects) and `test_texture` (POT + 1024 clamp).
