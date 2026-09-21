# G10 report — same-boundary dual replay (PCSX2 + paraLLEl, per-pass reset)

Brief: G10 (this turn) — executes the frontier redirect (NOT G9's §4, which
is SUPERSEDED): replay the EXISTING G8 dump through PCSX2 + paraLLEl at the
SAME packet/vsync boundary and native geometry, with a bounded 8-scanout
series and PER-PASS stats reset. Tables + hypothesis + next-action
recommendation, no verdicts beyond the hypothesis. Time box 6 h (used ~2.5 h).
Read first per the brief: `docs/reports/G9.md` (all — aspect explanation
ACCEPTED, no 597 hunt) + `docs/reports/G8.md` §3 (alignment table + flush
series, WITH the erratum: stats accumulate across passes unless reset —
per-pass reset REQUIRED).

Machine: same as G8/G9 (Apple M4, macOS 27.0 — no new installs this brief) +
bytesize WSL (Ubuntu 24.04, clang 18.1.3, cmake 3.28.3, Ninja, llvmpipe).
Pin: paraLLEl-GS `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` /
PCSX2 `9056c08349cc29ad02a6d1a3a4133259019195af` (both re-verified, §2a).
Candidate treated as an independent implementation, not an oracle.

Headline result: the same-boundary design works end to end. The pinned
reference CAN replay the dump (`pcsx2-gsrunner`, every doc facility verified
in the pinned tree before use), so no combined recapture run was needed.
Seven exact-geometry pairs exist (512×448 both sides — the
`ScreenshotSize = 2` uncorrected prediction from G9 §4 CONFIRMED empirically
on the replay path), with per-pass raster counts (cold 272 / warmed 272 —
G8's stacked 544 reconciles EXACTLY, and first=306 = 272+34 confirms the
frontier's decomposition). At boundary k=0 the pair is black-vs-icon
(PSNR 32 dB): PCSX2 presents the loading icon from post-vsync#0 state while
paraLLEl presents black — the first-black lag is paraLLEl-side scanout
behavior, not dump content. At k=1..6 both sides show the icon (PSNR
44.6–47.5 dB, exact-frac ≥ 0.9974).

Provenance note (adopted staging, verified — not silently claimed): at G10
start the SSD clone already carried an unbuilt G10 hook edit (mtime 22:48,
`/tmp/g10-hook.py` 6-hunk recipe + `g10-build/run/setup.sh` staged 22:47 by a
prior attempt that never built, ran, or committed). This brief ADOPTED the
hook (reviewed line-by-line, built `-j2` exit 0, behavior-verified: FIRST/LAST
shas reproduce G8 exactly) and SUPERSEDED the staged sw-run plan after a
verified device failure (sw needs a GL context; creation fails headless AND
under Xvfb — vulkan-on-llvmpipe substituted, geometry unaffected). The
unexecuted staged run scripts are NOT mirrored as recipe (this report's
`g10-gsrunner.sh` is what actually ran); `g10-hook.py` IS mirrored as the
adopted hook's exact recipe.

## 0. Byte caps (declared) vs actuals (apparent + allocated)

| class | cap | actual apparent | actual allocated / delta |
| --- | --- | --- | --- |
| bytesize new clones | 0 | 0 | 0 |
| bytesize build growth (gsrunner reconfigure+build) | 2 GB | +89,020 KiB (577,568 → 666,588); binary 90,452,152 B, 119 steps | build total 666,588 KiB |
| bytesize run outputs (`g10-frames` + `g10-frames-loop1` + ini) | 1 GB | 7+7 PNGs (~48+48 KB) + 2 emulogs (9,176 B stays on bytesize; counts only retrieved) + 32 B ini | 72 + 72 KiB (`du -s`) |
| bytesize staging (`C:\pcsx2-t4\*_frame*.png` retrieval copies) | counts only | 7 PNGs ~48 KB | — (network) |
| bytesize dat-g8 | 0 growth | 0 | 378,536 KiB unchanged (== G8) |
| Task retrieval (7 PNGs) | 1 GB | 48,550 B to `/Volumes/Extreme SSD/ps2x-g10/` | — (network) |
| SSD G10 dir (new: .gs copy + 10 PPMs + 7 PNGs) | 100 MB | ~12.6 MB (5,517,531 + 10×688,143 + 48,550) | 43,008 KiB (`du -sk`) |
| SSD G8/G9 dirs | 0 growth (pristine) | 0 — G8 `.gs`+PPM shas re-verified identical at end | 16,384 / 28,672 KiB unchanged |
| SSD clone/build | 500 MB (rebuild `-j2` in place — tree moved by adopted hook, §3a) | replayer binary +11,872 B (51,867,352 → 51,879,224) | build dir 3,808,256 KiB unchanged (== G8/G9); `df` SSD 453→454 Gi avail (+1 Gi other-lane/rounding, not this brief) |
| internal volume (`/`) | 0 (no installs) | 0 installs; `/tmp/g10-*` ~29 KB apparent / 64 KiB allocated (6 scripts + ini + 3 logs + 2 view PNGs + 3 staged predecessor scripts) | `df` 14 Gi avail unchanged start→end (G9-end 15 Gi → G10-start 14 Gi is other-lane activity) |
| committed to git | text only (report + scripts) | this file (ps2xGS) + REPORT.md + 6 scripts (ssx3 mirror); no captures, no binaries, no build dirs | — |

No code copied into any GPL tree. No P-lane contention: no recomp boots/builds,
no P-lane lease, no `adb`; T29's concurrent bytesize run finished cleanly
(`T29_DONE`) in its own tree while this brief built/ran in `pcsx2-g7` only
(`-j2`, load ≤ 2.45 during build, 0.25 at end). WSL restarted once at 23:02
(not by this brief — no reboot issued); all bytesize artifacts re-verified
after (frame md5 `e0464128…` identical).

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | Same-boundary dual replay yields an exact-geometry comparable pair with per-pass raster counts |
| observable signal | boundary definition, per-side scanouts, per-pass flush series, FIRST-vs-REF diff at exact geometry if paired |
| alternatives | (a) reference cannot replay the dump → capability table + the ONE combined run, then stop; (b) no exact pair even same-boundary → table + recipe, stop; (c) pair found → diff + verdict + ONE next action |
| stop condition | existing dump + at most ONE combined run — no tuning loop, no per-vsync series here (still queued after) |
| outcome → next action | numbers name the next single experiment (§4) |

Outcome: alternative (c) — no combined run needed (reference replay works).

## 2. Task 1 — reference capability + boundary definition (no runs)

### 2a. Reuse verification (pre-work)

| item | observed |
| --- | --- |
| `/Volumes/Extreme SSD/parallel-gs-g7` HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (== G8/G9 pin) |
| tree status | ` m Granite`, ` M tools/gs_dump_replayer.cpp`, `?? tools/._gs_dump_replayer.cpp` — G8's hook + the adopted G10 hook (§3a), uncommitted, unbuilt (binary still 51,867,352 B == G8/G9) |
| G8 dump `.gs` (SSD) | sha `64f6cddf…` (== G8); G10 working copy sha-matches |
| G8 dump `.gs` (bytesize original `dat-g8/PCSX2/snaps/`) | sha `64f6cddf…` — byte-identical to SSD copy; both sides replay the same bytes |
| PCSX2 source rev (bytesize) | `9056c08349cc29ad02a6d1a3a4133259019195af` (== G8 producer rev); status ` M GS.cpp, GSState.cpp, R5900OpcodeImpl.cpp` (G7+G8 hunks, dormant under dump replay — no EE, ExecPS2 stays 0) |
| G8 PPM pristine baseline | first `99418f1b…`, last `0a799a44…` (re-verified identical after all runs) |
| existing build cache | `ENABLE_GSRUNNER:BOOL=OFF`, no gsrunner binary anywhere (`find` over `/home/brad`) |

### 2b. Reference capability table (doc pointer → pinned-tree proof)

Source: official GS Dump Runner docs (https://pcsx2.net/docs/advanced/gsdumprunner/)
— every facility below VERIFIED in rev `9056c08349` before use.

| doc facility | pinned-tree proof | G10 use |
| --- | --- | --- |
| `pcsx2-gsrunner` target, `ENABLE_GSRUNNER=ON` | `pcsx2/CMakeLists.txt:66-70` (`add_subdirectory(pcsx2-gsrunner)`), `pcsx2-gsrunner/CMakeLists.txt` (21 lines: `Main.cpp` + link `PCSX2`) | reconfigure + `cmake --build --target pcsx2-gsrunner -j2` (119 steps, exit 0, §3c) |
| `test_run_dumps.py`: `-runner -gsdir -dumpdir -renderer -upscale -renderhacks -parallel` | read in full (120 lines): `-renderer/-upscale/-renderhacks/-dumpdir/-logfile/-loop 2/-noshadercache/-surfaceless -- <dump>` | recipe shape followed (single-dump equivalent; `-parallel` n/a) |
| `test_check_dumps.py`: md5 frame compare + HWSTAT stats + HTML | read in full (197 lines) | NOT used for cross-implementation compare (md5-exact is the wrong tool across renderers); G8 `g8-diff.py` shape used instead (§3d) |
| `-renderer … sw` (SW option) | `Main.cpp` `-renderer` parse: Auto/dx11/dx12 (Win32)/gl/vulkan/metal + `sw` UNCONDITIONAL (no ifdef) | ATTEMPTED and FAILED at runtime (§3c): sw GSDevice needs a GL context; `GL: Failed to create any context` headless AND with `DISPLAY=:99` (Xvfb) → exit 1 |
| `-renderer vulkan` substitution | same parse block (`#ifdef ENABLE_VULKAN`); lavapipe present (`llvmpipe (LLVM 20.1.2)`) | USED: device creates surfaceless, exit 0; geometry renderer-independent (uncorrected = `m_real_size`) |
| `-dumpdir`: `{title}_frameN.png` per presented frame | `Main.cpp:236-249` (`BeginPresentFrame` → `GSQueueSnapshot({prefix}_frame{:05}.png)`, loop 0 only) + `:779-789` (prefix = `dumpdir/title`) | 7 PNGs retrieved (§3c) |
| `-loop N` (script uses 2) | `Main.cpp` `-loop` → `DumpReplayLoopCount`; `GSDumpReplayer.cpp:290-305` (extra-loops countdown) | `-loop 2` primary + `-loop 1` control: outputs byte-identical (md5, §3c) |
| `-ini` (EmuCore/GS injection) | `Main.cpp` `-ini` → `INISettingsInterface`, copies `EmuCore/GS` keys | `ScreenshotSize = 2` (key verified `Pcsx2Config.cpp:945`, section `[EmuCore/GS]` verified in dat-g8 ini `:118`) |
| uncorrected = internal, no aspect | `GSRenderer.cpp:775-776`: `internal=(size>=Internal)`, `aspect_correct=(size!=Uncorrected)` | predicted 512×448; OBSERVED 512×448 on all 7 PNGs (§3c) — G9 §4 prediction confirmed |
| frame++ per VSync packet | `GSDumpReplayer.cpp:391-393` (`case VSync: s_dump_frame_number++` before present) | file N ↔ post-vsync#(N-1) (§2c); last present never snapshots (no `_frame00000`/`_frame00008`) |
| per-draw `-dump`/`-dumprange[f]` | `Main.cpp:420-427` help + parse | NOT used (generates lots of data; frame snapshots suffice) |
| `SetFrameRange(bool,u32,u32)` API | `GSDumpReplayer.h:20` | NOT wired to any CLI flag at this rev → full replay from packet 0 both sides; boundary = index, not seek |

Capability verdict: reference CAN replay the dump (alternative (a) avoided —
no combined run). Two substitutions forced by verification, both tabled with
mechanism: (i) vulkan-for-sw (device creation, §3c); (ii) 7-of-8 frames
(final-present snapshot race, §3c).

### 2c. THE boundary + native geometry + expected pair

`g10-boundary.py` (packet-ordinal census; same framing as `g8-census.py`,
EOF-synced): 104 packets total. EVERY dump-vsync#k (k=0..7) has the identical
layout — 11 Transfers (all path 3, sizes
1056,592,48,144,80,48,64,48,80,144,144 = 2,448 B), 1 PrivRegs, 1 VSync:

| dump-vsync#k | VSync packet ordinal P_k | phase | transfer ordinals |
| --- | --- | --- | --- |
| 0 | 12 | 1 | 0–10 |
| 1 | 25 | 0 | 13–23 |
| 2 | 38 | 1 | 26–36 |
| 3 | 51 | 0 | 39–49 |
| 4 | 64 | 1 | 52–62 |
| 5 | 77 | 0 | 65–75 |
| 6 | 90 | 1 | 78–88 |
| 7 | 103 | 0 | 91–101 |

(PrivRegs at ordinals 11,24,37,50,63,76,89,102 — one per vsync, between that
vsync's transfers and its VSync packet.)

THE boundary k (shared packet index + vsync index): state = dump initial
regs+GS-state + packets 0..P_k inclusive, where P_k = 13k+12. Both sides
replay from packet 0; no seeking (no CLI for it, §2b).

| side | output at boundary k | native geometry | mechanism |
| --- | --- | --- | --- |
| paraLLEl | `g10-vsync{k}.ppm` (iterate-true #k scanout) | 512×448 progressive (observed, all 8) | `iterate_until_vsync` + `consume_vsync_result` |
| PCSX2 | `_frame{k+1:05d}.png` (post-vsync#k present) | 512×448 uncorrected (observed, k=0..6) | VSync packet → present → `GSQueueSnapshot` with `ScreenshotSize=2` |

Expected pair: FIRST (`g10-vsync0.ppm`, post-vsync#0, phase 1) vs
`_frame00001.png` (post-vsync#0). This fixes G8/G9's misalignment
(`ScreenshotSize=2` alone still compares REF pre-vsync#0 vs candidate
vsync#0): BOTH images here are post-vsync#0.

## 3. Task 2 — dual replay + 8-scanout series (bounded)

### 3a. paraLLEl hook (adopted) + rebuild

The adopted hook (`g10-hook.py`, 6 exact-match hunks on G8's hook): (H1)
`<vector>` include; (H3) `g10_series` stash replaces first-only stash;
(H4) per-pass `consume_flush_stats()` reset at each pass start
(`restart()` rewinds the file, not the stats — the erratum fix);
(H5) per-vsync consume+log on EVERY pass (pass-tagged `G10: pass %u vsync
#%u …`), series stash on last pass only; (H6) bounded last-pass series save
(one PPM per iterate-true) + FIRST/LAST front/back saves for continuity.
Loop-body execution count unchanged; all PPM I/O after `end_ns` (timing
comparable to G8). Non-last passes never call `consume_vsync_result` (same
as G8 — no new behavior risk).

Rebuild `-j2`: exit 0; only the pre-existing `-Wshadow FileDeleter`
warning + `ld` duplicate-libraries notice (same as G8). Binary
51,867,352 → 51,879,224 B (+11,872).

Replay: `./tools/parallel-gs-replayer <g10-dump>.gs --iterations 2`
(defaults = native/X1), exit 0, 18 `Running frame` lines (9 cold + 9
warmed, same as G8), only the 2 known benign ERROR lines + 3 sync shader
compiles (same as G8). Time 2.112 ms/VBlank (G8: 0.758 — host-wall
variance on shared MoltenVK; same caveat: host wall, not GPU time).
Memory Heap 0 DEVICE 453/459 MiB cold → 457/464 warmed (G8: 453/459 →
454/460).

### 3b. Per-pass flush series (cold vs warmed — the erratum fix)

| pass | vsync#0 | vsync#1..#7 (each) | pass total |
| --- | --- | --- | --- |
| cold (0) | 34 prims / 2 passes / 1 pal (scratch 128,752 B, img 1,052,672 B — one-time pool growth) | 34 / 2 / 1 (scratch 128,752 B, img 0) | 272 / 16 / 8 |
| warmed (1) | 34 / 2 / 1 (img 0) | 34 / 2 / 1 (img 0) | 272 / 16 / 8 |

(copies/copy_threads/copy_barriers = 0 throughout, both passes.)

G8 reconciliation (EXACT): G8's stacked totals 544 prims / 32 passes /
16 pal = 272 cold + 272 warmed; G8's last-pass first take 306 = 272
(entire cold pass accumulated) + 34 (warm vsync#0); G8's #1–#7 = 34 each
(warm). The frontier's decomposition (first = 272 cold + 34 warm) is
CONFIRMED empirically. Viability held without recapture, as directed.

### 3c. PCSX2 replay (gsrunner, vulkan, uncorrected)

Build: reconfigure `cmake -S … -B … -DENABLE_GSRUNNER=ON` (cache reuse:
Ninja/Devel/clang/`/home/brad/deps`; only known Qt private-module
warnings) + `cmake --build --target pcsx2-gsrunner -j2`: exit 0, 119
steps (libzip + `Main.cpp` + link), binary 90,452,152 B. (The staged
predecessor plan `ninja -C build pcsx2-gsrunner` without reconfigure was
not executed; the reconfigure path is what ran. `CMakeCache.txt`
`ENABLE_GSRUNNER` OFF→ON is a build-dir-only change, uncommitted.)

Renderer substitution (verified failure, not preference): `-renderer sw`
fails with `GL: Failed to create any context` + `Failed to create GS
device` (exit 1) BOTH headless AND with `DISPLAY=:99` (T29's Xvfb) — the
sw GSDevice needs a GL context this environment cannot provide.
`-renderer vulkan` creates its device surfaceless via lavapipe
(`llvmpipe (LLVM 20.1.2, 256 bits)`, Vulkan 1.4.318, Mesa 25.2.8) and
runs to exit 0. G8's capture used `Renderer = -1` (Auto); Auto here
would resolve to the same lavapipe Vulkan (GL fails), so the explicit
flag documents rather than changes behavior. GameDB applied its 4 pinned
SSX3 HW fixes (blending/textureInsideRT/halfPixelOffset/nativeScaling) —
part of pinned-reference behavior, tabled not tuned.

Primary run (`-renderer vulkan -dumpdir g10-frames -logfile … -loop 2
-noshadercache -surfaceless -ini g10-uncorrected.ini -- <dump>`, 300 s
wall cap, ~1.05 s actual): exit 0, `HW STATISTICS FOR 16 (16) FRAMES`:
53 draw calls / 53 render passes / 0 barriers / 22 copies / 2 uploads /
6 readbacks. Control run (`-loop 1`, separate dir): exit 0, `FOR 8 (8)
FRAMES`: 29 / 29 / 0 / 14 / 2 / 6 — loop-2 totals are NOT 2× loop-1
(second loop draws less; stabilization, tabled without speculation).
Loop-1 PNGs are BYTE-IDENTICAL to loop-2 PNGs (all 7 md5s match) — one
set retrieved.

Frames: 7 PNGs `_frame00001 … _frame00007.png`, ALL 512×448
(ScreenshotSize=2 uncorrected = `m_real_size` — G9 §4's prediction
confirmed on the replay path). No `_frame00000` (counter increments
before the first present, `GSDumpReplayer.cpp:391-393`) and no
`_frame00008` (the final present never snapshots — shutdown wins the
race; `GSJoinSnapshotThreads` IS called at shutdown
(`VMManager.cpp:456`), so the 8th snapshot was never queued, not merely
unflushed; identical 7-file result with `-loop 1`). Mapping: file N ↔
post-dump-vsync#(N-1) — pinned jointly by the increment-before-present
source order AND the observed 1..7 numbering (a stale-counter theory
would yield 0..6). Usable pairs: k=0..6 (7 of 8 boundaries); post-vsync#7
(LAST) has no PCSX2 file — documented gap with mechanism + recipe (§7.1).

| file | bytes | md5 | geometry |
| --- | --- | --- | --- |
| `_frame00001.png` | 6,629 | `e0464128…` | 512×448 |
| `_frame00002.png` | 6,692 | `aa0053f9…` | 512×448 |
| `_frame00003.png` | 6,875 | `db676261…` | 512×448 |
| `_frame00004.png` | 6,937 | `43444061…` | 512×448 |
| `_frame00005.png` | 7,086 | `810bbe55…` | 512×448 |
| `_frame00006.png` | 7,099 | `bc95ce34…` | 512×448 |
| `_frame00007.png` | 7,232 | `dad65e30…` | 512×448 |

### 3d. Series tables + FIRST-vs-REF diffs at exact geometry

paraLLEl 8-scanout series (warmed pass; `g10-series.py` from PPM bytes;
hook log confirms internal=mode=512×448 progressive, phases 1,0,1,0… =
dump phases):

| scanout | distinct | nonblack px | sha (short) |
| --- | --- | --- | --- |
| `g10-vsync0.ppm` (= FIRST) | 1 | 0 (0.0000, ALL BLACK) | `99418f1b…` == G8 first ✓ |
| `g10-vsync1.ppm` | 180 | 402 (0.0018) | `32d23648…` |
| `g10-vsync2.ppm` | 186 | 438 (0.0019) | `91a9d042…` |
| `g10-vsync3.ppm` | 191 | 475 (0.0021) | `4d6cfffd…` |
| `g10-vsync4.ppm` | 198 | 486 (0.0021) | `03326ebb…` |
| `g10-vsync5.ppm` | 210 | 534 (0.0023) | `06b5851f…` |
| `g10-vsync6.ppm` | 211 | 569 (0.0025) | `8caa7f2a…` |
| `g10-vsync7.ppm` (= LAST) | 216 | 607 (0.0026) | `0a799a44…` == G8 last ✓ |

Continuity: the rebuilt binary reproduces G8's FIRST/LAST bytes EXACTLY —
the adopted hook preserves G8 behavior; the replayer is deterministic
across the rebuild. (The icon-growth-by-vsync localization comes free
with the series; causal analysis stays in the queued per-vsync work.)

Same-boundary diffs (`g10-diff.py`, G8 `g8-diff.py` shape —
pixel-diff ONLY on exact geometry; all 7 pairs ARE 512×448 vs 512×448,
so all 7 diffs ran — the first real FIRST-vs-REF comparison in the
G-series):

| k | PPM nonblack | PNG nonblack | exact_frac | PSNR (R=G=B) |
| --- | --- | --- | --- | --- |
| 0 (FIRST vs `_frame00001`) | 0 (black) | 432 (icon) | 0.9981 | 32.0 dB |
| 1 | 402 | 466 | 0.9981 | 44.6 dB |
| 2 | 438 | 505 | 0.9979 | 45.0 dB |
| 3 | 475 | 531 | 0.9978 | 45.5 dB |
| 4 | 486 | 568 | 0.9976 | 45.0 dB |
| 5 | 534 | 607 | 0.9975 | 46.4 dB |
| 6 | 569 | 648 | 0.9974 | 47.5 dB |

(|d|≤32 fractions all ≥ 0.9987; full per-pair histograms in
`/tmp/g10-diff.log` — retained session-only, reproducible via `g10-diff.py`.)

Visual inspection (4 images): k=0 = black vs black+white-snowflake-icon
(bottom-right, same icon/corner as G8's REF); k=6 = icon vs icon, same
corner, visually identical placement. R=G=B PSNR identical throughout
(grayscale icon on black, both sides).

Reading (numbers + method limits only): at the SAME post-vsync#0 state,
PCSX2 presents the icon while paraLLEl presents black — the first-black
lag is paraLLEl-side scanout behavior (its vsync#0 DID execute 34 prims,
§3b), not dump content. At k=1..6 both sides track the same growing icon
with small coverage differences (PNG leads PPM by ~60–80 nonblack px per
step — consistent with a one-field presentation offset between the two
scanout pipelines; cause OPEN, queued).

## 4. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| Same-boundary dual replay yields an exact-geometry comparable pair with per-pass raster counts | **SUPPORTED, with two refinements**: (i) 7 exact-geometry pairs (512×448 both sides), not 8 — post-vsync#7 has no PCSX2 file (final-present race, §3c); the FIRST pair exists and diffs at exact geometry (0.9981 exact, PSNR 32 dB, §3d). (ii) Per-pass counts separate cleanly (cold 272 / warmed 272; G8's 544 and first=306 reconcile EXACTLY, §3b). Numbers + method limits only |
| Reference-side substitution | vulkan-on-llvmpipe for sw (sw device creation impossible in this environment — verified failure, §3c). Geometry unaffected (uncorrected = `m_real_size` either way) |

The ONE next action the numbers justify (adoption input, not an adoption
decision): **run the queued per-vsync scanout series to localize the
first-black cause — the method now supports it.** G10 proved exact pairs
exist at every step and that the lag is paraLLEl-side (PCSX2 shows the
icon from identical post-vsync#0 state): trace what paraLLEl presents vs
renders at iterate-true #0 (one-field presentation offset? `consume_vsync_result`
lag? scanout double-buffer behind the draw?) on the EXISTING dump, using
this brief's 7 paired PCSX2 frames as the oracle. Queued behind it (not
this action): the post-vsync#7 pair (needs a runner-side final-present
snapshot or a longer dump, §7.1), then a rich post-loading dump. Rationale:
the comparison method is unblocked for the first time — spend the next
experiment on the content gap it revealed (k=0 black-vs-icon), not on new
content whose first frame would hit the same question.

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.
Odin on-device init/replay REMAINS open (G7 §5 recipe on file, untouched —
capability queries only, per the frontier instruction).

## 5. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (clone HEAD re-verified) |
| PCSX2 source rev | `9056c08349cc29ad02a6d1a3a4133259019195af` (== G8 producer rev) |
| G10 replayer hook | adopted staging (`/tmp/g10-hook.py`, 6 exact-match hunks on G8's hook), uncommitted in the SSD clone only (file already carries LGPL-3.0+ SPDX; no license change, nothing copied anywhere); verified by build + run + FIRST/LAST sha continuity |
| gsrunner build | pinned tree UNMODIFIED (only `CMakeCache.txt ENABLE_GSRUNNER` OFF→ON in the build dir, uncommitted); G7/G8 trigger hunks in the tree are dormant under dump replay (no EE); no new code written on bytesize |
| G10 scripts | `g10-boundary.py`, `g10-series.py`, `g10-diff.py`, `g10-gsrunner.sh`, `g10-uncorrected.ini` authored this brief; `g10-hook.py` adopted staging — mirror carries all six as text |
| brew/tools installs | none (G8's set + PIL reused as-is) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 6. Exact commands

Mac (`/Users/bradrichardson/dev/ps2xGS` unless noted; `COPYFILE_DISABLE=1` on
SSD steps; `VK_ICD_FILENAMES=…MoltenVK_icd.json`,
`DYLD_LIBRARY_PATH=/opt/homebrew/lib` on replayer runs):

```text
git -C "/Volumes/Extreme SSD/parallel-gs-g7" rev-parse HEAD   # 3a66c19… (hook adopted, unbuilt → rebuild)
python3 /tmp/g10-boundary.py <dump.gs>                         # Task 1 boundary table (packet ordinals)
cmake --build "/Volumes/Extreme SSD/parallel-gs-g7-build" --target parallel-gs-replayer -j2  # exit 0
cp "<g8-dir>/<dump>.gs" "/Volumes/Extreme SSD/ps2x-g10/"      # working copy (sha-verified)
./tools/parallel-gs-replayer "<g10-dump>.gs" --iterations 2    # dual-pass replay, defaults
python3 /tmp/g10-series.py <g10-dir> "*.g10-*.ppm"             # 8-scanout series table
python3 /tmp/g10-diff.py <g10-dir>                             # 7 same-boundary diffs (G8 shape)
scp "bytesize:pcsx2-t4/*_frame*.png" "/Volumes/Extreme SSD/ps2x-g10/"  # retrieval
shasum -a 256 <g8-dir>/*.gs <g8-dir>/*.ppm                    # G8 pristine re-verify (before + after)
du -sk <ssd dirs> ; df -h / "/Volumes/Extreme SSD"            # allocated + deltas
```

bytesize (each via ONE `ssh bytesize "wsl …"`, `;` separators, NO inline
pipes or redirects — Windows cmd eats `|`/`>`; files staged via
`scp … "bytesize:pcsx2-t4/"` + `wsl cp /mnt/c/…`; single quotes inside the
outer double quotes for paths with spaces):

```text
wsl git -C /home/brad/pcsx2-g7/pcsx2 rev-parse HEAD             # 9056c08349…
wsl grep -n -e FrameNumber -e LoopCount … GSDumpReplayer.cpp    # -e chains, never a|b
wsl sed -n "400,470p" …/pcsx2-gsrunner/Main.cpp                 # usage + arg parsing (Task 1 reads)
wsl sed -n "1,120p" …/test_run_dumps.py ; sed -n "1,197p" …/test_check_dumps.py
wsl sha256sum '/home/brad/pcsx2-g7/dat-g8/PCSX2/snaps/*.gs'     # == SSD sha
wsl cmake -S …/pcsx2 -B …/pcsx2/build -DENABLE_GSRUNNER=ON      # cache reuse
wsl cmake --build …/build --target pcsx2-gsrunner -j2           # exit 0, 119 steps
wsl cp /mnt/c/Users/bradr/pcsx2-t4/g10-uncorrected.ini /home/brad/pcsx2-g7/ ; mkdir -p …/g10-frames
wsl timeout 300 …/bin/pcsx2-gsrunner -renderer vulkan -dumpdir …/g10-frames -logfile …/emulog.txt -loop 2 -noshadercache -surfaceless -ini …/g10-uncorrected.ini -- '<dump>.gs'   # exit 0
wsl md5sum …/g10-frames/*.png ; md5sum …/g10-frames-loop1/*.png # loop1 == loop2
wsl cp …/g10-frames/*.png /mnt/c/Users/bradr/pcsx2-t4/          # retrieval staging
wsl du -s …/build …/g10-frames …/g10-frames-loop1 …/dat-g8      # allocated actuals
```

Local experiment diffs (uncommitted): SSD clone `tools/gs_dump_replayer.cpp`
(G8 hook + adopted G10 hook, recipe `g10-hook.py`); bytesize `pcsx2/GS/GS.cpp`
+ `pcsx2/GS/GSState.cpp` + `pcsx2/R5900OpcodeImpl.cpp` (G7+G8 hunks, untouched
by G10) + build-dir `CMakeCache.txt` (gsrunner ON) + `g10-uncorrected.ini` +
`g10-frames{,-loop1}/` + `build/bin/pcsx2-gsrunner`.

## 7. Gaps (what this brief could not do)

1. Post-vsync#7 (LAST) has no PCSX2 file: the final present never snapshots
   at this rev (both `-loop 1` and `-loop 2` yield `_frame00001..7` only;
   shutdown joins snapshot threads, so the 8th was never queued). Recipe:
   a runner-side snapshot of the final present (e.g. snapshot in the
   shutdown path or a trailing dummy vsync), or a longer dump where the
   frame of interest is not last. The 7 existing pairs are unaffected.
2. SW renderer unverified as a replay path: `-renderer sw` cannot create its
   GSDevice in this environment (GL context fails headless and under Xvfb).
   vulkan-on-llvmpipe substituted; geometry is renderer-independent, but a
   sw-vs-vulkan pixel comparison on capable hardware would strengthen the
   reference. NOT a doc gap (the sw flag exists and parses) — an environment
   capability gap.
3. First-black cause: OPEN but LOCALIZED (G8 gap #1 advanced, not closed).
   Rendering runs on vsync#0 (34 prims, §3b) and PCSX2 shows the icon from
   the same state (§3d) — the lag is paraLLEl-side presentation. Needs the
   §4 next action, not speculation.
4. Coverage deltas at k=1..6 (PNG leads PPM ~60–80 nonblack px/step,
   PSNR ~45–47 dB): tabled, unexplained — same queued work as #3 (one-field
   presentation offset is a candidate, not a claim).
5. Loading-icon content is a light draw load (272 prims / 16 passes per
   pass over 8 vsyncs). Scene-scale rasterization stays queued behind the
   comparison unblock (now unblocked).
6. No isolated GPU time (host wall only; replayer CLI still has no timestamp
   path — unchanged from G8). gsrunner `-perf` (+ `SetGPUTimingEnabled`)
   exists but was not used (llvmpipe timings would not transfer).
7. WSL wall-clock skew persists (bytesize log stamps vs Mac); all wall
   evidence is duration/count-based, unaffected.
8. Odin on-device project init/replay: still OPEN (G7 §5 recipe on file; no
   change, no claim upgrade).
9. `upstream/` and ps2xGS harness code untouched; ps2xGS `.gscap` captures are
   a different format from PCSX2 `.gs` dumps (no adapter attempted or implied).
10. Session-only `/tmp/g10-*` (~29 KB apparent / 64 KiB allocated); external-SSD artifacts (G10 dir
    ~12.6 MB apparent / 43,008 KiB allocated; G8/G9 dirs pristine) retained,
    none committed. Staged-but-unexecuted predecessor scripts
    (`/tmp/g10-{build,run,setup}.sh`) retained session-only, NOT mirrored
    (superseded: sw plan fails §3c, ninja-without-reconfigure untested).

## 8. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…` (+G7 shim, +G8 hook, +adopted G10 hook).
- Build: `/Volumes/Extreme SSD/parallel-gs-g7-build/`, `tools/parallel-gs-replayer` (51,879,224 B).
- Dump dirs: `/Volumes/Extreme SSD/ps2x-g8/` (pristine, shas re-verified);
  `/Volumes/Extreme SSD/ps2x-g10/` (.gs copy + 10 PPMs + 7 PNGs).
- Logs: `/tmp/g10-replayer-build.log`, `/tmp/g10-replay.log`,
  `/tmp/g10-diff.log` (session-only).
- Tools: `/tmp/g10-boundary.py`, `/tmp/g10-series.py`, `/tmp/g10-diff.py`,
  `/tmp/g10-gsrunner.sh`, `/tmp/g10-uncorrected.ini`, `/tmp/g10-hook.py`
  (mirrored to ssx3; `/tmp` originals are session-only).
- bytesize: `/home/brad/pcsx2-g7/` (source+build+dat-g8 untouched,
  `g10-uncorrected.ini`, `g10-frames{,-loop1}/`, `build/bin/pcsx2-gsrunner`
  90,452,152 B), staging copies at `C:\Users\bradr\pcsx2-t4\*frame*.png` +
  `g10-uncorrected.ini`.
- Commits: ps2xGS `[G10]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G10/` `[G10]` + same trailer (NOT pushed).
