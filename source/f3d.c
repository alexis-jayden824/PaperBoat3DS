#include "pb3ds/f3d.h"

#include "pb3ds/gfx.h"
#include "pb3ds/log.h"
#include "pb3ds/tev.h"
#include "pb3ds/tex.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    float x;
    float y;
    float z;
    float w;
    float s;
    float t;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    bool valid;
} PBClipVtx;

typedef struct {
    const void *img;
    uint8_t fmt;
    uint8_t siz;
    uint16_t width;
    uint16_t height;
    uint16_t uls;
    uint16_t ult;
    uint16_t lrs;
    uint16_t lrt;
} PBTile;

static PBF3dDiag g_diag;
static PBClipVtx g_vtx[PB_F3D_VTX_MAX];
static const PBGfx *g_dl_stack[PB_F3D_DL_STACK];
static unsigned g_dl_sp;
static float g_mv[PB_F3D_MTX_STACK][16];
static float g_pj[PB_F3D_MTX_STACK][16];
static unsigned g_mv_sp;
static unsigned g_pj_sp;
static uint32_t g_geom;
static uint32_t g_combine0;
static uint32_t g_combine1;
static uint32_t g_prim;
static uint32_t g_env;
static uint32_t g_fog;
static uint32_t g_blend;
static uint32_t g_fill;
static const void *g_timg;
static uint8_t g_timg_fmt;
static uint8_t g_timg_siz;
static uint16_t g_timg_width;
static PBTile g_tiles[8];
static uint16_t g_tlut[256];
static unsigned g_tlut_count;
static int g_scissor_x0;
static int g_scissor_y0;
static int g_scissor_x1;
static int g_scissor_y1;
static uint32_t g_other_h;
static uint32_t g_other_l;
static const void *g_segments[PB_F3D_SEGMENTS];

static void identity(float *m) {
    unsigned i;
    memset(m, 0, 16U * sizeof(float));
    for (i = 0; i < 4U; i++) {
        m[i * 4U + i] = 1.0f;
    }
}

static void mul4(const float *a, const float *b, float *out) {
    unsigned r;
    unsigned c;
    unsigned k;
    float tmp[16];

    for (r = 0; r < 4U; r++) {
        for (c = 0; c < 4U; c++) {
            float s = 0.0f;
            for (k = 0; k < 4U; k++) {
                s += a[r * 4U + k] * b[k * 4U + c];
            }
            tmp[r * 4U + c] = s;
        }
    }
    memcpy(out, tmp, sizeof(tmp));
}

static void load_mtx(const uint32_t *src, float *dst) {
    unsigned i;
    /* N64 Mtx: 8 s16 integer + 8 u16 fraction, big-endian 16-bit cells. */
    const uint8_t *p = (const uint8_t *)src;

    if (src == NULL) {
        identity(dst);
        return;
    }
    for (i = 0; i < 16U; i++) {
        int16_t ip = (int16_t)((uint16_t)((p[i * 2U] << 8) | p[i * 2U + 1U]));
        uint16_t fp = (uint16_t)((p[32 + i * 2U] << 8) | p[32 + i * 2U + 1U]);
        dst[i] = (float)ip + (float)fp / 65536.0f;
    }
}

static void reset_state(void) {
    unsigned i;

    memset(g_vtx, 0, sizeof(g_vtx));
    g_dl_sp = 0U;
    g_mv_sp = 0U;
    g_pj_sp = 0U;
    identity(g_mv[0]);
    identity(g_pj[0]);
    g_geom = PB_F3D_G_SHADE | PB_F3D_G_SHADING_SMOOTH | PB_F3D_G_ZBUFFER;
    g_combine0 = 0U;
    g_combine1 = 0U;
    g_prim = 0xFFFFFFFFU;
    g_env = 0U;
    g_fog = 0U;
    g_blend = 0U;
    g_fill = 0U;
    g_timg = NULL;
    g_timg_fmt = 0U;
    g_timg_siz = 0U;
    g_timg_width = 0U;
    memset(g_tiles, 0, sizeof(g_tiles));
    memset(g_tlut, 0, sizeof(g_tlut));
    g_tlut_count = 0U;
    g_scissor_x0 = 0;
    g_scissor_y0 = 0;
    g_scissor_x1 = (int)PB_GFX_SOURCE_WIDTH;
    g_scissor_y1 = (int)PB_GFX_SOURCE_HEIGHT;
    g_other_h = 0U;
    g_other_l = 0U;
    for (i = 1; i < PB_F3D_MTX_STACK; i++) {
        identity(g_mv[i]);
        identity(g_pj[i]);
    }
}

static const void *resolve_ptr(uint32_t w1) {
    if (w1 < PB_F3D_SEGMENTS) {
        return g_segments[w1];
    }
    return (const void *)(uintptr_t)w1;
}

static void unsupported(uint8_t op) {
    static uint32_t seen[(256U + 31U) / 32U];
    unsigned word = (unsigned)op / 32U;
    unsigned bit = 1U << ((unsigned)op % 32U);

    g_diag.unsupported++;
    g_diag.last_unsupported = op;
    if ((seen[word] & bit) == 0U) {
        seen[word] |= bit;
        pb_log(PB_LOG_WARNING, "unsupported", pb_f3d_opcode_name(op));
    }
}

static void clamp_scissor(int *x0, int *y0, int *x1, int *y1) {
    if (*x1 < *x0) {
        int t = *x0;
        *x0 = *x1;
        *x1 = t;
        g_diag.scissor_clamps++;
    }
    if (*y1 < *y0) {
        int t = *y0;
        *y0 = *y1;
        *y1 = t;
        g_diag.scissor_clamps++;
    }
    if (*x0 < 0) {
        *x0 = 0;
        g_diag.scissor_clamps++;
    }
    if (*y0 < 0) {
        *y0 = 0;
        g_diag.scissor_clamps++;
    }
    if (*x1 > (int)PB_GFX_SOURCE_WIDTH) {
        *x1 = (int)PB_GFX_SOURCE_WIDTH;
        g_diag.scissor_clamps++;
    }
    if (*y1 > (int)PB_GFX_SOURCE_HEIGHT) {
        *y1 = (int)PB_GFX_SOURCE_HEIGHT;
        g_diag.scissor_clamps++;
    }
}

static void xform_vertex(const PBVtx *in, PBClipVtx *out) {
    float x = (float)in->ob[0];
    float y = (float)in->ob[1];
    float z = (float)in->ob[2];
    const float *mv = g_mv[g_mv_sp];
    const float *pj = g_pj[g_pj_sp];
    float e[4];
    float c[4];
    unsigned i;

    e[0] = mv[0] * x + mv[1] * y + mv[2] * z + mv[3];
    e[1] = mv[4] * x + mv[5] * y + mv[6] * z + mv[7];
    e[2] = mv[8] * x + mv[9] * y + mv[10] * z + mv[11];
    e[3] = mv[12] * x + mv[13] * y + mv[14] * z + mv[15];
    c[0] = pj[0] * e[0] + pj[1] * e[1] + pj[2] * e[2] + pj[3] * e[3];
    c[1] = pj[4] * e[0] + pj[5] * e[1] + pj[6] * e[2] + pj[7] * e[3];
    c[2] = pj[8] * e[0] + pj[9] * e[1] + pj[10] * e[2] + pj[11] * e[3];
    c[3] = pj[12] * e[0] + pj[13] * e[1] + pj[14] * e[2] + pj[15] * e[3];
    (void)i;
    out->x = c[0];
    out->y = c[1];
    out->z = c[2];
    out->w = c[3] != 0.0f ? c[3] : 1.0f;
    out->s = (float)in->tc[0] / 32.0f;
    out->t = pb_gfx_upright_t((float)in->tc[1] / 32.0f);
    out->r = in->cn[0];
    out->g = in->cn[1];
    out->b = in->cn[2];
    out->a = in->cn[3];
    out->valid = true;
}

static void submit_tri(unsigned i0, unsigned i1, unsigned i2) {
    PBGfxVertex verts[3];
    unsigned idx[3];
    unsigned n;

    idx[0] = i0;
    idx[1] = i1;
    idx[2] = i2;
    if (i0 >= PB_F3D_VTX_MAX || i1 >= PB_F3D_VTX_MAX || i2 >= PB_F3D_VTX_MAX) {
        return;
    }
    if (!g_vtx[i0].valid || !g_vtx[i1].valid || !g_vtx[i2].valid) {
        return;
    }
    for (n = 0; n < 3U; n++) {
        const PBClipVtx *v = &g_vtx[idx[n]];
        verts[n].x = v->x;
        verts[n].y = v->y;
        verts[n].z = v->z;
        verts[n].w = v->w;
        verts[n].s = v->s;
        verts[n].t = v->t;
        verts[n].color = ((uint32_t)v->r << 24) | ((uint32_t)v->g << 16) |
                         ((uint32_t)v->b << 8) | (uint32_t)v->a;
    }
    (void)pb_gfx_submit_vertices(verts, 3U);
    g_diag.triangles++;
}

static void bind_tile_texture(unsigned tile) {
    PBTexKey key;
    const uint8_t *rgba;
    PBTile *t = &g_tiles[tile & 7U];
    uint16_t w;
    uint16_t h;

    if (t->img == NULL && g_timg == NULL) {
        return;
    }
    w = t->width != 0U ? t->width : (g_timg_width != 0U ? g_timg_width : 1U);
    h = t->height != 0U ? t->height : 1U;
    if (w > PB_TEX_DIM_MAX) {
        w = PB_TEX_DIM_MAX;
    }
    if (h > PB_TEX_DIM_MAX) {
        h = PB_TEX_DIM_MAX;
    }
    memset(&key, 0, sizeof(key));
    key.img = t->img != NULL ? t->img : g_timg;
    key.fmt = t->fmt;
    key.siz = t->siz;
    key.width = w;
    key.height = h;
    key.tlut_crc = (uint16_t)g_tlut_count;
    rgba = pb_tex_cache_get(&key, key.img,
                            pb_tex_src_bytes(key.fmt, key.siz, w, h), g_tlut,
                            g_tlut_count);
    if (rgba != NULL) {
        PBGfxTexDesc desc;
        desc.width = w;
        desc.height = h;
        desc.format = PB_GFX_FMT_RGBA8888;
        desc.slot = 0U;
        /* Decode path is the source of truth; POT upload is optional. */
        if ((w & (w - 1U)) == 0U && (h & (h - 1U)) == 0U) {
            (void)pb_gfx_tex_upload(&desc, rgba, (size_t)w * (size_t)h * 4U);
        }
        g_diag.textures++;
    }
}

static void run_dl(const PBGfx *dl, unsigned budget);

static void opcode(const PBGfx **pp, unsigned *left) {
    const PBGfx *p = *pp;
    uint8_t op = (uint8_t)(p->w0 >> 24);
    g_diag.commands++;

    switch (op) {
        case PB_F3D_G_NOOP:
        case PB_F3D_G_SPNOOP:
        case PB_F3D_G_RDPLOADSYNC:
        case PB_F3D_G_RDPPIPESYNC:
        case PB_F3D_G_RDPTILESYNC:
        case PB_F3D_G_RDPFULLSYNC:
            break;
        case PB_F3D_G_ENDDL:
            *left = 0U;
            return;
        case PB_F3D_G_VTX: {
            unsigned n = (p->w0 >> 12) & 0xFFU;
            unsigned end = (p->w0 >> 1) & 0x7FU;
            unsigned v0 = end > n ? end - n : 0U;
            const PBVtx *src = (const PBVtx *)resolve_ptr(p->w1);
            unsigned i;
            g_diag.vtx += n;
            if (src != NULL) {
                for (i = 0; i < n && v0 + i < PB_F3D_VTX_MAX; i++) {
                    xform_vertex(&src[i], &g_vtx[v0 + i]);
                }
            }
            break;
        }
        case PB_F3D_G_TRI1: {
            unsigned v0 = (p->w0 >> 16) & 0xFFU;
            unsigned v1 = (p->w0 >> 8) & 0xFFU;
            unsigned v2 = p->w0 & 0xFFU;
            submit_tri(v0 / 2U, v1 / 2U, v2 / 2U);
            break;
        }
        case PB_F3D_G_TRI2: {
            unsigned a0 = (p->w0 >> 16) & 0xFFU;
            unsigned a1 = (p->w0 >> 8) & 0xFFU;
            unsigned a2 = p->w0 & 0xFFU;
            unsigned b0 = (p->w1 >> 16) & 0xFFU;
            unsigned b1 = (p->w1 >> 8) & 0xFFU;
            unsigned b2 = p->w1 & 0xFFU;
            submit_tri(a0 / 2U, a1 / 2U, a2 / 2U);
            submit_tri(b0 / 2U, b1 / 2U, b2 / 2U);
            break;
        }
        case PB_F3D_G_QUAD:
            submit_tri((p->w1 >> 16) & 0xFFU, (p->w1 >> 8) & 0xFFU, p->w1 & 0xFFU);
            break;
        case PB_F3D_G_DL: {
            const PBGfx *child = (const PBGfx *)resolve_ptr(p->w1);
            unsigned push = (p->w0 >> 16) & 0xFFU;
            if (child == NULL) {
                break;
            }
            if (push == 0U) {
                if (g_dl_sp + 1U >= PB_F3D_DL_STACK) {
                    pb_log(PB_LOG_ERROR, "f3d", "dl stack overflow");
                    break;
                }
                g_dl_stack[g_dl_sp++] = p + 1;
                g_diag.dl_depth = g_dl_sp > g_diag.dl_depth ? g_dl_sp : g_diag.dl_depth;
                run_dl(child, *left);
            } else {
                *pp = child - 1;
            }
            break;
        }
        case PB_F3D_G_MTX: {
            unsigned param = p->w0 & 0xFFU;
            float loaded[16];
            float *stack = (param & PB_F3D_MTX_PROJECTION) ? g_pj[g_pj_sp] : g_mv[g_mv_sp];
            load_mtx((const uint32_t *)resolve_ptr(p->w1), loaded);
            if ((param & PB_F3D_MTX_PUSH) != 0U) {
                if ((param & PB_F3D_MTX_PROJECTION) != 0U) {
                    if (g_pj_sp + 1U >= PB_F3D_MTX_STACK) {
                        pb_log(PB_LOG_ERROR, "matrix", "proj overflow");
                        break;
                    }
                    memcpy(g_pj[g_pj_sp + 1U], g_pj[g_pj_sp], sizeof(float) * 16U);
                    g_pj_sp++;
                    stack = g_pj[g_pj_sp];
                } else {
                    if (g_mv_sp + 1U >= PB_F3D_MTX_STACK) {
                        pb_log(PB_LOG_ERROR, "matrix", "mv overflow");
                        break;
                    }
                    memcpy(g_mv[g_mv_sp + 1U], g_mv[g_mv_sp], sizeof(float) * 16U);
                    g_mv_sp++;
                    stack = g_mv[g_mv_sp];
                }
                g_diag.mtx_push++;
            }
            if ((param & PB_F3D_MTX_LOAD) != 0U) {
                memcpy(stack, loaded, sizeof(loaded));
            } else {
                mul4(stack, loaded, stack);
            }
            break;
        }
        case PB_F3D_G_POPMTX: {
            unsigned n = p->w1 / 64U;
            if (n == 0U) {
                n = 1U;
            }
            if (g_mv_sp < n) {
                pb_log(PB_LOG_ERROR, "matrix", "mv underflow");
                g_mv_sp = 0U;
            } else {
                g_mv_sp -= n;
            }
            g_diag.mtx_pop++;
            break;
        }
        case PB_F3D_G_GEOMETRYMODE:
            g_geom = (g_geom & (p->w0 & 0x00FFFFFFU)) | p->w1;
            g_diag.depth_test = (g_geom & PB_F3D_G_ZBUFFER) != 0U;
            pb_gfx_set_depth_enabled(g_diag.depth_test);
            break;
        case PB_F3D_G_TEXTURE:
            break;
        case PB_F3D_G_SETTIMG:
            g_timg_fmt = (uint8_t)((p->w0 >> 21) & 7U);
            g_timg_siz = (uint8_t)((p->w0 >> 19) & 3U);
            g_timg_width = (uint16_t)((p->w0 & 0xFFFU) + 1U);
            g_timg = resolve_ptr(p->w1);
            break;
        case PB_F3D_G_SETTILE: {
            unsigned tile = (p->w1 >> 24) & 7U;
            g_tiles[tile].fmt = (uint8_t)((p->w0 >> 21) & 7U);
            g_tiles[tile].siz = (uint8_t)((p->w0 >> 19) & 3U);
            if (g_timg != NULL) {
                g_tiles[tile].img = g_timg;
            }
            break;
        }
        case PB_F3D_G_SETTILESIZE: {
            unsigned tile = (p->w1 >> 24) & 7U;
            unsigned lrs = (p->w1 >> 12) & 0xFFFU;
            unsigned lrt = p->w1 & 0xFFFU;
            g_tiles[tile].uls = (uint16_t)((p->w0 >> 12) & 0xFFFU);
            g_tiles[tile].ult = (uint16_t)(p->w0 & 0xFFFU);
            g_tiles[tile].lrs = (uint16_t)lrs;
            g_tiles[tile].lrt = (uint16_t)lrt;
            g_tiles[tile].width = (uint16_t)((lrs - g_tiles[tile].uls) / 4U + 1U);
            g_tiles[tile].height = (uint16_t)((lrt - g_tiles[tile].ult) / 4U + 1U);
            bind_tile_texture(tile);
            break;
        }
        case PB_F3D_G_LOADBLOCK:
        case PB_F3D_G_LOADTILE:
            bind_tile_texture((p->w1 >> 24) & 7U);
            break;
        case PB_F3D_G_LOADTLUT: {
            const uint16_t *src = (const uint16_t *)g_timg;
            unsigned count = ((p->w1 >> 14) & 0x3FFU) + 1U;
            unsigned i;
            if (count > 256U) {
                count = 256U;
            }
            if (src != NULL) {
                for (i = 0; i < count; i++) {
                    g_tlut[i] = src[i];
                }
                g_tlut_count = count;
            }
            break;
        }
        case PB_F3D_G_SETCOMBINE:
            g_combine0 = p->w0;
            g_combine1 = p->w1;
            g_diag.combiners++;
            pb_tev_set_combine(p->w0, p->w1);
            break;
        case PB_F3D_G_SETPRIMCOLOR:
            g_prim = p->w1;
            break;
        case PB_F3D_G_SETENVCOLOR:
            g_env = p->w1;
            break;
        case PB_F3D_G_SETFOGCOLOR:
            g_fog = p->w1;
            break;
        case PB_F3D_G_SETBLENDCOLOR:
            g_blend = p->w1;
            break;
        case PB_F3D_G_SETFILLCOLOR:
            g_fill = p->w1;
            break;
        case PB_F3D_G_SETSCISSOR: {
            int x0 = (int)((p->w0 >> 12) & 0xFFFU) / 4;
            int y0 = (int)(p->w0 & 0xFFFU) / 4;
            int x1 = (int)((p->w1 >> 12) & 0xFFFU) / 4;
            int y1 = (int)(p->w1 & 0xFFFU) / 4;
            clamp_scissor(&x0, &y0, &x1, &y1);
            g_scissor_x0 = x0;
            g_scissor_y0 = y0;
            g_scissor_x1 = x1;
            g_scissor_y1 = y1;
            pb_gfx_set_scissor_source(x0, y0, x1, y1);
            break;
        }
        case PB_F3D_G_FILLRECT:
            g_diag.texrects++;
            break;
        case PB_F3D_G_TEXRECT:
        case PB_F3D_G_TEXRECTFLIP:
            g_diag.texrects++;
            if (*left > 1U) {
                *pp += 1;
                *left -= 1U;
            }
            break;
        case PB_F3D_G_SETOTHERMODE_H:
            g_other_h = p->w1;
            break;
        case PB_F3D_G_SETOTHERMODE_L:
            g_other_l = p->w1;
            break;
        case PB_F3D_G_SETCIMG:
        case PB_F3D_G_SETZIMG:
        case PB_F3D_G_MOVEWORD:
        case PB_F3D_G_MOVEMEM:
        case PB_F3D_G_SETPRIMDEPTH:
        case PB_F3D_G_MODIFYVTX:
        case PB_F3D_G_CULLDL:
            break;
        default:
            unsupported(op);
            break;
    }
    (void)g_combine0;
    (void)g_combine1;
    (void)g_prim;
    (void)g_env;
    (void)g_fog;
    (void)g_blend;
    (void)g_fill;
    (void)g_other_h;
    (void)g_other_l;
}

static void run_dl(const PBGfx *dl, unsigned budget) {
    const PBGfx *p = dl;
    unsigned left = budget;

    while (p != NULL && left > 0U) {
        unsigned before = left;
        opcode(&p, &left);
        if (left == 0U) {
            break;
        }
        if (left == before) {
            left--;
        }
        p++;
    }
}

void pb_f3d_reset(void) {
    memset(&g_diag, 0, sizeof(g_diag));
    memset(g_segments, 0, sizeof(g_segments));
    g_diag.invert_y_applied = true;
    g_diag.depth_test = true;
    reset_state();
    pb_tex_init();
    pb_tev_reset();
}

void pb_f3d_set_segment(unsigned slot, const void *pointer) {
    if (slot < PB_F3D_SEGMENTS) {
        g_segments[slot] = pointer;
    }
}

void pb_f3d_execute(const void *display_list, size_t bytes_hint) {
    unsigned limit;

    if (display_list == NULL) {
        g_diag.commands++;
        return;
    }
    limit = PB_F3D_CMD_LIMIT;
    if (bytes_hint >= sizeof(PBGfx)) {
        unsigned from_bytes = (unsigned)(bytes_hint / sizeof(PBGfx));
        if (from_bytes < limit) {
            limit = from_bytes;
        }
    }
    run_dl((const PBGfx *)display_list, limit);
}

void pb_f3d_query(PBF3dDiag *diag) {
    if (diag == NULL) {
        return;
    }
    *diag = g_diag;
}

const char *pb_f3d_opcode_name(uint8_t opcode) {
    switch (opcode) {
        case PB_F3D_G_VTX:
            return "G_VTX";
        case PB_F3D_G_TRI1:
            return "G_TRI1";
        case PB_F3D_G_TRI2:
            return "G_TRI2";
        case PB_F3D_G_MTX:
            return "G_MTX";
        case PB_F3D_G_POPMTX:
            return "G_POPMTX";
        case PB_F3D_G_SETTIMG:
            return "G_SETTIMG";
        case PB_F3D_G_SETTILE:
            return "G_SETTILE";
        case PB_F3D_G_LOADBLOCK:
            return "G_LOADBLOCK";
        case PB_F3D_G_LOADTLUT:
            return "G_LOADTLUT";
        case PB_F3D_G_SETCOMBINE:
            return "G_SETCOMBINE";
        case PB_F3D_G_SETSCISSOR:
            return "G_SETSCISSOR";
        case PB_F3D_G_ENDDL:
            return "G_ENDDL";
        default: {
            static char buf[8];
            snprintf(buf, sizeof(buf), "%02X", opcode);
            return buf;
        }
    }
}
