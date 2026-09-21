# G22 report — sampler_feedback skip-gate workaround: O4 absent, frames complete, new post-run teardown wall (O5), pixels all-black

Brief: G22 (this turn) — executes G21 §4's WORKAROUND arm (user-authorized;
filing stays open and unfiled per the standing no-upstream order): (1) gate
the `sampler_feedback` dispatch in `GSRenderer::dispatch_texture_analysis`
(`gs_renderer.cpp:3486`) behind an env knob as ONE skip hunk; (2) rebuild
the replayer in a NEW SSD build dir; (3) ONE bounded on-device knob run
(no-O4 + frame completes, or the exact new fate); (4) pixel-diff scanouts
vs G13 oracles to QUANTIFY the behavior change. Tables + hypothesis +
next-action recommendation, no verdicts beyond the hypothesis. Time box
6 h (used ~2 h). Read first per the brief: `local/research/G21/REPORT.md`
(all of it) + G20 §3. No upstream contact of any kind.

Machine: same as G8-G21 (Apple M4, macOS — no new installs).
Device: Odin3 (`622c49b1`, Android 15), transient dir `/data/local/tmp/g22/`
ONLY; removed at end (`mg/` only).

Headline result: the workaround WORKS as a crash fix and FAILS as a
rendering fix — both halves quantified. (1) O4 is ABSENT with proof: exit
134 (SIGABRT, not 139/SIGSEGV), the `7463`/`c61f` crasher is never logged,
9/9 sync compiles succeed, the death stack has ZERO driver frames (all 25
are libc Scudo + replayer teardown), and the run prints `Done!` as its
last logcat line. (2) A NEW wall fires 37 ms AFTER `Done!` (tabled O5):
Scudo `corrupted chunk header` SIGABRT in `main ← Device::~Device ←
PerFrame ← vector<CommandPool>` teardown — writer unnamed, post-run only,
scanouts unaffected. (3) The behavior change is TOTAL, not partial: all 10
device scanouts are pure black (pixsha `9c70d3e9`, mean 0,0,0) — k=1..7
exact/le2/le32 all 0.0000 vs the bright oracles. (4) Finding for adoption:
upstream ALREADY ships `--disable-sampler-feedback` (in HEAD, full-upload
fallback, no dispatch at all) — untested, and the likely-better adoption
shape. Filing stays open regardless.

## 0. Byte caps (declared at session start) vs actuals

| class | cap | actual |
| --- | --- | --- |
| NEW SSD build dir `parallel-gs-g22-android-build` | 6 GB | 4,482,048 KiB (== G18/G20 dirs exactly) PASS |
| NEW SSD `ps2x-g22/` (retrieval: logcat + tombstone + 10 PPMs) | 50 MB | ~7.1 MB apparent; 25,600 KiB allocated (ExFAT 1 MiB clusters) PASS |
| SSD `ps2x-g10..g21` + G14/G18/G20 build dirs (read-only) | 0 growth | all == pre-build snapshot exactly (g10 43008, g14 7168, g18 5120, g19 5120, g20 11264, g21 15360, g14-build 4480000, g18/g20-build 4482048 KiB) PASS |
| SSD clone (source) | ONE hunk only | `gs_renderer.cpp` +15/−0 only; all other files == G20 §2a PASS |
| internal volume (`/`) | <=1 GB delta, no clones/builds | 11 Gi avail before and after; residue `/tmp/g22-*` ~KB text PASS |
| SSD volume | — | 237 -> 233 Gi avail (build dir + receipts; remainder other-lane) |
| device | `/data/local/tmp/g22/` ONLY | pushed 2 files, pulled 12, dir removed after (`mg/` only) PASS |
| network | none used | NDK + cmake + ninja all local; no clones, no installs PASS |
| host build | `-j2` | configure exit 0 + full build exit 0 + 1-TU recompile exit 0 PASS |
| committed to git | text only | REPORT.md + `g22-workaround.diff` + `g22-tu-rebuild.log`; no binaries PASS |

No P-lane lease, no bytesize/WSL. `COPYFILE_DISABLE=1` on all SSD steps.
No code copied into any project tree.

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | Gating the `sampler_feedback` dispatch behind `PGS_SKIP_SAMPLER_FEEDBACK` removes O4 (no sync crash at `dispatch_texture_analysis`) and the run completes frames; the rendering-behavior change is quantified by pixel-diff vs G13 oracles |
| observable signal | hunk diff + gate-semantics table + build exit/sha/build-id + ONE bounded knob run (exit/wall/frames/fate) + knob-fired LOGI + crasher-hash absence + tombstone triage + `g14-diff.py` score |
| alternatives | (a) no O4 + frames complete => adoption input with quantified diff; (b) O4 persists => next attempt; (c) new wall => its exact brief; (d) O1/O3 instead of first draw => lottery note + ONE retry |
| stop condition | ONE hunk max; TWO bounded device runs max (knob run + one retry iff O1/O3 fires instead of first draw); no tuning loop, no second workaround shape, no new dumps; no lldb unless the tombstone cannot triage |
| outcome → next action | numbers name the next single experiment (§4) |

Outcome: alternatives (a)+(c) — no O4 + FULL completion (all 8 vsyncs ×
2 passes + `Done!`) AND a new post-run teardown wall (O5). No tuning loop
was entered: one hunk, one build, one device run. Retry not used (O1/O3
never fired); lldb not used (tombstone classifies; tabled §3d).

## 2. Task 1 — workaround hunk + rebuild (no device until §3)

### 2a. Pin verification (pre-work — G20 §2a reproduced + G21 corrections)

| item | observed |
| --- | --- |
| SSD clone HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (== pin) |
| tree status (pre-hunk) | `M CMakeLists.txt` (S1) + `m Granite` (S2) + `M gs/gs_interface.cpp` (G11) + `M tools/CMakeLists.txt` (S3) + `M tools/gs_dump_replayer.cpp` (G8+G10) + ExFAT `._` sidecars — G20 §2a exactly |
| Granite rev | `16e7395f6a4858c1783dbf6f521f90b9d5f82ac5` (== pin); diff pre-hunk: S2 (`platforms/CMakeLists.txt`) + G7 (`util/timer.cpp`) + G20 (`command_buffer.cpp` +18) + G18 (`shader.cpp`) |
| G18 hunk bytes | `git -C Granite diff vulkan/shader.cpp` byte-identical to `g18-fix.diff` (`diff` exit 0, HUNK_MATCH) — verified BEFORE writing the workaround hunk |
| G20 capture hunk | STILL PRESENT in tree (`command_buffer.cpp` +18, G20 diff text verbatim) => compiled into the G22 binary. Tabled, NOT removed (brief: no other clone edits). Logging-only per G20 §2b proof; gives crash-site attribution if O4 had persisted |
| G14 shims S1–S3 | marker `G14 local experiment shim` ×1 in each of `CMakeLists.txt`, `tools/CMakeLists.txt`, `Granite/application/platforms/CMakeLists.txt` |
| G11 / G8+G10 | `G11:` markers (`gs_interface.cpp:302-311,3915-3938,4303-4305`) + `G8`/`G10` markers (`gs_dump_replayer.cpp:113,118`) present |
| rich dump (host) | 11,537,377 B, sha `154d9d8577a210fb794b29a048ee2cf08053f87fe220bb2bf975933a02ad7e32` full-match |
| G14 binary (baseline) | 265,837,472 B, sha `c4cd63b4859ee72480abce6790904f41aa95374eaec3bd39395bb94a42d79db7` full-match (zero-fill watch: intact) |
| G22 binary (post-build re-sha) | sha `5e1f7827…` at build, pre-push, AND report time — all three match (no zero-fill recurrence; cf. G21 correction A) |
| knob precedent | `PGS_SKIP_COMPILATION_TASKS` at `gs_renderer.cpp:459` (`getenv` + `strcmp(env,"1")==0` + LOGI + early return) — the hunk mirrors this shape |
| NDK / cmake | r30 (`/opt/homebrew/share/android-ndk`); ninja `/opt/homebrew/bin/ninja` |
| device (read-only pre-check) | `622c49b1`, Odin3; `/data/local/tmp/` == `mg/` only; newest tombstone `_14` (G20's); `/data` 29 G free |

### 2b. Gate design (what the dispatch does, what skipping leaves unset)

Static reads (all line refs = SSD clone at pin):

| slot | fact |
| --- | --- |
| gate point | `GSRenderer::dispatch_texture_analysis`, `gs/gs_renderer.cpp:3486` — the ONLY `set_program(shaders.sampler_feedback)` site (`grep`: :3488 only); single caller `flush_rendering` :3242, guarded by `!texture_analysis.empty()` (:3238) |
| what the dispatch writes | per-texture indirect-upload state in `device_scratch`: `indirect_dispatch` (dispatch[3] args), `indirect_bitmask` (dedup), `indirect_workgroups` (block list) — via atomics over physical pointers (`sampler_feedback.comp:66-86`); the ONLY populator (sole writer besides the zeroing clears) |
| what runs regardless | `qword_clear` clears of all three buffers are queued at alloc (`:1453/:1459/:1467`) and flushed on `clear_cmd` (`flush_qword_clears`, :1089) — INDEPENDENT of the skipped dispatch, so args stay at cleared zeros |
| the consumer | `upload_texture` :3644-3645: `if (upload.indirection.buffer) cmd.dispatch_indirect(*indirect, offset)` — reads the 12 arg bytes from a valid buffer; zeros => ZERO workgroups launch => upload shader never runs for that texture |
| host-side lifecycle | UNCHANGED: `texture_analysis.clear()` stays at the call site (:3245); the hunk early-returns inside the callee. No host readback of the feedback buffers exists (`check_bug_feedback` is `#if 0`'d, :1228-1241) |
| why no corruption | fewer GPU writers (clear→consume, populate removed — no new hazard); zero-arg indirect dispatch is Vulkan-valid; consumer reads only valid-range zeros; later draws sample in-bounds images whose content is the quantified delta. No host write added/removed (one static bool init) |
| resulting behavior | sparse-uploaded texels missing: sampled content is whatever the image held (fresh `UNDEFINED`-layout images, :1379 — observed §3e as black). Rendering delta only, no crash class |

Defer/emulate rejection (brief-mandated, tabled not relitigated):

| shape | verdict | reason |
| --- | --- | --- |
| defer (compile/move the dispatch later) | REJECTED | moves the crash, does not remove it: the compile still names `sampler_feedback` on the same driver |
| host-emulation of the shader | REJECTED | out of scope (full GPU→CPU port of the feedback algorithm) |
| SKIP (chosen) | IMPLEMENTED | removes the crashing compile entirely; behavior change quantified §3e |

Upstream-flag finding (observed during design, NOT acted on — second
shape forbidden by the stop rule; input to §4): upstream ALREADY ships a
full-upload fallback — replayer `--disable-sampler-feedback`
(`tools/gs_dump_replayer.cpp:55`, in HEAD — not a G-hunk; usage line :33),
`debug_mode.disable_sampler_feedback` (`gs_interface.hpp:143`, default
false) => `commit_cached_texture(..., false)` => NO indirection alloc
(`:1478-1482` skipped) => `upload_texture` takes the direct full-dispatch
branch (`:3647`). Untouched this brief; validation is the §4 action.

The hunk (`git diff gs/gs_renderer.cpp`, +15/−0, also saved as
`g22-workaround.diff` beside this report):

```diff
@@ -3485,6 +3485,21 @@ uint32_t GSRenderer::update_palette_cache(const PaletteUploadDescriptor &desc)
 
 void GSRenderer::dispatch_texture_analysis(Vulkan::CommandBuffer &cmd, const RenderPass &rp)
 {
+	// G22 local workaround (not upstream): skip the sampler_feedback
+	// dispatch entirely when PGS_SKIP_SAMPLER_FEEDBACK=1. The per-texture
+	// indirect-dispatch args stay at their qword-cleared zeros, so the
+	// consuming upload dispatches launch zero workgroups: in-bounds, no
+	// new hazards, rendering delta only (sparse-uploaded texels missing).
+	static bool skip_sampler_feedback = [] {
+		const char *env = getenv("PGS_SKIP_SAMPLER_FEEDBACK");
+		bool skip = env && strcmp(env, "1") == 0;
+		if (skip)
+			LOGI("G22: skipping sampler_feedback dispatch (workaround).\n");
+		return skip;
+	}();
+	if (skip_sampler_feedback)
+		return;
+
 	cmd.set_program(shaders.sampler_feedback);
```

Gate semantics: knob `PGS_SKIP_SAMPLER_FEEDBACK=1` (same `getenv`+`strcmp`
idiom as `PGS_SKIP_COMPILATION_TASKS`, :459-460); `getenv`/`strcmp`/`LOGI`
all pre-used in this TU (no new includes); static-cached => exactly ONE
knob-fired receipt line per process (no per-flush spam); unset/≠1 =>
byte-identical behavior to unpatched (falls through to `set_program`).
Post-hunk tree status: G20 §2a files + `M gs/gs_renderer.cpp`, nothing else.

### 2c. Build record (NEW SSD dir — G14/G18/G20 dirs untouched)

| item | observed |
| --- | --- |
| configure | G14 recipe verbatim into `parallel-gs-g22-android-build` → exit 0, `Configuring done (13.4s)`, `Generating done (8.8s)`, `Processor: aarch64` |
| build | `cmake --build … --target parallel-gs-replayer -j2` → exit 0, `[458/458] Linking CXX executable tools/parallel-gs-replayer` |
| warnings | full build: 2 pre-existing classes only (`gs_dump_parser.hpp:49` shadow + `command_buffer.cpp:375` unused, same as G20); hunk TU (`gs_renderer.cpp.o`) recompiled solo to capture output => exit 0, ZERO warnings/errors (`g22-tu-rebuild.log`, `[3/3]`) |
| binary | `tools/parallel-gs-replayer`, 265,840,344 B (+1,776 vs G20 — hunk + strings), valid ELF (`7f45 4c46`) |
| sha | `5e1f782735234fa9a938ebde51f815cb99e7e8a234555f868fb6cc049ae717e8` (build-time; re-sha pre-push + report-time both match) |
| build-id | `c2dc902b48d04b8406707c7799f0477b7695294a` (NDK `llvm-readelf --notes`) |
| workaround presence | `strings` grep: `PGS_SKIP_SAMPLER_FEEDBACK` ×1, `G22: skipping` ×1 |
| build dir | 4,482,048 KiB (cap 6 GB ✓); G14/G18/G20 dirs == pre-run exactly (0 growth ✓) |

## 3. Task 2 — ONE bounded knob run + score (Odin)

### 3a. Run table (ONE run — retry not used, §3d)

| item | observed |
| --- | --- |
| staging | `/data/local/tmp/g22/` ONLY; dump + G22 binary pushed; on-device shas FULL-match host (`154d9d85…` / `5e1f7827…`); logcat cleared, Granite empty before |
| command | `PGS_SKIP_COMPILATION_TASKS=1 PGS_SKIP_SAMPLER_FEEDBACK=1 timeout -s KILL 280 ./parallel-gs-replayer /data/local/tmp/g22/g13-dump.gs --iterations 2` (G19/G20 shape + the new knob; the compilation-tasks knob is kept so the async path cannot compile `sampler_feedback` independently of the dispatch gate and confound the experiment — tabled choice) |
| WITHOUT-knob run | NOT done per the brief (G18/G19/G20 already prove the crash) |
| exit / wall | **134** (SIGABRT — NOT 139) / ~1 s wall (`date` 1789999883→1789999884; logcat 10:11:23.658→10:11:24.240; tombstone `Process uptime: 2s`) |
| knob-fired receipts | line 25 `Skipping precompilation of shaders…` (×1) + line 48 `G22: skipping sampler_feedback dispatch (workaround).` (×1, static-cached — both knobs effective) |
| logcat | 1810 lines (vs 48 in G20): init + ext list + 18 `Running frame` + 16 G10 pass lines (pass 0+1 × vsync #0–#7 — BOTH iterations complete) + 9 `G20 pre-create` compute groups + 9 `Stalled compile (…success: yes)` + 10 G8 `wrote … scanout` + `Done!` as the LAST line (1810/1810, 10:11:24.240); 0 `success: no`; 0 `Failed to create descriptor update template` (G18 fix holds over the full replay) |
| crasher absence | `7463dfd379df2855` / `c61f1a8116a82d4b` refs in logcat: **0** (the crashing compile never happens; post-`b710…` the run continues to the next compile instead of dying) |
| compile order check | `502d…` success (:38) → `b710…` success (:47) → G22 skip fires (:48) → `a0d6…` success (:55) — G20's death point is now a continue point |
| progress vs G20 | identical shape through the second stalled compile, then past the old crash into full completion: 8 vsyncs × 2 passes, all scanouts, `Done!` |
| fate | NOT O1 (init null-`table` SIGSEGV — this completes the run); NOT O2 (no `0xf0` fault); NOT O3 (O3 = SIGABRT @ `trim():173` in init — this is SIGABRT in post-run `Device` teardown, §3c); NOT O4 (proof §3b) → NEW signature, tabled **O5** (§3c) |
| device outputs | 10 scanouts, 688,143 B each (`g10-vsync0..7` + `g8-first` + `g8-last`); tombstone_15 written 10:11 (pulled, §3b–§3c); device dir removed after (`mg/` only ✓) |

### 3b. O4-absent proof (four independent legs)

| # | leg | evidence |
| --- | --- | --- |
| 1 | exit/signal differs | 134/SIGABRT vs O4's 139/SIGSEGV; `Done!` printed before death |
| 2 | crashing input never exists | crasher pipeline/shader hashes logged 0×; 9/9 pre-creates each paired with a `success: yes` post (G20 decode rule: no dangling pre-create) |
| 3 | death stack has zero driver frames | tombstone_15: all 25 frames are `libc.so` (Scudo `reportHeaderCorruption` ← `deallocate`) + replayer teardown (`main ← Device::~Device ← vector<PerFrame> ← PerFrame::~ ← vector<CommandPool> ← operator delete`); `libllvm-qgl.so` appears 4× ONLY in the memory-maps section (same BuildId `28a2407f…`, mapped-not-executing); `vkCreateComputePipelines` / `kick_compilation` / `__async_func`: 0× anywhere |
| 4 | death is post-run | tombstone timestamp 10:11:24.277 = 37 ms AFTER the last logcat line (`Done!`, .240); crashing thread is main in teardown, binary BuildId `c2dc902b…` == G22 binary |

O4 is absent. The skip removes the only path that compiled
`sampler_feedback` on the sync thread, and the compilation-tasks knob keeps
the async path from compiling it independently (0 async mentions — same
proof shape as G19/G20).

### 3c. Tombstone_15 triage (pid 18830 — NEW wall, tabled O5)

| slot | fact |
| --- | --- |
| signal | SIGABRT (signal 6, SI_QUEUE), Scudo abort message: `corrupted chunk header at address 0x200007f59a19ef0` |
| stack | `main+4532 ← Device::~Device()+712 ← vector<unique_ptr<PerFrame>>::~ ← PerFrame::~PerFrame()+452 ← vector<CommandPool>::~ ← __libcpp_operator_delete ← Scudo deallocate → reportHeaderCorruption → abort` (25 frames, main thread; #00–#05 libc, #06–#23 replayer BuildId `c2dc902b…`, #24 `__libc_init`) |
| timing | post-`Done!` teardown only — the replay loop, both iterations, all scanout writes, and `Done!` complete BEFORE the abort; scanouts provably unaffected |
| vs O1/O2/O3 | NOT O1 (init SIGSEGV `0x380`); NOT O2 (`0xf0`); NOT O3 (SIGABRT @ init `trim():173` — different site, different phase); first wall of its shape => new number O5 (proposed; orchestrator confirms) |
| writer | UNNAMED — heap-header corruption is detected at teardown `free`, arbitrarily far from the overflowing write; naming needs a future instrumented brief (ASan/HWASan per G15/G16 precedent). NOT named here |
| workaround-causation | NONE plausible: the skip REMOVES a GPU writer and adds/removes no host write (one static bool); host vector lifecycle unchanged (§2b). O5 is the next wall exposed by progress — this is the FIRST run on this path ever to reach teardown (G18–G20 all died in first draw). Same reasoning shape as G18 §3b |
| binary identity | tombstone BuildId `c2dc902b…` == G22 binary => the tested binary is proven |

### 3d. Retry + lldb decisions (tabled, both NO)

| decision | verdict | rationale |
| --- | --- | --- |
| ONE retry | NOT USED | Brief permits it iff "O1/O3 fires instead of first draw" — neither fired; the run reached first draw, completed both iterations, and yielded two classifiable signals (O4-absent + named O5) |
| lldb triage | NOT USED | Tombstone_15 fully classifies the teardown SIGABRT (signal/message/full stack/thread census/timing vs `Done!`); the open question (corruption WRITER) is a future instrumented brief, not answerable by post-mortem lldb within this one |

### 3e. Score vs G13 oracles (`g14-diff.py`, zero new tooling)

`g14-diff.py ps2x-g13 ps2x-g22`: oracles **ALL OK** (8/8 pixel-shas);
device PPMs: **10** (all 688,143 B, 512x448).

| k | exact | le2 | le32 | psnrR | psnrG | psnrB |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0.0000 | 0.0000 | 0.0000 | 1.9 | 2.4 | 33.1 |
| 2 | 0.0000 | 0.0000 | 0.0000 | 1.1 | 1.5 | 32.3 |
| 3 | 0.0000 | 0.0000 | 0.0000 | 0.4 | 0.8 | 31.2 |
| 4 | 0.0000 | 0.0000 | 0.0000 | 0.5 | 1.2 | 7.7 |
| 5 | 0.0000 | 0.0000 | 0.0000 | 0.5 | 1.2 | 7.7 |
| 6 | 0.0000 | 0.0000 | 0.0000 | 0.3 | 2.3 | 12.8 |
| 7 | 0.0000 | 0.0000 | 0.0000 | 0.3 | 2.3 | 12.8 |

Characterization (pixel-content census, same `pixsha` definition as
`g14-diff.py`): ALL 10 device scanouts are pure black — pixsha `9c70d3e9`
(== oracle PPM#0's all-black sha), channel means (0.0, 0.0, 0.0) — while
oracle frames k=1..7 are bright (means R 203–245, G 182–228). So the
behavior change is TOTAL: every sampled sparse-uploaded texel is missing
(zero-arg indirect => zero upload workgroups, §2b; fresh images read
black). vsync0 (oracle black) matches trivially but is unscored by the
tool (k=1..7 only — reported as observed, no rescoring). SCORE tabled as
above: the skip is a complete causation proof and a degenerate rendering
fix — adoption input in §4.

## 4. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| Gating the `sampler_feedback` dispatch behind `PGS_SKIP_SAMPLER_FEEDBACK` removes O4 and the run completes frames; the rendering-behavior change is quantified by pixel-diff vs G13 oracles | **CONFIRMED on all three**: O4 absent (4-leg proof §3b: 134≠139, crasher never logged, 0 driver frames, death 37 ms post-`Done!`) AND full completion (8 vsyncs × 2 passes, 10 scanouts, `Done!` last line) AND the behavior change quantified (all-black scanouts, k=1..7 all-0.0000 exact/le2/le32). Companion finding: new post-run teardown wall O5 (Scudo header corruption, writer unnamed, scanouts unaffected). |

The ONE next action the numbers justify: **validate the UPSTREAM
`--disable-sampler-feedback` flag on-device (single bounded run, NO
rebuild — the G22 binary already ships it) + pixel-diff — it decides the
adoption shape**. Rationale: the skip-shape proves causation but renders
black, so it is not adoptable as a rendering workaround; the upstream
flag (full-upload fallback, no `sampler_feedback` dispatch at all, §2b)
predicts no-O4 AND correct pixels on the same dump/iterations — one run
confirms or refutes both halves, with O5 expected to re-fire harmlessly
post-`Done!` either way. Queued behind it (not this action): O5
writer-naming brief (ASan/HWASan per G15/G16); G18-hunk fix adoption
(still queued — now behind O5 on the device path); O1/O3 writer naming
(still open); the Adreno filing — STILL OPEN regardless (submit needs
user identity/tracker; the workaround does not close it).

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.

## 5. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c19…` (LGPL-3.0+ per G7 §4 — cited, not re-read) |
| Granite rev | `16e7395f…` (MIT per G7 §4 — cited); G20 capture hunk compiled in (logging-only, uncommitted, SSD clone only) |
| G14 shims S1–S3 + G7/G8/G10/G11/G18 hunks | untouched, still uncommitted in SSD clone only (G18 HUNK_MATCH re-verified pre-hunk) |
| G22 addition | ONE hunk (+15/−0, `gs/gs_renderer.cpp`), uncommitted in SSD clone only; diff committed as text (`g22-workaround.diff`) beside this report |
| upstream `--disable-sampler-feedback` | observed in HEAD (cited file/line); not used, not modified |
| NDK r30 / cmake / ninja | local toolchain use only (Apache-2.0 / BSD) |
| logcat/tombstone/scanouts | run receipts of our own binary in SSD `ps2x-g22/` ONLY (not in git); no PII (`uid: 2000` shell) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 6. Exact commands

Mac (`/Users/bradrichardson/dev/ssx3` unless noted;
`COPYFILE_DISABLE=1` on SSD steps; `SSD="/Volumes/Extreme SSD"`):

```text
git -C $SSD/parallel-gs-g7 rev-parse HEAD ; status --short   # §2a
git -C $SSD/parallel-gs-g7/Granite log -1 --format=%H ; diff --stat  # §2a
git -C $SSD/parallel-gs-g7/Granite diff vulkan/shader.cpp | diff local/research/G18/g18-fix.diff -  # HUNK_MATCH
shasum -a 256 $SSD/ps2x-g13/g13-dump.gs <g14-binary>         # §2a pins
grep -n sampler_feedback $SSD/parallel-gs-g7/gs/gs_renderer.cpp ...  # §2b design
# (write the ONE hunk at gs_renderer.cpp:3486; diff > /tmp/g22-workaround.diff)
cmake -S <clone> -B $SSD/parallel-gs-g22-android-build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-35  # §2c exit 0
cmake --build <g22-build> --target parallel-gs-replayer -j2  # §2c exit 0 [458/458]
rm <g22-build>/gs/CMakeFiles/parallel-gs.dir/gs_renderer.cpp.o ; cmake --build ...  # TU recompile exit 0, 0 warnings
shasum -a 256 <g22-binary> ; llvm-readelf --notes <g22-binary>  # 5e1f7827… + c2dc902b…
strings <g22-binary> | grep -c PGS_SKIP_SAMPLER_FEEDBACK      # 1
du -sk <dirs> ; df -h / $SSD                                 # §0 (0 growth)
python3 local/research/G14/g14-diff.py $SSD/ps2x-g13 $SSD/ps2x-g22  # §3e
python3 -c <pixsha+means census over both PPM sets>          # §3e black proof
```

adb (`-s 622c49b1` throughout; `/data/local/tmp/g22/` ONLY; `mg/` never touched):

```text
shell 'ls /data/local/tmp/ ; ls -lt /data/tombstones/ | head'  # pre-check (mg/ only, _14 newest)
shell 'rm -rf /data/local/tmp/g22 && mkdir -p /data/local/tmp/g22'
push <dump> $G22DIR/g13-dump.gs ; push <g22-binary> $G22DIR/parallel-gs-replayer
shell 'sha256sum g13-dump.gs parallel-gs-replayer'             # both FULL-match host
logcat -c ; logcat -d -s Granite:V | tail -2                   # before (empty)
shell 'cd $G22DIR && date +%s; PGS_SKIP_COMPILATION_TASKS=1 PGS_SKIP_SAMPLER_FEEDBACK=1 timeout -s KILL 280 ./parallel-gs-replayer $G22DIR/g13-dump.gs --iterations 2; echo RUN_EXIT=$?; date +%s'  # 134
logcat -d -s Granite:V > $SSD/ps2x-g22/g22-logcat.txt          # 1810 lines
shell 'ls -la $G22DIR/ ; ls -lt /data/tombstones/ | head -6'   # 10 scanouts; _15 new 10:11
pull /data/tombstones/tombstone_15 $SSD/ps2x-g22/g22-tombstone-15.txt
pull $G22DIR/*.ppm $SSD/ps2x-g22/                              # 10 × 688,143 B
shell 'rm -rf /data/local/tmp/g22 && ls /data/local/tmp/'      # DEVICE_CLEAN (mg/ only)
```

## 7. Gaps (what this brief could not do)

1. O5's heap-corruption WRITER is unnamed (detection-at-teardown only;
   needs an instrumented brief — §4 queue).
2. The upstream `--disable-sampler-feedback` flag is UNTESTED (observed in
   HEAD only; validation is the §4 action — no run, no rebuild spent here).
3. The skip-shape renders all-black: adoptable as causation proof only,
   not as a rendering workaround (quantified §3e, not fixed — no second
   shape allowed).
4. The Adreno filing is still open and unfiled (needs user identity /
   tracker — unchanged owner; the workaround does not close it).
5. O1/O3 writers, G18-hunk adoption, G17 filing: unchanged / queued.
6. No lldb (decision tabled §3d); OS tombstone store otherwise untouched.
   `upstream/` + harness code untouched; no new dumps.

## 8. Receipt paths

- SSD receipts: `/Volumes/Extreme SSD/ps2x-g22/` (12 files: logcat 1810
  lines, tombstone_15, 10 PPMs × 688,143 B) + `parallel-gs-g22-android-build/`
  (binary 265,840,344 B `5e1f7827…` BuildID `c2dc902b…`).
- SSD receipts (read-only, 0 growth): `ps2x-g10/`-`ps2x-g21/` + G14/G18/G20
  build dirs + SSD clone (HEAD `3a66c19…`, Granite `16e7395…`, G22 hunk
  uncommitted in `gs_renderer.cpp`).
- Session-only: `/tmp/g22-*` (hunk diff, TU rebuild log).
- Commits: ps2xGS `[G22]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G22/` `[G22]` + same trailer (NOT pushed).

TAIL-RECEIPT: G22 report ends here. Skip-gate works (O4 absent,
frames complete, Done!), new post-run wall O5 named, pixels all-black
(quantified), upstream-flag validation next, filing still open.
