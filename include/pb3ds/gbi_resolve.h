#pragma once

#include "pb3ds/gbi_command_span.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PB_GBI_OP_VTX 0x01U
#define PB_GBI_OP_DL 0xDEU
#define PB_GBI_OP_ENDDL 0xDFU
#define PB_GBI_DL_NOPUSH 0x01U
#define PB_GBI_RESOLVE_DEPTH_LIMIT 32U
#define PB_GBI_RESOLVE_COMMAND_LIMIT 65536U

/*
 * Host tests and the 3DS walker share this packet layout. On ARM11, Gfx is
 * two 32-bit words, matching uintptr_t.
 */
typedef struct PBGbiPacket {
    struct {
        uintptr_t w0;
        uintptr_t w1;
    } words;
} PBGbiPacket;

typedef uint8_t (*PBGbiOtrSigCheck)(const char *data);
typedef void *(*PBGbiResourceGet)(const char *name);

/* True when a GBI pointer can be dereferenced on this host. Rejects NULL,
 * low integers, and leftover N64 KSEG addresses that data-abort on 3DS. */
static inline bool pb_gbi_host_pointer_ok(uintptr_t address) {
    if (address < 0x10000U) return false;
    /*
     * Leftover N64 KSEG0/KSEG1/KSSEG/KSEG3 pointers (0x8xxxxxxx-0xFxxxxxxx)
     * data-abort on ARM11. 3DS user pointers never live in that half. On
     * 64-bit hosts only the 32-bit leftover encoding is rejected.
     */
#if UINTPTR_MAX > 0xFFFFFFFFU
    if (address <= 0xFFFFFFFFU && address >= 0x80000000U) return false;
#else
    if (address >= 0x80000000U) return false;
#endif
    return true;
}

static inline void pb_gbi_resolve_vtx_walk(
    PBGbiPacket *displayList, unsigned int depth, size_t *seen,
    PBGbiOtrSigCheck otr_sig_check, PBGbiResourceGet resource_get) {
    if (displayList == NULL || seen == NULL || otr_sig_check == NULL ||
        resource_get == NULL || depth > PB_GBI_RESOLVE_DEPTH_LIMIT) {
        return;
    }
    for (PBGbiPacket *command = displayList;
         *seen < PB_GBI_RESOLVE_COMMAND_LIMIT;) {
        const unsigned int opcode =
            (unsigned int)(command->words.w0 >> 24U) & 0xFFU;
        if (opcode == PB_GBI_OP_ENDDL) return;
        if (opcode == PB_GBI_OP_VTX) {
            const uintptr_t w1 = command->words.w1;
            /* PaperBoat skips odd tagged pointers so OTRSigCheck never
             * strncmp's unaligned or already-resolved native data. */
            if (pb_gbi_host_pointer_ok(w1) && (w1 & 1U) == 0U &&
                otr_sig_check((const char *)w1)) {
                void *data = resource_get((const char *)w1);
                if (data != NULL) command->words.w1 = (uintptr_t)data;
            }
        } else if (opcode == PB_GBI_OP_DL) {
            const unsigned int pushFlag =
                (unsigned int)(command->words.w0 >> 16U) & 0xFFU;
            const uintptr_t subAddress = command->words.w1;
            const size_t span = pb_gbi_command_span(opcode);
            *seen += span;
            if (pb_gbi_host_pointer_ok(subAddress) &&
                !otr_sig_check((const char *)subAddress)) {
                pb_gbi_resolve_vtx_walk((PBGbiPacket *)subAddress,
                                        depth + 1U, seen, otr_sig_check,
                                        resource_get);
                if (pushFlag == PB_GBI_DL_NOPUSH) return;
            }
            command += span;
            continue;
        }
        const size_t span = pb_gbi_command_span(opcode);
        command += span;
        *seen += span;
    }
}

static inline void pb_gbi_resolve_vtx_in_static_dl(
    PBGbiPacket *displayList, PBGbiOtrSigCheck otr_sig_check,
    PBGbiResourceGet resource_get) {
    size_t seen = 0U;
    pb_gbi_resolve_vtx_walk(displayList, 0U, &seen, otr_sig_check,
                            resource_get);
}

#ifdef __cplusplus
}
#endif
