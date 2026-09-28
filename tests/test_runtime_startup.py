#!/usr/bin/env python3
"""Source-level contract for M13's staged PaperBoat startup lifecycle.

The upstream runtime is too large to unit-link on the host without replacing
the very services this contract is intended to audit.  These checks therefore
lock the ordering and state transitions in the production translation units;
the devkitARM CI job separately compiles and links those exact units.
"""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
RUNTIME = (ROOT / "source" / "runtime_platform.c").read_text(encoding="utf-8")
MAIN = (ROOT / "source" / "main.c").read_text(encoding="utf-8")

checks = 0


def check(condition: bool, message: str) -> None:
    global checks
    checks += 1
    if not condition:
        raise AssertionError(message)


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    check(start >= 0, f"missing function: {signature}")
    brace = source.find("{", start)
    check(brace >= 0, f"missing function body: {signature}")
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError(f"unterminated function body: {signature}")


def appears_in_order(source: str, tokens: tuple[str, ...], message: str) -> None:
    position = -1
    for token in tokens:
        position = source.find(token, position + 1)
        check(position >= 0, f"{message}: missing {token}")


def main() -> int:
    expected_steps = (
        "PB_START_RESOURCE_INDEX",
        "PB_START_RESOURCE_PREFLIGHT",
        "PB_START_GLOBALS",
        "PB_START_DEFAULTS",
        "PB_START_WORLD_DEFAULTS",
        "PB_START_FLASH",
        "PB_START_INPUT",
        "PB_START_GENERAL_HEAP",
        "PB_START_RENDER_TASKS",
        "PB_START_WORKERS",
        "PB_START_SCRIPTS",
        "PB_START_CAMERAS",
        "PB_START_PLAYER_STATUS",
        "PB_START_PLAYER_SPRITES",
        "PB_START_ENTITY_MODELS",
        "PB_START_ANIMATORS",
        "PB_START_MODELS",
        "PB_START_SPRITE_SHADING",
        "PB_START_BACKGROUND",
        "PB_START_CHARACTER_SET",
        "PB_START_PRINTERS",
        "PB_START_GAME_MODE",
        "PB_START_NPCS",
        "PB_START_HUD_AUX",
        "PB_START_HUD",
        "PB_START_TRIGGERS",
        "PB_START_ENTITIES",
        "PB_START_PLAYER_DATA",
        "PB_START_BATTLE",
        "PB_START_ENCOUNTER",
        "PB_START_OVERLAYS",
        "PB_START_EFFECTS",
        "PB_START_SAVED_VARIABLES",
        "PB_START_ITEM_ENTITIES",
        "PB_START_COLLISION",
        "PB_START_MUSIC",
        "PB_START_MUSIC_PLAYERS",
        "PB_START_AMBIENT",
        "PB_START_SOUNDS",
        "PB_START_WINDOWS",
        "PB_START_PARTNERS",
        "PB_START_MUSIC_VOLUME",
        "PB_START_CURTAINS",
        "PB_START_RUMBLE",
        "PB_START_SAVE_GLOBALS",
        "PB_START_SOUND_OUTPUT",
        "PB_START_ENGINE_READY",
        "PB_START_TITLE",
    )
    enum_match = re.search(
        r"typedef enum \{(?P<body>.*?)\} PBRuntimeStartupStep;",
        RUNTIME,
        re.DOTALL,
    )
    check(enum_match is not None, "startup step enum is missing")
    actual_steps = tuple(re.findall(r"\bPB_START_[A-Z_]+\b", enum_match.group("body")))
    check(actual_steps == expected_steps, "staged startup order changed")

    stage_names = function_body(RUNTIME, "static const char *runtime_startup_stage")
    continuation = function_body(RUNTIME, "bool pb_runtime_continue_startup")
    for step in expected_steps:
        check(f"case {step}:" in stage_names,
              f"startup diagnostics omit {step}")
        check(f"case {step}:" in continuation,
              f"startup executor omits {step}")

    begin = function_body(RUNTIME, "bool pb_runtime_begin_game")
    appears_in_order(
        begin,
        (
            "pb_runtime_resources_bind(&runtime->resources);",
            "runtime->state = PB_RUNTIME_LOADING;",
            "runtime->startup_step = PB_START_RESOURCE_INDEX;",
            "runtime_mark_startup_stage(runtime);",
        ),
        "initial startup transition",
    )

    appears_in_order(
        continuation,
        (
            "pb_runtime_resources_prepare(&runtime->resources)",
            "pb_runtime_resources_validate_m13(&runtime->resources)",
            "runtime_validate_player_raster_tables(runtime)",
            "runtime_validate_player_sprites(runtime)",
            "runtime_validate_intro_npc_sprites(runtime)",
            "init_game_globals()",
            "runtime_set_engine_defaults()",
            "runtime_set_world_defaults()",
            "fio_init_flash()",
            "general_heap_create()",
            "spr_init_sprites(PLAYER_SPRITES_MARIO_WORLD)",
            "clear_printers()",
            "hud_element_set_aux_cache(NULL, 0)",
            "reset_battle_status()",
            "initialize_collision()",
            "bgm_init_music_players()",
            "partner_initialize_data()",
            "bgm_reset_volume()",
            "fio_load_globals()",
            "runtime_apply_sound_preference()",
            "runtime_finish_engine_data()",
            "runtime_activate_title(runtime)",
        ),
        "resource-to-title startup order",
    )
    check("if (runtime->state == PB_RUNTIME_FAILED) return false;" in continuation,
          "failed startup must not advance")
    check("if (runtime->state == PB_RUNTIME_ACTIVE) return true;" in continuation,
          "title activation must terminate staged startup")
    check(continuation.count("runtime->startup_step++;") == 1,
          "each APT iteration must advance at most one startup stage")

    finish = function_body(RUNTIME, "static void runtime_finish_engine_data")
    check("clear_game_mode();" in finish,
          "initial startup must leave a clean mode before title activation")
    check("set_game_mode(GAME_MODE_STARTUP)" not in finish,
          "initial startup must not recursively request a soft reset")

    activate = function_body(RUNTIME, "static void runtime_activate_title")
    appears_in_order(
        activate,
        (
            "set_game_mode(GAME_MODE_TITLE_SCREEN);",
            "runtime->next_update_ms = pb_platform_time_ms();",
            "runtime->state = PB_RUNTIME_ACTIVE;",
        ),
        "title activation transition",
    )

    restart = function_body(RUNTIME, "void PB3DS_RuntimeRestartToTitle")
    appears_in_order(
        restart,
        (
            "gOverrideFlags |= GLOBAL_OVERRIDES_DISABLE_DRAW_FRAME;",
            "active_runtime->restart_requested = true;",
        ),
        "soft-reset request",
    )
    check("port_release_map_textures()" not in restart and
          "port_release_background_resource()" not in restart,
          "soft-reset callback must not free data inside step_game_loop")

    update = function_body(RUNTIME, "bool pb_runtime_update")
    check("now_ms < runtime->next_update_ms" in update,
          "runtime update must honor its 30 Hz deadline")
    check("runtime->stats.pacing_waits++" in update,
          "pacing skips must remain observable")
    check("Graphics_ThreadUpdate();" in update,
          "active update must call the real upstream frame function")
    appears_in_order(
        update,
        (
            "Graphics_ThreadUpdate();",
            "if (runtime->restart_requested)",
            "pb_gfx_api_3ds_invalidate_texture(runtime->graphics, NULL);",
            "port_release_map_textures();",
            "port_release_background_resource();",
            "runtime->state = PB_RUNTIME_LOADING;",
            "runtime->startup_step = PB_START_RESOURCE_INDEX;",
            "runtime_mark_startup_stage(runtime);",
        ),
        "deferred soft-reset transition",
    )
    check("runtime->stats.updates % 3U == 0U ? 34U : 33U" in update,
          "30 Hz cadence must remain 33/33/34 ms")
    check("while (now_ms" not in update and "do {" not in update,
          "slow frames must not trigger an unbounded catch-up loop")

    shutdown = function_body(RUNTIME, "void pb_runtime_shutdown")
    appears_in_order(
        shutdown,
        (
            "pb_gfx_api_3ds_invalidate_texture(runtime->graphics, NULL);",
            "port_release_map_textures();",
            "port_release_background_resource();",
            "active_runtime = NULL;",
            "pb_runtime_resources_clear(&runtime->resources);",
            "runtime->state = PB_RUNTIME_INACTIVE;",
        ),
        "shutdown lifetime order",
    )

    main_body = function_body(MAIN, "int main")
    check("(void)pb_runtime_begin_game(&runtime);" in main_body,
          "normal application path must enter the staged game runtime")
    loading = main_body.find("runtime.state == PB_RUNTIME_LOADING")
    active = main_body.find("runtime.state == PB_RUNTIME_ACTIVE")
    check(loading >= 0 and active > loading,
          "APT loop must service loading before active updates")
    check(main_body.count("pb_runtime_continue_startup(&runtime)") == 1,
          "APT loop must execute only one startup stage per iteration")
    check(main_body.count("pb_runtime_update(&runtime)") == 1,
          "APT loop must execute at most one upstream update per iteration")
    check("PB3DS_GAME_RUNTIME_READY" not in MAIN + RUNTIME,
          "obsolete compile-time gameplay gate must not return")

    print(f"M13 runtime startup lifecycle: {checks} checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
