#include "ex3/jni.h"

#include "ex3/aes128.h"
#include "ex3/base64.h"
#include "ex3/crypto_envelope.h"

#include <string.h>

int ex3_jni_encode_b64_aes(const char *encode_key, const char *plain_utf8,
                           char *out_b64, size_t out_cap)
{
    uint8_t key[EX3_AES_KEY_LEN];
    uint8_t padded[2048];
    uint8_t cipher[2048];
    size_t plain_len;
    size_t padded_len;

    if (encode_key == NULL || plain_utf8 == NULL || out_b64 == NULL) {
        return -1;
    }
    plain_len = strlen(plain_utf8);
    if (plain_len + 32 > sizeof(padded)) {
        return -1;
    }
    if (ex3_crypto_key16(encode_key, key) != 0) {
        return -1;
    }
    padded_len = ex3_crypto_double_pad((const uint8_t *)plain_utf8, plain_len, padded,
                                       sizeof(padded));
    if (padded_len == 0 || padded_len > sizeof(cipher)) {
        return -1;
    }
    if (ex3_aes128_cbc_encrypt(key, ex3_crypto_iv(), padded, cipher, padded_len) != 0) {
        return -1;
    }
    return ex3_base64_encode(cipher, padded_len, out_b64, out_cap);
}

int ex3_jni_decode_b64_aes(const char *encode_key, const char *b64, uint8_t *out_plain,
                           size_t out_cap, size_t *out_len)
{
    uint8_t key[EX3_AES_KEY_LEN];
    uint8_t cipher[2048];
    uint8_t plain[2048];
    size_t cipher_len = 0;
    size_t unpadded = 0;

    if (encode_key == NULL || b64 == NULL || out_plain == NULL || out_len == NULL) {
        return -1;
    }
    if (ex3_crypto_key16(encode_key, key) != 0) {
        return -1;
    }
    if (ex3_base64_decode(b64, cipher, sizeof(cipher), &cipher_len) != 0) {
        return -1;
    }
    if ((cipher_len % 16) != 0 || cipher_len > sizeof(plain)) {
        return -1;
    }
    if (ex3_aes128_cbc_decrypt(key, ex3_crypto_iv(), cipher, plain, cipher_len) != 0) {
        return -1;
    }
    /* Double-pad encode → first unpad strips the extra 0x10 block; a second
     * unpad strips the real PKCS7 trailer. Single-padded responses stop after
     * the first successful unpad. */
    if (ex3_crypto_pkcs7_unpad(plain, cipher_len, &unpadded) != 0) {
        return -1;
    }
    {
        size_t again = 0;
        if (ex3_crypto_pkcs7_unpad(plain, unpadded, &again) == 0) {
            unpadded = again;
        }
    }
    if (unpadded > out_cap) {
        return -1;
    }
    memcpy(out_plain, plain, unpadded);
    *out_len = unpadded;
    return 0;
}

void ex3_jni_rlog(const char *msg)
{
    (void)msg;
}

void ex3_jni_rlogs(const char *msg)
{
    ex3_jni_rlog(msg);
}

void ex3_jni_purchase_reset_flags(void)
{
}

int ex3_jni_purchase_pending(void)
{
    return 0;
}

int ex3_jni_helper_get_static_method(const char *class_name, const char *method_name)
{
    (void)class_name;
    (void)method_name;
    return 1;
}

void ex3_jni_helper_call_void(int method_id)
{
    (void)method_id;
}
