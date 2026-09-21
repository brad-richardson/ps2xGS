# G11 report — per-vsync scanout series: first-black cause localized (existing dump)

Brief: G11 (this turn) — executes G10's §4: trace what paraLLEl presents
vs renders at iterate-true #0 and localize the first-black cause on the
EXISTING dump, using G10's 7 paired PCSX2 frames as the oracle. Tables +
hypothesis + next-action recommendation, no verdicts beyond the hypothesis.
Time box 6 h (used ~2 h). Read first per the brief: `docs/reports/G10.md`
(all) + `docs/reports/G8.md` §3 as needed (WITH the erratum: per-pass
reset REQUIRED — honored: all takes below are per-pass-reset).

Machine: same as G8/G9/G10 (Apple M4, macOS 27.0 — no new installs) +
bytesize WSL read-only greps only (no builds, no runs, no lease).
Pin: paraLLEl-GS `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (re-verified,
§2a). Candidate treated as an independent implementation, not an oracle.

Headline result: the first-black cause is LOCALIZED to a named stage —
**interface-side recording order inside the vsync flush**. Each dump-vsync
carries two 17-prim batches: an on-screen composite (FRAME FBP=112, the
scanned buffer) that samples the off-screen sprite (TEX0 TBP0=0), and the
sprite batch itself (FRAME FBP=0, sampled from atlas TBP0=3584+palette).
The composite is recorded mid-stream at the FRAME 112→0 hazard flush; the
sprite it samples is recorded LATER, inside `flush()` — so the single
submit executes composite-shading before sprite-shading and the composite
samples the PREVIOUS vsync's sprite (WAR-ordered). Presentation lags
rendering by exactly one vsync, systematically: the cross-k matrix shows
PPM#(k+1) ≈ PNG#(k+1) at 62.8–64.6 dB vs same-boundary 44–47 dB at EVERY
row. Scanout #0 composites the cleared initial sprite buffer (and the
initial scanned buffer is itself RGB-black: 0/229,376 nonblack through the
scanout's own swizzle) — black needs no second cause. Killed with
receipts: DISPFB flip (FBP=112 constant all 8), `consume_vsync_result` lag
(by construction), field selection (force-progressive full-frame),
backbuffer promotion (hack off), GPU cross-submit visibility / missing
barrier (same-queue submit order + moved-from submit handles + coherent
memory + deterministic full-frame signature), upload deferral (flushes
fully each flush + single — not double — lag in the matrix), and
cross-vsync batch deferral (H4 order: the sprite records INSIDE its own
flush, correcting the H1-only reading).

## 0. Byte caps (declared) vs actuals (apparent + allocated)

| class | cap | actual apparent | actual allocated / delta |
| --- | --- | --- | --- |
| bytesize new clones / builds / runs | 0 (read-only greps only) | 0 (`ssh bytesize` greps/seds, 8 calls, no artifacts) | 0 |
| SSD G11 dir (new: .gs copy + 10 PPMs) | 100 MB | ~12.4 MB (5,517,531 + 10×688,143) | 28,672 KiB (`du -sk`) |
| SSD G8/G9/G10 dirs | 0 growth (pristine) | 0 — G10 PPM/PNG shas re-verified identical (8/8 + 7/7) | 16,384 / 28,672 / 43,008 KiB unchanged |
| SSD clone/build (2× `-j2` rebuild in place — tree moved by design, §3c) | 500 MB | replayer binary 51,879,224 → 51,879,240 B (+16; H4 relink same size) | build dir 3,808,256 KiB unchanged (== G8/G9/G10); `df` SSD 452→449 Gi avail (3 Gi other-lane — this brief wrote ~15 MB; 454→452 pre-brief is also other-lane) |
| internal volume (`/`) | 0 (no installs) | 0 installs; `/tmp/g11-*` ~45 KB (7 scripts + 8 logs) | `df` 14 Gi avail unchanged start→end |
| committed to git | text only (report + scripts) | this file (ps2xGS) + REPORT.md + 7 scripts (ssx3 mirror); no captures, no binaries, no build dirs | — |

No code copied into any GPL tree. No P-lane contention: no recomp
boots/builds, no lease, no `adb`, no bytesize builds/runs (greps only).

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | The per-vsync series names the stage where iterate-true #0's rendered content misses presentation |
| observable signal | per-vsync presented bytes vs rendered state at #0..#2 (extended to #0..#7 — cheap) on both sides |
| alternatives | (a) cause localized to a named stage → table + ONE next action; (b) series inconclusive → table + recipe, stop |
| stop condition | existing dump + existing 7-frame oracle only — no recapture, no runner changes beyond bounded read-only instrumentation, no tuning loop |
| outcome → next action | numbers name the next single experiment (§4) |

Outcome: alternative (a) — localized (§3e/§4).

## 2. Task 1 — reuse verification + dump-state decode (no runs)

### 2a. Reuse verification (pre-work)

| item | observed |
| --- | --- |
| SSD clone HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (== G8/G9/G10 pin) |
| tree status (pre-work) | ` m Granite`, ` M tools/gs_dump_replayer.cpp` (G8+G10 hook), `?? tools/._…` — G10-built binary 51,879,224 B present, no rebuild needed for analysis |
| G10 dump `.gs` + G11 working copy | sha `64f6cddf…` both |
| G10 PPMs (8) | shas `99418f1b 32d23648 91a9d042 4d6cfffd 03326ebb 06b5851f 8caa7f2a 0a799a44` (== G10 §3d) |
| G10 PNGs (7) | md5s `e0464128 aa0053f9 db676261 43444061 810bbe55 bc95ce34 dad65e30` (== G10 §3c) |

### 2b. Per-vsync scanout-state decode (`g11-privregs.py`, read-only)

PrivRegs decoded per dump-vsync (paraLLEl `PrivRegisterState` HW offsets;
PMODE bit order settled against PCSX2 `GSRegs.h` `GIFRegPMODE` at the
pinned rev via bytesize read-only `sed` — EN1[0] EN2[1] CRTMD[2:4] MMOD[5]
AMOD[6] SLBG[7] ALP[8:15], identical to paraLLEl's `PMODEBits`):

- ALL 8 dump-vsyncs carry IDENTICAL priv state except CSR FIELD (alternates
  0/1 with phase): PMODE EN1=1 EN2=0 MMOD=1 SLBG=0 ALP=255; SMODE1 CMOD=2
  (NTSC) LC=32 (ANALOG); SMODE2 INT=1 FFMD=0 (interlaced FIELD mode);
  **DISPFB1 FBP=112 FBW=8 PSM=1 DBX=DBY=0 — CONSTANT (single-buffered, no
  flip)**; DISPLAY1 DW=2560 DH=447 MAGH=4 MAGV=0 (→ 512×448 via the
  compute_circuit_rect math: 2561/5=512, 448/1=448); circuit 2 fully unset;
  EXTWRITE=0; BGCOLOR black.
- G9 erratum (minor, no headline impact): G9's `dec_pmode` sub-field decode
  (MMOD=0, ALP=249 for `0xff21`) used wrong bit positions; correct is
  MMOD=1, SLBG=0, ALP=255. G9 used only EN1/EN2 (both right).

Kills at the display-state level: draw/scanout double-buffer flip, circuit
merge differences, EXTWRITE feedback, field-selection (`force_progressive`
+ FIELD mode → `phase_offset=0, stride=1`, full-frame sampling —
`compute_circuit_rect`, gs_renderer.cpp:4188).

### 2c. Cross-k diff matrix (`g11-xdiff.py`, existing pairs only)

Full 8 PPM × 7 PNG PSNR matrix (all 512×448, G10 diff shape). Rows =
PPM#i (iterate-true #i, consumed transfers 0..i); columns = PNG#j
(post-vsync#(j-1), consumed 0..j-1):

| PSNR_R (dB) | PNG#1 | PNG#2 | PNG#3 | PNG#4 | PNG#5 | PNG#6 | PNG#7 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PPM#0 | 31.99 | 31.61 | 31.29 | 30.96 | 30.68 | 30.42 | 30.17 |
| PPM#1 | **64.64** | 44.63 | 39.63 | 36.99 | 35.42 | 34.35 | 33.62 |
| PPM#2 | 44.67 | **64.42** | 44.98 | 39.79 | 37.19 | 35.60 | 34.55 |
| PPM#3 | 39.76 | 45.19 | **63.78** | 45.49 | 40.23 | 37.59 | 35.98 |
| PPM#4 | 37.05 | 39.80 | 45.47 | **62.84** | 45.00 | 40.37 | 37.87 |
| PPM#5 | 35.50 | 37.24 | 40.38 | 45.50 | **63.85** | 46.41 | 41.41 |
| PPM#6 | 34.41 | 35.62 | 37.64 | 40.49 | 46.62 | **64.02** | 47.46 |
| PPM#7 | 33.68 | 34.57 | 36.04 | 37.98 | 41.61 | 47.96 | **63.40** |

Reading: the SHIFTED diagonal (PPM#(k+1) vs PNG#(k+1), i.e. paraLLEl
post-#i vs PCSX2 post-#(i-1)) wins at EVERY row by ~20 dB (62.8–64.6 vs
44–47). Systematic one-vsync presentation lag, not a first-only artifact:
presented(i) = rendered-through-(i-1). Nonblack counts agree (shifted
diffs ~30–45 px vs same-k ~56–82). Double-shift is far worse (e.g.
PPM#2/PNG#1 44.67) — exactly ONE step of lag, not two.

### 2d. Initial-frame decode (`g11-vram.py`, `g11-vramcheck.py`, `g11-initframe.py`)

- Dump VRAM window: state blob ends 84 B after VRAM (4× GIF path + f32),
  cross-checked against paraLLEl `restart()` read order; FBP=112 base =
  offset 229,376 = page 28.
- VRAM bytes at FBP=112 base: `00 00 00 80` repeating (cleared).
- Full-frame decode through the scanout's own `swizzle_PS2` PSMCT24 path
  (FBP=112, FBW=8, 512×448): **0/229,376 nonblack RGB pixels**.
- Presented(0)=black is therefore consistent with pure one-vsync lag
  (presented(0) = initial VRAM = black-by-dump-content). No second cause
  needed. (PCSX2's dump-start .png shows the icon because its screenshot
  reflects presented state; its dumped VRAM snapshot is pre-draw — a
  PCSX2-side content note, not a claim.)

## 3. Task 1 — code path + bounded instrumentation (2 rebuilds, 2 reruns)

### 3a. Parser/consume path (static — kills `consume_vsync_result` lag)

`GSDumpParser::iterate_until_vsync` (dump/gs_dump_parser.cpp:137): per
VSync packet runs `iface->flush()` then `vsync_result = iface->vsync(…)` and
returns true (every dump-vsync has transfers). `consume_vsync_result`
(:209) swaps out exactly that just-recorded result. The G10 hook consumes
immediately after each iterate-true. Parser/consume ordering CANNOT lag by
construction — the miss is downstream of recording, in what the recorded
GPU work samples.

### 3b. Submit path (static — constrains to recording order)

Per iterate-true: `flush()` → `flush_submit` (draws) runs BEFORE `vsync()`
→ `flush_submit` (scanout, gs_renderer.cpp:5143) on the same generic
queue. Granite `Device::submit` takes the handle by reference and moves
from it (Granite/vulkan/device.cpp:1248) — submitted buffers reset, so
scanout records into fresh command buffers submitted after the draws.
Same-queue submission order + coherent unified memory + the observed
DETERMINISTIC full-frame lag signature (byte-identical across 3 runs/2
rebuilds) rule out GPU cross-submit visibility / missing-barrier races
(which would show nondeterminism or partial updates). Backbuffer promotion
is off (`backbuffer_promotion = false`, gs_interface.hpp:229) — scanout
always samples live VRAM (`sample_circuit.frag` slice 0, verified), which
the rasterizer writes directly (`ubershader.comp:747`).

### 3c. Instrumentation (H1–H4, LOGI-only, 39 added lines)

`g11-hook.py` (H1–H3) + `g11-hook2.py` (H4), exact-match on
gs/gs_interface.cpp: H1 — `flush()` entry logs the primitive accumulator
(prims/instances/states/tex); H2/H3 — `a_d_FRAME_1/2` log draw-target
identity on change; H4 — `flush_render_pass` logs each recorded batch
(prims, per-instance FRAME FBP, per-texture TEX0 TBP0). Two `-j2` rebuilds
(exit 0, only the known `ld` duplicate-libraries notice), two reruns
(`--iterations 2`, exit 0, 18 `Running frame` lines, heap 453/459 →
457/464 MiB == G10).

Behavior-preservation receipt: all 10 G11-run PPMs sha-match G10's
(`99418f1b … 0a799a44`) — the instrumentation is read-only in effect as
well as intent. Takes reproduce G10 exactly (34/2/1 every vsync, both
passes; img pool growth at cold #0 only).

### 3d. Batch structure (H4 — identical all 16 iterates, both passes)

Per dump-vsync (uniform — 16/16 records each):

| batch | prims | target (FRAME) | samples (TEX0) | recorded |
| --- | --- | --- | --- | --- |
| A (composite) | 17 | FBP=112 FBW=8 PSM=1 (the scanned buffer) | TBP0=0 TBW=8 TPSM=0 (the sprite buffer) | mid-stream, at the FRAME 112→0 hazard flush (`tracker.flush_render_pass` → `cb.flush(FB_ALL)` → interface :412) |
| B (sprite) | 17 | FBP=0 FBW=8 PSM=0 (off-screen) | TBP0=3584 TBW=1 TPSM=27 CBP=10752 (static atlas + palette; pal=1) | LATER, inside `flush()` (after H1 entry line, before submit — via transfer/upload resolution triggering a tracker flush) |

Order per iterate (log-observed): record(A) → flush-entry (acc=17: batch
B still pending) → record(B) → ONE submit (A-shading before B-shading in
the command stream) → vsync scanout → take (34/2/1).

Reasoning-correction note (H1 alone misleads): the H1 line (`acc prims=17`
at every flush) initially suggested cross-vsync deferral (batch B slipping
to the next vsync). H4 refutes it: B records INSIDE its own flush. Takes
are therefore self-contained (T(k)=34 ∀k — no carry, no cross-pass
contamination, no T(0)≠T(k) paradox). The lag is NOT deferral — it is
RECORDING ORDER.

### 3e. Localization: the stage

**Named stage: interface-side recording order within the vsync flush —
the mid-stream FRAME-switch hazard flush records the dependent composite
(batch A, sampling TBP0=0) BEFORE the sprite batch it depends on (batch B,
recorded later inside `flush()`).** The single submit therefore executes
composite-shading before sprite-shading; the composite's texture reads are
WAR-ordered against the sprite's writes and see the PREVIOUS vsync's
sprite. Presented(k) = composite(k) ∘ sprite(k−1): exactly one vsync of
lag, every vsync.

First-black (#0): composite(0) samples initial buffer-0 (cleared
`00 00 00 80`) and initial buffer-112 is RGB-black (§2d) → scanout #0 is
black although its 34 prims executed. #0's own sprite batch renders too
late for scanout #0 and first appears via composite(1). No second cause.

Killed with receipts (in addition to §2b/§3a/§3b): upload deferral —
`flush_pending_transfer` drains fully each flush (`last_flushed_qwords =
size()`, nothing spans vsyncs) and the matrix shows single, not double,
lag; cross-vsync batch deferral — H4 order (§3d).

Explicitly OPEN (opposite side): HOW PCSX2 achieves sprite-first-effective
(post-#k = composite(k) ∘ sprite(k), forced jointly by the matrix and
paraLLEl's observed recording order — the absolute anchor is paraLLEl's
own record order, not a PCSX2 assumption). Candidates: different batching
granularity (no mid-stream FRAME-switch split), deferred composite
rasterization, or present-path resolve. Needs PCSX2-side draw-level
tracing — out of G11's read-only-inspection contract. This is the §4
action, not a gap in the paraLLEl-side localization (every link of which
is directly observed).

## 4. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| The per-vsync series names the stage where iterate-true #0's rendered content misses presentation | **SUPPORTED**: the miss is the recording order inside the vsync flush — dependent composite recorded (mid-stream FRAME-switch hazard flush) before its sprite input (recorded later inside `flush()`), so scanout #k shows sprite(k−1); #0 composites cleared initial VRAM → black. Series + matrix + batch tables, §§2–3 |

The ONE next action the numbers justify (adoption input, not an adoption
decision): **trace PCSX2 gsrunner's per-draw execution order for
dump-vsync#0 — when the FBP=112-composite vs the FBP=0-sprite rasterize
relative to each other and to the present — to find where
sprite-first-effective arises on the oracle side.** G11 fully explains
paraLLEl-behind-own-stream (recording order, every link observed); the
matrix forces PCSX2-current; the asymmetry's other half needs the same
draw-level observability on PCSX2. Recipe: bounded log-only patch (log
FBP/TBP per HW draw + present markers), rebuild gsrunner, rerun the
EXISTING dump, compare batch order vs G11's H4 table (needs a bytesize
build/run — lane-owner lease question, not taken here). Queued behind it
(not this action): paraLLEl-side confirmatory probes (flush-reason tag for
batch B's exact trigger; per-submit VRAM snapshot making WAR-order
direct), then a rich post-loading dump. Rationale: the oracle-side "how"
decides whether paraLLEl's lag is a fidelity bug or a batching choice the
reference happens not to make — that is the adoption-relevant finding;
paraLLEl-side refinements would only re-confirm a localized stage.

No verdicts beyond the hypothesis. No port, no adoption, no upstream
contact. Odin on-device init/replay REMAINS open (G7 §5 recipe on file,
untouched).

## 5. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (clone HEAD re-verified) |
| G11 hooks (H1–H4) | local experiment change, uncommitted in the SSD clone only (`gs/gs_interface.cpp` — file already carries LGPL-3.0+ SPDX; no license change, nothing copied anywhere); verified log-only by build + 2 runs + 10/10 PPM sha-identity with G10 |
| G8+G10 replayer hook | untouched by G11 (still uncommitted in the SSD clone) |
| PCSX2 bytesize work | read-only `grep`/`sed` in `pcsx2-g7` (PMODE layout, paths); no builds, no runs, no writes |
| G11 scripts | `g11-privregs.py`, `g11-xdiff.py`, `g11-vram.py`, `g11-vramcheck.py`, `g11-initframe.py`, `g11-hook.py`, `g11-hook2.py` authored this brief — mirror carries all seven as text |
| brew/tools installs | none (G8's set + PIL reused as-is) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 6. Exact commands

Mac (`/Users/bradrichardson/dev/ps2xGS` unless noted; `COPYFILE_DISABLE=1`
on SSD steps; `VK_ICD_FILENAMES=…MoltenVK_icd.json`,
`DYLD_LIBRARY_PATH=/opt/homebrew/lib` on replayer runs):

```text
git -C "/Volumes/Extreme SSD/parallel-gs-g7" rev-parse HEAD   # 3a66c19… (verified pre-work)
python3 /tmp/g11-privregs.py <g10-dump>.gs                     # per-vsync PrivRegs decode (§2b)
python3 /tmp/g11-xdiff.py <g10-dir>                            # 8x7 cross-k matrix (§2c)
python3 /tmp/g11-vram.py <g10-dump>.gs                         # VRAM zero-check (§2d)
python3 /tmp/g11-vramcheck.py <g10-dump>.gs                    # cleared-pattern confirm (§2d)
python3 /tmp/g11-initframe.py <g10-dump>.gs                    # initial-frame swizzle decode (§2d)
cp <g10-dump>.gs "/Volumes/Extreme SSD/ps2x-g11/"              # working copy (sha-verified)
python3 /tmp/g11-hook.py                                       # H1-H3 apply (3 asserts pass)
cmake --build "/Volumes/Extreme SSD/parallel-gs-g7-build" --target parallel-gs-replayer -j2  # exit 0
./tools/parallel-gs-replayer "<g11-dump>.gs" --iterations 2    # rerun 1 (H1-H3)
python3 /tmp/g11-hook2.py                                      # H4 apply (1 assert passes)
cmake --build ... --target parallel-gs-replayer -j2            # exit 0
./tools/parallel-gs-replayer "<g11-dump>.gs" --iterations 2    # rerun 2 (H1-H4)
sha256sum <g11-dir>/*.ppm                                      # 10/10 match G10 (§3c)
du -sk <ssd dirs> ; df -h / "/Volumes/Extreme SSD"            # allocated + deltas
```

bytesize (each via ONE `ssh bytesize "wsl …"`, `;` separators, NO inline
pipes or redirects; single quotes inside the outer double quotes):

```text
wsl ls /home/brad/pcsx2-g7                                     # path re-discovery (nested pcsx2/ dir)
wsl grep -n -m2 -e 'struct GIFRegPMODE' -e 'EN1' …/pcsx2/pcsx2/GS/GSRegs.h
wsl sed -n '380,410p' …/pcsx2/pcsx2/GS/GSRegs.h                 # PMODE bit order (§2b)
```

Local experiment diffs (uncommitted): SSD clone
`tools/gs_dump_replayer.cpp` (G8+G10 hook, untouched by G11) +
`gs/gs_interface.cpp` (G11 H1–H4, recipe `g11-hook.py` + `g11-hook2.py`).

## 7. Gaps (what this brief could not do)

1. PCSX2-side "how" (sprite-first-effective): OPEN — the §4 next action,
   not speculation. Needs draw-level tracing (a bytesize build+run with
   a log-only patch — beyond G11's read-only-inspection rule).
2. Batch-B record trigger (exact sub-call inside `flush()`): INFERRED
   (transfer/upload resolution → tracker copy-hazard → `cb.flush(FB_ALL)`)
   from call-graph + log order, not reason-tagged. A one-line reason log
   would pin it (queued refinement).
3. WAR-order staleness is inferred from recording order + same-submit
   execution order + the matrix — a per-submit VRAM snapshot would make
   it direct (queued refinement).
4. Post-vsync#7 still has no PCSX2 file (G10 gap #1, unchanged — the
   matrix's PPM#7 row matches PNG#7/post-#6, consistent with the lag).
5. G9 PMODE sub-decode erratum (§2b): MMOD/ALP positions corrected here;
   G9's headlines unaffected (they used only EN1/EN2).
6. Loading-icon content stays a light draw load (34 prims / 2 passes per
   vsync; scene-scale rasterization still queued behind the comparison
   work).
7. No isolated GPU time (host wall only — unchanged from G8/G10).
8. Odin on-device project init/replay: still OPEN (no change).
9. `upstream/` and ps2xGS harness code untouched.

## 8. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…` (+G7 shim,
  +G8/G10 replayer hook, +G11 H1–H4).
- Build: `/Volumes/Extreme SSD/parallel-gs-g7-build/`,
  `tools/parallel-gs-replayer` (51,879,240 B).
- Dump dirs: `/Volumes/Extreme SSD/ps2x-g8/` + `ps2x-g10/` (pristine,
  shas re-verified); `/Volumes/Extreme SSD/ps2x-g11/` (.gs copy + 10
  PPMs, sha-identical to G10's).
- Logs: `/tmp/g11-privregs.log`, `/tmp/g11-xdiff.log`,
  `/tmp/g11-vram.log`, `/tmp/g11-vramcheck.log`,
  `/tmp/g11-initframe.log`, `/tmp/g11-replayer-build.log`,
  `/tmp/g11-replay.log`, `/tmp/g11-replayer-build2.log`,
  `/tmp/g11-replay2.log` (session-only).
- Tools: `/tmp/g11-privregs.py`, `/tmp/g11-xdiff.py`, `/tmp/g11-vram.py`,
  `/tmp/g11-vramcheck.py`, `/tmp/g11-initframe.py`, `/tmp/g11-hook.py`,
  `/tmp/g11-hook2.py` (mirrored to ssx3; `/tmp` originals session-only).
- Commits: ps2xGS `[G11]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G11/` `[G11]` + same trailer (NOT pushed).
