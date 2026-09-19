# Upstream PR prep — CLUT/RMW/Present patches (NOT SUBMITTED)

Prep only. Nothing in this file has left this repo: no `gh`, no PR
submission, no push outside `origin main` of ps2xGS. All numbers below
are cited against `docs/reports/G5.md` (patch inventory, dry-run,
proof target) and the G6 Step 1 reruns (`docs/reports/G6.md` §1);
no copied-but-stale figures.

## 0. Base rev

| item | value |
| --- | --- |
| upstream HEAD | `14b1e5cb39b4af7e6fc12f9a29fdc751efde49d7` |
| describe | `v0.4-31-g14b1e5c` |
| tree state | submodule clean (verified `git -C upstream status`, G6 Step 1) |

Each patch applies to the pristine pinned rev on its own with
`patch -p1` from the repo root (`a/upstream/...` / `b/upstream/...`
paths). Application order for the combined tree: clut, rmw, present.

## 1. Per-patch summary

### 1a. `clut-cache.patch` (`[G3-CLUT]`, G3 §4)

What: per-`Submit` memo of resolved CLUT entries
(`m_clutCache`/`m_clutTag`/`m_clutGen` + `invalidateClutCache`),
invalidated on `Reset`/`Submit`/`TextureFlush`. Touches
`gs_cpu_backend.h` + `gs_cpu_backend.cpp`, 6 hunks, 4376 bytes.

Why (profiled): G4 §1c Submit spin (tex-t8-csm1, cpu backend):
`LookupCLUT` 517 samples (14.8% total), `SampleTexture` 1390 (39.9%).

Measured effect at -O0 (default build, no `-O` flag):

| capture | metric | cpu | with CLUT (spike/clut-only) | source |
| --- | --- | --- | --- | --- |
| tex-t8-csm1 | submit med (min-max) | 0.271 (0.269-0.280) | 0.235 (0.230-0.237) | G3 §4b |
| tex-t8-csm2 | submit med (min-max) | 0.269 (0.265-0.273) | 0.235 (0.227-0.239) | G3 §4b |
| tex-t4-csm1 | submit med (min-max) | 0.272 (0.271-0.275) | 0.220 (0.219-0.229) | G3 §4b |
| tex-t4-csm2 | submit med (min-max) | 0.279 (0.278-0.288) | 0.219 (0.218-0.232) | G3 §4b |
| tex-t8h | submit med (min-max) | 0.265 (0.263-0.266) | 0.229 (0.228-0.231) | G3 §4b |
| tex-linear (CT32 control) | submit med (min-max) | 0.387 (0.384-0.392) | 0.383 (0.380-0.389) | G3 §4b |
| prim-sprite (untextured control) | submit med (min-max) | 0.255 (0.255-0.259) | 0.255 (0.252-0.265) | G3 §4b |
| blend-a-cs (untextured control) | submit med (min-max) | 0.638 (0.632-0.694) | 0.640 (0.636-0.644) | G3 §4b |
| tex-t8-csm1 | submit, patched-cpu combined | 0.272 (0.271-0.273) | 0.236 (0.233-0.238) | G5 §2b |
| tex-t8-csm1 | submit, patched-cpu combined | 0.268 (0.243-0.285) | 0.232 (0.227-0.233) | G6 §1 fresh -O0 |

Measured effect at Release (`-O3 -DNDEBUG`, G6 §1; patched-cpu
carries CLUT+RMW jointly — tex-t8 frame draws are CT32, so RMW also
applies per the G3 §5b CLUT-only control row):

| capture | metric | cpu | patched-cpu |
| --- | --- | --- | --- |
| tex-t8-csm1 | submit med (min-max) | 0.019 (0.019-0.038) | 0.024 (0.022-0.031) |

No CLUT-only binary was built at Release; per-patch isolation at
Release is not measured beyond the untextured RMW control (§1b).

### 1b. `rmw-lookup.patch` (`[G3-RMW]`, G3 §5)

What: single-address read-modify-write for CT32 frame writes in
`WritePixel`: the `frmw` read and the conditional store share one
computed VRAM address via a local copy of the C32 page table
(`ensureC32RmwPage`, `rmwBegin32`/`rmwCommit32`). All other PSMs keep
the two-call path. Touches `gs_cpu_backend.cpp`, 5 hunks, 4893 bytes.

Why (profiled): G4 §1c Submit spin (blend-a-cs, cpu backend):
`Address` 680 samples (18.9%), `PageId` 235 (6.5%), per-pixel
`std::function` dispatch on the read and store paths.

Measured effect at -O0:

| capture | cpu submit | spike-clut (CLUT only) | spike (CLUT+RMW) | source |
| --- | --- | --- | --- | --- |
| blend-a-cs | 0.790 (0.778-0.832) | 0.786 (0.778-0.832) | 0.735 (0.690-0.738) | G3 §5b |
| blend-d-cs | 0.784 (0.772-0.823) | 0.781 (0.770-0.831) | 0.687 (0.685-0.702) | G3 §5b |
| prim-fbmsk | 0.436 (0.432-0.442) | 0.432 (0.429-0.433) | 0.258 (0.255-0.259) | G3 §5b |
| blend-a-cs, patched combined | 0.647 (0.645-0.654) | — | 0.573 (0.570-0.584) | G5 §2b |
| blend-a-cs, patched combined | 0.638 (0.619-0.650) | — | 0.565 (0.564-0.572) | G6 §1 fresh -O0 |

(G3 §5b absolute values ran high under load; G4/G5/G6 reruns agree
with each other. Full 10-row G3 §5b table in `docs/reports/G3.md`.)

Measured effect at Release (G6 §1; blend-a-cs is untextured, so the
patched-cpu delta isolates RMW from CLUT):

| capture | metric | cpu | patched-cpu |
| --- | --- | --- | --- |
| blend-a-cs | submit med (min-max) | 0.037 (0.033-0.044) | 0.034 (0.033-0.039) |

### 1c. `present-scratch.patch` (`[G4-PRESENT]`, G4 §2)

What: bulk Present scratch buffers. `CopyFrameToHostRgba` + dual-CRT
composite: `assign(K, 0)` → range-`assign` from a process-lifetime
mutable zeros buffer. `Present`: thread-local `vector` +
`SnapshotVram` (first-use `resize(4M)`) → thread-local raw buffer +
`malloc`/`memcpy` under the identical lock. Touches
`gs_cpu_backend.cpp`, 4 hunks, 4638 bytes.

Why (profiled): G4 §1: single-shot ~16 ms = 4 MB snapshot
first-`resize` ~11.5 ms + 1.3 MB host-frame `assign(K, 0)` ~3.9 ms
fill at -O0 (G4 §1e); Present spins 56–57% fill-construct (G4 §1b).

Measured effect at -O0 (`median_ms`, single `Present` call):

| capture | cpu | spike (G4 §2b) | patched-cpu (G5 §2b) | patched-cpu (G6 fresh -O0) |
| --- | --- | --- | --- | --- |
| tex-t8-csm1 | 15.846 / 16.047 / 15.848 | 0.578 (0.551-0.582) | 0.561 (0.545-0.572) | 0.560 (0.541-0.578) |
| transfer-l2l | 15.858 / 15.973 / 15.885 | 0.559 (0.542-0.578) | 0.557 (0.550-0.572) | 0.580 (0.554-0.593) |
| present-both | 29.145 / 29.408 / 29.057 | 6.140 (6.108-6.161) | 6.174 (6.162-6.277) | 6.117 (5.843-6.158) |
| present-field | 18.543 / 18.701 / 18.550 | 3.236 (3.210-3.255) | 3.261 (3.253-3.280) | 3.238 (3.204-3.330) |
| present-fbp0-black | 22.519 / 22.743 / 22.528 | 3.377 (3.372-3.409) | 3.423 (3.407-3.513) | 3.397 (3.372-3.433) |

(cpu column: G4 §2b med / G5 §2b med / G6 §1 fresh -O0 med;
min-max ranges in the source tables.)

Measured effect at Release (G6 §1, `median_ms` med (min-max)):

| capture | cpu | patched-cpu |
| --- | --- | --- |
| tex-t8-csm1 | 0.364 (0.337-0.498) | 0.386 (0.379-0.510) |
| transfer-l2l | 0.357 (0.335-0.392) | 0.381 (0.373-0.427) |
| present-both | 0.479 (0.457-0.518) | 0.550 (0.531-0.560) |
| present-field | 0.422 (0.415-0.458) | 0.468 (0.459-0.478) |
| present-fbp0-black | 0.417 (0.405-0.455) | 0.496 (0.463-0.543) |

## 2. Dry-run + proof-target receipts (by reference, G5 §1–§2)

| receipt | path |
| --- | --- |
| dry-run clut-cache | `/tmp/ps2xgs-build/g5-dryrun-clut-cache.txt` |
| dry-run rmw-lookup | `/tmp/ps2xgs-build/g5-dryrun-rmw-lookup.txt` |
| dry-run present-scratch | `/tmp/ps2xgs-build/g5-dryrun-present-scratch.txt` |
| combined apply, byte-equal staged | `COMBINED-H-MATCH`, `COMBINED-C-MATCH` (G5 §1) |
| CMake-staged vs applied, identical | `STAGED-C-MATCH`, `STAGED-H-MATCH` (G5 §2) |
| Release-staged vs -O0-staged, identical | `STAGED-C-O2-MATCH` (G6 §1, `cmp`) |
| proof target | `patched-cpu` factory backend (`cmake/apply_g5_patches.cmake`; staged `g5-patched/`) |
| 102-capture identity, patched-cpu vs cpu | G5 §2a (all max 0, vram yes) |

## 3. Test plan (CTest gate names)

| # | gate | pins |
| --- | --- | --- |
| 1 | `gsregs-roundtrip` | GS register round-trip |
| 2 | `gscap-roundtrip` | capture format round-trip |
| 3 | `gen-synth` | (re)generate the 102 synthetic captures |
| 4 | `identity-replay` | `cpu` identity over all 102 |
| 5 | `run-census` | feature census outputs |
| 6 | `census-features` | census feature checks |
| 7 | `strict-diffs-on-mip` | `strict` sensitivity (mip diffs) |
| 8 | `strict-exact-on-control` | `strict` specificity (controls exact) |
| 9 | `cpu-identity-on-new-captures` | `cpu` identity on new captures |
| 10 | `present-heuristic` | Present fbp==0 fallback heuristic pins |
| 11 | `spike-identity` | `spike` identity over all 102 |
| 12 | `patched-identity` | `patched-cpu` identity over all 102 |

Release run (G6 §3): 12/12 pass, 8.03 s
(receipt `/tmp/ps2xgs-build-o2/g6-ctest.log`).
-O0 run (G5 §3): 12/12 pass, 59.05 s
(receipt `/tmp/ps2xgs-build/g5-ctest.log`).

## 4. Known caveats (G5 §6)

- CLUT-memo exactness assumes no draw writes its own CLUT footprint
  mid-batch (G3 §9).
- RMW covers CT32 frame writes only (G3 §9).
- The ~2.6 ms caller-side frame destroy and the dual-CRT/field/
  fbp0-fallback temp destroys stay (G4 §6).
- No game/reference captures beyond the synthetic set (G0 §10);
  identity is shown on the 102 only.
- Timings are same-machine observations on Apple silicon (CPU-only),
  recorded with min-max ranges: -O0 (G0–G5) and Release
  `-O3 -DNDEBUG` (G6).
- `upstream/` untouched throughout; the proof target compiles staged
  scratch copies. The patches were not submitted upstream.
