# G12 report — PCSX2-side per-draw execution order for dump-vsync#0 (existing dump)

Brief: G12 (this turn) — executes G11's §4: trace PCSX2 gsrunner's
per-draw execution order for dump-vsync#0 on the EXISTING dump, with ONE
bounded log-only patch. Tables + hypothesis + next-action recommendation,
no verdicts beyond the hypothesis. Time box 6 h (used ~3.5 h). Read first
per the brief: `docs/reports/G11.md` (all) + `docs/reports/G10.md` §3c
(gsrunner build/run recipe).

Machine: same as G8–G11 (Apple M4, macOS 27.0 — no new installs) +
bytesize WSL (Ubuntu 24.04, clang, cmake, Ninja, llvmpipe — G10's build
cache reused, no new toolchain). Pins: PCSX2 `9056c08349cc29ad02a6d1a3a4133259019195af`
(re-verified, §2a) / dump sha `64f6cddf…` (re-verified both sides, §2a).

Headline result: the PCSX2-side trace names the stage — and it is NOT a
rasterization stage. Per-HW-draw FBP/TBP log + present markers for all 8
dump-vsyncs (contract asked #0, extended to #0–#7 free) show PCSX2
executing **composite-before-own-sprite in every window**, content-identical
to paraLLEl: window#0 = C(0)[FBP=112←TBP0=0, rasterizes] + U(0)[FBP=0
untextured, never rasterizes], windows#1–7 = S(k−1)[lagged one window,
rasterizes] + C(k)[rasterizes] + U(k)[never rasterizes]. No image transfers
exist anywhere in the dump (88/88 transfers decoded, read-only), initial
VRAM block-0 is the cleared pattern, and presents are never skipped with
synchronous capture — so present#0's content is black on PCSX2 too, and
**file N = present#(N+1)**: gsrunner's snapshot numbering lags one present
by FIFO construction (Main-thread counter sync lambda queued after each
VSync packet runs after that vsync's present). G10's file↔boundary mapping
(file N ↔ post-vsync#(N−1)) was off by one; corrected (file N ↔ post-vsync#N),
the G11 matrix shows **same-boundary agreement at every step** (PPM#i ≈ PNG#i
= both post-#i, 63–65 dB). The G8–G11 "first-black lag" was a labeling
artifact, not a rendering difference: paraLLEl's recording order (composite
shading before sprite shading) is faithful to in-order packet execution
(C-prims precede S-prims in every vsync's packets), and G11's mechanism
observations all stand — only the file↔boundary premise falls.

## 0. Byte caps (declared) vs actuals (apparent + allocated)

| class | cap | actual apparent | actual allocated / delta |
| --- | --- | --- | --- |
| bytesize build growth (gsrunner reconfigure NOT needed; 2 objects + relink `-j2`) | 500 MB | binary 90,452,152 → 90,455,240 B (+3,088) | build dir 666,588 → 666,596 KiB (+8 KiB; pre-build == G10-end exactly) |
| bytesize run outputs (`g12-frames` + emulog + hook scripts) | 500 MB | 7 PNGs (~48 KB) + emulog 13,069 B + hook 4,991 B + fix 858 B | `g12-frames` 60 KiB |
| bytesize staging (`C:\pcsx2-t4\` retrieval copies) | counts only | 7 PNGs + emulog + 2 scripts (~70 KB) | — (network) |
| bytesize dat-g8 | 0 growth | 0 | 378,536 KiB unchanged (== G8/G10) |
| Task retrieval (7 PNGs + emulog) | 20 MB | ~62 KB to `/Volumes/Extreme SSD/ps2x-g12/` | — (network) |
| SSD G12 dir (new: 7 PNGs + emulog) | 20 MB | ~62 KB | 9,216 KiB (`du -sk`) |
| SSD G8/G9/G10/G11 dirs | 0 growth (pristine) | 0 | 16,384 / 28,672 / 43,008 / 28,672 KiB unchanged |
| internal volume (`/`) | 0 (no installs) | 0 installs; `/tmp/g12-*` ~28 KB (4 scripts, session-only) | `df` 13 → 15 Gi avail (other-lane freed 2 Gi); SSD 445 → 435 Gi avail (10 Gi other-lane — this brief wrote ~62 KB + scripts) |
| committed to git | text only (report + scripts) | this file (ps2xGS) + REPORT.md + 4 scripts (ssx3 mirror); no captures, no binaries, no build dirs | — |

No code copied into any GPL tree. No P-lane contention: no recomp
boots/builds, no lease (PCSX2-side, T28 recipe shape), no fork writes, no
`adb`, no bytesize builds/runs outside `pcsx2-g7` (`-j2` target-only
rebuild + one `-loop 1` rerun + one stillborn CWD-relative invocation).

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | The PCSX2-side trace names the stage where sprite-first-effective arises |
| observable signal | per-HW-draw FBP/TBP log + present markers for dump-vsync#0 (extended to #1–#7 — free) |
| alternatives | (a) stage named → table + ONE next action (fidelity bug vs batching choice); (b) trace inconclusive → table + recipe, stop |
| stop condition | existing dump + ONE bounded log-only patch — no tuning loop, no renderer changes, no new content |
| outcome → next action | numbers name the next single experiment (§4) |

Outcome: alternative (a) — with an inversion: the named stage is the
present/snapshot-numbering path, and the numbers show sprite-first-effective
arises NOWHERE in rasterization (both sides execute composite-before-own-sprite).

## 2. Task 1 — oracle-side draw-order trace (existing dump, one bounded patch)

### 2a. Reuse verification (pre-work)

| item | observed |
| --- | --- |
| bytesize source HEAD | `9056c08349cc29ad02a6d1a3a4133259019195af` (== G8/G10 pin) |
| tree status (pre-work) | ` M pcsx2/GS/GS.cpp, pcsx2/GS/GSState.cpp, pcsx2/R5900OpcodeImpl.cpp` (G7+G8 hunks, dormant under replay — no EE) |
| dump (bytesize `dat-g8/…snaps/SSX 3_SLUS-20772_20260920212606.gs`) | sha `64f6cddfced79692…752fc21b` (== G8/G10/G11) |
| dump (SSD `ps2x-g10` copy) | same sha (re-verified before decode, §2e/§2f) |
| G10 build cache | `pcsx2/build/bin/pcsx2-gsrunner` 90,452,152 B present; build dir 666,588 KiB (== G10 end); `ENABLE_GSRUNNER` still ON (no reconfigure needed) |
| bytesize load at start | 0.01 (WSL just rebooted; no other-lane activity seen) |

### 2b. The ONE bounded log-only patch (`g12-hook.py`, H1–H6 + `g12-fix.py`)

Six exact-match hunks (each asserts count==1, aborts otherwise), `Console.WriteLn`
only — the established G8 emulog channel, captured via gsrunner `-logfile`:

- H1 `GSRendererHW::Draw()` entry: `G12_DRAW n=… FBP/FBW/FPSM TBP0/TBW/TPSM TME/PRIM verts` (verts = `vtx_buff.tail - vtx_buff.head` — reads 0 uniformly, §2d probe note).
- H2/H3 the two Draw() early returns: `G12_SKIP reason=badframe|blackpoint`.
- H4 `GSRendererHW::DrawPrims()` entry: `G12_RASTER n=… rtTBP0/dsTBP0/texTBP0/texTBW` (target blocks in TBP0 units; null-guarded).
- H5 `GSRendererHW::VSync()` entry: `G12_VSYNC field/idle/n/DISPFB0_FBP` (HW/MTGS-thread marker, FIFO-ordered with draws).
- H6 `GSDumpReplayer.cpp` VSync-packet case: `G12_DUMP_VSYNC dump_frame=…` (CPU-thread marker).

Final diff: `GSRendererHW.cpp` +22/−1, `GSDumpReplayer.cpp` +1 (all
uncommitted; G7+G8 hunks untouched). Recipe notes (all inside the mirrored
scripts): H2's anchor includes the just-applied H1 block (the same GL_INS
line appears 2× more, deeper in Draw); H4 anchors on the full DrawPrims
signature (a second function shares the parameter-list tail); H3 is braced
(`g12-fix.py` repaired the live tree after review caught a bare-`if` that
would have made `return;` unconditional — the mirrored `g12-hook.py` emits
the braced form directly, so from-scratch application == live-tree state).

### 2c. Rebuild + rerun (existing dump)

- Rebuild: `cmake --build …/pcsx2/build --target pcsx2-gsrunner -j2` → exit 0,
  3 steps (2 objects + link), no new warnings. Binary +3,088 B.
- Run 1 (stillborn, recipe-relevant): bare dump filename with default CWD →
  gsrunner used it as-is (CWD-relative, `Main.cpp:750-752`), replayed nothing
  (empty frames dir, 2,721 B emulog ending after "Applying settings…").
  G10's `-- '<dump>.gs'` placeholder needs an absolute path (or snaps-dir CWD).
- Run 2 (the trace): G10 §3c shape with absolute dump path, `-loop 1`
  (G10 proved loop-1 PNGs == loop-2 PNGs), `-renderer vulkan -noshadercache
  -surfaceless -ini g10-uncorrected.ini -dumpdir g12-frames -logfile
  emulog-g12.txt` → exit 0, `HW STATISTICS FOR 8 (8) FRAMES`: **29 draw calls
  / 29 render passes / 0 barriers / 14 copies / 2 uploads / 6 readbacks —
  identical to G10's loop-1 control**, and **7/7 PNG md5s match G10 §3c
  exactly** (`e0464128 aa0053f9 db676261 43444061 810bbe55 bc95ce34 dad65e30`).
  Behavior-preservation receipt: the patch is log-only in effect as well as intent.
- Trace yield: 54 G12 lines (23 DRAW + 15 RASTER + 8 VSYNC + 8 DUMP_VSYNC + 0 SKIP).

### 2d. Per-HW-window draw-order table (MTGS-thread order; the observable)

`n` = `s_n` (bumped by `FlushPrim` per prim-flush and by `Merge` per vsync, §2g).
All DRAWs: PRIM=6 (sprite). RASTER `rtTBP0` is in 256-B-block units:
`FRAME::Block() = FBP<<5` (verified `GSRegs.h:330`), so rtTBP0=3584 IS the
FBP=112 target (112×32) — no redirect; dsTBP0=7168 ⟺ ZBP=224.

| HW window (dump-vsync → HW-VSYNC) | n | batch | DRAW (FBP←TBP0, TME) | RASTER (rt←tex) |
| --- | --- | --- | --- | --- |
| #0 (phase 1 → field 1) | 1 | C(0) composite | 112←0, TME=1 | 3584(=112)←0 ✓ |
| | 2 | U(0) untextured | 0←0, TME=0 | — (never reaches DrawPrims) |
| | — | HW-VSYNC#1 (DISPFB0=112) | — | — |
| #1 (phase 0 → field 0) | 4 | **S(0) sprite, LAGGED one window** | 0←3584 (atlas, TPSM=27), TME=1 | 0←3584 ✓ |
| | 5 | C(1) | 112←0, TME=1 | 3584←0 ✓ |
| | 6 | U(1) | 0, TME=0 | — |
| | — | HW-VSYNC#2 | — | — |
| #2–#7 (identical shape) | 4k,4k+1,4k+2 (k=2..7) | S(k−1) → C(k) → U(k) | same FBP/TBP0/TME pattern | S,C rasterize; U never |
| | — | HW-VSYNC#3–#8 (fields 1,0,1,0,1,0) | — | — |

After HW-VSYNC#8: nothing (S(7) never draws — trailing prims, no C(8) to flush
them; replay ends without a final prim-flush). No G12_SKIP anywhere (neither
early-out fires); the U-draws pass both logged checks and skip deeper
(open micro-point §7.3 — provably non-rasterizing: 8 U-DRAWs, 0 U-RASTERs).

`verts=0` probe note: `vtx_buff.tail - vtx_buff.head` reads 0 at Draw() entry
uniformly (`FlushPrim` consumes/resets the accumulators before Draw runs;
Draw-body counts come from `idx_buff.tail` later). Probe infidelity, not a
behavior signal — prim counts below come from the packet decode (§2e) and
G11 H4 instead. No re-patch (ONE-patch contract).

**H4 comparison (G11 §3d vs G12, side by side):**

| G11 H4 (paraLLEl record order, per iterate) | G12 (PCSX2 MTGS-execution order, per window) |
| --- | --- |
| A: 17 prims, FBP=112←TBP0=0, recorded mid-stream at FRAME 112→0 hazard flush | C: DRAW FBP=112←TBP0=0, rasterizes into the 112-target (rt=3584blk) |
| B: 17 prims, FBP=0←atlas(3584)+palette, recorded later inside `flush()` | S: DRAW FBP=0←TBP0=3584/TBW=1/TPSM=27, rasterizes into the 0-target (rt=0) — **in window#k≥1 the S-draw is S(k−1), lagged (§2e)** |
| (B is one batch: FRAME=0 throughout) | U: DRAW FBP=0, TME=0, 16 prims (§2e) — split from S by the TME change; never rasterizes |
| execute order within one submit: A-shading before B-shading ⟹ A(k) samples pre-S(k) state | execute order: C(k) before S(k)-draw (S(k) lags to window k+1) ⟹ C(k) samples pre-S(k) state — **SAME content order** |

### 2e. Packet decode (`g12-transfers.py`, read-only — geometry + transfer census)

GIF-stream walk of all 88 Transfer packets (PACKED + REGLIST tags, IMAGE-mode
accounting via TRXDIR/TRXREG+DPSM; ADDRs 0x50–0x53 verified against
`Gif_Unit.cpp` at the pinned rev). EOF-synced, `leftover_qw=0` on all 88,
all 8 vsyncs structurally identical:

| xfer/vsync (sizes 1056,592,48,144,80,48,64,48,80,144,144 — G10 census) | decoded content |
| --- | --- |
| #0 (1056 B) | context setup (27 A+D incl. TEX0_1=0) + REGLIST PRIM/RGBA + REGLIST 17×[UV,XYZ2,UV,XYZ2] = **17 sprites → C geometry** (= G11 A=17 ✓) |
| #1 (592 B) | PRIM(AD)=0x6 (sprite, TME=0) + RGBAQ + 32 XYZ2 (A+D) = **16 sprites → U geometry** (untextured) |
| #2–#4 | state (0x40/0x41 pairs, 0x47/0x48, ALPHA/CLAMP pairs — 0x4x names unmapped, §7.4) |
| #5 (48 B) | TEX0_1=**3584** + TEX1_1 (texture state → atlas) |
| #6 (64 B) | REGLIST 1×[PRIM,ST,RGBA,XYZ2,ST,XYZ2] = **1 sprite → S geometry** (textured), EOP=1 |
| #7–#10 | state restore (same shapes as #2–#4) |

Consequences (all observed, no inference beyond the walk):

1. **Zero A+D writes to 0x50–0x53 in all 8 vsyncs, and no 0xE (A+D) in any
   REGLIST tag-list** ⟹ NO host→local (or any) image transfers exist in this
   dump. The transfer-write candidate for sprite-first-effective is dead.
2. Per-vsync geometry: C=17 + U=16 + S=1 = **34 sprites = G10's 34 prims/vsync
   exactly** (independent count cross-check ✓). S+U = 17 FBP=0 prims = G11 B=17
   ✓ — paraLLEl merges them (same FRAME), PCSX2 splits them (TME change).
3. In-packet geometry order: **C … U … S** (S-prim trails in xfer#6). This
   FORCES the lagged-S reading: S(k)'s prim is the last geometry of vsync#k's
   packets; nothing after it flushes it (VSync does not prim-flush — proven by
   S(0)'s survival past HW-VSYNC#1 with no window#0 S-draw); vsync#(k+1)'s
   packets lead with C (xfer#0-shape) whose state change flushes S(k) ALONE
   (merge with S(k+1) impossible — C arrives first) ⟹ n=4m is S(m−1)-only.
   (Alternatives killed in §3.)

### 2f. Initial VRAM at the sprite buffer (`g12-vram0.py`, read-only)

G11-layout VRAM census (`state tail: VRAM 4 MiB + 84 B`): VRAM[0:64] is pure
`00 00 00 80` repeating; **FBP units 0–15 (bytes 0–32768, covering block-0):
0 non-`0x80000000` words in all 16 units** — block-0-initial is the cleared
pattern (G11's §2d buffer-0 claim now directly observed, not assumed). Page 28
(FBP=112 @229376): 2048 nonzero bytes = exactly the `0x80` bytes ⟹ cleared ✓
(G11 reconfirmed). (Atlas bytes @917504: sparse, 695/8192 nonzero — consistent
with a small icon atlas; not load-bearing.)

### 2g. Counter/forensics receipts (static, pinned rev)

- `s_n` bump sites: exactly TWO `IncDraw()` callers — `GSState::FlushPrim`
  (`GSState.cpp:2537`, per prim-flush → every G12_DRAW) and **`GSRenderer::Merge`
  (`GSRenderer.cpp:153`, once per vsync)** ⟹ the 7 s_n gaps (3,7,…,27) are the
  per-vsync display-merge bumps (8th, after HW-VSYNC#8, never logged — structural).
- No hidden rasterization: every RASTER matches a DRAW n (15/15); no orphans.
- 29 device draws (HWSTAT) vs 23 G12_DRAWs ⟹ 6 present-path/OSD device draws
  (outside `GSRendererHW::Draw` — expected, content-neutral here).
- `Uploads: 2` (HWSTAT) = texture-cache→device uploads (atlas+palette at cold
  start), NOT EE→GS transfers — consistent with §2e (no TRX in dump).

## 3. Localization: the stage (present/snapshot numbering) + content closure

### 3a. Present/content audit for window#0 — the lag has nowhere to hide

Writer audit for block-0 content before present#0 (all observed): initial VRAM
cleared (§2f); S-draws: none (no S-DRAW in window#0, §2d); U(0): non-rasterizing
(§2d); EE→GS uploads: none in the dump (§2e). ⟹ C(0) samples cleared block-0 ⟹
C(0) renders black into the 112-target ⟹ **present#0's synchronous content is
black on PCSX2** — yet PNG#1 (G10-mapped to post-#0) shows the icon. The capture
path is synchronous on the GS thread (`GSRenderer::VSync` → `SaveSnapshotToMemory`
inline into a pixels vector, `GSRenderer.cpp:713-793`, then async PNG encode),
presents run in MTGS FIFO order after their window's draws, and no present is
skipped here (SkipDuplicateFrames=true but every frame is DISPFBBlit-unique —
C-draws blit sprites to DISPFB each vsync, `FlushPrim` counts them,
`PerformanceMetrics` auto-selects; None/default also yields unique). A
content-lagging present is therefore impossible — the shift must be in the
snapshot NUMBERING. It is:

### 3b. The mechanism: file N = present#(N+1) (Main-counter FIFO lag)

- `Host::BeginPresentFrame` (`Main.cpp:238-247`) names each snapshot from
  Main's OWN `s_dump_frame_number` (`Main.cpp:85`) — not the replayer's.
- Main's counter is synced in `Host::PumpMessagesOnCPUThread` (`Main.cpp:905`):
  `MTGS::RunOnGSThread([frame_number = GetFrameNumber()](){ s_dump_frame_number
  = frame_number; })`, called from the replayer VSync case AFTER `PostVsyncStart`
  (`GSDumpReplayer.cpp:397-401`, which does NOT wait — queue-size throttle only,
  `MTGS.cpp:239-281`).
- GS-thread FIFO per dump-vsync#k: [VSync packet → GSvsync → HW::VSync →
  Merge → present#(k+1) reads Main counter (still k — the sync lambda was queued
  AFTER the VSync packet, so it runs AFTER this present) → file `_frame{k:05}`]
  → sync lambda runs → counter=k+1.
- ⟹ **present#j → file j−1; file N = present#(N+1) = post-vsync#N content.**
  Present#1 → file0 (absent: first-present capture else-branch — `GetCurrent()`
  null at the first present → OSD-only error, silent in emulog; inferred by
  code-path elimination, §7.1). Present#8 → file7 ✓. No present#9 → no file8 —
  this REFINES G10's gap #1 ("shutdown wins the race"): the missing file8 is
  structural (every present names the previous counter), not racy.

### 3c. Content closure: both sides show C∘sprite(k−1) at every true boundary

| index j (1..7) | PNG#j (file) | = present#(j+1) | content | PPM#j (iterate#j) | content | G11 matrix |
| --- | --- | --- | --- | --- | --- | --- |
| j | file j | post-vsync#j | C(j)∘S(0..j−1)-drawn | post-vsync#j | A(j)∘B(0..j−1)-drawn | PPM#j≈PNG#j, 63–65 dB ✓ same boundary |
| (0) | file0 absent (≈black present#1) | post-vsync#0 | C(0)∘cleared | PPM#0 black | A(0)∘cleared | agree (black) ✓ |

Both composites sample pre-own-sprite state — which is what the packets ORDER:
C-prims precede S-prims in every vsync (§2e), so strict in-order execution gives
C(k)∘sprite(k−1) on real hardware too. paraLLEl's A-before-B shading is faithful
to that order; PCSX2's C(k)-before-S(k)-draw (S lagged one window by trailing
layout + accumulation) is content-identical to it. G10's k=0 "black-vs-icon"
pair compared post-#0 (PPM#0) vs post-#1 (PNG#1) — never same-boundary. The
off-diagonal 44–47 dB = ±1 growth-step-apart (post-#i vs post-#(i±1)) ✓ consistent.

### 3d. Killed with receipts

- Different batching granularity (no mid-stream split): DEAD as a content
  difference — PCSX2 splits MORE (C/S/U separately) yet every C samples
  pre-own-S state, same as paraLLEl (§2d).
- Deferred composite rasterization: DEAD — every C-DRAW is immediately followed
  by its RASTER in MTGS order (no Draw/Raster gap, §2d).
- Transfer-written sprite (window#0 uploads before C(0)): DEAD — zero TRX in
  the dump (§2e).
- Present-content lag (stale capture / skipped presents): DEAD — captures are
  synchronous inline readbacks (§3a); no skips (DISPFBBlit-unique, §3a).
- Initial-VRAM sprite (dump block-0 alive): DEAD — cleared pattern, 0/8192
  non-cleared words in units 0–15 (§2f).
- Hidden rasterization (Move blits, unlogged paths): DEAD — 15/15 RASTERs match
  DRAWs, no orphans (§2g); U provably non-rasterizing (§2d).
- S-merge / S(0)-culled (vs lagged-S): DEAD — vsync#1 leads with C-geometry
  (forces S(0)-alone flush as n=4); S-prims exist in all 8 vsyncs' packets
  (§2e.3). (S(7) never draws — trailing, content-neutral, §7.2.)
- Render-target redirect (rt=3584 ≠ FBP=112): DEAD — `Block()=FBP<<5` units
  identity (§2d).

G11 disposition: ALL G11 mechanism observations stand (A-before-B record order,
17+17 counts, matrix numbers, VRAM-112 cleared — each reconfirmed or reused
above). Only the file↔boundary premise (G10's mapping) falls, and with it the
"lag"/"miss" verdict: presented(k)=C∘sprite(k−1) is CORRECT post-#k content,
not one-behind. G11's §4 question ("HOW does PCSX2 achieve sprite-first-effective")
is answered: it doesn't — the oracle agrees with paraLLEl at every true boundary.

## 4. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| The PCSX2-side trace names the stage where sprite-first-effective arises | **SUPPORTED, with inversion**: the named stage is the present/snapshot-numbering path (file N = present#(N+1) via Main-counter FIFO lag, §3b) + the lagged-S draw order (§2d–§2e); jointly they show sprite-first-effective arises NOWHERE — PCSX2 executes composite-before-own-sprite, content-identical to paraLLEl at every true boundary (§3c). G10's file↔boundary mapping was off by one; the G8–G11 "lag" was that artifact. paraLLEl's recording order is FAITHFUL on this dump (fidelity supported, not a bug) |

The ONE next action the numbers justify (adoption input, not an adoption
decision): **run the rich post-loading dump dual replay with corrected labels
(file N ↔ post-vsync#N) and S-lag-aware windowing — and retire the lag line.**
This dump is now fully closed (every draw, present, file, and counter
attributed; both sides agree) but too thin to discriminate batching (no
uploads, S=1 sprite, U degenerate, C-geometry static). The comparison method is
calibrated for the first time (labels corrected, S-lag understood, no-skip
verified, Merge-IncDraw named) — spend the next experiment on scene-scale
content with real transfer/draw interleaving, multi-target ping-pong, and depth,
where batching choices actually diverge. Queued behind it (not this action):
the §7 micro-probes (U skip-site reason tag, file0/GetCurrent direct
confirmation). Rationale: the oracle-side "how" turned out to be "it doesn't" —
further loading-icon work cannot move the adoption question; only richer
content can.

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.
Odin on-device init/replay REMAINS open (G7 §5 recipe on file, untouched).

## 5. License / provenance receipts

| artifact | receipt |
| --- | --- |
| PCSX2 source rev | `9056c08349cc29ad02a6d1a3a4133259019195af` (HEAD re-verified) |
| G12 hook (H1–H6 + brace fix) | local experiment change, uncommitted in bytesize `pcsx2-g7` only (`GSRendererHW.cpp` +22/−1, `GSDumpReplayer.cpp` +1 — files already carry GPL-3.0+ SPDX; no license change, nothing copied anywhere); verified log-only by build + run + 7/7 PNG md5-identity with G10 + HWSTAT-identity |
| G7+G8 hunks | untouched by G12 (still uncommitted, still dormant under replay) |
| G12 scripts | `g12-hook.py`, `g12-fix.py`, `g12-transfers.py`, `g12-vram0.py` authored this brief — mirror carries all four as text |
| paraLLEl-GS / SSD clone | untouched (no reads, no builds, no runs) |
| bytesize static reads | `grep`/`sed` only, in `pcsx2-g7` (paths, counters, present/snapshot code); one build (target-only `-j2`), two gsrunner invocations (one stillborn, one `-loop 1` trace) |
| brew/tools installs | none |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 6. Exact commands

Mac (`/Users/bradrichardson/dev/ps2xGS` unless noted; `COPYFILE_DISABLE=1`
on SSD steps except the retrieval, whose `._` files were removed after):

```text
python3 /tmp/g12-transfers.py "<ssd>/ps2x-g10/<dump>.gs" 8   # §2e (all 8)
python3 /tmp/g12-vram0.py "<ssd>/ps2x-g10/<dump>.gs"          # §2f
scp /tmp/g12-{hook,fix}.py "bytesize:pcsx2-t4/"              # staging (×3: hook v1, fix, hook v2+v3)
mkdir -p "/Volumes/Extreme SSD/ps2x-g12/"
scp "bytesize:pcsx2-t4/*_frame0000*.png" "bytesize:pcsx2-t4/emulog-g12.txt" "/Volumes/Extreme SSD/ps2x-g12/"
rm -f "/Volumes/Extreme SSD/ps2x-g12/._"*
md5 "/Volumes/Extreme SSD/ps2x-g12/"*.png                    # 7/7 == G10 §3c
du -sk <ssd dirs> ; df -h / "/Volumes/Extreme SSD"           # allocated + deltas
```

bytesize (each via ONE `ssh bytesize "wsl …"`; everything after the single
`wsl` runs in Linux, so chain LINUX commands with `;` — a second `wsl` fails;
NO `|`/`>`/`\|` — Windows cmd eats them; single quotes inside the outer
double quotes for spaced paths):

```text
wsl git -C /home/brad/pcsx2-g7/pcsx2 rev-parse HEAD            # 9056c08349… (§2a)
wsl git -C /home/brad/pcsx2-g7/pcsx2 status --short            # G7+G8 (+G12 after hook)
wsl sha256sum '/home/brad/pcsx2-g7/dat-g8/PCSX2/snaps/SSX 3_SLUS-20772_20260920212606.gs'  # 64f6cddf…
wsl find /home/brad/pcsx2-g7 -maxdepth 4 -name pcsx2-gsrunner  # build cache (pcsx2/build/bin/)
wsl grep -n -e 'GSRendererHW::Draw' -e 'GSRendererHW::VSync' …/GS/Renderers/HW/GSRendererHW.cpp
wsl grep -n -e 's_dump_frame_number' -e 'VSync' …/GSDumpReplayer.cpp
wsl sed -n '2776,2830p' …/GSRendererHW.cpp                    # Draw() entry (H1 anchor)
wsl sed -n '98,130p' …/GSRendererHW.cpp                       # VSync entry (H5 anchor)
wsl sed -n '9281,9300p' …/GSRendererHW.cpp                    # DrawPrims entry (H4 anchor)
wsl sed -n '380,410p' …/GSDumpReplayer.cpp                    # VSync case (H6 anchor)
wsl grep -n -e 'PRIM.TME' …/GSRendererHW.cpp ; grep -n -e 'DISPFB.FBP' …/GSState.cpp  # field proof
wsl grep -n -e 'm_regs' …/GSRendererHW.cpp
wsl cp /mnt/c/Users/bradr/pcsx2-t4/g12-hook.py /home/brad/pcsx2-g7/g12-hook.py ; python3 /home/brad/pcsx2-g7/g12-hook.py  # ×3 (H2/H4 anchor fixes between)
wsl cp /mnt/c/Users/bradr/pcsx2-t4/g12-fix.py … ; python3 …/g12-fix.py           # H3 brace fix
wsl git -C /home/brad/pcsx2-g7/pcsx2 diff …/GSRendererHW.cpp  # H3 review (caught bare-if)
wsl cmake --build /home/brad/pcsx2-g7/pcsx2/build --target pcsx2-gsrunner -j2   # exit 0, 3 steps
wsl timeout 300 …/build/bin/pcsx2-gsrunner -renderer vulkan -dumpdir …/g12-frames -logfile …/emulog-g12.txt -loop 1 -noshadercache -surfaceless -ini …/g10-uncorrected.ini -- '<abs dump path>'  # exit 0 (bare-name attempt first: stillborn §2c)
wsl md5sum …/g12-frames/*.png                                 # 7/7 == G10
wsl grep -e G12_ …/emulog-g12.txt                             # the 54-line trace (§2d)
wsl grep -e HWSTAT …/emulog-g12.txt                           # 29/29/0/14/2/6 == G10 loop-1
wsl grep -n -e 'Block()' …/GSRegs.h                           # FBP<<5 units (§2d)
wsl grep -rn -e 's_n++' …/GS/GSState.cpp …                    # → IncDraw only
wsl grep -rn -e 'IncDraw()' …/GS                             # → FlushPrim + Merge (§2g)
wsl grep -n -e 'MTGS::PostVsyncStart' -A 30 …/MTGS.cpp ; sed -n '269,310p' …/MTGS.cpp  # no-wait (§3b)
wsl sed -n '225,260p' …/pcsx2-gsrunner/Main.cpp ; grep -n -e 'void GSQueueSnapshot' -A 25 …/GS.cpp  # snapshot queue
wsl grep -n -e 'm_snapshot' …/GS/Renderers/Common/GSRenderer.cpp ; sed -n '700,795p' …  # sync capture (§3a)
wsl grep -n -e 's_dump_frame_number' …/pcsx2-gsrunner/Main.cpp ; sed -n '880,930p' …    # FIFO lag (§3b)
wsl grep -e 'Failed to render' -e 'GSScreenshot' -e 'frame00000' …/emulog-g12.txt      # silent (file0, §7.1)
wsl grep -n -e 'SkipDuplicateFrames' …/Pcsx2Config.cpp ; grep -rn -e 'InternalFPSMethod' …/PerformanceMetrics.cpp ; sed -n '185,220p' …  # no-skip (§3a)
wsl grep -n -e '0x50' -e '0x53' …/Gif_Unit.cpp ; sed -n '120,175p' …                   # TRX ADDRs (§2e)
wsl cp …/g12-frames/*.png …/emulog-g12.txt /mnt/c/Users/bradr/pcsx2-t4/      # retrieval staging (absolute WSL paths; spaced names quoted per-file)
wsl du -s …/build …/g12-frames …/dat-g8 ; du -b …/emulog-g12.txt …/g12-hook.py …/g12-fix.py  # §0
```

Local experiment diffs (uncommitted): bytesize `pcsx2/GS/GS.cpp` +
`pcsx2/GS/GSState.cpp` + `pcsx2/R5900OpcodeImpl.cpp` (G7+G8, untouched by G12) +
`pcsx2/GS/Renderers/HW/GSRendererHW.cpp` + `pcsx2/GSDumpReplayer.cpp` (G12,
recipe `g12-hook.py` as mirrored + `g12-fix.py` for the live tree) +
`g12-hook.py` + `g12-fix.py` + `g12-frames/` + `emulog-g12.txt` +
`build/bin/pcsx2-gsrunner` (90,455,240 B).

## 7. Gaps (what this brief could not do)

1. File0's absence is mechanism-INFERRED (first-present `GetCurrent()` null →
   capture else-branch → OSD-only error, silent in emulog; the only in-code
   path to no-file given the Host hook runs unconditionally), not directly
   observed. Direct confirmation recipe: a log line in the
   `SaveSnapshotToMemory` else-branch (`GSRenderer.cpp:~785`) — needs another
   bounded patch (not this brief's ONE). Does not affect content closure
   (present#1≈black follows from the writer audit regardless).
2. S(7)'s prims never draw (trailing, no C(8) to flush them; no DRAW after
   HW-VSYNC#8). Observed + content-neutral (beyond the last snapshot). A
   shutdown-path prim-flush would change nothing here.
3. U-draws' exact skip site unidentified (both logged early-outs pass; the skip
   is deeper in `Draw()` — the `return`s between the black-point check and the
   first `DrawPrims` call). Provably non-rasterizing (8 DRAWs, 0 RASTERs) and
   content-neutral (matrix agrees to 63 dB). Recipe: reason-tagged early-out
   logs (another bounded patch, queued).
4. GIF A+D ADDRs 0x40–0x4F (state regs in xfer#2–#4/#7–#10) left unmapped
   (names unverified; not FRAME/TEX0/TRX — geometry + TRX census complete
   without them). Cosmetic.
5. `verts=0` (§2d probe note): per-draw vertex counts not captured (FlushPrim
   consumes accumulators before Draw entry). Counts came from the packet decode
   + G11 H4 instead (34 = 17+16+1 cross-checked). No re-patch (contract).
6. Loading-icon content stays a light draw load (23 GS draws + 6 device draws
   over 8 vsyncs; no uploads, S=1 sprite, U degenerate). Scene-scale
   rasterization is the §4 next action (now with calibrated labels).
7. G10 gap #1 mechanism REFINED (not closed as stated): file8 is missing
   structurally (file N = present#(N+1); no present#9), not by shutdown race.
   Post-vsync#7 content (present#8 = file7) EXISTS — G10's "7 of 8 pairs" is
   really "pairs at post-#1..#7" (post-#0's file0 absent per #1).
8. No isolated GPU time (host wall only — unchanged from G8–G11).
9. Odin on-device project init/replay: still OPEN (no change).
10. `upstream/` and ps2xGS harness code untouched.

## 8. Receipt paths

- bytesize source: `/home/brad/pcsx2-g7/pcsx2/` @ `9056c08349…` (+G7/G8 hunks,
  +G12 hook — all uncommitted).
- bytesize build: `/home/brad/pcsx2-g7/pcsx2/build/`,
  `bin/pcsx2-gsrunner` (90,455,240 B).
- bytesize run: `/home/brad/pcsx2-g7/g12-frames/` (7 PNGs) +
  `/home/brad/pcsx2-g7/emulog-g12.txt` (13,069 B, 54 G12 lines) +
  `/home/brad/pcsx2-g7/g12-{hook,fix}.py`; staging copies at
  `C:\Users\bradr\pcsx2-t4\`.
- SSD: `/Volumes/Extreme SSD/ps2x-g12/` (7 PNGs + emulog; NO .gs copy — the
  existing dump was reused in place); `ps2x-g8/…-g11/` pristine.
- Tools: `/tmp/g12-hook.py`, `/tmp/g12-fix.py`, `/tmp/g12-transfers.py`,
  `/tmp/g12-vram0.py` (mirrored to ssx3; `/tmp` originals session-only).
- Commits: ps2xGS `[G12]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G12/` `[G12]` + same trailer (NOT pushed).
