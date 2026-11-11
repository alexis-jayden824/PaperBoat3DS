#include "ex3/mst.h"

#include "ex3/aes128.h"
#include "ex3/crypto_envelope.h"
#include "ex3/fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int parse_records(const uint8_t *payload, size_t payload_len, uint32_t count,
                         Ex3MstTable *out)
{
    size_t need;
    uint32_t i;

    need = (size_t)count * (4U + EX3_MST_NAME_LEN);
    if (payload_len < need) {
        snprintf(out->error, sizeof(out->error), "%s", "payload too short");
        return -1;
    }
    if (count > EX3_MST_MAX_RECORDS) {
        snprintf(out->error, sizeof(out->error), "%s", "too many records");
        return -1;
    }
    for (i = 0; i < count; i++) {
        const uint8_t *rec = payload + (size_t)i * (4U + EX3_MST_NAME_LEN);
        out->records[i].id = rd_u32(rec);
        memcpy(out->records[i].name, rec + 4, EX3_MST_NAME_LEN);
        out->records[i].name[EX3_MST_NAME_LEN - 1] = '\0';
    }
    out->record_count = count;
    return 0;
}

Ex3MstResult ex3_mst_parse(const uint8_t *data, size_t len, Ex3MstTable *out)
{
    uint32_t version;
    uint32_t flags;
    uint32_t record_count;
    uint32_t payload_len;
    const uint8_t *payload;
    uint8_t *decrypted = NULL;
    size_t plain_len = 0;
    Ex3MstResult rc = EX3_MST_OK;

    if (data == NULL || out == NULL) {
        return EX3_MST_ERR_PARAM;
    }
    memset(out, 0, sizeof(*out));

    if (len < 8 + 4 + 4 + 8 + 4 + 4) {
        snprintf(out->error, sizeof(out->error), "%s", "header truncated");
        return EX3_MST_ERR_FORMAT;
    }
    if (memcmp(data, "EX3MST1", 7) != 0) {
        snprintf(out->error, sizeof(out->error), "%s", "bad magic");
        return EX3_MST_ERR_FORMAT;
    }

    version = rd_u32(data + 8);
    flags = rd_u32(data + 12);
    memcpy(out->key_token, data + 16, 8);
    out->key_token[8] = '\0';
    record_count = rd_u32(data + 24);
    payload_len = rd_u32(data + 28);
    if (version != 1U) {
        snprintf(out->error, sizeof(out->error), "%s", "unsupported version");
        return EX3_MST_ERR_FORMAT;
    }
    if (len < 32U + payload_len) {
        snprintf(out->error, sizeof(out->error), "%s", "payload truncated");
        return EX3_MST_ERR_FORMAT;
    }

    out->version = version;
    out->flags = flags;
    payload = data + 32;

    if (flags & EX3_MST_FLAG_ENCRYPTED) {
        uint8_t key[EX3_AES_KEY_LEN];
        size_t unpadded = 0;

        if ((payload_len % 16U) != 0U) {
            snprintf(out->error, sizeof(out->error), "%s", "encrypted len not aligned");
            return EX3_MST_ERR_CRYPTO;
        }
        if (ex3_crypto_key16(out->key_token, key) != 0) {
            snprintf(out->error, sizeof(out->error), "%s", "bad key token");
            return EX3_MST_ERR_CRYPTO;
        }
        decrypted = (uint8_t *)malloc(payload_len);
        if (decrypted == NULL) {
            snprintf(out->error, sizeof(out->error), "%s", "oom");
            return EX3_MST_ERR_CRYPTO;
        }
        if (ex3_aes128_cbc_decrypt(key, ex3_crypto_iv(), payload, decrypted, payload_len) !=
            0) {
            free(decrypted);
            snprintf(out->error, sizeof(out->error), "%s", "aes decrypt failed");
            return EX3_MST_ERR_CRYPTO;
        }
        if (ex3_crypto_pkcs7_unpad(decrypted, payload_len, &unpadded) != 0) {
            free(decrypted);
            snprintf(out->error, sizeof(out->error), "%s", "bad pkcs7");
            return EX3_MST_ERR_CRYPTO;
        }
        payload = decrypted;
        plain_len = unpadded;
    } else {
        plain_len = payload_len;
    }

    if (parse_records(payload, plain_len, record_count, out) != 0) {
        rc = (record_count > EX3_MST_MAX_RECORDS) ? EX3_MST_ERR_LIMIT : EX3_MST_ERR_FORMAT;
        free(decrypted);
        return rc;
    }

    free(decrypted);
    out->loaded = true;
    return EX3_MST_OK;
}

Ex3MstResult ex3_mst_load_path(const char *logical_path, Ex3MstTable *out)
{
    uint8_t *buf = NULL;
    size_t len = 0;
    Ex3FsResult fs;
    Ex3MstResult rc;

    if (logical_path == NULL || out == NULL) {
        return EX3_MST_ERR_PARAM;
    }
    fs = ex3_fs_read_all(logical_path, &buf, &len);
    if (fs != EX3_FS_OK) {
        memset(out, 0, sizeof(*out));
        snprintf(out->error, sizeof(out->error), "fs error %d", (int)fs);
        return EX3_MST_ERR_FORMAT;
    }
    rc = ex3_mst_parse(buf, len, out);
    ex3_fs_free(buf);
    return rc;
}
