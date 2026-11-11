#include "ex3/texpack.h"

#include "ex3/fs.h"

#include <stdio.h>
#include <string.h>

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

Ex3TexPackResult ex3_texpack_parse(const uint8_t *data, size_t len, Ex3TexPack *out)
{
    uint32_t w;
    uint32_t h;
    size_t need;

    if (data == NULL || out == NULL) {
        return EX3_TEXPACK_ERR_PARAM;
    }
    memset(out, 0, sizeof(*out));
    if (len < 16 || memcmp(data, "EX3TEX1", 7) != 0) {
        snprintf(out->error, sizeof(out->error), "%s", "bad magic");
        return EX3_TEXPACK_ERR_FORMAT;
    }
    w = rd_u32(data + 8);
    h = rd_u32(data + 12);
    need = (size_t)w * (size_t)h * 4U;
    if (need == 0 || need > EX3_TEXPACK_MAX_PIXELS || len < 16U + need) {
        snprintf(out->error, sizeof(out->error), "%s", "pixel payload invalid");
        return EX3_TEXPACK_ERR_LIMIT;
    }

    out->plan = ex3_texture_plan(w, h);
    if (!out->plan.valid) {
        snprintf(out->error, sizeof(out->error), "%s", out->plan.reject_reason);
        return EX3_TEXPACK_ERR_PLAN;
    }

    out->width = w;
    out->height = h;
    memcpy(out->rgba, data + 16, need);
    out->rgba_len = need;
    out->loaded = true;
    return EX3_TEXPACK_OK;
}

Ex3TexPackResult ex3_texpack_load_path(const char *logical_path, Ex3TexPack *out)
{
    uint8_t *buf = NULL;
    size_t len = 0;
    Ex3FsResult fs;
    Ex3TexPackResult rc;

    if (logical_path == NULL || out == NULL) {
        return EX3_TEXPACK_ERR_PARAM;
    }
    fs = ex3_fs_read_all(logical_path, &buf, &len);
    if (fs != EX3_FS_OK) {
        memset(out, 0, sizeof(*out));
        snprintf(out->error, sizeof(out->error), "fs error %d", (int)fs);
        return EX3_TEXPACK_ERR_FORMAT;
    }
    rc = ex3_texpack_parse(buf, len, out);
    ex3_fs_free(buf);
    return rc;
}
