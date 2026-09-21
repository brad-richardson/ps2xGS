# G19 report — fixed binary rebuilt BIT-IDENTICAL (as-found file was all-zero damage, erratum resolved), knob run: O4 PERSISTS on main's sync compile (async path eliminated ⇒ Adreno compiler bug on a specific compute shader)

Brief: G19 (this turn) — executes G18's §4: (1) VERIFY-then-rebuild the
fixed binary from the pinned tree + verified hunk (sha erratum: on-disk
file no longer matches the report — authorized, not a tree change);
(2) ONE bounded on-device run with `PGS_SKIP_COMPILATION_TASKS=1` + score.
Tables + hypothesis + next-action recommendation, no verdicts beyond the
hypothesis. Time box 6 h (used ~0.5 h). Read first per the brief:
`local/research/G18/REPORT.md` (all of it). No tree changes of any kind
(kept — the only clone touch is an mtime-only `touch` to force ninja,
§2b; `git status` + hunk bytes identical before/after).

Machine: same as G8–G18 (Apple M4, macOS 27.0 — no new installs) +
Odin3 (`622c49b1`, Android 15). Pins: paraLLEl-GS
`3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` + G14 S1–S3 shims + G11/G8+G10
hunks + G18 fix hunk (re-verified §2a; ZERO content delta) / rich dump
sha `154d9d85…` (re-verified host-side AND on-device) / knob at
`gs_renderer.cpp:459-464` (re-verified §2a, fired on-device §3a).

Headline result: erratum RESOLVED + branch RESOLVED. The as-found binary
(`c568dc23…`) is 265,837,472 ZERO BYTES — `c568dc23…` is computationally
verified as the sha256 of that many zeros (§2a); objects/archives in the
build dir are intact (valid ELF/`!<arch>` headers), so the damage is the
final link output only, post-run (G18's on-device sha match + tombstone
build-id predate it). The authorized rebuild reproduces the G18 binary
BIT-FOR-BIT: fresh sha `a62d8a2b…` == G18 report exactly, build-id
`0b01b274…` == G18 tombstone exactly (llvm-readelf receipt, §2b). The knob
run (exit 139, first draw reached, knob-fired LOGI present): O4 PERSISTS —
SIGSEGV fault `0x0` in `libllvm-qgl.so` inside
`vkCreateComputePipelines`, now on MAIN tid 5883 (pid==tid) via
`dispatch_texture_analysis`, with NO async worker in the process (only
thread besides main is `PGS-Waiter` parked in `pthread_cond_wait`; zero
`kick_compilation`/`__async_func` mentions in the tombstone). The async /
concurrent path is therefore ELIMINATED as a necessary condition: O4 is a
sync-path Adreno compiler bug on a specific compute pipeline (minimizing
facts §3c). Score: oracles ALL OK, device scanouts 0 → SCORE N/A. Retry
NOT used (O1/O3 never fired; tabled §3d). lldb NOT used (tombstone fully
classifies; tabled §3d).

## 0. Byte caps (declared at session start) vs actuals

| class | cap | actual |
| --- | --- | --- |
| G18 build dir REUSE (relink + 1 object, SSD) | 1 GB growth | 0 KiB — 4,482,048 KiB pre-rebuild == post-rebuild == G18-end exactly ✓ |
| SSD ps2x-g19 retrieval (logcat + tombstone_13) | 100 MB | ~178 KB apparent; 5,120 KiB allocated (ExFAT 1 MiB clusters) ✓ |
| SSD ps2x-g13…g18 (read-only) | 0 growth | 35,840 / 7,168 / 31,744 / 43,008 / 17,408 / 5,120 KiB == G18-end exactly ✓ |
| SSD G14 build dir (read-only) | 0 growth | 4,480,000 KiB exactly ✓ (others untouched, not re-read) |
| SSD clone (source) | 0 content edits | `git status` + hunk bytes identical pre/post (mtime-only touch, §2b) ✓ |
| internal volume (`/`) | ≤1 GB delta, no clones/builds | 4.5 Gi avail at start → 4.4 Gi at end (other-lane noise); my residue = `/tmp/g19-*` ~1.1 KB ✓ |
| device `/data/local/tmp/g19/` (transient) | 600 MB peak | peak ~278 MB (266 + 11.5); 0 remaining — dir removed, `mg/` only ✓ |
| `/tmp/g19-*` host logs | session-only | build log 208 B + hunk diff 918 B ✓ |
| committed to git | text only (report) | this file (ssx3 mirror + ps2xGS); no captures, no binaries, no build dirs ✓ |

SSD 255 → 252 Gi avail: this brief wrote ~5 MB allocated (retrieval) + 0
build growth; the remainder is other-lane. No code copied into any GPL
tree. No P-lane contention: no recomp boots/builds, no P-lane lease (no
fork writes at all), no bytesize/WSL use. Host build `-j2` (kept).
Tombstones: pulled ONLY `tombstone_13` (this run's, §3b); OS store
otherwise untouched. Slots keep rotating (`_13` new 07:44 after G18's
`_12`); `ls -lt` remains the oracle.

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | The O4 discriminator resolves: EITHER the knob run clears O4 (crash needs the async/concurrent path) OR O4 persists on main's sync compile (Adreno compiler bug on a specific shader) |
| observable signal | rebuild exit/sha/build-id + knob-fired receipt + ONE bounded run (exit/wall/frames/fate) + tombstone triage + diff score |
| alternatives | (a) O4 disappears ⇒ async-path issue (driver thread-safety vs async-path — table which the receipts support); (b) O4 persists on sync compile ⇒ Adreno compiler bug (table minimizing/filing facts); (c) O1/O3 fires instead of first draw ⇒ lottery note + ONE retry |
| stop condition | VERIFY-then-rebuild authorized, otherwise ZERO tree changes; TWO bounded device runs max (knob run + one retry iff O1/O3 fires instead of first draw); no tuning loop, no renderer changes, no new dumps, no source fix; no lldb unless the tombstone cannot triage |
| outcome → next action | numbers name the next single experiment (§4) |

Outcome: alternative (b) — O4 PERSISTS on main's sync compile with the
async worker provably absent (§3b–§3c), with a capture-first next action
queued (§4). No tuning loop was entered: one rebuild, one device run.

## 2. Task 1 — verify hunk + rebuild + verify binary (no device runs)

### 2a. Pin verification (pre-work — G18 §2a reproduced)

| item | observed |
| --- | --- |
| SSD clone HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (== pin) |
| tree status | `M CMakeLists.txt` (S1) + `m Granite` (S2) + `M tools/CMakeLists.txt` (S3) + `M gs/gs_interface.cpp` (G11) + `M tools/gs_dump_replayer.cpp` (G8+G10) + ExFAT `._` sidecars — G18 §2a exactly, NO other delta |
| Granite rev | `16e7395f6a48` (== pin); Granite diff: `application/platforms/CMakeLists.txt` S2 + `util/timer.cpp` G7 + `vulkan/shader.cpp` G18 — G18 §2a exactly |
| G18 hunk bytes | `git -C Granite diff vulkan/shader.cpp` is byte-identical to `g18-fix.diff` (`diff` exit 0, HUNK_MATCH) — verified BEFORE any build |
| rich dump (host) | 11,537,377 B, sha `154d9d8577a210fb794b29a048ee2cf08053f87fe220bb2bf975933a02ad7e32` full-match (device re-verified §3a) |
| G14 binary (baseline) | 265,837,472 B, sha `c4cd63b4859ee72480abce6790904f41aa95374eaec3bd39395bb94a42d79db7` (G14 build dir untouched) |
| knob file/lines | `PGS_SKIP_COMPILATION_TASKS` at `gs/gs_renderer.cpp:459` (`getenv`), early-return + `LOGI("Skipping precompilation…")` at `:459-464` — G18 §4 citation confirmed |
| NDK / cmake | r30 `30.0.16248370` (`/opt/homebrew/share/android-ndk`, from G18 CMakeCache toolchain path); ninja `/opt/homebrew/bin/ninja` |
| device (read-only pre-check) | `622c49b1`, Odin3, Android 15; `/data` 29 G free; `/data/local/tmp/` == `mg/` only; newest tombstone `_12` 06:51 (G18's run) |
| driver | Adreno (TM) 830, API 1.3.284, Driver 512.800.58 (logcat §3a; == G14–G18) |

As-found binary (the erratum — tabled, then explained):

| item | observed |
| --- | --- |
| path/size/mtime | `parallel-gs-g18-android-build/tools/parallel-gs-replayer`, 265,837,472 B (same size as G18 report), Sep 21 06:50 |
| as-found sha | `c568dc238140de8d371575eb240cfee210145a8b761e56a00e049c6879f4aafa` (== erratum expectation `c568dc23…`) |
| content | ALL 265,837,472 bytes are ZERO (`xxd` head/mid/tail all-zero; python first-nonzero scan: ALL ZERO; `file` says `data`, no ELF magic) |
| zero-hash proof | sha256 of 265,837,472 zero bytes computed independently == `c568dc23…` exactly ⇒ the sha IS the zero-fill signature, not a relink |
| damage scope | sampled 8× `.o` (valid `7f45 4c46` ELF) + 6× `.a` (valid `!<arch>`) — build dir is otherwise INTACT; damage is the final link output only |
| ninja state | `ninja -n -d explain`: "no work to do" (mtime-based, content-blind — the zero-fill postdates the link) |
| timing bound | G18 pushed the binary then verified on-device sha `a62d8a2b…` (§3a) and the tombstone records build-id `0b01b274…`; the zero-fill (mtime 06:50, between G18's push and this brief) postdates the good run — G18's run evidence stands, same anomaly class as G15's link mismatch |

### 2b. Rebuild record (G18 build dir REUSED — authorized, 0 growth)

| item | observed |
| --- | --- |
| force mechanism | mtime-only `touch Granite/vulkan/shader.cpp` (ZERO content change: `git status` + hunk-vs-`g18-fix.diff` byte comparison identical before/after — §2a HUNK_MATCH re-confirmed post-touch) |
| ninja plan | `[1/3] shader.cpp.o` compile → `[2/3] libgranite-vulkan.a` → `[3/3] parallel-gs-replayer` relink (dry-run receipt; nothing else dirty) |
| build | `cmake --build … --target parallel-gs-replayer -j2` → exit 0, `[3/3] Linking CXX executable tools/parallel-gs-replayer`, 0 warnings/errors; log `/tmp/g19-build.log` (208 B) |
| fix presence | rebuild log contains the `shader.cpp` compile (grep count 1) ✓ |
| fresh binary | 265,837,472 B, valid ELF (`7f45 4c46`), sha `a62d8a2b3380c2bf62087728c793ed189a640f7450cda0c945e8b10d223bfe27` |
| fresh build-id | `0b01b27431fadd2fedbbe1f6e657e0f003eb665b` (NDK `llvm-readelf --notes`, `.note.gnu.build-id`) |
| sha ≠ G14 | `a62d8a2b…` ≠ `c4cd63b4…` ✓ |
| build dir | 4,482,048 KiB post-rebuild == pre-rebuild exactly (0 growth ✓); G14 dir == 4,480,000 KiB (untouched ✓) |

Sha table (the brief's three-value table + build-id):

| value | sha / build-id | standing |
| --- | --- | --- |
| G18 report | `a62d8a2b…e27` | REPRODUCED bit-for-bit by the fresh build |
| as-found on-disk | `c568dc23…afa` | explained: all-zero fill (zero-hash proof §2a), NOT a link — discarded |
| fresh rebuild | `a62d8a2b…e27` | == report exactly; build-id `0b01b274…665b` == G18 tombstone exactly |
| conclusion | — | link is DETERMINISTIC (no third value, no nondeterminism to table); the fix hunk + behavior are the identity AND the bytes agree — the strongest possible rebuild receipt |

## 3. Task 2 — ONE bounded knob run + branch (Odin)

### 3a. Run table (ONE run — retry not used, §3d)

| item | observed |
| --- | --- |
| staging | `/data/local/tmp/g19/` ONLY; dump + FRESH binary pushed; on-device shas FULL-match host (`154d9d85…` / `a62d8a2b…`); logcat cleared, Granite empty before |
| command | `PGS_SKIP_COMPILATION_TASKS=1 timeout -s KILL 280 ./parallel-gs-replayer /data/local/tmp/g19/g13-dump.gs --iterations 2` (G18 shape + knob prefix) |
| exit / wall | **139** (SIGSEGV) / ~1 s wall (`date` 07:44:34→35; logcat spans 34.788→34.918, ~130 ms) |
| knob-fired receipt | logcat line `Skipping precompilation of shaders. May stutter way more.` (the `:462` LOGI) + tombstone shows NO async worker (§3b) ⇒ knob fully effective |
| logcat | 37 lines: init + extension list (incl. `VK_EXT_descriptor_buffer`, no heap) + `Running frame` + G11/G10 first-draw lines + TWO `Stalled compile (compute, …success: yes)` (`502d5366ce6695c1`, then `b7109780b562b0fe` — same two hashes, same order as G18) + knob line; 0 "Failed to create descriptor update template" |
| progress vs G18 | identical shape through the second stalled compile (first draw reached, O2's site passed again — no `0xf0` fault) |
| fate | NOT O1 (init-trim null-`table`, G15 — this dies post-first-draw in the driver compiler); NOT O2 (no fault-`0xf0`); NOT O3 (SIGABRT — this is SIGSEGV) → O4 signature again, branch-resolved §3c |
| device outputs | 0 scanouts (dir == inputs only); tombstone_13 written 07:44 (pulled, §3b); device dir removed after (`mg/` only ✓) |

### 3b. Tombstone_13 triage (pid 5883 — O4 on the sync path)

| slot | fact |
| --- | --- |
| crashing thread | tid **5883** == pid ⇒ MAIN thread (G18's crash was async worker tid 20753; this run has no such thread) |
| fault | SIGSEGV fault `0x0`, null-pointer dereference, pc in `libllvm-qgl.so` (BuildId `28a2407f…`) ×18 frames → `vulkan.adreno.so` (BuildId `d05dded9…`) → `qglinternal::vkCreateComputePipelines+1224` (same +1224 offset as G18) |
| app stack | `PipelineCache::create_pipeline+260` ← `build_compute_pipeline+1328` ← `flush_compute_pipeline` ← `flush_compute_state` ← `dispatch` ← **`dispatch_texture_analysis+824`** ← `flush_rendering+844` ← `flush_render_pass` ← `flush` ← `PageTracker::flush_render_pass` ← `check_frame_buffer_state` ← `drawing_kick_append` ← `drawing_kick` ← `a_d_XYZ2` ← `write_register` ← `packed_ADONLY` ← `gif_transfer` ← `iterate_until_vsync` ← `main` |
| binary identity | replayer BuildId in tombstone `0b01b274…` == fresh binary == G18 tombstone ⇒ the tested binary is proven |
| other threads | exactly ONE: tid 5884 `PGS-Waiter`, parked in `pthread_cond_wait` ← `GSRenderer` ctor lambda (benign waiter, no driver calls) |
| async absence | ZERO `kick_compilation` / `__async_func` mentions in the whole tombstone (grep count 0) + knob-fired LOGI (§3a) ⇒ the async precompilation path did not exist in this process |
| vs G18 | same driver/compiler wall (fault-`0x0` in `libllvm-qgl.so` @ `vkCreateComputePipelines+1224`), entered synchronously from a first-draw compute dispatch; G18's main was concurrently in the same call from `dispatch_triangle_setup` — the wall moves with the dispatch, not the thread |

### 3c. Branch resolution (G18 §4 branch — receipts decide)

| branch | verdict | receipts |
| --- | --- | --- |
| O4 disappears ⇒ needs async/concurrent path | REJECTED | exit 139 with the identical O4 signature (§3b) while the async path provably did not exist (knob LOGI + zero async mentions + waiter-only second thread) |
| driver thread-safety vs async-path issue | MOOT | no concurrency remains to blame — single compiling thread (main), second thread in cond-wait outside the driver |
| O4 persists on sync compile ⇒ Adreno compiler bug on a specific shader | TAKEN | main-thread-only `vkCreateComputePipelines` SIGSEGV fault-`0x0` inside `libllvm-qgl.so`; app inputs are a fixed dump + deterministic binary ⇒ the crashing input is a specific compute pipeline |

Minimizing facts for the filing package (tabled, not filed — no upstream contact):

| slot | fact |
| --- | --- |
| device/driver | Odin3, Android 15, Adreno (TM) 830, API 1.3.284, Driver 512.800.58; `libllvm-qgl.so` BuildId `28a2407fad77f0ce9e9cba56e729e521`, `vulkan.adreno.so` BuildId `d05dded928ad46330f433c26cf9b82ec` |
| repro | dump sha `154d9d85…`, binary build-id `0b01b274…`, `PGS_SKIP_COMPILATION_TASKS=1`, `--iterations 2` → exit 139 in ~1 s, tombstone_13 |
| crash site | `dispatch_texture_analysis` compute `dispatch` during first draw (post `record prims=17`, post sync compiles `502d…` then `b710…` — the crashing pipeline is the NEXT compute compile after `b7109780b562b0fe`) |
| fault shape | sync `vkCreateComputePipelines` → SIGSEGV fault `0x0` null deref, pc `libllvm-qgl.so+0x472fc0`, 18 llvm frames, single-threaded compile |
| still missing | the crashing pipeline's hash + SPIR-V (no per-pipeline logging exists — the §4 capture run provides it) |

### 3d. Retry + lldb decisions (tabled, both NO)

| decision | verdict | rationale |
| --- | --- | --- |
| ONE retry | NOT USED | Brief permits it iff "O1/O3 fires instead of first draw" — neither fired; the run reached first draw, passed O2's site, and resolved the branch with a classifiable signal |
| lldb triage | NOT USED | Tombstone_13 fully classifies O4-on-sync (fault/regs/main stack/thread census/async absence); no debugger question is open that lldb could answer within this brief |

### 3e. Score vs G13 oracles (`g14-diff.py`, zero new tooling)

`g14-diff.py ps2x-g13 ps2x-g19`: oracles **ALL OK** (8/8 pixel-shas);
device PPMs in `ps2x-g19`: **0** → `SCORE: N/A`. (Vintage reason string
"G14 run crashed…" — the scored fact is 0 scanouts: O4 dies in first draw
before any vsync completes.) No diff table is fabricable and none is faked.

## 4. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| The O4 discriminator resolves: either the knob run clears O4 (async-path crash) or O4 persists on main's sync compile (Adreno compiler bug on a specific shader) | **RESOLVED toward the second arm**: O4 PERSISTS — exit 139, same fault-`0x0`-in-`libllvm-qgl.so` @ `vkCreateComputePipelines+1224` signature, now on main's synchronous compile from `dispatch_texture_analysis` with the async worker provably absent (knob-fired LOGI + zero async mentions + waiter-only thread census). Separately RESOLVED: the sha erratum — the as-found file was all-zero post-run damage, and the authorized rebuild reproduces the G18 binary bit-for-bit (sha + build-id both exact). |

The ONE next action the numbers justify: **a capture-first O4-minimize
brief — a logging-only instrumented build that records the crashing
compute pipeline's identity (Granite pipeline hash + SPIR-V) at the
`dispatch_texture_analysis` dispatch, ONE bounded knob-env run to capture
it, then assemble the Adreno filing package** (device/driver/build-ids +
repro + fault shape §3c + shader). Rationale: the branch is settled
(sync-path compiler bug — no further knob or thread-safety experiment can
move it); the filing package is complete EXCEPT the shader input, which
only a capture run can name; logging-only keeps the compiler input
bit-identical so the capture does not perturb the crash. Queued behind it
(not this action): fix adoption (G18 hunk still queued behind O4);
O1/O3 writer naming (still open, no longer first-draw-blocking); the G17
Adreno robustness filing (unchanged); any workaround (defer/skip the
crashing dispatch — premature before the shader is named).

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.

## 5. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c19…` (LGPL-3.0+ per G7 §4 — cited, not re-read) |
| Granite rev | `16e7395f…` (MIT per G7 §4 — cited); rebuild + read sites are Granite framework code |
| G14 shims S1–S3 + G7/G8/G10/G11/G18 hunks | untouched, still uncommitted in SSD clone only (mtime touch is content-null) |
| G19 addition | NONE — zero source changes (verify + rebuild + knob-env run only) |
| NDK r30 | `30.0.16248370` (Apache-2.0 toolchain) |
| logcat/tombstone | OS/debugger-produced run receipts of our own binary (no PII; `uid: 2000` shell) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 6. Exact commands

Mac (`/Users/bradrichardson/dev/ssx3` unless noted;
`COPYFILE_DISABLE=1` on SSD steps):

```text
git -C "<ssd>/parallel-gs-g7" rev-parse HEAD ; status --short   # §2a
git -C <clone>/Granite diff vulkan/shader.cpp | diff <ssx3>/local/research/G18/g18-fix.diff -  # HUNK_MATCH
shasum -a 256 <g18-binary> ; xxd -l 64 <g18-binary>             # §2a as-found (all-zero)
python3 -c <sha256-of-265837472-zeros>                          # == c568dc23… (zero-hash proof)
find <g18-build> -name '*.o'/-name '*.a' | xxd heads            # damage scope (intact)
ninja -C <g18-build> -n -d explain tools/parallel-gs-replayer   # "no work to do"
touch <clone>/Granite/vulkan/shader.cpp                         # mtime-only (§2b)
cmake --build <g18-build> --target parallel-gs-replayer -j2     # exit 0 [3/3]
shasum -a 256 <fresh-binary> ; llvm-readelf --notes <fresh-binary>  # a62d8a2b… + 0b01b274…
du -sk <dirs> ; df -h / <ssd>                                   # §0
python3 <ssx3>/local/research/G14/g14-diff.py <ps2x-g13> <ps2x-g19>  # §3e (oracles OK, N/A)
```

adb (`-s 622c49b1` throughout; `/data/local/tmp/g19/` ONLY; `mg/` never touched):

```text
shell 'ls /data/local/tmp/ ; ls -lt /data/tombstones/ | head'   # pre-check (mg/ only, _12 newest)
shell 'rm -rf /data/local/tmp/g19 && mkdir -p /data/local/tmp/g19'
push <dump> $G19DIR/g13-dump.gs ; push <fresh-binary> $G19DIR/parallel-gs-replayer
shell 'sha256sum g13-dump.gs parallel-gs-replayer'               # both match host
logcat -c ; logcat -d -s Granite:V | tail -2                     # before (empty)
shell 'cd $G19DIR && date +%s; PGS_SKIP_COMPILATION_TASKS=1 timeout -s KILL 280 ./parallel-gs-replayer $G19DIR/g13-dump.gs --iterations 2; echo RUN_EXIT=$?; date +%s'  # 139
logcat -d -s Granite:V > <ssd>/ps2x-g19/g19-logcat.txt           # 37 lines
shell 'ls -la $G19DIR/ ; ls -lt /data/tombstones/ | head -6'     # 0 scanouts; _13 new 07:44
pull /data/tombstones/tombstone_13 <ssd>/ps2x-g19/g19-tombstone-13.txt
shell 'rm -rf /data/local/tmp/g19 && ls /data/local/tmp/'        # DEVICE_CLEAN (mg/ only)
```

## 7. Gaps (what this brief could not do)

1. The crashing pipeline's hash + SPIR-V are UNCAPTURED (no per-pipeline
   logging exists; the §4 capture run names them — the ONLY filing-package
   gap).
2. O4 determinism-vs-lottery stays single-sample per arm (G18 unknobbed 1/1
   O4; G19 knobbed 1/1 O4-on-sync — consistent, not yet a rate).
3. Whether the G18 fix changes ANY behavior on raw==derived devices: no
   (predicate agrees there by construction) — but unverified by a run on
   such a device.
4. Zero on-device scanouts (again) → no pixel score. `g14-diff.py`
   stands ready (oracles ALL OK re-verify passes through it).
5. O1's null-write site and O3's corruption writer stay UNNAMED (init-trim
   lottery members; neither fired this run).
6. No tombstones pulled except `_13` (this run's; OS store otherwise untouched).
7. The zero-fill's cause/mechanism is UNKNOWN (post-run, link-output-only;
   same anomaly class as G15's link mismatch — tabled, run evidence stands).
8. `upstream/` and ps2xGS harness code untouched; no new dumps (per the
   stop rule). No upstream contact (nothing filed).

## 8. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…`
  (G7/G8/G10/G11 hunks + G14 S1–S3 + G18 fix hunk — all uncommitted,
  content-identical to G18-end).
- G14 build: `/Volumes/Extreme SSD/parallel-gs-g14-android-build/` (pristine).
- G18 build: `/Volumes/Extreme SSD/parallel-gs-g18-android-build/`,
  `tools/parallel-gs-replayer` (265,837,472 B, `a62d8a2b…`, build-id
  `0b01b274…` — fresh rebuild, bit-identical to G18).
- SSD: `/Volumes/Extreme SSD/ps2x-g19/` (logcat + tombstone_13);
  `ps2x-g13/`–`ps2x-g18/` pristine.
- Tools: `/tmp/g19-build.log`, `/tmp/g19-hunk-actual.diff` (session-only).
- Commits: ps2xGS `[G19]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G19/` `[G19]` + same trailer (NOT pushed).
