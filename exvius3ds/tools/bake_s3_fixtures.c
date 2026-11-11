/* Host tool: bake synthetic S3 fixtures into romfs/fixtures. */
#include "ex3/aes128.h"
#include "ex3/crypto_envelope.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

static int write_file(const char *path, const uint8_t *data, size_t len)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        perror(path);
        return -1;
    }
    if (fwrite(data, 1, len, fp) != len) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static void append_record(uint8_t *dst, size_t *off, uint32_t id, const char *name)
{
    size_t i;
    wr_u32(dst + *off, id);
    *off += 4;
    memset(dst + *off, 0, 32);
    for (i = 0; name[i] != '\0' && i < 31; i++) {
        dst[*off + i] = (uint8_t)name[i];
    }
    *off += 32;
}

int main(int argc, char **argv)
{
    const char *outdir = (argc > 1) ? argv[1] : "romfs/fixtures";
    char path[512];
    uint8_t records[3 * 36];
    size_t rec_len = 0;
    uint8_t padded[256];
    size_t padded_len;
    uint8_t encrypted[256];
    uint8_t key[16];
    uint8_t filebuf[512];
    size_t file_len;
    uint8_t tex[16 + 16 * 16 * 4];
    int x, y;
    size_t toff;

    append_record(records, &rec_len, 1, "Rain");
    append_record(records, &rec_len, 2, "Lasswell");
    append_record(records, &rec_len, 3, "Fina");

    /* plaintext MST */
    memset(filebuf, 0, sizeof(filebuf));
    memcpy(filebuf, "EX3MST1", 7);
    wr_u32(filebuf + 8, 1);
    wr_u32(filebuf + 12, 0);
    memcpy(filebuf + 16, "4foXVwWd", 8);
    wr_u32(filebuf + 24, 3);
    wr_u32(filebuf + 28, (uint32_t)rec_len);
    memcpy(filebuf + 32, records, rec_len);
    file_len = 32 + rec_len;
    snprintf(path, sizeof(path), "%s/diag.mst", outdir);
    if (write_file(path, filebuf, file_len) != 0) {
        return 1;
    }

    /* encrypted MST */
    padded_len = ex3_crypto_pkcs7_pad(records, rec_len, padded, sizeof(padded));
    if (padded_len == 0 || ex3_crypto_key16("4foXVwWd", key) != 0) {
        fprintf(stderr, "pad/key failed\n");
        return 1;
    }
    if (ex3_aes128_cbc_encrypt(key, ex3_crypto_iv(), padded, encrypted, padded_len) != 0) {
        fprintf(stderr, "encrypt failed\n");
        return 1;
    }
    memset(filebuf, 0, sizeof(filebuf));
    memcpy(filebuf, "EX3MST1", 7);
    wr_u32(filebuf + 8, 1);
    wr_u32(filebuf + 12, 1);
    memcpy(filebuf + 16, "4foXVwWd", 8);
    wr_u32(filebuf + 24, 3);
    wr_u32(filebuf + 28, (uint32_t)padded_len);
    memcpy(filebuf + 32, encrypted, padded_len);
    file_len = 32 + padded_len;
    snprintf(path, sizeof(path), "%s/diag_enc.mst", outdir);
    if (write_file(path, filebuf, file_len) != 0) {
        return 1;
    }

    /* texpack */
    memset(tex, 0, sizeof(tex));
    memcpy(tex, "EX3TEX1", 7);
    wr_u32(tex + 8, 16);
    wr_u32(tex + 12, 16);
    toff = 16;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            tex[toff++] = 0xD4;
            tex[toff++] = 0xA0;
            tex[toff++] = 0x17;
            tex[toff++] = ((x + y) & 1) ? 0xCC : 0xFF;
        }
    }
    snprintf(path, sizeof(path), "%s/diag.tex", outdir);
    if (write_file(path, tex, toff) != 0) {
        return 1;
    }

    printf("baked fixtures into %s\n", outdir);
    return 0;
}
