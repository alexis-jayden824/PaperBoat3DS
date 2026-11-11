# S2 acceptance record

**Sprint:** S2 — Cocos / renderer shim v0  
**Date:** 2026-11-11 (America/New_York)  
**Status:** **Complete** (with follow-up bottom-clear fix in S3).

## Criteria → evidence

| Criterion | Evidence |
| --- | --- |
| Director/Scene/Node/Sprite/Label stubs | `exvius3ds/include/ex3/cocos.h` |
| Draw path via plat | `ex3_plat_draw_rect` → citro2d |
| Texture size clamps | `ex3/texture.h` |
| Diagnostic scene on top | Owner hardware: gold rectangle + pale bar on navy top |
| Bottom panel blue | Initially black on hardware (clear without SceneBegin/draw); fixed in S3 `plat_3ds` clear path |

## Hardware note

Owner confirmed top gold panel on real n3DS XL. Bottom black was a present bug; S3 paints full-bleed clears on both screens every frame.
