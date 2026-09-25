# ps2xGS

> **Dormant (2026-09-24).** This harness is no longer developed. GS capture/replay now lives in the
> PS2Recomp fork (`brad-richardson/PS2Recomp`, branch `ssx3`: `gs_replay_core`), and the GPU backend
> is paraLLEl-GS (`brad-richardson/parallel-gs`, branch `ssx3`). Kept for its history (G1–G22).

A GPU Graphics Synthesizer backend for
[PS2Recomp](https://github.com/ran-j/PS2Recomp) (`upstream/`, pinned at
`14b1e5cb`), plus the deterministic harness that an autonomous loop
needs to develop it: captured GS streams (`.gscap`), replay-and-diff,
a synthetic stream generator, and a feature census.

Plan: `docs/plan.md` (copy of the upstream plan). G0 report:
`docs/reports/G0.md`.

## Layout

- `upstream/` — pinned upstream submodule (read-only; never edited).
- `backend/` — `GSVulkanBackend` lands here in S1 (empty in G0).
- `harness/` — `GSRecordingBackend`, `gsreplay`, `gsgen`, `gsregs.h`,
  capture format doc (`docs/gscap-format.md`).
- `census/` — `gscensus`.
- `tests/` — CTest suites (register round trips, capture round trip,
  per-capture CPU identity replay, census expectations).
- `captures/` — local capture scratch (git-ignored; large files live
  under `/Volumes/Extreme SSD/ps2xgs/`).

## Build

Prerequisites: `cmake`, `ninja`, a C++20 compiler. Host-only, no
device, no Vulkan in G0.

```sh
cmake -S . -B /tmp/ps2xgs-build -G Ninja
cmake --build /tmp/ps2xgs-build
ctest --test-dir /tmp/ps2xgs-build --output-on-failure
```

## Commands (steps 3–6)

```sh
# 3. Recording is done in-process by GSRecordingBackend (see
#    docs/gscap-format.md); no CLI.
# 4. Replay a capture against the CPU backend and diff vs. reference:
gsreplay <capture.gscap> --backend cpu [--repeat N] [--threshold 4] \
    [--max-bad-pct 1.0] [--json out.json]
# 5. Generate the synthetic capture set:
gsgen <outdir>
# 6. Census a set of captures:
gscensus <capture...> --json census.json --md census.md
```

All three binaries live in the CMake build dir (`/tmp/ps2xgs-build`).
Captures and anything over 5 MB go under
`/Volumes/Extreme SSD/ps2xgs/`, never into this repo.

## License

GPL-3.0-or-later. This repository is licensed under the GNU General Public
License, version 3 (see `LICENSE`), matching upstream
[PS2Recomp](https://github.com/ran-j/PS2Recomp), whose runtime and headers
every build here compiles or links against. Choosing the same license
removes any question about combining the two. Copyright (c) 2026 Brad
Richardson.
