#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * F3DEX2 / RDP command bytes (public Fast3D opcode map). PaperBoat is built
 * with F3DEX_GBI_2. This header does not vendor SGI gbi.h.
 */
#define PB_F3D_G_NOOP 0x00U
#define PB_F3D_G_VTX 0x01U
#define PB_F3D_G_MODIFYVTX 0x02U
#define PB_F3D_G_CULLDL 0x03U
#define PB_F3D_G_BRANCH_Z 0x04U
#define PB_F3D_G_TRI1 0x05U
#define PB_F3D_G_TRI2 0x06U
#define PB_F3D_G_QUAD 0x07U
#define PB_F3D_G_LINE3D 0x08U
#define PB_F3D_G_SPECIAL_1 0xd5U
#define PB_F3D_G_DMA_IO 0xd6U
#define PB_F3D_G_TEXTURE 0xd7U
#define PB_F3D_G_POPMTX 0xd8U
#define PB_F3D_G_GEOMETRYMODE 0xd9U
#define PB_F3D_G_MTX 0xdaU
#define PB_F3D_G_MOVEWORD 0xdbU
#define PB_F3D_G_MOVEMEM 0xdcU
#define PB_F3D_G_LOAD_UCODE 0xddU
#define PB_F3D_G_DL 0xdeU
#define PB_F3D_G_ENDDL 0xdfU
#define PB_F3D_G_SPNOOP 0xe0U
#define PB_F3D_G_RDPHALF_1 0xe1U
#define PB_F3D_G_SETOTHERMODE_L 0xe2U
#define PB_F3D_G_SETOTHERMODE_H 0xe3U
#define PB_F3D_G_TEXRECT 0xe4U
#define PB_F3D_G_TEXRECTFLIP 0xe5U
#define PB_F3D_G_RDPLOADSYNC 0xe6U
#define PB_F3D_G_RDPPIPESYNC 0xe7U
#define PB_F3D_G_RDPTILESYNC 0xe8U
#define PB_F3D_G_RDPFULLSYNC 0xe9U
#define PB_F3D_G_SETKEYGB 0xeaU
#define PB_F3D_G_SETKEYR 0xebU
#define PB_F3D_G_SETCONVERT 0xecU
#define PB_F3D_G_SETSCISSOR 0xedU
#define PB_F3D_G_SETPRIMDEPTH 0xeeU
#define PB_F3D_G_RDPSETOTHERMODE 0xefU
#define PB_F3D_G_LOADTLUT 0xf0U
#define PB_F3D_G_RDPHALF_2 0xf1U
#define PB_F3D_G_SETTILESIZE 0xf2U
#define PB_F3D_G_LOADBLOCK 0xf3U
#define PB_F3D_G_LOADTILE 0xf4U
#define PB_F3D_G_SETTILE 0xf5U
#define PB_F3D_G_FILLRECT 0xf6U
#define PB_F3D_G_SETFILLCOLOR 0xf7U
#define PB_F3D_G_SETFOGCOLOR 0xf8U
#define PB_F3D_G_SETBLENDCOLOR 0xf9U
#define PB_F3D_G_SETPRIMCOLOR 0xfaU
#define PB_F3D_G_SETENVCOLOR 0xfbU
#define PB_F3D_G_SETCOMBINE 0xfcU
#define PB_F3D_G_SETTIMG 0xfdU
#define PB_F3D_G_SETZIMG 0xfeU
#define PB_F3D_G_SETCIMG 0xffU

#define PB_F3D_G_ZBUFFER 0x00000001U
#define PB_F3D_G_SHADE 0x00000004U
#define PB_F3D_G_CULL_FRONT 0x00000200U
#define PB_F3D_G_CULL_BACK 0x00000400U
#define PB_F3D_G_FOG 0x00010000U
#define PB_F3D_G_LIGHTING 0x00020000U
#define PB_F3D_G_TEXTURE_GEN 0x00040000U
#define PB_F3D_G_SHADING_SMOOTH 0x00200000U
#define PB_F3D_G_CLIPPING 0x00800000U

#define PB_F3D_MTX_MODELVIEW 0x00U
#define PB_F3D_MTX_PROJECTION 0x01U
#define PB_F3D_MTX_MUL 0x00U
#define PB_F3D_MTX_LOAD 0x02U
#define PB_F3D_MTX_NOPUSH 0x00U
#define PB_F3D_MTX_PUSH 0x04U

#define PB_F3D_VTX_MAX 32U
#define PB_F3D_DL_STACK 16U
#define PB_F3D_MTX_STACK 16U
#define PB_F3D_SEGMENTS 8U
#define PB_F3D_CMD_LIMIT 4096U

#define PB_F3D_FMT_RGBA 0U
#define PB_F3D_FMT_YUV 1U
#define PB_F3D_FMT_CI 2U
#define PB_F3D_FMT_IA 3U
#define PB_F3D_FMT_I 4U

#define PB_F3D_SIZ_4B 0U
#define PB_F3D_SIZ_8B 1U
#define PB_F3D_SIZ_16B 2U
#define PB_F3D_SIZ_32B 3U

typedef struct {
    uint32_t w0;
    uint32_t w1;
} PBGfx;

typedef struct {
    int16_t ob[3];
    uint16_t flag;
    int16_t tc[2];
    uint8_t cn[4];
} PBVtx;

typedef struct {
    uint32_t commands;
    uint32_t triangles;
    uint32_t texrects;
    uint32_t vtx;
    uint32_t textures;
    uint32_t combiners;
    uint32_t unsupported;
    uint32_t last_unsupported;
    uint32_t dl_depth;
    uint32_t mtx_push;
    uint32_t mtx_pop;
    uint32_t scissor_clamps;
    bool depth_test;
    bool invert_y_applied;
} PBF3dDiag;

void pb_f3d_reset(void);
void pb_f3d_set_segment(unsigned slot, const void *pointer);
void pb_f3d_execute(const void *display_list, size_t bytes_hint);
void pb_f3d_query(PBF3dDiag *diag);
const char *pb_f3d_opcode_name(uint8_t opcode);

#ifdef __cplusplus
}
#endif
