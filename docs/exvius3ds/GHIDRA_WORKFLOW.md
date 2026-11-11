# Ghidra workflow — `libgame.so` recovery

Local-only. **Never** commit the APK, `libgame.so`, Ghidra projects with embedded binaries, or extracted packs.

## Prerequisites

- User-owned Global v9.0.0 APK matching SHA-256  
  `955deacfbe4b3d0cd04e2947eb005e310915ba98132d71216626bfb3554ad246`  
  (exvius-offline `source/README.md`)
- Ghidra 11+ (or IDA); `llvm-nm` / `llvm-objdump` / `strings` optional
- Working tree path for private inputs: `exvius3ds/private/` (gitignored)

## Extract

```bash
# from your private working directory — example only
unzip -l FFBE-Global-v9.0.0-original.apk | grep libgame.so
unzip -j FFBE-Global-v9.0.0-original.apk \
  'lib/arm64-v8a/libgame.so' -d exvius3ds/private/arm64-v8a/
# if present:
unzip -j FFBE-Global-v9.0.0-original.apk \
  'lib/armeabi-v7a/libgame.so' -d exvius3ds/private/armeabi-v7a/
sha256sum exvius3ds/private/*/libgame.so
```

Record hashes in your private notes / local copy of `S0_BINARY_INVENTORY.md` fill-in table. Do not push.

## Import

1. New Ghidra project under `exvius3ds/private/ghidra/` (gitignored).
2. Import `libgame.so` as ELF; analyze with default analyzers + Demangler.
3. Prefer **armeabi-v7a** for early ARM11-oriented recovery when available; keep ARM64 for cross-checks.

## First-pass commands (outside Ghidra)

```bash
llvm-nm -D --defined-only --demangle libgame.so | tee nm.defined.txt | wc -l
strings -n 8 libgame.so | rg -n 'cocos2d|LapisJNI|dZMjkk8gFDzKHlsx|libcurl'
rg 'getEncodeKey|getRequestID|getUrl|JniHelper|HttpURLConnection' nm.defined.txt
```

## Triage checklist (per symbol)

For each interesting symbol:

1. Assign partition **A–E** per [`S0_BINARY_INVENTORY.md`](S0_BINARY_INVENTORY.md).
2. Note ABI, address, demangled name, and whether it is Android-bound.
3. If game logic (A): sketch C prototype; add a matching unit under `exvius3ds/tests/` when behavior is known.
4. If JNI/Android (C): list the Java callee; add an `ex3_plat` stub ticket (S4).

## Boot-chain focus

Do not boil the ocean. Recover in this order:

1. Crypto JNI bridge → already recovered as `ex3_crypto_*` (contract §1e)
2. `BaseRequest` / connect list construction
3. `HttpClient::send` → mark for stub retarget
4. Login / `GetUserInfo*` response parsers (enough to enter Home)
5. First Home scene init

## What may enter git

| Allowed | Forbidden |
| --- | --- |
| Symbol **names**, addresses as hex notes, call graphs | The `.so` / APK / dex |
| Recovered **algorithms** with public-contract provenance | Bulk `.rodata` dumps of game strings/assets |
| Synthetic fixtures | MST/CPK/user saves |
| Screenshots of Ghidra with **no** binary path leaking secrets beyond public facts | Full Ghidra project archives containing the ELF |

## Cross-reference

exvius-offline already recovered large parts of the request table and crypto. Prefer citing  
`source/docs/analysis/FFBE_SERVER_CONTRACT.md` over re-deriving from scratch when the fact is already high-confidence.
