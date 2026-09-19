# G5 report — CLUT/RMW/Present spikes as upstream patches, proven by a build-time proof target

Brief: G5 runbook. Read: `docs/reports/G3.md` (§3 spike vehicle,
`[G3-CLUT]`/`[G3-RMW]` hunks) and `docs/reports/G4.md` (§2
`[G4-PRESENT]` hunk). Host-only, no `adb`, no device, no emulator, no
lease. `cpu` backend default untouched; `upstream/` untouched (pinned
at `14b1e5cb39b4af7e6fc12f9a29fdc751efde49d7`,
`v0.4-31-g14b1e5c`); `strict` untouched; `spike` untouched. No new
captures (102 reused).

Machine: Apple M4, 10 cores, macOS 27.0. Toolchain: `/usr/bin/c++`,
AppleClang 21.0.0, `-std=c++20`, no `-O` flag (default
`CMAKE_BUILD_TYPE=` empty — same as G0/G3/G4 builds), libc++.

## 1. Patch inventory (Step 1)

Three files under `upstream-patches/`, each self-contained (applies to
the pristine pinned rev on its own) with `patch -p1` from the repo
root (`a/upstream/...` / `b/upstream/...` paths). Helper identifiers
are upstream-neutral (`ensureC32RmwPage`, `rmwBegin32`/`rmwCommit32`,
`hostFrameZeros`/`assignHostZeros`, `VramSnap`); the ported logic and
`[G3-CLUT]`/`[G3-RMW]`/`[G4-PRESENT]` markers match the G3/G4 hunks.

| patch file | ports | upstream files touched | hunks | bytes |
| --- | --- | --- | --- | --- |
| `clut-cache.patch` | `[G3-CLUT]` (G3 §4) | `gs_cpu_backend.h`, `gs_cpu_backend.cpp` | 6 | 4376 |
| `rmw-lookup.patch` | `[G3-RMW]` (G3 §5) | `gs_cpu_backend.cpp` | 5 | 4893 |
| `present-scratch.patch` | `[G4-PRESENT]` (G4 §2) | `gs_cpu_backend.cpp` | 4 | 4638 |

Hunk contents (upstream line numbers refer to the pinned rev):

| patch | hunk | region |
| --- | --- | --- |
| clut-cache | `@@ -3,6 +3,7 @@` (h) | header: `#include <cstdint>` |
| clut-cache | `@@ -72,4 +73,10 @@` (h) | header: `invalidateClutCache` decl, `m_clutCache`/`m_clutTag`/`m_clutGen` |
| clut-cache | `@@ -551,10 +551,21 @@` | `invalidateClutCache` def + `ResetUnlocked` call |
| clut-cache | `@@ -568,6 +579,7 @@` | `Submit` invalidation call |
| clut-cache | `@@ -580,6 +592,8 @@` | `TextureFlush` lock + invalidation call |
| clut-cache | `@@ -947,26 +961,41 @@` | `LookupCLUT` memo check + `resolved` + store |
| rmw-lookup | `@@ -467,6 +467,11 @@` | `ensureC32RmwPage` forward decl |
| rmw-lookup | `@@ -474,6 +479,7 @@` | constructor table-init call |
| rmw-lookup | `@@ -771,8 +777,78 @@` | C32 page table + `rmwBegin32`/`rmwCommit32` helpers |
| rmw-lookup | `@@ -815,9 +891,19 @@` | `WritePixel` frmw shared-lookup read |
| rmw-lookup | `@@ -930,7 +1016,10 @@` | `WritePixel` conditional store through the slot |
| present-scratch | `@@ -1678,7 +1678,54 @@` | `hostFrameZeros`/`assignHostZeros`/`VramSnap` helpers |
| present-scratch | `@@ -1692,7 +1739,7 @@` | `CopyFrameToHostRgba` bulk fill |
| present-scratch | `@@ -1761,13 +1808,26 @@` | `Present` raw thread-local snapshot |
| present-scratch | `@@ -1842,7 +1902,7 @@` | dual-CRT composite bulk fill |

Dry-run receipts (each on a scratch copy of the pinned tree,
`patch -p1 --dry-run`, recorded as observed):

| patch | dry-run output | receipt |
| --- | --- | --- |
| clut-cache | `patching file 'upstream/.../gs_cpu_backend.h'`, `patching file 'upstream/.../gs_cpu_backend.cpp'` | `/tmp/ps2xgs-build/g5-dryrun-clut-cache.txt` |
| rmw-lookup | `patching file 'upstream/.../gs_cpu_backend.cpp'` | `/tmp/ps2xgs-build/g5-dryrun-rmw-lookup.txt` |
| present-scratch | `patching file 'upstream/.../gs_cpu_backend.cpp'` | `/tmp/ps2xgs-build/g5-dryrun-present-scratch.txt` |

Composition: all three applied in order (clut, rmw, present) to a
fourth scratch copy; both resulting files are byte-equal (`cmp`) to
the generator's combined output (`COMBINED-H-MATCH`,
`COMBINED-C-MATCH`).

## 2. Proof target (Step 2)

`patched-cpu` factory backend: the staged patched sources compiled
beside the pristine `cpu` backend. Build-time apply
(`cmake/apply_g5_patches.cmake`, invoked by a CMake custom command):
copy the two pinned files to `${CMAKE_BINARY_DIR}/g5-scratch/work/`,
run `patch -p1` × 3, stage results to
`${CMAKE_BINARY_DIR}/g5-patched/`. A failed hunk fails the build.
`upstream/` is never written (submodule status clean after build).

| mechanism | value |
| --- | --- |
| staged `.cpp` | `${CMAKE_BINARY_DIR}/g5-patched/src/gs_cpu_backend.cpp` |
| staged header | `${CMAKE_BINARY_DIR}/g5-patched/include/runtime/gs/gs_cpu_backend.h` |
| library | `ps2xgs_patched_cpu` (staged `.cpp` + `harness/patched_cpu_factory.cpp` only; `ps2_gs_memory.cpp` not recompiled) |
| ODR separation | `-DGSCpuBackend=GSPatchedCpuBackend`, PRIVATE include dirs (staged dir first) |
| factory | `--backend patched-cpu` → `createPatchedCpuBackend()` |
| pristine isolation | `cpu`/`strict`/`spike` TUs see only `upstream/` headers |

Staged-vs-applied receipts: `cmp` of the CMake-staged files against
the §1 fourth-scratch outputs reports identical for both files
(`STAGED-C-MATCH`, `STAGED-H-MATCH`).

Files added/changed for the target (besides `upstream-patches/` and
this report):

| file | change |
| --- | --- |
| `cmake/apply_g5_patches.cmake` | new: copy + `patch -p1` × 3 + stage |
| `harness/patched_cpu_backend.h` | new: `createPatchedCpuBackend()` decl |
| `harness/patched_cpu_factory.cpp` | new: creator TU (sees patched header) |
| `CMakeLists.txt` | new custom command + `ps2xgs_patched_cpu` lib + `patched-identity` test; `gsreplay` links the lib |
| `harness/backend_factory.cpp` | `patched-cpu` branch + error-string word |
| `tests/replay_patched.sh` | new: per-capture `--backend patched-cpu` gate |

Smoke outputs (recorded as observed, current binary):

```
present 0 64x64 max=0 mean=0.0000 bad=0.0000% vram=yes median_ms=0.528 PASS  (tex-t8-csm1, patched-cpu)
present 0 64x64 max=0 mean=0.0000 bad=0.0000% vram=yes median_ms=0.553 PASS  (blend-a-cs, patched-cpu)
present 0 64x64 max=0 mean=0.0000 bad=0.0000% vram=yes median_ms=5.979 PASS  (present-both, patched-cpu)
```

### 2a. Identity replay, patched-cpu vs cpu references (102 rows)

Threshold 4, max-bad-pct 1.0 (`gsreplay --backend patched-cpu --json`,
one present per capture). 102 rows, all max 0, vram yes.

| capture | WxH | max | mean | bad% | vram | median_ms |
| --- | --- | --- | --- | --- | --- | --- |
| atest-afail-fb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.522 |
| atest-afail-rgb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.548 |
| atest-afail-zb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.521 |
| atest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.532 |
| atest-date-datm0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.587 |
| atest-date-datm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.578 |
| atest-equal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.564 |
| atest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.546 |
| atest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.566 |
| atest-lequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.537 |
| atest-less.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.606 |
| atest-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.546 |
| atest-notequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.570 |
| blend-a-cd.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.578 |
| blend-a-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.586 |
| blend-b-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.552 |
| blend-c-ad.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.562 |
| blend-c-fix.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.542 |
| blend-colclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.543 |
| blend-d-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.556 |
| blend-dthe.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.547 |
| blend-fba.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.551 |
| blend-pabe.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.553 |
| clear-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.613 |
| clear-ct16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.565 |
| clear-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.564 |
| clear-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.530 |
| fog-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.587 |
| fog-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.537 |
| fog-textured.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.571 |
| iso-aa1-clear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.583 |
| iso-aa1-set.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.577 |
| iso-colclamp-off.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.559 |
| iso-colclamp-on.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.549 |
| iso-dthe-off.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.556 |
| iso-dthe-on.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.562 |
| iso-scanmsk-set.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.595 |
| iso-scanmsk-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.553 |
| iso-zte-off-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.581 |
| iso-zte-off-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.529 |
| iso-zte-on-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.549 |
| mip-chain-mxl0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.545 |
| mip-chain-mxl1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.540 |
| mip-chain-mxl2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.584 |
| mip-chain-stq-mxl2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.559 |
| present-both.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 6.127 |
| present-circuit2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.587 |
| present-fbp0-black.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 3.389 |
| present-fbp0-empty.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 6.134 |
| present-fbp0-multi.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 3.367 |
| present-fbp0-nonblack.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.552 |
| present-field.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 3.244 |
| present-frame.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.582 |
| present-progressive.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.546 |
| prim-fbmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.555 |
| prim-line-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.548 |
| prim-line-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.574 |
| prim-point.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.580 |
| prim-scissor.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.602 |
| prim-sprite.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.566 |
| prim-trifan.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.557 |
| prim-trilist-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.576 |
| prim-trilist-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.560 |
| prim-tristrip.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.584 |
| prim-xyoffset.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.584 |
| tex-clamp-clamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.567 |
| tex-clamp-rclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.588 |
| tex-clamp-repeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.556 |
| tex-clamp-rrepeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.566 |
| tex-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.572 |
| tex-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.558 |
| tex-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.556 |
| tex-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.566 |
| tex-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.567 |
| tex-stq.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.562 |
| tex-t4-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.560 |
| tex-t4-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.556 |
| tex-t8-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.562 |
| tex-t8-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.559 |
| tex-t8h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.571 |
| tex-texa.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.556 |
| tex-uv.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.596 |
| tex1-filter-agree-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.551 |
| tex1-filter-agree-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.579 |
| tex1-filter-disagree-lin.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.561 |
| tex1-filter-disagree-near.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.565 |
| tex1-filter-mixed-mmin.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.566 |
| transfer-h2l-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.559 |
| transfer-h2l-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.575 |
| transfer-h2l-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.561 |
| transfer-h2l-t4.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.554 |
| transfer-h2l-t8.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.408 |
| transfer-l2h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.601 |
| transfer-l2l.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.552 |
| ztest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.538 |
| ztest-gequal-z16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.542 |
| ztest-gequal-z16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.536 |
| ztest-gequal-z24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.573 |
| ztest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.580 |
| ztest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.555 |
| ztest-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.578 |
| ztest-zmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.573 |

### 2b. Timing deltas

G4 §2b capture set; G3/G4 method: backends interleaved per run, 5
runs, median (min-max), same machine, recorded as observed.
`median_ms` (single `Present` call) is the Present-patch metric;
`submit_ms_total` is the no-change control for the Present patch and
carries the CLUT/RMW Submit effects.

| capture | submits | cpu median_ms med (min-max) | patched-cpu median_ms med (min-max) | cpu submit med (min-max) | patched-cpu submit med (min-max) |
| --- | --- | --- | --- | --- | --- |
| tex-t8-csm1.gscap | 1 | 16.047 (16.032-16.060) | 0.561 (0.545-0.572) | 0.272 (0.271-0.273) | 0.236 (0.233-0.238) |
| blend-a-cs.gscap | 2 | 16.042 (16.007-16.147) | 0.571 (0.545-0.584) | 0.647 (0.645-0.654) | 0.573 (0.570-0.584) |
| transfer-l2l.gscap | 0 | 15.973 (15.971-16.025) | 0.557 (0.550-0.572) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-both.gscap | 0 | 29.408 (29.323-29.501) | 6.174 (6.162-6.277) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-field.gscap | 0 | 18.701 (18.688-19.154) | 3.261 (3.253-3.280) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-fbp0-black.gscap | 0 | 22.743 (22.705-22.788) | 3.423 (3.407-3.513) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |

## 3. CTest output (Step 3)

Receipt: `/tmp/ps2xgs-build/g5-ctest.log`. Full output (12 tests;
`patched-identity` is new; timing is machine-dependent and not
asserted):

```
Test project /tmp/ps2xgs-build
      Start  1: gsregs-roundtrip
 1/12 Test  #1: gsregs-roundtrip .................   Passed    0.00 sec
      Start  2: gscap-roundtrip
 2/12 Test  #2: gscap-roundtrip ..................   Passed    0.18 sec
      Start  3: gen-synth
 3/12 Test  #3: gen-synth ........................   Passed    6.02 sec
      Start  4: identity-replay
 4/12 Test  #4: identity-replay ..................   Passed   15.52 sec
      Start  5: run-census
 5/12 Test  #5: run-census .......................   Passed    5.70 sec
      Start  6: census-features
 6/12 Test  #6: census-features ..................   Passed    0.08 sec
      Start  7: strict-diffs-on-mip
 7/12 Test  #7: strict-diffs-on-mip ..............   Passed    0.44 sec
      Start  8: strict-exact-on-control
 8/12 Test  #8: strict-exact-on-control ..........   Passed    1.34 sec
      Start  9: cpu-identity-on-new-captures
 9/12 Test  #9: cpu-identity-on-new-captures .....   Passed    2.98 sec
      Start 10: present-heuristic
10/12 Test #10: present-heuristic ................   Passed    0.32 sec
      Start 11: spike-identity
11/12 Test #11: spike-identity ...................   Passed   12.86 sec
      Start 12: patched-identity
12/12 Test #12: patched-identity .................   Passed   13.59 sec

100% tests passed out of 12

Total Test time (real) =  59.05 sec
```

Test mapping: (4) `cpu` identity over all 102 (byte-identical default
unchanged); (7–8) `strict` sensitivity/specificity unchanged; (10)
Present-heuristic pins; (11) `spike` identity; (12)
`gsreplay --backend patched-cpu` exits 0 on all 102 captures.

Capture stability: the 4 `present-fbp0-*` md5s after the suite's
regeneration match G3 §1 exactly (`f7af2946…`, `1eeb0340…`,
`1340e0ba…`, `92f6bba9…`); no captures committed.

## 4. Exact commands

```
cmake -S . -B /tmp/ps2xgs-build -G Ninja
cmake --build /tmp/ps2xgs-build -j4
ctest --test-dir /tmp/ps2xgs-build --output-on-failure
/tmp/ps2xgs-build/gsreplay <capture.gscap> --backend cpu|spike|patched-cpu [--time-submits] [--json out.json]
sh /tmp/g5-matrix.sh /tmp/ps2xgs-build/gsreplay "/Volumes/Extreme SSD/ps2xgs/synth" /tmp/ps2xgs-build/g5-replay-patched
sh /tmp/g5-time.sh /tmp/ps2xgs-build/gsreplay "/Volumes/Extreme SSD/ps2xgs/synth" <captures...>
cp -r upstream /tmp/g5-receipt-<name>/upstream
(cd /tmp/g5-receipt-<name> && patch -p1 --dry-run < upstream-patches/<name>.patch)
```

## 5. Receipt paths

- `/tmp/ps2xgs-build/` — binaries (`gsreplay` with `patched-cpu`),
  `g5-ctest.log`, `g5-replay-patched/*.json` (102, §2a) +
  `g5-identity-table.md`, `g5-time-raw.txt` (60 lines) +
  `g5-time-table.md` (§2b), `g5-dryrun-*.txt` (§1),
  `g5-patched/` (staged patched sources), `g5-scratch/` (apply work
  tree).
- `/tmp/g5-patchwork/gen.py` — patch generator (pristine + edits →
  `diff -u`); `/tmp/g5-dryrun/` — per-patch and combined scratch
  applications; `/tmp/g5-matrix.sh`, `/tmp/g5-time.sh` — table scripts.
- `/Volumes/Extreme SSD/ps2xgs/synth/` — 102 `.gscap` captures
  (unchanged; §3 md5s). Nothing over 5 MB in the repo; no captures
  committed.
- Repo commits (prefix `[G5]`, trailer `Orchestrated-By: Muse Code`):
  patches, proof target + CTest gate, this report.

## 6. What I could not do

- The patches inherit the G3/G4 spike caveats unchanged: CLUT-memo
  exactness assumes no draw writes its own CLUT footprint mid-batch
  (G3 §9); RMW covers CT32 frame writes only (G3 §9); the ~2.6 ms
  caller-side frame destroy and the dual-CRT/field/fbp0-fallback temp
  destroys stay (G4 §6); no game/reference captures beyond the
  synthetic set (G0 §10).
- Identity is shown on the 102 synthetic captures only; the upstream
  project has its own CI/review gates outside this repo, and the
  patches were not submitted there.
- All timings are same-machine observations on Apple silicon at the
  repo's default -O0 build (CPU-only, not backend targets), recorded
  with min-max ranges, not claimed.
- `upstream/` untouched throughout; the proof target compiles staged
  scratch copies. Applying the patches to the submodule working tree
  itself was deliberately never done (only `--dry-run` on copies and
  real application inside scratch trees).

