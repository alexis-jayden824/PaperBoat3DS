# S2 acceptance record

**Sprint:** S2 — Cocos / renderer shim v0  
**Date:** 2026-11-11 (America/New_York)  
**Status:** Host software gate **met**. Hardware gold-sprite confirmation **owner pending**.

## Criteria → evidence

| Criterion | Evidence |
| --- | --- |
| Director/Scene/Node/Sprite/Label stubs | `exvius3ds/include/ex3/cocos.h`, `source/cocos.c` |
| Draw path via plat | `ex3_plat_draw_rect` → citro2d / host |
| Texture size clamps | `ex3/texture.h` — max 1024, POT pad |
| Diagnostic scene on top | `ex3_diag_scene_create` used by bootstrap |
| Host proof | `make -C exvius3ds test` (`test_cocos`, `test_texture`) |

## Owner hardware check

Pass when top shows gold panel on navy (not flat navy alone). See `S2_COCOS_RENDERER.md`.
