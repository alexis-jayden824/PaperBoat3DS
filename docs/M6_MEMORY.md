# M6 Memory Strategy

M6 establishes an Old Nintendo 3DS-first memory policy before the complete
PaperBoat runtime is linked. The limits below are deliberately conservative
engineering guardrails, not claims about final gameplay usage.

## Provisional runtime budgets

| Concern | Limit or reserve | Enforcement |
|---|---:|---|
| Application free reserve | 8 MiB | Reject tracked heap allocations before crossing it |
| Linear free reserve | 4 MiB | Reject linear allocations before crossing it |
| Archive/index allocations | 2 MiB | Per-class accounting and hard cap |
| Scene allocations | 12 MiB | Per-class accounting and hard cap |
| Transient allocations | 2 MiB | Per-class accounting and hard cap |
| Linear allocations | 6 MiB | Per-class accounting and hard cap |
| Stack high-water warning | 512 KiB | Runtime pressure flag |
| Archive read chunk | 64 KiB | `pb_archive_read` clamps each read |

`PBMemoryMonitor` records initial and current application/linear free space,
peak deltas, stack distance from the startup anchor, live and peak usage for
each allocation class, rejected/failed allocations, and a memory-pressure
flag. Folium's known zero application-free result is treated as unavailable;
linear, stack, class, and CI telemetry remain active.

All future scene, archive-cache, transient, and linear allocations must use
`pb_memory_alloc`/`pb_memory_free` or reserve the same class through an
equivalent audited owner. A failed budget or system allocation returns `NULL`
and increments the failure counter; callers must degrade, unload, or show a
fatal diagnostic rather than continuing with invalid memory.

## Build-time budgets

`make m6-budget-check` records ELF `text`, `data`, and `bss` sizes and fails CI
when either threshold is exceeded:

- Loadable image (`text + data`): 16 MiB
- Static RAM (`data + bss`): 8 MiB

The report is uploaded as `build/memory-budget.txt` beside every build. These
ceilings can be tightened as more game code is linked; they prevent accidental
large static caches or embedded assets now.

## Hardware evidence still required

Folium is useful for presentation and failure-path checks but cannot establish
Old 3DS memory ceilings. Before M6 closes, record the following on real
hardware from a build with an SD log:

1. Cold boot, idle for 30 seconds, suspend/resume once, then exit normally.
2. Capture the startup and shutdown memory log lines.
3. Record model, firmware, build SHA, application/linear free values, peak
   deltas, stack peak, allocation failures, and pressure state.
4. Repeat after a legal `paperboat.o2r` is present once the M7 pipeline can
   create it; archive access must remain streaming and bounded.

The user's New 3DS XL/LL result is valuable secondary evidence. The Old 3DS
baseline remains authoritative per the roadmap and may require a later tester.
