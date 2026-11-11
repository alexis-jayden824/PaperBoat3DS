# S4 — JNI / Android surface catalog

Seeded from exvius-offline `FFBE_SERVER_CONTRACT.md` (Global v9.0.0).  
These are the Android edges the recomp must **not** leave unresolved.

## Catalog

| Surface | Evidence | Exvius3DS replacement |
| --- | --- | --- |
| `LapisJNI.encodeCStringForBase64WithNewCrypto` | contract §1e | `ex3_crypto_*` + `ex3_aes128_cbc_*` via `ex3_jni_encode_b64_aes` |
| `LapisJNI` purchase / rlog exports | contract §2.x | no-op stubs (`ex3_jni_purchase_*`, `ex3_jni_rlog`) |
| `Cocos2dxHttpURLConnection` / `HttpClient::send` | contract §1f | `ex3_http_request` → offline dispatcher hook (S6 fills body) |
| `SaveUtils` → SharedPreferences via LapisJNI | contract prefs notes | `ex3_prefs_*` SDMC key/value store |
| `cocos2d::JniHelper::*` | engine bridge | `ex3_jni_helper_*` no-op / host shims |
| LoginService Google/Facebook/Apple/PGS | contract §2.1 | offline guest stubs (`ex3_login_guest`) |
| Firebase / ads / analytics | third-party | dropped no-ops |

## Link rule (S4 acceptance)

A synthetic “recovered client” translation unit may call only the `ex3_*` replacements above.  
`make -C exvius3ds test` builds `test_jni_link` which references every stub symbol and must link with **zero** unresolved Android/JNI names.
