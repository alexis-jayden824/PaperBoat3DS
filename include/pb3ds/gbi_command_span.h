#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Number of Gfx packets consumed by one F3DEX2 / Fast3D opcode, including the
 * opcode itself. Static-list walkers must skip the extra words; treating a
 * TEXRECT payload as opcodes can miss G_ENDDL and hang.
 */
static inline size_t pb_gbi_command_span(unsigned int opcode) {
    switch (opcode) {
        case 0xE4U: /* G_TEXRECT */
        case 0xE5U: /* G_TEXRECTFLIP */
        case 0x37U: /* G_TEXRECT_WIDE */
        case 0x3CU: /* G_IMAGERECT */
            return 3U;
        case 0x20U: /* G_SETTIMG_OTR_HASH */
        case 0x31U: /* G_DL_OTR_HASH */
        case 0x32U: /* G_VTX_OTR_HASH */
        case 0x33U: /* G_MARKER */
        case 0x35U: /* G_BRANCH_Z_OTR */
        case 0x36U: /* G_MTX_OTR */
        case 0x38U: /* G_FILLWIDERECT */
        case 0x42U: /* G_MOVEMEM_HASH */
        case 0x47U: /* G_LOADBLOCK_WIDE */
            return 2U;
        default:
            return 1U;
    }
}

#ifdef __cplusplus
}
#endif
