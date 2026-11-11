# S3 — FS / MST / texpack bring-up

## Bugfix (from S2 hardware)

Bottom LCD went black because `C2D_TargetClear` without a later `SceneBegin`/draw on that target was not presented. `ex3_plat_clear_screens` now clears, begins, and full-bleed fills **both** screens every frame.

## What shipped

| Piece | Role |
| --- | --- |
| `ex3_fs` | `romfs:/` / `sdmc:/` path mapping; host root override |
| `ex3_mst` | `EX3MST1` fixture parse; AES-CBC + PKCS7 using recovered key/IV |
| `ex3_texpack` | `EX3TEX1` RGBA fixture + texture plan clamps |
| `ex3_content` | Load diag fixtures; bottom status chrome |
| Bake tool | `make -C exvius3ds fixtures` → `romfs/fixtures/` |

## Hardware expect (S3)

1. **Top** — navy + gold panel + pale bar (unchanged)  
2. **Bottom** — **panel blue again**, plus green status strip, light-blue name bars, gold tex chip  
3. START exits  

## Proof

```bash
make -C exvius3ds test
```
