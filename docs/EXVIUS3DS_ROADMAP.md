# Exvius3DS — FFBE Offline for New 3DS XL

Milestone plan to rebuild [Final Fantasy Brave Exvius Offline](https://github.com/Elvlin/exvius-offline) as a **native** dual-screen homebrew client for New Nintendo 3DS XL.

Unofficial fan preservation project. Not affiliated with Square Enix or gumi.

## Binding technical direction

### What exvius-offline actually is

Elvlin/exvius-offline is **not** a portable engine. It is:

- The original Global **Cocos2d-x** Android client (`libgame.so`, v9.0.0)
- An embedded **local Python game server** (Chaquopy) that answers the client's ~211 encrypted JSON endpoints
- Frida/JS hooks for offline UX (season summons, Vortex filters, Lapis replay, etc.)
- ~**14.5 GB** of MST tables + CPK resource packs downloaded once to the phone

That stack **cannot run on 3DS**. Exvius3DS is therefore a **native reimplementation**:

| Layer | Choice |
| --- | --- |
| Client | Custom C/C++ scenes + citro2d/citro3d UI/renderer |
| Game logic | In-process C++ ports of offline `server/` domains (player, battle, gacha, events, raids-solo) |
| Data | PC bake: MST/CPK → Exvius3DS packs (`.ex3`); user-owned, never committed |
| Oracle | Open `source/server/` + `FFBE_SERVER_CONTRACT.md` + golden fixtures |
| Package | `.3dsx` canonical; `.cia` optional |
| Hardware | **New 3DS / New 3DS XL exclusive** (804 MHz, L2 cache, extended app RAM) |
| Evidence | No milestone claimed without Folium and/or hardware logs |

### Dual-screen UX contract

| Screen | Role | Content |
| --- | --- | --- |
| Top 400×240 | Stage / spectacle | World, battle field, summon cinema, cutscenes, unit showcase. No touch. Minimal HUD. |
| Bottom 320×240 | Command / chrome | Touch-first FFBE panels: skill deck, items, party strip, menus, maps, filters. |

**Rule:** top is spectacle, bottom is agency. Phone full-screen menus become bottom sheets with live top-screen preview.

Controls baseline:

- Circle Pad — cursor / scroll / camera
- D-pad — discrete menu nav
- A confirm · B back · X/Y context
- L / R / ZL / ZR — hotkeys (items, esper, auto, skip)

### Legal / asset policy

- Repository is **source-only**
- No Square Enix APK, MST, CPK, artwork, audio, or fonts in git or CI
- Users generate `.ex3` packs on PC from data they already own via exvius-offline (or equivalent legal personal copy)
- Same evidence discipline as PaperBoat3DS: claims require reproducible proof

---

## Phases

| Phase | Sprints | Goal | Exit criteria |
| --- | --- | --- | --- |
| **P0 Foundations** | S0–S1 | Toolchain, legal boundary, dual-screen shell | Boots `.3dsx` on n3DS XL; both screens + diagnostics |
| **P1 Data & State** | S2–S3 | PC pipeline + local-authoritative services | Decoded MST subset + SDMC save load/store |
| **P2 Native GUI** | S4–S5 | FFBE-styled dual-screen UI + party systems | Home / Units / Equip / Party usable offline |
| **P3 Playable Core** | S6–S8 | Battle + exploration + story vertical slice | Clear Chapter 1 end-to-end on hardware |
| **P4 Systems Breadth** | S9–S11 | Summon, Vortex, raids-solo, Espers, Vision Cards | Offline loop matches exvius-offline feature set |
| **P5 Native Feel** | S12–S14 | QoL, performance, hardware matrix, release | Ship-ready New 3DS XL homebrew (source-only) |

---

## Sprint backlog

### S0 — Architecture lock & spikes

**Deliverables**

- Architecture Decision Records (native client; no Android/Python on device)
- Memory / VRAM / SD budgets for New 3DS XL exclusive
- Map of offline `server/` modules → in-process C++ services
- Spike: decode one MST table; blit one downsampled sprite on top screen

**Acceptance:** ADRs merged; spike Folium capture attached.

### S1 — Bootstrap + dual-screen shell

**Deliverables**

- devkitARM Makefile; libctru, citro2d, citro3d, ndsp
- New 3DS profile: 804 MHz, L2, extended RAM
- Frame loop clearing both screens; APT lifecycle; diagnostics log to SDMC
- Packages: `.3dsx`, `.3ds` (QA), `.cia` (optional)

**Acceptance:** Boots on Folium + real n3DS XL; clean exit.

### S2 — PC data pipeline (MST / CPK → `.ex3`)

**Deliverables**

- Host tools: MST decode, CPK extract, texture downsample/atlas/swizzle
- Chapter / season pack layout on SDMC
- Hash manifests; synthetic CI fixtures (no SE content)
- Streaming residency notes (VRAM 6 MB, linear RAM budgets)

**Acceptance:** Pipeline produces user-owned packs; CI validates format only.

### S3 — Local game services + saves

**Deliverables**

- C++ ports of critical offline domains: account/login, user info, inventory, battle start/end, rewards
- Atomic SDMC saves with versioning / recovery
- Golden tests against offline server fixtures

**Acceptance:** Create → login → get user info → persist round-trip.

### S4 — Custom FFBE GUI shell

**Deliverables**

- Design tokens matching FFBE (panel chrome, blues/golds, readable 240p type)
- Scene stack: Title → Home → Units → Shop stubs
- Bottom-screen touch chrome; top-screen stage preview
- Input map including shoulders

**Acceptance:** Navigate shell on hardware with readable dual-screen UI.

### S5 — Units, equipment, party

**Deliverables**

- Roster browser, unit detail, equip / materia, party of 6
- Friend-unit stub as AI ally (offline)
- Capacity limits; stat calculation vs reference

**Acceptance:** Build a legal party from fixture data; stats match oracle.

### S6 — Battle engine v1

**Deliverables**

- Turn order, skills, LB, items, status, damage formula subset
- Victory / defeat / drops
- Top = battle field; bottom = command deck
- Suspend / resume mid-battle

**Acceptance:** Win a scripted 3-wave encounter at ≥20 FPS on n3DS XL.

### S7 — World / town / exploration

**Deliverables**

- Map streaming, collision, NPC interact, chests, transitions
- Bottom mini-map + action prompts

**Acceptance:** Town → dungeon node without softlock; streaming within VRAM budget.

### S8 — Story vertical slice

**Deliverables**

- Dialogue / cutscene compositor
- Mission select; offline AP / energy rules
- Chapter 1 through first dungeon boss

**Acceptance:** Fresh save → clear Chapter 1 boss on hardware; evidence log.

### S9 — Summon / banners offline

**Deliverables**

- Season / collab banner picker (exvius-offline parity)
- Rates UI; multi-pull; unit grant; Lapis economy

**Acceptance:** Pull Standard + one seasonal banner; inventory updates.

### S10 — Vortex, events, solo raids

**Deliverables**

- Event filters (Permanent / Seasons / Collab / Other)
- Raid shared-progress adjusted for solo
- Replay Lapis stage objectives

**Acceptance:** Clear one event stage + one raid reward ladder offline.

### S11 — Espers, Vision Cards, advanced

**Deliverables**

- Esper unlock / grid / equip
- Vision Card equip
- Shops + former cash bundles for Lapis
- Login calendar
- Colosseum lite only if schedule allows

**Acceptance:** Esper + VC affect combat; Lapis bundle purchase; daily login advances.

### S12 — QoL & native feel pass

**Deliverables**

- Shoulder macros; auto-battle presets; dual-pane inspect
- Quick party dock; text scale; colorblind-safe UI
- Home / sleep lifecycle; battery-aware frame pacing

**Acceptance:** Playtest checklist marks “feels handheld-native” on n3DS XL.

### S13 — Perf, memory, content breadth

**Deliverables**

- Frame budgets; texture LRU; SD streaming
- Chapter / season coverage matrix
- Crash telemetry; known-issue triage

**Acceptance:** Stable 20–30 FPS combat; no OOM on long session.

### S14 — Hardware matrix & release

**Deliverables**

- n3DS XL / LL validation matrix
- Reproducible `.3dsx` build; source-only tag
- Compatibility notes; no SE assets in release artifacts

**Acceptance:** Owner-signed hardware evidence; RC tagged.

---

## Native QoL (must-ship)

| Feature | Intent |
| --- | --- |
| Dual-pane inspect | Art/stats on top while equipping on bottom |
| Shoulder macros | L items · R esper · ZL auto · ZR skip text |
| Auto-battle presets | Bottom-screen saved AI loadouts |
| Quick party dock | Swap units without leaving exploration |
| Always-available Vortex | Season filters like exvius-offline |
| Replay Lapis farms | Repeat stage objectives for Lapis |
| Suspend-safe saves | APT home/sleep mid-battle & exploration |
| Readable 240p UI | Large tap targets, high-contrast FFBE panels |

---

## Risk register

| Risk | Mitigation |
| --- | --- |
| 14.5 GB mobile assets vs 6 MB VRAM | PC bake: downsample, atlas, streamed tiles, LRU; chapter packs on SD |
| Cocos Android client not portable | Reimplement; offline server + contract docs as oracle |
| Combat / skill surface area | Vertical slice first; golden tests; expand by content priority |
| Copyright / distribution | Source-only repo; user-owned data; never commit APK/MST/CPK |
| Old 3DS too weak | New 3DS XL exclusive; Old 3DS out of scope |

---

## Recommended first build order

1. **S0 + S1** — lock architecture and ship a dual-screen shell.
2. **S2 ∥ S3** — data pipeline in parallel with service stubs.
3. **S4 → S5** — GUI and party before combat (FFBE is menu-heavy; bottom screen is the product).
4. **S6 → S8** — battle, exploration, Chapter 1 vertical slice = first “fully playable” gate.
5. **S9 → S14** — offline feature parity, native QoL, ship.

## Upstream references

- [Elvlin/exvius-offline](https://github.com/Elvlin/exvius-offline) — offline Android preservation client + local server
- `source/docs/analysis/FFBE_SERVER_CONTRACT.md` — client↔server surface inventory
- [Hirurawa/ExviusReborn](https://github.com/Hirurawa/ExviusReborn) — clean-room systems notes (Godot; not a 3DS target)
- PaperBoat3DS milestone/evidence discipline for 3DS homebrew delivery
