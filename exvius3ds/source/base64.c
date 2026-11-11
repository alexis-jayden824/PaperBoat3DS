#include "ex3/base64.h"

static const char kTable[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

int ex3_base64_encode(const uint8_t *in, size_t in_len, char *out, size_t out_cap)
{
    size_t i;
    size_t o = 0;

    if ((in == NULL && in_len != 0) || out == NULL) {
        return -1;
    }

    for (i = 0; i < in_len; i += 3) {
        uint32_t n = ((uint32_t)in[i]) << 16;
        int remain = (int)(in_len - i);

        if (remain > 1) {
            n |= ((uint32_t)in[i + 1]) << 8;
        }
        if (remain > 2) {
            n |= (uint32_t)in[i + 2];
        }
        if (o + 4 >= out_cap) {
            return -1;
        }
        out[o++] = kTable[(n >> 18) & 63];
        out[o++] = kTable[(n >> 12) & 63];
        out[o++] = (remain > 1) ? kTable[(n >> 6) & 63] : '=';
        out[o++] = (remain > 2) ? kTable[n & 63] : '=';
    }
    if (o >= out_cap) {
        return -1;
    }
    out[o] = '\0';
    return 0;
}

int ex3_base64_decode(const char *in, uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t len = 0;
    size_t i = 0;
    size_t o = 0;

    if (in == NULL || out == NULL || out_len == NULL) {
        return -1;
    }
    while (in[len] != '\0') {
        len++;
    }
    if ((len % 4) != 0) {
        return -1;
    }

    while (i < len) {
        int v0 = b64_val(in[i++]);
        int v1 = b64_val(in[i++]);
        char c2 = in[i++];
        char c3 = in[i++];
        int v2 = (c2 == '=') ? 0 : b64_val(c2);
        int v3 = (c3 == '=') ? 0 : b64_val(c3);
        uint32_t n;

        if (v0 < 0 || v1 < 0 || (c2 != '=' && v2 < 0) || (c3 != '=' && v3 < 0)) {
            return -1;
        }
        n = ((uint32_t)v0 << 18) | ((uint32_t)v1 << 12) | ((uint32_t)v2 << 6) | (uint32_t)v3;
        if (o >= out_cap) {
            return -1;
        }
        out[o++] = (uint8_t)((n >> 16) & 0xff);
        if (c2 != '=') {
            if (o >= out_cap) {
                return -1;
            }
            out[o++] = (uint8_t)((n >> 8) & 0xff);
        }
        if (c3 != '=') {
            if (o >= out_cap) {
                return -1;
            }
            out[o++] = (uint8_t)(n & 0xff);
        }
    }
    *out_len = o;
    return 0;
}
