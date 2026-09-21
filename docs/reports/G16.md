# G16 report — mac-host ASan (+UBSan) run is CLEAN to exit 0: app-side heap writer eliminated, bug is Android-specific — stop with recipe

Brief: G16 (this turn) — executes G15's §3: ONE mac-host ASan (+UBSan)
build (separate build dir) and ONE bounded ASan run of the same replayer
source on the same dump, to name-or-eliminate the app-side heap writer
with no device. Tables + hypothesis + next-action recommendation, no
verdicts beyond the hypothesis. Time box 6 h (used ~1 h). Read first per
the brief: `local/research/G15/REPORT.md` (all: O1 flaky 1/6, O3 caught
live with PROVEN scudo heap corruption, MTE naming BLOCKED by the
kernel, HWASan build silent and crashed on O2) + G13 mac build/run
receipts + G14 build scripts as needed.

Machine: same as G8–G15 (Apple M4, macOS 27.0 — no new installs). No
device, no Odin, no `adb`, no renderer changes, no P-lane lease, no
bytesize/WSL. Pins: paraLLEl-GS
`3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` + G14 S1–S3 shims + G11/G8+G10
hunks (G15 §2a clone state reproduced EXACTLY, §2a) / rich dump sha
`154d9d85…` (re-verified host-side) / mac recipe = G7/G13 (`VULKAN_SDK`
+ Ninja + `-j2`, tabled in §2a).

Headline result: outcome (b) — CLEAN-RUN ELIMINATION. The ASan (+UBSan)
build compiled exit 0 ([457/458], 213,618,296 B Mach-O arm64, sha
`ae404762…`, 68,339 `__asan` + 16 `__ubsan` symbols, single Homebrew
LLVM 23 ASan runtime). The ONE bounded run exited 0 in 4.0 s (280 s cap
untouched), printed all 18 `Running frame` lines (== G13's 9 cold + 9
warmed), wrote 10 scanouts, and produced ZERO sanitizer output across
the complete 1,741-line stderr (0 matches on every sanitizer token; the
only 13 ERROR/WARN lines are the known-benign Granite/Metal notes).
Score: oracles 8/8 re-verified AND all 7 same-boundary pairs
pixel-IDENTICAL to the G13 oracles (exact 1.0000, PSNR inf; k=0
pixel-sha matches too). The wild write does NOT fire anywhere in this
workload under host ASan+UBSan — the writer is Android-specific (Adreno
driver or Android-only path). No minimal fix exists for a non-app
writer, so per the stop rule there is NO source fix, NO second
sanitizer variant, NO renderer change. The ONE next action (§3): the
driver-bug recipe — one O2-argument debugger run naming the bad
descriptor input at `flush_descriptor_set():3505`.

## 0. Byte caps (declared at session start) vs actuals

| class | cap | actual |
| --- | --- | --- |
| ASan build dir (SSD, new `parallel-gs-g16-asan-build`) | 20 GB apparent | 4,577,280 KiB allocated (ExFAT 1 MiB clusters); binary 213,618,296 B; apparent ≤ allocated ≪ cap ✓ |
| SSD ps2x-g16 retrieval (dump copy + 10 PPMs + logs + scripts) | 100 MB | ~18.6 MB apparent (11.5 dump + 6.9 PPMs + ~0.16 logs/scripts); 43,008 KiB allocated ✓ |
| SSD ps2x-g13 / ps2x-g14 / ps2x-g15 (read-only) | 0 growth | 35,840 / 7,168 / 31,744 KiB == G15-end exactly ✓ |
| SSD mac / G14 / HWASan build dirs (read-only) | 0 growth | 3,808,256 / 4,480,000 / 4,816,896 KiB == G15-end exactly ✓ |
| SSD clone (source) | 0 + nothing | HEAD + status + shims + hooks all == G15 §2a; ZERO clone edits ✓ |
| internal volume (`/`) | ≤1 GB delta, no clones/builds | 4.4 Gi avail at start → 4.3 Gi at end; G16 net ≈ 0 (`/tmp/g16-*` ~160 KB; remainder other-lane — same honest accounting as G13–G15 §0) |
| `/tmp/g16-*` host scripts/logs | 600 MB peak, session-only | ~160 KB (2 scripts + build log + run stdout/stderr) ✓ |
| committed to git | text only (report + scripts) | this file (ps2xGS) + REPORT.md + 2 scripts (ssx3 mirror); no captures, no binaries, no build dirs ✓ |

SSD 326 → 309 Gi avail: this brief wrote ~4.6 GB allocated (asan build
+ retrieval); the remainder is other-lane. No code copied into any GPL
tree. No P-lane contention: no recomp boots/builds, no P-lane lease (no
fork writes at all), no bytesize/WSL use. Host build `-j2` (one
`parallel-gs-replayer` target). No `adb`, no Odin, no tombstones, no
lldb anywhere this brief.

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | The mac-host ASan (+UBSan) run names the app-side heap writer (file/line + alloc/free traces) |
| observable signal | build exit + instrumentation census + bounded-run exit/wall/frames + complete sanitizer output (or proven absence) + pulled scanouts + per-pair diff table vs G13 oracles |
| alternatives | (a) ASan fires → table the writer + ONE next action (minimal-fix brief input); (b) ASan clean to exit 0 → writer is Android-specific → table the clean run + the driver-bug-recipe next action |
| stop condition | ONE ASan build + ONE bounded run — no tuning loop, no source fix, no second sanitizer variant, no renderer change |
| outcome → next action | numbers name the next single experiment (§3) |

Outcome: alternative (b) — CLEAN-RUN ELIMINATION (exit 0, 18/18
frames, 0 sanitizer lines, 7/7 pixel-identical pairs). No tuning loop
was entered: no source fix, no second sanitizer variant, no renderer
change, no second run.

## 2. Task 1 — pin verify + ASan build (no run yet)

### 2a. Pin verification (pre-work — G15 §2a reproduced exactly)

| item | observed |
| --- | --- |
| SSD clone HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (== G15 §2a pin) |
| tree status | `M CMakeLists.txt` (S1) + `m Granite` (S2) + `M tools/CMakeLists.txt` (S3) + `M gs/gs_interface.cpp` (G11) + `M tools/gs_dump_replayer.cpp` (G8+G10) + ExFAT `._` sidecars — G15 §2a exactly |
| shim markers | `G14 local experiment shim` ×1 in each of the 3 cmake files (S1–S3); `G11` ×10 in `gs_interface.cpp`; `G8/G10` ×11 in `gs_dump_replayer.cpp`; `__APPLE__` ×1 in Granite `util/timer.cpp` (G7) |
| Granite rev | `16e7395f…` (== G15 pin); submodules all at pinned revs (incl. `spirv-tools 556c7ca`); one untracked `utils/Table/.___pycache__` dir inside spirv-tools (ExFAT junk, content-neutral — tabled, untouched) |
| rich dump (host) | 11,537,377 B, sha `154d9d8577a210fb794b29a048ee2cf08053f87fe220bb2bf975933a02ad7e32` full-match |
| G14 binary | 265,837,472 B, sha `c4cd63b4…` (post-relink receipt, exact) |
| HWASan binary | 289,108,176 B, sha `b93bb69a…` (exact) |
| mac binary (G13) | 51,879,240 B (== G13 §2a size; untouched); sha `935dab20…` (recorded — never tabled before) |
| mac recipe (what it actually uses) | cmake 4.4.3 + ninja 1.13.2 + `VULKAN_SDK=/opt/homebrew` + `CC=/opt/homebrew/opt/llvm/bin/clang` (Homebrew LLVM 23.1.1, from shell env) + `CXX=/usr/bin/c++` (AppleClang — G13 build mixed the two); NO build type; empty C/CXX flags; run env `VK_ICD_FILENAMES=…/MoltenVK_icd.json` + `DYLD_LIBRARY_PATH=/opt/homebrew/lib`; NO NDK involvement (NDK/toolchain not needed — nothing tabled beyond this row) |
| macOS `timeout` | ABSENT (no `timeout`/`gtimeout` binary) → wall cap enforced by a `python3` `subprocess.run(timeout=280)` wrapper (tabled substitution, same 280 s cap as the G14/G15 device runs) |

### 2b. ASan (+UBSan) build (new SSD dir — G13/G14/G15 dirs untouched)

Configure: G13 mac recipe + sanitizer flags only (build-dir
configuration; ZERO clone edits):

| flag / env | G13 mac recipe | G16 ASan build (delta) |
| --- | --- | --- |
| `VULKAN_SDK` | `/opt/homebrew` | same (no delta) |
| generator | Ninja | same (no delta) |
| `CMAKE_BUILD_TYPE` | empty | same — empty/`-O0` (no delta) |
| `CC` | Homebrew clang 23.1.1 (env-inherited) | same (no delta) |
| `CXX` | AppleClang `/usr/bin/c++` (G13 mixed) | **Homebrew `clang++` 23.1.1 — uniform-toolchain delta (ONE shared ASan runtime; a mixed Apple/Homebrew link would pull two `libclang_rt.asan` dylibs)** |
| `CMAKE_C_FLAGS` | empty | **`-fsanitize=address,undefined -fno-omit-frame-pointer`** |
| `CMAKE_CXX_FLAGS` | empty | **`-fsanitize=address,undefined -fno-omit-frame-pointer`** |
| `CMAKE_EXE_LINKER_FLAGS` | empty | **`-fsanitize=address,undefined`** |
| target / parallelism | `parallel-gs-replayer -j2` | same (no delta) |

| item | value |
| --- | --- |
| configure | exit 0; `build.ninja` generated; cache confirms both compilers Homebrew LLVM + all three sanitizer flag lines |
| build | `cmake --build … --target parallel-gs-replayer -j2` → exit 0, `[457/458]`; one pre-existing `FileDeleter -Wshadow` warning (G7-noted) + one benign `ld: ignoring duplicate libraries` note — no new warnings |
| binary | `tools/parallel-gs-replayer`, 213,618,296 B, sha `ae404762408ee09a5e2157ede40c31d4acd03fc074a1af3324127f1ce65a2366`, Mach-O 64-bit arm64 |
| instrumentation proof | **68,339 `__asan` symbols + 16 `__ubsan` symbols** (`nm` census); `otool -L` shows exactly ONE sanitizer runtime: `/opt/homebrew/opt/llvm/lib/clang/23/lib/darwin/libclang_rt.asan_osx_dynamic.dylib` (+ system `libSystem`/`libc++` only) |

## 3. Task 2 — ONE bounded ASan run + verdict

Run input: sha-verified COPY of the dump at `ps2x-g16/g16-dump.gs`
(11,537,377 B, sha `154d9d85…` full-match — a copy, not a new dump:
the G8+G10 PPM hook writes scanouts next to the dump path and
`ps2x-g13` is read-only). Args: `g16-dump.gs --iterations 2` (G13's mac
iteration count — 9 cold + 9 warmed frames). Env: G13 run env +
`ASAN_OPTIONS=detect_leaks=0:symbolize=1` (LeakSanitizer is unsupported
on macOS — heap/stack/UB checks unaffected) +
`UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`.

### 3a. Run table (THE bounded run)

| item | value |
| --- | --- |
| exit | **0** (clean) |
| wall | **4.0 s** (280 s cap untouched; host wall, not GPU time — same caveat as G7–G13) |
| frames | **18/18 `Running frame` lines** (== G13's 9 cold + 9 warmed) |
| stdout | 0 B (all Granite output goes to stderr on desktop by design) |
| stderr | 92,594 B, **1,741 lines, captured COMPLETE** (`g16-asan-run-stderr.txt` in `ps2x-g16/`) |
| sanitizer output | **NONE — 0 lines** matching `sanitizer|runtime error|heap-buffer|use-after|stack-buffer|SUMMARY:|SEGV|abort` (case-insensitive, whole file) |
| other ERROR/WARN | 13 lines, ALL benign and G13-shaped: 2 `[ERROR]` (calibrated-timestamp domain absent on MoltenVK; RenderDoc not attached) + 11 `[WARN]` (Metal-emulation adaptations + `Stalled compile` shader notes, all `success: yes`) |
| tail receipt | 8 `G8: wrote vsync…` lines + `G8: wrote first/last-vsync` + **`[INFO]: Done!`** (clean tail, not truncated) |
| scanouts | 10 PPMs in `ps2x-g16/` (vsync0–7 + first + last, 688,143 B each) |
| per-VBlank | 7.904 ms/VBlank host wall (vs G13's 8.579 — same neighborhood under ASan) |

Head of stderr (init path — same GPU/driver as G13):

```text
[INFO]: Layer count: 0
[INFO]: Enabling instance extension: VK_EXT_debug_utils.
[INFO]: Enabling instance extension: VK_KHR_portability_enumeration.
[INFO]: Found Vulkan GPU: Apple M4
[INFO]:     API: 1.4.357
[INFO]:     Driver: 0.2.2210
[INFO]: Using Vulkan GPU: Apple M4
[INFO]: Enabling device extension: VK_KHR_calibrated_timestamps.
```

Tail of stderr (last 3 of the 1,741 lines):

```text
[INFO]: G8: wrote first-vsync scanout 512x448 ... to /Volumes/Extreme SSD/ps2x-g16/g16-dump.gs.g8-first.ppm.
[INFO]: G8: wrote last-vsync scanout 512x448 ... to /Volumes/Extreme SSD/ps2x-g16/g16-dump.gs.g8-last.ppm.
[INFO]: Done!
```

### 3b. Score vs G13 oracles (`g14-diff.py`, zero new tooling)

`g14-diff.py ps2x-g13 ps2x-g16`: oracles **8/8 OK**; run PPMs: **10**
(vsync0–7 + first + last):

| k | exact | \|d\|≤2 | \|d\|≤32 | PSNR R/G/B (dB) |
| --- | --- | --- | --- | --- |
| 1 | 1.0000 | 1.0000 | 1.0000 | inf / inf / inf |
| 2 | 1.0000 | 1.0000 | 1.0000 | inf / inf / inf |
| 3 | 1.0000 | 1.0000 | 1.0000 | inf / inf / inf |
| 4 | 1.0000 | 1.0000 | 1.0000 | inf / inf / inf |
| 5 | 1.0000 | 1.0000 | 1.0000 | inf / inf / inf |
| 6 | 1.0000 | 1.0000 | 1.0000 | inf / inf / inf |
| 7 | 1.0000 | 1.0000 | 1.0000 | inf / inf / inf |

k=0 (outside the script's k=1..7 range, same pixel-sha method):
oracle `9c70d3e9` vs run `9c70d3e9` — MATCH. The ASan build reproduces
G13's mac run **bit-for-bit** (8/8 scanouts) while reporting nothing —
**SCORE: 7/7 IDENTICAL (8/8 incl. k=0)**. (Script note: one
`DeprecationWarning` about `Image.getdata` on this Pillow — vintage
API, numbers unaffected.)

## 4. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| The mac-host ASan (+UBSan) run names the app-side heap writer | **NOT SUPPORTED — eliminated instead**: exit 0, 18/18 frames, 0 sanitizer lines under 68,339 `__asan` symbols, 8/8 scanouts bit-identical to the G13 oracles. The wild write behind O1/O2/O3 does NOT fire anywhere in this workload on the mac host — no app-side writer exists to name. Separately SUPPORTED: the writer is Android-specific (Adreno driver or an Android-only path) |

The ONE next action the numbers justify: **the driver-bug recipe —
ONE on-device lldb run breaking at `flush_descriptor_set():3505` (the
O2 site, modal at 3/6 incl. under HWASan) to name which of `vk_set /
update_template / bindings` is bad, capture the descriptor-set state,
and package it as a minimal Adreno repro.** Rationale: both host
controls are now clean (G13 plain exit 0 + G16 ASan/UBSan exit 0 with
identical pixels), device HWASan is silent, and scudo proves
corruption exists on-device — the remaining unknown is exactly one bad
input at one named call site, and a debugger converts it to a
driver-bug filing in one session. Queued behind it (not this action):
any renderer change (forbidden until the driver input is named); any
further host sanitizer work (the host leg is closed twice over).

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.

## 5. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c19…` (LGPL-3.0+ per G7 §4 — cited, not re-read) |
| Granite rev | `16e7395f…` (MIT per G7 §4 — cited) |
| G14 shims S1–S3 + G7/G8/G10/G11 hunks | untouched, still uncommitted in SSD clone only; ZERO clone edits this brief (sanitizer flags are build-dir configuration) |
| G16 additions | ZERO clone edits — session files only: `g16-build-asan.sh` + `g16-run-asan.py` (G16-original, text, mirrored) |
| Homebrew LLVM 23.1.1 | Apache-2.0 toolchain (pre-installed); ASan runtime dylib linked from it, no redistribution |
| MoltenVK / Vulkan SDK | pre-installed (`/opt/homebrew`); run env only |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 6. Exact commands

Mac (`/Users/bradrichardson/dev/ps2xGS` unless noted;
`COPYFILE_DISABLE=1` on SSD steps; `CC` inherited from shell env):

```text
git -C "<ssd>/parallel-gs-g7" rev-parse HEAD ; status --short   # §2a (+ Granite)
shasum -a 256 <g13-dump.gs> <g14-binary> <hwasan-binary> <mac-binary>  # pins
grep -E "^CMAKE_(C_FLAGS|CXX_FLAGS|...)" <mac-build>/CMakeCache.txt    # recipe flags
VULKAN_SDK=/opt/homebrew CC=.../clang CXX=.../clang++ cmake -S <clone> -B <asan-build> -G Ninja "-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer" "-DCMAKE_CXX_FLAGS=..." "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"  # exit 0
cmake --build <asan-build> --target parallel-gs-replayer -j2   # exit 0 [457/458]
shasum -a 256 <asan-binary> ; nm <asan-binary> | grep -c __asan  # 68339 (instrumented)
nm <asan-binary> | grep -c __ubsan ; otool -L <asan-binary>     # 16 ; one runtime
cp <g13-dump.gs> <ps2x-g16>/g16-dump.gs ; shasum -a 256        # staging copy, sha match
python3 /tmp/g16-run-asan.py                                    # THE run: exit 0, 4.0 s
grep -c "Running frame" /tmp/g16-run-stderr.txt                 # 18
grep -c -i -e sanitizer -e "runtime error" ... stderr           # 0 (proven absence)
python3 <ssx3>/G14/g14-diff.py <ps2x-g13> <ps2x-g16>            # §3b (8/8 OK, 7/7 identical)
du -sk <ssd dirs> ; df -h / "<ssd>"                             # §0
```

## 7. Gaps (what this brief could not do)

1. The Android-side writer is still UNNAMED (expected: this brief's
   outcome (b) eliminates the host, it does not name the device writer
   — the §4 recipe run is the prescribed naming attempt).
2. O2's bad argument (`vk_set` vs `update_template` vs `bindings`) is
   still unknown — needs the queued O2-argument run.
3. No fix, no clean re-run on-device (correctly: nothing app-side to
   fix or verify — the host re-run IS the clean run, scored 7/7).
4. Host coverage is this workload only (rich dump, `--iterations 2`):
   ASan proves no wild write fires HERE, not in untested dumps — but
   this is the exact dump + path the device crashes on, which is the
   comparison that matters.
5. LeakSanitizer unavailable on macOS (`detect_leaks=0`): leak
   detection was off by platform design; heap-corruption/UB detection
   (the brief's target) is unaffected.
6. One `DeprecationWarning` (`Image.getdata`) from `g14-diff.py` on
   this Pillow — numbers unaffected, script untouched per zero-new-
   tooling.
7. `upstream/` and ps2xGS harness code untouched; no new dumps (the
   `ps2x-g16` copy is the same bytes, not a new capture).
8. No upstream contact (nothing filed).

## 8. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…`
  (G7/G8/G10/G11 hunks + G14 S1–S3 — all uncommitted, untouched by G16).
- ASan build: `/Volumes/Extreme SSD/parallel-gs-g16-asan-build/`,
  `tools/parallel-gs-replayer` (213,618,296 B, `ae404762…`).
- SSD: `/Volumes/Extreme SSD/ps2x-g16/` (dump copy + 10 PPMs + run
  stderr + build log + build/run scripts);
  `ps2x-g13/`/`ps2x-g14/`/`ps2x-g15/` pristine.
- Tools: `/tmp/g16-build-asan.sh`, `/tmp/g16-run-asan.py`,
  `/tmp/g16-build.log`, `/tmp/g16-run-stdout.txt`,
  `/tmp/g16-run-stderr.txt` (scripts mirrored; logs session-only +
  SSD retrieval).
- Commits: ps2xGS `[G16]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G16/` `[G16]` + same trailer (NOT pushed).
