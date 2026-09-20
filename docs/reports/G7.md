# G7 report — paraLLEl-GS bounded adoption check (capability gate + one short SSX replay)

Brief: `local/muse/prompts/G7.md` (ssx3) — executes the adoption experiment in
`docs/research/review-2026-09-20-first-frame-and-gs.md` §paraLLEl-GS + §GS gates.
Tables + hypothesis + next-action recommendation, no verdicts. Time box 6 h
(used ~1 h). Read first per the brief: `docs/reports/G6.md`, the review §131
checklist, R1 (`local/research/R1/REPORT.md`), OD1 (`local/research/OD1/REPORT.md`).

Machine: Apple M4, 10 cores, macOS 27.0, AppleClang 21.0.0, cmake + ninja (brew).
Pin: paraLLEl-GS `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd`
(Granite submodule `16e7395f6a4858c1783dbf6f521f90b9d5f82ac5`).
Candidate treated as an independent implementation, not an oracle.

## 0. Byte caps (declared) vs actuals

| class | cap | actual |
| --- | --- | --- |
| clone (`/Volumes/Extreme SSD/parallel-gs-g7`) | 10 GB apparent | 598,811,713 B apparent, 26,000 files (29 GB allocated — ExFAT 1 MiB clusters) |
| build dir (`...-build`, replayer closure only, `-j2`) | 60 GB | 1,732,726,017 B apparent (3.6 GB allocated); binary 51,850,328 B |
| run logs (all `/tmp/g7-*`) | 500 MB | ~450 KB total (largest: pcbuild 168 KB, vulkaninfo JSON 187 KB) |
| Task 2 retrieval (ONE dump + ONE png + park + excerpts) | 1 GB | 5,497,419 + 1,938 + 61,142 B ≈ 5.6 MB to `/Volumes/Extreme SSD/ps2x-g7/` |
| emulog/full traces | stay on bytesize; counts only | emulog 356,661,191 B on bytesize; retrieved counts only |
| internal volume (`/`, 20 GiB free at start) | no clones/builds; tool installs only | brew molten-vk 1.4.2 + vulkan-headers + vulkan-tools (+glslang, vulkan-loader): 20→19 GiB free |
| committed to git | text only (report + scripts) | this file (ps2xGS) + REPORT.md + 6 scripts (ssx3 mirror); no captures, no binaries, no build dirs |

No code copied into any GPL tree. Local experiment changes (all in the
external-SSD clone or the bytesize G7 clone; none committed anywhere):
`Granite/util/timer.cpp` macOS `nanosleep` shim, replayer PPM hook
(`tools/gs_dump_replayer.cpp`), PCSX2 G7 auto-dump hunks (bytesize only).

## 1. Experiment contracts

### E1 — Mac/MoltenVK capability gate (project's own init path)

| slot | content |
| --- | --- |
| hypothesis | The pinned `GSRenderer::init` gate (gs_renderer.cpp:807-817) passes on Apple M4 / MoltenVK 1.4.2 |
| observable signal | `parallel-gs-replayer` (built `-j2`, init path only) reaches post-gate code; per-item cross-check via `vulkaninfo` on the same driver |
| alternatives | (a) a gate item fails on the real driver → table item, stop Mac arm; (b) build blocked → table obstacle, stop Mac arm |
| stop condition | first hard fail (gate item or material build obstacle amounting to a port) |
| outcome → next action | pass → Task 2 on Mac; fail → recommend next target/fix, no replay attempted |

### E2 — Odin3 capability check (existing tooling only: adb + NDK probe)

| slot | content |
| --- | --- |
| hypothesis | Odin3 (Adreno 830) reports all 10 gate items via direct Vulkan queries |
| observable signal | per-item 0/1 from a 9,608 B NDK probe pushed to `/data/local/tmp/g7`, run once, removed |
| alternatives | (a) any item 0 → table, Odin init not attempted; (b) probe inconclusive → table OPEN + recipe |
| stop condition | probe output collected, or tooling gap (then OPEN + recipe, no new device infra) |
| outcome → next action | pass → on-device project-init stays OPEN (needs Android build; recipe §5); fail → recommend, stop Odin arm |

### E3 — one short SSX replay on Mac (E1 passed, so executed)

| slot | content |
| --- | --- |
| hypothesis | The pinned replayer replays a 5-frame native SSX dump at 1× without device errors and emits a scanout matching PCSX2's own aligned screenshot |
| observable signal | exit code, last-vsync PPM (native res, X1 defaults), diff stats vs the dump-start PNG, wall-per-VBlank, heap usage |
| alternatives | (a) capture fails → table obstacle + recipe, stop; (b) replay errors → table, stop; (c) large diffs → table numbers + method limits, no verdict |
| stop condition | one dump, one replay configuration (native/X1, defaults); no tuning loop |
| outcome → next action | numbers name the next single experiment (§6) |

## 2. Task 1 — capability gate

### 2a. Build receipt (init/capability path only, `-j2`)

- Clone: shallow (`--depth 1 --recurse-submodules --shallow-submodules`,
  `-c core.symlinks=false -c core.fileMode=false` for ExFAT), HEAD verified
  `3a66c19…`. Nested submodules needed a second
  `git submodule update --init --recursive --depth 1` + a forced re-checkout of
  `third_party/muFFT` (landed empty; ExFAT `._` sidecar friction, no content issue).
- Configure: `cmake -S <clone> -B <build> -G Ninja`, `VULKAN_SDK=/opt/homebrew`,
  `COPYFILE_DISABLE=1` → `Configuring done`.
- Build: `cmake --build <build> --target parallel-gs-replayer -j2` → exit 0.
  One local build shim required: `Granite/util/timer.cpp` uses Linux-only
  `clock_nanosleep`/`TIMER_ABSTIME`; added an `__APPLE__` relative-`nanosleep`
  fallback (timer utility only, no Vulkan semantics). Tabled as a local change,
  not upstreamed. Remaining build warning is pre-existing (`-Wshadow` on
  `FileDeleter` in `gs_dump_parser.hpp`).
- Binary: `tools/parallel-gs-replayer`, 51,850,328 B.
- Init order verified by reading `tools/gs_dump_replayer.cpp:36-104`:
  `Context::init_loader` → `init_instance_and_device` (push-desc/heaps/buffer bits
  requested) → `GSInterface::init` (`gs_interface.cpp:41` → `renderer.init`,
  `:51`) → `GSRenderer::init` gate (`gs_renderer.cpp:807-817`) → dump open.
  The gate runs BEFORE any dump is touched, so a placeholder dump path exercises
  the real gate. NOTE for re-runs: `parser.open(argv[1], …)` uses argv[1]
  literally — the dump path must be the FIRST argument; flags go after.

### 2b. Mac/MoltenVK — project's own init path: PASS

Command: `parallel-gs-replayer /tmp/g7-no-such-dump.gs` (placeholder),
`VK_ICD_FILENAMES=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json`,
`DYLD_LIBRARY_PATH=/opt/homebrew/lib`. Exit 1 (missing dump, expected — after the gate).

| signal | observed |
| --- | --- |
| `Minimum requirements for parallel-gs are not met` | 0 occurrences (gate passed) |
| post-gate code reached | `Using image slab size of 4041 MiB` + `max allocated image memory per flush of 606 MiB` (gs_renderer.cpp:863-866) |
| GPU selected | `Found Vulkan GPU: Apple M4`, API 1.4.357, driver 0.2.2210 |
| adaptations (non-fatal) | push descriptors disabled on Metal emulation; events emulated as barriers; no calibrated-timestamp domain; RenderDoc absent |
| log bytes | 998 B (`/tmp/g7-gate-run.log`) |

### 2c. Mac/MoltenVK — per-item checklist (independent oracle: vulkaninfo 1.4.357)

Target: Apple M4, MoltenVK 1.4.2. Predicate replica evaluated per
`Granite/vulkan/device.cpp:5808-5850` with default stage COMPUTE (`device.hpp:542`).

| # | gate item (gs_renderer.cpp) | observed value | pass |
| --- | --- | --- | --- |
| 1 | `vk12_features.descriptorIndexing` (:808) | true | yes |
| 2 | `vk12_features.timelineSemaphore` (:809) | true | yes |
| 3 | `vk12_features.bufferDeviceAddress` (:810) | true | yes |
| 4 | `vk12_features.storageBuffer8BitAccess` (:811) | true | yes |
| 5 | `vk11_features.storageBuffer16BitAccess` (:812) | true | yes |
| 6 | `enabled_features.shaderInt16` (:813) | true | yes |
| 7 | `vk12_features.scalarBlockLayout` (:814) | true | yes |
| 8 | subgroup ARITH+SHUFFLE+VOTE+BALLOT+BASIC (:815) | all present (+RELATIVE/CLUSTERED/QUAD/ROTATE); stages incl. COMPUTE | yes |
| 9 | `supports_subgroup_size_log2(true,2,6)` (:816) | sizeControl=1, fullSubgroups=1, min=4, max=32 → full_range (4≤4, 64≥32) = true | yes |
| 10 | `maxComputeSharedMemorySize ≥ 32 KiB` (:817) | 32768 (exactly at threshold; check is `< 32*1024` → pass) | yes |

Result: 10/10 pass; consistent with the project's own init passing (§2b).

### 2d. Odin3 — NDK probe (existing tooling): 10/10 capability queries pass

Device `622c49b1`: Odin3 (ayn), Android 15, SoC props as observed
(`ro.soc.model=CQ8725S`, `ro.hardware=qcom`). P-lane lease absent before/after;
probe (9,608 B, NDK r30 `aarch64-linux-android35-clang -O2 -lvulkan`) pushed to
`/data/local/tmp/g7/`, run once (~1 s), binary + dir removed. No
`/data/local/tmp/mg` touches, no settings changes, no contention (K1 rule kept:
no recomp boots/builds anywhere this brief).
Vulkan: Adreno (TM) 830, apiVersion 1.3.284, driverID 8 (Qualcomm proprietary),
instance 1.3.0. Probe source: `g7-vkprobe.c` (ssx3 mirror).

| # | gate item | observed value | pass |
| --- | --- | --- | --- |
| 1 | descriptorIndexing | 1 | yes |
| 2 | timelineSemaphore | 1 | yes |
| 3 | bufferDeviceAddress | 1 | yes |
| 4 | storageBuffer8BitAccess | 1 | yes |
| 5 | storageBuffer16BitAccess | 1 | yes |
| 6 | shaderInt16 | 1 | yes |
| 7 | scalarBlockLayout | 1 | yes |
| 8 | subgroup ops need=0x1f | have=0x2bf, have_all=1, subgroupSize=64 | yes |
| 9 | subgroup predicate (true,2,6,COMPUTE) | sizeControl=1, fullSubgroups=1, min=max=64 → full_range (4≤64, 64≥64) = 1 | yes |
| 10 | maxComputeSharedMemorySize ≥ 32 KiB | 32768, ge32k=1 | yes |

Caveat: this is the probe's replication of the gate queries, NOT the project's
own `GSRenderer::init` executing on Odin. On-device project init is OPEN
(§5 recipe — Android NDK build class work, out of scope for this gate).

## 3. Task 2 — one short SSX replay

### 3a. Dump provenance + version check

| item | value |
| --- | --- |
| file | `SSX 3_SLUS-20772_20260920193109.gs`, 5,497,419 B, sha256 `74d55f1e7981ecbbe972cf43b5b75fe97825482fb28b8b57dfaee913eae5fc69` (SSD copy sha-matches the bytesize original) |
| producer | G7 PCSX2 clone `/home/brad/pcsx2-g7/pcsx2` (local clone of T4 `9056c08349…`) + R1 one-liner (epoch verification) + G7 auto-dump hunks (ExecPS2 counter in `R5900OpcodeImpl.cpp` global scope; vsync hook in `GS.cpp:GSvsync`); binary sha `2bb85880…`, 130,935,400 B |
| pre-existing trees | T4 + R1 trees/binaries re-verified untouched after the G7 build (T4 `6719f5d6…`, R1 `6069b91b…`, R1 tree shows only its own committed one-line diff) |
| trigger | `GSQueueSnapshot("", 5)` on the GS thread, 60 vsyncs after ExecPS2 #5 (game entry); `G7_DUMP_QUEUED` ×1 in emulog; `ExecPS2` ×5; 1,813,775 bios calls |
| run | 150 s wall, recompiler + turbo, Xvfb :99, WID 2097159 (`SSX 3`), clean SIGTERM; emulog 356,661,191 B stays on bytesize (counts only retrieved) |
| ini (G7 dat copy; R1 dat untouched) | `GSDumpCompression = 0` (was 2/Zstandard — uncompressed is required by the pinned parser), `ScreenshotSize = 1` (internal), `ScreenshotFormat = 0` (png) |
| dump version | file header version **9** ∈ parser range 8..9 → ACCEPT (verified from bytes, not the README: README says "version 8", stale; PCSX2 `GSState::STATE_VERSION = 9`, parser `STATE_VERSION(_MIN) = 9/8`) |
| header cross-check | fakeCRC `0xffffffff`, header_size 1228846 = 36 + 10 (serial) + 1228800 (640×480×4 shot); state_size 4194813; packet region EOF-syncs after `+ sizeof(GSPrivRegSet)=8192` |
| packet census (EOF-synced, exact) | 8 Vsync (phases 1,0 alternating = interlaced fields), 8 PrivRegisters, **0 Transfer, 0 GIF bytes**, 0 ReadFIFO |
| aligned reference | `…09.png`, 1,938 B, 640×480 RGBA, ALL BLACK (1 distinct color) — dumped at the same vsync the dump starts |
| embedded shot | dump-header 640×480 u32, uniform (0,0,0,255) — opaque black, consistent |
| run-end proof | `g7-park.jpg`, 61,142 B, 1280×1024, 56,074 colors, 100% nonblack — the game renders fully by run end; the dump window (entry+60) is simply pre-first-draw |

Build note: the first G7 build failed at link (`undefined symbol:
g_g7_execps2_count`) — the counter was defined inside
`R5900::Interpreter::OpcodeImpl` while GS.cpp declared it at global scope.
Fixed by moving the definition above `namespace R5900 {` (`g7-fixup.sh`;
`g7-build.sh` carries the corrected hunk). Rebuild → exit 0.

Transport note (re-learned R1 rule): exactly ONE `wsl` per `ssh bytesize`
invocation; further commands are `;`-separated (cmd.exe eats pipes and a second
`wsl` fails). All remote work used single-shot commands, staged scripts, `;`
separators, no inline pipes.

### 3b. Replay table (image + diffs + time + memory)

Primary = the brief's configuration (replayer defaults = native res, X1
`SuperSampling::X1`, `--iterations 2` for a warmed second pass).
`--conservative-crtc` is a labeled diagnostic (needed mid-investigation to
enumerate vsyncs; the default gate `has_transfer` yields no iterate-true on
zero-transfer content). Screenshot hook (local change): consumes the timed
loop's leftover vsync result — no extra pass, init/replay untouched.

| # | measure | default (native/X1) | conservative-crtc (diagnostic) |
| --- | --- | --- | --- |
| 1 | image | `….gs.g7-default.ppm`, **640×448, ALL BLACK** (286,720 px, 1 color) | `….gs.g7-first.ppm`, 712×240, all black |
| 2 | diffs vs `…09.png` (640×480 black) | overlap 640×448 (center crop, y+16): **exact 1.0000** (286720/286720), all `\|d\|≤thr` = 1.0000, PSNR inf/inf/inf | not diffed (different CRTC geometry) |
| 3 | time | **3.080 ms per counted unit** (1 unit = full 8-vsync zero-transfer pass incl. file re-read + 4 MB state upload; tool counts loop-body executions, not true vsyncs). `--iterations 1`: `nan` (0 units — divide-by-zero tool quirk, tabled not fixed) | 0.279 ms/unit (9 units = 8 vsyncs + terminal check) |
| 4 | memory | Heap 0 DEVICE **327 tracked / 328 device MiB**, identical both passes | 322 / 324 MiB, stable |
| — | exit / errors | 0; no `Minimum requirements`; no device errors; one sync shader compile 558 ms on the cold pass (`success: yes`) | 0; same |

What the numbers do NOT claim: wall-per-VBlank is host wall (file re-read +
state upload + GPU work + MoltenVK), NOT isolated GPU time — the replayer has
no timestamp path and MoltenVK exposes no calibrated time domain (the
`Could not find a suitable time domain` ERROR line). Memory is Granite's heap
budget after a full pass. The diff is a black-on-black consistency check:
exact, but weak — see E3 verdict.

## 4. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (clone HEAD verified; shallow) |
| gs sources | `SPDX-License-Identifier: LGPL-3.0+`, Arntzen Software AS (gs_renderer.cpp, gs_interface.hpp, page_tracker.hpp, tools/gs_dump_replayer.cpp — first 4 lines each) |
| dump parser files | NO SPDX header (gs_dump_parser.{hpp,cpp} start with includes); covered by root `COPYING.LGPLv3` + README `## License` (LGPLv3+) |
| shader | `gs/shaders/ubershader.comp` carries the LGPL-3.0+ SPDX block after `#version 450` |
| Granite rev | `16e7395f6a4858c1783dbf6f521f90b9d5f82ac5`; `Granite/LICENSE` = MIT-style grant ("Permission is hereby granted…", © 2017-2026 Hans-Kristian Arntzen) |
| Granite third_party | astc-encoder, dirent, fossilize, fsr2, glslang, khronos, meshoptimizer, mikktspace, muFFT, oboe, pyroenc, pyrowave, rapidjson, renderdoc, sdl3, shaderc, spirv-cross, spirv-headers, spirv-tools, stb, volk (+ nested, e.g. googletest under astc-encoder) |
| PCSX2 G7 clone | T4 rev `9056c08349…` pristine source; R1 one-liner + G7 hunks uncommitted in `/home/brad/pcsx2-g7` only (GPL-3.0+ tree, no license mixing — nothing copied out) |
| brew tools | molten-vk 1.4.2, vulkan-headers 1.4.357, vulkan-tools 1.4.357, glslang 16.6.0, vulkan-loader 1.4.357 (all Apache-2.0) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 5. Odin on-device init — OPEN recipe (not attempted)

1. NDK r30 (`/opt/homebrew/share/android-ndk`, `darwin-x86_64`, API 35 sysroot —
   same toolchain that built the 9,608 B probe) + CMake Android toolchain file.
2. Configure the G7 clone for Android (`-DCMAKE_TOOLCHAIN_FILE=…/android.toolchain.cmake
   -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-35`). Keep the runtime
   shader compiler ON (the replayer compiles shaders at init); accept the glslang
   build. `PARALLEL_GS_STANDALONE=ON` first only if the null platform still links
   the offline tool (it doesn't add `tools/` — likely need default + SDL3 closure).
3. `ninja parallel-gs-replayer` (host `-j2` rule; expect 30–90 min first build).
4. Push binary + a `.gs` dump to `/data/local/tmp/g7/` (NOT the `mg/` tree), run
   with a wall cap, pull stdout + exit code + (G7 PPM hook) the scanout.
5. Gate bar: absence of `Minimum requirements` + slab-size lines, same as §2b.

## 6. Hypothesis verdicts + next actions

| exp | hypothesis verdict | the ONE next action it justifies |
| --- | --- | --- |
| E1 Mac gate | SUPPORTED: project's own init passes; 10/10 oracle items pass (shared memory exactly at threshold — re-check on any driver update) | — (led to E3, done) |
| E2 Odin caps | SUPPORTED (capability queries): 10/10 pass; on-device init OPEN | Keep §5 recipe on file; attempt on-device init only when a replay-target decision needs it |
| E3 replay | SUPPORTED WITH CONTENT CAVEAT: clean exit, no device errors, scanout exactly matches the black reference — but 0 GIF transfers, so only init + state upload + CRTC/scanout were exercised, not the draw core | Capture ONE post-first-draw dump with a draw-aware trigger (count Transfer packets on the GS thread; start the 5-frame dump K transfers after the first non-zero-transfer vsync), then re-run this exact replay table |

No verdicts beyond the hypotheses. No port, no adoption, no upstream contact.

## 7. Exact commands

Mac (`/Users/bradrichardson/dev/ps2xGS` unless noted; `COPYFILE_DISABLE=1` on SSD steps):

```text
brew install molten-vk vulkan-headers vulkan-tools   # + glslang, vulkan-loader; 20→19 GiB free on /
git clone -c core.symlinks=false -c core.fileMode=false --depth 1 --recurse-submodules \
  --shallow-submodules https://github.com/Arntzen-software/parallel-gs.git "/Volumes/Extreme SSD/parallel-gs-g7"
git submodule update --init --recursive --depth 1    # + muFFT force re-checkout (in Granite/)
find . -name '._*' -delete                            # ExFAT AppleDouble cleanup, repeat as needed
VULKAN_SDK=/opt/homebrew cmake -S <clone> -B <build> -G Ninja
cmake --build <build> --target parallel-gs-replayer -j2   # + timer shim edit before; + hook edits + rebuilds after
VK_ICD_FILENAMES=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json DYLD_LIBRARY_PATH=/opt/homebrew/lib \
  ./tools/parallel-gs-replayer /tmp/g7-no-such-dump.gs            # gate run (placeholder dump)
vulkaninfo --summary ; vulkaninfo --json --output /tmp/g7-vulkaninfo.json   # same env (oracle)
/opt/homebrew/share/android-ndk/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android35-clang \
  /tmp/g7-vkprobe.c -O2 -lvulkan -o /tmp/g7-vkprobe
adb -s 622c49b1 shell 'mkdir -p /data/local/tmp/g7'
adb -s 622c49b1 push /tmp/g7-vkprobe /data/local/tmp/g7/g7-vkprobe
adb -s 622c49b1 shell '/data/local/tmp/g7/g7-vkprobe'
adb -s 622c49b1 shell 'rm -f /data/local/tmp/g7/g7-vkprobe; rmdir /data/local/tmp/g7'
./tools/parallel-gs-replayer "<dump>.gs" --iterations 2 [--conservative-crtc]  # dump FIRST (argv[1])
python3 /tmp/g7-diff.py <scanout.ppm> <ref.png>
python3 -c "<header/census/stats one-liners>"          # §3a numbers
```

bytesize (each via ONE `ssh bytesize 'wsl …'`, `;` separators, no inline pipes;
scripts staged `scp … "bytesize:pcsx2-t4/"` then `wsl cp /mnt/c/… /home/brad/pcsx2-t4`):

```text
wsl grep -n -m5 "STATE_VERSION" …/pcsx2/GS/GSState.h
wsl sed -n "1241,1270p;480,510p" …/pcsx2/GS/GS.cpp            # hotkeys + GSQueueSnapshot
wsl grep -n -m20 "m_snapshot\|m_dump_frames" …/GS/Renderers/Common/GSRenderer.cpp
wsl sed -n "705,795p" …/GS/Renderers/Common/GSRenderer.cpp    # snapshot/dump consumer
wsl sed -n "430,478p" …/pcsx2/GS/GS.cpp                       # GSvsync hook site
wsl sed -n "895,925p" …/pcsx2/R5900OpcodeImpl.cpp             # SYSCALL context
wsl grep -n -m6 "ExecPS2" …/R5900OpcodeImpl.cpp …/R5900OpcodeTables.h
wsl grep -rln -m2 "GSDumpCompression" …/pcsx2                 # ini key hunt
wsl grep -n -m4 -B3 -A3 "GSDumpCompression" …/Pcsx2Config.cpp
wsl grep -n -m3 -A10 "CreateUncompressedDump" …/GS/GSDump.cpp
wsl grep -n -m2 -A8 "enum class GSDumpCompressionMethod" …/Config.h
wsl grep -n -m2 -A14 "GSDumpUncompressed::GSDumpUncompressed" …/GS/GSDump.cpp
wsl grep -n -m2 -A8 "GetScreenshotSuffix" …/GS/Renderers/Common/GSRenderer.cpp
wsl grep -n -m2 -A8 "enum class GSScreenshotSize" …/Config.h
wsl grep -n -m3 "ScreenshotFormat\|ScreenshotSize" …/Pcsx2Config.cpp
wsl grep -n "ScreenshotFormat\|ScreenshotSize\|GSDumpCompression" …/r1/dat/PCSX2/inis/PCSX2.ini
wsl grep -n -m3 "include <atomic>" …/R5900OpcodeImpl.cpp …/GS/GS.cpp
wsl sed -n "1,40p" …/GS/GS.cpp ; grep -n -m5 "Console.WriteLn" …/GS/GS.cpp
wsl bash /home/brad/pcsx2-t4/g7-build.sh      # clone T4→G7, patch, cmake (T4 flags), ninja -j2
wsl bash /home/brad/pcsx2-t4/g7-fixup.sh      # namespace fix + rebuild (BUILD_EXIT:0)
wsl bash /home/brad/pcsx2-t4/g7-setup.sh      # dat copy + ini flips (verify re-grep after)
wsl bash /home/brad/pcsx2-t4/g7-run.sh        # 150 s wall run, auto dump, post counts
wsl cp "<snaps>/*.gs" "<snaps>/*.png" …/g7-park.jpg /mnt/c/Users/bradr/pcsx2-t4
scp "bytesize:pcsx2-t4/<artifacts>" "/Volumes/Extreme SSD/ps2x-g7/"
wsl sha256sum "<snaps>/*.gs"                  # transfer-integrity cross-check
wsl grep -n -A45 "GSDumpBase::AddHeader" …/GS/GSDump.cpp          # writer layout
wsl grep -n -A30 "struct GSPrivRegSet" …/GS/GSRegs.h ; sed -n "1273,1300p" …  # sizeof = 8192
```

Local experiment diffs (uncommitted, in the SSD clone): `Granite/util/timer.cpp`
(`__APPLE__` shim), `tools/gs_dump_replayer.cpp` (PPM hook v1 → v2); bytesize:
`pcsx2/R5900OpcodeImpl.cpp` + `pcsx2/GS/GS.cpp` (committed here as `g7-build.sh`
python hunks + `g7-fixup.sh`).

## 8. Gaps (what this brief could not do)

1. Zero-transfer dump: the draw core (GIF path, ubershader draws, hazards) was
   not exercised. Next: the draw-aware capture in §6 (still one dump, same table).
2. No isolated GPU time: host wall only; no timestamp path in the replayer;
   MoltenVK exposes no calibrated time domain. A Granite timestamp build or an
   external GPU profiler would be needed for device-side numbers.
3. No default-mode FIRST-vsync image: the hook saves the leftover LAST vsync
   (zero-transfer dumps yield no iterate-true). All 8 fields are black by the
   packet census, but only the last was imaged in default mode.
4. Diff geometry: reference 640×480 vs scanout 640×448 → center-crop overlap
   (y+16); full-frame equality not asserted, only overlap equality.
5. Odin on-device project init: OPEN (§5). Capability queries pass; running
   `GSRenderer::init` on Adreno needs the Android build (not attempted).
6. Shared memory exactly at threshold (32768) on both targets: a driver update
   lowering it by one byte would flip item 10 — re-check on driver changes.
7. `upstream/` and ps2xGS harness code untouched; ps2xGS `.gscap` captures are a
   different format from PCSX2 `.gs` dumps (no adapter attempted or implied).
8. Internal volume now 19 GiB free (brew tools); external-SSD artifacts
   (clone 0.56 GiB apparent, build 1.61 GiB, dump dir ~6 MB) retained, none
   committed. `/tmp/g7-*` logs (~450 KB) retained for the session only.

## 9. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…` (+2 local file diffs).
- Build: `/Volumes/Extreme SSD/parallel-gs-g7-build/`, `tools/parallel-gs-replayer`.
- Dump dir: `/Volumes/Extreme SSD/ps2x-g7/` (.gs + .png + park jpg + 2 PPMs).
- Logs: `/tmp/g7-gate-run.log`, `/tmp/g7-replay{,2,3,4}.log`, `/tmp/g7-{build,build2}.log`,
  `/tmp/g7-{pcbuild,fixup,run}.log`, `/tmp/g7-vulkaninfo.json`, `/tmp/g7-odin-probe.txt`.
- Tools: `/tmp/g7-vkprobe.c`, `/tmp/g7-diff.py`, `/tmp/g7-{build,setup,run,fixup}.sh`
  (mirrored to ssx3; `/tmp` originals are session-only).
- bytesize: `/home/brad/pcsx2-g7/` (source+build+dat+logs+emulog), scripts also at
  `/home/brad/pcsx2-t4/g7-*.sh` + `C:\Users\bradr\pcsx2-t4\g7-*.sh` staging copies.
- Commits: ps2xGS `[G7]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G7/` `[G7]` + same trailer (NOT pushed).
