# G20 report — capture-first O4 minimize: logging-only build names the crashing pipeline (sampler_feedback, hash 7463dfd379df2855 + SPIR-V), Adreno filing package assembled

Brief: G20 (this turn) — executes G19's §4: (1) write a logging-only
capture patch (ONE hunk) that records the crashing compute pipeline's
identity at the `dispatch_texture_analysis` dispatch WITHOUT changing any
compiler input; (2) build it in a NEW SSD build dir; (3) ONE bounded
knob-env run to capture the identity; (4) assemble the COMPLETE Adreno
filing package (tabled, NOT filed — no upstream contact). Tables +
hypothesis + next-action recommendation, no verdicts beyond the
hypothesis. Time box 6 h (used ~1 h). Read first per the brief:
`local/research/G19/REPORT.md` (all of it). No renderer behavior changes
(the hunk is a pure 18-line insertion: reads + LOGI only — no-mutation
proof §2b).

Machine: same as G8–G19 (Apple M4, macOS 27.0 — no new installs) +
Odin3 (`622c49b1`, Android 15). Pins: paraLLEl-GS
`3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` + G14 S1–S3 shims + G11/G8+G10
hunks + G18 fix hunk + G20 capture hunk (re-verified §2a; all other hunk
bytes identical) / rich dump sha `154d9d85…` (re-verified host-side AND
on-device) / knob at `gs_renderer.cpp:459-464` (re-verified §2a, fired
on-device §3a).

Headline result: the crashing pipeline is NAMED. The capture run (exit
139, first draw reached, knob-fired LOGI present) died on compute pipeline
`7463dfd379df2855` — program `62cacea0b15bd6e7`, shader
`c61f1a8116a82d4b`, 0 spec constants — and the device-logged shader hash
matches the host-computed Granite hash of the `sampler_feedback`
(`sampler_feedback.comp`) SPIR-V blob EXACTLY, closing the identity chain
end to end (§3a–§3c). The tombstone re-triages O4 identically (SIGSEGV
fault `0x0`, pc `libllvm-qgl.so+0x472fc0`, `vkCreateComputePipelines+1224`,
main thread from `dispatch_texture_analysis+824`, async worker absent) —
the capture did not perturb the crash. The filing package is COMPLETE
(§3d): device/driver/build-ids + repro + fault shape + shader
(SPIR-V sha `b0a09fa0…`, 5049 words, extraction recipe §3d). Score:
oracles ALL OK, device scanouts 0 → SCORE N/A. Retry NOT used (O1/O3
never fired; tabled §3e). lldb NOT used (tombstone fully classifies;
tabled §3e).

## 0. Byte caps (declared at session start) vs actuals

| class | cap | actual |
| --- | --- | --- |
| NEW G20 build dir (SSD, configure + full build) | 6 GB | 4,482,048 KiB (~4.3 GB) ✓ |
| SSD ps2x-g20 retrieval (logcat + tombstone_14 + .spv + scripts + hunk diff) | 100 MB | ~200 KB apparent; 11,264 KiB allocated (ExFAT 1 MiB clusters) ✓ |
| SSD ps2x-g10…g19 (read-only) | 0 growth | 43008/28672/9216/35840/7168/31744/43008/17408/5120/5120 KiB == pre-run exactly ✓ |
| SSD G14/G18 build dirs (read-only) | 0 growth | 4,480,000 / 4,482,048 KiB == pre-run exactly ✓ |
| SSD clone (source) | ONE hunk only, uncommitted | `command_buffer.cpp` +18/−0; all other hunk bytes identical (G18 HUNK_MATCH re-confirmed pre-build) ✓ |
| internal volume (`/`) | ≤1 GB delta, no clones/builds | 4.4 Gi (G19-end) → 1.3–3.0 Gi avail (other-lane churn); my residue = `/tmp/g20-*` ~150 KB ✓ |
| device `/data/local/tmp/g20/` (transient) | 600 MB peak | peak ~279 MB (266 + 11.5); 0 remaining — dir removed, `mg/` only ✓ |
| `/tmp/g20-*` host logs | session-only | build log 58,902 B + hunk diff 1,512 B + extract scripts + .spv ✓ |
| committed to git | text only (report + hunk diff) | REPORT.md + g20-capture.diff (ssx3 mirror + ps2xGS report); no captures, no binaries, no build dirs, no .spv in git ✓ |

SSD 250 → 242 Gi avail: this brief wrote ~4.3 GB (new build dir) + ~11 MB
allocated (retrieval); the remainder is other-lane. No code copied into
any GPL tree. No P-lane contention: no recomp boots/builds, no P-lane
lease (no fork writes at all), no bytesize/WSL use. Host build `-j2`
(kept). Tombstones: pulled ONLY `tombstone_14` (this run's, §3b); OS
store otherwise untouched. Slots keep rotating (`_14` new 08:05 after
G19's `_13`); `ls -lt` remains the oracle.

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | A logging-only instrumented build (no compiler-input change) reproduces O4 AND names the crashing compute pipeline (Granite pipeline hash + shader hash), with the SPIR-V recovered host-side from the static bank — completing the Adreno filing package |
| observable signal | capture-hunk diff + no-mutation proof + build exit/sha/build-id + ONE bounded run (exit/wall/frames/fate) + last pre-create line + shader-hash match + tombstone re-triage + filing package + diff score |
| alternatives | (a) capture names a pipeline + hashes match ⇒ filing package complete (table it); (b) O4 disappears under instrumentation ⇒ capture perturbed the crash (table, STOP — proves nothing); (c) O1/O3 fires instead of first draw ⇒ lottery note + ONE retry |
| stop condition | patch must be proven logging-only on static review or NO build; TWO bounded device runs max (capture run + one retry iff O1/O3 fires instead of first draw); no tuning loop, no behavior changes, no new dumps, no workaround; no lldb unless the tombstone cannot triage |
| outcome → next action | numbers name the next single experiment (§4) |

Outcome: alternative (a) — O4 reproduced bit-for-bit on the instrumented
binary AND the crashing pipeline is named with a verified shader-hash
match (§3a–§3d). No tuning loop was entered: one hunk, one build, one
device run.

## 2. Task 1 — capture patch + build (no device until §3)

### 2a. Pin verification (pre-work — G19 §2a reproduced + G18 binary check)

| item | observed |
| --- | --- |
| SSD clone HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (== pin) |
| tree status | `M CMakeLists.txt` (S1) + `m Granite` (S2) + `M gs/gs_interface.cpp` (G11) + `M tools/CMakeLists.txt` (S3) + `M tools/gs_dump_replayer.cpp` (G8+G10) + ExFAT `._` sidecars — G19 §2a exactly, NO other delta |
| Granite rev | `16e7395f6a48` (== pin); Granite diff pre-hunk: S2 + G7 + G18 only |
| G18 hunk bytes | `git -C Granite diff vulkan/shader.cpp` byte-identical to `g18-fix.diff` (`diff` exit 0, HUNK_MATCH) — verified BEFORE writing the capture hunk |
| rich dump (host) | 11,537,377 B, sha `154d9d8577a210fb794b29a048ee2cf08053f87fe220bb2bf975933a02ad7e32` full-match (device re-verified §3a) |
| G18 binary (zero-fill watch) | 265,837,472 B, sha `a62d8a2b…` — NO recurrence of the G19 all-zero damage |
| G14 binary (baseline) | 265,837,472 B, sha `c4cd63b4…` (G14 build dir untouched) |
| knob file/lines | `PGS_SKIP_COMPILATION_TASKS` at `gs/gs_renderer.cpp:459` (`getenv`), early-return + `LOGI("Skipping precompilation…")` at `:459-464` — confirmed |
| NDK / cmake | r30 `30.0.16248370` (`/opt/homebrew/share/android-ndk`); ninja `/opt/homebrew/bin/ninja` |
| device (read-only pre-check) | `622c49b1`, Odin3, Android 15; `/data` 28 G free; `/data/local/tmp/` == `mg/` only; newest tombstone `_13` 07:44 (G19's run) |
| driver | Adreno (TM) 830, API 1.3.284, Driver 512.800.58 (logcat §3a; == G14–G19) |

### 2b. Capture design (where the identity already exists)

| slot | fact |
| --- | --- |
| crash call | `CommandBuffer::build_compute_pipeline` → `device->pipeline_binary_cache.create_pipeline(&info, …)` → `vkCreateComputePipelines` (tombstone §3b) |
| why the crasher is silent today | the only per-pipeline log, `log_compile_time("compute", compile.hash, …)`, runs AFTER `create_pipeline` returns — the crashing pipeline dies first |
| already-computed key | `compile.hash` (Granite pipeline hash: program + layout + spec mask/values + robustness + subgroup, folded in `update_hash_compute_pipeline` during flush, before the build call) |
| already-computed shader id | `shader.get_hash()` = `Shader::hash(SPIR-V, size)` = FNV-1a-64 over the SPIR-V words (replicable host-side — `Util::Hasher` in `Granite/util/hash.hpp`) |
| already-computed variant inputs | `spec_info.mapEntryCount` + `spec_entries[i].constantID` + `spec_constants[i]` (== `pSpecializationInfo`), `robustness`, `subgroup_control_size` — all locals/fields read by the existing code on this path |
| insertion point | `Granite/vulkan/command_buffer.cpp`, `build_compute_pipeline`, immediately before `auto start_ts = …` (line 1198) — after ALL `info`/`spec`/`flags` setup, so every logged value is final |
| SPIR-V recovery | NO device capture needed: all PGS compute SPIR-V is a static bank (`gs/shaders/slangmosh.hpp`, `spirv_bank[]`, word offsets + byte sizes) — the device-logged shader hash selects the blob, extracted + hashed host-side (§3c) |
| decode rule | the LAST `G20 pre-create` line with no matching post-create line names the crashing pipeline (crash is inside the immediately following `create_pipeline`) |

The hunk (`git -C Granite diff vulkan/command_buffer.cpp`, +18/−0, also
saved as `g20-capture.diff` beside this report):

```diff
@@ -1195,6 +1195,24 @@ Pipeline CommandBuffer::build_compute_pipeline(Device *device, const DeferredPip
  		info.pNext = &robustness;
  	}
 
+	// G20 capture: log the compute pipeline identity BEFORE create_pipeline, so the
+	// crashing pipeline (which never reaches log_compile_time) is named in logcat.
+	// Logging-only: reads of already-computed locals/const-ref fields + LOGI; no
+	// pipeline-creation input is written, reordered, or skipped.
+	LOGI("G20 pre-create (compute, %016llx): program %016llx, shader %016llx, specs %u, robust %u, subgroup %u.\n",
+	     static_cast<unsigned long long>(compile.hash),
+	     static_cast<unsigned long long>(compile.program->get_hash()),
+	     static_cast<unsigned long long>(shader.get_hash()),
+	     spec_info.mapEntryCount,
+	     compile.static_state.state.robustness,
+	     compile.static_state.state.subgroup_control_size);
+	for (uint32_t g20_spec = 0; g20_spec < spec_info.mapEntryCount; g20_spec++)
+	{
+		LOGI("G20 pre-create spec %016llx: id %u = %08x.\n",
+		     static_cast<unsigned long long>(compile.hash),
+		     spec_entries[g20_spec].constantID, spec_constants[g20_spec]);
+	}
+
  	auto start_ts = Util::get_current_time_nsecs();
  	VkResult vr = device->pipeline_binary_cache.create_pipeline(&info, compile.cache, &compute_pipeline);
```

No-mutation proof (static review — the brief's gate; any doubt would have
stopped the build):

| # | claim | proof |
| --- | --- | --- |
| 1 | zero deletions/modifications | diff shows ONLY `+` lines; every pre-existing line byte-identical |
| 2 | reads only | `compile` is a `const DeferredPipelineCompile &` (compiler-enforced); `program`/`shader` touched solely via const `get_hash()`; `spec_*` locals read by index; `robustness`/`subgroup_control_size` are 1-bit `uint32_t` bitfields read for `%u` (type-checked §2a) |
| 3 | writes confined | only the new loop counter `g20_spec` (dies at hunk end) + LOGI's internal log buffer — the same class as the ~10 pre-existing LOG calls on this path |
| 4 | control flow unchanged | the `for` loop always terminates (`mapEntryCount` fixed, no break/return/continue/goto); falls through to the unchanged `start_ts` line |
| 5 | memory-safe | `spec_entries[i]`/`spec_constants[i]` for `i < mapEntryCount`: entries `0..mapEntryCount-1` were written by the mask loop above; arrays sized `VULKAN_NUM_TOTAL_SPEC_CONSTANTS`; mask==0 ⇒ count==0 ⇒ loop skipped — no OOB |
| 6 | no new null risk | `compile.program` and `shader` are dereferenced at function top already (line ~1086); our uses come strictly after |
| 7 | compiler inputs bit-identical | `info`, `spec_info`, `spec_entries`, `spec_constants`, `compile`, `device`, shader module, pipeline cache — all untouched at the `create_pipeline` call; delta vs the G18 binary is logcat bytes + nanoseconds of timing only (same class as existing `log_compile_time` logging) |

### 2c. Build record (NEW SSD dir — G14/G18 dirs untouched)

| item | observed |
| --- | --- |
| configure | `cmake -S <clone> -B parallel-gs-g20-android-build -G Ninja -DCMAKE_TOOLCHAIN_FILE=…/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-35` (G14 recipe verbatim) → exit 0, `Configuring done (14.7s)`, `Processor: aarch64` |
| build | `cmake --build … --target parallel-gs-replayer -j2` → exit 0, `[458/458] Linking CXX executable tools/parallel-gs-replayer` |
| warnings | 2 pre-existing classes, 0 from the hunk: `command_buffer.cpp:375` unused `is_legacy_layout` (line 375, far from hunk at 1198+) + `gs_dump_parser.hpp:49` shadow (unrelated file) |
| hunk compiled | `[352/458] …/command_buffer.cpp.o` in build log ✓ |
| binary | `tools/parallel-gs-replayer`, 265,838,568 B (+1,096 vs G18 — log strings + code), valid ELF (`7f45 4c46`) |
| sha | `8d5079cc822932efd8794060c12dd6a3d2aeec59df962459f4b3e03a0bba3341` |
| build-id | `a8402ce01b06ce8581c712d95ad120a401b878fe` (NDK `llvm-readelf --notes`) |
| build dir | 4,482,048 KiB (cap 6 GB ✓); G14/G18 dirs == pre-run exactly (0 growth ✓) |

## 3. Task 2 — ONE bounded capture run + filing package (Odin)

### 3a. Run table (ONE run — retry not used, §3e)

| item | observed |
| --- | --- |
| staging | `/data/local/tmp/g20/` ONLY; dump + G20 binary pushed; on-device shas FULL-match host (`154d9d85…` / `8d5079cc…`); logcat cleared, Granite empty before |
| command | `PGS_SKIP_COMPILATION_TASKS=1 timeout -s KILL 280 ./parallel-gs-replayer /data/local/tmp/g20/g13-dump.gs --iterations 2` (G19 shape, G20 binary) |
| exit / wall | **139** (SIGSEGV) / ~0 s wall (`date` 08:05:08→08:05:08; logcat spans 08.335→08.465, ~130 ms) |
| knob-fired receipt | logcat line `Skipping precompilation of shaders. May stutter way more.` + tombstone shows NO async worker (§3b) ⇒ knob fully effective |
| logcat | 48 lines: init + extension list (incl. `VK_EXT_descriptor_buffer`, no heap) + `Running frame` + G11/G10 first-draw lines + `record prims=17` + THREE `G20 pre-create` groups (below) + TWO `Stalled compile (compute, …success: yes)` (`502d…` then `b710…` — same two hashes, same order as G18/G19); 0 "Failed to create descriptor update template" |
| capture lines | (1) `502d5366ce6695c1` program `709a59f0212465f0` shader `da8d2940f6faff6f` specs 4 (id0=0, id1=003fffff, id2=0, id3=1) → success; (2) `b7109780b562b0fe` program `5f00655f89269727` shader `a7a0133ba031cebd` specs 4 (all zero) → success; (3) **`7463dfd379df2855` program `62cacea0b15bd6e7` shader `c61f1a8116a82d4b` specs 0, robust 0, subgroup 0 — LAST line, no post-create line ⇒ THE CRASHER** |
| progress vs G19 | identical shape through the second stalled compile (first draw reached, O2's site passed again — no `0xf0` fault), then the instrumented binary names the next pipeline before dying on it |
| fate | NOT O1 (init-trim null-`table` — this dies post-first-draw in the driver compiler); NOT O2 (no fault-`0xf0`); NOT O3 (SIGABRT — this is SIGSEGV) → O4 signature again, re-triaged §3b |
| device outputs | 0 scanouts (dir == inputs only); tombstone_14 written 08:05 (pulled, §3b); device dir removed after (`mg/` only ✓) |

### 3b. Tombstone_14 triage (pid 11868 — O4 on the sync path, unperturbed)

| slot | fact |
| --- | --- |
| crashing thread | tid **11868** == pid ⇒ MAIN thread (same as G19) |
| fault | SIGSEGV fault `0x0`, null-pointer dereference, pc `libllvm-qgl.so+0x472fc0` (SAME offset as G19) ×18 frames (BuildId `28a2407f…`) → `vulkan.adreno.so` (BuildId `d05dded9…`) → `qglinternal::vkCreateComputePipelines+1224` (same +1224 offset as G18/G19) |
| app stack | `PipelineCache::create_pipeline+260` (same +260) ← `build_compute_pipeline+1776` (offset shifted from G19's +1328 — EXPECTED: our 18 added lines) ← `flush_compute_pipeline` ← `flush_compute_state` ← `dispatch` ← **`dispatch_texture_analysis+824`** (same +824) ← `flush_rendering+844` ← … ← `main` (frames #26–#42 identical shape to G19) |
| binary identity | replayer BuildId in tombstone `a8402ce0…` == G20 binary ⇒ the tested binary is proven |
| other threads | exactly ONE: tid 11869 `PGS-Waiter`, parked in `pthread_cond_wait` (benign waiter, no driver calls) |
| async absence | ZERO `kick_compilation` / `__async_func` mentions in the whole tombstone (grep count 0) + knob-fired LOGI (§3a) ⇒ the async precompilation path did not exist in this process |
| perturbation check | same dispatch (+824), same fault, same driver offsets, same fault address, same thread census as G19 — the instrumentation did NOT move the crash; alternative (b) rejected |

### 3c. Shader attribution (device hashes × host bank — identity chain closed)

All 14 compute SPIR-V entries hashed host-side with the replicated
Granite `Util::Hasher` (FNV-1a-64, `slangmosh.hpp` word offsets + byte
sizes); every device-logged shader hash matches EXACTLY:

| pipeline | device shader hash | host match | program | fate |
| --- | --- | --- | --- | --- |
| `502d5366ce6695c1` | `da8d2940f6faff6f` | `upload[0]` (`upload.comp`, 5212 words) | `709a59f0212465f0` | success (14.3 ms stall) |
| `b7109780b562b0fe` | `a7a0133ba031cebd` | `triangle_setup` (`triangle_setup.comp`, 8890 words) | `5f00655f89269727` | success (29.9 ms stall) |
| **`7463dfd379df2855`** | **`c61f1a8116a82d4b`** | **`sampler_feedback` (`sampler_feedback.comp`, 5049 words)** | `62cacea0b15bd6e7` | **CRASH (no post line)** |

Non-firing compute shaders (hashed, not seen this run — completeness):
`binning` `23d775fe7c09f431`, `upload[1]` `4bee26c6b654ee00`,
`clut_write` `e13e8606339fafde`, `vram_copy` `349bf6ade2ed283a`,
`single_sample_heuristic` `40958a93556a9c1b`, `ubershader` ×4
(`0ac1b3751f44b997`, `ef145d65fd80a15c`, `f0da12d658ab2d57`,
`13de547182dbcf80`), `extwrite` `accfa079df93ae3d`, `qword_clear`
`d9521138600486ca. All 14 blobs show valid SPIR-V magic `0x07230203`.

Crashing-input note: the crashing pipeline carries ZERO specialization
constants (spec mask empty), robustness 0, subgroup-control 0 — the
simplest variant of the program; the crash input is the `sampler_feedback`
SPIR-V + the descriptor-buffer pipeline flags Granite sets on this path
(`VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT`, logcat §3a; no heap).

### 3d. Filing package (COMPLETE — tabled, NOT filed: no upstream contact)

| slot | content |
| --- | --- |
| device / OS | Odin3 (`622c49b1`), Android 15 |
| GPU / driver | Adreno (TM) 830, Vulkan API 1.3.284, Driver 512.800.58; `libllvm-qgl.so` BuildId `28a2407fad77f0ce9e9cba56e729e521`, `vulkan.adreno.so` BuildId `d05dded928ad46330f433c26cf9b82ec` |
| app revision | paraLLEl-GS `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd`, Granite `16e7395f6a48` (replayer build-id `a8402ce0…`, instrumented logging-only; crash also reproduces on uninstrumented build-id `0b01b274…`, G19) |
| repro | dump sha `154d9d85…` (11,537,377 B), `PGS_SKIP_COMPILATION_TASKS=1`, `--iterations 2` → exit 139 in ~1 s, tombstone_14 |
| crashing pipeline | hash `7463dfd379df2855`, program `62cacea0b15bd6e7`, entry `main`, stage COMPUTE, pSpecializationInfo NULL (0 entries), robustness off, subgroup-size-control off, `VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT` set (descriptor-buffer path, no heap) |
| crashing shader | `sampler_feedback` (`sampler_feedback.comp`): SPIR-V sha256 `b0a09fa039c1d9f9b4fc02b613233e9f051937d38be42e3761834df06aa96ea1`, 5049 words / 20,196 bytes, magic `0x07230203`, Granite shader hash `c61f1a8116a82d4b` |
| SPIR-V extraction | `slangmosh.hpp`: `spirv_bank[]` (`uint32_t`), words [148867, 153916) — `sampler_feedback = device.request_program(spirv_bank + 148867, 20196, &layout)`; contiguity-checked vs neighbors (`qword_clear` ends 148867, `ui_vert` starts 153916); extractor `g20-extract-spv.py` + blob `g20-sampler-feedback.spv` in SSD `ps2x-g20/` (NOT in git) |
| crash site | `dispatch_texture_analysis` compute `dispatch` during first draw (post `record prims=17`, post sync compiles `502d…` then `b710…` — the crashing pipeline is the NEXT compute compile after `b7109780b562b0fe`) |
| fault shape | sync `vkCreateComputePipelines` → SIGSEGV fault `0x0` null deref, pc `libllvm-qgl.so+0x472fc0`, 18 llvm frames, `+1224` wrapper offset, single-threaded compile (async path provably absent), main thread |
| determinism | 3/3 O4 across arms (G18 unknobbed, G19 knobbed, G20 instrumented-knobbed) — same dispatch, same fault, same driver offsets |
| still missing | NOTHING for the file-vs-workaround decision (§4) — the package is complete |

### 3e. Retry + lldb decisions (tabled, both NO)

| decision | verdict | rationale |
| --- | --- | --- |
| ONE retry | NOT USED | Brief permits it iff "O1/O3 fires instead of first draw" — neither fired; the run reached first draw, passed O2's site, captured the identity, and re-triaged O4 with a classifiable signal |
| lldb triage | NOT USED | Tombstone_14 fully classifies O4-on-sync (fault/regs/main stack/thread census/async absence); no debugger question is open that lldb could answer within this brief |

### 3f. Score vs G13 oracles (`g14-diff.py`, zero new tooling)

`g14-diff.py ps2x-g13 ps2x-g20`: oracles **ALL OK** (8/8 pixel-shas);
device PPMs in `ps2x-g20`: **0** → `SCORE: N/A`. (Vintage reason string
"G14 run crashed…" — the scored fact is 0 scanouts: O4 dies in first draw
before any vsync completes.) No diff table is fabricable and none is faked.

## 4. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| A logging-only instrumented build (no compiler-input change) reproduces O4 AND names the crashing compute pipeline (Granite pipeline hash + shader hash), with the SPIR-V recovered host-side from the static bank — completing the Adreno filing package | **CONFIRMED**: O4 reproduced bit-for-bit on the instrumented binary (exit 139, same dispatch `+824`, same fault `0x0`, same driver offsets `+0x472fc0`/`+1224`, async absent) AND the crashing pipeline is named — `7463dfd379df2855` / `sampler_feedback`, device shader hash `c61f1a8116a82d4b` matching the host bank hash exactly — with the SPIR-V (sha `b0a09fa0…`) completing the filing package (§3d). |

The ONE next action the numbers justify: **file the Adreno report (or
authorize a workaround brief) — the filing package is complete and the
decision it was assembled for is now ripe**. Recommended order: (1) file
upstream with §3d + the `.spv` (sync-path `vkCreateComputePipelines`
null-deref on a fixed compute shader, 3/3 repro, zero app-side
concurrency); (2) only if the file cannot move the schedule, brief the
workaround (defer/skip the `sampler_feedback` dispatch — no longer
premature now that the shader is named; note it changes rendering
behavior, unlike everything in G18–G20). Queued behind it (not this
action): fix adoption (G18 hunk still queued behind O4); O1/O3 writer
naming (still open, no longer first-draw-blocking); the G17 Adreno
robustness filing (unchanged).

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.

## 5. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c19…` (LGPL-3.0+ per G7 §4 — cited, not re-read) |
| Granite rev | `16e7395f…` (MIT per G7 §4 — cited); hunk + read sites are Granite framework code |
| G14 shims S1–S3 + G7/G8/G10/G11/G18 hunks | untouched, still uncommitted in SSD clone only (G18 HUNK_MATCH re-verified pre-build) |
| G20 addition | ONE hunk (+18/−0, `Granite/vulkan/command_buffer.cpp`), uncommitted in SSD clone only; diff committed as text (`g20-capture.diff`) beside this report |
| sampler_feedback SPIR-V | extracted host-side from the LGPL `slangmosh.hpp` bank; blob lives in SSD `ps2x-g20/` ONLY (not in any git tree); report tables its sha + extraction recipe |
| NDK r30 | `30.0.16248370` (Apache-2.0 toolchain) |
| logcat/tombstone | OS/debugger-produced run receipts of our own binary (no PII; `uid: 2000` shell) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 6. Exact commands

Mac (`/Users/bradrichardson/dev/ssx3` unless noted;
`COPYFILE_DISABLE=1` on SSD steps):

```text
git -C "<ssd>/parallel-gs-g7" rev-parse HEAD ; status --short   # §2a
git -C <clone>/Granite diff vulkan/shader.cpp | diff <ssx3>/local/research/G18/g18-fix.diff -  # HUNK_MATCH
shasum -a 256 <g18-binary> <g14-binary>                        # §2a (no zero-fill recurrence)
sed -n '450,470p' <clone>/gs/gs_renderer.cpp                    # §2a knob
shasum -a 256 <ssd>/ps2x-g13/g13-dump.gs                        # §2a dump
# (write the ONE hunk in Granite/vulkan/command_buffer.cpp @ :1198)
git -C <clone>/Granite diff vulkan/command_buffer.cpp > /tmp/g20-capture.diff  # §2b
python3 /tmp/g20-extract-spv.py ; python3 /tmp/g20-allcomp.py    # §3c (host hashes; read-only)
cmake -S <clone> -B <ssd>/parallel-gs-g20-android-build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-35  # §2c exit 0
cmake --build <g20-build> --target parallel-gs-replayer -j2     # §2c exit 0 [458/458]
shasum -a 256 <g20-binary> ; llvm-readelf --notes <g20-binary>  # 8d5079cc… + a8402ce0…
du -sk <dirs> ; df -h / <ssd>                                   # §0
python3 <ssx3>/local/research/G14/g14-diff.py <ps2x-g13> <ps2x-g20>  # §3f (oracles OK, N/A)
```

adb (`-s 622c49b1` throughout; `/data/local/tmp/g20/` ONLY; `mg/` never touched):

```text
shell 'ls /data/local/tmp/ ; ls -lt /data/tombstones/ | head'   # pre-check (mg/ only, _13 newest)
shell 'rm -rf /data/local/tmp/g20 && mkdir -p /data/local/tmp/g20'
push <dump> $G20DIR/g13-dump.gs ; push <g20-binary> $G20DIR/parallel-gs-replayer
shell 'sha256sum g13-dump.gs parallel-gs-replayer'               # both match host
logcat -c ; logcat -d -s Granite:V | tail -2                     # before (empty)
shell 'cd $G20DIR && date +%s; PGS_SKIP_COMPILATION_TASKS=1 timeout -s KILL 280 ./parallel-gs-replayer $G20DIR/g13-dump.gs --iterations 2; echo RUN_EXIT=$?; date +%s'  # 139
logcat -d -s Granite:V > <ssd>/ps2x-g20/g20-logcat.txt           # 48 lines
shell 'ls -la $G20DIR/ ; ls -lt /data/tombstones/ | head -6'     # 0 scanouts; _14 new 08:05
pull /data/tombstones/tombstone_14 <ssd>/ps2x-g20/g20-tombstone-14.txt
shell 'rm -rf /data/local/tmp/g20 && ls /data/local/tmp/'        # DEVICE_CLEAN (mg/ only)
```

## 7. Gaps (what this brief could not do)

1. The report is NOT filed upstream (no upstream contact in this brief —
   §3d is the filing package, §4 recommends the file).
2. No minimized repro below the full dump + replayer (the shader + flags
   are named; a Fossilize/standalone `vkCreateComputePipelines` repro is
   unbuilt — a filing-aid, not a filing-blocker).
3. Which SPIR-V construct the Adreno compiler chokes on is UNKNOWN (needs
   disassembly/delta work on the named blob — out of scope).
4. O4 rate stays 3/3 across arms (G18/G19/G20 — consistent, not yet a
   larger-n rate; each arm is single-sample).
5. Zero on-device scanouts (again) → no pixel score. `g14-diff.py`
   stands ready (oracles ALL OK re-verify passes through it).
6. O1's null-write site and O3's corruption writer stay UNNAMED (init-trim
   lottery members; neither fired this run).
7. No tombstones pulled except `_14` (this run's; OS store otherwise untouched).
8. `upstream/` and ps2xGS harness code untouched; no new dumps (per the
   stop rule).

## 8. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…`
  (G7/G8/G10/G11 hunks + G14 S1–S3 + G18 fix hunk + G20 capture hunk — all
  uncommitted, all other hunk bytes identical to G19-end).
- G14 build: `/Volumes/Extreme SSD/parallel-gs-g14-android-build/` (pristine).
- G18 build: `/Volumes/Extreme SSD/parallel-gs-g18-android-build/`,
  `tools/parallel-gs-replayer` (265,837,472 B, `a62d8a2b…` — undamaged).
- G20 build: `/Volumes/Extreme SSD/parallel-gs-g20-android-build/`,
  `tools/parallel-gs-replayer` (265,838,568 B, `8d5079cc…`, build-id
  `a8402ce0…` — logging-only instrumented).
- SSD: `/Volumes/Extreme SSD/ps2x-g20/` (logcat + tombstone_14 +
  sampler_feedback.spv + extractor + hunk diff); `ps2x-g10/`–`ps2x-g19/`
  pristine.
- Tools: `/tmp/g20-build.log`, `/tmp/g20-capture.diff`,
  `/tmp/g20-extract-spv.py`, `/tmp/g20-allcomp.py`,
  `/tmp/g20-sampler-feedback.spv` (session-only).
- Commits: ps2xGS `[G20]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G20/` `[G20]` + same trailer (NOT pushed).

TAIL-RECEIPT: G20 report ends here. Crashing pipeline 7463dfd379df2855 =
sampler_feedback, SPIR-V sha b0a09fa0…, filing package §3d COMPLETE.
