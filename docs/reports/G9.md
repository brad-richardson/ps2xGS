# G9 report — geometry-alignment probe on the G8 dump (no new capture)

Brief: G9 (this turn) — executes the ONE next action from `docs/reports/G8.md`
§4: derive the expected scanout width from the G8 dump's own PrivRegisters,
compare against the replayer's 512 and PCSX2's 597, and test whether any
replayer CRTC flag yields a 597-wide scanout. Tables + hypothesis +
next-action recommendation, no verdicts. Time box 4 h (used ~1 h). Read first
per the brief: `docs/reports/G8.md` (§3c + §4) and `docs/reports/G7.md` §3
(the retired center-crop proxy — NOT resurrected: no pixel diff runs anywhere
in this brief except exact-geometry-or-nothing, and no same-geometry pair was
found, so no diff runs at all).

Machine: same as G8 (Apple M4, macOS 27.0 — no new installs this brief).
Pin: paraLLEl-GS `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd`
(Granite submodule `16e7395f6a4858c1783dbf6f521f90b9d5f82ac5`).
Candidate treated as an independent implementation, not an oracle.

Headline result: the 85 px gap is fully explained, and the explanation is not
a CRTC-geometry disagreement — both sides agree the internal scanout is
**512×448**. PCSX2's 597 is its screenshot path's 4:3 display-aspect
resample of that 512×448 frame (448 × 4/3 = 597.33 → 597), derived
end-to-end from dump bytes + pinned source + run ini and corroborated by the
G7 dump as a control (640×480 predicted the same way). No replayer CRTC flag
can yield 597: it is not a CRTC width. The sweep confirms empirically.

## 0. Byte caps (declared) vs actuals (apparent + allocated)

| class | cap | actual apparent | actual allocated / delta |
| --- | --- | --- | --- |
| bytesize new clones/builds/boots | 0 (read-only source greps only) | 0 — no builds, no boots, no dat writes, no lease | 0 |
| bytesize dat/build growth | 0 | 0 (nothing written) | — |
| SSD clone/build | 0 new (reuse; tree did not move, no rebuild) | replayer binary untouched, 51,867,352 B (== G8) | build dir untouched |
| SSD G8 dump dir | 0 growth (pristine; sweep runs in G9 dir) | 0 — both G8 PPM shas re-verified identical at end | 16,384 KiB unchanged |
| SSD G9 dir (new: .gs copy + 8 sweep PPMs) | 100 MB | 14,595,989 B (.gs 5,517,531, sha-matches G8 original; PPMs 9,078,458) | 28,672 KiB (`du -sk`); `df` 460→459 Gi (1 GiB ExFAT rounding step) |
| internal volume (`/`) | 0 (no installs) | 0 installs; `/tmp/g9-*` 27,556 B (3 scripts + help + 4 replay logs) | `df` 15 Gi avail unchanged start→end |
| committed to git | text only (report + scripts) | this file (ps2xGS) + REPORT.md + 3 scripts (ssx3 mirror); no captures, no binaries, no build dirs | — |

No code copied into any GPL tree. No local experiment changes anywhere this
brief (the G8 hook in the SSD clone is reused as-is, uncommitted, untouched).
No P-lane contention: no recomp boots/builds, no P-lane lease, no `adb`.

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | The 85 px width gap is explained by display-state geometry derivable from the dump itself, and a replayer CRTC flag yields a 597-wide scanout |
| observable signal | derived expected width, per-flag scanout widths, FIRST-vs-REF diff at exact geometry if a pair is found |
| alternatives | (a) gap unexplained by PrivRegs → table + recipe, stop; (b) no flag yields 597 → table all tried widths + stop; (c) matched pair found but FIRST still black → table, queue the per-vsync series (NOT this brief) |
| stop condition | this dump only — NO new capture, no tuning loop, no per-vsync series here |
| outcome → next action | numbers name the next single experiment (§4) |

Verdict granularity: the hypothesis has two separable claims — (i) the gap is
explained by dump-derivable display state, (ii) a replayer CRTC flag yields a
597-wide scanout. §4 scores them separately.

## 2. Task 1 — derive the expected width (no runs)

### 2a. Reuse verification (pre-work)

| item | observed |
| --- | --- |
| `/Volumes/Extreme SSD/parallel-gs-g7` HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (== G8 pin) |
| tree status | ` m Granite`, ` M tools/gs_dump_replayer.cpp`, `?? tools/._gs_dump_replayer.cpp` — exactly G8's diffs, tree did NOT move |
| replayer binary | 51,867,352 B (== G8 receipt) — NO rebuild |
| G8 dump `.gs` | sha `64f6cddf…` (== G8); G9 working copy sha-matches |
| PCSX2 source rev (bytesize, read-only) | `9056c08349cc29ad02a6d1a3a4133259019195af` (== G8 producer rev) |
| G8 PPM pristine baseline | first `99418f1b…`, last `0a799a44…` (re-verified identical after the sweep) |

### 2b. PrivRegs decode from the dump's own bytes (`g9-privregs.py`)

Layout cross-check: parallel-gs `PrivRegisterState`
(`gs/gs_interface.hpp:70`, 16 B stride, low qword live) reads the 8192 B blob
directly; PCSX2 `GSPrivRegSet` (`pcsx2/GS/GSRegs.h:1243`) confirms the
identical order (PMODE@0x00, SMODE1@0x10, SMODE2@0x20, SYNCV@0x60,
DISPFB1@0x70, DISPLAY1@0x80, DISPFB2@0x90, DISPLAY2@0xA0, …,
CSR@0x1000). Bit positions verified against PCSX2 `GSRegDISPLAY`
(`GSRegs.h:333`, identical to parallel-gs `DISPLAYBits`).

Header regs0 (dump-start state) + all 8 PrivRegs packets: every named display
register STABLE across all 9 blobs. The only varying byte in any packet is
CSR+1 (`0x1001`, `0x60`↔`0x40`, delta `0x20` = CSR bit 13 FIELD toggling with
interlaced phase — expected, not geometry).

| register | source bytes (regs0 lo qword) | decoded fields |
| --- | --- | --- |
| PMODE | `0x000000000000ff21` | EN1=1, EN2=0, MMOD=0, AMOD=0, SLBG=0, ALP=249 |
| SMODE1 | `0x0000000740814504` | RC=4, LC=32 (ANALOG), T1248=1, CMOD=2 (NTSC), PRST=1, SPML=4, CLKSEL=1, NVCK=1, SLCK2=1, VCKSEL=1 |
| SMODE2 | `0x0000000000000001` | INT=1, FFMD=0 (interlaced FIELD mode) |
| SYNCV | `0x00c7800601a01801` | VFP=1, VFPE=6, VBP=26, VBPE=6, VDP=480, VS=6 |
| DISPFB1 | `0x0000000000009070` | FBP=112, FBW=8, PSM=1, DBX=0, DBY=0 |
| DISPLAY1 | `0x001bfa0002032281` | DX=641, DY=50, MAGH=4, MAGV=0, DW=2560, DH=447 |
| DISPFB2/DISPLAY2/EXT* | all zero | circuit 2 fully unset, EN2=0 |
| BGCOLOR | zero | — |

Run ini (dat-g8, read-only): `AspectRatio = Auto 4:3/3:2` (line 126),
`ScreenshotSize = 1` (line 128), Crop all 0 (134–137). Not in ini → pinned
defaults: `UpscaleMultiplier = 1.0f` (`Config.h:725,868`),
`PCRTCOffsets = PCRTCOverscan = false` (`Pcsx2Config.cpp:716-717`).

### 2c. Chain (A) — PCSX2 sidecar PNG width, derived (`g9-derive.py`)

Refs are PCSX2 rev `9056c08349`.

| step | computation | value |
| --- | --- | --- |
| video mode | `GetVideoMode()` (`GSState.cpp:834`): CMOD=2 → NTSC (`:854-855`) → index 0 | NTSC |
| tables | `VideoModeOffsets[0]` / `VideoModeDividers[0]` (`GSState.cpp:7066/7084`) | (640,224,642,25) / (3,0,2559,239) |
| `SetRects` c0 (`:7312`) | mag=(MAGH+1,MAGV+1)=(5,1); DW+1=2561, DH+1=448; render=2561//5 × 448//1 | 512×448 |
| `SetRects` clamp | PCRTCOffsets=false → min(512, 2561//4=640) × min(448, 448//1) | displayRect 512×448 |
| circuit 2 | FBW=0 & DW=DH=MAGH=0 → force-disabled (`:7325`) | single circuit |
| `GetResolution` (`:7172`) | single enabled → {512,448}; clamp x≤640, y≤224<<1=448 (interlaced=INT&&analogue) | 512×448 |
| upscale ×1.0 | `fs` = 512×448 = `m_real_size` = `current` texture (`GSRenderer.cpp:221-225`); Crop=0 → src_rect = full texture (`:443`) | 512×448 internal |
| aspect | ScreenshotSize=1=InternalResolution (`Config.h:362`): internal + aspect-corrected (`GSRenderer.cpp:776-782`); Auto, non-progressive → 4/3 (`:293`) | 4/3 |
| `SaveSnapshotToMemory(0,0,true,true)` (`:1061`) | tex 512×448, tex_aspect 1.1429 < 4/3 → height-driven: draw=(448×4/3, 448)=(597.333→u32, 448) (`:1086-1110`) | **PNG 597×448** |

Observed sidecar PNG: 597×448. **MATCH.** (The header embedded shot is a
separate fixed 640×480 capture: `DUMP_SCREENSHOT_WIDTH/HEIGHT`
`GSRenderer.cpp:738-740` — explains why it never varies.)

### 2d. Chain (B) — replayer scanout width, derived (`g9-derive.py`)

Refs are parallel-gs rev `3a66c197`.

| step | computation | value |
| --- | --- | --- |
| VSyncInfo (defaults) | parser sets force_progressive + anti_blur + adapt (`gs_dump_parser.cpp:173-186`); PrivRegs blob read straight into `PrivRegisterState` (`:198`) | — |
| NTSC branch (`gs_renderer.cpp:4334`) | CMOD=NTSC, LC=ANALOG, no overscan → 640×224, off (159,25); force_progressive clears interlace → ×448 | mode 640×448 |
| clock divider | composite (`SMODE1Bits::CLOCK_DIVIDER_COMPOSITE`) | 4 |
| circuit 1 (`compute_circuit_rect`, `:4189`) | DW/MAGH = 2561//5 = 512; DH/MAGV = 448 → (448+1)&~1 | 512×448 |
| adapt (`:4760-4797`, always on) | h0=512, h1=0→512, equal → scaling = 4/5 = 0.8 → mode = round(640×0.8) | mode **512**×448 |
| output (`:4845-4850`) | hi-res off at X1 (sampling log2s (0,0) force it off, `:4305`; `gs_interface.cpp:75-76`) → internal=mode, image=mode | **PPM 512×448** |

Observed G8 FIRST/LAST PPMs: 512×448. **MATCH.**

### 2e. Width-derivation table + G7 control

| value | width | source |
| --- | --- | --- |
| PCSX2 internal (`m_real_size`, pre-aspect) | 512 | derived §2c from DISPLAY1/DISPFB1/SMODE + ini (UNEQUALLED — no PCSX2 run this brief; the formula is the receipt) |
| replayer internal/mode (defaults) | 512 | derived §2d from the same regs; observed G8 PPMs 512×448 |
| PCSX2 sidecar PNG (aspect-corrected) | 597 | derived §2c: u32(448×4/3); observed 597×448 |
| gap 85 px | 597−512 | aspect resample (bilinear `StretchRect`, `GSRenderer.cpp:1118`), NOT CRTC geometry |

Control — the same `g9-derive.py`, unmodified, on the G7 dump (PMODE=0 both
circuits off, SMODE2=0x3, DISPLAY zero): PCSX2 both-disabled →
{640, 224<<1=448} → width-driven branch (1.4286 ≥ 4/3) → draw=(640,
u32(640/(4/3))=480) → **640×480 MATCH** (observed G7 PNG); replayer no
circuits → no adapt → **640×448 MATCH** (observed G7 default PPM);
conservative → overscan base, no adapt → **712×240 MATCH** (observed G7
`--conservative-crtc` PPM). Both chains corroborated on independent bytes.

## 3. Task 2 — flag sweep + conditional re-diff (Mac)

### 3a. Flag enumeration (table first, from `--help` + source)

`--help` receipt (`/tmp/g9-help.log`, exit 0) plus `tools/gs_dump_replayer.cpp`
`:25-58` (help text + `cbs.add` handlers) and the vsync/SSAA consumers in
`gs/`:

| # | flag | consumer | width mechanism (source-read) | tried? |
| --- | --- | --- | --- | --- |
| 1 | `--high-res-scanout` | `VSyncInfo.high_resolution_scanout` | image = mode×2, but forced OFF unless both sampling log2s nonzero (`gs_renderer.cpp:4305-4310`) — i.e. no-op at X1 | yes (R1 alone, R4 +SSAA4) |
| 2 | `--conservative-crtc` | skip_deinterlace + crtc_offsets + overscan (`gs_dump_parser.cpp:173-178`) | overscan base 712 instead of 640; adapt still applies (always on, `:171`) | yes (R2) |
| 3 | `--ssaa <rate>` | `set_super_sampling_rate` (`gs_interface.cpp:68-110`): X1→(0,0), X2→(0,1), X4→(1,1), X8→(1,2) | sampling only; scanout width unchanged unless enabling flag 1's ×2 | yes (R3 @X2, R4 @X4) |
| 4 | `--ssaa-textures` | `super_sampled_textures` (texture LOD only) | none on scanout geometry | no (no mechanism) |
| 5 | `--strided` / `--full` | `DebugMode::draw_mode` (draw-debug rasterizers) | none on CRTC/scanout | no (no mechanism) |
| 6 | `--disable-sampler-feedback` | feedback heuristics | none on scanout geometry | no (no mechanism) |
| 7 | `--iterations <n>` | pass count | none (used =2 for the warmed pass, G8 shape) | n/a |

Predicted reachable widths for THIS dump (MAGH+1=5, clk=4 fixed by regs):
round(base × 4/5)[×2], base ∈ {640, 712} → {512, 570, 1024, 1140}. **597 is
not in the set for any integer MAGH the code paths admit** — no flag can yield
it, because it is not a CRTC width (it is u32(448×4/3)).

### 3b. Sweep (bounded: 4 replays, same dump copy, `--iterations 2`, defaults otherwise)

Each run: exit 0, no `Minimum requirements`, only the 2 known benign ERROR
lines (calibrated-time domain, RenderDoc). Geometry from the G8 hook log line
+ independently from PPM file bytes (all 8 files: dims match hook, sizes =
W×H×3+header). Predictions below were computed by `g9-derive.py` BEFORE the
sweep ran.

| run | flags | predicted | observed FIRST (phase 1) | observed LAST (phase 0) | 597? |
| --- | --- | --- | --- | --- | --- |
| R0 (G8 baseline, ps2x-g8 dir) | defaults | 512×448 | 512×448 progressive | 512×448 progressive | no |
| R1 | `--high-res-scanout` | 512×448 (forced off at X1) | 512×448, internal=mode 512×448 | 512×448 | no |
| R2 | `--conservative-crtc` | 570×240 (round(712×0.8), stays interlaced) | 570×240, interlaced 1 | 570×240 | no |
| R3 | `--ssaa 2` | 512×448 (sampling (0,1), no ×2) | 512×448, internal=mode 512×448 | 512×448 | no |
| R4 | `--ssaa 4 --high-res-scanout` | 1024×896 (mode 512×448, image ×2) | 1024×896, mode 512×448 | 1024×896 | no |

Tried-width set: {512, 570, 1024} (+1140 = conservative+SSAA4+hi-res,
derived-not-run under the bounded stop; the same formula covers it).
**No 597-wide scanout exists in the reachable set — alternative (b).**

### 3c. Conditional re-diff: NOT RUN (no pair)

The brief's re-diff fires only on a matched-geometry pair. None was found, so
per the G8 alignment rule (exact geometry or nothing — the G7 center-crop
proxy stays retired) no pixel comparison runs here: not FIRST-vs-REF at
512-vs-597, not a down/up-resample of either side (that would be a new proxy).
The path to a real pair is the §4 next action, which needs a new capture and
is therefore OUT of this brief by its stop rule.

## 4. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| (i) The 85 px gap is explained by display-state geometry derivable from the dump itself | **SUPPORTED, with refinement**: the gap is fully explained, and the explanation dissolves it — both sides compute the SAME internal scanout (512×448) from the same PrivRegs. The 85 px is PCSX2's screenshot-path 4:3 DAR resample (u32(448×4/3)=597), derived end-to-end from dump bytes + pinned source + run ini, corroborated by the G7 control (640×480). Numbers + method limits only (§2c–2e) |
| (ii) …and a replayer CRTC flag yields a 597-wide scanout | **NOT SUPPORTED on this dump**: sweep widths {512, 570, 1024} (+1140 derived-not-run); 597 is unreachable because it is not a CRTC width. Table only (§3b) |

The ONE next action the numbers justify (adoption input, not an adoption
decision): **capture ONE post-first-draw dump with `ScreenshotSize = 2`
(`InternalResolutionUncorrected`) and the same K=500 draw-aware trigger, then
FIRST-vs-REF diff at exact geometry.** Chain (A) predicts the sidecar PNG at
uncorrected size = `m_real_size` = **512×448** — an exact-geometry pair with
the replayer default scanout that unblocks claim (ii)-style comparison for the
first time. Queued behind it (not this action): the per-vsync scanout series
to localize the first-black frame (G8 gap #1, still OPEN), then a rich
post-loading dump. Rationale: G9 proved the width gap is a screenshot-crop
artifact, not a renderer disagreement — the cheapest unblock is one ini flip,
not a tuning loop.

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.
Odin on-device init/replay REMAINS open (G7 §5 recipe on file, untouched —
capability queries only, per the frontier instruction).

## 5. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (clone HEAD re-verified; shallow) |
| PCSX2 source rev (read-only reads) | `9056c08349cc29ad02a6d1a3a4133259019195af` (== G8 producer rev; nothing copied out) |
| G8 replayer hook | reused as-is, uncommitted in the SSD clone only (file already carries LGPL-3.0+ SPDX; no license change, nothing copied anywhere) |
| G9 scripts | authored this brief (`g9-privregs.py`, `g9-derive.py`, `g9-blobdiff.py`) — mirror carries them as text |
| brew tools | none installed this brief (G8's set reused as-is) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 6. Exact commands

Mac (`/Users/bradrichardson/dev/ps2xGS` unless noted; `COPYFILE_DISABLE=1` on
SSD steps; `VK_ICD_FILENAMES=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json`,
`DYLD_LIBRARY_PATH=/opt/homebrew/lib` on replayer runs):

```text
git -C "/Volumes/Extreme SSD/parallel-gs-g7" rev-parse HEAD   # 3a66c19… (tree unmoved → no rebuild)
python3 /tmp/g9-privregs.py <dump.gs>                          # Task 1 decode (also ran on the G7 .gs control)
python3 /tmp/g9-blobdiff.py <dump.gs>                           # CSR.FIELD-toggle check
python3 /tmp/g9-derive.py <dump.gs> <pngW> <pngH> <ppmW> <ppmH> # chains A+B, G8 + G7 control
cp "<g8-dir>/<dump>.gs" "/Volumes/Extreme SSD/ps2x-g9/"        # working copy (sha-verified)
./tools/parallel-gs-replayer --help                             # flag enumeration receipt
./tools/parallel-gs-replayer "<g9-dump>.gs" --iterations 2 <flags>  # R1..R4 (one replay per flag set)
sha256sum <g8-dir>/*.ppm                                        # G8 pristine re-verify (before + after)
du -sk <ssd dirs> ; df -h / "/Volumes/Extreme SSD"            # allocated + deltas
```

bytesize (each via ONE `ssh bytesize 'wsl …'`, `;` separators, no inline
pipes; READ-ONLY greps/seds — no builds, no boots, no writes):

```text
wsl git -C /home/brad/pcsx2-g7/pcsx2 rev-parse HEAD             # 9056c08349…
wsl sed -n "1243,1330p" …/pcsx2/GS/GSRegs.h                     # GSPrivRegSet layout (== parallel-gs order)
wsl sed -n "333,348p" …/pcsx2/GS/GSRegs.h                       # GSRegDISPLAY bitfields
wsl grep -n -m20 TakeScreenshot …/GS/Renderers/Common/GSRenderer.cpp   # (no hits — path is SaveSnapshotToMemory)
wsl grep -rn -m10 GetInternalResolution …/GS/Renderers/Common/GSRenderer.cpp
wsl sed -n "100,230p" …/GSRenderer.cpp                          # Merge → m_real_size = fs
wsl sed -n "7000,7210p" …/GS/GSState.cpp                        # VideoMode tables + GetResolution
wsl sed -n "7312,7390p" …/GS/GSState.cpp                        # SetRects (displayRect math)
wsl sed -n "975,1090p" …/GSRenderer.cpp                         # PresentCurrentFrame + capture-size use
wsl sed -n "380,560p" …/GSRenderer.cpp                          # CalculateDrawSrcRect (crop) + screenshot writer
wsl sed -n "615,760p" …/GSRenderer.cpp                          # VSync → Merge → snapshot block (header 640×480)
wsl sed -n "760,860p" …/GSRenderer.cpp                          # sidecar PNG block (ScreenshotSize>=Internal)
wsl sed -n "1090,1180p" …/GSRenderer.cpp                        # aspect-resize math (597 + 480 branches)
wsl grep -n "UpscaleMultiplier\|AspectRatio\|Crop\|ScreenshotSize\|PCRTCOffsets\|PCRTCOverscan\|InterlaceMode" …/dat-g8/PCSX2/inis/PCSX2.ini
wsl grep -n -A10 "enum class GSScreenshotSize" …/pcsx2/Config.h  # 0=Window 1=Internal 2=Uncorrected
wsl grep -n -m3 "UpscaleMultiplier" …/pcsx2/Config.h             # default 1.0f (:725)
wsl grep -n -m6 "PCRTCOffsets\|PCRTCOverscan" …/pcsx2/Pcsx2Config.cpp  # defaults false (:716-717)
wsl grep -n -m2 -B3 -A20 "GetVideoMode()" …/GS/GSState.cpp       # CMOD=2 → NTSC (:834-855)
wsl grep -n "VideoModeOffsets\[6\]\|…" …/GS/GSState.cpp          # table lines (:7066/:7075/:7084)
wsl grep -n -m2 "bool GSRenderer::SaveSnapshotToMemory" …/GSRenderer.cpp  # :1061
```

Local experiment diffs: NONE this brief (G8's hook + shim reused untouched).

## 7. Gaps (what this brief could not do)

1. No same-geometry FIRST-vs-REF diff exists yet: no replayer flag yields 597,
   and resampling either side to force a pair would be a new proxy. Needs the
   §4 next action (`ScreenshotSize = 2` capture → predicted 512×448 pair).
2. First-scanout-black cause: still OPEN (G8 gap #1). Rendering runs on vsync#0
   (306 prims per G8) yet that scanout is black; needs the queued per-vsync
   scanout series, not speculation.
3. R2/R4 content not visually inspected: this probe is width-only by brief
   design (no content verdicts needed or made).
4. conservative+SSAA4+hi-res (predicted 1140 wide) derived-not-run under the
   bounded stop (one replay per flag set); covered by the same formula, and
   1140 ≠ 597 in any case.
5. PCSX2 `m_real_size` (512×448) is derived from pinned source + dump bytes,
   not observed in a PCSX2 run (no new capture/rerun allowed). The G7 control
   (640×480) corroborates the formula on independent bytes.
6. Loading-icon content is a light draw load (G8: 544 prims / 32 passes). A
   rich post-loading dump stays queued behind the comparison unblock.
7. No isolated GPU time (host wall only; replayer CLI still has no timestamp
   path — unchanged from G8).
8. Odin on-device project init/replay: still OPEN (G7 §5 recipe on file; no
   change, no claim upgrade).
9. `upstream/` and ps2xGS harness code untouched; ps2xGS `.gscap` captures are
   a different format from PCSX2 `.gs` dumps (no adapter attempted or implied).
10. Session-only `/tmp/g9-*` (~28 KB); external-SSD artifacts (G9 dir ~14.6 MB
    apparent / 28,672 KiB allocated; G8 dir pristine) retained, none committed.

## 8. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…` (+G7 shim, +G8 hook, untouched).
- Build: `/Volumes/Extreme SSD/parallel-gs-g7-build/`, `tools/parallel-gs-replayer` (51,867,352 B, untouched).
- Dump dirs: `/Volumes/Extreme SSD/ps2x-g8/` (pristine, shas re-verified);
  `/Volumes/Extreme SSD/ps2x-g9/` (.gs working copy + 8 sweep PPMs).
- Logs: `/tmp/g9-help.log`, `/tmp/g9-r1-hires.log`,
  `/tmp/g9-r2-conservative.log`, `/tmp/g9-r3-ssaa2.log`,
  `/tmp/g9-r4-ssaa4hires.log` (session-only).
- Tools: `/tmp/g9-privregs.py`, `/tmp/g9-derive.py`, `/tmp/g9-blobdiff.py`
  (mirrored to ssx3; `/tmp` originals are session-only).
- Commits: ps2xGS `[G9]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G9/` `[G9]` + same trailer (NOT pushed).
