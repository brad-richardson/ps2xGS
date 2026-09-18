# G0 report — ps2xGS skeleton + GS capture/replay harness + census (synthetic streams)

Brief: G0 runbook. Plan served: `docs/plan.md` (§§1–3, 5, 7).
Upstream: `upstream/` = `ran-j/PS2Recomp` @
`14b1e5cb39b4af7e6fc12f9a29fdc751efde49d7` (submodule gitlink,
read-only, unmodified). Host-only, no device, no Vulkan work.

## 1. Upstream files compiled (`ps2xgs_upstream_cpu` target)

| file | role |
| --- | --- |
| `upstream/ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp` | `GSCpuBackend` rasterizer (~1,900 lines) |
| `upstream/ps2xRuntime/src/lib/gs/ps2_gs_memory.cpp` | VRAM PSM lookup tables |

Include paths added: `upstream/ps2xRuntime/include`. Defines added:
none. C++ standard: C++20 (matches upstream `ps2xRuntime`).
Deliberately not compiled: `gs_frontend.cpp`, `ps2_gif_arbiter.cpp`,
the runtime (raylib), everything else.

Headers pulled in transitively (unmodified):
`runtime/gs/{gs_backend,gs_cpu_backend,gs_types,ps2_gs_common,ps2_gs_memory,ps2_gs_psmct16,ps2_gs_psmct32,ps2_gs_psmt4,ps2_gs_psmt8}.h`,
`types.h`, `ps2_log.h`. `gs_cpu_backend.cpp` includes `ps2_log.h`
but only uses the `PS2_IF_AGRESSIVE_LOGS` macro (compiled out by
default) — no shim was needed.

Build receipt: `/tmp/ps2xgs-build/g0-configure-build.log` (91 lines).
First 30 lines:

```
-- The CXX compiler identification is AppleClang 21.0.0.21000334
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Configuring done (0.6s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/ps2xgs-build
[1/17] Building CXX object CMakeFiles/test_gscap_roundtrip.dir/tests/test_gscap_roundtrip.cpp.o
[2/17] Building CXX object CMakeFiles/ps2xgs_harness.dir/harness/recording_backend.cpp.o
[3/17] Building CXX object CMakeFiles/test_gsregs.dir/tests/test_gsregs.cpp.o
[4/17] Building CXX object CMakeFiles/gsreplay.dir/harness/backend_factory.cpp.o
[5/17] Building CXX object CMakeFiles/ps2xgs_upstream_cpu.dir/upstream/ps2xRuntime/src/lib/gs/ps2_gs_memory.cpp.o
In file included from /Users/bradrichardson/dev/ps2xGS/upstream/ps2xRuntime/src/lib/gs/ps2_gs_memory.cpp:3:
/Users/bradrichardson/dev/ps2xGS/upstream/ps2xRuntime/include/runtime/gs/ps2_gs_memory.h:67:11: warning: enumeration value 'Max' not handled in switch [-Wswitch]
   67 |                 switch (psm)
      |                         ^~~
/Users/bradrichardson/dev/ps2xGS/upstream/ps2xRuntime/include/runtime/gs/ps2_gs_memory.h:67:11: note: add missing switch cases
   67 |                 switch (psm)
      |                         ^
/Users/bradrichardson/dev/ps2xGS/upstream/ps2xRuntime/include/runtime/gs/ps2_gs_memory.h:91:11: warning: enumeration value 'Max' not handled in switch [-Wswitch]
   91 |                 switch (psm)
      |                         ^~~
/Users/bradrichardson/dev/ps2xGS/upstream/ps2xRuntime/include/runtime/gs/ps2_gs_memory.h:91:11: note: add missing switch cases
   91 |                 switch (psm)
      |                         ^~
```

Final link lines (tail of the same log):

```
[10/17] Linking CXX static library libps2xgs_harness.a
[11/17] Building CXX object CMakeFiles/gscensus.dir/census/gscensus.cpp.o
[12/17] Linking CXX executable test_gsregs
[13/17] Linking CXX executable test_gscap_roundtrip
[14/17] Linking CXX executable gsreplay
[15/17] Linking CXX executable gscensus
[16/17] Building CXX object CMakeFiles/gsgen.dir/harness/gsgen.cpp.o
[17/17] Linking CXX executable gsgen
```

Build warnings: 3 pre-existing `-Wswitch` warnings inside upstream
`ps2_gs_memory.h` (unhandled `PixelStorageMode` enumerators).
Upstream code, untouched.

## 2. Capture format summary (`.gscap` v1)

Full spec: `docs/gscap-format.md`. Little-endian tagged records:

- Header (16 B): magic `"GSCAP001"`, version u32 (`1`), VRAM size u32.
- One record per interface call in order (`tag u32` + `size u32` +
  payload): Initialize, Reset, Submit (`GSPrimitiveBatch` /
  `GSDrawState` serialized field by field, never `memcpy`'d),
  BeginTransfer, UploadImage (payload bytes), Flush, TextureFlush,
  Sync, Present (request + returned `PresentationFrame` + full VRAM
  snapshot via `SnapshotVram`), ClearFramebuffer (+ result),
  ConsumeLocalToHostBytes (+ returned bytes), ReadVram (+ result),
  WriteVram, explicit SnapshotVram, GetTransferSnapshot.
- Trailing index record (present record offsets) + footer
  (`indexOffset u64` + magic `"GSCAPEND"`).
- Raw `uint64_t` registers travel verbatim; bit layouts live in
  `harness/gsregs.h` (TEST/ALPHA/CLAMP/TEX1/TEXA/FOGCOL/DIMX/PMODE/
  SMODE2/DISPFB/DISPLAY), matching the CPU backend's decoders.

## 3. Synthetic captures

78 captures in `/Volumes/Extreme SSD/ps2xgs/synth/`, 433,709,159
bytes total (under the 500 MB budget). Every present is 64x64.
`transfer-h2l-t8` is ~9.7 MB because it carries an extra explicit
`SnapshotVram` record on top of the per-present snapshot. (Note: the
SSD is ExFAT with 1 MB clusters plus macOS `._` sidecar files, so
`du` reports ~551 MiB on-disk; file bytes are the 433.7 MB above.)

| capture | bytes | presents |
| --- | --- | --- |
| atest-afail-fb.gscap | 5505793 | 1 |
| atest-afail-rgb.gscap | 5505793 | 1 |
| atest-afail-zb.gscap | 5505793 | 1 |
| atest-always.gscap | 5505793 | 1 |
| atest-date-datm0.gscap | 5506075 | 1 |
| atest-date-datm1.gscap | 5506075 | 1 |
| atest-equal.gscap | 5505793 | 1 |
| atest-gequal.gscap | 5505793 | 1 |
| atest-greater.gscap | 5505793 | 1 |
| atest-lequal.gscap | 5505793 | 1 |
| atest-less.gscap | 5505793 | 1 |
| atest-never.gscap | 5505793 | 1 |
| atest-notequal.gscap | 5505793 | 1 |
| blend-a-cd.gscap | 5505793 | 1 |
| blend-a-cs.gscap | 5505793 | 1 |
| blend-b-zero.gscap | 5505793 | 1 |
| blend-c-ad.gscap | 5505793 | 1 |
| blend-c-fix.gscap | 5505793 | 1 |
| blend-colclamp.gscap | 5505793 | 1 |
| blend-d-cs.gscap | 5505793 | 1 |
| blend-dthe.gscap | 5505793 | 1 |
| blend-fba.gscap | 5505793 | 1 |
| blend-pabe.gscap | 5506075 | 1 |
| clear-ct16.gscap | 5505363 | 1 |
| clear-ct16s.gscap | 5505363 | 1 |
| clear-ct24.gscap | 5505363 | 1 |
| clear-ct32.gscap | 5505380 | 1 |
| fog-flat.gscap | 5505511 | 1 |
| fog-gouraud.gscap | 5505511 | 1 |
| fog-textured.gscap | 5509656 | 1 |
| present-both.gscap | 5505465 | 1 |
| present-circuit2.gscap | 5505347 | 1 |
| present-field.gscap | 5505347 | 1 |
| present-frame.gscap | 5505347 | 1 |
| present-progressive.gscap | 5505347 | 1 |
| prim-fbmsk.gscap | 5505629 | 1 |
| prim-line-flat.gscap | 5505793 | 1 |
| prim-line-gouraud.gscap | 5505793 | 1 |
| prim-point.gscap | 5506084 | 1 |
| prim-scissor.gscap | 5505511 | 1 |
| prim-sprite.gscap | 5505511 | 1 |
| prim-trifan.gscap | 5505511 | 1 |
| prim-trilist-flat.gscap | 5505511 | 1 |
| prim-trilist-gouraud.gscap | 5505511 | 1 |
| prim-tristrip.gscap | 5505511 | 1 |
| prim-xyoffset.gscap | 5505511 | 1 |
| tex-clamp-clamp.gscap | 5509656 | 1 |
| tex-clamp-rclamp.gscap | 5509656 | 1 |
| tex-clamp-repeat.gscap | 5509656 | 1 |
| tex-clamp-rrepeat.gscap | 5509656 | 1 |
| tex-ct16.gscap | 5507608 | 1 |
| tex-ct24.gscap | 5508632 | 1 |
| tex-ct32.gscap | 5509656 | 1 |
| tex-linear.gscap | 5509656 | 1 |
| tex-nearest.gscap | 5509656 | 1 |
| tex-stq.gscap | 5509656 | 1 |
| tex-t4-csm1.gscap | 5508169 | 1 |
| tex-t4-csm2.gscap | 5507145 | 1 |
| tex-t8-csm1.gscap | 5508681 | 1 |
| tex-t8-csm2.gscap | 5508681 | 1 |
| tex-t8h.gscap | 5508681 | 1 |
| tex-texa.gscap | 5507608 | 1 |
| tex-uv.gscap | 5509656 | 1 |
| transfer-h2l-ct16.gscap | 5505851 | 1 |
| transfer-h2l-ct24.gscap | 5506107 | 1 |
| transfer-h2l-ct32.gscap | 5506363 | 1 |
| transfer-h2l-t4.gscap | 5508201 | 1 |
| transfer-h2l-t8.gscap | 9703061 | 1 |
| transfer-l2h.gscap | 5507440 | 1 |
| transfer-l2l.gscap | 5506348 | 1 |
| ztest-always.gscap | 5506075 | 1 |
| ztest-gequal-z16.gscap | 5506075 | 1 |
| ztest-gequal-z16s.gscap | 5506075 | 1 |
| ztest-gequal-z24.gscap | 5506075 | 1 |
| ztest-gequal.gscap | 5506075 | 1 |
| ztest-greater.gscap | 5506075 | 1 |
| ztest-never.gscap | 5506193 | 1 |
| ztest-zmsk.gscap | 5506075 | 1 |

Total: 78 captures, 433709159 bytes.

## 4. Identity replay (`gsreplay --backend cpu`, threshold 4, max-bad-pct 1.0)

One row per present (median_ms over a single Present call; CPU-only
timings on Apple silicon, not backend targets):

| capture | WxH | max | mean | bad% | vram | median_ms |
| --- | --- | --- | --- | --- | --- | --- |
| atest-afail-fb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 21.865 |
| atest-afail-rgb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.324 |
| atest-afail-zb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 20.974 |
| atest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 23.208 |
| atest-date-datm0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 21.776 |
| atest-date-datm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.632 |
| atest-equal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.363 |
| atest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.363 |
| atest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.432 |
| atest-lequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.589 |
| atest-less.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.443 |
| atest-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.368 |
| atest-notequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.760 |
| blend-a-cd.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.728 |
| blend-a-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.776 |
| blend-b-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 23.614 |
| blend-c-ad.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.807 |
| blend-c-fix.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.711 |
| blend-colclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 29.093 |
| blend-d-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.726 |
| blend-dthe.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.758 |
| blend-fba.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.679 |
| blend-pabe.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.514 |
| clear-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.410 |
| clear-ct16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.404 |
| clear-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.658 |
| clear-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.467 |
| fog-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.756 |
| fog-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.648 |
| fog-textured.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 23.859 |
| present-both.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 44.671 |
| present-circuit2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.396 |
| present-field.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 27.267 |
| present-frame.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.718 |
| present-progressive.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.282 |
| prim-fbmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.663 |
| prim-line-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.283 |
| prim-line-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.278 |
| prim-point.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.415 |
| prim-scissor.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 32.892 |
| prim-sprite.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.475 |
| prim-trifan.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 37.249 |
| prim-trilist-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.756 |
| prim-trilist-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.707 |
| prim-tristrip.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 29.410 |
| prim-xyoffset.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.423 |
| tex-clamp-clamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 22.698 |
| tex-clamp-rclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 25.620 |
| tex-clamp-repeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 26.102 |
| tex-clamp-rrepeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 29.040 |
| tex-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.663 |
| tex-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.497 |
| tex-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.713 |
| tex-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 28.598 |
| tex-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.546 |
| tex-stq.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.477 |
| tex-t4-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.877 |
| tex-t4-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.806 |
| tex-t8-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.497 |
| tex-t8-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.500 |
| tex-t8h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.752 |
| tex-texa.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 27.064 |
| tex-uv.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.481 |
| transfer-h2l-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 26.104 |
| transfer-h2l-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.484 |
| transfer-h2l-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.751 |
| transfer-h2l-t4.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.482 |
| transfer-h2l-t8.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.073 |
| transfer-l2h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.422 |
| transfer-l2l.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 26.214 |
| ztest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.581 |
| ztest-gequal-z16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.457 |
| ztest-gequal-z16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 25.604 |
| ztest-gequal-z24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.761 |
| ztest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 26.063 |
| ztest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 24.573 |
| ztest-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 26.111 |
| ztest-zmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 27.249 |

78 presents, 78 with max diff 0, 78 VRAM-exact. `gsreplay` exit
code 0 on every capture (non-zero exit = any failing row).
Per-capture JSON receipts: `/tmp/ps2xgs-build/replay/*.json`.

## 5. Census (`gscensus` on the 78 synthetic captures)

`census.json` receipt: `/tmp/ps2xgs-build/census.json`. Markdown
receipt below, generated by the tool (also at
`/tmp/ps2xgs-build/census.md`):

# GS feature census

Over 78 captures: 110 primitives (~228523 px est.), 12 clears, 78 presents, 34 transfers (71424 uploaded bytes), 2 local->host consumes (1024 bytes), 2 ReadVram / 1 WriteVram calls.

## Primitives

### type (count)

| value | count |
| --- | --- |
| sprite | 95 |
| trilist | 6 |
| line | 4 |
| point | 3 |
| trifan | 1 |
| tristrip | 1 |

### type (est. pixels)

| value | count |
| --- | --- |
| sprite | 219264 |
| trilist | 6912 |
| tristrip | 1152 |
| trifan | 768 |
| line | 424 |
| point | 3 |

### flags set

| value | count |
| --- | --- |
| tme | 20 |
| fst | 19 |
| abe | 11 |
| iip | 5 |
| fge | 3 |
| pabe | 2 |
| aa1 | 1 |
| ctxt | 1 |
| fix | 1 |

## Textures

### TEX0 psm

| value | count |
| --- | --- |
| CT32 | 10 |
| T4 | 3 |
| T8 | 3 |
| CT16 | 2 |
| CT24 | 1 |
| T8H | 1 |

### texture size

| value | count |
| --- | --- |
| 32x32 | 20 |

### CLUT cpsm/csm/csa

| value | count |
| --- | --- |
| CT32/csm0/csa0 | 2 |
| CT32/csm0/csa1 | 2 |
| CT32/csm1/csa0 | 2 |
| CT16/csm1/csa0 | 1 |

### TEX1 mmin/mmag/mxl

| value | count |
| --- | --- |
| mmin0/mmag0/mxl0 | 19 |
| mmin1/mmag1/mxl0 | 1 |

### CLAMP wms/wmt

| value | count |
| --- | --- |
| REPEAT/REPEAT | 17 |
| CLAMP/CLAMP | 1 |
| REGION_CLAMP/REGION_CLAMP | 1 |
| REGION_REPEAT/REGION_REPEAT | 1 |

### TEXA aem

| value | count |
| --- | --- |
| aem0 | 19 |
| aem1 | 1 |

## Blending

### ALPHA A/B/C/D/FIX

| value | count |
| --- | --- |
| A0/B1/C0/D1/fix128 | 6 |
| A0/B1/C0/D0/fix128 | 1 |
| A0/B1/C1/D1/fix128 | 1 |
| A0/B1/C2/D1/fix128 | 1 |
| A0/B2/C0/D1/fix128 | 1 |
| A1/B0/C0/D1/fix128 | 1 |

### PABE/FBA/COLCLAMP0/DTHE

| value | count |
| --- | --- |
| pabe | 2 |
| colclamp0 | 1 |
| dthe | 1 |
| fba | 1 |

## Tests

### ATST (ATE on; OFF = ATE disabled)

| value | count |
| --- | --- |
| OFF | 99 |
| LESS | 4 |
| ALWAYS | 1 |
| EQUAL | 1 |
| GEQUAL | 1 |
| GREATER | 1 |
| LEQUAL | 1 |
| NEVER | 1 |
| NOTEQUAL | 1 |

### AFAIL (only failing draws)

| value | count |
| --- | --- |
| FB_ONLY | 1 |
| RGB_ONLY | 1 |
| ZB_ONLY | 1 |

### DATE/DATM

| value | count |
| --- | --- |
| date0/datm0 | 108 |
| date1/datm0 | 1 |
| date1/datm1 | 1 |

### ZTST (/zte0 = ZTE disabled)

| value | count |
| --- | --- |
| ALWAYS | 89 |
| GEQUAL | 15 |
| GREATER | 3 |
| NEVER/zte0 | 3 |

### ZBUF psm

| value | count |
| --- | --- |
| Z32 | 101 |
| Z16 | 3 |
| Z16S | 3 |
| Z24 | 3 |

ZMSK draws: 3

### FRAME psm

| value | count |
| --- | --- |
| CT32 | 119 |
| CT16 | 1 |
| CT16S | 1 |
| CT24 | 1 |

FBMSK nonzero draws: 1

### SCISSOR WxH

| value | count |
| --- | --- |
| 64x64 | 109 |
| 17x17 | 1 |

### SCANMSK nonzero

| value | count |
| --- | --- |
| scanmsk2 | 1 |

Fogged (FGE) primitives: 3

## Transfers

### direction

| value | count |
| --- | --- |
| host->local | 32 |
| local->host | 1 |
| local->local | 1 |

### psm

| value | count |
| --- | --- |
| CT32 | 20 |
| CT16 | 4 |
| T4 | 3 |
| T8 | 3 |
| CT24 | 2 |
| CT32+local-local | 1 |
| T8H | 1 |

## Presentation

### PMODE circuits

| value | count |
| --- | --- |
| en1/-- | 76 |
| --/en2 | 1 |
| en1/en2/mmod | 1 |

### SMODE2 mode

| value | count |
| --- | --- |
| progressive | 76 |
| field | 1 |
| frame | 1 |

### DISPFB psm

| value | count |
| --- | --- |
| CT32 | 75 |
| CT16 | 1 |
| CT16S | 1 |
| CT24 | 1 |

### present size

| value | count |
| --- | --- |
| 64x64 | 78 |

## 6. CTest

Receipt: `/tmp/ps2xgs-build/g0-ctest.log`. Full output:

```
Test project /tmp/ps2xgs-build
    Start 1: gsregs-roundtrip
1/6 Test #1: gsregs-roundtrip .................   Passed    0.07 sec
    Start 2: gscap-roundtrip
2/6 Test #2: gscap-roundtrip ..................   Passed    0.24 sec
    Start 3: gen-synth
3/6 Test #3: gen-synth ........................   Passed    7.84 sec
    Start 4: identity-replay
4/6 Test #4: identity-replay ..................   Passed   15.09 sec
    Start 5: run-census
5/6 Test #5: run-census .......................   Passed    8.66 sec
    Start 6: census-features
6/6 Test #6: census-features ..................   Passed    0.43 sec

100% tests passed out of 6

Total Test time (real) =  32.33 sec
```

Test mapping to the brief: (a) `gsregs-roundtrip` = register
encode/decode round trips; (b) `gscap-roundtrip` = capture
write→read round trip on a small stream through
`GSRecordingBackend(CPU)`; (c) `identity-replay` = `gsreplay
--backend cpu` over every synthetic capture (§4 table above);
(d) `census-features` = expected feature rows in `census.json`
(`tests/check_census.py`).

## 7. Exact commands

```
git submodule add https://github.com/ran-j/PS2Recomp.git upstream
git -C upstream checkout 14b1e5cb
cmake -S . -B /tmp/ps2xgs-build -G Ninja
cmake --build /tmp/ps2xgs-build
ctest --test-dir /tmp/ps2xgs-build --output-on-failure
/tmp/ps2xgs-build/gsgen "/Volumes/Extreme SSD/ps2xgs/synth"
 /tmp/ps2xgs-build/gsreplay <capture.gscap> --backend cpu [--repeat N] [--threshold 4] [--max-bad-pct 1.0] [--json out.json]
/tmp/ps2xgs-build/gscensus <capture...> --json census.json --md census.md
```

`PS2XGS_SYNTH_DIR` CMake cache var overrides the capture directory
(default `/Volumes/Extreme SSD/ps2xgs/synth`).

## 8. Receipt paths

- `/tmp/ps2xgs-build/` — CMake build dir (APFS): binaries
  (`gsreplay`, `gsgen`, `gscensus`, `test_gsregs`,
  `test_gscap_roundtrip`), `g0-configure-build.log`,
  `g0-ctest.log`, `g0-identity-table.md`, `census.json`,
  `census.md`, `replay/*.json` (78 per-capture replay receipts).
- `/Volumes/Extreme SSD/ps2xgs/synth/` — 78 `.gscap` captures
  (§3 table). Nothing over 5 MB is in the repo; no captures are
  committed.
- Repo commits (prefix `[G0]`, same trailers): skeleton+pin,
  format+recording, gsreplay, gsregs+gsgen, gscensus, tests, this
  report.

## 9. Shims and upstream issue candidates

Shims made to compile upstream standalone: none.

Candidates for upstream (behavior observed while writing the
generator/census, upstream code untouched):

1. `GSCpuBackend` never reads `GSDrawState.context.tex1` — TEX1
   filtering/mip fields have no effect; `linearFilter`/
   `textureWidth`/`textureHeight` are carried resolved instead.
2. These carried values have no effect in the CPU backend: `DIMX`/
   `DTHE`, `COLCLAMP`, `SCANMSK`, `GSPrimReg.aa1`, `GSPrimReg.fix`,
   and the `ZTE` bit (`ZTST` is read directly regardless of `ZTE`).
   The generator still emits distinct values for them and the
   census still reports them, so a future backend that honors them
   will show up as diffs.
3. Pre-existing `-Wswitch` warnings in
   `runtime/gs/ps2_gs_memory.h` (unhandled `PixelStorageMode`
   enumerators, e.g. `Max`).
4. `Present` with `fbp == 0` and an all-black frame falls back to
   scanning `contextFrames` for a non-black candidate — a heuristic
   worth knowing when authoring present tests.

## 10. What I could not do

- Game reference captures (plan Gate C): the P1b boot stall is
  owned by the P-series briefs; no game frames exist to capture.
- TEX1 `MXL > 0` mip chains: the synthetic set uses `MXL = 0`
  throughout (see candidate 1 above); mip addressing is unexercised.
- Vulkan/`vulkan` backend selection: out of scope for G0 (factory
  stub in `harness/backend_factory.cpp` is the S1 seam).
- `median_ms` figures are single-Present-call CPU timings on this
  laptop; they are recorded observations, not performance claims.

