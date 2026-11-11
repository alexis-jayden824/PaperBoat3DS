# Exvius3DS — FFBE Offline **recomp** for New 3DS XL

Milestone plan to **recompile** the original Final Fantasy Brave Exvius Global client onto New Nintendo 3DS XL, using [exvius-offline](https://github.com/Elvlin/exvius-offline) as the offline/data oracle — not as an APK to “port,” and not as a clean-room rewrite of game systems.

Unofficial fan preservation. Not affiliated with Square Enix or gumi.

## What “recomp” means here

This project follows the **decomp → recompile → platform shim** lineage (SM64/OoT decomps, Ship of Harkinian, N64Recomp hosts), adapted to FFBE’s real binary:

| Recomp principle | Exvius3DS application |
| --- | --- |
| Original game code is authority | Recover logic from Global v9.0.0 `libgame.so` (Cocos2d-x client) |
| Recompile for a new host | Build recovered C/C++ against **devkitARM / ARM11**, not Android NDK |
| Replace platform backends | Android / JNI / GLES / Java crypto / HTTPS → **libctru · citro3d/citro2d · ndsp · SDMC** |
| Keep data contracts | MST tables, CPK packs, obfuscated request surface stay as the client expects |
| Enhancements are layered | Dual-screen layout, native controls, QoL, offline economy = **mods on the recomp**, not a second game |

### Explicit non-goals

- Running the Android APK, ART, Chaquopy, or Frida on 3DS
- Static ARM64→ARM11 binary translation as the primary path (wrong ABI/OS; treat as research-only if ever attempted)
- Clean-room reimplementation of battle/party/gacha when the original client code can be recovered instead

### Upstream inputs

| Input | Role |
| --- | --- |
| Global v9.0.0 `libgame.so` (+ optional `armeabi-v7a` build if present) | Recomp source of truth for client logic |
| exvius-offline `source/server/` + `FFBE_SERVER_CONTRACT.md` | Offline endpoint oracle; becomes **in-process stubs** the recompiled client calls |
| exvius-offline downloaded MST/CPK (~14.5 GB) | User-owned data; PC-baked for 3DS memory/VRAM |
| exvius-offline Frida/JS offline UX notes | Spec for enhancement mods (season filters, Lapis replay, etc.) |

---

## Binding technical direction

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Recompiled FFBE client (recovered from libgame.so)     │
│  scenes · battle · units · UI graph · MST/CPK consumers │
└─────────────┬───────────────────────────┬───────────────┘
              │                           │
              ▼                           ▼
┌──────────────────────────┐   ┌──────────────────────────┐
│  Platform shim (3DS)     │   │  Offline service stubs   │
│  gfx · audio · input     │   │  (exvius-offline server  │
│  fs · threads · crypto   │   │   logic, in-process)     │
│  dual-screen presenter   │   └──────────────────────────┘
└──────────────────────────┘
              │
              ▼
┌──────────────────────────┐
│  PC-baked .ex3 packs     │  user-owned; never in git
│  (downsampled CPK/MST)   │
└──────────────────────────┘
```

| Layer | Choice |
| --- | --- |
| Game code | Recovered / decompiled from `libgame.so`, recompiled for ARM11 |
| Platform | New 3DS exclusive: 804 MHz, L2, extended app RAM; `.3dsx` canonical |
| Graphics | Cocos/GLES draw path → citro3d/citro2d shim; dual-screen presenter as first-class host API |
| Network | Original HTTPS+AES client path retargeted to **local stubs** (no Python on device) |
| Data | PC bake of user MST/CPK → `.ex3`; streaming + LRU for 6 MB VRAM |
| Evidence | Milestone complete only with Folium and/or hardware proof |

### Dual-screen UX (enhancement layer on the recomp)

Original FFBE is single-screen phone UI. The recomp host **splits presentation**:

| Screen | Role |
| --- | --- |
| Top 400×240 | Stage — world, battle field, cutscenes, summons (original scene root, scaled) |
| Bottom 320×240 | Command chrome — touch menus, skill deck, party strip, maps, filters |

Phone full-screen menus become bottom sheets with live top-screen preview. Circle Pad / shoulders / ZL/ZR are host input maps into the recompiled HID path.

### Legal / asset policy

- **Source-only** repository: no APK, `libgame.so`, MST, CPK, artwork, audio, or fonts in git/CI
- Contributors work from **user-owned** Global v9.0.0 / exvius-offline data locally
- CI uses synthetic fixtures and public contracts only
- Distribution: source + host tools; players bring their own data

---

## Phases

| Phase | Sprints | Goal | Exit criteria |
| --- | --- | --- | --- |
| **P0 Recomp foundation** | S0–S2 | Binary inventory, host bootstrap, Cocos/platform shim skeleton | Recompiled stub links on 3DS; both screens clear |
| **P1 Bring-up** | S3–S5 | FS/assets, crypto/JNI elimination, dual-screen presenter + input | First recovered scene frame from baked data |
| **P2 Offline loop** | S6–S7 | In-process stubs + login/home from recompiled client | Reach Home with offline account on hardware |
| **P3 Playable core** | S8–S10 | Party/equip paths + battle + exploration via original code | Chapter 1 clear on n3DS XL |
| **P4 Parity & mods** | S11–S12 | Summon/Vortex/raids + dual-screen QoL enhancements | Offline feature parity + native feel checklist |
| **P5 Ship** | S13–S14 | Perf, coverage, hardware matrix, release | Source-only RC |

---

## Sprint backlog

### S0 — Recomp methodology & binary inventory

**Status: software complete (2026-11-11).** Owner binary hash fill-in remains local.

**Deliverables**

- ADR: decomp→recompile (not clean rewrite; not APK run) → [`docs/exvius3ds/ADR-001-recomp-methodology.md`](exvius3ds/ADR-001-recomp-methodology.md)
- Inventory Global v9.0.0 ABIs; partition map → [`docs/exvius3ds/S0_BINARY_INVENTORY.md`](exvius3ds/S0_BINARY_INVENTORY.md)
- Ghidra workflow + matching policy → [`GHIDRA_WORKFLOW.md`](exvius3ds/GHIDRA_WORKFLOW.md), [`MATCHING_POLICY.md`](exvius3ds/MATCHING_POLICY.md)
- Recovered crypto envelope (`ex3_crypto_*`) + host unit test → `exvius3ds/` (`make -C exvius3ds test`)

**Acceptance:** Published inventory + ADR; one recovered function builds as a host-side unit test.  
Evidence: [`docs/exvius3ds/S0_ACCEPTANCE.md`](exvius3ds/S0_ACCEPTANCE.md).

### S1 — New 3DS host bootstrap

**Status: host software complete (2026-11-11).** CI builds `.3dsx`; Folium/hardware paint is owner-gated.

**Deliverables**

- `ex3_plat` ABI + host/3DS backends → `exvius3ds/include/ex3/plat.h`, `plat_host.c`, `plat_3ds.c`
- Dual-screen diagnostic clear loop + APT lifecycle + SDMC log → `bootstrap.c`, `log.c`
- New 3DS exclusive RSF (96MB / 804 MHz / L2) → `packaging/Exvius3DS.rsf`
- Host tests + CI package workflow → `make -C exvius3ds test`, `.github/workflows/exvius3ds-3ds-build.yml`

**Acceptance:** `.3dsx` boots Folium + n3DS XL; both screens painted.  
Evidence: [`docs/exvius3ds/S1_ACCEPTANCE.md`](exvius3ds/S1_ACCEPTANCE.md), [`S1_HOST_BOOTSTRAP.md`](exvius3ds/S1_HOST_BOOTSTRAP.md).  
**Hardware:** owner confirmed dual navy clears on real n3DS XL (2026-11-11) — S1 closed.

### S2 — Cocos / renderer shim v0

**Status: host software complete (2026-11-11).** Hardware gold-sprite confirm owner-gated.

**Deliverables**

- Director/Scene/Node/Sprite/Label stubs → `exvius3ds/include/ex3/cocos.h`
- GLES-like rect draw → `ex3_plat_draw_rect` (citro2d on device)
- Texture plan clamps (≤1024, POT) → `ex3/texture.h`
- Diagnostic top-screen scene (gold sprite + label bar) in bootstrap

**Acceptance:** Diagnostic cocos-like scene graph draws on top screen from host code.  
Evidence: [`docs/exvius3ds/S2_ACCEPTANCE.md`](exvius3ds/S2_ACCEPTANCE.md), [`S2_COCOS_RENDERER.md`](exvius3ds/S2_COCOS_RENDERER.md).

### S3 — File system, MST, CPK bring-up

**Deliverables**

- Replace Android asset/APK zip paths with SDMC/RomFS
- Recover MST decrypt/load paths from client; wire to PC-baked tables
- CPK/resource open with streaming; chapter pack layout
- Host PC tools: bake + hash manifest (user-owned input)

**Acceptance:** Recompiled loader reads a fixture MST + one texture pack on device.

### S4 — JNI / Java surface elimination

**Deliverables**

- Catalog every JNI / `LapisJNI` / `HttpURLConnection` / SharedPreferences call site
- Native replacements: AES-CBC envelope (double-pad aware per contract), prefs→SDMC, threading
- Kill Android-only branches behind `ex3_plat` shims

**Acceptance:** Link of recovered client objects with **zero** unresolved Android/JNI symbols.

### S5 — Input + dual-screen presenter (first host mod)

**Deliverables**

- Map 3DS HID → original touch/virtual-pad model
- Presenter: assign scene layers to top vs bottom; safe areas for 400×240 / 320×240
- Touch on bottom; Circle Pad cursor; shoulder hotkey hooks (stubs)

**Acceptance:** Navigate a recovered menu scene with dual-screen split on hardware.

### S6 — Offline service stubs (exvius-offline oracle)

**Deliverables**

- Retarget client HTTP+AES transport to in-process dispatch
- Port critical offline server handlers to C++ stubs: connect, user info, feature switches, resource version lies
- Golden tests: stub responses match exvius-offline fixtures for the boot chain

**Acceptance:** Recompiled client completes connect/login against stubs in host tests + on device.

### S7 — Title → Home vertical bring-up

**Deliverables**

- Boot through Connect / download-skip / Title / Home using recovered scenes
- Dual-screen Home chrome (bottom nav, top showcase)
- Save path on SDMC (recovered format or versioned wrapper)

**Acceptance:** Fresh offline account reaches Home on n3DS XL; Folium + hardware evidence.

### S8 — Units / equipment / party (recovered paths)

**Deliverables**

- Exercise original unit/equip/party UI and model code under dual-screen presenter
- Fix only shim/host bugs; prefer client fidelity over redesign
- Stat/calc golden tests vs offline oracle

**Acceptance:** Build a party of 6 from user data; enter quest select.

### S9 — Battle bring-up (recovered combat)

**Deliverables**

- Mission start/end + battle scene through original code
- Bottom command deck mapping; top battle field
- Suspend/resume; ≥20 FPS target on n3DS XL

**Acceptance:** Win a scripted early-game encounter on hardware.

### S10 — Exploration + Chapter 1 slice

**Deliverables**

- World/town/dungeon streaming via recovered map paths + baked packs
- Dialogue/cutscene presenter
- Chapter 1 through first major boss

**Acceptance:** Fresh save → Chapter 1 boss clear on n3DS XL.

### S11 — Offline parity systems

**Deliverables**

- Summon banners (season/collab picker), Vortex filters, solo raid progress, Lapis replay objectives — behavior from exvius-offline, invoked through recompiled client + stubs
- Espers / Vision Cards along recovered equip paths
- Shops + Lapis bundles; login calendar

**Acceptance:** Offline loop parity checklist green for targeted seasons.

### S12 — Native QoL enhancement pack

**Deliverables** (host mods, not client rewrites unless required)

- Dual-pane inspect; shoulder macros; auto-battle presets
- Quick party dock; readable 240p scaling; colorblind-safe options
- Battery frame pacing; sleep/home safety

**Acceptance:** “Feels native to n3DS XL” playtest checklist passed.

### S13 — Performance, memory, coverage

**Deliverables**

- VRAM/linear LRU; SD streaming; frame budgets
- Long-session soak; coverage ledger across story packs in scope
- Crash diagnostics

**Acceptance:** Stable 20–30 FPS combat; no OOM on soak; coverage ledger updated.

### S14 — Hardware matrix & release

**Deliverables**

- n3DS XL/LL validation matrix
- Reproducible source release; bake-tool docs; no SE binaries in artifacts

**Acceptance:** Owner-signed hardware evidence; RC tagged.

---

## Native QoL (enhancement layer)

| Feature | Notes |
| --- | --- |
| Dual-pane inspect | Host presenter: art/stats top, equip bottom |
| Shoulder macros | Injected into recompiled input path |
| Auto-battle presets | Bottom-screen host overlay |
| Quick party dock | Host UI calling original party APIs |
| Season Vortex / summons | Stub + UI parity with exvius-offline |
| Replay Lapis farms | Offline stub behavior |
| Suspend-safe saves | APT lifecycle in platform shim |
| Readable 240p UI | Presenter scaling / hit targets |

---

## Risk register

| Risk | Mitigation |
| --- | --- |
| `libgame.so` is huge, stripped, Android-bound | Partition early; shim Android edges; prefer `armeabi-v7a` recovery if available for closer ARM bring-up |
| Matching decomp is slow | Prioritize boot→home→battle call chains; leave dead/social/IAP paths stubbed |
| Cocos/GLES ≠ PICA | Narrow draw subset; atlas/downsample in bake; reject unsupported effects explicitly |
| 14.5 GB assets vs 6 MB VRAM | Mandatory PC bake + streaming; chapter packs |
| Offline server in Python | Reimplement stubs in C++ against contract + fixtures; never ship CPython on 3DS |
| Legal exposure | Source-only; user-owned data; no binaries in git/CI |

---

## First build order

1. **S0** lock recomp methodology + binary map  
2. **S1–S2** host + cocos/gfx shim  
3. **S3–S5** assets, JNI kill, dual-screen presenter  
4. **S6–S7** offline stubs → Home (first “it boots as FFBE” gate)  
5. **S8–S10** party → battle → Chapter 1 (first “playable recomp” gate)  
6. **S11–S14** parity, QoL, ship  

## References

- [Elvlin/exvius-offline](https://github.com/Elvlin/exvius-offline) — offline data + local server oracle  
- `source/docs/analysis/FFBE_SERVER_CONTRACT.md` — client↔server surface  
- PaperBoat3DS — 3DS packaging/evidence discipline for a related recomp-style host  
- Ship of Harkinian / N64Recomp — methodological analogues (platform shim + enhancements on original code)
