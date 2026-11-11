#include "ex3/crypto_envelope.h"

#include <string.h>

static const uint8_t kEx3Iv[EX3_AES_IV_LEN] = {
    /* "dZMjkk8gFDzKHlsx" — FFBE_SERVER_CONTRACT.md §1e */
    'd', 'Z', 'M', 'j', 'k', 'k', '8', 'g',
    'F', 'D', 'z', 'K', 'H', 'l', 's', 'x'
};

const uint8_t *ex3_crypto_iv(void)
{
    return kEx3Iv;
}

int ex3_crypto_key16(const char *token, uint8_t out[EX3_AES_KEY_LEN])
{
    size_t n;
    size_t i;

    if (token == NULL || out == NULL) {
        return -1;
    }

    memset(out, 0, EX3_AES_KEY_LEN);
    n = strlen(token);
    if (n > EX3_AES_KEY_LEN) {
        n = EX3_AES_KEY_LEN;
    }
    for (i = 0; i < n; i++) {
        out[i] = (uint8_t)token[i];
    }
    return 0;
}

size_t ex3_crypto_pkcs7_pad(const uint8_t *src, size_t len,
                            uint8_t *dst, size_t dst_cap)
{
    size_t pad;
    size_t total;
    size_t i;

    if (dst == NULL || (src == NULL && len != 0)) {
        return 0;
    }

    pad = EX3_AES_BLOCK - (len % EX3_AES_BLOCK);
    total = len + pad;
    if (total > dst_cap) {
        return 0;
    }

    if (len > 0) {
        memcpy(dst, src, len);
    }
    for (i = 0; i < pad; i++) {
        dst[len + i] = (uint8_t)pad;
    }
    return total;
}

size_t ex3_crypto_double_pad(const uint8_t *src, size_t len,
                             uint8_t *dst, size_t dst_cap)
{
    size_t first;
    size_t i;

    /* Manual PKCS7, then one extra full block of 0x10 (contract §1e). */
    if (dst_cap < len + EX3_AES_BLOCK + EX3_AES_BLOCK) {
        /* Need room for worst-case first pad (+16) plus extra block (+16). */
        if (dst_cap < len + (EX3_AES_BLOCK - (len % EX3_AES_BLOCK)) + EX3_AES_BLOCK) {
            return 0;
        }
    }

    first = ex3_crypto_pkcs7_pad(src, len, dst, dst_cap);
    if (first == 0) {
        return 0;
    }
    if (first + EX3_AES_BLOCK > dst_cap) {
        return 0;
    }
    for (i = 0; i < EX3_AES_BLOCK; i++) {
        dst[first + i] = 0x10;
    }
    return first + EX3_AES_BLOCK;
}

int ex3_crypto_pkcs7_unpad(uint8_t *buf, size_t len, size_t *out_len)
{
    uint8_t pad;
    size_t i;

    if (buf == NULL || out_len == NULL || len == 0 || (len % EX3_AES_BLOCK) != 0) {
        return -1;
    }

    pad = buf[len - 1];
    if (pad < 1 || pad > EX3_AES_BLOCK || pad > len) {
        return -1;
    }
    for (i = 0; i < pad; i++) {
        if (buf[len - 1 - i] != pad) {
            return -1;
        }
    }
    *out_len = len - pad;
    return 0;
}
