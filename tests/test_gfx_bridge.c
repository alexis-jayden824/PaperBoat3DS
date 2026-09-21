#include "pb3ds/gfx_bridge.h"
#include "pb3ds/fast3d_semantics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks_run;

#define CHECK(expression)                                                    \
    do {                                                                     \
        checks_run++;                                                        \
        if (!(expression)) {                                                 \
            fprintf(stderr, "M10 graphics check failed at %s:%d: %s\n",  \
                    __FILE__, __LINE__, #expression);                        \
            return false;                                                    \
        }                                                                    \
    } while (0)

static uint64_t pack_formula(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                             unsigned int shift) {
    return ((uint64_t)a << shift) | ((uint64_t)b << (shift + 4U)) |
           ((uint64_t)c << (shift + 8U)) | ((uint64_t)d << (shift + 12U));
}

static uint64_t shade_shader_id(void) {
    return pack_formula(0, 0, 0, PB_GFX_SHADER_SHADE, 0) |
           pack_formula(0, 0, 0, PB_GFX_SHADER_SHADE, 16);
}

static uint64_t texture_shade_shader_id(void) {
    return pack_formula(PB_GFX_SHADER_TEXEL0, 0, PB_GFX_SHADER_SHADE, 0, 0) |
           pack_formula(PB_GFX_SHADER_TEXEL0, 0,
                        PB_GFX_SHADER_SHADE, 0, 16);
}

static void encode_combine_words(const uint8_t cycles[2][2][4],
                                 uint32_t *word0, uint32_t *word1) {
    *word0 = UINT32_C(0xFC000000) |
             (((uint32_t)cycles[0][0][0] & 0xFU) << 20U) |
             (((uint32_t)cycles[0][0][2] & 0x1FU) << 15U) |
             (((uint32_t)cycles[0][1][0] & 0x7U) << 12U) |
             (((uint32_t)cycles[0][1][2] & 0x7U) << 9U) |
             (((uint32_t)cycles[1][0][0] & 0xFU) << 5U) |
             ((uint32_t)cycles[1][0][2] & 0x1FU);
    *word1 = (((uint32_t)cycles[0][0][1] & 0xFU) << 28U) |
             (((uint32_t)cycles[1][0][1] & 0xFU) << 24U) |
             (((uint32_t)cycles[1][1][0] & 0x7U) << 21U) |
             (((uint32_t)cycles[1][1][2] & 0x7U) << 18U) |
             (((uint32_t)cycles[0][0][3] & 0x7U) << 15U) |
             (((uint32_t)cycles[0][1][1] & 0x7U) << 12U) |
             (((uint32_t)cycles[0][1][3] & 0x7U) << 9U) |
             (((uint32_t)cycles[1][0][3] & 0x7U) << 6U) |
             (((uint32_t)cycles[1][1][1] & 0x7U) << 3U) |
             ((uint32_t)cycles[1][1][3] & 0x7U);
}

static bool test_fast3d_semantics_and_tev(void) {
    uint8_t cycles[2][2][4] = {
        { { 1U, 31U, 4U, 31U }, { 1U, 7U, 4U, 7U } },
        { { 31U, 31U, 31U, 31U }, { 7U, 7U, 7U, 7U } },
    };
    uint32_t word0 = 0U;
    uint32_t word1 = 0U;
    encode_combine_words(cycles, &word0, &word1);
    PBFast3DCombiner semantic;
    CHECK(pb_fast3d_generate_combiner(
        &semantic, word0, word1,
        pb_gfx_shader_option(PB_GFX_OPT_ALPHA)));
    CHECK(semantic.shader_id0 == texture_shade_shader_id());
    CHECK(semantic.used_textures[0]);
    CHECK(!semantic.used_textures[1]);
    CHECK(semantic.uses_shade);

    /* (TEXEL0 - ENVIRONMENT) * SHADE + ENVIRONMENT is a single
     * hardware interpolate stage, not TEXEL0 times a CPU approximation. */
    cycles[0][0][0] = 1U;
    cycles[0][0][1] = 5U;
    cycles[0][0][2] = 4U;
    cycles[0][0][3] = 5U;
    encode_combine_words(cycles, &word0, &word1);
    CHECK(pb_fast3d_generate_combiner(
        &semantic, word0, word1,
        pb_gfx_shader_option(PB_GFX_OPT_ALPHA)));
    CHECK(semantic.input_mapping[0][0] ==
          PB_FAST3D_INPUT_ENVIRONMENT);
    PBGfxCombinerPlan plan;
    CHECK(pb_gfx_combiner_decode(&plan, semantic.shader_id0,
                                 semantic.shader_id1));
    float inputs[6][4] = { { 0 } };
    inputs[0][0] = 0.25f;
    inputs[0][1] = 0.50f;
    inputs[0][2] = 0.75f;
    PBGfxTevProgram program;
    CHECK(pb_gfx_combiner_compile_tev(&plan, inputs, &program));
    CHECK(program.stage_count == 1U);
    CHECK(program.stages[0].rgb_function == PB_GFX_TEV_INTERPOLATE);
    CHECK(program.stages[0].rgb_sources[0] == PB_GFX_TEV_TEXTURE0);
    CHECK(program.stages[0].rgb_sources[1] == PB_GFX_TEV_CONSTANT);
    CHECK(program.stages[0].rgb_sources[2] == PB_GFX_TEV_SHADE);
    CHECK(program.stages[0].constant[0] == 0.25f);
    CHECK(program.stages[0].constant[1] == 0.50f);
    CHECK(program.stages[0].constant[2] == 0.75f);

    /* One-cycle TEXEL1 aliases TEXEL0 in the canonical generator. */
    cycles[0][0][0] = 31U;
    cycles[0][0][1] = 31U;
    cycles[0][0][2] = 31U;
    cycles[0][0][3] = 2U;
    cycles[0][1][0] = 7U;
    cycles[0][1][1] = 7U;
    cycles[0][1][2] = 7U;
    cycles[0][1][3] = 2U;
    encode_combine_words(cycles, &word0, &word1);
    CHECK(pb_fast3d_generate_combiner(
        &semantic, word0, word1,
        pb_gfx_shader_option(PB_GFX_OPT_ALPHA)));
    CHECK((semantic.shader_id0 & UINT64_C(0xF000)) ==
          ((uint64_t)PB_GFX_SHADER_TEXEL0 << 12U));
    CHECK(semantic.used_textures[0]);
    CHECK(!semantic.used_textures[1]);

    /* In two-cycle mode the logical TEXEL0 source swaps physical texture
     * units in cycle two, matching Fast3D's RDP pipeline semantics. */
    const uint8_t two_cycle[2][2][4] = {
        { { 31U, 31U, 31U, 1U }, { 7U, 7U, 7U, 1U } },
        { { 0U, 31U, 1U, 31U }, { 0U, 7U, 1U, 7U } },
    };
    encode_combine_words(two_cycle, &word0, &word1);
    CHECK(pb_fast3d_generate_combiner(
        &semantic, word0, word1,
        pb_gfx_shader_option(PB_GFX_OPT_ALPHA) |
            pb_gfx_shader_option(PB_GFX_OPT_TWO_CYCLE)));
    CHECK(semantic.used_textures[0]);
    CHECK(semantic.used_textures[1]);
    CHECK(pb_gfx_combiner_decode(&plan, semantic.shader_id0,
                                 semantic.shader_id1));
    CHECK(plan.vertex_stride_floats == 9U);
    CHECK(pb_gfx_combiner_compile_tev(&plan, inputs, &program));
    CHECK(program.stage_count == 2U);
    CHECK(program.stages[0].rgb_sources[0] == PB_GFX_TEV_TEXTURE0);
    CHECK(program.stages[1].rgb_function == PB_GFX_TEV_INTERPOLATE);
    CHECK(program.stages[1].rgb_sources[0] == PB_GFX_TEV_PREVIOUS);
    CHECK(program.stages[1].rgb_sources[1] == PB_GFX_TEV_CONSTANT);
    CHECK(program.stages[1].rgb_sources[2] == PB_GFX_TEV_TEXTURE1);

    CHECK(!pb_fast3d_generate_combiner(NULL, word0, word1, 0U));
    return true;
}

static bool test_combiner_decode(void) {
    PBGfxCombinerPlan plan;
    CHECK(pb_gfx_combiner_decode(&plan, shade_shader_id(),
                                 pb_gfx_shader_option(PB_GFX_OPT_ALPHA)));
    CHECK(plan.supported);
    CHECK(plan.mode == PB_GFX_COMBINER_SHADE);
    CHECK(plan.uses_shade);
    CHECK(plan.uses_alpha);
    CHECK(!plan.used_textures[0]);
    CHECK(plan.vertex_stride_floats == 9);

    CHECK(pb_gfx_combiner_decode(
        &plan, texture_shade_shader_id(),
        pb_gfx_shader_option(PB_GFX_OPT_ALPHA)));
    CHECK(plan.mode == PB_GFX_COMBINER_TEXTURE0_SHADE);
    CHECK(plan.used_textures[0]);
    CHECK(!plan.used_textures[1]);
    CHECK(plan.vertex_stride_floats == 11);

    CHECK(!pb_gfx_combiner_decode(
        &plan, texture_shade_shader_id(),
        pb_gfx_shader_option(PB_GFX_OPT_ALPHA) |
            pb_gfx_shader_option(PB_GFX_OPT_TEXEL0_CLAMP_S)));
    CHECK((plan.reject_reasons & PB_GFX_REJECT_OPTION) != 0);

    CHECK(pb_gfx_combiner_decode(
        &plan, texture_shade_shader_id(),
        pb_gfx_shader_option(PB_GFX_OPT_TWO_CYCLE)));
    CHECK(plan.two_cycle);
    CHECK(plan.used_textures[0]);
    CHECK(plan.used_textures[1]);

    const uint64_t texture1 =
        pack_formula(0, 0, 0, PB_GFX_SHADER_TEXEL1, 0) |
        pack_formula(0, 0, 0, PB_GFX_SHADER_TEXEL1_ALPHA, 16);
    CHECK(pb_gfx_combiner_decode(&plan, texture1, 0));
    CHECK(plan.used_textures[1]);
    CHECK(plan.vertex_stride_floats == 7U);

    CHECK(!pb_gfx_combiner_decode(
        &plan, shade_shader_id(), UINT64_C(2) << PB_GFX_OPT_PRISM_SHADER));
    CHECK((plan.reject_reasons & PB_GFX_REJECT_OPTION) != 0);

    const uint64_t unsupported_formula =
        pack_formula(PB_GFX_SHADER_INPUT_1, PB_GFX_SHADER_INPUT_2,
                     PB_GFX_SHADER_INPUT_3, PB_GFX_SHADER_INPUT_4, 0) |
        pack_formula(0, 0, 0, PB_GFX_SHADER_ONE, 16);
    CHECK(pb_gfx_combiner_decode(&plan, unsupported_formula, 0));
    CHECK(plan.num_inputs == 4);
    float inputs[6][4] = { { 0 } };
    PBGfxTevProgram program;
    CHECK(pb_gfx_combiner_compile_tev(&plan, inputs, &program));
    CHECK(program.stage_count == 1U);

    const uint64_t noise_formula =
        pack_formula(0, 0, 0, PB_GFX_SHADER_NOISE, 0) |
        pack_formula(0, 0, 0, PB_GFX_SHADER_ONE, 16);
    CHECK(!pb_gfx_combiner_decode(&plan, noise_formula, 0));
    CHECK((plan.reject_reasons & PB_GFX_REJECT_FORMULA) != 0);
    CHECK(!pb_gfx_combiner_decode(NULL, 0, 0));
    CHECK(pb_gfx_shader_option((PBGfxShaderOption)99) == 0);
    return true;
}

static bool test_draw_contract(void) {
    PBGfxCombinerPlan plan;
    CHECK(pb_gfx_combiner_decode(&plan, shade_shader_id(),
                                 pb_gfx_shader_option(PB_GFX_OPT_ALPHA)));
    float triangle[27] = { 0 };
    size_t bytes = 0;
    CHECK(pb_gfx_validate_draw(&plan, triangle, 27, 1, &bytes));
    CHECK(bytes == sizeof(triangle));
    CHECK(!pb_gfx_validate_draw(&plan, triangle, 26, 1, &bytes));
    CHECK(bytes == 0);
    CHECK(!pb_gfx_validate_draw(&plan, NULL, 27, 1, &bytes));
    CHECK(!pb_gfx_validate_draw(&plan, triangle, 27, 0, &bytes));
    CHECK(!pb_gfx_validate_draw(&plan, triangle, 27,
                                PB_GFX_MAX_STREAM_TRIANGLES + 1U, &bytes));
    CHECK(!pb_gfx_validate_draw(&plan, triangle, 27, 1, NULL));
    return true;
}

static bool test_frame_and_rectangles(void) {
    PBGfxBridge bridge;
    pb_gfx_bridge_init(&bridge);
    CHECK(bridge.active);
    CHECK(pb_gfx_bridge_set_viewport(&bridge, 0, 0, 400, 240));
    CHECK(pb_gfx_bridge_set_scissor(&bridge, 40, 30, 320, 180));
    CHECK(bridge.viewport_valid);
    CHECK(bridge.scissor_valid);
    CHECK(!pb_gfx_bridge_set_viewport(&bridge, -1, 0, 400, 240));
    CHECK(!pb_gfx_bridge_set_scissor(&bridge, 399, 0, 2, 1));
    CHECK(pb_gfx_bridge_start_frame(&bridge));
    CHECK(!pb_gfx_bridge_start_frame(&bridge));
    CHECK(pb_gfx_bridge_end_frame(&bridge, true));
    CHECK(!pb_gfx_bridge_end_frame(&bridge, true));
    pb_gfx_bridge_set_active(&bridge, false);
    CHECK(!pb_gfx_bridge_start_frame(&bridge));
    pb_gfx_bridge_set_active(&bridge, true);
    CHECK(pb_gfx_bridge_start_frame(&bridge));
    pb_gfx_bridge_set_active(&bridge, false);
    CHECK(!bridge.frame_open);
    CHECK(bridge.stats.frame_failures == 1);
    return true;
}

static bool test_texture_registry(void) {
    PBGfxBridge bridge;
    pb_gfx_bridge_init(&bridge);
    uint32_t ids[PB_GFX_MAX_TEXTURES];
    for (size_t index = 0; index < PB_GFX_MAX_TEXTURES; index++) {
        ids[index] = pb_gfx_bridge_new_texture(&bridge);
        CHECK(ids[index] == index + 1U);
    }
    CHECK(pb_gfx_bridge_new_texture(&bridge) == 0);
    CHECK(pb_gfx_bridge_select_texture(&bridge, 0, ids[0]));
    CHECK(!pb_gfx_bridge_select_texture(&bridge, 2, ids[0]));
    CHECK(!pb_gfx_bridge_select_texture(&bridge, 0, 9999));
    CHECK(pb_gfx_bridge_upload_texture(&bridge, ids[0], 8, 8));
    CHECK(bridge.stats.texture_bytes == 256);
    CHECK(pb_gfx_bridge_upload_texture(&bridge, ids[0], 16, 8));
    CHECK(bridge.stats.texture_bytes == 512);
    CHECK(!pb_gfx_bridge_upload_texture(&bridge, ids[0], 12, 8));
    CHECK(pb_gfx_bridge_find_texture(&bridge, ids[0])->uploaded);
    CHECK(pb_gfx_bridge_delete_texture(&bridge, ids[0]));
    CHECK(bridge.selected_textures[0] == 0);
    CHECK(bridge.stats.texture_bytes == 0);
    CHECK(!pb_gfx_bridge_delete_texture(&bridge, ids[0]));
    const uint32_t replacement = pb_gfx_bridge_new_texture(&bridge);
    CHECK(replacement == PB_GFX_MAX_TEXTURES + 1U);
    return true;
}

static bool test_integrated_draw_accounting(void) {
    PBGfxBridge bridge;
    PBGfxCombinerPlan plan;
    pb_gfx_bridge_init(&bridge);
    CHECK(pb_gfx_combiner_decode(
        &plan, texture_shade_shader_id(),
        pb_gfx_shader_option(PB_GFX_OPT_ALPHA)));
    CHECK(pb_gfx_bridge_record_shader(&bridge, &plan));
    const uint32_t texture = pb_gfx_bridge_new_texture(&bridge);
    CHECK(texture != 0);
    CHECK(pb_gfx_bridge_select_texture(&bridge, 0, texture));
    float triangle[33];
    memset(triangle, 0, sizeof(triangle));
    CHECK(pb_gfx_bridge_start_frame(&bridge));
    CHECK(!pb_gfx_bridge_record_draw(&bridge, &plan, triangle, 33, 1));
    CHECK(pb_gfx_bridge_upload_texture(&bridge, texture, 8, 8));
    CHECK(pb_gfx_bridge_record_draw(&bridge, &plan, triangle, 33, 1));
    CHECK(pb_gfx_bridge_end_frame(&bridge, true));
    CHECK(bridge.stats.draw_calls == 1);
    CHECK(bridge.stats.triangles == 1);
    CHECK(bridge.stats.vertices == 3);
    CHECK(bridge.stats.streamed_bytes == sizeof(triangle));
    CHECK(bridge.stats.stream_peak_bytes == sizeof(triangle));
    CHECK(bridge.stats.frames_presented == 1);
    CHECK(bridge.stats.rejected_commands >= 1);
    pb_gfx_bridge_clear_shaders(&bridge);
    CHECK(bridge.stats.shaders_live == 0);
    return true;
}

int main(void) {
    if (!test_fast3d_semantics_and_tev() || !test_combiner_decode() ||
        !test_draw_contract() ||
        !test_frame_and_rectangles() || !test_texture_registry() ||
        !test_integrated_draw_accounting()) {
        return EXIT_FAILURE;
    }
    printf("M10 graphics bridge: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
