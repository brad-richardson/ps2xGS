# G6 report — Release-build numbers + upstream PR prep (no submit)

Brief: G6 runbook. Read: `docs/reports/G4.md` (§2b timing tables +
method, §6 -O0 caveats) and `docs/reports/G5.md` (§1 patch
inventory). Every timing in G0–G5 is a `-O0` build; this report
re-measures at release optimization and preps the upstream submission
without submitting it.

Host only, no `adb`, no device, no emulator, no lease. `cpu` backend
default untouched; `upstream/` untouched (pinned at
`14b1e5cb39b4af7e6fc12f9a29fdc751efde49d7`,
`v0.4-31-g14b1e5c`); `strict` untouched; `spike` untouched. No new
captures (102 reused). No `CMakeLists.txt` flag changes (the Release
build is a separate configure line into a fresh dir).

Machine: Apple M4, 10 cores, macOS 27.0. Toolchain: `/usr/bin/c++`,
AppleClang 21.0.0 (clang-2100.3.34.2), `-std=c++20`, `-arch arm64`,
libc++. Load average during timing runs: 2.13 (Release matrix),
2.34–2.36 (-O0 baseline matrix).

## 0. Release flags (recorded, not changed)

Configure line: `cmake -S . -B /tmp/ps2xgs-build-o2 -G Ninja
-DCMAKE_BUILD_TYPE=Release` (exit 0). The brief's `-O2` shorthand
maps to these exact flags:

| source | value |
| --- | --- |
| `CMAKE_BUILD_TYPE` | `Release` |
| `CMAKE_CXX_FLAGS_RELEASE` | `-O3 -DNDEBUG` |
| `build.ninja` per-TU `FLAGS` | `-O3 -DNDEBUG -std=c++20 -arch arm64` |
| patched-cpu `DEFINES` (plus) | `-DGSCpuBackend=GSPatchedCpuBackend` |

-O0 baseline binary: the untouched `/tmp/ps2xgs-build/gsreplay`
(default empty `CMAKE_BUILD_TYPE`, no `-O` flag — same as G0/G3/G4/G5
builds), executed read-only; no files written to `/tmp/ps2xgs-build`.

Build: `cmake --build /tmp/ps2xgs-build-o2 -j4`, exit 0. Compiler
output included 3 `-Wswitch` warnings from the `spike_backend.cpp` TU,
reproduced verbatim below (same 3 occur with no `-O` flag;
pre-existing, in the upstream header, not edited):

```
upstream/ps2xRuntime/include/runtime/gs/ps2_gs_memory.h:67:11: warning: enumeration value 'Max' not handled in switch [-Wswitch]
upstream/ps2xRuntime/include/runtime/gs/ps2_gs_memory.h:91:11: warning: enumeration value 'Max' not handled in switch [-Wswitch]
upstream/ps2xRuntime/include/runtime/gs/ps2_gs_memory.h:168:11: warning: 9 enumeration values not handled in switch: 'C32', 'C24', 'C16'... [-Wswitch]
3 warnings generated.
```

Staged patched sources: `cmp` of
`/tmp/ps2xgs-build-o2/g5-patched/src/gs_cpu_backend.cpp` against the
-O0 build's staged file reports identical (`STAGED-C-O2-MATCH`).

## 1. Release numbers (Step 1)

G4 §2b capture set (6 captures), all four backends (`cpu`, `strict`,
`spike`, `patched-cpu`), G3/G4 method: backends interleaved per run,
5 runs, median (min-max), recorded as observed. `median_ms` (single
`Present` call) is the Present-patch metric; `submit_ms_total` is the
no-change control for the Present patch and carries the CLUT/RMW
Submit effects. `-O0` column: fresh re-runs with the untouched -O0
binary (same method, same session); `-O2` column: the Release build
(§0 flags). 120 lines per matrix, all well-formed
(`g6-time-o0-raw.txt`, `g6-time-o2-raw.txt`).

### 1a. `cpu`: -O0 vs Release

| capture | submits | -O0 median_ms med (min-max) | -O2 median_ms med (min-max) | -O0 submit med (min-max) | -O2 submit med (min-max) |
| --- | --- | --- | --- | --- | --- |
| tex-t8-csm1.gscap | 1 | 15.848 (15.373-15.910) | 0.364 (0.337-0.498) | 0.268 (0.243-0.285) | 0.019 (0.019-0.038) |
| blend-a-cs.gscap | 2 | 15.881 (15.372-15.897) | 0.353 (0.328-0.423) | 0.638 (0.619-0.650) | 0.037 (0.033-0.044) |
| transfer-l2l.gscap | 0 | 15.885 (15.857-15.938) | 0.357 (0.335-0.392) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-both.gscap | 0 | 29.057 (28.868-29.106) | 0.479 (0.457-0.518) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-field.gscap | 0 | 18.550 (18.528-18.563) | 0.422 (0.415-0.458) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-fbp0-black.gscap | 0 | 22.528 (22.341-22.790) | 0.417 (0.405-0.455) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |

### 1b. `strict`: -O0 vs Release

| capture | submits | -O0 median_ms med (min-max) | -O2 median_ms med (min-max) | -O0 submit med (min-max) | -O2 submit med (min-max) |
| --- | --- | --- | --- | --- | --- |
| tex-t8-csm1.gscap | 1 | 15.839 (15.613-15.896) | 0.349 (0.330-0.430) | 0.277 (0.269-0.280) | 0.021 (0.019-0.028) |
| blend-a-cs.gscap | 2 | 15.886 (15.753-16.092) | 0.335 (0.322-0.387) | 0.645 (0.636-0.697) | 0.035 (0.035-0.040) |
| transfer-l2l.gscap | 0 | 15.840 (15.720-15.978) | 0.350 (0.339-0.374) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-both.gscap | 0 | 29.108 (28.951-29.161) | 0.478 (0.471-0.507) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-field.gscap | 0 | 18.528 (18.506-18.594) | 0.408 (0.401-0.440) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-fbp0-black.gscap | 0 | 22.596 (22.487-22.832) | 0.421 (0.409-0.445) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |

### 1c. `spike`: -O0 vs Release

| capture | submits | -O0 median_ms med (min-max) | -O2 median_ms med (min-max) | -O0 submit med (min-max) | -O2 submit med (min-max) |
| --- | --- | --- | --- | --- | --- |
| tex-t8-csm1.gscap | 1 | 0.554 (0.546-0.568) | 0.424 (0.376-0.517) | 0.232 (0.230-0.243) | 0.026 (0.024-0.037) |
| blend-a-cs.gscap | 2 | 0.568 (0.554-0.595) | 0.401 (0.379-0.489) | 0.568 (0.561-0.570) | 0.037 (0.034-0.039) |
| transfer-l2l.gscap | 0 | 0.563 (0.527-0.578) | 0.413 (0.377-0.429) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-both.gscap | 0 | 6.115 (6.051-6.141) | 0.566 (0.538-0.580) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-field.gscap | 0 | 3.258 (3.180-3.283) | 0.477 (0.464-0.505) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-fbp0-black.gscap | 0 | 3.394 (3.325-3.502) | 0.468 (0.447-0.513) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |

### 1d. `patched-cpu`: -O0 vs Release

| capture | submits | -O0 median_ms med (min-max) | -O2 median_ms med (min-max) | -O0 submit med (min-max) | -O2 submit med (min-max) |
| --- | --- | --- | --- | --- | --- |
| tex-t8-csm1.gscap | 1 | 0.560 (0.541-0.578) | 0.386 (0.379-0.510) | 0.232 (0.227-0.233) | 0.024 (0.022-0.031) |
| blend-a-cs.gscap | 2 | 0.568 (0.555-0.580) | 0.398 (0.374-0.409) | 0.565 (0.564-0.572) | 0.034 (0.033-0.039) |
| transfer-l2l.gscap | 0 | 0.580 (0.554-0.593) | 0.381 (0.373-0.427) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-both.gscap | 0 | 6.117 (5.843-6.158) | 0.550 (0.531-0.560) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-field.gscap | 0 | 3.238 (3.204-3.330) | 0.468 (0.459-0.478) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-fbp0-black.gscap | 0 | 3.397 (3.372-3.433) | 0.496 (0.463-0.543) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |

### 1e. Cross-check: fresh -O0 medians vs G4 §2b / G5 §2b medians

`median_ms` medians (fresh G6 -O0 re-run vs published):

| capture | cpu fresh (G4, G5) | spike fresh (G4) | patched-cpu fresh (G5) |
| --- | --- | --- | --- |
| tex-t8-csm1 | 15.848 (15.846, 16.047) | 0.554 (0.578) | 0.560 (0.561) |
| blend-a-cs | 15.881 (15.848, 16.042) | 0.568 (0.538) | 0.568 (0.571) |
| transfer-l2l | 15.885 (15.858, 15.973) | 0.563 (0.559) | 0.580 (0.557) |
| present-both | 29.057 (29.145, 29.408) | 6.115 (6.140) | 6.117 (6.174) |
| present-field | 18.550 (18.543, 18.701) | 3.258 (3.236) | 3.238 (3.261) |
| present-fbp0-black | 22.528 (22.519, 22.743) | 3.394 (3.377) | 3.397 (3.423) |

`submit_ms_total` medians (fresh G6 -O0 re-run vs published;
all other captures are 0.000 on all backends):

| capture | cpu fresh (G4, G5) | spike fresh (G4) | patched-cpu fresh (G5) |
| --- | --- | --- | --- |
| tex-t8-csm1 | 0.268 (0.271, 0.272) | 0.232 (0.233) | 0.232 (0.236) |
| blend-a-cs | 0.638 (0.639, 0.647) | 0.568 (0.564) | 0.565 (0.573) |

`strict` 5-run medians have no G4/G5 counterpart (first measured
here); G2 identity tables hold single-shot strict medians under a
different method.

## 2. Upstream PR prep (Step 2)

`upstream-patches/PR.md` (new, this report's Step 2 commit). Contents:

| section | contents |
| --- | --- |
| §0 | exact base rev (`14b1e5c…`, `v0.4-31-g14b1e5c`), apply order |
| §1a–§1c | per-patch what/why/measured effect at -O0 AND Release |
| §2 | dry-run + proof-target receipts by reference (G5 §1–§2) |
| §3 | test plan (the 12 CTest gate names) |
| §4 | known caveats (G5 §6) |

Every number cited there was checked against the G5 report plus the
§1 reruns. Nothing was submitted; no `gh`, no network writes.

## 3. CTest output (Step 1 gate)

Receipt: `/tmp/ps2xgs-build-o2/g6-ctest.log`. Full output (12 tests;
exit 0; timing is machine-dependent and not asserted):

```
Test project /tmp/ps2xgs-build-o2
      Start  1: gsregs-roundtrip
 1/12 Test  #1: gsregs-roundtrip .................   Passed    0.21 sec
      Start  2: gscap-roundtrip
 2/12 Test  #2: gscap-roundtrip ..................   Passed    0.07 sec
      Start  3: gen-synth
 3/12 Test  #3: gen-synth ........................   Passed    5.51 sec
      Start  4: identity-replay
 4/12 Test  #4: identity-replay ..................   Passed    0.64 sec
      Start  5: run-census
 5/12 Test  #5: run-census .......................   Passed    0.15 sec
      Start  6: census-features
 6/12 Test  #6: census-features ..................   Passed    0.07 sec
      Start  7: strict-diffs-on-mip
 7/12 Test  #7: strict-diffs-on-mip ..............   Passed    0.02 sec
      Start  8: strict-exact-on-control
 8/12 Test  #8: strict-exact-on-control ..........   Passed    0.05 sec
      Start  9: cpu-identity-on-new-captures
 9/12 Test  #9: cpu-identity-on-new-captures .....   Passed    0.11 sec
      Start 10: present-heuristic
10/12 Test #10: present-heuristic ................   Passed    0.06 sec
      Start 11: spike-identity
11/12 Test #11: spike-identity ...................   Passed    0.61 sec
      Start 12: patched-identity
12/12 Test #12: patched-identity .................   Passed    0.53 sec

100% tests passed out of 12

Total Test time (real) =   8.03 sec
```

No failures; no fixes of any kind were needed. Capture stability:
the 4 `present-fbp0-*` md5s after the suite's regeneration match G3
§1 exactly (`f7af2946…`, `1eeb0340…`, `1340e0ba…`, `92f6bba9…`); no
captures committed.

## 4. Exact commands

```
cmake -S . -B /tmp/ps2xgs-build-o2 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/ps2xgs-build-o2 -j4
ctest --test-dir /tmp/ps2xgs-build-o2 --output-on-failure
sh /tmp/g6-time.sh <gsreplay-bin> "/Volumes/Extreme SSD/ps2xgs/synth" tex-t8-csm1 blend-a-cs transfer-l2l present-both present-field present-fbp0-black
python3 /tmp/g6-agg.py /tmp/ps2xgs-build-o2/g6-time-o0-raw.txt /tmp/ps2xgs-build-o2/g6-time-o2-raw.txt
/tmp/ps2xgs-build-o2/gsreplay <capture.gscap> --backend cpu|strict|spike|patched-cpu --time-submits
cmp /tmp/ps2xgs-build/g5-patched/src/gs_cpu_backend.cpp /tmp/ps2xgs-build-o2/g5-patched/src/gs_cpu_backend.cpp
```

## 5. Receipt paths

- `/tmp/ps2xgs-build-o2/` — Release binaries (`gsreplay` with all
  four backends), `g6-configure.log`, `g6-ctest.log`,
  `g6-time-o0-raw.txt` (120 lines) + `g6-time-o2-raw.txt` (120 lines)
  + `g6-time-table.md` (§1a–§1d source), `g5-patched/` (staged patched
  sources), `g5-scratch/` (apply work tree).
- `/tmp/g6-time.sh` — interleaved 4-backend timing script;
  `/tmp/g6-agg.py` — median (min-max) aggregator.
- `/tmp/ps2xgs-build/` — untouched (no builds, no writes; the -O0
  binary there was executed read-only for §1a–§1d baseline columns).
- `/Volumes/Extreme SSD/ps2xgs/synth/` — 102 `.gscap` captures
  (unchanged; §3 md5s). Nothing over 5 MB in the repo; no captures
  committed.
- Repo commits (prefix `[G6]`, trailer `Orchestrated-By: Muse Code`):
  PR prep, this report.

## 6. What I could not do

- Per-patch isolation exists at Release only where the capture set
  allows it: blend-a-cs (untextured) isolates RMW from CLUT, but no
  CLUT-only or RMW-only binary was built at Release, so the tex-t8
  Release submit delta is the joint CLUT+RMW effect (all
  `WritePixel` frame draws in the 102 are CT32, G3 §9).
- Release min-max spreads are wide in relative terms (e.g. cpu
  tex-t8-csm1 0.364 med over 0.337–0.498): sub-millisecond medians
  with scheduling noise. Recorded as observed.
- The G6 Release numbers inherit the G5 §6 caveats unchanged:
  CLUT-memo mid-batch assumption, RMW CT32-only, caller-side frame
  destroy and dual-CRT/field/fbp0 temp destroys, synthetic 102 only.
- Identity at Release rests on the CTest gates (§3), which assert
  byte-identity, not timing; no separate 102-row Release identity
  table was produced.
- `upstream/` untouched throughout; the patches were not submitted
  there (prep file only: `upstream-patches/PR.md`).