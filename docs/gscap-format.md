# `.gscap` capture format (v1)

Little-endian tagged records. All multi-byte integers are stored
little-endian regardless of host. Floats are stored as their IEEE-754
bit patterns (`float` → u32, `double` → u64). `bool` is one byte
(`0`/`1`). Enums are stored as their underlying width (`GSPrimType`
and `GSSyncReason` as u8). No struct is ever `memcpy`'d: every
`GSPrimitiveBatch` / `GSDrawState` field is written explicitly so an
upstream layout change breaks the build (missing field) instead of
silently corrupting captures.

## File layout

```
header
record*            # one per GSRasterBackend call, in call order
index-record
footer
```

**Header** (16 bytes):

| offset | size | field |
| --- | --- | --- |
| 0 | 8 | magic `"GSCAP001"` |
| 8 | 4 | version u32 (`1`) |
| 12 | 4 | VRAM size in bytes u32 (4 MiB for the real GS) |

**Record**: `tag u32` + `payloadSize u32` + `payload[payloadSize]`.

**Index record**: tag `0xFFFFFFFE`, payload = `count u32` followed by
`count` file offsets (u64 each) of every `Present` record's tag —
the "trailing index of present offsets".

**Footer** (16 bytes): `indexRecordOffset u64` + magic `"GSCAPEND"`.
A reader validates the tail magic, seeks to the index, and learns the
present offsets without scanning.

## Record tags and payloads

| tag | name | payload |
| --- | --- | --- |
| 1 | Initialize | `vramSize u32` |
| 2 | Reset | (empty) |
| 3 | Submit | `GSPrimitiveBatch` (below) |
| 4 | BeginTransfer | `GSTransferCommand` (below) |
| 5 | UploadImage | `sizeBytes u32` + raw bytes |
| 6 | Flush | (empty) |
| 7 | TextureFlush | (empty) |
| 8 | Sync | `reason u8` |
| 9 | Present | `GSPresentationRequest` + reference `PresentationFrame` (width, height, displayFbp, sourceFbp u32; usedPreferred u8; `pixelBytes u32` + RGBA8 pixels) + full VRAM snapshot (`vramBytes u32` + bytes, taken via `SnapshotVram`) |
| 10 | ClearFramebuffer | `GSContext` + `rgba u32` + `result u8` |
| 11 | ConsumeLocalToHostBytes | `maxBytes u32` + `count u32` + `count` returned bytes |
| 12 | ReadVram | `psm, base, bw, x, y u32` + `result u32` |
| 13 | WriteVram | `psm, base, bw, x, y, value u32` |
| 14 | SnapshotVram | `vramBytes u32` + bytes (explicit frontend call; the per-present snapshot lives inside record 9) |
| 15 | GetTransferSnapshot | `x, y, totalPixels, copiedPixels, direction u32` + `pendingBytes u64` |
| 0xFFFFFFFE | Index | `count u32` + `count` × `offset u64` |

## Structure encodings (field order on the wire)

- `GSVertex`: x f32, y f32, z f64, r u8, g u8, b u8, a u8, q f32,
  s f32, t f32, u u16, v u16, fog u8.
- `GSFrameReg`: fbp u32, fbw u32, psm u8, fbmsk u32.
- `GSZbufReg`: zbp u32, psm u8, zmask u8.
- `GSScissorReg`: x0 u16, x1 u16, y0 u16, y1 u16.
- `GSTex0Reg`: tbp0 u32, tbw u8, psm u8, tw u8, th u8, tcc u8,
  tfx u8, cbp u32, cpsm u8, csm u8, csa u8, cld u8.
- `GSXYOffsetReg`: ofx u16, ofy u16.
- `GSTexaReg`: ta0 u8, aem u8, ta1 u8.
- `GSTexClutReg`: cbw u8, cou u8, cov u16.
- `GSContext`: frame, scissor, tex0, xyoffset, zbuf, tex1 u64,
  miptbp1 u64, miptbp2 u64, clamp u64, alpha u64, test u64, fba u64.
- `GSPrimReg`: type u8, iip u8, tme u8, fge u8, abe u8, aa1 u8,
  fst u8, ctxt u8, fix u8.
- `GSDrawState`: context, prim, texa, texclut, pabe u8,
  scanmsk u64, dimx u64, dthe u64, colclamp u64, fogR u8, fogG u8,
  fogB u8, textureWidth u16, textureHeight u16, linearFilter u8.
- `GSPrimitiveBatch`: vertices[3], vertexCount u8, state.
- `GSBitBltBuf`: sbp u32, sbw u8, spsm u8, dbp u32, dbw u8,
  dpsm u8.
- `GSTrxPos`: ssax u16, ssay u16, dsax u16, dsay u16, dir u8.
- `GSTrxReg`: rrw u16, rrh u16.
- `GSTransferCommand`: bitbltbuf, trxpos, trxreg, direction u32.
- `GSPresentationRequest`: pmode u64, smode2 u64, dispfb1 u64,
  display1 u64, dispfb2 u64, display2 u64, bgcolor u64,
  vsyncTick u64, contextFrames[2], preferredSource (`GSFrameReg`),
  preferredDestFbp u32, hasPreferredSource u8.

Raw `uint64_t` registers (`TEX1`, `CLAMP`, `ALPHA`, `TEST`, `PMODE`,
`SMODE2`, `DISPFB*`, `DISPLAY*`, `DIMX`, …) travel verbatim; their
bit layouts are encoded/decoded by `harness/gsregs.h`.
