#include "ex3/fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_host_root[512];

void ex3_fs_set_host_root(const char *root)
{
    if (root == NULL || root[0] == '\0') {
        g_host_root[0] = '\0';
        return;
    }
    snprintf(g_host_root, sizeof(g_host_root), "%s", root);
}

static void build_physical(const char *logical, char *out, size_t out_cap)
{
#if defined(__3DS__)
    if (strncmp(logical, "romfs:/", 7) == 0) {
        snprintf(out, out_cap, "romfs:/%s", logical + 7);
        return;
    }
    if (strncmp(logical, "sdmc:/", 6) == 0) {
        snprintf(out, out_cap, "sdmc:/%s", logical + 6);
        return;
    }
    snprintf(out, out_cap, "%s", logical);
#else
    const char *rest;
    if (strncmp(logical, "romfs:/", 7) == 0) {
        rest = logical + 7;
        if (g_host_root[0] != '\0') {
            snprintf(out, out_cap, "%s/romfs/%s", g_host_root, rest);
        } else {
            snprintf(out, out_cap, "romfs/%s", rest);
        }
        return;
    }
    if (strncmp(logical, "sdmc:/", 6) == 0) {
        rest = logical + 6;
        if (g_host_root[0] != '\0') {
            snprintf(out, out_cap, "%s/sdmc/%s", g_host_root, rest);
        } else {
            snprintf(out, out_cap, "sdmc/%s", rest);
        }
        return;
    }
    if (g_host_root[0] != '\0') {
        snprintf(out, out_cap, "%s/%s", g_host_root, logical);
    } else {
        snprintf(out, out_cap, "%s", logical);
    }
#endif
}

Ex3FsResult ex3_fs_read_all(const char *logical_path, uint8_t **out_buf, size_t *out_len)
{
    char path[768];
    FILE *fp;
    long sz;
    uint8_t *buf;
    size_t nread;

    if (logical_path == NULL || out_buf == NULL || out_len == NULL) {
        return EX3_FS_ERR_PARAM;
    }
    *out_buf = NULL;
    *out_len = 0;

    build_physical(logical_path, path, sizeof(path));
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return EX3_FS_ERR_OPEN;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return EX3_FS_ERR_READ;
    }
    sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        return EX3_FS_ERR_READ;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return EX3_FS_ERR_READ;
    }

    buf = (uint8_t *)malloc((size_t)sz + 1U);
    if (buf == NULL) {
        fclose(fp);
        return EX3_FS_ERR_NOMEM;
    }
    nread = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (nread != (size_t)sz) {
        free(buf);
        return EX3_FS_ERR_READ;
    }
    buf[sz] = 0;
    *out_buf = buf;
    *out_len = (size_t)sz;
    return EX3_FS_OK;
}

void ex3_fs_free(uint8_t *buf)
{
    free(buf);
}
