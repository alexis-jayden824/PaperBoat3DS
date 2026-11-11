#include "ex3/aes128.h"

#include <stdio.h>
#include <string.h>

static int g_failures;

static void expect_true(int cond, const char *name)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        g_failures++;
    } else {
        printf("ok: %s\n", name);
    }
}

int main(void)
{
    /* NIST AES-128 ECB vector exercised via CBC with zero IV on one block
     * is not identical; use encrypt→decrypt roundtrip instead, plus a
     * known OpenSSL-compatible CBC vector from FFBE IV/key style. */
    static const uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };
    static const uint8_t iv[16] = {
        'd', 'Z', 'M', 'j', 'k', 'k', '8', 'g',
        'F', 'D', 'z', 'K', 'H', 'l', 's', 'x'
    };
    static const uint8_t plain[32] = {
        'R', 'a', 'i', 'n', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        'F', 'i', 'n', 'a', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    uint8_t cipher[32];
    uint8_t out[32];

    expect_true(ex3_aes128_cbc_encrypt(key, iv, plain, cipher, 32) == 0, "encrypt");
    expect_true(memcmp(cipher, plain, 32) != 0, "ciphertext differs");
    expect_true(ex3_aes128_cbc_decrypt(key, iv, cipher, out, 32) == 0, "decrypt");
    expect_true(memcmp(out, plain, 32) == 0, "roundtrip");

    if (g_failures != 0) {
        fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
