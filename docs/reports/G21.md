# G21 report — Adreno filing draft + standalone repro package (NO submission, NO workaround, NO device runs)

Brief: G21 (this turn) — re-verify every G20 S3d filing fact against its
primary receipt; disassemble the named SPIR-V host-side and survey its
construct families (G20 gap 3, first pass); assess standalone-repro
feasibility (Fossilize vs minimal app vs full-dump) and build the pick
host-side; write a complete paste-ready filing DRAFT (not submitted).
Tables + hypothesis + next-action recommendation, no verdicts beyond the
hypothesis. Time box 6 h (used ~1 h). Read first per the brief:
`local/research/G20/REPORT.md` (all of it). No device (`adb` never
invoked), no renderer/behavior changes, no workaround, no submission,
ZERO clone edits.

Machine: same as G8-G20 (Apple M4, macOS 27.0 — no new installs).
No device contact of any kind.

Headline result: the filing package RE-VERIFIES with two corrections.
(1) 30 of 32 checkable S3d facts CONFIRM byte-for-byte against primary
receipts (hashes, offsets, BuildIds, counts, code gating); the S3c
attribution table re-derives EXACTLY (all three FNV hashes MATCH).
(2) CORRECTION A (storage, post-run): the G18 and G20 replayer binaries
on SSD are zero-damaged (G18: all 265,837,472 bytes zero; G20: first
~19.2 MiB zero) — their run-time identities survive via tombstone
BuildIds + the on-device pre-run matches, but the current files no
longer hash to the recorded shas. (3) CORRECTION B (wording): G20 S3d's
"3/3 same dispatch / same driver offsets" over-claims the G18 arm — G18
crashed on an async worker at pc 0x2a5fe8 (main thread concurrently in
dispatch_triangle_setup, crashing shader unnamed); the deterministic
pair is G19+G20 (bit-identical: same fault, same pc 0x472fc0, same
stack, async provably absent). The filing draft states the corrected
form. The named SPIR-V is VALID (spirv-val clean) and the differential
vs the two same-process survivors is sharp: the crasher is the ONLY one
using PhysicalStorageBuffer64 + workgroup barriers. The minimal NDK
repro BUILT host-side (13,792 B arm64, zero warnings, UNTESTED on
device — tabled honestly).

## 0. Byte caps (declared at session start) vs actuals

| class | cap | actual |
| --- | --- | --- |
| NEW SSD ps2x-g21 (repro package) | 10 MB | ~70 KB apparent; 15,360 KiB allocated (ExFAT 1 MiB clusters) PASS |
| SSD ps2x-g10..g20 + G14/G18/G20 build dirs (read-only) | 0 growth | all == pre-build snapshot exactly PASS |
| SSD clone (source) | ZERO edits | read-only greps/seds only; G18 + G20 hunks still present PASS |
| internal volume (`/`) | <=1 GB delta, no clones/builds | 3.2 Gi (pre-build) -> 3.0 Gi avail (other-lane churn); my residue = `/tmp/g21-*` ~250 KB text PASS |
| device | NO contact | `adb` never invoked PASS |
| network | none used | NDK + headers + SPIRV-Tools all local; no clones, no installs PASS |
| host build | `-j2` | single-TU clang compile; exit 0, zero warnings PASS |
| committed to git | text only | REPORT.md + FILING-DRAFT.txt + RUN.txt + g21-minrepro.c + g21-build.sh; no binaries, no .spv PASS |

SSD 241 -> 239 Gi avail across the session: this brief wrote ~15 MB
allocated (ps2x-g21); the remainder is other-lane. No code copied into
any project tree. No P-lane lease, no bytesize/WSL.
`COPYFILE_DISABLE=1` on all SSD steps.

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | All G20 S3d filing facts re-verify against primary receipts; the named SPIR-V disassembles host-side into a surveyed construct set with a valid differential vs the two survivors; a standalone repro is feasible to build host-side (or the exact obstacle is tabled); the package yields a complete paste-ready filing draft |
| observable signal | re-verification table (S2a) + disassembly census + survivor differential (S2b) + feasibility table + built-binary identity (S3a-S3b) + draft text (FILING-DRAFT.txt, S3c) |
| alternatives | (a) all confirm + repro builds => draft as-is; (b) discrepancies found => tabled with both values, draft adjusted, never silently fixed; (c) repro infeasible in-box => exact obstacle tabled, full-dump recipe as fallback |
| stop condition | no device work, no submission, no renderer/behavior changes, no workaround implementation, no new dumps; 6 h box; host builds SSD-only |
| outcome -> next action | numbers name the next single experiment (S4) |

Outcome: alternatives (a)+(b) — the repro built AND two discrepancies
were found, tabled, and folded into the draft (corrections A + B above).
No tuning loop was entered: verify -> survey -> assess -> build -> draft.

## 2. Task 1 — fact re-verification + SPIR-V survey (no build)

### 2a. S3d re-verification table (every fact vs its primary receipt)

Result key: CONFIRMED (receipt matches G20's value byte-for-byte) /
DISCREPANT (differs — both values shown) / NOT-RECEIPTED (report-only
claim with no durable file receipt — corroboration noted, never upgraded).

| # | S3d fact (G20 value) | primary receipt | result |
| --- | --- | --- | --- |
| 1 | device Odin3, Android 15 | tombstones x3 fingerprint `qti/sun/sun:15/.../eng.Odin3.20260204.171202` | CONFIRMED |
| 2 | serial `622c49b1` | ABSENT from all receipt files (grep 0 hits) | NOT-RECEIPTED (lab-session fact; draft uses fingerprint) |
| 3 | GPU Adreno (TM) 830 | logcats x3 `Found Vulkan GPU` | CONFIRMED |
| 4 | API 1.3.284 | logcats x3 | CONFIRMED |
| 5 | Driver 512.800.58 | logcats x3 | CONFIRMED |
| 6 | libllvm-qgl.so BuildId `28a2407fad77f0ce9e9cba56e729e521` (full) | tombstones x3 (frames + maps) | CONFIRMED |
| 7 | vulkan.adreno.so BuildId `d05dded928ad46330f433c26cf9b82ec` (full) | tombstones x3 | CONFIRMED |
| 8 | paraLLEl-GS `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` | clone HEAD | CONFIRMED |
| 9 | Granite `16e7395f6a48...` | `git log -1` full `16e7395f6a4858c1783dbf6f521f90b9d5f82ac5` | CONFIRMED |
| 10 | G20 replayer BuildId `a8402ce01b06ce8581c712d95ad120a401b878fe` (full) | tombstone_14 frames #22-#42 + maps | CONFIRMED |
| 11 | uninstrumented BuildId `0b01b27431fadd2fedbbe1f6e657e0f003eb665b` (full) | tombstones _12 + _13 | CONFIRMED |
| 12 | dump sha `154d9d85...2ad7e32` (full) + 11,537,377 B | ps2x-g13 file | CONFIRMED (both) |
| 13 | knob effective (G19/G20) | knob-fired LOGI in G19+G20 logcats; ABSENT in G18 (consistent: unknobbed) + knob at `gs_renderer.cpp:459` | CONFIRMED |
| 14 | `--iterations 2` | tombstone Cmdline x3 | CONFIRMED |
| 15 | exit 139 | NO file receipt | NOT-RECEIPTED (timestamps/slots corroborate; draft omits exit code, states SIGSEGV) |
| 16 | ~1 s wall | `Process uptime: 1s` x3 + logcat spans (G20: 08.335->08.465) | CONFIRMED |
| 17 | pipeline `7463dfd379df2855`, program `62cacea0b15bd6e7` | g20-logcat pre-create; `grep -c` == 1 (no post line) | CONFIRMED |
| 18 | shader hash `c61f1a8116a82d4b` | device logcat AND independent host FNV of SSD blob | CONFIRMED (double) |
| 19 | entry `main`, stage COMPUTE | code (`command_buffer.cpp:1091-1092`) AND disassembly `OpEntryPoint GLCompute ... "main"` | CONFIRMED (double) |
| 20 | specs 0, pSpecializationInfo NULL | logcat `specs 0` + code (`if (mask)` gates the pointer; mask==0 => NULL) | CONFIRMED |
| 21 | robustness off | logcat `robust 0` + code (pNext gated on the bit) | CONFIRMED |
| 22 | subgroup-size-control off | logcat `subgroup 0` + code (gated on `subgroup_control_size`) | CONFIRMED |
| 23 | DESCRIPTOR_BUFFER_BIT set | code (:1166-1169, iff `supports_descriptor_buffer`) + logcat ext enabled | CONFIRMED |
| 24 | no heap | logcats `heap` grep 0/0/0 + no heap ext in list + code (`if (heap)` gates bit + mapping pNext) | CONFIRMED |
| 25 | shader `sampler_feedback` | `slangmosh.hpp:19731` `request_program(spirv_bank + 148867, 20196)` | CONFIRMED |
| 26 | SPIR-V sha `b0a09fa0...a96ea1` (full) | SSD blob + independent re-extract (`cmp` IDENTICAL) | CONFIRMED |
| 27 | 5049 words / 20,196 B / magic `0x07230203` | blob bytes + header parse (v1.3, gen `0x000d000b`, bound 2461) | CONFIRMED |
| 28 | bank words [148867, 153916) + contiguity | `slangmosh.hpp:19729/19731/19733` (`qword_clear` 148555+312=148867; `ui_vert` @153916) | CONFIRMED |
| 29 | crash site `dispatch_texture_analysis+824`, post `record prims=17`, post `502d...` then `b710...` | logcat order (all three runs) + tombstone frames #27/#28 (G19/G20) | CONFIRMED |
| 30 | fault: sync create, SIGSEGV `0x0`, pc `+0x472fc0`, 18 llvm frames, `+1224`, single-threaded, main | tombstones _13 + _14 (each element grepped) | CONFIRMED (G19/G20) |
| 31 | G18 arm "same dispatch / same driver offsets" (3/3 wording) | tombstone_12: worker crash, pc `0x2a5fe8`; main in `dispatch_triangle_setup+1100`; shader UNNAMED. Same: fault `0x0`, libs + BuildIds, `+1224`/`+260`/`+1328` | DISCREPANT wording -> CORRECTED (note below; draft S6 uses corrected form) |
| 32 | G20 binary sha `8d5079cc...` / 265,838,568 B | current file sha `d7b60c7b...`: first 20,168,704 B zero, rest intact; size unchanged | DISCREPANT (post-run storage damage; identity survives via #10 + on-device pre-run match) |
| 33 | G18 binary sha `a62d8a2b...` / 265,837,472 B | current file sha `c568dc23...`: ALL bytes zero (nonzero_bytes=0) | DISCREPANT (same note; identity survives via #11) |
| 34 | G14 binary sha `c4cd63b4...` | current file: ELF magic, full sha match | CONFIRMED |
| 35 | S3c survivors: `upload[0]` `da8d2940f6faff6f`/5212w, `triangle_setup` `a7a0133ba031cebd`/8890w | independent re-extract + FNV: BOTH MATCH; bank sizes agree | CONFIRMED |
| 36 | 48 logcat lines; 3 pre-create groups; 2 stalled compiles; same order G18/G19 | `wc -l`=48; greps; ext-list sha `5ab93697...` IDENTICAL x3 | CONFIRMED |
| 37 | 0 "descriptor update template" lines | grep 0/0/0 | CONFIRMED |
| 38 | tombstone timestamps match runs (G20: 08:05:08.477, 12 ms after last logcat line) | tombstone headers vs logcat spans | CONFIRMED |

Score: 30 CONFIRMED + 2 DISCREPANT (both post-run/with-correction) +
2 NOT-RECEIPTED (kept out of / hedged in the draft). No S3d fact was
"fixed" — corrections A/B are new rows with both values.

S2a-note (correction B, precise): G18's own report never claimed
texture_analysis — it tables the worker crash + main thread in
triangle_setup. G20 S3d's determinism row compressed "3/3 O4" into
"same dispatch, same fault, same driver offsets"; strictly, the G18 arm
shares the fault class, the driver libraries + BuildIds, and the wrapper
offsets (+1224/+260/+1328), but NOT the pc (0x2a5fe8 vs 0x472fc0), NOT
the thread (worker vs main), and NOT the dispatch (worker has no dispatch
frame; main is in triangle_setup). The defensible filing claim is:
G19+G20 form a bit-for-bit deterministic pair (same thread, pc, stack,
single-threaded); G18 is a supporting async-path instance. The draft
(S6) states exactly this. G18's crashing shader is UNKNOWN and stays
unknown — the draft does not attribute sampler_feedback to run C.

S2a-note (correction A, damage scope): zero-damage is limited to the
two newest Android replayer binaries (G18 all-zero; G20 head-zeroed).
Intact: G14 + G15 Android binaries (ELF magic; G14 full-sha match),
G16 + G7-build macOS binaries (Mach-O magic), ALL ps2x-g* receipts
(shas parse), all build-dir sizes. mtimes preserved on damaged files
(07:44/08:01 = original builds), so this is not a rebuild — mechanism
unknown (ExFAT/flash-level suspected), tabled as observed fact only.

### 2b. SPIR-V survey (host tools only; G20 gap 3, first pass)

Disassembly: `spirv-dis` exit 0, 1176 lines (`/tmp/g21-sampler-feedback.dis`,
session-only; regenerable from the SSD blob). Validity: `spirv-val
--target-env spv1.3` exit 0, no errors — the crashing input is VALID
SPIR-V, so the driver crash is on valid input (filed as such).

Header: SPIR-V 1.3, generator "Google Shaderc over Glslang; 11", bound
2461. Capabilities: Shader, Int16, StorageBuffer16BitAccess,
UniformAndStorageBuffer16BitAccess, PhysicalStorageBufferAddresses +
ext `SPV_KHR_physical_storage_buffer`, memory model
PhysicalStorageBuffer64. Entry `main`, GLCompute, LocalSize 256x1x1,
built-ins LocalInvocationIndex + GlobalInvocationID.

Construct-family census (crashing shader):

| family | count | detail |
| --- | --- | --- |
| functions / calls | 1 / 0 | single-function shader |
| images / samplers | 0 | pure buffer compute; no `OpImage*`, no `OpSampledImage` |
| storage buffers / uniform buffers | 3 / 3 | set0 b4/b7/b8 SSBO (b4/b7 NonWritable), b9/b15 UBO; set1 b0 UBO (holds 3 physical pointers + 16-bit fields) |
| push constants | 12 B | struct uint,uint,int @0/4/8, COMPUTE stage |
| physical pointers | 54 mentions | dereferenced device addresses carried in set1 UBO |
| atomics | 7 | IAdd x3, Or x2, Exchange x2; Device scope, no semantics; workgroup + buffer targets |
| barriers | 2 | `OpControlBarrier` Workgroup/Workgroup/0x108 (workgroup-mem) |
| workgroup vars | 4 | uint + uint[256] + 2x v2int[256] (~5 KB shared) |
| control flow | 19 Phi, 29 Select, 1 Switch, 4 LoopMerge | structured, no `OpUnreachable` |
| 16-bit | short/ushort/v2ushort/v4short types | StorageBuffer + UniformAndStorageBuffer 16-bit access; 13 converts, 16 bitcasts |
| ExtInst (GLSL.std.450) | 18 | NClamp x4, FMin x3, FMax x3, Floor x2, SClamp x4, FindUMsb x2 |
| derivatives / subgroups / demote | 0 | none |
| runtime arrays | 5 types | inside SSBOs + behind physical pointers (layout-neutral) |

Differential vs the two same-process survivors (re-extracted + hashed
independently, S2a #35):

| shader | fate | memory model | atomics | images | barriers | workgroup vars | LocalSize |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `upload[0]` (502d) | success (14.3 ms) | Logical | 0 | 1 op | 0 | 0 | 8x8x1 |
| `triangle_setup` (b710) | success (29.9 ms) | Logical | 2 | 0 | 0 | 0 | 64x1x1 |
| `sampler_feedback` (7463) | CRASH | PhysicalStorageBuffer64 | 7 | 0 | 2 | 4 | 256x1x1 |

The crasher is the ONLY one of the three using physical storage buffer
addresses, workgroup shared memory + barriers, and LocalSize 256. This
is CORRELATION, not a proven trigger (5049 words hold many constructs)
— no over-claim. Delta-minimization order (named, NOT performed — out
of scope): (1) physical-addressing use (UBO-carried pointers + deref);
(2) workgroup/barrier structure at LocalSize 256; (3) 16-bit-in-UBO;
(4) device-scope atomics; (5) residual ALU/control flow.

## 3. Task 2 — repro feasibility + filing draft

### 3a. Feasibility table (assessed FIRST, per the brief)

| shape | needs | in-box? | verdict |
| --- | --- | --- | --- |
| Fossilize capture | Fossilize binaries (MISSING — not installed, not in homebrew) + a build from a network clone + an ON-DEVICE capture run | NO: device runs forbidden; no tooling present | REJECTED (capture needs the device by construction) |
| hand-synthesized Fossilize blob | .foz format knowledge + tooling to validate host-side (none present) | NO: unvalidatable without device/tooling | REJECTED (could not verify what it proves) |
| minimal hand-written app (bare `vkCreateComputePipelines`) | NDK (present, r30) + Vulkan headers (present, NDK sysroot) + aarch64 clang (present, API-35 wrapper) + shader interface (recovered S2b) | YES: single-file C, host cross-compile, no network, no device to BUILD | CHOSEN + BUILT (S3b) |
| full-dump repro (replayer + dump + command) | nothing to build (recipe exists); exact G18/G20 binaries damaged but recipe (source rev + dump sha + command) is intact | YES as documentation | FALLBACK (documented in draft S3 + RUN.txt; needs no build) |

Pick: minimal NDK app as the primary minimized repro (built, UNTESTED
on device — tabled honestly), full-dump recipe as the confirmed
fallback. Fossilize stays rejected unless a future brief allows device
capture + network builds.

### 3b. Repro build record (host-side, SSD only)

| item | observed |
| --- | --- |
| source | `g21-minrepro.c` (~300 lines C11, MIT, no deps beyond libvulkan via dlopen); committed as text |
| build | `aarch64-linux-android35-clang -O2 -Wall -Wextra -std=c11 -Wl,--build-id` (NDK r30, API 35) -> exit 0, ZERO warnings |
| binary | `ps2x-g21/g21-minrepro`, 13,792 B, ELF64 arm64, `BuildID 3341438c7671d7096008f6f82d8227aa8462c105` |
| binary sha | `7fcd59f5f9d51b0a3fd8dfc87c5f5329390b479f033b3d3cf84531629ea05137` |
| shader sidecar | `ps2x-g21/g21-sampler-feedback.spv`, 20,196 B, sha `b0a09fa0...` (== receipt) |
| package | 7 files (binary + source + spv + build script + build log + RUN.txt + FILING-DRAFT.txt), 15,360 KiB allocated |
| device status | UNTESTED — never run (no device in this brief); RUN.txt holds the exact future commands + expected SIGSEGV vs SURVIVED outcomes |

Fidelity vs Granite's crashing call (what matches, what differs):

| input | Granite (crasher) | minrepro | match? |
| --- | --- | --- | --- |
| SPIR-V bytes | bank blob (FNV `c61f...`) | sidecar, size+FNV self-checked | EXACT |
| stage / entry | COMPUTE / "main" | COMPUTE / "main" | EXACT |
| pSpecializationInfo | NULL | NULL | EXACT |
| flags / flags2 | DESCRIPTOR_BUFFER_BIT / _2_ twin | both set | EXACT |
| robustness / subgroup / heap pNext | absent | absent | EXACT |
| descriptor layout | 6 bindings + 12 B push (via reflection) | same 6 bindings + 12 B push (hand-built from disassembly) | EQUIVALENT |
| pipeline cache | valid, empty (fresh dir) | valid, empty | EQUIVALENT |
| device extensions | 15 (logcat list) | same 15 | EXACT |
| instance ext `VK_EXT_debug_utils` | enabled | omitted (cannot affect pipeline compile) | DELTA (tabled) |
| required features | int16 + sb16 + bda + descBuf | same set, support-verified | EQUIVALENT |
| thread / order | main thread, 3rd compile | lone thread, 1st compile | DELTA (if SURVIVED => order/state dependence — draft says so) |
| surrounding app state | full GS renderer | none | DELTA (minimization is the point) |

### 3c. Filing draft (complete text, NOT submitted)

`FILING-DRAFT.txt` (beside this report + SSD package copy): title,
S1 summary, S2 environment (fingerprint/driver/BuildIds), S3 repro
(minimal steps + full-app alternate + honest UNTESTED note), S4 exact
pipeline inputs, S5 crash signature, S6 determinism (CORRECTED form:
G19+G20 pair + G18 supporting — correction B), S7 expected-vs-actual,
S8 attachments (A1 spv, A2 minrepro pkg, A3 disassembly, T1-T3
tombstones+logcats, D dump-if-needed), S9 filer contact [BLANK —
needs the user]. No accounts, trackers, or email touched.

## 4. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| All G20 S3d filing facts re-verify; the SPIR-V surveys with a valid survivor differential; a standalone repro is feasible to build host-side (or the obstacle is tabled); the package yields a complete filing draft | CONFIRMED with two folded corrections: 30/32 facts byte-exact + full S3c re-derivation; correction A (G18/G20 binaries zero-damaged post-run — tombstone BuildIds carry the identities); correction B (G18 arm wording corrected to supporting-instance); SPIR-V valid with a sharp physical-addressing differential; minimal repro BUILT (UNTESTED tabled); draft COMPLETE and paste-ready. |

The ONE next action the numbers justify: **the USER's file-or-workaround
decision (unchanged owner — this brief did everything short of it)**.
Decision text for the user: (a) FILE — pick tracker + filer identity,
optionally run `RUN.txt` once on the lab device first (converts the
minrepro from UNTESTED to confirmed; if it SURVIVEs, file the full-app
recipe instead and report back); then paste `FILING-DRAFT.txt` + attach
A1/A2/A3/T1-T3 (D only if asked). (b) WORKAROUND — authorize a
workaround brief, sketched as: gate the `sampler_feedback` dispatch in
`GSRenderer::dispatch_texture_analysis` (`gs_renderer.cpp:3486`) behind
a knob (defer/skip/emulate-on-host — the brief would pick one),
validate on-device (no O4 + frame completes), pixel-diff vs G13 oracles
to quantify the rendering-behavior change, and record that the Adreno
filing stays open regardless. Queued behind it (not this action):
SPIR-V delta-minimization per S2b order; G18-hunk fix adoption (still
queued behind O4); O1/O3 writer naming; the G17 robustness filing.

No verdicts beyond the hypothesis. No submission, no workaround, no
device contact.

## 5. License / provenance receipts

| artifact | receipt |
| --- | --- |
| G21 additions | `g21-minrepro.c` (MIT, standalone — no third-party code) + `g21-build.sh` + `RUN.txt` + `FILING-DRAFT.txt` + REPORT.md; committed as text in both trees |
| sampler_feedback SPIR-V | LGPL bank bytes; blob lives on SSD ONLY (`ps2x-g20/` receipt + `ps2x-g21/` package copy); report/draft table its sha, never the bytes |
| disassembly / survivor blobs | `/tmp/g21-*` session-only (regenerable); not committed |
| minrepro binary | SSD `ps2x-g21/` ONLY (not in git); MIT source committed |
| NDK r30 / SPIRV-Tools v2026.3 | local toolchain use only (Apache-2.0) |
| logcats/tombstones | read-only receipt reads (no PII; `uid: 2000` shell) |
| clone | ZERO edits; read-only greps/seds; hunks untouched |

## 6. Exact commands

Mac (`/Users/bradrichardson/dev/ssx3` unless noted;
`COPYFILE_DISABLE=1` on SSD steps; `SSD="/Volumes/Extreme SSD"`):

```text
cat $SSD/ps2x-g20/g20-logcat.txt ; cat $SSD/ps2x-g20/g20-extract-spv.py  # S2a
shasum -a 256 $SSD/ps2x-g20/g20-sampler-feedback.spv ; xxd -l 16 <spv>    # #26-27
shasum -a 256 $SSD/ps2x-g13/g13-dump.gs                               # #12
shasum -a 256 <G14/G18/G20 replayer> ; xxd -l 8 <each>                 # #32-34
python3 -c <fnv1a64 over blob>                                        # #18 MATCH
python3 -c <nonzero scan of G18/G20 binaries>                         # 0 / 43090909
grep -E <fault frames> $SSD/ps2x-g*/g*tombstone-*.txt                 # #30-31
grep -c -E "kick_compilation|__async_func" <each tombstone>            # 32 / 0 / 0
sed -n <main-thread ranges> g18-tombstone-12.txt                      # triangle_setup+1100
grep -n <offsets/flags/entry/heap/spec> <clone files>                 # #19-25, #28 (read-only)
git -C <clone> rev-parse HEAD ; git -C <clone>/Granite log -1         # #8-9
spirv-dis <spv> -o /tmp/g21-sampler-feedback.dis                      # S2b exit 0
spirv-val --target-env spv1.3 <spv>                                   # S2b exit 0 (VALID)
python3 /tmp/g21-extract-survivors.py                                 # S2a #35 MATCH x3
grep -E <OpCapability|OpAtomic|OpVariable|...> /tmp/g21-*.dis         # S2b census
bash local/research/G21/g21-build.sh                                  # S3b exit 0, 0 warnings
shasum -a 256 $SSD/ps2x-g21/g21-minrepro ; file <it> ; llvm-readelf --notes <it>
du -sk <all prior dirs> ; df -h / $SSD                                # S0 (0 growth)
cp g21-build.sh RUN.txt FILING-DRAFT.txt $SSD/ps2x-g21/               # package
```

Device: NONE (no `adb` in this brief).

## 7. Gaps (what this brief could not do)

1. The report is NOT filed upstream (needs user identity/tracker — S4).
2. The minimal repro is UNTESTED on device (built + hash-verified only;
   RUN.txt is the exact next run; SURVIVED-vs-SIGSEGV decides filing arm).
3. No SPIR-V delta-minimization (order named S2b; trigger unproven —
   physical addressing is correlation, not cause).
4. Exit 139 + device serial lack file receipts (kept out of / hedged in
   the draft; tombstones carry the signal + fingerprint instead).
5. G18/G20 replayer binaries zero-damaged on SSD (rebuild recipe intact —
   G19 proved bit-identical rebuild; filing cites tombstone BuildIds).
6. O1/O3 writers, G18-hunk adoption, G17 filing: unchanged / queued.
7. No tombstones pulled (none generated — no device work); OS store
   untouched. `upstream/` + harness code untouched; no new dumps.

## 8. Receipt paths

- SSD package: `/Volumes/Extreme SSD/ps2x-g21/` (7 files: binary 13,792 B
  `7fcd59f5...` BuildID `3341438c...`, source, spv `b0a09fa0...`, build
  script + log, RUN.txt, FILING-DRAFT.txt).
- SSD receipts (read-only, 0 growth): `ps2x-g10/`-`ps2x-g20/` + all three
  Android build dirs + SSD clone (HEAD `3a66c19...`, Granite `16e7395...`).
- Session-only: `/tmp/g21-*` (disassembly x3, survivor .spv x3,
  extractor scripts).
- Commits: ps2xGS `[G21]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G21/` `[G21]` + same trailer (NOT pushed).

TAIL-RECEIPT: G21 report ends here. S3d re-verified 30/32 + 2 corrected,
SPIR-V valid with physical-addressing differential, minrepro built
(UNTESTED), filing draft COMPLETE, user decision next.
