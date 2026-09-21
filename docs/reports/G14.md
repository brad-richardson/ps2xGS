# G14 report — Odin on-device replay attempt: init gate PASSES, replay BLOCKED on a Granite trim() null deref

Brief: G14 (this turn) — executes G13's §5: build `parallel-gs-replayer`
for Android arm64 (G7 §5 recipe), push it + the RICH dump (`g13-dump.gs`)
to the Odin, run ONE bounded on-device replay, pull scanout(s), score vs
the dual-agreed G13 oracles. Tables + hypothesis + next-action
recommendation, no verdicts beyond the hypothesis. Time box 6 h (used
~1 h). Read first per the brief: `docs/reports/G13.md` (all) +
`docs/reports/G7.md` §5. The emulator-vs-emulator comparison stays CLOSED
(cited, not re-run).

Machine: same as G8–G13 (Apple M4, macOS 27.0 — no new installs) + Odin3
(`622c49b1`, Android 15). Pins: paraLLEl-GS
`3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (G11-end state re-verified, §2a)
+ 3 ANDROID-gated cmake shims (new, §2b) / NDK r30 `30.0.16248370`
(re-confirmed) / rich dump sha `154d9d85…` (re-verified on host AND
on-device).

Headline result: alternative (c) for the replay leg — BLOCKED, with the
exact obstacle tabled — and a positive close of the G7-OPEN init leg. The
Android build works (exit 0, 265,837,472 B aarch64 binary, system libs
only). The ONE bounded run initialized Vulkan on the Adreno 830 (API
1.3.284, driver 512.800.58), PASSED the project's own `GSRenderer::init`
gate (no `Minimum requirements` line; slab lines printed: 1792 MiB slab /
268 MiB per-flush), then SIGSEGV'd <1 s in, inside Granite framework code
— `CommandPool::trim()` (`command_pool.cpp:171`, `table->vkResetCommandPool`
load with `table == 0`, pool handle non-null) — reached via
`GSInterface::init → set_super_sampling_rate → invalidate_super_sampling_state
→ wait_idle → trim_command_pools`, BEFORE the dump was opened (0 `Running
frame` lines, 0 PPMs, 25 logcat lines). Same source exits 0 on
macOS/MoltenVK (G13: 18 frames). The null's origin is NOT visible
statically (table is assigned only at construction/move, never nulled —
§2e audit); catching it needs a device-side debugger, which is the
prescribed next experiment. Score: oracles 8/8 re-verified, device
scanouts 0 files → SCORE N/A (no pixels diverged — none were produced).

## 0. Byte caps (declared at build start) vs actuals

| class | cap | actual |
| --- | --- | --- |
| Android build dir (SSD, new `parallel-gs-g14-android-build`) | 20 GB apparent | 4,480,000 KiB allocated (ExFAT 1 MiB clusters); binary 265,837,472 B + 840 `.o` + archives; apparent ≤ allocated ≪ cap ✓ |
| SSD ps2x-g14 retrieval (tombstone + logcat + run-stdout) | 100 MB | ~208 KB apparent (205,623 + 2,304 + 35 B); 7,168 KiB allocated ✓ |
| SSD ps2x-g13 (oracles, read-only) | 0 growth | 35,840 KiB == G13-end exactly; 8/8 pixel-shas re-verified ✓ |
| SSD clone (source) | 0 + cmake text shims | 3 shim hunks only (§2b); mac binary 51,879,240 B + timestamp untouched; no mac rebuild ✓ |
| SSD mac build dir | 0 growth | 3,808,256 KiB == pre-work ✓ |
| internal volume (`/`) | ≤1 GB delta, no clones/builds | 11 Gi avail before AND after (0 delta); no installs ✓ |
| device `/data/local/tmp/g14/` (transient) | 500 MB | peak ~278 MB (binary 254 + dump 11.5); 0 remaining — dir removed, `mg/` untouched ✓ |
| `/tmp/g14-*` host scripts/logs | 50 MB, session-only | ~282 KB ✓ |
| committed to git | text only (report + scripts) | this file (ps2xGS) + REPORT.md + 4 scripts (ssx3 mirror); no captures, no binaries, no build dirs ✓ |

SSD 419 → 403 Gi avail: this brief wrote ~4.5 GB allocated; the remainder
is other-lane (T/I/E lanes active — same honest accounting as G13 §0).
No code copied into any GPL tree. No P-lane contention: no recomp
boots/builds, no P-lane lease (device-side replay only), no fork writes,
no bytesize/WSL use at all this brief. Host builds `-j2` (one
`parallel-gs-replayer` target). One device-side tombstone (`tombstone_06`,
205 KB) remains in the OS-owned `/data/tombstones/` (auto-rotated store,
not removable as shell — tabled, §6.6).

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | On-device replay produces scanouts matching the dual-agreed emulator scanouts within a tabled tolerance |
| observable signal | build receipts + run exit/wall/heap + pulled scanout bytes + per-pair diff table vs G13 PPM/PNG oracles |
| alternatives | (a) match → table + ONE next action (adoption input); (b) mismatch → table the divergence + the ONE experiment it prescribes, stop; (c) build or run blocked → table the exact obstacle + recipe, stop |
| stop condition | ONE bounded on-device run — no tuning loop, no renderer changes, no new dumps |
| outcome → next action | numbers name the next single experiment (§3) |

Outcome: alternative (c) — run blocked at O1 (Granite trim() SIGSEGV
before first iterate; 0 scanouts). The init half of the observable is
POSITIVE (gate passed on-device with the project's own init — the
G7-OPEN leg closes). No tuning loop was entered: no source fix, no
rebuild-after-crash, no second run. Backtrace/disassembly/audit below are
receipt reads from the completed run, not new runs.

## 2. Task 1 — build + ONE bounded on-device run + score (rich dump only)

### 2a. Reuse verification (pre-work)

| item | observed |
| --- | --- |
| SSD clone HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (== G11/G13 pin) |
| tree status (pre-work) | `M gs/gs_interface.cpp` (G11 H1–H4, LOGI-only) + `M tools/gs_dump_replayer.cpp` (G8+G10 PPM hook) + `m Granite` (G7 `__APPLE__` timer shim, +11) + ExFAT `._` sidecars — G11-end state exactly |
| mac replayer binary (pre-work) | 51,879,240 B (== G13 §2a; untouched at end, §0) |
| NDK | r30 `30.0.16248370`, `source.properties` re-confirmed; toolchain file present; prebuilt `darwin-x86_64` |
| host tools | cmake 4.4.3, ninja 1.13.2, no `VULKAN_SDK` in env (correct for Android configure) |
| rich dump (host) | 11,537,377 B, sha `154d9d8577a210fb…ad7e32` full-match to G13 §2d |
| G13 oracles (host) | 7 PNG md5s match §3c exactly; 8 PPM pixel-shas match §3c exactly (the §3c "sha12" values are sha256-of-pixel-data, header-stripped — verified, not assumed) |
| device | `622c49b1`, Odin3, CQ8725S/qcom/sun, Android 15 (SDK 35), `/data` 28 G free, `/data/local/tmp/` holds only `mg/` |

Hook audit (why the mac-tested source runs as-is on Android): the G11
hooks are LOGI-only (no file writes, no behavior change); the G8+G10 hook
writes PPMs to `dump_path + suffix` via portable `fopen/fwrite` (lands
next to the dump on-device); the G7 timer shim is `__APPLE__`-gated
(Android uses stock `clock_nanosleep`). No source changes needed for the
replay path — only the 3 build-system shims in §2b.

### 2b. Android build (G7 §5 recipe + 3 ANDROID-gated cmake shims)

Shim S1+S2 (configure blocker): Granite `application/platforms/` takes
`if (ANDROID)` FIRST — unconditionally demanding AGDK prefab packages
(`game-activity`, `games-controller`, `games-frame-pacing`), which exist
nowhere on this machine (no Android SDK; NDK-only host). The offline
replayer links `granite-base + granite-application-global-init +
parallel-gs + parallel-gs-dump` — never `granite-platform`/SDL — so the
requirement is configure-only. S1: parallel-gs top-level forces
`GRANITE_PLATFORM=null` when `ANDROID` (else `SDL`, unchanged). S2:
platforms `if (ANDROID)` → `if (ANDROID AND NOT (null|headless))`, so
Android+null falls through to the stock `application_null.cpp` stub.
Shim S3 (link blocker): first build reached final link in 158 s then
failed on `undefined symbol: __android_log_print` (Granite logging on
Android targets logcat, tag `Granite` — `logging.hpp:65-69`); S3 links
`log` to the replayer target, ANDROID-gated. All 3 shims are cmake-only,
ANDROID-gated, zero-effect on desktop builds by inspection (no mac
rebuild; mac binary bit-untouched, §0).

| item | value |
| --- | --- |
| configure flags | `-DCMAKE_TOOLCHAIN_FILE=…/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-35` (G7 §5 verbatim; no `VULKAN_SDK`, no build-type — matches mac config) |
| configure | exit 0, ~21 s (`Configuring done (10.2s)`), Clang 21.0.0, `Processor: aarch64` |
| build | `cmake --build … --target parallel-gs-replayer -j2` → exit 0 (attempt 1: link fail S3 in 158 s; relink after S3: 9 s — far under G7 §5's 30–90 min estimate) |
| binary | `tools/parallel-gs-replayer`, 265,837,472 B, sha `c4cd63b4859ee724…d79db7`, ELF aarch64 PIE, `/system/bin/linker64`, unstripped (`-g`); 840 `.o` |
| NEEDED | `libdl, libm, liblog, libc` — ALL system (libc++ static via `-static-libstdc++`; Vulkan via volk `dlopen`) → single-file push, no sidecar `.so` |

### 2c. Device reconfirm (G7 E2 shape, abbreviated)

| item | value |
| --- | --- |
| model / SoC / OS | Odin3 / CQ8725S / Android 15, SDK 35 |
| Vulkan (project's own init, logcat) | `Found Vulkan GPU: Adreno (TM) 830`, API 1.3.284, Driver 512.800.58 |
| Vulkan (OS oracle, `cmd gpu vkjson`) | driverID 8, Adreno Vulkan Driver, build `a9ee82cd83`, 10/08/25; pkg `com.qualcomm.qti.gpudrivers.sun.api35` |
| init gate (G7 §2b bar) | `Minimum requirements` ×0; `Using image slab size of 1792 MiB` + `max allocated image memory per flush of 268 MiB` (heap-scaled vs mac's 4041/606 — expected on a 12 GB-class device) → **GATE PASSES on-device** |
| device extensions on | external_semaphore_fd, external_memory_fd, calibrated_timestamps, conservative_rasterization, push_descriptor, index_type_uint8, maintenance5, astc_decode_mode, image_compression_control(+swapchain), ray_query, acceleration_structure, deferred_host_operations, descriptor_buffer, shader_image_atomic_int64; `UMA-style device detected`; `Disabling pipeline cache control` (warn, benign) |

### 2d. Push + ONE bounded run + pull

Clean `/data/local/tmp/g14/`; push 184.7 MB/s (dump) / 124.5 MB/s
(binary); on-device shas match host exactly (dump `154d9d85…`, binary
`c4cd63b4…`); logcat before-snapshot: 0 `Granite` lines (clean baseline).
Run: `timeout -s KILL 280 ./parallel-gs-replayer
/data/local/tmp/g14/g13-dump.gs --iterations 2` (G13-identical flags, dump
first per the G7 argv-note).

| item | value |
| --- | --- |
| exit | **139 (SIGSEGV)**, same-second (<1 s wall; wall cap untouched) |
| stdout/stderr | 3 lines (epoch, `RUN_EXIT=139`, epoch) — all Granite output goes to logcat on Android by design |
| logcat (`Granite` tag) | 25 lines total, ends at the slab-size pair (§2c); 0 `Running frame` → crash BEFORE first iterate, BEFORE dump open (dump bytes never touched) |
| device outputs | 0 PPMs (dir held only binary + dump post-run) |
| pull | `tombstone_06` (205,623 B) + logcat + run-stdout → SSD `ps2x-g14/` |
| cleanup | binary + dump removed, `g14/` rmdir'd; `/data/local/tmp/` == `mg/` only ✓ |

Threads at crash (tombstone): main (faulting, 9 frames) + `PGS-Waiter`
(parked on condvar, correct) + 4 threads inside Qualcomm
`libllvm-qgl.so` (driver compiler workers — device/driver fully alive).

### 2e. Obstacle O1 (exact): `CommandPool::trim()` null-`table` SIGSEGV

Tombstone `tombstone_06` (01:34:58, pid 19724): `signal 11 (SIGSEGV),
code 1 (SEGV_MAPERR), fault addr 0x380`, `null pointer dereference`.

```text
#00  CommandPool::trim()+48
#01  PerFrame::trim_command_pools()+164
#02  wait_idle_nolock()+904
#03  wait_idle()+100
#04  GSRenderer::invalidate_super_sampling_state()+516   (gs_renderer.cpp:341 wait_idle, after VRAM-clear submit + slab flush)
#05  GSInterface::set_super_sampling_rate()+516
#06  GSInterface::init()+268                             (BEFORE parser.open — dump uninvolved)
#07  main+1616
#08  __libc_init
```

addr2line (NDK llvm, unstripped binary): `#00 → command_pool.cpp:171`
(`table->vkResetCommandPool(…)` — first statement past the
`pool == VK_NULL_HANDLE` guard, so pool is live). Disassembly at
`trim()+48`: `ldr x9,[x8,#8]` (`this->table`, offset 8 ✓) then
`ldr x9,[x9,#0x380]` (faults; tombstone x9 = 0) → **`table == nullptr`
with live `pool`**. Static audit (all in the pinned Granite
`16e7395f`): `table` is assigned ONLY at `command_pool.cpp:29`
(ctor-init from `Device::get_device_table()`, a member reference —
`device.cpp:5787-5790`) and `:48` (move-assign copy) — never nulled;
PerFrame emplaces every entry with a live device (`device.cpp:2330-2337`);
`pool` defaults to `VK_NULL_HANDLE` and is created only in the ctor, which
would crash first if `table` were null there. A live-pool + null-table
state is UNCONSTRUCTIBLE through in-tree paths — the null is written
post-construction by a mechanism invisible statically (needs a
device-side watchpoint/ASan catch — that IS the §3 experiment). In
Granite framework code (MIT license), NOT parallel-gs renderer code, NOT
any experiment hook (hooks are LOGI/`fopen` only and never execute before
the crash point — no G8/G10/G11 line appears in the 25 logcat lines).

Platform dependence (same source): macOS/MoltenVK runs this exact path
clean (G13: 18 `Running frame` lines, exit 0, 8 PPMs). Delta candidates
for the debugger to check (unranked, untested): Adreno queue-family
layout vs MoltenVK's single family (which `cmd_pools[i][j]` entry faults
is knowable from the pool index — not captured); volk device-table load
on Android; heap-layout luck around a stray write.

### 2f. Score vs G13 oracles (corrected labels, S-lag-aware method)

`g14-diff.py`: (1) re-verifies all 8 oracle pixel-shas (8/8 OK — table in
run receipt, §5); (2) scans `ps2x-g14/` for device PPMs: **0 files** →
`SCORE: N/A — 0 device scanouts (run crashed before first iterate)`,
exit 0. No diff table is fabricable and none is faked: with no device
pixels, alternatives (a)/(b) are both unreachable and the per-pair table
is empty by receipt, not by omission. The script implements the full
G13-shape diff (exact/le2/le32/PSNR, k=1..7, corrected labels) so the §3
re-run scores with zero new tooling.

## 3. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| On-device replay produces scanouts matching the dual-agreed emulator scanouts within a tabled tolerance | **BLOCKED (alternative (c))**: 0 on-device scanouts exist — the run segfaulted in pre-dump init (O1, §2e). No match/mismatch verdict is reachable; no pixels diverged because none were produced. Separately, the G7-OPEN on-device INIT leg now reads SUPPORTED: the project's own gate passes on Adreno 830 (API 1.3.284, slab lines, 0 gate-fail lines) |

The ONE next action the numbers justify: **one instrumented bounded
on-device run to catch O1's null-write live (lldb watchpoint on the
faulting pool's `table` slot, or an HWASan build), then the minimal fix
the watchpoint names + ONE clean re-run scored by `g14-diff.py`.**
Rationale: init is proven — the ONLY remaining unknown is a single null
slot with a 9-frame stack and a disassembled faulting load; a debugger
converts it to a named write-site in one session, and the leading fix
candidate (a `table == nullptr` guard in `trim()`/`begin()`/dtor, mirroring
the existing pool guard — UNTESTED, not a verdict) is 3 lines in Granite
if the write-site proves to be an init-order edge rather than corruption.
Queued behind it (not this action): any pixel comparison (needs scanouts);
any renderer change (forbidden until the framework crash is named).

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.

## 4. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c197…` (LGPL-3.0+ per G7 §4 — cited, not re-read) |
| Granite rev | `16e7395f…` (MIT per G7 §4 — cited); crash site + audited lines are Granite framework code |
| G14 shims S1–S3 | local experiment changes, uncommitted in SSD clone only (2 hunks outer `CMakeLists.txt`+`tools/`, 1 hunk Granite `application/platforms/` — cmake-only, ANDROID-gated; diff text mirrored as `g14-shims.diff`) |
| G7/G8/G10/G11 hunks | untouched in form; mac binary bit-identical (size + timestamp, §0) |
| NDK r30 | `30.0.16248370` (Apache-2.0 toolchain; no SDK/AGDK installed — that absence is WHY S1+S2 exist) |
| tombstone/logcat | OS-produced run receipts of our own binary (no PII; `uid: 2000` shell) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 5. Exact commands

Mac (`/Users/bradrichardson/dev/ps2xGS` unless noted;
`COPYFILE_DISABLE=1` on SSD steps):

```text
git -C "<ssd>/parallel-gs-g7" rev-parse HEAD ; status --short   # §2a (+ Granite status)
shasum -a 256 <g13-dump.gs> ; md5 <7 PNGs> ; python3 <pixsha one-liners>  # oracle verify
sed -n '1,120p' <clone>/CMakeLists.txt ; cat <clone>/tools/CMakeLists.txt # §2b (STANDALONE prediction check)
cmake -S <clone> -B <android-build> -G Ninja -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-35  # exit 0
cmake --build <android-build> --target parallel-gs-replayer -j2  # exit 0 (via S3 relink)
llvm-readelf --dynamic <binary> | grep NEEDED ; file <binary>    # aarch64 + system libs
llvm-addr2line -e <binary> -f -C -i 0x7e1540 ...                 # §2e line map
llvm-objdump -d --start-address=0x7e1510 --stop-address=0x7e1570 # §2e faulting load
python3 /tmp/g14-diff.py                                          # §2f (oracles 8/8 OK, 0 device, N/A)
du -sk <ssd dirs> ; df -h / "<ssd>"                              # §0
```

adb (`-s 622c49b1` throughout; `/data/local/tmp/g14/` ONLY; `mg/` never touched):

```text
shell 'getprop ro.product.model; ...; df -h /data'               # §2a + §2c reconfirm
shell 'cmd gpu vkjson'                                           # §2c driver oracle (abbrev)
shell 'rm -rf /data/local/tmp/g14 && mkdir -p /data/local/tmp/g14'
push <dump> /data/local/tmp/g14/g13-dump.gs                      # 184.7 MB/s
push <binary> /data/local/tmp/g14/parallel-gs-replayer           # 124.5 MB/s
shell 'cd /data/local/tmp/g14 && chmod 755 ... && sha256sum ...' # on-device sha match
logcat -d -s Granite:V                                           # before: 0 lines
shell 'cd /data/local/tmp/g14 && date +%s; timeout -s KILL 280 ./parallel-gs-replayer /data/local/tmp/g14/g13-dump.gs --iterations 2; echo RUN_EXIT=$?; date +%s'  # THE run: 139
logcat -d -s Granite:V                                           # after: 25 lines (§2c/§2d)
shell 'ls -la /data/local/tmp/g14/ ; ls -lat /data/tombstones/'  # 0 PPMs; tombstone_06
pull /data/tombstones/tombstone_06 <ssd>/ps2x-g14/               # 205,623 B
shell 'rm -f /data/local/tmp/g14/parallel-gs-replayer /data/local/tmp/g14/g13-dump.gs && rmdir /data/local/tmp/g14 && ls /data/local/tmp/'  # DEVICE_CLEAN (mg/ only)
```

Local experiment diffs (uncommitted): SSD clone `CMakeLists.txt` (S1) +
`tools/CMakeLists.txt` (S3) + `gs/gs_interface.cpp` (G11) +
`tools/gs_dump_replayer.cpp` (G8+G10) + Granite submodule
`application/platforms/CMakeLists.txt` (S2) + `util/timer.cpp` (G7) +
`parallel-gs-g14-android-build/` (binary 265,837,472 B, `c4cd63b4…`).

## 6. Gaps (what this brief could not do)

1. O1's null-write origin is unresolved statically (all in-tree
   table assignments are non-null — §2e). Needs the §3 debugger session;
   no mechanism is claimed.
2. Zero on-device scanouts → no pixel score, no tolerance table, no
   match/mismatch verdict. `g14-diff.py` stands ready for the re-run.
3. No heap/wall-per-VBlank numbers (run died before any timed region;
   wall <1 s by device-epoch receipts).
4. "Shims don't affect mac" is by ANDROID-gate inspection, not by mac
   rebuild (mac binary untouched — a rebuild would have risked the
   G13-verified binary for zero gain).
5. `tombstone_06` remains in OS-owned `/data/tombstones/` (shell cannot
   remove; auto-rotated; 205 KB).
6. No upstream contact (Granite/parallel-gs read-only + local shims;
   nothing filed).
7. `upstream/` and ps2xGS harness code untouched; no bytesize/WSL use;
   no new dumps (per the stop rule).

## 7. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…` (+G7/G8/G10/G11
  hunks + G14 S1–S3 — all uncommitted).
- Android build: `/Volumes/Extreme SSD/parallel-gs-g14-android-build/`,
  `tools/parallel-gs-replayer` (265,837,472 B, `c4cd63b4…`).
- SSD: `/Volumes/Extreme SSD/ps2x-g14/` (tombstone + logcat + run-stdout);
  `ps2x-g13/` pristine (35,840 KiB, 8/8 pixel-shas re-verified).
- Tools: `/tmp/g14-diff.py`, `/tmp/g14-configure.log`,
  `/tmp/g14-build{,2}.log`, `/tmp/g14-run-stdout.txt`,
  `/tmp/g14-logcat-{before,after}.txt`, `/tmp/g14-tombstone-06.txt`,
  `/tmp/g14-shim-{top,granite}.diff` (scripts mirrored; logs session-only).
- Commits: ps2xGS `[G14]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G14/` `[G14]` + same trailer (NOT pushed).
