# G8 report — post-first-draw SSX replay (re-run the G7 table on drawn content)

Brief: G8 (this turn) — executes the ONE next action from `docs/reports/G7.md`
§6 (E3 row): capture ONE post-first-draw dump with a draw-aware trigger, then
re-run the exact G7 replay table. Tables + hypothesis + next-action
recommendation, no verdicts. Time box 6 h (used ~2.5 h). Read first per the
brief: `docs/reports/G7.md` (all) and
`docs/research/review-2026-09-20-first-frame-and-gs.md` §paraLLEl-GS.

Machine: same as G7 (Apple M4, 10 cores, macOS 27.0, cmake + ninja, brew
MoltenVK 1.4.2 / vulkan-headers+tools 1.4.357 — no new installs this brief).
Pin: paraLLEl-GS `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd`
(Granite submodule `16e7395f6a4858c1783dbf6f521f90b9d5f82ac5`).
Candidate treated as an independent implementation, not an oracle.

## 0. Frontier inputs received mid-brief (recorded verbatim in effect)

1. **Evidence correction** (before capture): (1) Transfer count > 0 does NOT
   prove raster draws — the dump must show decoded primitive kicks / draw work,
   and a NONBLACK reference/game image must be inspected. (2) Alignment: G7's
   PPM hook exports the LAST vsync while the dump PNG is at START — align the
   EXACT field/frame and CRTC geometry for drawn content; do not reuse the
   all-black center-crop proxy. (3) Odin project init/replay REMAINS open
   despite 10/10 queries — do not upgrade that claim. (4) Storage caps track
   ALLOCATED bytes / free-space delta (ExFAT note) — table allocated, not just
   apparent.
2. **Tooling shortcut** (before inventing counters):
   `GSInterface::consume_flush_stats()` already exposes num_primitives,
   num_render_passes, palette updates, copies — use those counters to establish
   raster work in the dump/replay. Timing note: `GSOptions::timestamps` +
   `get_accumulated_timestamps(TimestampType)` exist for future timing; the CLI
   currently does not enable/report them, and uncalibrated host/GPU clocks alone
   do NOT prove ordinary GPU interval queries unavailable. No extra performance
   run required.

How applied: (1) raster work is established with the project's own flush
counters (per-vsync series, §3b) — a planned Python GIFtag walker was dropped
as superseded; ref + park + both scanouts visually inspected (§2/§3). (2) the
replayer hook now saves the FIRST iterate-true scanout (field-mapped to
dump-vsync#0, §3a) and pixel-diff runs only on exact geometry match — the G7
center-crop proxy was NOT reused. (3) no Odin work was done or claimed; E2
stands as G7 left it. (4) §0 tables apparent + allocated + df deltas.

## 0. Byte caps (declared) vs actuals (apparent + allocated)

| class | cap | actual apparent | actual allocated / delta |
| --- | --- | --- | --- |
| bytesize new clones | 0 (reuse pcsx2-g7 tree) | 0 | 0 |
| bytesize dat-g8 (copy of G7 dat, logs cleared pre-run; incl. run emulog) | 8 GB | 23,510,693 B pre-run; +358,483,122 B emulog during run | 378,536 KiB total (`du -s`) |
| bytesize build growth (incremental `-j2`) | 2 GB | +16,569 B (587,204,723 → 587,221,292) | build total 577,568 KiB |
| bytesize emulog/full traces | stay on bytesize; counts only | 358,483,122 B stayed | — |
| Task 1 retrieval (ONE dump + ONE png + park) | 1 GB | 5,517,531 + 3,391 + 72,050 = 5,592,972 B to `/Volumes/Extreme SSD/ps2x-g8/` | — (network) |
| SSD G8 dump dir (new; +2 PPMs written by replay) | 2 GB | 6,969,258 B + `._` sidecars | 16,384 KiB (`du -sk`) |
| SSD clone/build | 0 new (reuse; rebuild `-j2` in place — tree moved by design, §3) | replayer binary +336 B (51,867,016 → 51,867,352) | build dir 3,808,256 KiB (~3.63 GB, no material growth); `df` 459 Gi avail unchanged start→end |
| internal volume (`/`) | 0 (no installs) | 0 installs; `/tmp/g8-*` ~60 KB | `df` 15 Gi avail unchanged start→end (the G7-end 19 Gi → G8-start 15 Gi delta is other-lane activity, not this brief) |
| committed to git | text only (report + scripts) | this file (ps2xGS) + REPORT.md + 5 scripts (ssx3 mirror); no captures, no binaries, no build dirs | — |

No code copied into any GPL tree. Local experiment changes (all in the
external-SSD clone or the bytesize G7 clone; none committed anywhere):
G7's `timer.cpp` shim + G7 PPM hook (superseded, see §3) + G8 first/flush hook
(`tools/gs_dump_replayer.cpp`), PCSX2 G7 hunks + G8 trigger hunks (bytesize
only, recipe in `g8-build.sh`).

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | The pinned replayer replays a post-first-draw 5-frame dump with transfer packets and emits a scanout measurably comparable to PCSX2's aligned screenshot |
| observable signal | exit code, first-vsync PPM, diff stats, wall-per-unit, heap |
| alternatives | (a) draw-aware trigger fails → table + recipe, stop; (b) replay errors on transfers → table, stop; (c) large diffs → table numbers + method limits, no verdict |
| stop condition | one dump, one replay configuration (native/X1, defaults) — no tuning loop |
| outcome → next action | numbers name the next single experiment (§4) |

Verdict granularity (set by the frontier correction): the hypothesis has two
separable claims — (i) replay executes drawn content without errors,
(ii) first-frame scanout is measurably comparable to the aligned screenshot.
§4 scores them separately.

## 2. Task 1 — post-first-draw dump

### 2a. Clone + hunk verification (pre-patch)

| item | observed |
| --- | --- |
| `/home/brad/pcsx2-g7/pcsx2` HEAD | `9056c08349cc29ad02a6d1a3a4133259019195af` (== G7's T4 rev) |
| G7 tree status | ` M pcsx2/GS/GS.cpp`, ` M pcsx2/R5900OpcodeImpl.cpp` (G7 hunks present: `G7_DUMP_QUEUED` at GS.cpp:454, counter decl + increment at R5900OpcodeImpl.cpp:252/:990, global scope) |
| T4 tree (`/home/brad/pcsx2-t4/pcsx2`) | same rev, `status --short` clean |
| R1 tree (`/home/brad/pcsx2-r1/pcsx2`) | same rev, only its own 1-line R5900OpcodeImpl.cpp diff (no G7/G8 content) |
| parallel-gs clone (`/Volumes/Extreme SSD/parallel-gs-g7`) | HEAD `3a66c197…`, Granite `16e7395f…`, diffs exactly G7's two (`Granite/util/timer.cpp` shim shown as submodule `m`, `tools/gs_dump_replayer.cpp` hook) + one ExFAT `._` sidecar |
| replayer binary (pre-G8-hook) | 51,867,016 B (G7 §2a quoted the pre-hook 51,850,328 B; +16 KB is the G7 hook) |

### 2b. Draw-aware trigger design (implemented, then built)

Counter site: `GSState::Transfer` (`pcsx2/GS/GSState.cpp:3526-3529`), two lines
above the single `m_dump->Transfer(index, start, mem - start)` emission, under
the identical `mem > start` condition — so the counter counts exactly the
Transfer packets a dump would record (the writer additionally returns early on
`size == 0`, which `mem > start` already excludes). Thread: the MTGS ring
consumer in `pcsx2/MTGS.cpp` calls `GSgifTransfer*` → `g_gs_renderer->
Transfer<>`, i.e. increments happen on the GS thread; the vsync hook only
*reads* the atomic. GSDump framing re-verified from the writer for the census:
Transfer = `u8(0)+u8(index)+u32(size)+size` bytes;
VSync = `u8(3)+regs[8192]+u8(1)+u8(field)`;
ReadFIFO = `u8(2)+u32`; header = `u32(fakeCRC) + u32(header_size)` + 36 B
header + serial + screenshot + state + regs[8192] (values in §2d).

Trigger (replaces G7's entry+60 queue call; the ExecPS2 epoch counter is kept):
arm on the first `GSvsync` with ExecPS2 ≥ 5 (baselining pre-entry BIOS traffic
out); record the first vsync with a non-zero transfer delta
(`G8_FIRST_NONZERO vsync=%d transfers=%d`); queue `GSQueueSnapshot("", 5)` once
the post-first-nonzero transfer delta reaches **K = 500**
(`G8_DUMP_QUEUED vsync=%d transfers=%d K=500`). K rationale (tabled per brief):
small enough to stay near first draw so the 5-frame window holds drawn content,
large enough to be past a single partial upload burst; the run then shows what
500 transfers cost in vsyncs (§2c: 169 — the loading trickle, see below).

Build: `g8-build.sh` (4 hunks with `count == 1` asserts, all passed) +
incremental `ninja -j2`: 3 steps (GS.cpp.o, GSState.cpp.o, link) →
`BUILD_EXIT:0` in ~9 s (warm ccache/WSL, load 0.00 — no P-lane contention).
Binary sha `06f140a8b5fe789d9c1335d2813e4495a25245257e963d139beee2775f4f4f53`,
130,938,392 B. R1/T4 bins+trees re-verified untouched after the build
(`6069b91b…`, `6719f5d6…` — both match G7's receipts).

### 2c. Bounded capture run (wall cap 150 s)

`g8-run.sh`: G8 binary, same flags/path as G7 (`-nogui -slowboot -turbo`,
Xvfb :99), own `dat-g8` (G7-dat copy, logs+snaps cleared; ini re-verified:
`GSDumpCompression = 0`, `ScreenshotSize = 1`, `ScreenshotFormat = 0`).

| item | value |
| --- | --- |
| wall | 150 s sleep; uptime 85 → 252 (167 s incl. overhead); clean SIGTERM |
| game entry | `Bios call: ExecPS2` ×5 (same epoch as G7/R1); `Bios call:` ×1,816,608 (G7: 1,813,775 — same boot path) |
| first draw signal | `G8_FIRST_NONZERO vsync=907 transfers=178853` ×1 (emulog; ×0 stdout) at emulog t=36.98 s — 178,853 pre-entry BIOS/boot transfers baselined out by the arming logic |
| dump trigger | `G8_DUMP_QUEUED vsync=1076 transfers=179358 K=500` ×1/×0 at t=40.87 s — delta 505 ≥ 500 over **169 vsyncs** (~3 transfers/vsync: the game is on a loading screen, not yet rendering a scene) |
| old trigger | `G7_DUMP_QUEUED` ×0/×0 (replaced, not duplicated) |
| window | `SSX 3_SLUS-20772_20260920212606.gs` + same-stamp `.png` (only files in clean snaps dir) |
| run-end proof | `g8-park.jpg`, 72,050 B, 1280×1024 — visually inspected: SSX 3 attract-mode mountain scene (snow, trees, EA watermark, turbo fast-forward badge) — the game renders fully by run end; WID 2097159 |
| emulog | 358,483,122 B stays on bytesize (counts only retrieved) |

Note: WSL wall clock trails the Mac by ~3 h (bytesize said Sep 21 01:24 UTC
while the Mac said Sep 20 21:29 PDT = Sep 21 04:29 UTC); all wall evidence
above is `/proc/uptime`-based and unaffected.

### 2d. Dump provenance + version check (G7 §3a shape)

`g8-census.py` (framing from §2b; validated against the G7 dump as a negative
control: reproduces G7's sha `74d55f1e…`, version 9, EOF-sync, 0 Transfer,
all-black embedded shot).

| item | value |
| --- | --- |
| file | `SSX 3_SLUS-20772_20260920212606.gs`, 5,517,531 B, sha256 `64f6cddfced79692bfd6bb111841b78e2337c0a6f693a18c0d6e2c2f752fc21b` (SSD copy sha-matches the bytesize original) |
| producer | G7 PCSX2 clone + G8 trigger hunks (§2b); binary sha `06f140a8…`, 130,938,392 B |
| pre-existing trees | T4 + R1 bins/trees re-verified untouched after the G8 build (shas match G7) |
| trigger | `GSQueueSnapshot("", 5)`, K=500 transfers after first-nonzero vsync#907 (game entry gated); queued at vsync#1076 |
| run | 150 s wall, recompiler + turbo, Xvfb :99, WID 2097159, clean SIGTERM; emulog 358,483,122 B stays on bytesize |
| ini (dat-g8 copy; G7 dat + R1 dat untouched) | `GSDumpCompression = 0`, `ScreenshotSize = 1` (internal), `ScreenshotFormat = 0` (png) |
| dump version | file header version **9** ∈ parser range 8..9 → ACCEPT (verified from bytes both sides: dump header bytes + `dump/gs_dump_parser.hpp:15-16` `STATE_VERSION(_MIN) = 9/8`; the producer side is proven by the version-9 bytes this dump carries; README's "version 8" still stale, never consulted) |
| header cross-check | fakeCRC `0xffffffff`, header_size 1228846 = 36 + 10 (serial `SLUS-20772`) + 1228800 (640×480×4 shot); crc `0x08fff00d`; state_size 4194813 (== G7 — same boot-state size); packet region EOF-syncs after header + state + 8192 regs |
| packet census (EOF-synced, exact) | 8 Vsync (phases 1,0 alternating = interlaced fields, == G7), 8 PrivRegisters, **88 Transfer, 19,584 GIF bytes**, 0 ReadFIFO. ALL transfers on path index 3; EVERY vsync carries exactly 11 transfers / 2,448 B (packet-region delta vs G7 = 20,112 B = 88×6 header + 19,584 payload, exact) |
| per-vsync transfer map | all 8 dump-vsyncs have transfers → default-mode iterate-true #N ↔ dump-vsync #N; **first iterate-true ↔ dump-vsync#0 (phase 1)** |
| aligned reference | `…2606.png`, 3,391 B, **597×448** RGB, 200 distinct colors, 436 nonblack px (0.16%) — visually inspected: black + small white snowflake loading icon, bottom-right |
| embedded shot | dump-header 640×480 u32, 210 distinct, 514 nonblack (0.17%) — black + icon, consistent with the ref |
| run-end proof | `g8-park.jpg` (§2c) — full attract-mode scene; the dump window (trigger+5f) is the loading screen, i.e. minimal but genuinely drawn content |

Census gate: transfers = 88 > 0 → proceed to Task 2 (no stop). Content
character: uniform 11×~223 B path-3 uploads per field = loading-icon animation
traffic (setup + small draws), not a scene — the flush series in §3b says how
much of it is raster work.

## 3. Task 2 — replay + delta table (Mac)

### 3a. Hook change (replay-alignment requirement, not tuning)

The G7 hook saved only the leftover LAST vsync. Per the frontier alignment
correction, `tools/gs_dump_replayer.cpp` now (local change, uncommitted):
(1) stashes the first iterate-true scanout handle of the last pass in-loop
(handle swap only — PPM readback/write stays after `end_ns`, so wall-per-unit
timing is comparable to G7) and saves it as `.g8-first.ppm`;
(2) keeps the leftover-last save as `.g8-last.ppm` (same consume as G7);
(3) logs `consume_flush_stats()` per last-pass vsync (take+reset semantics
verified at `gs/gs_renderer.cpp:1256-1261`; a baseline consume after
`parser.open` discards state-load counters);
(4) logs per-scanout `internal/mode WxH, interlaced, interlace_phase` for the
alignment table.
Loop-body execution count is unchanged (the `do/while` became an equivalent
`for(;;)`+`break`). Rebuild `-j2`: exit 0; only the pre-existing `-Wshadow`
`FileDeleter` warning (+ an `ld` duplicate-libraries notice). The tree moved by
brief design (alignment data the G7 tree cannot produce), not by drift.

### 3b. Replay table (image + diffs + time + memory; G7 §3b shape)

Primary = the brief's configuration (replayer defaults = native res, X1
`SuperSampling::X1`, `--iterations 2` for a warmed second pass). Dump path
first (argv[1]).

| # | measure | G8 (native/X1/defaults) | G7 baseline | G7→G8 delta |
| --- | --- | --- | --- | --- |
| 1 | image (first) | `….gs.g8-first.ppm`, **512×448, ALL BLACK** (229,376 px, 1 color; internal=mode 512×448, progressive `force_progressive`, phase **1**) | last-vsync 640×448 all black (no first saved) | geometry 640→512 wide (content/state-dependent CRTC); first now captured |
| 1b | image (last) | `….gs.g8-last.ppm`, 512×448, **216 colors, 607 nonblack px (0.26%)** — visually inspected: black + white snowflake loading icon, bottom-right (phase **0**) | — (G7's only scanout was the last) | draw core output now visible |
| 2 | diffs vs aligned ref (`…2606.png`, 597×448, 200 colors, 436 nonblack 0.16%, icon bottom-right) | **NO pixel diff: GEOMETRY_MISMATCH 512×448 vs 597×448** (widths differ; heights match). No crop proxy used. Non-spatial: FIRST all-black vs REF icon-present (content differs at dump start); LAST icon-present, 607 vs 436 nonblack px (same icon, same corner — see visuals). Same-geometry FIRST-vs-LAST: 99.74% exact, PSNR 30.16/30.16/30.16 dB (differ only in the icon region) | exact 1.0000 on 640×448 center-crop overlap (black-on-black; proxy now retired) | comparability REGRESSED from exact-overlap to blocked: geometry + first-black gaps (§3c) |
| 3 | time | **0.758 ms per counted unit** (9 warmed units = 8 iterate-trues + terminal check; 18 `Running frame` lines = 9 cold + 9 warmed; ≈6.8 ms warmed pass) | 3.080 ms/unit (1 unit incl. file re-read + 4 MB state upload) | per-unit NOT directly comparable (fixed costs amortized over 1 vs 9 units); warmed-pass totals ≈3.1 ms → ≈6.8 ms. Same caveat: host wall, not GPU time — and per the frontier timing note, `GSOptions::timestamps` exist but the CLI doesn't enable them (G7 gap #2's "no timestamp path" wording is corrected: the path exists, it is just not wired to the CLI) |
| 4 | memory | Heap 0 DEVICE **453/459 MiB cold → 454/460 MiB warmed** (tracked/device) | 327 tracked / 328 device MiB, identical both passes | **+127/+132 MiB** — drawn content materially grows image-pool use |
| 5 | raster work (NEW — project's own counters) | per last-pass vsync: **#0: 306 prims / 18 passes / 9 pal** (scratch 1,158,768 B, img 1,052,672 B); **#1–#7: 34 / 2 / 1 each** (scratch 128,752 B, img 0). Totals: **544 primitives, 32 render passes, 16 palette updates, 0 copies**. Draw core exercised (lightly — loading-icon load, not scene load) | — (no counters; 0 transfers) | zero → light raster load |
| — | exit / errors | 0; no `Minimum requirements`; no device errors; only the 2 known benign ERROR lines (calibrated-time domain, RenderDoc — same as G7); 3 sync shader compiles on the cold pass (`success: yes`) vs G7's 1 | 0; same benign lines; 1 compile | +2 compiles (drawn content needs more shaders) |

### 3c. Alignment table (exact field/frame + CRTC geometry)

| link | value |
| --- | --- |
| dump-vsync#0 (census) | phase 1, 11 transfers / 2,448 B — first iterate-true maps here (§2d) |
| FIRST scanout (hook log) | phase 1, progressive 512×448 — **exact field match** with dump-vsync#0 |
| LAST scanout (hook log) | phase 0 = dump-vsync#7's phase — exact match |
| REF PNG | pre-vsync#0 frame (dump-start screenshot), 597×448 |
| frame adjacency | REF (pre-#0) → FIRST (#0): adjacent; icon present in REF, absent in FIRST |
| CRTC geometry | replayer internal=mode=**512×448** progressive vs PCSX2 internal screenshot **597×448** — widths differ by 85 px; pixel-diff blocked. (G7: 640×448 vs 640×480 — widths matched, heights differed. The mismatch axis is display-state-dependent.) |

Two independent gaps block "measurably comparable": (i) the 512-vs-597 width
gap (method gap — no same-geometry pair exists in this configuration);
(ii) FIRST-black-while-REF-shows-icon (content gap at the aligned frame —
rendering demonstrably happens on vsync#0 with 306 prims, yet that vsync's
scanout is black while the icon appears by the last vsync; candidate
explanations include first-displayed-frame flip/scanout-state lag, but the
brief's no-tuning stop rule leaves the cause OPEN).

## 4. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| (i) The pinned replayer replays a post-first-draw 5-frame dump with transfer packets without errors | **SUPPORTED**: exit 0, no device errors, 88 transfers replayed, 544 primitives / 32 passes executed, loading icon rendered by the last vsync (visually confirmed, same icon/corner as the ref) |
| (ii) …and emits a scanout measurably comparable to PCSX2's aligned screenshot | **NOT SUPPORTED on this dump**: no pixel comparison was possible (512-vs-597 geometry) and the aligned first scanout is black while the ref shows the icon. Numbers + method limits only (§3c); no verdict on the renderer's fidelity beyond what is tabled |

The ONE next action the numbers justify (adoption input, not an adoption
decision): **run the geometry-alignment probe on THIS dump (no new capture)** —
derive the expected scanout width from the dump's own PrivRegisters
(DISPLAY/DISPFB/SMODE2), compare it against the replayer's internal/mode width
(512) and PCSX2's screenshot-crop width (597) to explain the 85 px gap, and test
whether any replayer CRTC flag (`--high-res-scanout`, `--conservative-crtc`)
yields a 597-wide scanout; if a matched-geometry pair is found, re-diff
FIRST-vs-REF at exact geometry. Queued after it (not this action): a per-vsync
scanout series to localize the first-black frame, and a rich post-loading dump
once the comparison method is unblocked. Rationale for geometry-first: the
width gap is display-state geometry, not content — no future dump (however
rich) can satisfy claim (ii) until a same-geometry pair exists.

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.
Odin on-device init/replay REMAINS open (G7 §5 recipe on file, untouched —
capability queries only, per the frontier instruction).

## 5. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (clone HEAD re-verified; shallow) |
| G8 replayer hook | local experiment change, uncommitted in the SSD clone only (`tools/gs_dump_replayer.cpp` — file already carries LGPL-3.0+ SPDX; no license change, nothing copied anywhere) |
| PCSX2 G8 clone hunks | T4 rev `9056c08349…` + R1 one-liner + G7 hunks + G8 trigger hunks, all uncommitted in `/home/brad/pcsx2-g7` only (GPL-3.0+ tree, no license mixing — nothing copied out; recipe committed here as `g8-build.sh`) |
| G8 scripts | authored this brief (`g8-build/setup/run.sh`, `g8-census.py`, `g8-diff.py`) — mirror carries them as text |
| brew tools | none installed this brief (G7's set reused as-is) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 6. Exact commands

Mac (`/Users/bradrichardson/dev/ps2xGS` unless noted; `COPYFILE_DISABLE=1` on
SSD steps; `VK_ICD_FILENAMES=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json`,
`DYLD_LIBRARY_PATH=/opt/homebrew/lib` on replayer runs):

```text
git -C "/Volumes/Extreme SSD/parallel-gs-g7" rev-parse HEAD   # 3a66c19… (+ Granite 16e7395…)
cmake --build /Volumes/Extreme\ SSD/parallel-gs-g7-build --target parallel-gs-replayer -j2   # after G8 hook edit; exit 0
./tools/parallel-gs-replayer "<dump>.gs" --iterations 2       # dump FIRST (argv[1]); defaults
python3 /tmp/g8-census.py <dump.gs> [<ref.png>]               # provenance + EOF-synced census + map
python3 /tmp/g8-census.py <g7-dump.gs>                        # negative control (reproduces G7)
python3 /tmp/g8-diff.py <first.ppm> <last.ppm> <ref.png>      # exact-geometry-or-stats diff
sha256sum <retrieved .gs>                                     # == bytesize sha
du -sk <ssd dirs> ; df -h / "/Volumes/Extreme SSD"            # allocated + deltas
```

bytesize (each via ONE `ssh bytesize 'wsl …'`, `;` separators, no inline pipes;
scripts staged `scp … "bytesize:pcsx2-t4/"` then
`wsl cp /mnt/c/Users/bradr/pcsx2-t4/g8-*.sh /home/brad/pcsx2-t4/`):

```text
wsl git -C /home/brad/pcsx2-g7/pcsx2 rev-parse HEAD; git … status --short; …   # rev + hunk verification
wsl grep -n Transfer …/GS/GSDump.h                                            # dump Transfer interface
wsl grep -rn -m30 "DumpTransfer|m_dump->Transfer|dump.*Transfer" …/GS          # emission site: GSState.cpp:3527
wsl sed -n "3495,3545p" …/GS/GSState.cpp                                      # emission context
wsl grep -n -m10 "GSState::Transfer" …/GSState.cpp …/GSState.h                 # template + definition
wsl grep -n -i -m5 mtgs …/r1/dat/PCSX2/inis/PCSX2.ini                          # SynchronousMTGS = false
wsl grep -n -m8 "Console.WriteLn" …/GS/GS.cpp                                 # printf-style logging form
wsl grep -rln -m1 "GSgifTransfer" …/pcsx2 --include=*.cpp --include=*.h        # MTGS.cpp consumer (GS thread)
wsl sed -n "1,140p" …/GS/GSDump.cpp ; sed -n "1,110p" …/GS/GSDump.h            # writer framing (walker-grade)
wsl sed -n "140,200p" …/GS/GSDump.cpp                                         # AppendRawData encodings
wsl bash /home/brad/pcsx2-t4/g8-build.sh                                      # patch (4 asserts) + ninja -j2
wsl bash /home/brad/pcsx2-t4/g8-setup.sh                                      # dat-g8 copy + clean + ini verify
wsl bash /home/brad/pcsx2-t4/g8-run.sh                                        # 150 s wall run, auto dump, post counts
wsl stat -c "%s %n" <snaps>/* <park> <emulog> ; sha256sum <snaps>/*.gs        # pre-retrieval sizes + sha
wsl cp <snaps>/*.gs <snaps>/*.png <park> /mnt/c/Users/bradr/pcsx2-t4
scp "bytesize:pcsx2-t4/<artifacts>" "/Volumes/Extreme SSD/ps2x-g8/"
wsl du -s <dat-g8> <build> <logs> ; df -k /home/brad                         # allocated actuals
```

Local experiment diffs (uncommitted, in the SSD clone): G7's `timer.cpp` shim
(kept), `tools/gs_dump_replayer.cpp` G8 hook (first-stash + flush series +
phase logging; supersedes G7's last-only block); bytesize:
`pcsx2/GS/GS.cpp` + `pcsx2/GS/GSState.cpp` (committed here as `g8-build.sh`
python hunks; R1 one-liner + G7-EE counter kept).

## 7. Gaps (what this brief could not do)

1. First-scanout-black cause: OPEN. Rendering demonstrably runs on vsync#0
   (306 prims) yet that scanout is black while the ref (pre-#0) shows the icon.
   Needs the queued per-vsync scanout series, not speculation.
2. 512-vs-597 width gap: OPEN (the §4 next action). No same-geometry pair
   exists in this configuration, so no pixel-diff claim is made at all.
3. Loading-icon content is a light draw load (544 prims / 32 passes over 8
   vsyncs). It exercises init + upload + draws + scanout, but not scene-scale
   rasterization, hazards, or texture pressure. A rich post-loading dump is
   queued behind the geometry probe.
4. No isolated GPU time: host wall only. `GSOptions::timestamps` +
   `get_accumulated_timestamps` exist in the pinned source but the CLI does not
   enable/report them; wiring them (or an external profiler) is future work —
   explicitly not this brief's.
5. WSL wall-clock skew (~3 h vs the Mac) — all wall evidence is
   `/proc/uptime`-based; wall-clock stamps from bytesize are offset, not wrong
   in duration.
6. Odin on-device project init/replay: still OPEN (G7 §5 recipe on file; no
   change, no claim upgrade).
7. `upstream/` and ps2xGS harness code untouched; ps2xGS `.gscap` captures are
   a different format from PCSX2 `.gs` dumps (no adapter attempted or implied).
8. Session-only `/tmp/g8-*` logs (~60 KB); external-SSD artifacts (G8 dump dir
   ~7 MB apparent / 16 MB allocated; clone/build reused) retained, none
   committed.

## 8. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…` (+G7 shim, +G8 hook).
- Build: `/Volumes/Extreme SSD/parallel-gs-g7-build/`, `tools/parallel-gs-replayer` (51,867,352 B).
- Dump dir: `/Volumes/Extreme SSD/ps2x-g8/` (.gs + .png + park jpg + 2 PPMs).
- Logs: `/tmp/g8-pcbuild.log`, `/tmp/g8-run.log`, `/tmp/g8-replay.log`,
  `/tmp/g8-replayer-build.log` (session-only).
- Tools: `/tmp/g8-build.sh`, `/tmp/g8-setup.sh`, `/tmp/g8-run.sh`,
  `/tmp/g8-census.py`, `/tmp/g8-diff.py` (mirrored to ssx3; `/tmp` originals
  are session-only).
- bytesize: `/home/brad/pcsx2-g7/` (source+build+dat-g8+logs+emulog), scripts at
  `/home/brad/pcsx2-t4/g8-*.sh` + `C:\Users\bradr\pcsx2-t4\g8-*.sh` staging
  copies (+ staged `.gs/.png/.jpg` retrieval copies).
- Commits: ps2xGS `[G8]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G8/` `[G8]` + same trailer (NOT pushed).
