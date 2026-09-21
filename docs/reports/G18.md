# G18 report — static cause NAMED (heap/init path: raw-vs-derived descriptor-buffer flag divergence at `shader.cpp:531`), one-hunk fix, re-run PASSES O2's site then dies on a NEW wall (O4: Adreno compiler SIGSEGV fault-0x0 in `vkCreateComputePipelines` on async worker)

Brief: G18 (this turn) — executes G17's §3: (1) STATICALLY name why
`update_template[0]` is null for this compute pipeline's layout (mask-skip
vs heap/init path vs runtime-nulled slot; creation-failed already eliminated
by G17's 0-line LOGE census); (2) add the MINIMAL guard/handling the cause
justifies (one hunk); (3) rebuild + bounded on-device re-run (`--iterations
2`) + score. Tables + hypothesis + next-action recommendation, no verdicts
beyond the hypothesis. Time box 6 h (used ~1.0 h). Read first per the brief:
`local/research/G17/REPORT.md` (all: O2's bad input NAMED —
`update_template == nullptr` for set 0 with `vk_set` valid at
`flush_descriptor_set():3505`, fault `0xf0` at driver `ldr w8, [x2, #0xf0]`;
`:3504 VK_ASSERT` compiled out; three candidates open). No new dumps (kept).

Machine: same as G8–G17 (Apple M4, macOS 27.0 — no new installs) +
Odin3 (`622c49b1`, Android 15). Pins: paraLLEl-GS
`3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` + G14 S1–S3 shims + G11/G8+G10
hunks (re-verified §2a; the ONLY delta is the G18 fix hunk, §2d) / rich dump
sha `154d9d85…` (re-verified host-side AND on-device) / G14 binary
`c4cd63b4…` (pre-fix baseline — G14 build dir untouched; the fix rebuilds
from the same tree + fix hunk in a NEW dir).

Headline result: hypothesis SUPPORTED with a new wall exposed. Static cause
NAMED (§2b–§2c): heap/init path — `PipelineLayout::init_legacy` gates
`create_update_templates()` on the RAW
`descriptor_buffer_features.descriptorBuffer` bit (`shader.cpp:531`), while
`CommandBuffer` selects the descriptor-buffer flush path on the DERIVED
`supports_descriptor_buffer` flag (`command_buffer.cpp:108`, suitability
conditions at `context.cpp:2279-2284`). On Odin3's Adreno 830 the raw bit is
true (`VK_EXT_descriptor_buffer` enabled, all logcats) but the derived flag
is false (Generic cmd buffer took the legacy flush path — O2 reached `:3505`
in G17), so NO templates were ever created for ANY set of the layout, and set
0's zero-init null slot (`shader.hpp:259`) reached the driver. Mask-skip
ELIMINATED (dispatch loop proves set 0 ∈ mask, §2b); runtime-nulled
ELIMINATED (no writers besides init/create/dtor, §2b). The one-hunk fix
mirrors the derived-flag gate into `shader.cpp:531` (create-the-template
shape — the cause decides; skip-the-call and assert-and-early-out tabled and
rejected, §2d). Rebuild exit 0 (G18 binary `a62d8a2b…`). Bounded re-run:
exit 139, reached first draw, PASSED O2's exact site (main thread progressed
past the first-draw texture-upload `dispatch_indirect` — O2's 4/4 death site
— to `dispatch_triangle_setup`), then died on a NEW signature (tabled O4, NOT
O1/O2/O3): SIGSEGV fault `0x0` in `libllvm-qgl.so` (Adreno compiler) inside
`vkCreateComputePipelines`, on async worker tid 20753 (`kick_compilation_tasks`),
with main tid 20749 concurrently inside the same driver call. Score: oracles
ALL OK, device scanouts 0 → SCORE N/A. Retry NOT used (condition "O1/O3
instead of first draw" not met — run reached first draw and passed O2;
tabled §2f). lldb NOT used (tombstone fully classifies; tabled §2f).

## 0. Byte caps (declared at session start) vs actuals

| class | cap | actual |
| --- | --- | --- |
| NEW SSD build dir `parallel-gs-g18-android-build` | 6 GB | 4,482,048 KiB (~4.3 GiB) ✓ |
| SSD ps2x-g18 retrieval (logcat + tombstone_12) | 100 MB | ~236 KB apparent; 5,120 KiB allocated (ExFAT 1 MiB clusters) ✓ |
| SSD ps2x-g13…g17 (read-only) | 0 growth | 35,840 / 7,168 / 31,744 / 43,008 / 17,408 KiB == G17-end exactly ✓ |
| SSD mac / G14 / HWASan / ASan build dirs (read-only) | 0 growth | G14 == 4,480,000 KiB exactly ✓ (others not re-read; untouched) |
| SSD clone (source) | 0 + fix hunk only | HEAD + status == G17 §2a + fix hunk in `Granite/vulkan/shader.cpp` ONLY (§2a/§2d) ✓ |
| internal volume (`/`) | ≤1 GB delta, no clones/builds | 4.3 Gi avail at start → 4.2 Gi at end (other-lane noise); my residue = `/tmp/g18-*.log` 68 KB ✓ |
| device `/data/local/tmp/g18/` (transient) | 600 MB peak | peak ~278 MB (266 + 11.5); 0 remaining — dir removed, `mg/` only ✓ |
| `/tmp/g18-*` host logs | session-only | configure 8 KB + build 60 KB ✓ |
| committed to git | text only (report + fix diff) | this file + `g18-fix.diff` (ssx3 mirror); report only (ps2xGS); no captures, no binaries, no build dirs ✓ |

SSD 281 → 263 Gi avail: this brief wrote ~4.3 GB (new build dir) +
~5 MB allocated (retrieval); the remainder is other-lane. No code copied
into any GPL tree. No P-lane contention: no recomp boots/builds, no P-lane
lease (no fork writes at all), no bytesize/WSL use. Host build `-j2` (kept).
Tombstones: pulled ONLY `tombstone_12` (this run's, §2f); OS store otherwise
untouched. Note: tombstone slots ROTATE (new crash took `_12`, not `_32`) —
G17's "max _31" readings remain valid for debugger-held runs (none written),
but max-slot is not a new-crash oracle; `ls -lt` is (§2f).

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | `update_template[0]` is null via a statically nameable cause (one of mask-skip / heap-init-path / runtime-nulled); the minimal cause-justified fix lets the bounded re-run pass O2's site |
| observable signal | cause table with file/line receipts + one-hunk diff + rebuild exit/sha + re-run exit/wall/frames/fate + tombstone triage + diff score |
| alternatives | (a) cause named + fix passes O2's site → adoption input + next wall named; (b) O2 persists at `:3505` → next naming/fix attempt; (c) O1/O3 fires instead of first draw → lottery note + retry accounting |
| stop condition | ONE hunk max; if static reads cannot decide among the three candidates, STOP with ambiguity + recipe and NO fix; TWO bounded device runs max (re-run + one retry iff O1/O3 fires instead of first draw); no lldb unless the re-run needs triage |
| outcome → next action | numbers name the next single experiment (§3) |

Outcome: alternative (a) — cause NAMED (heap/init path, §2b–§2c), fix passes
O2's exact site, run exposes a NEW wall (O4, §2f) with a no-rebuild
discriminator queued (§3). No tuning loop was entered: one hunk, one build,
one device run.

## 2. Task 1 — static cause + minimal fix + rebuild (no device yet)

### 2a. Pin verification (pre-work — G17 §2a reproduced + fix-hunk delta)

| item | observed |
| --- | --- |
| SSD clone HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (== pin) |
| tree status (pre-fix) | `M CMakeLists.txt` (S1) + `m Granite` (S2) + `M tools/CMakeLists.txt` (S3) + `M gs/gs_interface.cpp` (G11) + `M tools/gs_dump_replayer.cpp` (G8+G10) + ExFAT `._` sidecars — G17 §2a exactly |
| tree status (post-fix) | SAME + fix hunk in `Granite/vulkan/shader.cpp` ONLY (Granite diff: `application/platforms/CMakeLists.txt` S2 + `util/timer.cpp` G7 + `vulkan/shader.cpp` G18 — §2d) |
| Granite rev | `16e7395f6a48` (== pin; fix is uncommitted, same standing as G14 shims) |
| rich dump (host) | 11,537,377 B, sha `154d9d8577a210fb794b29a048ee2cf08053f87fe220bb2bf975933a02ad7e32` full-match (device re-verified §2e) |
| G14 binary (baseline) | 265,837,472 B, sha `c4cd63b4859ee72480abce6790904f41aa95374eaec3bd39395bb94a42d79db7` (G14 build dir untouched) |
| device | `622c49b1`, Odin3, Android 15; `/data` 28 G free; `/data/local/tmp/` == `mg/` only at start; newest tombstone pre-run `_11` 04:19 (rotation noted §0) |
| driver | Adreno (TM) 830, API 1.3.284, Driver 512.800.58 (logcat §2f; == G14–G17) |
| OS oracle `cmd gpu vkjson` | `VK_EXT_descriptor_buffer` PRESENT; `VK_EXT_descriptor_heap` count 0 (heap variant eliminated at the OS level too) |
| NDK / cmake | r30 `30.0.16248370` (re-confirmed); G14 recipe reused verbatim (§2e) |

### 2b. Static reads — the three candidates (all reads read-only, pre-fix)

| # | site | receipt |
| --- | --- | --- |
| 1 | O2 call site | `command_buffer.cpp:3505` IS `table.vkUpdateDescriptorSetWithTemplate(device->get_device(), vk_set, update_template, bindings.bindings[set]);` (re-grepped; == G17 §2b) |
| 2 | dispatch loop | `flush_descriptor_sets()` computes `set_update_mask = layout.descriptor_set_mask & dirty_sets_rebind` (`:3576`) and calls `flush_descriptor_set(set, …)` only `for_each_bit(set_update_mask)` (`:3623–3629`), in the `else` (legacy) branch (`:3605`) taken iff `desc_heap_enable == false && desc_buffer_enable == false` |
| 3 | bindless early-out | `flush_descriptor_set` returns BEFORE `:3505` for bindless sets (`:3489–3494`) |
| 4 | template creation loop | `create_update_templates()` (`shader.cpp:547`) covers every set ∈ `descriptor_set_mask` except bindless (`:551–554`); failure only LOGE'd (`:743–746`) |
| 5 | template slot init | `update_template[VULKAN_NUM_DESCRIPTOR_SETS] = {}` (`shader.hpp:259`) — null until created |
| 6 | creation gate | `init_legacy` calls `create_update_templates()` ONLY `if (!descriptor_buffer_features.descriptorBuffer)` — the RAW bit (`shader.cpp:531–532`, pre-fix) |
| 7 | heap path | ctor takes `init_heap()` iff raw `descriptor_heap_features.descriptorHeap` (`shader.cpp:541–542`); `init_heap()` never creates templates (`:461–479`) |
| 8 | cmd flush-path gate | `CommandBuffer` ctor sets `desc_buffer_enable` ONLY `else if (supports_descriptor_buffer)` — the DERIVED flag (`command_buffer.cpp:108`), for `Generic`/`AsyncCompute` types (`:85`); all other types stay legacy |
| 9 | derived flag | `supports_descriptor_buffer = descriptorBuffer.raw AND (sampler/range sizing × 512 Ki ×2 AND combinedImageSamplerDescriptorSingleArray)` (`context.cpp:2271–2284`) |
| 10 | crashing cmd type | `flush_cache_upload()` uses `direct_cmd` = `Type::Generic` (`gs_renderer.cpp:3897`), default `request_command_buffer()` type is `Generic` (`device.hpp:293`) |
| 11 | on-device raw bit | `VK_EXT_descriptor_buffer` in enabled-extension logcat lines, ALL launches G17 (3/3) + G18 (1/1); heap extension 0 lines everywhere + vkjson count 0 → raw descriptorBuffer=true, raw descriptorHeap=false |
| 12 | on-device derived flag | G17 r3 (unfixed) REACHED `:3505` on a Generic cmd buffer → legacy branch taken → `desc_buffer_enable == false` → `supports_descriptor_buffer == false` (proven by observation + #8/#10) |
| 13 | all slot writers | repo-wide grep: ONLY zero-init (#5), creation (`shader.cpp:743`), dtor destroy (`shader.cpp:754–757`); readers at `command_buffer.cpp:2983` (push) / `:3503` (flush) via `get_update_template` (`shader.hpp:167–169`) |
| 14 | sibling raw-bit gates | `descriptor_set.cpp:253–254` (push-layout skip — benign: only selects flush-vs-push path, consistent with observed flush crash) and `memory_allocator.cpp:1424` (pre-baked descriptor bytes — unused by the legacy template path) are NOT crash-causing; every OTHER descriptor-mode consumer uses the derived flag(s) |

### 2c. Cause verdict (static — the brief's §1 question)

| candidate | verdict | reasoning |
| --- | --- | --- |
| mask-skip (`shader.cpp:551–554`) | ELIMINATED | Reaching `:3505` for set 0 requires set 0 ∈ `set_update_mask` ⊆ `descriptor_set_mask` (#2) and ∉ bindless mask (#3). Had `create_update_templates()` run, #4 would have created template[0]. Creation-failed separately eliminated (G17 0-line LOGE census, re-confirmed: 0 "Failed to create descriptor update template" in G18 logcat too) |
| runtime-nulled slot | ELIMINATED | #13: no code nulls a live slot; the dtor path requires layout teardown, but the layout is alive (its allocator returned a VALID `vk_set` at `:3501` in G17's capture). Only heap corruption could null it — no evidence, and a guard masking that is forbidden by the brief |
| heap/init path (never created) | NAMED | #6+#8+#9+#11+#12: raw descriptorBuffer=true (extension enabled) → `init_legacy` SKIPS `create_update_templates()` for the whole layout; derived `supports_descriptor_buffer`=false (suitability conditions fail on this Adreno) → Generic cmd buffer takes the LEGACY flush path → `:3505` passes the zero-init null slot (#5). The heap half of "heap/init" is excluded (raw descriptorHeap=false, #7/#11) — the precise mechanism is the raw-vs-derived flag divergence |

Which suitability sub-condition fails (#9) is UNKNOWN (vkjson exposes only
extension names, not `descriptorBufferProperties`; no instrument allowed) —
tabled, and IRRELEVANT to the fix, which removes the divergence rather than
depending on the sub-condition.

### 2d. Fix shape decision (the cause decides — tabled per brief)

| shape | verdict | reasoning |
| --- | --- | --- |
| skip-the-call at `:3505` (`if (null) skip/continue`) | REJECTED | Would bind an uninitialized descriptor set (or unbind set 0) → garbage rendering or validation errors; masks the creation bug rather than fixing it |
| assert-and-early-out | REJECTED | Converts a segfault into silently dropped draws; not a fix |
| create-the-template (mirror the derived gate into `shader.cpp:531`) | ADOPTED | The layout wrongly skipped creation the legacy path needs. One-line predicate change aligns the two gates; on devices where raw==derived (all healthy devices) behavior is bit-identical — the change bites ONLY in the broken configuration |

The hunk (uncommitted in SSD clone, same standing as G14 shims; full text in
`g18-fix.diff`):

```diff
-	if (!device->get_device_features().descriptor_buffer_features.descriptorBuffer)
+	// G18 local experiment fix: gate template creation on the derived
+	// supports_descriptor_buffer flag (the same predicate CommandBuffer uses
+	// to select the descriptor-buffer flush path), not the raw feature bit.
+	// When the raw bit is set but Granite declines descriptor buffers
+	// (context.cpp suitability conditions), the legacy flush path needs
+	// these templates; skipping them passes NULL to the driver (O2).
+	if (!device->get_device_features().supports_descriptor_buffer)
 		create_update_templates();
```

Fix presence: rebuild log contains the `shader.cpp` compile; G18 binary sha
`a62d8a2b…` ≠ G14 `c4cd63b4…` (same tree + this hunk only). No other clone
edits (§2a). Latent-pattern note (NOT this hunk — tabled): `init_heap()`
also creates no templates, so a non-Generic/AsyncCompute cmd buffer doing
descriptor flushes on a heap/buffer device would share the pattern; no such
path fires in this workload (all GS dispatches are Generic, #10).

### 2e. Rebuild record (NEW SSD dir — G14 dir untouched)

| item | observed |
| --- | --- |
| configure | G14 recipe verbatim (`-G Ninja`, NDK android.toolchain.cmake, `arm64-v8a`, `android-35`, no build-type, no `VULKAN_SDK`) → exit 0, `Configuring done (14.8s)`, `Processor: aarch64` |
| build | `cmake --build … --target parallel-gs-replayer -j2` → exit 0, `[458/458] Linking CXX executable tools/parallel-gs-replayer` (1 pre-existing `-Wshadow` warning in `dump/gs_dump_parser.hpp`, same family as usual) |
| binary | `/Volumes/Extreme SSD/parallel-gs-g18-android-build/tools/parallel-gs-replayer`, 265,837,472 B (same size as G14 — one-line predicate change), sha `a62d8a2b3380c2bf62087728c793ed189a640f7450cda0c945e8b10d223bfe27` (≠ G14 ✓) |
| build dir | 4,482,048 KiB (cap 6 GB ✓); G14 dir == 4,480,000 KiB (untouched ✓) |

## 3. Task 2 — bounded on-device re-run + score (Odin)

### 3a. Re-run table (ONE run — retry not used, §3c)

| item | observed |
| --- | --- |
| staging | `/data/local/tmp/g18/` ONLY; dump + G18 binary pushed; on-device shas FULL-match host (`154d9d85…` / `a62d8a2b…`); logcat cleared, Granite empty before |
| command | `timeout -s KILL 280 ./parallel-gs-replayer /data/local/tmp/g18/g13-dump.gs --iterations 2` (G14 shape verbatim) |
| exit / wall | **139** (SIGSEGV) / <1 s wall (same-second `date` stamps; logcat spans 16.085→16.278, ~200 ms) |
| logcat | 36 lines: init + extension list (incl. `VK_EXT_descriptor_buffer`, no heap) + `Running frame` + G11/G10 first-draw lines + TWO `Stalled compile (compute, …success: yes)` (`502d5366ce6695c1`, then `b7109780b562b0fe`); 0 "Failed to create descriptor update template" |
| progress vs G17 r3 (O2) | r3's logcat ENDS at the first stalled compile (`502d…`) — the O2 death site (first-draw texture-upload `dispatch_indirect`); G18 continues PAST it (G11 `record prims=17…` + second stalled compile) |
| fate | NOT O2 (no fault-`0xf0`, §3b); NOT O1 (O1 = SIGSEGV `0x380` null-`table` @ `trim():171` in init — G15; this is fault-`0x0` in the driver compiler, post-first-draw); NOT O3 (O3 = SIGABRT @ `trim():173`) → NEW signature, tabled **O4** |
| device outputs | 0 scanouts (dir == inputs only); tombstone_12 written 06:51 (pulled, §3b); device dir removed after (`mg/` only ✓) |

### 3b. Tombstone_12 triage (pid 20749 — O4 naming)

| slot | fact |
| --- | --- |
| crashing thread | tid 20753 (async worker): `…__async_func<…kick_compilation_tasks()…>` → `CommandBuffer::build_compute_pipeline` → `PipelineCache::create_pipeline` → `qglinternal::vkCreateComputePipelines+1224` → `libllvm-qgl.so` ×16 → SIGSEGV fault `0x0`, null-pointer dereference (x8=0 at `pc …libllvm-qgl`) |
| main thread (20749) | CONCURRENTLY inside `vkCreateComputePipelines+1224` ← `build_compute_pipeline` ← `flush_compute_pipeline` ← `flush_compute_state` ← `dispatch` ← `dispatch_triangle_setup` ← `flush_rendering` ← `flush_render_pass` ← … ← `a_d_XYZ2` ← `write_register` ← `packed_ADONLY` ← `gif_transfer` ← `iterate_until_vsync` ← `main` |
| O2-site passed | main is lexically PAST the first-draw texture-upload `dispatch_indirect` (O2's 4/4 death site: `upload_texture ← flush_cache_upload ← flush(FBPointer)`): same dump ⇒ same first dispatch ⇒ same `:3505` flush now with a VALID template (fix), no `0xf0` fault anywhere |
| fix-causation | NONE plausible: `VkDescriptorUpdateTemplate` creation is not an input to pipeline compilation; the crash is inside the Adreno shader compiler on a pre-existing async path (`kick_compilation_tasks`, untouched by G18). O4 is the next wall exposed by progress, not a fix regression |
| O1/O3 lottery | neither fired (both die in init trims; this run reached first draw) |

### 3c. Retry + lldb decisions (tabled, both NO)

| decision | verdict | rationale |
| --- | --- | --- |
| ONE retry | NOT USED | Brief permits it iff "O1/O3 fires instead of first draw" — neither fired; the run reached first draw AND passed O2's site, yielding a classifiable signal + named next wall. O4 determinism-vs-lottery stays single-sample (gap §7.2) |
| lldb triage | NOT USED | Tombstone_12 fully classifies O4 (fault/regs/both stacks); no debugger question is open that lldb could answer within this brief |

### 3d. Score vs G13 oracles (`g14-diff.py`, zero new tooling)

`g14-diff.py ps2x-g13 ps2x-g18`: oracles **ALL OK** (8/8 pixel-shas);
device PPMs in `ps2x-g18`: **0** → `SCORE: N/A`. (Vintage reason string
"G14 run crashed…" — the scored fact is 0 scanouts: O4 dies in first draw
before any vsync completes.) No diff table is fabricable and none is faked.

## 4. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| `update_template[0]` is null via a statically nameable cause; the minimal cause-justified fix lets the bounded re-run pass O2's site | **SUPPORTED**: cause NAMED as the raw-vs-derived descriptor-buffer flag divergence (`shader.cpp:531` vs `command_buffer.cpp:108` + `context.cpp:2279-2284`), mask-skip and runtime-nulled eliminated with receipts; the one-hunk derived-gate fix passes O2's exact site (main progressed past the first-draw texture-upload dispatch; no `0xf0` fault). Separately OBSERVED: the run then dies on a NEW wall O4 (Adreno compiler SIGSEGV fault-`0x0` in `vkCreateComputePipelines` on the async worker, main concurrently in the same call) |

The ONE next action the numbers justify: **an O4-triage brief with a
no-rebuild first discriminator — re-run the SAME G18 binary (`a62d8a2b…`) with
`PGS_SKIP_COMPILATION_TASKS=1`** (env knob at `gs_renderer.cpp:459-464`,
skips async precompilation). Branch: O4 disappears ⇒ the crash needs the
async/concurrent path (driver thread-safety vs async-path issue — serialize
or gate); O4 persists on main's sync compile ⇒ Adreno compiler bug on a
specific shader (minimize + filing package, G17-§2f-shaped). Rationale: O2 is
beaten on the main path (fix-works ⇒ adoption input queues behind O4); O4 is
now the blocker and the knob isolates it in ONE bounded run without touching
the tree. Queued behind it (not this action): fix adoption (the hunk is
upstream-plausible as-is); O1/O3 writer naming (still open, no longer
first-draw-blocking); the §2f G17 Adreno robustness filing (unchanged).

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.

## 5. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c19…` (LGPL-3.0+ per G7 §4 — cited, not re-read) |
| Granite rev | `16e7395f…` (MIT per G7 §4 — cited); fix + read sites are Granite framework code |
| G14 shims S1–S3 + G7/G8/G10/G11 hunks | untouched, still uncommitted in SSD clone only |
| G18 addition | ONE hunk, `Granite/vulkan/shader.cpp` (`init_legacy` creation gate) — G18-original, uncommitted, text tabled (§2d + `g18-fix.diff`) |
| NDK r30 | `30.0.16248370` (Apache-2.0 toolchain) |
| logcat/tombstone | OS/debugger-produced run receipts of our own binary (no PII; `uid: 2000` shell) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 6. Exact commands

Mac (`/Users/bradrichardson/dev/ssx3` unless noted;
`COPYFILE_DISABLE=1` on SSD steps):

```text
git -C "<ssd>/parallel-gs-g7" rev-parse HEAD ; status --short   # §2a
grep -n "vkUpdateDescriptorSetWithTemplate(device" .../command_buffer.cpp  # :3505
grep -n "create_update_templates|init_legacy|init_heap" .../shader.cpp     # §2b
grep -n "desc_heap_enable|desc_buffer_enable" .../command_buffer.* .../device.cpp
grep -rn "update_template" <clone>/Granite/vulkan/              # §2b #13 (all writers)
grep -rn "descriptor_buffer_features\.descriptorBuffer\b" <clone>/Granite/vulkan/  # §2b #14
grep -rn "request_command_buffer" <clone>/gs/                   # §2b #10
sed -n .../shader.cpp .../command_buffer.cpp .../context.cpp .../descriptor_set.cpp .../memory_allocator.cpp  # §2b reads
<fix hunk applied to <clone>/Granite/vulkan/shader.cpp>         # §2d
git -C <clone>/Granite diff vulkan/shader.cpp > .../G18/g18-fix.diff
cmake -S <clone> -B <g18-build> -G Ninja -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-35  # exit 0
cmake --build <g18-build> --target parallel-gs-replayer -j2     # exit 0 [458/458]
shasum -a 256 <g18-binary> ; du -sk <dirs> ; df -h / <ssd>      # §0/§2e
python3 <ssx3>/G14/g14-diff.py <ps2x-g13> <ps2x-g18>             # §3d (oracles OK, N/A)
```

adb (`-s 622c49b1` throughout; `/data/local/tmp/g18/` ONLY; `mg/` never touched):

```text
shell 'cmd gpu vkjson' | grep -ciE 'VK_EXT_descriptor_heap'      # 0 (§2a)
shell 'rm -rf /data/local/tmp/g18 && mkdir -p /data/local/tmp/g18'
push <dump> $G18DIR/g13-dump.gs ; push <g18-binary> $G18DIR/parallel-gs-replayer
shell 'sha256sum g13-dump.gs parallel-gs-replayer'               # both match host
logcat -c ; logcat -d -s Granite:V | tail -2                     # before (empty)
shell 'cd $G18DIR && date +%s; timeout -s KILL 280 ./parallel-gs-replayer $G18DIR/g13-dump.gs --iterations 2; echo RUN_EXIT=$?; date +%s'  # 139
logcat -d -s Granite:V > <ssd>/ps2x-g18/g18-logcat.txt           # 36 lines
shell 'ls -la $G18DIR/ ; ls -lt /data/tombstones/ | head -8'     # 0 scanouts; _12 new 06:51
pull /data/tombstones/tombstone_12 <ssd>/ps2x-g18/g18-tombstone-12.txt
shell 'rm -rf /data/local/tmp/g18 && ls /data/local/tmp/'        # DEVICE_CLEAN (mg/ only)
```

## 7. Gaps (what this brief could not do)

1. WHICH suitability sub-condition fails on this Adreno (`context.cpp:2280-2283`)
   is UNKNOWN (vkjson lacks properties; no instrument allowed) — irrelevant to
   the fix, which removes the divergence.
2. O4 is single-sample: deterministic vs lottery UNKNOWN (retry not used per
   contract §3c — the §4 knob run answers this plus the sync/async branch).
3. Whether the fix changes ANY behavior on raw==derived devices: no (predicate
   agrees there by construction) — but unverified by a run on such a device.
4. Zero on-device scanouts (again) → no pixel score. `g14-diff.py`
   stands ready (oracles ALL OK re-verify passes through it).
5. O1's null-write site and O3's corruption writer stay UNNAMED (init-trim
   lottery members; neither fired this run).
6. No tombstones pulled except `_12` (this run's; OS store otherwise untouched).
7. `init_heap()`'s no-template pattern for non-Generic cmd buffers stays as
   upstream wrote it (tabled §2d, unfired here).
8. HWASan-binary sha anomaly (G17 §2a) untouched — G18 never uses that binary.
9. `upstream/` and ps2xGS harness code untouched; no new dumps (per the
   stop rule). No upstream contact (nothing filed).

## 8. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…`
  (G7/G8/G10/G11 hunks + G14 S1–S3 + G18 fix hunk — all uncommitted).
- G14 build: `/Volumes/Extreme SSD/parallel-gs-g14-android-build/` (pristine).
- G18 build: `/Volumes/Extreme SSD/parallel-gs-g18-android-build/`,
  `tools/parallel-gs-replayer` (265,837,472 B, `a62d8a2b…`).
- SSD: `/Volumes/Extreme SSD/ps2x-g18/` (logcat + tombstone_12);
  `ps2x-g13/`–`ps2x-g17/` pristine.
- Tools: `/tmp/g18-configure.log`, `/tmp/g18-build.log` (session-only).
- Commits: ps2xGS `[G18]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G18/` `[G18]` + same trailer (NOT pushed).
