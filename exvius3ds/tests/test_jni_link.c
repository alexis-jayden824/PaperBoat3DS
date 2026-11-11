#include "ex3/base64.h"
#include "ex3/http.h"
#include "ex3/jni.h"
#include "ex3/login.h"
#include "ex3/prefs.h"
#include "ex3/recovered.h"

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
    char b64[64];
    uint8_t raw[32];
    size_t raw_len = 0;
    Ex3LoginSession session;
    Ex3HttpRequest req;
    Ex3HttpResponse resp;
    char pref[64];

    /* Direct stub surface coverage (also exercised via recovered smoke). */
    expect_true(ex3_login_guest(&session) == 0, "login guest");
    expect_true(session.guest == 1, "guest flag");

    expect_true(ex3_prefs_init("build/host/s4-prefs.txt") == 0, "prefs init");
    expect_true(ex3_prefs_set_string("k", "v") == 0, "prefs set");
    expect_true(ex3_prefs_flush() == 0, "prefs flush");
    expect_true(ex3_prefs_get_string("k", pref, sizeof(pref)) == 0, "prefs get");
    expect_true(strcmp(pref, "v") == 0, "prefs value");
    ex3_prefs_shutdown();

    expect_true(ex3_base64_encode((const uint8_t *)"hi", 2, b64, sizeof(b64)) == 0,
                "b64 encode");
    expect_true(ex3_base64_decode(b64, raw, sizeof(raw), &raw_len) == 0, "b64 decode");
    expect_true(raw_len == 2 && raw[0] == 'h' && raw[1] == 'i', "b64 roundtrip");

    expect_true(ex3_jni_encode_b64_aes("4foXVwWd", "{\"a\":1}", b64, sizeof(b64)) == 0,
                "jni encode");
    expect_true(ex3_jni_decode_b64_aes("4foXVwWd", b64, raw, sizeof(raw), &raw_len) == 0,
                "jni decode");
    expect_true(raw_len == 7 && memcmp(raw, "{\"a\":1}", 7) == 0, "jni roundtrip");

    memset(&req, 0, sizeof(req));
    snprintf(req.url, sizeof(req.url), "%s", "https://offline/stub");
    req.body = (const uint8_t *)"{}";
    req.body_len = 2;
    expect_true(ex3_http_request(&req, &resp) == 0 && resp.status == 200, "http stub");
    ex3_http_response_free(&resp);

    ex3_jni_rlog("test");
    ex3_jni_rlogs("test");
    ex3_jni_purchase_reset_flags();
    expect_true(ex3_jni_purchase_pending() == 0, "purchase pending");
    expect_true(ex3_jni_helper_get_static_method("LapisJNI", "encode") == 1,
                "jni helper method");
    ex3_jni_helper_call_void(1);

    /* Recovered-client translation unit must link and run with zero JNI. */
    expect_true(ex3_recovered_client_smoke() == 0, "recovered client smoke");

    if (g_failures != 0) {
        fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
