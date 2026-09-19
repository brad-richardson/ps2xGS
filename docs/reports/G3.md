# G3 report — Present-heuristic pins + CLUT-cache / RMW-lookup spikes

Brief: G3 runbook. Read: `docs/reports/G2.md` (the `strict`
backend + sensitivity tables), `docs/reports/G0.md` §9 item 4 (the
`Present` `fbp==0` fallback heuristic), upstream
`gs_cpu_backend.cpp` TODOs (`// TODO: clut cache` above
`resolveClutIndex`, `// TODO: only one address lookup for rmw` at
the `frmw` computation in `WritePixel`). Host-only, no device, no
emulator. `cpu` backend default untouched; `upstream/` untouched;
`strict` untouched. Old 98 captures byte-identical (`md5` lists
diff clean before/after every regeneration in this step).

## 1. New captures (4)

`/Volumes/Extreme SSD/ps2xgs/synth/`, 1 present each (64x64).
New bytes 22021506; full set 102 files, 565913724 bytes.

| capture | bytes | presents | md5 |
| --- | --- | --- | --- |
| present-fbp0-black.gscap | 5505347 | 1 | f7af2946aee4c08f4ee6d57b75ffdbee |
| present-fbp0-nonblack.gscap | 5505465 | 1 | 1eeb0340e981752e57244e209c309cc0 |
| present-fbp0-multi.gscap | 5505465 | 1 | 1340e0ba72e1e3ff680911cfbd84b39d |
| present-fbp0-empty.gscap | 5505229 | 1 | 92f6bba96a34e8fd2e1187c52fff385e |

All four: display reads fbp 0 (CT32, fbw 10, progressive,
en1). Candidate fills via `ClearFramebuffer` (64x64 scissor):
fbp 8 red (`0xFF0000FF`), fbp 32 green (`0xFF00FF00`); fbp 0
blue (`0xFFFF0000`) only in `-nonblack`. Page-disjoint within
the 64x64 window at fbw 10 (fbp 0 touches pages {0,10}, fbp 8
touches {8,18}, fbp 32 touches {32,42}), so fills never alias
the display read. "Empty" context = zero `GSFrameReg`
(fbp 0, fbw 0, psm 0).

## 2. Present-behavior table (Step 1)

Observed via `test_present_heuristic` (live replay of the
recorded request through a fresh `cpu` backend on the recorded
VRAM, plus pinned recorded values):

| capture | fbp 0 content | contextFrames[0] | contextFrames[1] | observed sourceFbp | observed pixels (5 spots) |
| --- | --- | --- | --- | --- | --- |
| present-fbp0-black.gscap | black (VRAM zero) | fbp 8, red fill | empty | 8 | (255,0,0,255) |
| present-fbp0-nonblack.gscap | blue fill | fbp 8, red fill | empty | 0 | (0,0,255,255) |
| present-fbp0-multi.gscap | black | fbp 8, red fill | fbp 32, green fill | 8 | (255,0,0,255) |
| present-fbp0-empty.gscap | black | empty | empty | 0 | (0,0,0,255) |

`test_present_heuristic` asserts per capture: live-vs-recorded
max 0, live `sourceFbp` == recorded `sourceFbp` == pinned
value, 5 spot pixels (`(0,0)`, `(63,0)`, `(0,63)`,
`(63,63)`, `(32,32)`) == pinned RGB with alpha 255. Output:

```
PASS present-fbp0-black: sourceFbp=8 rgb=(255,0,0) max=0
PASS present-fbp0-nonblack: sourceFbp=0 rgb=(0,0,255) max=0
PASS present-fbp0-multi: sourceFbp=8 rgb=(255,0,0) max=0
PASS present-fbp0-empty: sourceFbp=0 rgb=(0,0,0) max=0
test_present_heuristic: 4/4 pinned behaviors hold
```

CTest `present-heuristic` runs it over `${PS2XGS_SYNTH_DIR}`
(fails if the heuristic changes).

## 3. Spike vehicle

`GSCpuBackend` is `final` with private non-virtual
`LookupCLUT`/`WritePixel`, so a wrapper cannot reach either
TODO site. Spikes live in a fork: `harness/spike_backend.h`
(95 lines), `harness/spike_backend.cpp` (2018 lines), class
`GSSpikeBackend`, factory name `"spike"` (the runbook's
flagged variant). The `cpu` target still compiles pristine
upstream; `strict` is untouched. `spike` is cpu-equivalent by
design (identity-exact everywhere); `strict` keeps its 14
intentional diffs.

Supporting changes: `gsreplay --time-submits` (prints total
`Submit` time; `median_ms` times `Present` calls only, so
Submit-path spikes are invisible to it; default output
unchanged), `tests/replay_spike.sh` + CTest `spike-identity`
(all captures `--backend spike` must exit 0),
`tests/check_census.py` counts 98→102 (submits unchanged at
138; the 4 new captures hold clears + presents only).

## 4. CLUT-cache spike (Step 2)

Per-`Submit` memo of resolved palette entries: within one
`Submit` every `LookupCLUT` parameter but the texel index is
fixed (single batch state), so a 256-entry
generation-tagged table keyed by raw index is exact unless a
draw writes its own CLUT footprint mid-batch. Invalidated
(generation bump) in `Submit`, `TextureFlush`, `Reset`.

Semantic diff vs upstream (class rename + include line
normalized away; full fork diff is the commit):

```
--- upstream/ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp
+++ harness/spike_backend.cpp (step 2, rename-normalized)
@@ -1,3 +1,9 @@
+// G3 steps 2-3: spike backend, forked from upstream
+// ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp at the pinned revision.
+// Upstream file untouched. G3 deltas vs upstream are marked [G3-CLUT]
+// and [G3-RMW]; see docs/reports/G3.md for the diff and identity
+// tables.
+
 #include "runtime/gs/gs_cpu_backend.h"
 #include "runtime/gs/ps2_gs_common.h"
 #include "runtime/gs/ps2_gs_psmct16.h"
@@ -553,8 +559,19 @@
     ResetUnlocked();
 }
 
+void GSCpuBackend::invalidateClutCache()
+{
+    // [G3-CLUT] Generation bump; tags clear on 32-bit wrap.
+    if (++m_clutGen == 0)
+    {
+        m_clutTag.fill(0);
+        m_clutGen = 1;
+    }
+}
+
 void GSCpuBackend::ResetUnlocked()
 {
+    invalidateClutCache(); // [G3-CLUT]
     m_transfer = {};
     m_transfer.direction = 3u;
     m_transferState = {};
@@ -568,6 +585,7 @@
     std::lock_guard<std::mutex> lock(m_mutex);
     if (!m_vram || batch.vertexCount == 0u)
         return;
+    invalidateClutCache(); // [G3-CLUT] per-Submit memo scope
     DrawPrimitive(batch);
 }
 
@@ -580,6 +598,8 @@
 {
     // CPU texture reads are coherent with local memory. Future cached/GPU
     // backends use this boundary to invalidate texture views.
+    std::lock_guard<std::mutex> lock(m_mutex);
+    invalidateClutCache(); // [G3-CLUT] documented invalidation boundary
 }
 
 void GSCpuBackend::Sync(GSSyncReason)
@@ -947,26 +967,41 @@
                                   uint8_t csa,
                                   uint8_t sourcePsm)
 {
+    // [G3-CLUT] Per-Submit memo: every parameter but the texel index
+    // is fixed within a Submit (single batch state), so the resolved
+    // entry is a pure function of the index until the next
+    // Submit/TextureFlush/Reset. Exact unless a draw writes its own
+    // CLUT footprint mid-batch.
+    if (m_clutTag[index] == m_clutGen)
+        return m_clutCache[index];
+
     const uint32_t clutIndex = resolveClutIndex(index, cpsm, csm, csa, sourcePsm);
     const uint32_t clutWidth = (state.texclut.cbw != 0u) ? static_cast<uint32_t>(state.texclut.cbw) : 1u;
     const uint32_t clutX = static_cast<uint32_t>(state.texclut.cou) + (clutIndex & 0x0Fu);
     const uint32_t clutY = static_cast<uint32_t>(state.texclut.cov) + (clutIndex >> 4);
 
+    uint32_t resolved = 0xFFFF00FFu;
     switch (cpsm)
     {
     case GS_PSM_CT32:
-        return applyTexa(state.texa, cpsm, GSMem::ReadCT32(m_vram, cbp, clutWidth, clutX, clutY));
+        resolved = applyTexa(state.texa, cpsm, GSMem::ReadCT32(m_vram, cbp, clutWidth, clutX, clutY));
+        break;
     case GS_PSM_CT24:
-        return applyTexa(state.texa, cpsm, GSMem::ReadCT24(m_vram, cbp, clutWidth, clutX, clutY));
+        resolved = applyTexa(state.texa, cpsm, GSMem::ReadCT24(m_vram, cbp, clutWidth, clutX, clutY));
+        break;
     case GS_PSM_CT16:
-        return applyTexa(state.texa, cpsm, Rgba5551ToRgba8888(GSMem::ReadCT16(m_vram, cbp, clutWidth, clutX, clutY)));
+        resolved = applyTexa(state.texa, cpsm, Rgba5551ToRgba8888(GSMem::ReadCT16(m_vram, cbp, clutWidth, clutX, clutY)));
+        break;
     case GS_PSM_CT16S:
-        return applyTexa(state.texa, cpsm, Rgba5551ToRgba8888(GSMem::ReadCT16S(m_vram, cbp, clutWidth, clutX, clutY)));
+        resolved = applyTexa(state.texa, cpsm, Rgba5551ToRgba8888(GSMem::ReadCT16S(m_vram, cbp, clutWidth, clutX, clutY)));
+        break;
     default:
         break;
     }
 
-    return 0xFFFF00FFu;
+    m_clutCache[index] = resolved; // [G3-CLUT]
+    m_clutTag[index] = m_clutGen;  // [G3-CLUT]
+    return resolved;
 }
 
 uint32_t GSCpuBackend::SampleTexture(const GSDrawState &state, float s, float t, float q, uint16_t u, uint16_t v)
```

Header delta (same normalization): `invalidateClutCache()`
decl, `m_clutCache`/`m_clutTag` (256 `uint32_t` each),
`m_clutGen`, `<cstdint>` include.

### 4a. Identity replay, spike(CLUT) vs cpu references (102 rows)

Threshold 4, max-bad-pct 1.0. 102 rows, all max 0, vram yes.

| capture | WxH | max | mean | bad% | vram | median_ms |
| --- | --- | --- | --- | --- | --- | --- |
| atest-afail-fb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.253 |
| atest-afail-rgb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.101 |
| atest-afail-zb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.095 |
| atest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.022 |
| atest-date-datm0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.155 |
| atest-date-datm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.059 |
| atest-equal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.082 |
| atest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.016 |
| atest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.918 |
| atest-lequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.152 |
| atest-less.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.128 |
| atest-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.107 |
| atest-notequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.164 |
| blend-a-cd.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.991 |
| blend-a-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.012 |
| blend-b-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.025 |
| blend-c-ad.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.122 |
| blend-c-fix.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.194 |
| blend-colclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.182 |
| blend-d-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.135 |
| blend-dthe.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.122 |
| blend-fba.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.139 |
| blend-pabe.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.941 |
| clear-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.156 |
| clear-ct16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.145 |
| clear-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.095 |
| clear-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.122 |
| fog-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.144 |
| fog-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.278 |
| fog-textured.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.972 |
| iso-aa1-clear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.080 |
| iso-aa1-set.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.997 |
| iso-colclamp-off.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.170 |
| iso-colclamp-on.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.225 |
| iso-dthe-off.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.106 |
| iso-dthe-on.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.172 |
| iso-scanmsk-set.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.141 |
| iso-scanmsk-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.205 |
| iso-zte-off-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.204 |
| iso-zte-off-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.512 |
| iso-zte-on-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.904 |
| mip-chain-mxl0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.279 |
| mip-chain-mxl1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.209 |
| mip-chain-mxl2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.133 |
| mip-chain-stq-mxl2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.201 |
| present-both.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 29.325 |
| present-circuit2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.196 |
| present-fbp0-black.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.870 |
| present-fbp0-empty.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 29.503 |
| present-fbp0-multi.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.973 |
| present-fbp0-nonblack.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.172 |
| present-field.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.866 |
| present-frame.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.075 |
| present-progressive.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.253 |
| prim-fbmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.154 |
| prim-line-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.111 |
| prim-line-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.171 |
| prim-point.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.966 |
| prim-scissor.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.400 |
| prim-sprite.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.134 |
| prim-trifan.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.099 |
| prim-trilist-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.189 |
| prim-trilist-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.121 |
| prim-tristrip.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.276 |
| prim-xyoffset.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.148 |
| tex-clamp-clamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.114 |
| tex-clamp-rclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.955 |
| tex-clamp-repeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.945 |
| tex-clamp-rrepeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.149 |
| tex-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.987 |
| tex-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.205 |
| tex-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.122 |
| tex-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.038 |
| tex-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.209 |
| tex-stq.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.045 |
| tex-t4-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.100 |
| tex-t4-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.075 |
| tex-t8-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.009 |
| tex-t8-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.155 |
| tex-t8h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.145 |
| tex-texa.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.085 |
| tex-uv.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.007 |
| tex1-filter-agree-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.152 |
| tex1-filter-agree-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.118 |
| tex1-filter-disagree-lin.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.026 |
| tex1-filter-disagree-near.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.128 |
| tex1-filter-mixed-mmin.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.229 |
| transfer-h2l-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.147 |
| transfer-h2l-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.087 |
| transfer-h2l-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.109 |
| transfer-h2l-t4.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.998 |
| transfer-h2l-t8.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.941 |
| transfer-l2h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.131 |
| transfer-l2l.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.284 |
| ztest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.047 |
| ztest-gequal-z16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.053 |
| ztest-gequal-z16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.149 |
| ztest-gequal-z24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.941 |
| ztest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.227 |
| ztest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.195 |
| ztest-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.172 |
| ztest-zmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.066 |

### 4b. Timing deltas, CLUT-heavy captures

`submit_ms_total` (all `Submit`s in the replay), median of 5
runs, backends interleaved per run, same machine, step-2
binary (`gsreplay-step2-clut`), recorded as observed.
`median_ms` (single `Present` call) before/after alongside.

| capture | submits | cpu submit med (min-max) | spike submit med (min-max) | cpu median_ms | spike median_ms |
| --- | --- | --- | --- | --- | --- |
| tex-t8-csm1.gscap | 1 | 0.271 (0.269-0.280) | 0.235 (0.230-0.237) | 16.022 | 16.009 |
| tex-t8-csm2.gscap | 1 | 0.269 (0.265-0.273) | 0.235 (0.227-0.239) | 16.070 | 16.155 |
| tex-t4-csm1.gscap | 1 | 0.272 (0.271-0.275) | 0.220 (0.219-0.229) | 16.116 | 16.100 |
| tex-t4-csm2.gscap | 1 | 0.279 (0.278-0.288) | 0.219 (0.218-0.232) | 16.071 | 16.075 |
| tex-t8h.gscap | 1 | 0.265 (0.263-0.266) | 0.229 (0.228-0.231) | 16.189 | 16.145 |
| tex-linear.gscap (CT32 control) | 1 | 0.387 (0.384-0.392) | 0.383 (0.380-0.389) | 16.000 | 16.038 |
| prim-sprite.gscap (untextured control) | 1 | 0.255 (0.255-0.259) | 0.255 (0.252-0.265) | 15.881 | 16.134 |
| blend-a-cs.gscap (untextured control) | 2 | 0.638 (0.632-0.694) | 0.640 (0.636-0.644) | 16.108 | 16.012 |

## 5. RMW-lookup spike (Step 3)

Single-address read-modify-write for CT32 frame writes in
`WritePixel`: the `frmw` read and the conditional store share
one computed VRAM address via a fork-local copy of the C32
page table (same literals + same
`PixelStorageTraits::Address` math as upstream
`ps2_gs_memory.cpp`); the byte pointer is reused for the
store. Table initialized once in the constructor. All other
PSMs keep the original two-call path. Diff vs the step-2
commit (`git diff 69b7f89 6bfe882 -- harness/spike_backend.cpp`):

```
@@ -474,12 +474,18 @@ namespace
     }
 }
 
+namespace
+{
+void ensureSpikePageC32(); // [G3-RMW] defined above WritePixel
+} // namespace
+
 GSSpikeBackend::GSSpikeBackend()
 {
     using namespace GSMem;
     static std::once_flag lookupTablesOnce;
     std::call_once(lookupTablesOnce, []()
                    { InitLookupTables(); });
+    ensureSpikePageC32(); // [G3-RMW] fork-local C32 page table, once
     for (size_t i = 0; i < kPsmHandlerCount; ++i)
     {
         switch (i)
@@ -793,6 +799,76 @@ void GSSpikeBackend::DrawPrimitive(const GSPrimitiveBatch &batch)
     }
 }
 
+namespace
+{
+
+// [G3-RMW] Single-address read-modify-write for CT32 frame writes.
+// Upstream computes the swizzled VRAM address twice per frmw pixel
+// (ReadVramUnlocked for the blend/fbmsk/alpha read, WriteVramUnlocked
+// for the store). This path computes it once via a fork-local copy of
+// the C32 page table (same literals + same
+// PixelStorageTraits::Address math as upstream ps2_gs_memory.cpp)
+// and reuses the byte pointer for the conditional store. All other
+// PSMs keep the original two-call path.
+using SpikeC32Traits = GSMem::PixelStorageTraits<GSMem::C32>;
+SpikeC32Traits::PageLookupTableT g_spikePageC32{};
+std::once_flag g_spikePageC32Once;
+
+void ensureSpikePageC32()
+{
+    // Called once from the constructor (never per-pixel: call_once
+    // costs a lock on every invocation even after initialization).
+    std::call_once(g_spikePageC32Once, []() {
+        static constexpr SpikeC32Traits::BlockLookupTableT kBlock{{
+            {0, 1, 4, 5, 16, 17, 20, 21},
+            {2, 3, 6, 7, 18, 19, 22, 23},
+            {8, 9, 12, 13, 24, 25, 28, 29},
+            {10, 11, 14, 15, 26, 27, 30, 31},
+        }};
+        static constexpr SpikeC32Traits::ColumnLookupTableT kColumn{{
+            {0, 1, 4, 5, 8, 9, 12, 13},
+            {2, 3, 6, 7, 10, 11, 14, 15},
+            {16, 17, 20, 21, 24, 25, 28, 29},
+            {18, 19, 22, 23, 26, 27, 30, 31},
+            {32, 33, 36, 37, 40, 41, 44, 45},
+            {34, 35, 38, 39, 42, 43, 46, 47},
+            {48, 49, 52, 53, 56, 57, 60, 61},
+            {50, 51, 54, 55, 58, 59, 62, 63},
+        }};
+        SpikeC32Traits::InitPageLookupTable(g_spikePageC32, kBlock, kColumn);
+    });
+}
+
+struct SpikeRmw32
+{
+    uint8_t *ptr = nullptr;
+};
+
+bool spikeRmwBegin(uint32_t base, uint32_t bw, uint32_t x, uint32_t y, uint8_t *vram,
+                   SpikeRmw32 &slot, uint32_t &oldValue)
+{
+    if (vram == nullptr)
+        return false;
+    // Table is initialized in the constructor; no per-pixel gate here.
+    // Same address + byte math as PixelStorageTraits<C32>::Read.
+    const uint32_t pixelAddr = SpikeC32Traits::Address(g_spikePageC32, base, bw, x, y);
+    const uint32_t bits = pixelAddr * 32u;
+    const uint32_t byteAddr = (bits / 8u) & (uint32_t)(GSMem::MEMORY_SIZE - sizeof(uint32_t));
+    slot.ptr = &vram[byteAddr];
+    uint32_t v = 0;
+    std::memcpy(&v, slot.ptr, sizeof(v));
+    oldValue = v;
+    return true;
+}
+
+void spikeRmwCommit(const SpikeRmw32 &slot, uint32_t newValue)
+{
+    // Same store as PixelStorageTraits<C32>::Write (plain u32 memcpy).
+    std::memcpy(slot.ptr, &newValue, sizeof(newValue));
+}
+
+} // namespace
+
 void GSSpikeBackend::WritePixel(const GSDrawState &state, int x, int y, int z, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t fog)
 {
     const auto &ctx = state.context;
@@ -835,9 +911,19 @@ void GSSpikeBackend::WritePixel(const GSDrawState &state, int x, int y, int z, u
 
     u32 rawFramebufferPixel = 0;
     u32 fbrgba = 0;
+    SpikeRmw32 rmwSlot{}; // [G3-RMW]
+    bool rmwActive = false; // [G3-RMW]
     if (frmw)
     {
-        rawFramebufferPixel = ReadVramUnlocked(fpsm, fbp, fbw, x, y);
+        // [G3-RMW] CT32 shares one address lookup between the read
+        // and the conditional store below; other PSMs keep the
+        // original two-call path.
+        if (fpsm == GS_PSM_CT32 &&
+            spikeRmwBegin(fbp, fbw, (uint32_t)x, (uint32_t)y, m_vram, rmwSlot,
+                          rawFramebufferPixel))
+            rmwActive = true;
+        else
+            rawFramebufferPixel = ReadVramUnlocked(fpsm, fbp, fbw, x, y);
         fbrgba = rawFramebufferPixel;
 
         if (bitsPerPixel(fpsm) == 16)
@@ -950,7 +1036,10 @@ void GSSpikeBackend::WritePixel(const GSDrawState &state, int x, int y, int z, u
             pixel = Rgba8888ToRgba5551(pixel);
         }
 
-        WriteVramUnlocked(fpsm, fbp, fbw, x, y, pixel);
+        if (rmwActive) // [G3-RMW] store through the shared lookup
+            spikeRmwCommit(rmwSlot, pixel);
+        else
+            WriteVramUnlocked(fpsm, fbp, fbw, x, y, pixel);
     }
 
     if (writeMask.writeDepth && !ctx.zbuf.zmask)
```

Work note (recorded, not committed as such): the first cut
called the `call_once` table init per pixel and measured
~1.7x slower than `cpu` on RMW-heavy captures
(e.g. `blend-a-cs` 1.166 vs 0.656); moving init to the
constructor produced the table below. Only the constructor
version was committed.

### 5a. Identity replay, spike(CLUT+RMW) vs cpu references (102 rows)

Threshold 4, max-bad-pct 1.0. 102 rows, all max 0, vram yes.

| capture | WxH | max | mean | bad% | vram | median_ms |
| --- | --- | --- | --- | --- | --- | --- |
| atest-afail-fb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.104 |
| atest-afail-rgb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.236 |
| atest-afail-zb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.144 |
| atest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.013 |
| atest-date-datm0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.217 |
| atest-date-datm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.129 |
| atest-equal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.295 |
| atest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.169 |
| atest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.322 |
| atest-lequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.248 |
| atest-less.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.301 |
| atest-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.913 |
| atest-notequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.008 |
| blend-a-cd.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.218 |
| blend-a-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.942 |
| blend-b-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.975 |
| blend-c-ad.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.264 |
| blend-c-fix.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.301 |
| blend-colclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.299 |
| blend-d-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.198 |
| blend-dthe.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.224 |
| blend-fba.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.320 |
| blend-pabe.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.034 |
| clear-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.523 |
| clear-ct16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.022 |
| clear-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.054 |
| clear-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.620 |
| fog-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.665 |
| fog-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.742 |
| fog-textured.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.727 |
| iso-aa1-clear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.720 |
| iso-aa1-set.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.672 |
| iso-colclamp-off.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.712 |
| iso-colclamp-on.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.672 |
| iso-dthe-off.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.693 |
| iso-dthe-on.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.572 |
| iso-scanmsk-set.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.576 |
| iso-scanmsk-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.544 |
| iso-zte-off-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.494 |
| iso-zte-off-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.523 |
| iso-zte-on-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.549 |
| mip-chain-mxl0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.420 |
| mip-chain-mxl1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.453 |
| mip-chain-mxl2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.506 |
| mip-chain-stq-mxl2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.427 |
| present-both.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 35.189 |
| present-circuit2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.518 |
| present-fbp0-black.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 26.910 |
| present-fbp0-empty.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 34.033 |
| present-fbp0-multi.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 27.463 |
| present-fbp0-nonblack.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.350 |
| present-field.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 21.738 |
| present-frame.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.364 |
| present-progressive.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.380 |
| prim-fbmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.319 |
| prim-line-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.378 |
| prim-line-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.338 |
| prim-point.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.329 |
| prim-scissor.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.348 |
| prim-sprite.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.409 |
| prim-trifan.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.417 |
| prim-trilist-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.357 |
| prim-trilist-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.062 |
| prim-tristrip.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.467 |
| prim-xyoffset.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.452 |
| tex-clamp-clamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.335 |
| tex-clamp-rclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.429 |
| tex-clamp-repeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.390 |
| tex-clamp-rrepeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.411 |
| tex-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.460 |
| tex-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.376 |
| tex-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.355 |
| tex-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.303 |
| tex-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.349 |
| tex-stq.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.280 |
| tex-t4-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.466 |
| tex-t4-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.379 |
| tex-t8-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.426 |
| tex-t8-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.449 |
| tex-t8h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.327 |
| tex-texa.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.489 |
| tex-uv.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.246 |
| tex1-filter-agree-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 20.692 |
| tex1-filter-agree-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 20.644 |
| tex1-filter-disagree-lin.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.352 |
| tex1-filter-disagree-near.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.411 |
| tex1-filter-mixed-mmin.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.484 |
| transfer-h2l-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.346 |
| transfer-h2l-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.312 |
| transfer-h2l-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.397 |
| transfer-h2l-t4.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.400 |
| transfer-h2l-t8.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.186 |
| transfer-l2h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.408 |
| transfer-l2l.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 20.654 |
| ztest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.358 |
| ztest-gequal-z16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.383 |
| ztest-gequal-z16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.444 |
| ztest-gequal-z24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 20.726 |
| ztest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.412 |
| ztest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.447 |
| ztest-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 20.749 |
| ztest-zmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.440 |

### 5b. Timing deltas, RMW-affected captures

`submit_ms_total`, median of 5 runs, backends interleaved
per run, same machine, recorded as observed. `cpu` = current
binary `--backend cpu`; `spike-clut` = preserved step-2
binary (`gsreplay-step2-clut`) `--backend spike`;
`spike` = current binary (CLUT+RMW) `--backend spike`.

| capture | submits | cpu submit med (min-max) | spike-clut submit med (min-max) | spike submit med (min-max) |
| --- | --- | --- | --- | --- |
| blend-a-cs.gscap | 2 | 0.790 (0.778-0.832) | 0.786 (0.778-0.832) | 0.735 (0.690-0.738) |
| blend-d-cs.gscap | 2 | 0.784 (0.772-0.823) | 0.781 (0.770-0.831) | 0.687 (0.685-0.702) |
| blend-c-fix.gscap | 2 | 0.785 (0.776-0.788) | 0.788 (0.778-0.845) | 0.704 (0.686-0.740) |
| prim-fbmsk.gscap | 1 | 0.436 (0.432-0.442) | 0.432 (0.429-0.433) | 0.258 (0.255-0.259) |
| atest-date-datm0.gscap | 3 | 0.856 (0.854-0.925) | 0.851 (0.847-0.853) | 0.736 (0.718-0.775) |
| atest-date-datm1.gscap | 3 | 0.860 (0.843-0.922) | 0.859 (0.845-0.903) | 0.734 (0.721-0.769) |
| atest-afail-rgb.gscap | 2 | 0.724 (0.687-0.730) | 0.722 (0.684-0.739) | 0.644 (0.599-0.650) |
| blend-pabe.gscap | 3 | 0.839 (0.837-0.852) | 0.792 (0.782-0.841) | 0.737 (0.735-0.743) |
| prim-sprite.gscap (no-frmw control) | 1 | 0.335 (0.314-0.350) | 0.332 (0.315-0.337) | 0.316 (0.312-0.333) |
| tex-t8-csm1.gscap (CLUT-only control) | 1 | 0.347 (0.330-0.364) | 0.302 (0.299-0.316) | 0.284 (0.283-0.313) |

## 6. CTest output

Receipt: `/tmp/ps2xgs-build/g3-ctest.log`. Full output:

```
Test project /tmp/ps2xgs-build
      Start  1: gsregs-roundtrip
 1/11 Test  #1: gsregs-roundtrip .................   Passed    0.00 sec
      Start  2: gscap-roundtrip
 2/11 Test  #2: gscap-roundtrip ..................   Passed    0.18 sec
      Start  3: gen-synth
 3/11 Test  #3: gen-synth ........................   Passed    5.96 sec
      Start  4: identity-replay
 4/11 Test  #4: identity-replay ..................   Passed   14.91 sec
      Start  5: run-census
 5/11 Test  #5: run-census .......................   Passed    5.43 sec
      Start  6: census-features
 6/11 Test  #6: census-features ..................   Passed    0.07 sec
      Start  7: strict-diffs-on-mip
 7/11 Test  #7: strict-diffs-on-mip ..............   Passed    0.44 sec
      Start  8: strict-exact-on-control
 8/11 Test  #8: strict-exact-on-control ..........   Passed    1.31 sec
      Start  9: cpu-identity-on-new-captures
 9/11 Test  #9: cpu-identity-on-new-captures .....   Passed    2.90 sec
      Start 10: present-heuristic
10/11 Test #10: present-heuristic ................   Passed    0.31 sec
      Start 11: spike-identity
11/11 Test #11: spike-identity ...................   Passed   15.03 sec

100% tests passed out of 11

Total Test time (real) =  46.54 sec
```

Test mapping: (10) `test_present_heuristic` pins §2
(sourceFbp + spot pixels + live replay, fails if the
heuristic changes); (11) `gsreplay --backend spike` exits 0
on all 102 captures. Tests 1–9 are the G0/G2 suite
unchanged in behavior (counts 98→102 where the suite counts
captures).

## 7. Exact commands

```
cmake -S . -B /tmp/ps2xgs-build -G Ninja
cmake --build /tmp/ps2xgs-build -j4
ctest --test-dir /tmp/ps2xgs-build --output-on-failure
/tmp/ps2xgs-build/gsgen "/Volumes/Extreme SSD/ps2xgs/synth"
/tmp/ps2xgs-build/gsreplay <capture.gscap> --backend cpu|strict|spike [--time-submits] [--json out.json]
/tmp/ps2xgs-build/test_present_heuristic "/Volumes/Extreme SSD/ps2xgs/synth"
/tmp/ps2xgs-build/gscensus <capture...> --json census.json --md census.md
```

`PS2XGS_SYNTH_DIR` CMake cache var overrides the capture
directory (default `/Volumes/Extreme SSD/ps2xgs/synth`).
Timing loops: `/tmp/g3-time.sh` (interleaved backends,
5 runs, `submit_ms_total` lines) and
`/tmp/g3-step2-matrix.sh` (per-capture `--json` matrices).

## 8. Receipt paths

- `/tmp/ps2xgs-build/` — binaries (`gsreplay`,
  `gsreplay-step2-clut` (preserved step-2 binary), `gsgen`,
  `gscensus`, `test_present_heuristic`), `g3-ctest.log`,
  `g3-replay-spike-clut/*.json` (102, §4a),
  `g3-replay-spike-rmw/*.json` (102, §5a),
  `g3-replay-cpu-clut/*.json` (8, §4b present medians),
  `g3-step2-time-final.txt` (§4b submit timings),
  `g3-step3-time-raw.txt` + `g3-step3-time-step2.txt`
  (§5b submit timings), `g3-diff-clut.txt`,
  `g3-diff-rmw.txt` (= `git diff 69b7f89 6bfe882 -- harness/spike_backend.cpp`),
  `g3-diff-header.txt`, `g3-baseline-md5.txt`,
  `g3-final-md5.txt` (old-98 diff clean),
  `g3-table-*.md` (report table sources).
- `/Volumes/Extreme SSD/ps2xgs/synth/` — 102 `.gscap`
  captures (§1 table for the 4 new; old 98 unchanged).
  Nothing over 5 MB in the repo; no captures committed.
- Repo commits (prefix `[G3]`, trailer `Orchestrated-By: Muse
  Code`): Step 1 captures+pins, Step 2 fork+CLUT, Step 3 RMW,
  this report.

## 9. What I could not do

- CLUT-cache exactness assumes no draw writes its own CLUT
  footprint mid-batch (the memo is per-`Submit` with no
  mid-batch invalidation on overlapping frame writes);
  proven only on the 102-capture set, none of which does
  this. Upstream hardening would need write-overlap
  invalidation or a narrower memo scope.
- RMW fast path covers CT32 frame writes only; CT24/CT16/
  paletted/depth-as-frame PSMs keep the original two-call
  path (all `WritePixel` frame draws in the 102 captures
  are CT32, so the fallback lines are exercised only by
  their presence, not by divergence).
- `median_ms` times `Present` calls only, so Submit-path
  spikes are invisible to it; Submit deltas come from the
  new `--time-submits` flag instead. All timings are
  same-machine observations on Apple silicon (CPU-only, not
  backend targets), recorded with min-max ranges, not
  claimed.
- `strict` honors and spikes are not combined: `spike` is
  cpu-equivalent by design, `strict` keeps its 14
  intentional diffs. No game/reference captures exist
  beyond the synthetic set (same gate as G0 §10).
- `upstream/` untouched throughout; both spikes live in the
  harness fork (`harness/spike_backend.*`). An upstream
  patch would re-apply the `[G3-CLUT]`/`[G3-RMW]` hunks to
  `gs_cpu_backend.cpp` instead.

