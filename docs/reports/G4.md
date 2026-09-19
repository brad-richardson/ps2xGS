# G4 report — CPU-backend perf profile + one Present fast path

Brief: G4 runbook. Read: `docs/reports/G3.md` (§3 spike vehicle,
§4b/§5b timing method) and `docs/reports/G0.md` §4 (~16 ms/present
baseline shape). Host-only, no device, no emulator. `cpu` backend
default untouched; `upstream/` untouched; `strict` untouched. All 102
captures replay byte-identical on `spike` before and after (this
report's §2 table). No new captures (102 reused).

Machine: Apple M4, 10 cores, macOS 27.0 (26A428). Toolchain:
`/usr/bin/c++`, AppleClang, `-std=c++20`, no `-O` flag (default
`CMAKE_BUILD_TYPE=` empty — same as G0/G3 builds), libc++. Load
average during timing runs: 1.8–3.3 (an unrelated `ld` link at ~950%
CPU inflated the earliest runs; split/timing tables below were taken
after it exited).

## 1. Profile

Tool: `sample(1)` (1 ms interval, 8 s per spin) for top-5 self/total
shares; manual instrumentation (`/tmp/g4-profile/prof.cpp`, same -O0
build) for the Submit vs Transfer vs Present split. Spin targets loop
one phase per capture so the sampled stacks stay in that phase:

| spin | capture | iters | per-iter ms (spin mean) | samples (thread) |
| --- | --- | --- | --- | --- |
| present | tex-t8-csm1 (CLUT-heavy) | 1500 | 7.232 | 3288 |
| present | blend-a-cs (blend-heavy) | 1500 | 6.931 | 2656 |
| present | transfer-l2l (transfer-heavy) | 1500 | 7.036 | 3474 |
| submit | tex-t8-csm1 | 40000 | 0.274 | 3483 |
| submit | blend-a-cs | 15000 | 0.329 | 3594 |
| transfer | transfer-l2l | 300000 | 0.036 | 4217 |

Capture roles: tex-t8-csm1 = T8 textured + CLUT lookups (1 submit, 2
transfer begins for texture/CLUT upload); blend-a-cs = untextured
blended (2 submits, 0 transfers); transfer-l2l = transfers only (0
submits, 2 begins, 1 upload of 1024 bytes).

### 1a. Submit vs Transfer vs Present split

`prof split 11`: single pass over all non-Present records (Submit /
transfer / other timed separately) + 11 in-process Presents. 5 runs,
median (min-max), ms. `other` = harness/record handling in the
profiler (decodes + 4 MB snapshot verification), not backend work.

| capture | submit_ms_total | transfer_ms_total | other_ms_total | present steady med (min-max of med) | present cold first (min-max of max) |
| --- | --- | --- | --- | --- | --- |
| tex-t8-csm1 | 0.270 (0.268-0.277) | 0.082 (0.080-0.084) | 2.394 (2.357-2.419) | 4.181 (4.096-4.219) | 15.881 (15.617-16.298) |
| blend-a-cs | 0.648 (0.628-0.661) | 0.000 (0.000-0.000) | 2.437 (2.375-2.450) | 4.202 (4.151-4.240) | 15.890 (15.596-15.967) |
| transfer-l2l | 0.000 (0.000-0.000) | 0.042 (0.041-0.043) | 2.407 (2.369-2.427) | 4.185 (4.125-4.236) | 15.846 (15.581-15.948) |

Single-shot `gsreplay` baselines (fresh process, `--backend cpu
--time-submits`, G0/G3-comparable), 5 runs:

| capture | submits | median_ms med (min-max) | submit_ms_total med (min-max) |
| --- | --- | --- | --- |
| tex-t8-csm1 | 1 | 15.866 (15.825-15.924) | 0.268 (0.258-0.270) |
| blend-a-cs | 2 | 15.828 (15.826-15.848) | 0.639 (0.624-0.650) |
| transfer-l2l | 0 | 15.858 (15.826-15.912) | 0.000 (0.000-0.000) |

`split-warm` (1 untimed Present, then 11 timed): tex-t8-csm1 med 4.076
min 4.027 max 4.114 — one warmup Present removes the cold-first
spread.

### 1b. Top-5, Present spins

Self = `sample` "Sort by top of stack" counts; total = call-graph
aggregation for the same symbol. Region rollup anchors:
`__construct_at_end` (fill-construct subtree),
`__base_destruct_at_end` (teardown subtree). Short names; full
signatures are in the receipt files.

tex-t8-csm1, thread total 3288:

| # | function | self | total |
| --- | --- | --- | --- |
| 1 | `allocator_traits::destroy<u8>` | 502 (15.3%) | 833 (25.3%) |
| 2 | `construct_at<u8>` | 479 (14.6%) | 479 (14.6%) |
| 3 | `allocator_traits::construct<u8>` | 452 (13.7%) | 1330 (40.5%) |
| 4 | `__construct_at<u8>` | 408 (12.4%) | 887 (27.0%) |
| 5 | `__to_address<u8>` | 359 (10.9%) | 359 (10.9%) |

Region rollup: fill-construct 1867 (56.8%), teardown 1292 (39.3%),
SnapshotVram/`memmove` 25 (0.8%), VRAM-read path 85 (2.6%),
remainder 19 (0.6%).

blend-a-cs, thread total 2656:

| # | function | self | total |
| --- | --- | --- | --- |
| 1 | `construct_at<u8>` | 396 (14.9%) | 396 (14.9%) |
| 2 | `allocator_traits::destroy<u8>` | 390 (14.7%) | 636 (23.9%) |
| 3 | `allocator_traits::construct<u8>` | 389 (14.6%) | 1083 (40.8%) |
| 4 | `__to_address<u8>` | 358 (13.5%) | 358 (13.5%) |
| 5 | `__construct_at<u8>` | 300 (11.3%) | 696 (26.2%) |

Region rollup: fill-construct 1514 (57.0%), teardown 1052 (39.6%),
SnapshotVram/`memmove` 35 (1.3%), VRAM-read path 48 (1.8%),
remainder 7 (0.3%).

transfer-l2l, thread total 3474:

| # | function | self | total |
| --- | --- | --- | --- |
| 1 | `allocator_traits::destroy<u8>` | 529 (15.2%) | 831 (23.9%) |
| 2 | `allocator_traits::construct<u8>` | 513 (14.8%) | 1408 (40.5%) |
| 3 | `construct_at<u8>` | 481 (13.8%) | 481 (13.8%) |
| 4 | `__to_address<u8>` | 428 (12.3%) | 428 (12.3%) |
| 5 | `__construct_at<u8>` | 417 (12.0%) | 898 (25.8%) |

Region rollup: fill-construct 1981 (57.0%), teardown 1365 (39.3%),
SnapshotVram/`memmove` 34 (1.0%), VRAM-read path 72 (2.1%),
remainder 22 (0.6%).

Call paths: fill-construct =
`Present`→`PresentFromLocalMemory`→`copySource`-lambda→`CopyFrameToHostRgba`→`vector::assign(1310720,
0)`; teardown = `PresentationFrame`/`vector` destructor →
`clear`→ per-element `destroy` (loop-frame destroys plus ~10–25
process-teardown samples).

### 1c. Top-5, Submit spins

tex-t8-csm1, thread total 3483:

| # | function | self | total |
| --- | --- | --- | --- |
| 1 | `GSCpuBackend::WritePixel` | 274 (7.9%) | 1684 (48.3%) |
| 2 | `clampU8` | 158 (4.5%) | 158 (4.5%) |
| 3 | `__value_func<void-write>` (std::function) | 122 (3.5%) | 874 (25.1%) |
| 4 | `combineTexture` | 110 (3.2%) | 268 (7.7%) |
| 5 | `__invoke<void-write>` | 100 (2.9%) | 665 (19.1%) |

Anchors: `Submit` 3467 (99.5%), `DrawSprite` 3430 (98.5%),
`SampleTexture` 1390 (39.9%), `WriteVramUnlocked` 989 (28.4%),
`ReadVramUnlocked` 630 (18.1%), `LookupCLUT` 517 (14.8%),
`Address` (all PSMs) 553 (15.9%), `combineTexture` 268 (7.7%).

blend-a-cs, thread total 3594:

| # | function | self | total |
| --- | --- | --- | --- |
| 1 | `GSCpuBackend::WritePixel` | 425 (11.8%) | 3606 (100.3%*) |
| 2 | `GSCpuBackend::WriteVramUnlocked` | 298 (8.3%) | 2385 (66.4%) |
| 3 | `__invoke<void-write>` | 201 (5.6%) | 1416 (39.4%) |
| 4 | `__value_func<void-write>` (std::function) | 166 (4.6%) | 1816 (50.5%) |
| 5 | `std::function<void-write>::operator()` | 142 (4.0%) | 2087 (58.1%) |

Anchors: `Submit` 3578 (99.6%), `DrawSprite` 3571 (99.4%),
`Address` 680 (18.9%), `PageId` 235 (6.5%), `UnpackedBitWidth` 200
(5.6%). (*) 3606 exceeds the 3594 thread total by 12 samples (0.3%);
`sample` attributes shared leaf addresses to two tree nodes — recorded
as measured.

### 1d. Top-5, transfer spin

transfer-l2l, thread total 4217:

| # | function | self | total |
| --- | --- | --- | --- |
| 1 | `PixelStorageTraits<C32>::Address` | 404 (9.6%) | 1857 (44.0%) |
| 2 | `DYLD-STUB$$…::PageId` | 308 (7.3%) | 626 (14.8%) |
| 3 | `PixelStorageTraits<C32>::PageId` | 290 (6.9%) | 818 (19.4%) |
| 4 | `__invoke<void-write>` | 196 (4.6%) | 1825 (43.3%) |
| 5 | `__value_func<void-write>` (std::function) | 165 (3.9%) | 2259 (53.6%) |

Anchors: `BeginTransfer` 2717 (64.4%) →
`PerformLocalToLocalTransfer` 2707 (64.2%); `UploadImage` 1474
(35.0%); `WriteVramUnlocked` 2465 (58.5%), `ReadVramUnlocked` 1098
(26.0%), `WriteCT32` 1608 (38.1%), `ReadCT32` 745 (17.7%).

### 1e. Micro-isolations

`prof snap` (tex-t8-csm1): `SnapshotVram` into a fresh 4 MB vector vs
a reused one:

| call | ms | bytes |
| --- | --- | --- |
| fresh #0 | 11.742 | 4194304 |
| fresh #1 | 11.526 | 4194304 |
| fresh #2 | 11.503 | 4194304 |
| reused #0 | 11.562 | 4194304 |
| reused #1 | 0.054 | 4194304 |
| reused #2 | 0.054 | 4194304 |

Vector-idiom timings, 1.3 MB `uint8_t`, same -O0 build
(`vectest.cpp`, N=11, destroy included unless noted):

| # | idiom | ms/iter |
| --- | --- | --- |
| 1 | `assign(K,0)` after clear | 6.722 |
| 2 | `dst=zeros` (fresh dst) | 2.624 |
| 3 | `assign(ptr,ptr)` cleared, kept capacity | 2.383 |
| 4 | range-ctor fresh dst | 2.626 |
| 5 | `resize(K)+memcpy` cleared, kept capacity | 5.974 |
| 6 | `reserve+insert-range` fresh dst | 2.630 |
| 7 | range-ctor + destroy | 2.622 |

Isolated (`vectest2.cpp`):

| idiom | ms/iter |
| --- | --- |
| range-ctor into holder (malloc+copy, no destroy) | 0.076 |
| `clear()` (destroy only) | 2.558 |
| `reserve(K)` + destroy-empty | 0.000 |
| `reserve+assign(K,0)` + full destroy | 6.401 |

Const-vs-mutable source range (`vectest3.cpp`, fresh dst, no
destroy):

| idiom | ms/iter |
| --- | --- |
| `assign(mut,mut)` | 0.081 |
| `assign(const,const)` | 4.186 |
| ctor(mut,mut) | 0.021 |
| ctor(const,const) | 4.154 |
| `assign(K,0)` | 3.917 |

## 2. One fast path: bulk Present scratch buffers (`[G4-PRESENT]`)

The profile attributes the single-shot ~16 ms to two element-wise
buffer constructions in the Present path: the 4 MB VRAM snapshot
first-`resize` (~11.5 ms, §1e) and the 1.3 MB host-frame `assign(K, 0)`
(~3.9 ms fill, §1e; 56–57% of steady-state Present, §1b). The spike
replaces both with bulk forms moving identical bytes:

- `CopyFrameToHostRgba` + dual-CRT composite: `assign(K, 0)` →
  range-`assign` from a process-lifetime zeros buffer. The source
  range is deliberately non-const: this libc++ copies const ranges
  element-wise at -O0 (§1e: 4.186 ms vs 0.081 ms).
- `Present`: thread-local `vector` + `SnapshotVram` (first-use
  `resize(4M)`) → thread-local raw buffer + `malloc`/`memcpy` under
  the identical lock. OOM behavior matches upstream (`new[]` throws
  `bad_alloc`, same as vector growth).

Diff (`git diff` of `harness/spike_backend.cpp`, +67/−7, the only file
changed; `upstream/`, `cpu`, `strict` untouched):

```
diff --git a/harness/spike_backend.cpp b/harness/spike_backend.cpp
index b2b7c01..a55ae74 100644
--- a/harness/spike_backend.cpp
+++ b/harness/spike_backend.cpp
@@ -1803,6 +1803,53 @@ bool GSSpikeBackend::ClearFramebuffer(const GSContext &context, uint32_t rgba)
     return false;
 }
 
+namespace
+{
+
+// [G4-PRESENT] Bulk Present scratch buffers. Profiled cause of the ~16 ms
+// single-shot Present (docs/reports/G4.md §1): at the repo's default -O0
+// build, `vector::assign(K, 0)` / `vector::resize(4M)` on the 1.3 MB host
+// frame and 4 MB VRAM snapshot construct every byte through a per-element
+// call chain (~3.9 ms + ~11.5 ms). The bulk forms below move bit-identical
+// bytes through single memmove/memcpy calls. OOM behavior matches upstream
+// (new[] throws bad_alloc, same as vector growth).
+
+uint8_t *spikeHostFrameZeros()
+{
+    // Process-lifetime zeros (malloc + one bulk memset); never freed, same
+    // as the fork's lookup tables. Function-local static init is thread-safe.
+    // Deliberately MUTABLE: this libc++ takes its bulk memmove path only for
+    // non-const source ranges (const ranges copy element-wise at -O0; measured
+    // 0.08 ms vs 4.2 ms for 1.3 MB -- see docs/reports/G4.md §2). Callers must
+    // only ever read from it.
+    static uint8_t *zeros = []() {
+        const size_t n = static_cast<size_t>(kHostFrameWidth) * kHostFrameHeight * 4u;
+        uint8_t *p = new uint8_t[n];
+        std::memset(p, 0, n);
+        return p;
+    }();
+    return zeros;
+}
+
+inline void spikeAssignHostZeros(std::vector<uint8_t> &out)
+{
+    const size_t n = static_cast<size_t>(kHostFrameWidth) * kHostFrameHeight * 4u;
+    uint8_t *z = spikeHostFrameZeros();
+    out.assign(z, z + n); // range copy: memmove-fast, identical bytes
+}
+
+struct SpikeVramSnap
+{
+    uint8_t *data = nullptr;
+    uint32_t size = 0;
+    SpikeVramSnap() = default;
+    ~SpikeVramSnap() { delete[] data; }
+    SpikeVramSnap(const SpikeVramSnap &) = delete;
+    SpikeVramSnap &operator=(const SpikeVramSnap &) = delete;
+};
+
+} // namespace
+
 bool GSSpikeBackend::CopyFrameToHostRgba(const GSFrameReg &frame,
                                        uint32_t width,
                                        uint32_t height,
@@ -1816,7 +1863,7 @@ bool GSSpikeBackend::CopyFrameToHostRgba(const GSFrameReg &frame,
     if (!m_vram || m_vramSize == 0u)
         return false;
 
-    outPixels.assign(kHostFrameWidth * kHostFrameHeight * 4u, 0u);
+    spikeAssignHostZeros(outPixels); // [G4-PRESENT] was assign(K, 0)
     const uint32_t baseBytes = frameBaseIsPages ? frame.fbp * 8192u : frame.fbp * 256u;
     const uint32_t basePtr = frameBaseIsPages ? GSInternal::framePageBaseToBlock(frame.fbp) : frame.fbp;
     const uint32_t fbw = frame.fbw ? frame.fbw : kHostFrameWidth / 64u;
@@ -1885,13 +1932,26 @@ PresentationFrame GSSpikeBackend::Present(const GSPresentationRequest &request)
 {
     // Snapshot local memory under the backend lock, then perform the expensive
     // display conversion without holding the producer-side raster lock.
-    thread_local std::vector<uint8_t> snapshot;
-    SnapshotVram(snapshot);
-    if (snapshot.empty())
-        return {};
+    // [G4-PRESENT] Raw thread-local snapshot: SnapshotVram's resize(4M) pays
+    // ~11.5 ms of first-use element construction at -O0; malloc+memcpy moves
+    // identical bytes under the identical lock.
+    thread_local SpikeVramSnap snap;
+    {
+        std::lock_guard<std::mutex> lock(m_mutex);
+        if (!m_vram || m_vramSize == 0u)
+            return {};
+        if (snap.size != m_vramSize)
+        {
+            uint8_t *grown = new uint8_t[m_vramSize]; // throws like vector growth
+            delete[] snap.data;
+            snap.data = grown;
+            snap.size = m_vramSize;
+        }
+        std::memcpy(snap.data, m_vram, m_vramSize);
+    }
 
     thread_local GSSpikeBackend snapshotBackend;
-    snapshotBackend.Initialize(snapshot.data(), static_cast<uint32_t>(snapshot.size()));
+    snapshotBackend.Initialize(snap.data, snap.size);
     return snapshotBackend.PresentFromLocalMemory(request);
 }
 
@@ -1966,7 +2026,7 @@ PresentationFrame GSSpikeBackend::PresentFromLocalMemory(const GSPresentationReq
         {
             result.width = std::max(width1, width2);
             result.height = std::max(height1, height2);
-            result.pixels.assign(kHostFrameWidth * kHostFrameHeight * 4u, 0u);
+            spikeAssignHostZeros(result.pixels); // [G4-PRESENT] was assign(K, 0)
             const uint8_t bgR = static_cast<uint8_t>(request.bgcolor);
             const uint8_t bgG = static_cast<uint8_t>(request.bgcolor >> 8u);
             const uint8_t bgB = static_cast<uint8_t>(request.bgcolor >> 16u);
```

Work note (recorded, not committed as such): the first cut passed a
`const uint8_t*` source range and measured identical to `assign(K,
0)` (steady-state 4.188 ms vs 4.177 ms cpu); the const/mutable
experiment (§1e) showed why, and only the mutable-range version was
committed.

### 2a. Identity replay, spike(G4) vs cpu references (102 rows)

Threshold 4, max-bad-pct 1.0. 102 rows, all max 0, vram yes.

| capture | WxH | max | mean | bad% | vram | median_ms |
| --- | --- | --- | --- | --- | --- | --- |
| atest-afail-fb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.544 |
| atest-afail-rgb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.539 |
| atest-afail-zb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.518 |
| atest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.534 |
| atest-date-datm0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.558 |
| atest-date-datm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.528 |
| atest-equal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.525 |
| atest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.546 |
| atest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.528 |
| atest-lequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.573 |
| atest-less.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.523 |
| atest-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.549 |
| atest-notequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.532 |
| blend-a-cd.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.537 |
| blend-a-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.530 |
| blend-b-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.534 |
| blend-c-ad.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.544 |
| blend-c-fix.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.524 |
| blend-colclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.531 |
| blend-d-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.525 |
| blend-dthe.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.569 |
| blend-fba.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.543 |
| blend-pabe.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.536 |
| clear-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.532 |
| clear-ct16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.531 |
| clear-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.541 |
| clear-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.536 |
| fog-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.544 |
| fog-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.552 |
| fog-textured.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.521 |
| iso-aa1-clear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.543 |
| iso-aa1-set.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.514 |
| iso-colclamp-off.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.538 |
| iso-colclamp-on.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.529 |
| iso-dthe-off.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.537 |
| iso-dthe-on.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.521 |
| iso-scanmsk-set.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.564 |
| iso-scanmsk-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.523 |
| iso-zte-off-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.541 |
| iso-zte-off-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.522 |
| iso-zte-on-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.544 |
| mip-chain-mxl0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.536 |
| mip-chain-mxl1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.568 |
| mip-chain-mxl2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.542 |
| mip-chain-stq-mxl2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.541 |
| present-both.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 6.127 |
| present-circuit2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.527 |
| present-fbp0-black.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 3.390 |
| present-fbp0-empty.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 6.229 |
| present-fbp0-multi.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 3.399 |
| present-fbp0-nonblack.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.554 |
| present-field.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 3.230 |
| present-frame.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.533 |
| present-progressive.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.539 |
| prim-fbmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.547 |
| prim-line-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.592 |
| prim-line-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.543 |
| prim-point.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.534 |
| prim-scissor.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.530 |
| prim-sprite.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.532 |
| prim-trifan.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.572 |
| prim-trilist-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.534 |
| prim-trilist-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.548 |
| prim-tristrip.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.544 |
| prim-xyoffset.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.540 |
| tex-clamp-clamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.555 |
| tex-clamp-rclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.573 |
| tex-clamp-repeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.548 |
| tex-clamp-rrepeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.552 |
| tex-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.554 |
| tex-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.581 |
| tex-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.548 |
| tex-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.532 |
| tex-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.554 |
| tex-stq.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.554 |
| tex-t4-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.546 |
| tex-t4-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.548 |
| tex-t8-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.558 |
| tex-t8-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.542 |
| tex-t8h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.527 |
| tex-texa.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.541 |
| tex-uv.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.585 |
| tex1-filter-agree-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.569 |
| tex1-filter-agree-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.574 |
| tex1-filter-disagree-lin.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.554 |
| tex1-filter-disagree-near.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.534 |
| tex1-filter-mixed-mmin.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.548 |
| transfer-h2l-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.530 |
| transfer-h2l-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.564 |
| transfer-h2l-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.545 |
| transfer-h2l-t4.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.547 |
| transfer-h2l-t8.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.390 |
| transfer-l2h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.543 |
| transfer-l2l.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.582 |
| ztest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.530 |
| ztest-gequal-z16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.566 |
| ztest-gequal-z16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.536 |
| ztest-gequal-z24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.557 |
| ztest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.534 |
| ztest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.545 |
| ztest-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.563 |
| ztest-zmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 0.532 |

### 2b. Timing deltas

The fast path touches Present only, so the affected metric is
`median_ms` (single `Present` call); `submit_ms_total` is the
no-change control. G3 §4b method: backends interleaved per run, 5
runs, median (min-max), same machine, recorded as observed.

`cpu` (current binary) vs `spike` (current binary, G4):

| capture | submits | cpu median_ms med (min-max) | spike median_ms med (min-max) | cpu submit med (min-max) | spike submit med (min-max) |
| --- | --- | --- | --- | --- | --- |
| tex-t8-csm1.gscap | 1 | 15.846 (15.727-15.876) | 0.578 (0.551-0.582) | 0.271 (0.267-0.281) | 0.233 (0.231-0.238) |
| blend-a-cs.gscap | 2 | 15.848 (15.811-15.876) | 0.538 (0.528-0.580) | 0.639 (0.634-0.641) | 0.564 (0.557-0.569) |
| transfer-l2l.gscap | 0 | 15.858 (15.811-15.885) | 0.559 (0.542-0.578) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-both.gscap | 0 | 29.145 (29.008-29.160) | 6.140 (6.108-6.161) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-field.gscap | 0 | 18.543 (18.532-18.590) | 3.236 (3.210-3.255) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-fbp0-black.gscap | 0 | 22.519 (22.510-22.589) | 3.377 (3.372-3.409) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |

Isolated G4 delta: `spike` from the preserved pre-G4 binary
(`gsreplay-preg4`, `--backend spike`) vs `spike` from the current
binary (same flag; the two differ only in the §2 hunk), interleaved,
5 runs:

| capture | submits | pre-G4 median_ms med (min-max) | post-G4 median_ms med (min-max) | pre-G4 submit med (min-max) | post-G4 submit med (min-max) |
| --- | --- | --- | --- | --- | --- |
| tex-t8-csm1.gscap | 1 | 15.864 (15.803-15.872) | 0.561 (0.535-0.564) | 0.231 (0.231-0.233) | 0.231 (0.227-0.242) |
| blend-a-cs.gscap | 2 | 15.848 (15.763-15.894) | 0.543 (0.536-0.561) | 0.563 (0.557-0.568) | 0.566 (0.563-0.572) |
| transfer-l2l.gscap | 0 | 15.822 (15.790-15.866) | 0.558 (0.540-0.589) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-both.gscap | 0 | 29.062 (29.043-29.112) | 6.110 (6.099-6.122) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-field.gscap | 0 | 18.518 (18.491-18.537) | 3.235 (3.153-3.243) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |
| present-fbp0-black.gscap | 0 | 22.531 (22.418-22.763) | 3.385 (3.366-3.412) | 0.000 (0.000-0.000) | 0.000 (0.000-0.000) |

The cpu-vs-spike submit differences are G3's CLUT/RMW effects
(unchanged by G4; e.g. tex-t8-csm1 spike submit 0.233 here vs 0.235
in G3 §4b). Steady-state in-process Present excluding caller-side
frame destroy (`split-warm` med, 5 runs): 4.177 ms cpu (§1a) vs 0.261
ms (0.243-0.277) post-G4 spike on tex-t8-csm1 (receipt:
`/tmp/g4-profile/g4-split-warm-post.txt`). Including caller-side
destroy (`gsreplay --repeat 11`, single runs): 6.833 ms cpu vs 2.927
ms spike; `prof spin-present 1500` spike mean 2.905 ms/iter. The
~2.6 ms destroy is identical for both backends (§1b teardown, §6).

## 3. CTest output

Receipt: `/tmp/ps2xgs-build/g4-ctest.log`. Full output (11 tests, no
new tests added — `spike-identity` pins the byte-identical contract;
timing is machine-dependent and not asserted):

```
Test project /tmp/ps2xgs-build
      Start  1: gsregs-roundtrip
 1/11 Test  #1: gsregs-roundtrip .................   Passed    0.00 sec
      Start  2: gscap-roundtrip
 2/11 Test  #2: gscap-roundtrip ..................   Passed    0.20 sec
      Start  3: gen-synth
 3/11 Test  #3: gen-synth ........................   Passed    5.95 sec
      Start  4: identity-replay
 4/11 Test  #4: identity-replay ..................   Passed   14.85 sec
      Start  5: run-census
 5/11 Test  #5: run-census .......................   Passed    5.44 sec
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
11/11 Test #11: spike-identity ...................   Passed   12.47 sec

100% tests passed out of 11

Total Test time (real) =  43.95 sec
```

Capture stability: the 4 `present-fbp0-*` md5s after the suite's
regeneration match G3 §1 exactly (`f7af2946…`, `1eeb0340…`,
`1340e0ba…`, `92f6bba9…`); no captures committed.

## 4. Exact commands

```
cmake -S . -B /tmp/ps2xgs-build -G Ninja
cmake --build /tmp/ps2xgs-build -j4
ctest --test-dir /tmp/ps2xgs-build --output-on-failure
/tmp/ps2xgs-build/gsreplay <capture.gscap> --backend cpu|spike [--time-submits] [--repeat N] [--json out.json]
/tmp/g4-profile/prof <capture.gscap> split|split-warm|spin-submit|spin-present|spin-transfer|snap [iters] [cpu|spike]
sample prof 8 -file <out.txt>   # while a prof spin runs; 1 ms interval
sh /tmp/ps2xgs-build/g4-time.sh <gsreplay-bin> <synth-dir> <capture...>  # §2b table 1
```

Build line for the profiler (same -O0 as the repo build):

```
/usr/bin/c++ -std=c++20 -I harness -I upstream/ps2xRuntime/include \
  /tmp/g4-profile/prof.cpp harness/backend_factory.cpp \
  harness/spike_backend.cpp harness/strict_backend.cpp \
  /tmp/ps2xgs-build/libps2xgs_harness.a \
  /tmp/ps2xgs-build/libps2xgs_upstream_cpu.a -o /tmp/g4-profile/prof
```

Spin iteration counts: present 1500, submit 40000 (tex-t8) / 15000
(blend-a-cs), transfer 300000. Vector-idiom probes:
`/tmp/g4-profile/vectest{,2,3}.cpp` (same `-std=c++20`, no `-O`).

## 5. Receipt paths

- `/tmp/ps2xgs-build/` — binaries (`gsreplay` post-G4,
  `gsreplay-preg4` preserved pre-G4 binary), `g4-ctest.log`,
  `g4-replay-spike/*.json` (102, §2a), `g4-diff-present.txt` (= `git
  diff` of the §2 hunk), `g4-time.sh` (§2b table-1 script).
- `/tmp/g4-profile/` — `prof.cpp` + `prof` (split/spin tool),
  `sample-{present-{tex-t8,blend-a-cs,transfer-l2l},submit-{tex-t8,blend-a-cs},transfer-l2l}.txt`
  (6 `sample` reports, §1b–1d), `agg.py` (self/total aggregator),
  `vectest{,2,3}.cpp` + binaries (§1e), `g4-split-5runs.txt`,
  `g4-singleshot-5runs.txt`, `g4-time-raw.txt`,
  `g4-time-prepost.txt`, `g4-split-warm-post.txt`,
  `g4-identity-table.md`, `spike_backend.G4.cpp` (copy of the patched
  file as built for the pre-G4-binary shuffle).
- `/Volumes/Extreme SSD/ps2xgs/synth/` — 102 `.gscap` captures
  (unchanged; §3 md5s). Nothing over 5 MB in the repo; no captures
  committed.
- Repo commits (prefix `[G4]`, trailer `Orchestrated-By: Muse Code`):
  fast-path hunk, this report.

## 6. What I could not do

- The §1b teardown region (~39% of steady-state Present, ~2.6 ms per
  1.3 MB vector at -O0) belongs to whoever destroys the returned
  `PresentationFrame`: the caller, not the backend. `gsreplay`
  repeat=1 does not time it; in-process loops do. No backend change
  can remove it while `Present` returns a 1.3 MB vector.
- Three Present paths keep internal 1.3 MB temp destroys inside the
  timed call: dual-CRT composite (2 temps: `crt1`/`crt2`),
  field-mode (`applyFieldPresentation` copy), fbp0-fallback
  (`candidatePixels` per tried candidate). Observed post-G4 medians:
  present-both 6.1, present-field 3.2, present-fbp0-black/multi 3.4,
  present-fbp0-empty 6.2 ms. Removing them needs restructuring (buffer
  reuse still pays element-wise `clear`), not the one-idiom path.
- Submit/transfer paths untouched: §1c–1d hot functions
  (`WritePixel`, `SampleTexture`+tap lambda, `Address`/`PageId`,
  per-pixel `std::function` dispatch) stand as profiled. Submit is
  0.3–0.7 ms against Present's former ~16 ms, so the single
  highest-leverage item was Present-side.
- No game/reference captures exist beyond the synthetic set (same
  gate as G0 §10); identity is proven on the 102 only.
- All timings are same-machine observations on Apple silicon at the
  repo's default -O0 build (CPU-only, not backend targets), recorded
  with min-max ranges, not claimed. The const-vs-mutable range-copy
  dispatch (§1e) is a property of this libc++ at -O0.
- `upstream/` untouched throughout; the fast path lives in the
  harness fork (`harness/spike_backend.cpp`). An upstream patch would
  re-apply the `[G4-PRESENT]` hunks to `gs_cpu_backend.cpp` instead.
