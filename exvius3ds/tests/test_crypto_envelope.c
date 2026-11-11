/* Host unit test for S0 recovered crypto envelope helpers.
 * Provenance vectors from FFBE_SERVER_CONTRACT.md §1e / ffbe_crypto.key16.
 */
#include "ex3/crypto_envelope.h"

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

static void test_iv(void)
{
    static const char expect[] = "dZMjkk8gFDzKHlsx";
    const uint8_t *iv = ex3_crypto_iv();
    expect_true(iv != NULL, "iv non-null");
    expect_true(memcmp(iv, expect, EX3_AES_IV_LEN) == 0, "iv literal");
}

static void test_key16_game_setting(void)
{
    /* Contract §1e live vector: EncodeKey "4foXVwWd" →
     * 34666f58567757640000000000000000 */
    uint8_t key[EX3_AES_KEY_LEN];
    static const uint8_t expect[EX3_AES_KEY_LEN] = {
        0x34, 0x66, 0x6f, 0x58, 0x56, 0x77, 0x57, 0x64,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    expect_true(ex3_crypto_key16("4foXVwWd", key) == 0, "key16 rc");
    expect_true(memcmp(key, expect, EX3_AES_KEY_LEN) == 0,
                "key16 GameSettingRequest vector");
}

static void test_key16_null(void)
{
    uint8_t key[EX3_AES_KEY_LEN];
    expect_true(ex3_crypto_key16(NULL, key) == -1, "key16 null token");
}

static void test_pkcs7_and_double_pad(void)
{
    /* 515-byte plaintext → manual pad of 13 × 0x0D → 528, then +16 × 0x10 → 544
     * (contract §1e live client request shape). */
    uint8_t src[515];
    uint8_t dst[515 + 32];
    size_t n;
    size_t i;

    memset(src, 0x41, sizeof(src));
    n = ex3_crypto_pkcs7_pad(src, sizeof(src), dst, sizeof(dst));
    expect_true(n == 528, "pkcs7 515→528");
    for (i = 515; i < 528; i++) {
        if (dst[i] != 0x0D) {
            expect_true(0, "pkcs7 trailer 0x0D");
            break;
        }
    }
    if (i == 528) {
        expect_true(1, "pkcs7 trailer 0x0D");
    }

    n = ex3_crypto_double_pad(src, sizeof(src), dst, sizeof(dst));
    expect_true(n == 544, "double-pad 515→544");
    for (i = 528; i < 544; i++) {
        if (dst[i] != 0x10) {
            expect_true(0, "double-pad trailer 0x10");
            break;
        }
    }
    if (i == 544) {
        expect_true(1, "double-pad trailer 0x10");
    }
}

static void test_unpad(void)
{
    uint8_t buf[16];
    size_t out = 0;
    size_t i;

    for (i = 0; i < 12; i++) {
        buf[i] = 0x42;
    }
    for (; i < 16; i++) {
        buf[i] = 0x04;
    }
    expect_true(ex3_crypto_pkcs7_unpad(buf, 16, &out) == 0, "unpad rc");
    expect_true(out == 12, "unpad len");
}

int main(void)
{
    test_iv();
    test_key16_game_setting();
    test_key16_null();
    test_pkcs7_and_double_pad();
    test_unpad();

    if (g_failures != 0) {
        fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
