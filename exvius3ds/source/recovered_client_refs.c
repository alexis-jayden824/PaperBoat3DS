/* Synthetic recovered-client references for S4 link proof.
 * This stands in for libgame.so call sites that used Android/JNI edges.
 * It must resolve entirely through ex3_* replacements — no JNI symbols.
 */
#include "ex3/recovered.h"

#include "ex3/http.h"
#include "ex3/jni.h"
#include "ex3/login.h"
#include "ex3/prefs.h"

#include <stdio.h>
#include <string.h>

int ex3_recovered_client_smoke(void)
{
    char b64[512];
    uint8_t plain[256];
    size_t plain_len = 0;
    Ex3HttpRequest req;
    Ex3HttpResponse resp;
    Ex3LoginSession session;
    char pref[64];

    if (ex3_login_guest(&session) != 0) {
        return -1;
    }
    if (ex3_prefs_init("build/host/s4-recovered-prefs.txt") != 0) {
        return -1;
    }
    if (ex3_prefs_set_string("handle", session.handle) != 0) {
        return -1;
    }
    if (ex3_prefs_flush() != 0) {
        return -1;
    }
    if (ex3_prefs_get_string("handle", pref, sizeof(pref)) != 0) {
        return -1;
    }
    if (strcmp(pref, "Exvius3DS") != 0) {
        return -1;
    }

    if (ex3_jni_encode_b64_aes("4foXVwWd", "{\"ping\":1}", b64, sizeof(b64)) != 0) {
        return -1;
    }
    if (ex3_jni_decode_b64_aes("4foXVwWd", b64, plain, sizeof(plain), &plain_len) != 0) {
        return -1;
    }
    if (plain_len < 9 || memcmp(plain, "{\"ping\":1}", 9) != 0) {
        return -1;
    }

    memset(&req, 0, sizeof(req));
    snprintf(req.url, sizeof(req.url), "%s", "https://offline/local/actionSymbol/deadbeef.php");
    snprintf(req.encode_key, sizeof(req.encode_key), "%s", "4foXVwWd");
    req.body = (const uint8_t *)"{}";
    req.body_len = 2;
    if (ex3_http_request(&req, &resp) != 0 || resp.status != 200) {
        ex3_http_response_free(&resp);
        return -1;
    }
    ex3_http_response_free(&resp);

    ex3_jni_rlog("smoke");
    ex3_jni_rlogs("smoke");
    ex3_jni_purchase_reset_flags();
    (void)ex3_jni_purchase_pending();
    (void)ex3_jni_helper_get_static_method("LapisJNI", "encode");
    ex3_jni_helper_call_void(1);

    ex3_prefs_shutdown();
    return 0;
}
