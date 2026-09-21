# G13 report — rich post-loading dump dual replay with corrected labels

Brief: G13 (this turn) — executes G12's §4: capture a RICH post-loading dump
(scene-scale content) and dual-replay it (PCSX2 gsrunner + paraLLEl) with
CORRECTED labels (file N ↔ post-vsync#N) and S-lag-aware windowing. Tables +
hypothesis + next-action recommendation, no verdicts beyond the hypothesis.
Time box 6 h (used ~4 h). Read first per the brief: `docs/reports/G12.md`
(all) + `docs/reports/G10.md` §3c (gsrunner build/run recipe). The G8–G11
"lag" line is RETIRED per G12 (cited, not re-litigated).

Machine: same as G8–G12 (Apple M4, macOS 27.0 — no new installs) + bytesize
WSL (Ubuntu 24.04, clang, cmake, Ninja, llvmpipe — G10's build cache reused,
no new toolchain). Pins: PCSX2 `9056c08349cc29ad02a6d1a3a4133259019195af`
(re-verified, §2a) / paraLLEl-GS `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd`
(re-verified, §2a) / rich dump sha `154d9d85…` (new, §2d).

Headline result: alternative (b) — agreement throughout; the calibrated
method holds at scene scale. The ONE bounded capture run fired the new
rate-based scene trigger at live vsync#1608 (loading determinism receipt:
`G8_FIRST_NONZERO vsync=907 transfers=178853` reproduces G8 EXACTLY) and
yielded an 11.5 MB / 8-vsync dump (4,585 transfers, 6.0 MB GIF, 590–918 KB
host→local uploads + ~98 draws per vsync — a flash-transition workload at
~33× G8's draw rate and ~300× its traffic). Capture-side per-draw trace
(785 DRAW / 777 RASTER, via the m_dump-gated G12 logs) and replay-side trace
are SEQUENCE-IDENTICAL (all 785 (FBP/FBW/FPSM/TBP0/TBW/TPSM/TME/PRIM)
tuples + all 777 raster targets, in order). paraLLEl records composite-112
before scene-0 in every iterate with a 96-texture order IDENTICAL to PCSX2's
window#0 textured-draw order. Same-boundary diffs at all 7 corrected pairs:
every pixel within ±2 (le2 = 1.0000), PSNR 49–61 dB; the (4,5)/(6,7)
pairwise-identical frames reproduce on BOTH sides (content, not artifact).
Post-#0 is black on both sides (initial VRAM block-0 observed cleared, as in
G12). No batching divergence found anywhere: the G12 content order
(composite-before-own-scene, lagged trailing prim content-neutral) recurs at
scale with receipts. The emulator-side comparison is CLOSED.

## 0. Byte caps (declared) vs actuals (apparent + allocated)

| class | cap | actual apparent | actual allocated / delta |
| --- | --- | --- | --- |
| bytesize build growth (qt target-only `-j2`: 3 objects + link) | 500 MB | qt binary 130,938,392 → 130,961,672 B (+23,280; sha `06f140a8…`→`1f664fdd…`); gsrunner bit-identical (`8e446ff1…` pre+post) | build dir 666,596 → 666,656 KiB (+60 KiB) |
| bytesize dat-g13 (fresh `dat` copy + run emulog) | 8 GB | 23,510,693 B pre-run (== G8's start byte-for-byte) + 225,632,707 B emulog | 254,676 KiB (`du -s`) |
| bytesize run outputs (`g13-frames` + `emulog-g13.txt` + hook/run scripts) | 500 MB | 7 PNGs (~700 KB) + emulog 139,139 B + scripts ~15 KB | `g13-frames` 916 KiB |
| bytesize staging (`C:\pcsx2-t4\` retrieval copies) | counts only | transient; bulky copies removed after retrieval (small scripts left, G8 precedent) | — (network) |
| bytesize dat-g8 / dat | 0 growth | 0 | 378,536 KiB unchanged (== G8/G10/G12); `dat` untouched |
| Task retrieval (.gs + ref + park + traces + 7 PNGs + emulog) | 2 GB | ~12.7 MB to `/Volumes/Extreme SSD/ps2x-g13/` | — (network) |
| SSD G13 dir (new: .gs + ref + park + traces + 7 PNGs + emulog + 10 PPMs) | 2 GB | ~19.6 MB | 35,840 KiB (`du -sk`) |
| SSD G8/G10/G11/G12 dirs | 0 growth (pristine) | 0 — G8 `.gs` sha + G12 PNG md5s re-verified identical at end | 16,384 / 43,008 / 28,672 / 9,216 KiB unchanged |
| internal volume (`/`) | 0 (no installs) | 0 installs; `/tmp/g13-*` ~40 KB (7 scripts + logs, session-only) | `df` 13 → 10 Gi avail (other-lane — this brief wrote ~40 KB); SSD 425 → 417 Gi avail (other-lane — this brief wrote ~20 MB) |
| committed to git | text only (report + scripts) | this file (ps2xGS) + REPORT.md + 7 scripts (ssx3 mirror); no captures, no binaries, no build dirs | — |

No code copied into any GPL tree. No P-lane contention: T-lane T32 occupied
the lane at first attempt (own `pcsx2-t4` tree, waited 3 min, no kill); no
lease (PCSX2-side, T28 recipe shape); no recomp boots/builds (capture is the
G8-precedented `pcsx2-g7` path); no fork writes, no `adb`, no bytesize
builds/runs outside `pcsx2-g7` (`-j2` qt-only rebuild + one capture run + one
`-loop 1` replay). WSL: 2 pre-session `AcceptAsync` kills on the old VM
(@35s/@90s, no ssh in flight) + 1 full VM restart mid-session AFTER all
evidence home (btime `…65798`→`…67511`; dump sha re-verified post-restart,
run window effects-verified per the T27 §4 rule: exit 0 + monotonic uptime
639→736 + clean markers).

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | The calibrated method attributes every draw/present/file on scene-scale content and names any batching divergence with receipts |
| observable signal | per-draw/per-present tables both sides + corrected-label diffs + divergence table (or agreement table) |
| alternatives | (a) divergence found → table + ONE next action (adoption-relevant); (b) agreement throughout → table + recipe, stop (method holds at scene scale); (c) method breaks at scale → table the exact break + repair recipe, stop |
| stop condition | ONE capture + bounded replay pair — no tuning loop, no renderer changes, no new content beyond the one dump |
| outcome → next action | numbers name the next single experiment (§5) |

Outcome: alternative (b) — agreement throughout (per-draw order identical
live-vs-replay, record order same content-order, all 7 corrected pairs
within ±2 LSB, k=0 black-agreement via cleared initial block-0).

## 2. Task 1 — rich capture + calibrated dual replay (one bounded capture)

### 2a. Reuse verification (pre-work)

| item | observed |
| --- | --- |
| bytesize source HEAD | `9056c08349cc29ad02a6d1a3a4133259019195af` (== G8/G10/G12 pin) |
| tree status (pre-work) | ` M pcsx2/GS/GS.cpp, GSState.cpp, GS/Renderers/HW/GSRendererHW.cpp, GSDumpReplayer.cpp, R5900OpcodeImpl.cpp` (G7+G8 hunks + G12 hook, all uncommitted — G12-end state exactly) |
| gsrunner binary (pre-work) | sha `8e446ff1…` (G12-built, 90,455,240 B) |
| qt binary (pre-work) | sha `06f140a8…` (== G8's receipt — G12 never rebuilt qt) |
| SSD clone HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` + `M gs_interface.cpp` (G11 H1–H4) + `M tools/gs_dump_replayer.cpp` (G8+G10 hook) — G11-end state exactly; binary 51,879,240 B present, no rebuild |
| bytesize load at start | 0.07 (idle); WSL `/` 896 G avail; C: 26 G avail |
| G8/G10/G11/G12 SSD dirs | sizes re-verified at end (see §0); content shas re-verified (§8) |

### 2b. The bounded patch (`g13-hook.py`, H1–H10, all asserts passed)

Design: G8's K=500 cumulative trigger would re-fire on the loading screen
(~1076), so the fire condition is REPLACED (arming + FIRST_NONZERO kept as a
determinism receipt). The scene detector needs draw-rate AND byte-rate per
vsync — two new GS-thread atomics next to the G8 counter. The G12 per-draw
logs are gated on `m_dump` so the capture run stays quiet outside the
8-vsync dump window (replay binary already built — its logs stay ungated).

| hunk | file | change |
| --- | --- | --- |
| H1 | GS.cpp | define `g_g13_transfer_bytes` + `g_g13_draw_batches` (ULL atomics) after the G8 counter decl |
| H2 | GS.cpp | comment note that the fire condition is replaced (G8 text kept as history) |
| H3 | GS.cpp | fire-block replace: rate trigger (floor vsync≥1200; fire when one vsync carries ≥25 batches AND ≥16 KiB for 2 consecutive vsyncs) + fallback (vsync≥5000) + `G13_SCAN` every 500 vsyncs; logs `G13_DUMP_QUEUED` / `G13_DUMP_FALLBACK`; same `GSQueueSnapshot("", 5)` window (N=5 → 8 vsyncs via the §2c close rule) |
| H4 | GSState.cpp | extern decls for the two counters |
| H5 | GSState.cpp | Transfer: also count GIF bytes (same `mem>start` condition; braced) |
| H6 | GSState.cpp | FlushPrim: count draw batches (same site as `IncDraw`) |
| H7 | GSRendererHW.cpp | gate H1 `G12_DRAW` on `m_dump` (single-statement `if` — no braces needed) |
| H8 | GSRendererHW.cpp | gate H4 `G12_RASTER` on `m_dump` |
| H9 | GSRendererHW.cpp | gate H2 `G12_SKIP/badframe` on `m_dump` (`return` stays unconditional) |
| H10 | GSRendererHW.cpp | gate H3 `G12_SKIP/blackpoint` on `m_dump` (inside braces; `return` stays unconditional) |

Deliberately unchanged: H5 `G12_VSYNC` (1 line/vsync cadence receipt) and H6
replayer (never executes in qt). Threshold rationale (declared up front):
floor 1200 sits past G8's fire (1076) + window (~1084) + margin; loading
trickle = ~3 draws + ~2.4 KiB/vsync, so 25 + 16 KiB × 2-in-a-row admits only
scene-scale vsyncs while rejecting single-vsync spikes; fallback 5000 ≈
190 s at G8's 26 vsync/s, inside the 280 s wall cap.

Review (before build): full `git diff` read — bare `if/else` in H3 govern
single statements; one-shot preserved via `g8_dump_queued`; H9/H10 leave
their `return`s unconditional. Build: `cmake --build … --target pcsx2-qt
-j2` → exit 0, 4 steps (3 objects + link), one benign new warning
(`g8_first_nonzero_total` set-but-unused — its only reader was the removed
K=500 fire; tabled, no re-patch per ONE-patch discipline). Post-build:
qt sha `1f664fdd…` (changed ✓), gsrunner sha `8e446ff1…` (UNCHANGED ✓ —
replay binary bit-identical, its G12 logs ungated).

### 2c. Dump-window semantics (verified in the pinned tree, not assumed)

`GSQueueSnapshot("", N)` sets `m_dump_frames=N`; the NEXT vsync opens the
dump (no packet that vsync); each following vsync appends one VSync packet
and decrements; `GSDumpBase::VSync` closes when `(++m_frames & 1)==0 && last
&& m_extra_frames<0` — N=5 yields 8 packets (observed G8 + G13). TRXDIR
correction (vs the G12 walker's assumption): the reg carries ONLY `XDIR:2`
(`GSRegs.h:893-897`) — no XFER bit; IMAGE data arrives as GIFtags with
FLG=IMAGE/IMAGE2 (=2/3, `GSRegs.h:133-138`; G12's "reserved" label was a
harmless misnomer — no such tags existed there). REGLIST regs are A_D
addresses directly (handler table `GSState.cpp:749-801`).

### 2d. Bounded capture run (wall cap 280 s + progress cap; `g13-run.sh`)

`g13-setup.sh`: `dat`→`dat-g13` copy (G7 lineage, same as G8) + clean
logs/snaps + ini verify (`GSDumpCompression = 0`, `ScreenshotSize = 1`,
`ScreenshotFormat = 0`); pre-run 23,510,693 B == G8's start byte-for-byte.
First attempt FELL OVER on the T32 lane occupant (own tree — waited, no
kill); second attempt ran clean (uptime 639→736, exit 0, `G13_RUN_DONE`).

| item | value |
| --- | --- |
| game entry | `Bios call: ExecPS2` ×5 (same epoch as G7/G8); `Bios call:` ×1,596,091 |
| loading receipt | `G8_FIRST_NONZERO vsync=907 transfers=178853` ×1 — EXACTLY G8's values (boot determinism through the trigger) |
| coarse scans | `G13_SCAN vsync=1000` (178853 xfers / 62.65 MB / 23692 batches), `vsync=1500` (185105 / 64.64 MB / 25303) |
| scene trigger | `G13_DUMP_QUEUED vsync=1608 transfers=188271 bytes=67252112 batches=26104 streak=2` ×1 at emulog t=48.26 s — rate trigger fired, fallback silent (`G13_DUMP_FALLBACK` ×0, `G8_DUMP_QUEUED` ×0) |
| trigger-vs-scan delta | +9,166 xfers / +2.6 MB / +801 batches over vsyncs 1500→1608 (~85 xfers/vsync — scene onset, vs ~3/vsync loading) |
| window | `SSX 3_SLUS-20772_20260921005452.gs` (11,537,377 B, sha `154d9d8577a210fb…2ad7e32`) + same-stamp `.png` (1,743 B, sha `f1acfd66…`; flat burnt-orange = pre-#0 frame, viewed) — only files in clean snaps dir |
| progress cap | hit at poll 4/14 (~80 s wall; marker + size-stable `.gs` across 2 polls) — run ended early, wall cap untouched |
| run-end proof | `g13-park.jpg` 44,728 B — viewed: attract-mode snowboarder close-up (blue sky, EA watermark, turbo badge); WID 2097159 (same as G8) |
| capture renderer | OpenGL HW on llvmpipe via Xvfb (`Using: OpenGL 4.5 Core`; `G12_DRAW/G12_RASTER` fire ⟹ HW path) |
| emulog | 225,632,707 B stays on bytesize (counts + 4,341-line G12/G13 extract retrieved) |

### 2e. Rich-dump census (`g13-census.py`, EOF-synced, leftover=0 on all 4,585)

Header: version 9 ∈ parser range (ACCEPT, same as G8), state 4,194,813 B
(== G8), serial `SLUS-20772`, crc `0x08fff00d`, 640×480 embedded shot.
8 dump-vsyncs (phases 1,0,… ✓), 0 ReadFIFO, all transfers path 3.

| dump-vsync | phase | xfers | GIF bytes | IMAGE up (host→local) | img tags | shape |
| --- | --- | --- | --- | --- | --- | --- |
| #0, #1 | 1, 0 | 509 | 626,768 | 589,824 B (61 tags) | 61 | 507+2 (extra TAGPRE-TME0 tristrip + 4 kicks + 4 NOPs) |
| #2, #4, #6 | 1, 0, 1 | 507 | 626,512 | 589,824 B (61 tags) | 61 | base: 3 upload dests (DBP 10756/10820/11017) |
| #3, #5, #7 | 0 | 682 | 959,792 | 917,504 B (96 tags) | 96 | stream: 38 upload dests (texture paging) |

Totals: 4,585 transfers / 6,012,448 GIF bytes (~300× G8's traffic).
Per-vsync geometry: 96 TAGPRE-TRISTRIP (TME1, CTXT0) + 2 PACKED-PRIM-SPRITE
(1×TME0 + 1×TME1, CTXT0) + (#0/#1 only) 1 TAGPRE-TRISTRIP-TME0; ~450 kicks
(384–388 PACKED XYZF2 + 66 REGLIST XYZ2); depth active (dsTBP0=7168 on all
scene draws replay-side); tags/vsync ≈ 449–587 PACKED + 2 REGLIST + 61–96
IMAGE. FRAME writes in packets: NONE (all draws take FRAME from
state/PrivRegs — paraLLEl logs `FRAME_1 FBP=112` then `FBP=0` at vsync
application; the FBP=112 composite + FBP=0 scene split is state-provided).
Upload dests match the top sampled textures (capture TBP0 histogram peaks
10756/10820/11017 = the 3 base dests).

### 2f. Capture-side per-window table (m_dump-gated G12 logs; 785/777/0)

8 consecutive windows (idx 1609–1616), closers field 1,0,1,0… =
dump phases ✓, DISPFB0_FBP=112 throughout, Merge gaps [1×7] ✓:

| window (≈dump-vsync) | draws / rasters | n-range | composition |
| --- | --- | --- | --- |
| #1609 (≈#0) | 98 / 97 | 27713–27810 | 112×1 (leads, n=27713) + FBP=0: 96 tri + U-sprite; unrastered n=27714 (2nd); TME0×1 |
| #1610 (≈#1) | 99 / 98 | 27812–27910 | lagged TME0-tri (leads, rasterizes) + 112 + U + 96 tri; unrastered n=27814; TME0×2 |
| #1611–#1616 (≈#2–#7) | 98 / 97 each | contiguous | 112 + U + 96 tri; unrastered = 2nd draw; TME0 as tabled (2,1,1,1,1,1) |

Totals: FBP 0×777 + 112×8; PRIM tri×769 + sprite×16; TME1×775/TME0×10.
Census cross-check: 96+2 programs/vsync (+1 TME0 in #0/#1 = 786) vs 785
draws — the dump's FIRST prim (a TME0 tristrip) is absorbed without its own
draw; the SECOND TME0 prim (trailing #0) flushes late as window#1610's lead
(the G12 lagged-S mechanics, layout-driven, live). Full VSYNC cadence also
logged (2,779 lines: 1,390 field-0 + 1,389 field-1; idle 663 — the
VSYNC-vs-GSvsync count gap is tabled open, §8.6, content-neutral).

## 3. Dual replay with corrected labels + S-lag-aware windowing

### 3a. paraLLEl replay (existing 51,879,240 B binary, `--iterations 2`)

Exit 0, 18 `Running frame` lines (9 cold + 9 warmed), only the 2 known
benign ERRORs, 8.579 ms/VBlank (host wall), heap 454/469 → 459/473 MiB.
8 PPMs `g13-dump.gs.g10-vsync{k}.ppm` (512×448, phases 1,0,1,0… = dump
phases ✓) + FIRST/LAST. Cold == warmed except one-time img-pool growth
(#0: 1,966,080 B texture-pool alloc; #1: 28,672; #3/#5: 327,680 on the
streaming shapes; warmed all 0).

Per-vsync flush stats (warmed pass; `copies` = IMAGE-tag count EXACTLY,
`copy_threads`×4 B = IMAGE bytes EXACTLY — every upload executed):

| vsync | prims | passes | copies / copy_threads | scratch |
| --- | --- | --- | --- | --- |
| #0, #1 | 130 | 2 | 61 / 147,456 (= 589,824 B ✓) | 919,024 |
| #2, #4, #6 | 129 | 2 | 61 / 147,456 ✓ | 916,936 |
| #3, #5, #7 | 129 | 2 | 96 / 229,376 (= 917,504 B ✓) | 1,244,616 |

Record structure (both passes identical — the H4-at-scale comparison):
every iterate records the FBP=112 composite FIRST (17 prims, samples
TBP0=0), THEN the FBP=0 scene flush (112–113 prims, 96 texture binds) —
same content order as PCSX2's per-window execution (112-draw leads).
`passes` = 2 = one pass per record; stats prims = 17 + 112/113 EXACTLY.
Window#0's 96-texture order is IDENTICAL to PCSX2's 96 textured-draw TBP0
order (verified tuple-for-tuple). Counter note (open, §8.2):
`num_primitives` counts post-decomposition TRIANGLES
(`gs_interface.cpp:137-139`, `prim[]` + 3 verts each) — 129/130 tris vs
98/99 GS draws (constant Δ=31/vsync); order + pixels agree regardless.

### 3b. PCSX2 gsrunner replay (bit-identical `8e446ff1…` binary, G10 §3c shape)

`-renderer vulkan -dumpdir g13-frames -logfile emulog-g13.txt -loop 1
-noshadercache -surfaceless -ini g10-uncorrected.ini -- '<abs dump>'` →
exit 0. HWSTAT for 8 (8) frames: 791 draw calls (= 785 GS + 6 present/OSD —
the same 6 as G12 ✓), 37 render passes, 0 barriers, 14 copies, 320 uploads
(texture-cache→device), 6 readbacks. Trace: 785 DRAW / 777 RASTER / 8 VSYNC
/ 8 DUMP_VSYNC / 0 SKIP. 7 PNGs `_frame00001…7.png`, all 512×448
(uncorrected = `m_real_size`, G9/G10 prediction holds on the new dump).

Replay-vs-capture: the 8 replay windows match the 8 capture windows
EXACTLY (98, 99, 98×6 draws; 97, 98, 97×6 rasters; unrastered = 2nd draw
each window; per-window FBP/PRIM/TME histograms identical; Merge gaps
[1×7]; closers field 1,0,1,0…), AND the normalized DRAW sequences
(FBP/FBW/FPSM/TBP0/TBW/TPSM/TME/PRIM × 785) and RASTER target sequences
(× 777) are tuple-for-tuple IDENTICAL live-vs-replay. Replay reproduces
live execution exactly at scene scale. Footnote: exactly one nonzero
`verts` in replay (n=98 `verts=2`, the trailing position — the next prim's
verts already kicked at flush; G12's probe note stands).

S-lag-aware windowing (G12 §2d–§2e applied): replay window#1's lead draw
(n=100, TME0 tristrip, rasterizes into FBP=0) is dump-vsync#0's trailing
prim, flushed late by vsync#1's leading state change — the lagged-S
mechanics at scale, layout-driven (identical live). It executes BEFORE
C(1) on PCSX2 and is consumed inside paraLLEl's iterate#0 (before its
C(1) too) — content-neutral on both sides (C(k)∘scene(k−1) at every
boundary, §3d). File attribution uses the CORRECTED mapping throughout
(file N ↔ post-vsync#N, G12 §3b — cited, not re-derived).

### 3c. Present/file attribution (corrected labels) + scanout tables

| file (md5 short) | bytes | = present | content = post-vsync# | PPM counterpart |
| --- | --- | --- | --- | --- |
| file0 | absent | #1 (≈black — entailed, §3d) | #0 | PPM#0 black ✓ |
| file1 `b7a3e8db` | 46,631 | #2 | #1 | PPM#1 |
| file2 `a7929218` | 48,301 | #3 | #2 | PPM#2 |
| file3 `bb8b1d85` | 48,446 | #4 | #3 | PPM#3 |
| file4 `817e934f` | 213,571 | #5 | #4 | PPM#4 |
| file5 `817e934f` | 213,571 | #6 | #5 | PPM#5 (== file4 byte-identical) |
| file6 `85cf3599` | 175,451 | #7 | #6 | PPM#6 |
| file7 `85cf3599` | 175,451 | #8 | #7 | PPM#7 (== file6 byte-identical) |

No file8 (structural per G12 — no present#9). PPM self-table (sha12):
#0 `9c70d3e9` (ALL BLACK, 0 nonblack) / #1 `667e7cf3` / #2 `37151cf4` /
#3 `8ac47660` / #4 = #5 `1ccda8b6` (sha-identical!) / #6 = #7 `def2f366`
(sha-identical!); #1–#7 all 229,376 nonblack (every pixel). The (4,5)/(6,7)
pairwise identity reproduces on BOTH sides ⟹ content (static scene frame
across the pair — geometry identical every vsync; only uploads differ —
30 fps scene on 60 Hz fields), not a present artifact. Visuals (viewed):
pre-#0 ref = flat burnt-orange; PPM#3 = yellow gradient + orange corner
(4,641 colors); PPM#5 = flare beams on yellow (44,036 colors) — the window
captures a flash-transition effect; workload scene-scale (98 draws, 96
binds, 0.6–0.9 MB uploads/vsync, depth-tested) though pixels are flash,
not gameplay (tabled honestly, §8.1).

### 3d. Same-boundary diffs (PPM#i vs PNG#i, 512×448 exact, RGB)

| k | exact_frac | \|d\|≤2 | \|d\|≤32 | PSNR R/G/B (dB) |
| --- | --- | --- | --- | --- |
| 1 | 0.1004 | 1.0000 | 1.0000 | 56.6 / 54.8 / 49.0 |
| 2 | 0.1125 | 1.0000 | 1.0000 | 55.8 / 55.4 / 48.9 |
| 3 | 0.8820 | 1.0000 | 1.0000 | 61.5 / 60.2 / 61.1 |
| 4 | 0.6485 | 1.0000 | 1.0000 | 56.4 / 55.6 / 56.1 |
| 5 | 0.6485 | 1.0000 | 1.0000 | 56.4 / 55.6 / 56.1 |
| 6 | 0.7349 | 1.0000 | 1.0000 | 57.9 / 56.8 / 57.3 |
| 7 | 0.7349 | 1.0000 | 1.0000 | 57.9 / 56.8 / 57.3 |

Every pixel at every pair agrees to ±2 LSB (a batching divergence — wrong
texture, missing draw, stale target — would show large localized deltas,
not universal ±1–2; none exist). k=0: PPM#0 black; PCSX2's present#1
unobserved (file0 absent, same standing gap as G12) but ENTAILED black:
initial VRAM block-0 is the cleared pattern (0/8192 non-cleared words in
units 0–15, observed via the G12 VRAM recipe — bytes 0–32768 pure
`00 00 00 80`), window#0's only 112-writer C(0) samples it, DISPFB0=112
⟹ post-#0 black on PCSX2 too. (Live block-0 is cleared at dump-open
because the scene onset (~1607–1608, exactly the trigger) is the first
FBP=0 rendering — the 1500→1608 transition traffic lived elsewhere.)

Off-by-one pairing is EXCLUDED by the data: consecutive PNGs are pairwise
distinct (#1/#2/#3 all differ), so a ±1 mis-pairing would show large
motion deltas — observed le2 = 1.0000 ⟹ pairs are true same-boundary.

## 4. Divergence-or-agreement table + killed candidates

No divergence found. Every dimension the brief named is attributed:

| dimension | PCSX2-side | paraLLEl-side | agreement receipt |
| --- | --- | --- | --- |
| per-draw order | 785 draws, 112 leads each window, lagged TME0-tri leads #1 | 112-record before scene-record every iterate; 96-tex order == PCSX2 win#0 | capture==replay DRAW seq identical; tex-order identical |
| uploads | 61/96 IMAGE tags/vsync executed (320 HWSTAT uploads) | copies = 61/96, copy_threads×4 B = IMAGE bytes exactly | counts exact both sides |
| presents/files | file N = present#(N+1) = post-vsync#N (G12, cited) | PPM#i = post-vsync#i | 7 pairs + k=0 entailment |
| pixels | 7 PNGs 512×448 | 8 PPMs 512×448 | le2 = 1.0000 at all 7; (4,5)/(6,7) identity both sides |
| k=0 edge | present#1 black (entailed: cleared block-0) | PPM#0 black (observed) | VRAM census + writer audit |

Killed with receipts: batching-granularity content difference (PCSX2 98
draws vs paraLLEl 2 records — same content order, pixels ±2); deferred
rasterization (every C-DRAW immediately rasterizes); transfer-written
surprise (all 590–918 KB/vsync uploads accounted on both sides);
present-content lag (G12 numbering account reuses cleanly — retired line,
not re-litigated); pairwise-identity-as-artifact (reproduces both sides);
off-by-one pairing (excluded by distinct consecutive PNGs + le2 = 1.0).

## 5. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| The calibrated method attributes every draw/present/file on scene-scale content and names any batching divergence with receipts | **SUPPORTED (alternative (b))**: 785/785 draws + 777/777 rasters attributed live-vs-replay (sequence-identical), 8/8 windows, 7/7 corrected pairs within ±2 LSB + k=0 closed via cleared initial VRAM, uploads/draws cross-checked both sides (copies/threads exact), no divergence found — the G12 content order recurs at 33× draws / 300× traffic with receipts. paraLLEl's recording order is FAITHFUL on rich content (fidelity supported twice over, not a bug) |

The ONE next action the numbers justify (adoption input, not an adoption
decision): **close the emulator-side comparison and run the G7 §5 Odin
on-device replay recipe on the RICH dump (one bounded run), scored against
the now-dual-agreed emulator scanouts.** Rationale: two dumps (thin +
rich) are now fully closed with the same calibrated method and the same
answer — further emulator-vs-emulator comparison faces diminishing
returns (the flash-transition workload already covers upload/draw
interleaving, texture-cache churn, and depth; §8.1's residual ping-pong
dimension is queued, not load-bearing). The adoption question has exactly
one unproven leg left: on-device init/replay (OPEN since G7). Queued
behind it (not this action): a gameplay/ping-pong dump IF the adopter
specifically needs render-to-texture evidence (the G13 trigger + fallback
recipe makes it cheap); the Δ=31 triangle-vs-draw counter note (§8.2);
G12's file0/GetCurrent micro-probe (unchanged).

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.

## 6. License / provenance receipts

| artifact | receipt |
| --- | --- |
| PCSX2 source rev | `9056c08349cc29ad02a6d1a3a4133259019195af` (HEAD re-verified) |
| G13 hook (H1–H10) | local experiment change, uncommitted in bytesize `pcsx2-g7` only (`GS.cpp`, `GSState.cpp`, `GSRendererHW.cpp` — files already carry GPL-3.0+ SPDX; no license change, nothing copied anywhere); trigger+counters verified by run markers; gates verified by 785/777 window-only logs + gsrunner-sha-unchanged |
| G7+G8 hunks + G12 hook | untouched in form (G8 fire condition replaced by H3 by design; G12 logs gated by H7–H10 by design); still uncommitted |
| G13 scripts | `g13-hook.py`, `g13-setup.sh`, `g13-run.sh`, `g13-extract.sh`, `g13-census.py`, `g13-trace.py`, `g13-diff.py` authored this brief — mirror carries all seven as text |
| paraLLEl-GS / SSD clone | untouched (no reads of source beyond the §8.2 counter grep, no builds, one replay run of the existing binary) |
| bytesize static reads | `grep`/`sed`/`find` only, in `pcsx2-g7` (snapshot/window/TRX/FLG code); one qt-only build (`-j2`), one capture run, one `-loop 1` replay |
| brew/tools installs | none (PIL reused as-is) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 7. Exact commands

Mac (`/Users/bradrichardson/dev/ps2xGS` unless noted; `COPYFILE_DISABLE=1`
on SSD steps):

```text
python3 /tmp/g13-census.py "<ssd>/ps2x-g13/g13-dump.gs"   # §2e (all 8)
python3 /tmp/g13-trace.py <trace.txt|emulog-g13.txt>       # §2f + §3b windows
python3 /tmp/g13-diff.py "<ssd>/ps2x-g13/"                 # §3d (7 pairs)
scp /tmp/g13-hook.py "bytesize:pcsx2-t4/"                  # staging (×4: hook, setup+run, extract)
mkdir -p "/Volumes/Extreme SSD/ps2x-g13/"
scp "bytesize:pcsx2-t4/g13-dump.gs" "bytesize:pcsx2-t4/g13-ref.png" "bytesize:pcsx2-t4/g13-park.jpg" "bytesize:pcsx2-t4/g13-window-trace.txt" "bytesize:pcsx2-t4/g13-markers.txt" "/Volumes/Extreme SSD/ps2x-g13/"
scp "bytesize:pcsx2-t4/*_frame0000*.png" "bytesize:pcsx2-t4/emulog-g13.txt" "/Volumes/Extreme SSD/ps2x-g13/"  # also pulled 7 STALE G10 pngs (md5-verified, deleted after)
shasum -a 256 <g13 .gs/.png> ; md5 <g13 PNGs>              # retrieval verify
./tools/parallel-gs-replayer "<g13-dump>.gs" --iterations 2  # §3a (existing binary, MoltenVK env)
du -sk <ssd dirs> ; df -h / "/Volumes/Extreme SSD"         # allocated + deltas
shasum -a 256 <g8 .gs> ; md5 <g12 PNGs>                    # pristine re-verify
```

bytesize (each via ONE `ssh bytesize "wsl …"`; everything after the single
`wsl` runs in Linux, so chain LINUX commands with `;` — a second `wsl` fails;
NO `|`/`>` — Windows cmd eats them; single quotes inside the outer
double quotes for spaced paths):

```text
wsl git -C /home/brad/pcsx2-g7/pcsx2 rev-parse HEAD            # 9056c08349… (§2a)
wsl git -C /home/brad/pcsx2-g7/pcsx2 status --short            # G7+G8+G12 (+G13 after hook)
wsl git -C /home/brad/pcsx2-g7/pcsx2 diff [--stat] [paths]     # H1–H10 review (§2b)
wsl grep -n -e GSQueueSnapshot …/GS/GS.cpp ; sed -n '525,575p' …  # queue path (§2c)
wsl sed -n '700,900p' …/GSRenderer.cpp                        # snapshot/dump-window logic (§2c)
wsl grep -n -e VSync …/GSDump.cpp ; sed -n '85,140p' …         # even-frame close rule (§2c)
wsl grep -n -e s_n …/GSState.h ; sed -n '290,325p' …           # (s_n unreadable — own counter instead)
wsl grep -n -e FlushPrim …/GSState.cpp ; sed -n '2515,2545p' …  # H6 anchor (§2b)
wsl grep -n -e G8_ …/GS.cpp ; sed -n '448,485p' …              # H3 anchor (§2b)
wsl grep -n -e G12_SKIP -e G12: …/GSRendererHW.cpp             # H7–H10 anchors (§2b)
wsl grep -n -e TRXDIR …/GSRegs.h ; sed -n '885,900p' …         # XDIR-only layout (§2c)
wsl grep -rn -e GIF_FLG_PACKED …/GS ; grep -n -e GIF_FLG …/GSRegs.h  # FLG enum (§2c)
wsl grep -n -e A_D_REG …/GSState.cpp                          # REGLIST-direct table (§2c)
wsl date -u; cat /proc/uptime; cat /proc/loadavg; df -h …     # pre-checks (§0)
wsl dmesg                                                     # pre+post flap check (§0; local redirect)
wsl cp /mnt/c/Users/bradr/pcsx2-t4/g13-hook.py /home/brad/pcsx2-g7/g13-hook.py ; python3 /home/brad/pcsx2-g7/g13-hook.py  # H1–H10 ok
wsl sha256sum …/build/bin/pcsx2-qt …/build/bin/pcsx2-gsrunner  # pre+post shas (§2b)
wsl cmake --build /home/brad/pcsx2-g7/pcsx2/build --target pcsx2-qt -j2   # exit 0, 4 steps + 1 benign warning
wsl cp /mnt/c/…/g13-setup.sh /mnt/c/…/g13-run.sh /home/brad/pcsx2-g7/     # staging
wsl bash /home/brad/pcsx2-g7/g13-setup.sh                     # dat-g13 (§2d)
wsl bash /home/brad/pcsx2-g7/g13-run.sh                       # THE capture (2nd attempt; 1st hit T32 occupant) (§2d)
wsl bash /home/brad/pcsx2-g7/g13-extract.sh                   # trace/markers/shas (§2d)
wsl cp '<snaps>/<dump>.gs' /mnt/c/Users/bradr/pcsx2-t4/g13-dump.gs ; … g13-ref.png ; … park/trace/markers  # retrieval staging (spaced names quoted per-file)
wsl rm /mnt/c/…/g13-dump.gs /mnt/c/…/g13-ref.png               # bulky staging cleanup (post-retrieval)
wsl mkdir -p …/g13-frames
wsl timeout 300 …/build/bin/pcsx2-gsrunner -renderer vulkan -dumpdir …/g13-frames -logfile …/emulog-g13.txt -loop 1 -noshadercache -surfaceless -ini …/g10-uncorrected.ini -- '<abs dump path>'  # exit 0 (§3b)
wsl grep -c -e G12_DRAW -e G12_RASTER -e G12_VSYNC -e G12_SKIP -e G12_DUMP_VSYNC …/emulog-g13.txt  # 785/777/8/0/8 (separate calls)
wsl grep -A8 STATISTICS …/emulog-g13.txt                       # HWSTAT (§3b)
wsl md5sum …/g13-frames/*.png                                  # 7 PNGs (§3c)
wsl cp …/g13-frames/*.png …/emulog-g13.txt /mnt/c/Users/bradr/pcsx2-t4/  # retrieval staging
wsl rm /mnt/c/…/emulog-g13.txt /mnt/c/…/SSX*20260921005452_frame*.png    # staging cleanup (my stamp only)
wsl du -s …/build …/dat-g13 …/g13-frames …/dat-g8              # §0
```

Local experiment diffs (uncommitted): bytesize `pcsx2/GS/GS.cpp` +
`pcsx2/GS/GSState.cpp` + `pcsx2/R5900OpcodeImpl.cpp` (G7+G8, H3 replaces the
G8 fire) + `pcsx2/GS/Renderers/HW/GSRendererHW.cpp` (G12 + G13 gates) +
`pcsx2/GSDumpReplayer.cpp` (G12) + `g13-hook.py` + `g13-setup.sh` +
`g13-run.sh` + `g13-extract.sh` + `g13-park.jpg/xwd` + `g13-window-trace.txt`
+ `g13-markers.txt` + `dat-g13/` + `g13-frames/` + `emulog-g13.txt` +
`build/bin/pcsx2-qt` (130,961,672 B) + `build/bin/pcsx2-gsrunner`
(90,455,240 B, `8e446ff1…` — untouched by the G13 build).

## 8. Gaps (what this brief could not do)

1. Window content is a flash-transition workload (flat-orange → yellow →
   flare beams), not gameplay pixels: workload scene-scale (98 draws, 96
   binds, 0.6–0.9 MB uploads/vsync, depth-tested tristrips) but single
   scene target (FBP=0) + composite (112) — the brief's multi-target
   ping-pong hope is NOT met. The upload/draw-interleave + texture-churn +
   depth dimensions ARE covered (the exact hazard shape behind the G11/G12
   recording-order question, at 100× scale). A gameplay/ping-pong dump is
   queued behind the §5 action, with the G13 trigger+fallback recipe making
   it cheap IF the adopter wants it.
2. Δ=31/vsync counter-definition gap: paraLLEl `num_primitives` counts
   post-decomposition TRIANGLES (129/130) vs GS draws (98/99). Order +
   pixels agree, so content-neutral; the strip→triangle mapping (and the
   112-record's 17 tris for 1 sprite draw) is untracked — a short
   packet→record walk would close it (not this brief).
3. File0's absence stays mechanism-entailed (G12 §7.1, unchanged): present#1
   black follows from the writer audit + cleared initial block-0, but no
   PNG#0 exists to diff. Direct-confirmation recipe unchanged (else-branch
   log line, another bounded patch).
4. U-draws' exact skip site still unidentified (G12 §7.3, unchanged — 8
   unrastered here, same standing micro-point, content-neutral).
5. No isolated GPU time (host wall only — unchanged from G8–G12).
6. Capture-side VSYNC-vs-GSvsync count gap (2,779 HW-VSYNCs vs ~2,100
   GSvsync indices; 663 idle=1) — noted, content-neutral (dump windows +
   census are authoritative), mechanism open.
7. Odin on-device init/replay: still OPEN — now the §5 next action (no
   change yet, no claim upgrade).
8. `upstream/` and ps2xGS harness code untouched; staged-but-prior-lane
   `C:\pcsx2-t4\*20260920212606_frame*.png` files left in place (not mine;
   the 7 that hitched a ride on my wildcard retrieval were md5-verified as
   G10's set and deleted from `ps2x-g13/` only).

## 9. Receipt paths

- bytesize source: `/home/brad/pcsx2-g7/pcsx2/` @ `9056c08349…` (+G7/G8
  hunks with H3 fire-replace, +G12 hook with H7–H10 gates — all uncommitted).
- bytesize build: `/home/brad/pcsx2-g7/pcsx2/build/`,
  `bin/pcsx2-qt` (130,961,672 B, `1f664fdd…`),
  `bin/pcsx2-gsrunner` (90,455,240 B, `8e446ff1…`).
- bytesize capture: `/home/brad/pcsx2-g7/dat-g13/` (dump + 225 MB emulog) +
  `/home/brad/pcsx2-g7/g13-{hook,setup,run,extract}.sh/.py` +
  `g13-park.jpg/xwd` + `g13-window-trace.txt` + `g13-markers.txt` +
  `logs/boot-g13.log/stdout`.
- bytesize replay: `/home/brad/pcsx2-g7/g13-frames/` (7 PNGs) +
  `/home/brad/pcsx2-g7/emulog-g13.txt` (139,139 B).
- SSD: `/Volumes/Extreme SSD/ps2x-g13/` (.gs + ref + park + traces +
  emulog + 7 PNGs + 10 PPMs); `ps2x-g8/…-g12/` pristine (sizes + shas
  re-verified).
- Tools: `/tmp/g13-hook.py`, `/tmp/g13-setup.sh`, `/tmp/g13-run.sh`,
  `/tmp/g13-extract.sh`, `/tmp/g13-census.py`, `/tmp/g13-trace.py`,
  `/tmp/g13-diff.py` (+ `/tmp/g13-{replay,census,diff}.log`,
  `/tmp/g13-dmesg-{pre,post}.txt` — mirrored scripts only; logs
  session-only).
- Commits: ps2xGS `[G13]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G13/` `[G13]` + same trailer (NOT pushed).

