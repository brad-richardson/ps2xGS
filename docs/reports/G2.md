# G2 report — Harness sensitivity: second backend + mip chains

Brief: G2 runbook. Read: `docs/reports/G0.md` (§9–§10),
`docs/gscap-format.md`, `harness/backend_factory.h`,
`harness/gsregs.h`, plan
`/Users/bradrichardson/dev/ssx3/docs/plan-gs-gpu-backend-2026-09-18.md`
§1–§3. Host-only, no device, no emulator. `cpu` backend untouched;
`upstream/` untouched. Old 78 captures byte-identical (`cmp` clean).

## 1. New captures (20)

`/Volumes/Extreme SSD/ps2xgs/synth/`, 1 present each (64x64).
New bytes 110183059; full set 98 files, 543892218 bytes.

| capture | bytes | presents |
| --- | --- | --- |
| tex1-filter-agree-nearest.gscap | 5509656 | 1 |
| tex1-filter-agree-linear.gscap | 5509656 | 1 |
| tex1-filter-disagree-lin.gscap | 5509656 | 1 |
| tex1-filter-disagree-near.gscap | 5509656 | 1 |
| tex1-filter-mixed-mmin.gscap | 5509656 | 1 |
| mip-chain-mxl0.gscap | 5517946 | 1 |
| mip-chain-mxl1.gscap | 5517946 | 1 |
| mip-chain-mxl2.gscap | 5517946 | 1 |
| mip-chain-stq-mxl2.gscap | 5517946 | 1 |
| iso-dthe-off.gscap | 5505511 | 1 |
| iso-dthe-on.gscap | 5505511 | 1 |
| iso-colclamp-on.gscap | 5505793 | 1 |
| iso-colclamp-off.gscap | 5505793 | 1 |
| iso-scanmsk-zero.gscap | 5505511 | 1 |
| iso-scanmsk-set.gscap | 5505511 | 1 |
| iso-aa1-clear.gscap | 5505511 | 1 |
| iso-aa1-set.gscap | 5505511 | 1 |
| iso-zte-on-always.gscap | 5506075 | 1 |
| iso-zte-off-always.gscap | 5506075 | 1 |
| iso-zte-off-never.gscap | 5506193 | 1 |

Mip chains: L0 red / L1 green / L2 blue, 32x32 CT32 at
tbp 128/136/144, `MIPTBP1` set, `MXL` 0/1/2. TEX1 filter cases use
2x magnification (16 texels → 32 pixels) so nearest-vs-linear differ.

## 2. Isolation pair table (Step 2)

| capture A | capture B | single differing field |
| --- | --- | --- |
| iso-dthe-off.gscap | iso-dthe-on.gscap | DTHE (0 vs 1; DIMX matrix held constant) |
| iso-colclamp-on.gscap | iso-colclamp-off.gscap | COLCLAMP (1 vs 0; overflowing blend A0/B2/C2/D1/fix255) |
| iso-scanmsk-zero.gscap | iso-scanmsk-set.gscap | SCANMSK (0 vs 2) |
| iso-aa1-clear.gscap | iso-aa1-set.gscap | aa1 (false vs true) |
| iso-zte-on-always.gscap | iso-zte-off-always.gscap | ZTE (on vs off; ZTST ALWAYS both) |

Control: `iso-zte-off-never.gscap` (ZTE off, ZTST NEVER,
pre-fill `0xFF202020`, draws that cpu rejects).

## 3. Backend diff summary (`strict`)

New: `harness/strict_backend.h` (43 lines),
`harness/strict_backend.cpp` (296 lines). Modified:
`harness/backend_factory.cpp` (`"strict"` registration),
`CMakeLists.txt` (strict sources in `gsreplay`),
`harness/gsregs.h` (`MIPTBP1/2` encode/decode),
`harness/gsgen.cpp` (20 cases). `cpu` behaviour unchanged
(old 78 `cmp` clean; old 78 cpu replay rows all max 0, vram yes).

`GSStrictBackend` wraps `GSCpuBackend`; forwards everything
except `Submit`. `Submit` behaviour:

- ZTE off forces `ZTST=ALWAYS` (no override when already ALWAYS).
- When `tme`: `linear = mmin||mmag`, overrides `linearFilter`.
- When `tme` and `MXL>0`: `LOD=min(MXL,2)` from `MIPTBP1`
  (levels same 32x32 size in G2 captures; only `tbp`/`tbw` swapped).
- `SCANMSK`: 2 restores even rows, 3 restores odd rows (frame+depth
  from pre-draw snapshot); other nonzero restores even rows.
- `DTHE` nonzero: adds `DIMX[y%4][x%4]` (0–7) to drawn RGB (CT32).
- `COLCLAMP=0` with blend on untextured unfogged draws, no
  `FBMSK`/`FBA`: recomputes blend without clamp, wraps mod 256.
- `aa1`: halves drawn RGB (CT32).
- Fast path: when no honored field is active the batch is
  forwarded unmodified.

`gsreplay --backend strict` runs: all 98 captures, one row each
(§5 table); per-capture JSON in
`/tmp/ps2xgs-build/g2-replay-strict/*.json` (98 files).

## 4. Identity replay on new captures (`cpu`)

Threshold 4, max-bad-pct 1.0. 20 rows, all max 0, vram yes.

| capture | WxH | max | mean | bad% | vram | median_ms |
| --- | --- | --- | --- | --- | --- | --- |
| tex1-filter-agree-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.757 |
| tex1-filter-agree-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.043 |
| tex1-filter-disagree-lin.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.434 |
| tex1-filter-disagree-near.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.837 |
| tex1-filter-mixed-mmin.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.024 |
| mip-chain-mxl0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.882 |
| mip-chain-mxl1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.412 |
| mip-chain-mxl2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.975 |
| mip-chain-stq-mxl2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.873 |
| iso-dthe-off.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.959 |
| iso-dthe-on.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.807 |
| iso-colclamp-on.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.832 |
| iso-colclamp-off.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.857 |
| iso-scanmsk-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.881 |
| iso-scanmsk-set.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.994 |
| iso-aa1-clear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.943 |
| iso-aa1-set.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.947 |
| iso-zte-on-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.881 |
| iso-zte-off-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.847 |
| iso-zte-off-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.922 |

Old 78 cpu replay rows: 78 rows max 0, vram yes (separate run,
`fails=0` over the 78 pre-G2 files).

## 5. Cross table `cpu`-vs-`strict` (98 rows)

`gsreplay --backend strict` vs cpu references. 14 rows with
max>0 and vram NO; 84 rows with max 0 and vram yes.

| capture | WxH | max | mean | bad% | vram | median_ms |
| --- | --- | --- | --- | --- | --- | --- |
| atest-afail-fb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.695 |
| atest-afail-rgb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.874 |
| atest-afail-zb.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.971 |
| atest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.542 |
| atest-date-datm0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.029 |
| atest-date-datm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.882 |
| atest-equal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.854 |
| atest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.279 |
| atest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.767 |
| atest-lequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.834 |
| atest-less.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.798 |
| atest-never.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.468 |
| atest-notequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.733 |
| blend-a-cd.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.786 |
| blend-a-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.747 |
| blend-b-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.125 |
| blend-c-ad.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.851 |
| blend-c-fix.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.815 |
| blend-colclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.830 |
| blend-d-cs.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.227 |
| blend-dthe.gscap | 64x64 | 7 | 0.0082 | 0.0879 | NO | 15.970 |
| blend-fba.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.617 |
| blend-pabe.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.914 |
| clear-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.272 |
| clear-ct16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.727 |
| clear-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.224 |
| clear-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.753 |
| fog-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.003 |
| fog-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.983 |
| fog-textured.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.705 |
| iso-aa1-clear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.851 |
| iso-aa1-set.gscap | 64x64 | 100 | 0.3428 | 0.5273 | NO | 16.192 |
| iso-colclamp-off.gscap | 64x64 | 232 | 0.3883 | 0.2344 | NO | 15.872 |
| iso-colclamp-on.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.013 |
| iso-dthe-off.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.764 |
| iso-dthe-on.gscap | 64x64 | 7 | 0.0185 | 0.1978 | NO | 15.890 |
| iso-scanmsk-set.gscap | 64x64 | 200 | 0.3428 | 0.2637 | NO | 15.819 |
| iso-scanmsk-zero.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.814 |
| iso-zte-off-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.853 |
| iso-zte-off-never.gscap | 64x64 | 168 | 0.6375 | 0.9375 | NO | 15.831 |
| iso-zte-on-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.823 |
| mip-chain-mxl0.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.870 |
| mip-chain-mxl1.gscap | 64x64 | 255 | 0.3984 | 0.1562 | NO | 16.811 |
| mip-chain-mxl2.gscap | 64x64 | 255 | 0.3984 | 0.1562 | NO | 16.124 |
| mip-chain-stq-mxl2.gscap | 64x64 | 255 | 0.4482 | 0.1758 | NO | 16.770 |
| present-both.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 29.129 |
| present-circuit2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.866 |
| present-field.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.380 |
| present-frame.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.080 |
| present-progressive.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.182 |
| prim-fbmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.727 |
| prim-line-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.808 |
| prim-line-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.937 |
| prim-point.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.763 |
| prim-scissor.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.001 |
| prim-sprite.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.259 |
| prim-trifan.gscap | 64x64 | 255 | 0.0923 | 0.0476 | NO | 15.905 |
| prim-trilist-flat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.757 |
| prim-trilist-gouraud.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.467 |
| prim-tristrip.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.731 |
| prim-xyoffset.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.825 |
| tex-clamp-clamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.773 |
| tex-clamp-rclamp.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.750 |
| tex-clamp-repeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.850 |
| tex-clamp-rrepeat.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.369 |
| tex-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.817 |
| tex-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.884 |
| tex-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.841 |
| tex-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.180 |
| tex-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 15.964 |
| tex-stq.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.169 |
| tex-t4-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.248 |
| tex-t4-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.517 |
| tex-t8-csm1.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.491 |
| tex-t8-csm2.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 19.673 |
| tex-t8h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 18.100 |
| tex-texa.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.557 |
| tex-uv.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.994 |
| tex1-filter-agree-linear.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.147 |
| tex1-filter-agree-nearest.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.545 |
| tex1-filter-disagree-lin.gscap | 64x64 | 123 | 0.0165 | 0.0097 | NO | 16.846 |
| tex1-filter-disagree-near.gscap | 64x64 | 123 | 0.0165 | 0.0097 | NO | 17.070 |
| tex1-filter-mixed-mmin.gscap | 64x64 | 123 | 0.0165 | 0.0097 | NO | 17.034 |
| transfer-h2l-ct16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.911 |
| transfer-h2l-ct24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.176 |
| transfer-h2l-ct32.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.986 |
| transfer-h2l-t4.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.042 |
| transfer-h2l-t8.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.644 |
| transfer-l2h.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.195 |
| transfer-l2l.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.000 |
| ztest-always.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.138 |
| ztest-gequal-z16.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.036 |
| ztest-gequal-z16s.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 16.997 |
| ztest-gequal-z24.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.806 |
| ztest-gequal.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.864 |
| ztest-greater.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.229 |
| ztest-never.gscap | 64x64 | 104 | 0.6125 | 0.9375 | NO | 16.973 |
| ztest-zmsk.gscap | 64x64 | 0 | 0.0000 | 0.0000 | yes | 17.058 |

## 6. Flipped-row field map

Rows from §5 with max>0 and vram NO (14), with the honored
field(s) active in the capture.

| capture | field |
| --- | --- |
| blend-dthe.gscap | DIMX/DTHE |
| prim-trifan.gscap | SCANMSK + aa1 (scanmsk 2, aa1 set; fix also set, ignored by strict) |
| ztest-never.gscap | ZTE (off, ZTST NEVER) |
| tex1-filter-disagree-lin.gscap | TEX1 filtering (cpu nearest, TEX1 linear) |
| tex1-filter-disagree-near.gscap | TEX1 filtering (cpu linear, TEX1 nearest) |
| tex1-filter-mixed-mmin.gscap | TEX1 filtering (mmin 1/mmag 0, strict linear) |
| mip-chain-mxl1.gscap | TEX1 mip (MXL 1, L1 green vs L0 red) |
| mip-chain-mxl2.gscap | TEX1 mip (MXL 2, L2 blue vs L0 red) |
| mip-chain-stq-mxl2.gscap | TEX1 mip + STQ (MXL 2, triangle, L2 vs L0) |
| iso-dthe-on.gscap | DIMX/DTHE |
| iso-colclamp-off.gscap | COLCLAMP (0, overflowing blend) |
| iso-scanmsk-set.gscap | SCANMSK (2) |
| iso-aa1-set.gscap | aa1 |
| iso-zte-off-never.gscap | ZTE (off, ZTST NEVER) |

Note: `blend-colclamp.gscap` (G0, COLCLAMP 0) row is max 0,
vram yes; its blend `((Cs-Cd)*128>>7)+Cd` equals `Cs` (no
overflow), so clamp and wrap coincide.

## 7. Census delta + gaps

`gscensus` exit 0 on the 20 new captures and on all 98; no
undecodable records. MIPTBP addresses are carried in captures
but not separately reported by the census (TEX1 `mxl` only);
not patched per the brief.

Delta (G0 §5 → G2 full): files 78 → 98, submits 110 → 138,
presents 78 → 98, clears 12 → 13, transfers 34 → 51,
uploaded bytes 71424 → 141056. New rows: TEX1
`mmin0/mmag0/mxl1` (1), `mmin0/mmag0/mxl2` (2),
`mmin1/mmag0/mxl0` (1); ZTST `ALWAYS/zte0` (3); ALPHA
`A0/B2/C2/D1/fix255` (2). Full census over the 20 new:

```
# GS feature census

Over 20 captures: 28 primitives (~57984 px est.), 1 clears, 20 presents, 17 transfers (69632 uploaded bytes), 0 local->host consumes (0 bytes), 0 ReadVram / 0 WriteVram calls.

## Primitives

### type (count)

| value | count |
| --- | --- |
| sprite | 27 |
| trilist | 1 |

### type (est. pixels)

| value | count |
| --- | --- |
| sprite | 56832 |
| trilist | 1152 |

### flags set

| value | count |
| --- | --- |
| tme | 9 |
| fst | 8 |
| abe | 2 |
| aa1 | 1 |

## Textures

### TEX0 psm

| value | count |
| --- | --- |
| CT32 | 9 |

### texture size

| value | count |
| --- | --- |
| 32x32 | 9 |

### TEX1 mmin/mmag/mxl

| value | count |
| --- | --- |
| mmin0/mmag0/mxl0 | 3 |
| mmin0/mmag0/mxl2 | 2 |
| mmin1/mmag1/mxl0 | 2 |
| mmin0/mmag0/mxl1 | 1 |
| mmin1/mmag0/mxl0 | 1 |

### CLAMP wms/wmt

| value | count |
| --- | --- |
| REPEAT/REPEAT | 9 |

### TEXA aem

| value | count |
| --- | --- |
| aem0 | 9 |

## Blending

### ALPHA A/B/C/D/FIX

| value | count |
| --- | --- |
| A0/B2/C2/D1/fix255 | 2 |

### PABE/FBA/COLCLAMP0/DTHE

| value | count |
| --- | --- |
| colclamp0 | 1 |
| dthe | 1 |

## Tests

### ATST (ATE on; OFF = ATE disabled)

| value | count |
| --- | --- |
| OFF | 28 |

### DATE/DATM

| value | count |
| --- | --- |
| date0/datm0 | 28 |

### ZTST (/zte0 = ZTE disabled)

| value | count |
| --- | --- |
| ALWAYS | 22 |
| ALWAYS/zte0 | 3 |
| NEVER/zte0 | 3 |

### ZBUF psm

| value | count |
| --- | --- |
| Z32 | 28 |

ZMSK draws: 0

### FRAME psm

| value | count |
| --- | --- |
| CT32 | 29 |

FBMSK nonzero draws: 0

### SCISSOR WxH

| value | count |
| --- | --- |
| 64x64 | 28 |

### SCANMSK nonzero

| value | count |
| --- | --- |
| scanmsk2 | 1 |

Fogged (FGE) primitives: 0

## Transfers

### direction

| value | count |
| --- | --- |
| host->local | 17 |

### psm

| value | count |
| --- | --- |
| CT32 | 17 |

## Presentation

### PMODE circuits

| value | count |
| --- | --- |
| en1/-- | 20 |

### SMODE2 mode

| value | count |
| --- | --- |
| progressive | 20 |

### DISPFB psm

| value | count |
| --- | --- |
| CT32 | 20 |

### present size

| value | count |
| --- | --- |
| 64x64 | 20 |
```

## 8. CTest output

Receipt: `/tmp/ps2xgs-build/g2-ctest.log`. Full output:

```
Test project /tmp/ps2xgs-build
    Start 1: gsregs-roundtrip
1/9 Test #1: gsregs-roundtrip .................   Passed    0.00 sec
    Start 2: gscap-roundtrip
2/9 Test #2: gscap-roundtrip ..................   Passed    0.18 sec
    Start 3: gen-synth
3/9 Test #3: gen-synth ........................   Passed    5.85 sec
    Start 4: identity-replay
4/9 Test #4: identity-replay ..................   Passed   15.75 sec
    Start 5: run-census
5/9 Test #5: run-census .......................   Passed    5.71 sec
    Start 6: census-features
6/9 Test #6: census-features ..................   Passed    0.08 sec
    Start 7: strict-diffs-on-mip
7/9 Test #7: strict-diffs-on-mip ..............   Passed    0.48 sec
    Start 8: strict-exact-on-control
8/9 Test #8: strict-exact-on-control ..........   Passed    1.45 sec
    Start 9: cpu-identity-on-new-captures
9/9 Test #9: cpu-identity-on-new-captures .....   Passed    3.15 sec

100% tests passed out of 9

Total Test time (real) =  32.65 sec
```

Test mapping: (7) `gsreplay --backend strict` exits 1 on
`mip-chain-mxl1/mxl2/stq-mxl2`; (8) `gsreplay --backend
strict` exits 0 on the 9 controls (§6 exact side +
`mip-chain-mxl0`); (9) `gsreplay --backend cpu` exits 0 on
the 20 new.

## 9. Exact commands

```
cmake -S . -B /tmp/ps2xgs-build -G Ninja
cmake --build /tmp/ps2xgs-build -j4
ctest --test-dir /tmp/ps2xgs-build --output-on-failure
/tmp/ps2xgs-build/gsgen "/Volumes/Extreme SSD/ps2xgs/synth"
/tmp/ps2xgs-build/gsreplay <capture.gscap> --backend cpu|strict [--json out.json]
/tmp/ps2xgs-build/gscensus <capture...> --json census.json --md census.md
```

`PS2XGS_SYNTH_DIR` CMake cache var overrides the capture
directory (default `/Volumes/Extreme SSD/ps2xgs/synth`).

## 10. Receipt paths

- `/tmp/ps2xgs-build/` — binaries (`gsreplay`, `gsgen`,
  `gscensus`), `g2-ctest.log`, `g2-identity-table.md`,
  `g2-cross-table.md`, `g2-census-new.json/.md`,
  `g2-census-all.json/.md`, `census.json/.md` (98, from
  CTest), `g2-replay-cpu/*.json` (20),
  `g2-replay-strict/*.json` (98).
- `/Volumes/Extreme SSD/ps2xgs/synth/` — 98 `.gscap`
  captures (§1 table for the 20 new; old 78 unchanged).
  Nothing over 5 MB in the repo; no captures committed.
- Repo commits (prefix `[G2]`, trailer `Orchestrated-By: Muse
  Code`): Steps 1–2 generator, Step 3 strict backend, Step 1
  fixup (2x magnify), Step 5 tests, this report.

## 11. What I could not do

- Game reference captures (plan Gate C): G1 stays queued
  behind first frame; this brief uses synthetics alone.
- Second oracle for reference-incomplete features (plan §5a:
  PCSX2 software renderer or hand-computed expectations):
  outside this brief; references here are cpu-recorded.
- `fix` field: strict ignores it per the brief's honored list
  (only TEX1/mip, DIMX/DTHE, COLCLAMP, SCANMSK, `aa1`, ZTE);
  `prim-trifan` carries `fix` with no strict effect beyond
  its `aa1`/`SCANMSK`.
- COLCLAMP wrap is implemented for untextured unfogged draws
  without `FBMSK`/`FBA` (covers G2's clamp cases); textured,
  fogged, or masked blends with `COLCLAMP=0` forward
  unmodified.
- Mip `LOD=min(MXL,2)` over same-size 32x32 levels only; no
  UV rescale for halved levels; `MXL>2` clamps to L2;
  `MIPTBP2` (levels 4–6) unset in G2 captures.
- Dither adds `DIMX` after blend (post-process on drawn
  pixels); `aa1` halves drawn RGB as a coverage marker
  rather than edge-specific filtering.
- `Present` path is forwarded unmodified (no honored fields
  in presentation).
- `median_ms` figures are single-Present-call CPU timings on
  this machine; recorded observations, not performance claims.

