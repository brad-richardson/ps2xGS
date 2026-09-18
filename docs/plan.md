# Plan: a GPU Graphics Synthesizer backend for PS2Recomp (prep for an autonomous loop)

Date: 2026-09-18. Owner of judgment: the orchestrator session. Executors:
muse briefs (prep) and, once the gates below are met, a sandboxed local
agent on the new Mac mini running the staged loop.

## 0. Why and what

The PS2 route to a true 120 Hz simulation (see `docs/todo.md`, "PS2
throughput gate READ" and "P1 read") is blocked on exactly one large item:
PS2Recomp's Graphics Synthesizer is a CPU rasterizer
(`ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp`, ~1,900 lines). Everything else
the game needs is either measured to fit or is a queue of small HLE fixes.
The runtime already exposes the seam we need: an abstract
`GSRasterBackend` (`ps2xRuntime/include/runtime/gs/gs_backend.h`) injected
through `GS::setRasterBackend(std::unique_ptr<GSRasterBackend>)`. The
deliverable is a second implementation of that interface running on the
GPU, developed in its own repository against a pinned upstream commit
(`14b1e5cb`, head since 2026-08-18).

An autonomous agent can only grind on this if three things exist first:
deterministic inputs (captured GS streams), a pass/fail it cannot argue
with (replay-and-diff against the CPU backend), and a ranked work queue
(the feature census). This document scopes those three, the design
decisions that must be made by a person before the loop starts, the
staged plan the loop follows, and the repository.

## 1. The interface, as it is today

Per-call contract (all in `gs_backend.h` / `gs_types.h`):

| Call | What crosses the seam |
| --- | --- |
| `Initialize(vram, size)` / `Reset()` | a 4 MB VRAM byte array owned by the frontend |
| `Submit(const GSPrimitiveBatch&)` | **one primitive**: up to 3 `GSVertex` (x, y float; z double; rgba; q, s, t; u, v; fog) plus the complete `GSDrawState` (both-context registers: frame, zbuf, scissor, tex0, tex1, miptbp1/2, clamp, alpha, test, fba, xyoffset; prim flags iip/tme/fge/abe/aa1/fst/ctxt/fix; texa, texclut, pabe, scanmsk, dimx, dthe, colclamp, fog colour, resolved texture size, linear filter) |
| `BeginTransfer(cmd)` + `UploadImage(bytes)` | BITBLTBUF/TRXPOS/TRXREG host→local, local→local, local→host image transfers |
| `Flush()`, `TextureFlush()`, `Sync(reason)` | ordering points (Finish, LocalToHost, Presentation, DebugReadback, Reset) |
| `Present(request)` → `PresentationFrame` | PMODE/SMODE2/DISPFB/DISPLAY registers in, RGBA8 pixels + dimensions out |
| `ClearFramebuffer(ctx, rgba)` | fast clear |
| `ReadVram/WriteVram(psm, base, bw, x, y)`, `SnapshotVram`, `ConsumeLocalToHostBytes` | **VRAM is memory**: the frontend and the game read it back, with format aliasing over the same bytes |
| `GetTransferSnapshot()` | transfer progress for the frontend's state machine |

Consequences: batching across primitives is the backend's job; VRAM
coherence (GPU copy vs. software reads, PSMCT32/16/PSMT8/4 aliasing) is the
central design problem; the interface is one month old (GS refactor
#204) and may move, so the repo pins upstream as a submodule and tracks
deliberately.

## 2. Gate A — the harness (`gsharness`)

Purpose: turn any GS stream into a deterministic, self-checking test.

**Capture.** A `GSRecordingBackend : GSRasterBackend` that wraps the CPU
backend, forwards every call, and appends an ordered record to a `.gscap`
file: the call tag, its arguments, image payload bytes for uploads, the
bytes returned by `ConsumeLocalToHostBytes`, `ReadVram` results, and at
every `Present` the returned `PresentationFrame` pixels plus a full VRAM
snapshot (the reference). Injection point: the runner's `setRasterBackend`
call, enabled by an environment variable (`PS2X_GS_CAPTURE=<path>`) in a
one-hunk patch carried in our fork branch of the runner. Format: little-
endian tagged records with a frame index at the end; expect ~5–10 MB per
present (VRAM snapshot dominates), so keep captures on the mini or the
SSD, never in git.

**Replay.** `gsreplay <capture> --backend cpu|gpu [--repeat N]`
instantiates the chosen backend, feeds the records in order, and at each
`Present` compares pixels to the reference; at each snapshot point
compares VRAM. Output: one row per present (max channel diff, mean diff,
% of pixels differing by more than the threshold, byte-exact yes/no for
VRAM), plus timing (median ms per present over `--repeat`, GPU submit +
wait included). Exit code is the verdict.

**Identity test.** Replaying a capture into the CPU backend must be
byte-exact against its own reference. That is the harness's own
acceptance test and it needs no game.

**Synthetic streams.** A generator emits small captures that exercise one
feature per file: each primitive type, flat/Gouraud, each texture format
with and without CLUT, each clamp mode, filtering, each alpha-blend
equation family, each test-register combination the census finds, each
transfer direction and format, interlaced vs. progressive present. These
exist before the game renders and are the unit tests of the loop.

**Reference set from the game** (Gate C below): 3–5 consecutive presents
each from the front-end menu, Snow Jam riding, the six-rider start gate,
and two heavier courses (Crow's Nest, Metro-City per the corpus
ranking), captured on the Mac from the P1 runner.

## 3. Gate B — the census (`gscensus`)

Reads captures (game and synthetic) and emits `census.json` plus a
Markdown table, ranked by frequency, of everything the game asks the GS
to do:

- primitives: type, iip, tme, fge, abe, aa1, fst, fix, ctxt; counts and
  pixel area estimates;
- textures: TEX0 psm, CLUT psm/csm/cbp usage, sizes, TEX1 filtering and
  mip levels actually addressed, CLAMP modes, TEXA usage;
- blending: ALPHA A/B/C/D/FIX combinations, PABE, FBA, COLCLAMP, DTHE;
- tests: ATE/ATST/AREF/AFAIL, DATE/DATM, ZTE/ZTST, ZBUF psm and ZMSK,
  FRAME psm and FBMSK, SCISSOR extents, SCANMSK;
- fog usage;
- transfers: direction, psm, sizes, count per frame; whether local→host
  or `ReadVram` ever happens during a frame (this decides how much
  readback the GPU design must support);
- presentation: PMODE circuits, SMODE2 interlace/field, DISPFB formats.

Rare features (< 0.1% of primitives) get a "stub with a visible marker"
policy in the loop rather than an implementation, unless a reference
frame fails because of them.

## 4. Gate C — the game renders through the CPU backend

P1b (running) is climbing the boot ladder; the current stall is the guest
sleeping on an event that the IOP/HLE side never signals. Until the CPU
backend produces real frames there is nothing to capture, and the
harness work in Gates A and B proceeds on synthetic streams only. This
gate is owned by the P-series briefs, not by this plan.

## 5. Decisions made before the loop starts (not delegated)

1. **Rendering model: exact compute rasterizer over a VRAM buffer first.**
   Keep the 4 MB VRAM as a GPU storage buffer that is the single source of
   truth; rasterize primitives in compute shaders that replicate the CPU
   backend's integer pipeline (fixed-point coordinates, swizzled page
   layouts per psm, blend and test math on integers). This is the
   "parallel-RDP" pattern that runs an exact N64 RDP on phones. It makes
   format aliasing, transfers and readbacks trivial, and lets the diff
   harness demand **byte-exact** results, which is what keeps an
   autonomous loop honest. The classic alternative (hardware raster into
   textures with page tracking, GSdx-style) is faster per pixel but its
   correctness tail is where PCSX2 spent years; it is the fallback only if
   the compute design misses the performance target after profiling, and
   then only for the hot primitive classes the census identifies.
2. **API: Vulkan 1.1 compute, MoltenVK on the Mac.** One code path for the
   mini and the Odin (Adreno 830, Vulkan 1.3). Metal is not written.
3. **Acceptance:** VRAM byte-exact after every transfer and every
   synthetic stream; rendered game frames byte-exact by stage 5, with an
   interim tolerance of ≤ 1% of pixels differing by > 4/255 per channel
   during stages 2–4. Performance: ≤ 4 ms per riding frame on the mini at
   native resolution (512×448), ≤ 6 ms on the Odin (half a 120 Hz budget
   with margin).
4. **Scope containment:** the backend and harness live in their own repo;
   upstream is a pinned submodule; the agent never edits the runtime, the
   interface, or the recompiler. If the interface must change, that is a
   stage gate for a person, filed as an upstream issue.
5. **License:** MIT for the backend and harness (GPL-compatible, easiest
   for upstream to vendor); any combined binary with the runtime is GPL-3.
   Upstream has no written contribution or AI-assistance policy; ask on
   its Discord before the upstream PR and disclose the assistance.

## 6. The staged loop (each stage is one muse brief with a diff table as its receipt)

| Stage | Deliverable | Pass |
| --- | --- | --- |
| S0 | Harness + census tools, identity test, synthetic set | CPU→CPU replay byte-exact; census runs on synthetic captures |
| S1 | Backend skeleton: Vulkan device, VRAM buffer, transfers (all directions, all psm), `ClearFramebuffer`, `Present` from DISPFB, `SnapshotVram`/`ReadVram`/`WriteVram` | transfer and clear synthetic streams byte-exact; presents of cleared frames match |
| S2 | Flat primitives: point, line, tri list/strip/fan, sprite; iip; scissor; xyoffset; FRAME psm/FBMSK; ZBUF with ZTE/ZTST/ZMSK; SCANMSK | flat synthetic set byte-exact |
| S3 | Textures: PSMCT32/24/16 direct, PSMT8/4(+H) with CLUT (csm1/csm2), STQ perspective and UV fixed, CLAMP modes, nearest/bilinear, TEXA, mip levels the census uses | textured synthetic set byte-exact |
| S4 | Blending and tests: ALPHA equations, PABE, FBA, COLCLAMP, DTHE/DIMX, ATE/AFAIL, DATE/DATM, fog | blend/test synthetic set byte-exact |
| S5 | Game reference frames | every capture byte-exact (or within the interim tolerance with a listed cause per frame) |
| S6 | Performance on the mini: batching by draw state, tile binning, profiling | ≤ 4 ms per riding frame, no correctness regression |
| S7 | Odin build: Android/Vulkan harness replay on device | all captures pass, ≤ 6 ms per riding frame |
| S8 | Live integration: the game running with the GPU backend | matches CPU backend on the reference frames live |

Rules for the loop: one feature per iteration taken from the census in
frequency order; every iteration ends with the full replay table; a red
row that was green is a stop; performance work never starts before S5;
the agent reports and stops at any stage gate for a human read.

## 7. Repository

Name: **`ps2xGS`** (follows upstream's component naming: `ps2xRuntime`,
`ps2xRecomp`, `ps2xIOP`, `ps2xTest`; if it is ever upstreamed it slots in
as the `ps2xGS` component). Layout:

```
ps2xGS/
  upstream/            git submodule: ran-j/PS2Recomp @ 14b1e5cb
  backend/             GSVulkanBackend : GSRasterBackend (+ shaders/)
  harness/             recording backend, gsreplay, gsdiff, synthetic generator
  census/              gscensus
  tests/               identity test, per-stage synthetic suites (CTest)
  captures/            .gitignore'd; lives on the mini or the SSD
  docs/                this plan (copied), census.md, stage reports
  CMakeLists.txt       builds libps2xgs, gsreplay, gscensus, tests
```

## 8. Prep briefs to run now (before the mini arrives)

- **G0 — harness and census on synthetic streams** (muse, host-only,
  Mac laptop, small): create the repo skeleton and submodule, the
  recording backend, `gsreplay` with the diff table, the synthetic
  generator for S1–S4 features, `gscensus`, and the identity test. Done
  when CPU→CPU replay of every synthetic capture is byte-exact and the
  census table renders. Estimated one to two days of muse.
- **G1 — reference captures and the real census** (muse, after Gate C):
  apply the capture hunk to the P1 runner, capture the reference set,
  run the census, publish `docs/census.md`. Half a day.
- **G2 — the loop brief** (written by the orchestrator once G0 and G1 are
  read): the S1–S5 runbook for the mini's sandboxed agent, with the
  decisions in §5 as rules.

## 9. Risks

- Upstream interface drift (one month old); mitigated by the pin, cost is
  a rebase per adoption.
- Present-path semantics (interlace, field mode, two circuits) are easy
  to get subtly wrong; the synthetic set includes them from S1.
- Compute rasterization cost on Adreno if primitive counts are high; the
  census gives the number early, and §5.1 names the fallback.
- MoltenVK compute limits (subgroup ops, 64-bit atomics) may constrain
  the tile design; check in S1 with a capability dump.
- Readback frequency: if the census shows per-frame local→host or
  `ReadVram` during rendering, the design keeps it cheap by construction,
  but it must be measured, not assumed.
