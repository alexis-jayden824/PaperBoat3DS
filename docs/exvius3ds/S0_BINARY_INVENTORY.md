# S0 — Binary inventory (seeded)

Inventory of Global v9.0.0 client surfaces for the Exvius3DS recomp.  
**Seeded from public exvius-offline analysis** (`FFBE_SERVER_CONTRACT.md`, `source/README.md`).  
Local `libgame.so` measurements are filled by the owner using [`GHIDRA_WORKFLOW.md`](GHIDRA_WORKFLOW.md) and are **never committed**.

## Provenance (public)

| Fact | Value | Source |
| --- | --- | --- |
| Client build | Global v9.0.0 | exvius-offline `source/README.md` |
| Original APK SHA-256 | `955deacfbe4b3d0cd04e2947eb005e310915ba98132d71216626bfb3554ad246` | same |
| Primary analysis ABI | ARM64 `libgame.so` | `FFBE_SERVER_CONTRACT.md` |
| Reported libgame content hash (partial) | `a1276540…6937e4` | contract §1e |
| Engine | Cocos2d-x (`cocos2d::` exports, `network::HttpClient`) | contract §1 |
| Endpoint count | 211 `/actionSymbol/<token>.php` paths; 222 `*Request` classes | contract §0 |
| Wire crypto | AES-128-CBC, fixed IV, EncodeKey NUL-pad 16, **double PKCS7** on client path via `LapisJNI` | contract §1e |

### Local fill-in (owner; do not commit binaries)

| Field | Value |
| --- | --- |
| Local APK path | _private_ |
| `arm64-v8a/libgame.so` SHA-256 | _TBD_ |
| `armeabi-v7a/libgame.so` present? | _TBD_ |
| `armeabi-v7a/libgame.so` SHA-256 | _TBD or N/A_ |
| Cocos2d-x version string | _TBD from strings_ |
| `nm -D --defined-only` line count | _TBD_ (contract cites ~112k demangled on ARM64) |

## Partition map

Recovered symbols are triaged into one of five buckets. S0 seeds the buckets from the contract; later sprints move symbols from **Unknown** → a bucket with evidence.

### A — Game logic (recompile target)

Client gameplay / meta code that should eventually compile for ARM11.

| Area | Example symbols / notes | Status |
| --- | --- | --- |
| Requests | `*Request::getUrl/getEncodeKey/getRequestID`, `BaseRequest::*` | Inventoried (contract) |
| User / party | `UserInfo::*`, `UserTeamInfo::*`, `ModelChangeUserInfo::*` | Inventoried |
| Battle / mission | `MissionStartRequest`, battle scene symbols | Partial |
| Gacha / shop / events | `GachaMst::*`, shop/event request classes | Partial |
| MST accessors | `*MstList/Response`, localized text loaders | Inventoried |

### B — Cocos engine (shim or subset recompile)

| Area | Notes | Host plan |
| --- | --- | --- |
| `cocos2d::Director/Scene/Node/Sprite/Label` | Scene graph | Shim subset (S2) |
| `cocos2d::network::HttpClient` | Uses Java `HttpsURLConnection` | Replace with stub transport (S6) |
| `cocos2d::JniHelper` | JNI bridge | Eliminate (S4) |
| `UserDefault` | Cocos prefs XML path | Map to SDMC (S4) |

### C — Android / JNI (eliminate)

| Surface | Evidence | Host replacement |
| --- | --- | --- |
| `LapisJNI.encodeCStringForBase64WithNewCrypto` | contract §1e | Native AES-CBC in `ex3_crypto_*` |
| `Cocos2dxHttpURLConnection` | contract §1f | In-process offline dispatch |
| `SaveUtils` → SharedPreferences via LapisJNI | contract (prefs) | SDMC atomic saves |
| Purchase / Play Games / Facebook / Apple login | LapisJNI purchase + LoginService* | Stub / offline guest only |
| `Java_com_square_1enix_…_LapisJNI_*` | JNI exports | No-op or host impl |

### D — Crypto / net (reimplement in shim)

| Fact | Detail |
| --- | --- |
| IV | ASCII `dZMjkk8gFDzKHlsx` (16 bytes), shared with MST CBC |
| Key | 8-char `EncodeKey`, UTF-8, NUL-padded to 16 |
| Mode | AES-128-CBC; client request path double-pads |
| Native OpenSSL in `.so` | Present but **dead** on the live network path (JNI instead) |
| libcurl 7.52.1 | Statically linked, **off** request path |

S0 recovered implementation: `exvius3ds/source/crypto_envelope.c` (+ host unit test).

### E — Third-party (stub or drop)

| Component | Notes | Plan |
| --- | --- | --- |
| CRIWARE / SBX (`crisbx.jp`) | Audio middleware analytics | Drop / stub |
| Firebase analytics | Telemetry | Drop |
| Ads / IAP SDKs | Purchase JNI | Drop |
| OpenSSL (bundled) | Prefer host crypto for wire path | Do not rely on for JNI path |

## Boot-critical call chain (priority order)

Recover / shim in this order for the S7 Home gate:

1. Process start → cocos `Application` / `AppDelegate` equivalent  
2. `LoginScene` / `ConnectScene` connect list  
3. Crypto encode/decode (`LapisJNI` → `ex3_crypto_*`)  
4. HTTP send → offline stub dispatch  
5. Feature switch + user info responses  
6. Resource version “already current” lies (skip multi-GB download on device)  
7. Title → Home scene enter  

## Endpoint / request surface

- **211** action paths under `/lapisProd/app/php/gme/actionSymbol/`  
- Host for online was versioned (`v900-lapis.gumi.sg`); Exvius3DS never dials out — stubs only  
- Full token tables live in exvius-offline `source/server/endpoints.json` (reference; not vendored here yet)

## Open S0 questions (owner binary required)

1. Does the release APK include `armeabi-v7a/libgame.so`?  
2. Exact Cocos2d-x tag/commit string in the binary?  
3. Size of `.text` / `.rodata` and export counts per ABI?  
4. Which scene class owns first Home render after connect?

Until those are filled locally, CI continues on **public-contract recoveries** (crypto envelope) and synthetic fixtures only.
