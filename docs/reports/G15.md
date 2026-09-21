# G15 report — instrumented on-device runs: O1 is flaky (1/6), O3 is scudo heap corruption, HWASan silent, writer unnamed — stop with recipe

Brief: G15 (this turn) — executes G14's §3: ONE instrumented bounded
on-device run to catch O1's null-write live (lldb watchpoint on the
faulting pool's `table` slot preferred, HWASan fallback), then the minimal
fix the watchpoint names + ONE clean re-run scored by `g14-diff.py`
against the G13 oracles. Tables + hypothesis + next-action
recommendation, no verdicts beyond the hypothesis. Time box 6 h (used
~3 h). Read first per the brief: `docs/reports/G14.md` (all) + G13 §3c
(oracle table) as needed. Init is PROVEN (G14) and was not re-litigated.

Machine: same as G8–G14 (Apple M4, macOS 27.0 — no new installs) + Odin3
(`622c49b1`, Android 15). Pins: paraLLEl-GS
`3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (G14-end state re-verified, §2a)
+ G14 S1–S3 shims (unchanged, no new source edits) / NDK r30
`30.0.16248370` (re-confirmed) / rich dump sha `154d9d85…` (re-verified on
host AND on-device) / G14 binary sha `c4cd63b4…` (reproduced EXACTLY by
deterministic relink after a pre-relink mismatch — §2a anomaly, tabled).

Headline result: hypothesis NOT SUPPORTED — the null-`table` write-site
was NOT named (outcome (b)/(c)-adjacent: table + recipe, stop — §3). The
instruments all ran but the writer stayed hidden: O1 reproduced only
1/6 meaningful launches (flaky — the lldb watchpoint never had a victim
to plant on: `table == 0` fired 0/5 debugger launches), a second trim
crash (O3, 2/6) was caught LIVE with a full 20-pool census (faulting
entry `f=0,i=2` TRANSFER, every app-side input sane) and died of PROVEN
heap corruption (`Scudo ERROR: corrupted chunk header at
0x200007ea68423b0`, bytes from the pool objects), MTE-sync naming was
BLOCKED by the kernel (`prctl` rc=-1, MTE off), and the HWASan fallback
build ran SILENT (164 `__hwasan` symbols prove instrumentation) and
crashed on O2 (fault 0xf0, 3rd instance — now the most-reproducing site
at 3/6). No minimal fix exists for an unnamed writer (the G14 guard
candidate stays UNTESTED — a guard would mask heap corruption, not fix
it), so per the stop rule there is NO clean re-run and NO renderer
change. Score: oracles 8/8 re-verified, device scanouts 0 files → SCORE
N/A. The ONE next action (§3): a mac-host ASan run of the same source —
names-or-eliminates an app-side writer with zero device cost.

## 0. Byte caps (declared at session start) vs actuals

| class | cap | actual |
| --- | --- | --- |
| HWASan build dir (SSD, new `parallel-gs-g15-hwasan-build`) | 20 GB apparent | 4,816,896 KiB allocated (ExFAT 1 MiB clusters); binary 289,108,176 B; apparent ≤ allocated ≪ cap ✓ |
| SSD ps2x-g15 retrieval (tombstones + logcats + lldb log + scripts) | 100 MB | ~1.3 MB apparent; 31,744 KiB allocated ✓ |
| SSD ps2x-g13 (oracles, read-only) | 0 growth | 35,840 KiB == G13/G14 exactly ✓ |
| SSD ps2x-g14 | 0 growth | 7,168 KiB == G14 exactly ✓ |
| SSD G14 build dir (relink only) | 0 + relink | 4,480,000 KiB == G14 exactly; binary same size, sha now `c4cd63b4…` (§2a) ✓ |
| SSD mac build dir | 0 growth | 3,808,256 KiB == G14 exactly ✓ |
| SSD clone (source) | 0 + nothing | pin + shims + hooks all == G14-end; no new edits ✓ |
| internal volume (`/`) | ≤1 GB delta, no clones/builds | 8.8 Gi avail at start → 4.4 Gi at end; G15 net ≈ 0 (`/tmp/g15-*` residue ~100 KB; 266 MB symbol copy removed same session); remainder other-lane (T/I/E lanes active — same honest accounting as G13/G14 §0) |
| device `/data/local/tmp/g15/` (transient) | 600 MB peak | peak ~577 MB (266 + 289 + 11.5 + 12.8 + runtimes); 0 remaining — dir removed, `mg/` untouched ✓ |
| `/tmp/g15-*` host scripts/logs + symbol copy | 600 MB peak, session-only | peak ~270 MB (symbol copy, removed); residue ~100 KB scripts/logs ✓ |
| committed to git | text only (report + scripts) | this file (ps2xGS) + REPORT.md + 6 scripts (ssx3 mirror); no captures, no binaries, no build dirs ✓ |

SSD 396 → 327 Gi avail: this brief wrote ~4.8 GB (hwasan build +
retrieval); the remainder is other-lane. No code copied into any GPL
tree. No P-lane contention: no recomp boots/builds, no P-lane lease (no
fork writes at all), no bytesize/WSL use. Host builds `-j2` (one
`parallel-gs-replayer` target per build). Device tombstones `tombstone_07`
through `_11` (ours, §2e) remain in OS-owned `/data/tombstones/`
(auto-rotated store, not removable as shell — tabled). adb forward
`tcp:5041` created for the debug session, removed at end (list empty ✓).

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | The instrumented run names the exact write-site of the null `table` slot |
| observable signal | watchpoint / sanitizer backtrace + fixed-run exit + pulled scanouts + per-pair diff table vs G13 oracles |
| alternatives | (a) write-site named + minimal fix + clean re-run scored → table + ONE next action (adoption input); (b) write-site named but fix is NOT minimal (framework corruption, driver bug) → table + recipe, stop; (c) instrumentation blocked → table the exact obstacle + recipe, stop |
| stop condition | TWO bounded device runs max (instrumented + verification) — no tuning loop, no renderer changes, no new dumps |
| outcome → next action | numbers name the next single experiment (§3) |

Outcome: hypothesis NOT SUPPORTED — a fourth shape the brief did not
enumerate: every instrument RAN (nothing blocked outright) yet the
write-site stayed unnamed, while a MECHANISM (heap corruption) was
proven. Verdict recorded as (b)/(c)-adjacent: corruption proven + no
minimal fix exists → table + recipe, stop. No tuning loop was entered:
no source fix, no second instrumented variant after HWASan, no clean
re-run (nothing to verify).

## 2. Task 1 — instrument choice + instrumented session + HWASan run

### 2a. Reuse verification + the relink anomaly (pre-work)

| item | observed |
| --- | --- |
| SSD clone HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (== G14 pin) |
| tree status | `M CMakeLists.txt` (S1) + `m Granite` (S2) + `M tools/CMakeLists.txt` (S3) + `M gs/gs_interface.cpp` (G11) + `M tools/gs_dump_replayer.cpp` (G8+G10) + ExFAT `._` sidecars — G14-end state exactly |
| NDK | r30 `30.0.16248370` re-confirmed; ships host `lldb` 21.0.0 AND device `lldb-server` 21.0.0 (aarch64, 12.8 MB, statically linked) at `toolchains/llvm/prebuilt/darwin-x86_64/lib/clang/21/lib/linux/aarch64/lldb-server` |
| host debugger | Apple `/usr/bin/lldb` (lldb-2103) — NDK lldb bus-errors on `target create` (crash report `lldb-2026-09-21-020042.ips`); Apple lldb opens the ELF, resolves `CommandPool::trim` to `command_pool.cpp:168` + slide, python works |
| rich dump (host) | 11,537,377 B, sha `154d9d85…` full-match (control-hashed 2×, stable) |
| G14 binary (pre-relink) | 265,837,472 B, mtime Sep 21 01:34 (G14's), but sha `9138c50b…` (4 stable reads: 3× sha256 + 2× md5 agree) ≠ G14 receipt `c4cd63b4…`, plus ONE transient `5bc93b85…` read — cause UNDETERMINED (no writer observed, mtime preserved, dump control clean); tabled, not claimed |
| G14 binary (post-relink) | `rm` + `cmake --build --target parallel-gs-replayer -j2` from the SAME `.o` files → same size, sha `c4cd63b4859ee72480abce6790904f41aa95374eaec3bd39395bb94a42d79db7` — G14's receipt EXACTLY (3 stable reads). Build is deterministic; G14 binary effectively reuses with a fresh receipt |
| device | `622c49b1`, Odin3, Android 15; `/data` 28 G free; `/data/local/tmp/` == `mg/` only at start |
| pool census (static) | 4 frames (`init_frame_contexts(4)`) × QUEUE_INDEX_COUNT=5 (GRAPHICS, COMPUTE, TRANSFER, VIDEO_DECODE, VIDEO_ENCODE) × 1 thread-index (`set_num_thread_indices(1)`) = 20 `CommandPool`s; `set_context` ALSO builds 2 old frames first (`device.cpp:975`) — trimmed at `main:92` entry `wait_idle_nolock` |

### 2b. Instrument choice (tabled with forcing receipts)

| instrument | verdict | forcing receipt |
| --- | --- | --- |
| lldb watchpoint (preferred) | VIABLE → executed (§2c–§2e) | NDK `lldb-server` runs on device (`v` → 21.0.0); unstripped binary in hand (sha-verified both ends); Apple-lldb + `lldb-server gdbserver -- prog` entry-stop flow proven (inferior in `ptrace_stop`, logcat silent) |
| MTE-sync wrapper (naming upgrade, §2f) | BLOCKED | `tagged_addr_ctrl=0x1` (MTE off) baseline; `prctl(PR_SET_TAGGED_ADDR_CTRL, SYNC)` rc=**-1** (kernel refuses); no `memtag` global knob visible to shell |
| HWASan rebuild (brief's fallback) | VIABLE → executed (§2f–§2g) | static-libc++ link fails (`R_AARCH64_ADR_PREL_PG_HI21 out of range` — .text is only 13 MB, so NOT a size issue); `-DANDROID_STL=c++_shared` + `-mllvm -hwasan-globals=0` links clean ([452/452], 289 MB, sha `b93bb69a…`, 164 `__hwasan` symbols) |

lldb friction tabled (all worked around, none blocking): `target create
--remote-file` fails on BOTH lldbs (`unable to open target file` — the
flag, not the path); NDK lldb bus-errors (above); ExFAT path with spaces
breaks `target create` (266 MB `/tmp` symbol copy, sha-verified,
removed after); breakpoint conditions with spaces need quoting;
`__builtin_debugtrap` in `LOGE` re-traps on `continue` (pc+4 skip in
`triage`); `quit` DETACHES (inferior runs free — `process kill` added;
two SIGTRAP-detach tombstones `_09/_10` are method noise, §2e).

### 2c. Instrumented session: launch census (ONE bounded activity)

Same G14 binary bytes (`c4cd63b4…`, sha-verified on-device), same dump,
same args — every launch classified:

| launch | pid | mode | path reached | outcome |
| --- | --- | --- | --- | --- |
| G14 (prior) | 19724 | free | init trim (NEW pools, `set_super_sampling_rate`) | **O1**: SIGSEGV 0x380, null `table` @ `trim():171` |
| stray (stub-port collision accident) | 27908 | free | first draw (`Running frame` + G11/G10 hooks) | **O2**: SIGSEGV 0xf0 in Adreno `vkUpdateDescriptorSetWithTemplate` ← `flush_descriptor_set():3505` (tombstone_07) |
| detached-at-entry (quit detached) | 26199 | ≈free | first draw | **O2 again** (tombstone_08, same frames) |
| 26569 / 27062 | — | debug→detach | renderdoc `LOGE` trap only | SIGTRAP-at-`debug_break` artifacts (tombstones _09/_10) — method noise, not app signal |
| debug | 27843 | lldb (cond-bp `table==0`, never fired) | `main:92` trim (**OLD** pools, `init_frame_contexts` entry) | **O3**: SIGABRT (silent — no scudo line) in trim @ `:173` via driver frames |
| debug | 29193 | lldb (cond-bp, never fired) | init trim (**NEW** pools, `set_super_sampling_rate`) | **O3 + mechanism**: SIGABRT @ `:173`, `f=0,i=2` TRANSFER — `Scudo ERROR: corrupted chunk header at 0x200007ea68423b0` ~150 ms before the abort (pool objects live at `0x…a6842xxx–0x…a6844xxx`) |
| HWASan (§2g) | 7343 | free (hwasan) | first draw | **O2** (tombstone_11), sanitizer SILENT |

Rates over the 6 crash launches: O2 **3/6**, O3 2/6, O1 **1/6**.
O1's victim `(f,i)` stays unknown (G14 had no debugger). The conditional
trim breakpoint evaluated every trim in 5 debugger launches and fired
**zero** times — there was never a null-`table` victim to plant the
watchpoint on, so NO watch was ever planted and NO fix was attempted.

### 2d. O3 live capture (pid 29193 — the session's crown receipt)

Full 20-pool census at the crash (DWARF-only walk, no expr compile;
`pool_size=80` as laid out; `table=0x7fffffb910` EVERYWHERE — valid,
points into main's stack-resident volk table via `Context`):

- frames=4, pools=20; handles live for `i=0,1,2`
  (GRAPHICS/COMPUTE/TRANSFER), `0x0` for `i=3,4` (no video queues →
  `VK_QUEUE_FAMILY_IGNORED` → pool never created — BY DESIGN, and
  `trim()`'s `pool == NULL` guard covers exactly that).
- Faulting pool: **`f=0,i=2`** (TRANSFER,
  `pool=0xb400007ea68433d0`, third pool in trim order — `i=0,1` trimmed
  clean first): `table` live, `handle` live,
  `buffers n=3` (3 plausible tagged handles, cap 32),
  `secondary n=0`, `index=0`.
- Crash: SIGABRT inside the driver (`0x7d58…` frames) called from
  `trim():173` (`vkFreeCommandBuffers`, buffers branch) — every app-side
  input SANE. The scudo line proves the abort's cause: a heap-chunk
  header near the pool objects was already smashed by an earlier wild
  write; the driver's allocation op inside the trim call tripped over it.

Consequence: O1 (nulled slot), O2 (driver null-deref on first draw) and
O3 (scudo abort in trim) are now best read as ONE heap-corruption bug
with layout-dependent victims — not three bugs. The writer is
uninstrumented-or-absent in every capture (see HWASan silence, §2g).

### 2e. Device-side receipts pulled

`g15-tombstone-07.txt` (O2 stray), `-08` (O2 detached), `-09/-10`
(SIGTRAP artifacts), `-11-hwasan.txt` (O2 hwasan), `g15-logcat-stray.txt`
(32 lines), `g15-logcat-debug-runs.txt` (27843+29193, incl. the scudo
line), `g15-logcat-hwasan.txt`, `g15-lldb-o3-live.txt` (full 29193
capture), `g15-hwasan-run-stdout.txt`, `g15-stub.log`. Device dir
removed after retrieval; `/data/local/tmp/` == `mg/` only ✓.

### 2f. HWASan build (second build dir — G14 dir untouched)

Configure: G14 recipe + `-DANDROID_STL=c++_shared` +
`-fsanitize=hwaddress -fno-omit-frame-pointer -mllvm
-hwasan-globals=0` (compile) + `-fsanitize=hwaddress` (link) → exit 0
(~27 s). First link attempt (static STL) failed as tabled in §2b;
shared-STL relink → exit 0, `tools/parallel-gs-replayer` 289,108,176 B,
sha `b93bb69af652152249a8b8fac95230eb4366d1cf2209a2e98774c3d20c140ef6`,
NEEDED `libclang_rt.hwasan-aarch64-android.so + libc++_shared.so` +
system libs. (Nuance: `.note.hwasan.globals` marker section is still
emitted (8 B) despite `-hwasan-globals=0` — the flag's exact effect is
reported as-configured, not independently verified; heap checks are
unaffected either way.)

### 2g. HWASan run (THE bounded instrumented run #2 — silent O2)

Push + on-device sha match (binary `b93bb69a…`, both `.so` match host
exactly); logcat before = 0 lines. Run:
`LD_LIBRARY_PATH=$G15DIR timeout -s KILL 280 ./hwasan-replayer
…/g13-dump.gs --iterations 2` → **exit 139, <1 s wall**, stdout 3 lines
(epochs + `HW_EXIT=139`), **zero sanitizer output** on stderr/logcat
(HWASan replaces scudo, so no scudo line is expected; a tag violation
would still print — none did). Logcat: full init + slab pair +
`Running frame` + G11/G10 + `Stalled compile` (O2 territory, same shape
as the stray). Tombstone_11: SIGSEGV 0xf0, `vkUpdateDescriptorSetWithTemplate`
← `flush_descriptor_set` — O2's exact frames under a different BuildId.

Reading: the app ran init + first draw under full heap tagging with NO
violation, then died on a NULL deref inside UNINSTRUMENTED driver code
(null+offset faults are untagged by design — HWASan cannot see them).
Either the wild write's TRIGGER didn't fire under HWASan's layout, or
the writer is outside instrumentation (driver). Both readings agree on
the next step: the writer cannot be named from this device any further
without a host-side control (§3). 0 PPMs (died in first draw).

### 2h. Score vs G13 oracles (`g14-diff.py`, zero new tooling)

`g14-diff.py ps2x-g13 ps2x-g15`: oracles **8/8 OK** (all §3c pixel-shas
re-verified); device PPMs in `ps2x-g15`: **0** → `SCORE: N/A`. (The
script's hardcoded reason string still says "G14 run crashed" — vintage
wording; the scored fact is 0 scanouts.) No diff table is fabricable
and none is faked.

## 3. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| The instrumented run names the exact write-site of the null `table` slot | **NOT SUPPORTED**: 5 debugger launches (0 conditional-breakpoint fires) + 1 HWASan run (silent) + 2 free runs — the write-site was never captured. Separately SUPPORTED: heap corruption exists in/around the Granite pool path (scudo receipt); O1 is flaky (1/6); O2 is the modal crash (3/6, incl. under HWASan) |

The ONE next action the numbers justify: **one mac-host ASan
(+UBSan) run of the SAME source (G13's mac replayer recipe + sanitizer
flags) — no device, no caps risk.** Rationale: macOS runs this exact
code clean to exit 0 (G13: 18 frames) — if the wild write ALSO fires
there, host ASan names the writer (file/line + alloc/free traces) at
zero device cost and the fix + re-run unblock; if host ASan is clean,
the writer is Android-specific (Adreno driver or Android-only path) and
the project stops at (b) with a driver-bug recipe instead of burning
more device budget. Either outcome is decisive, which no further
device-side dice roll can be (victim variance + 1/6 O1 rate).
Queued behind it (not this action): an O2-argument debugger run (break
`flush_descriptor_set:3505`, identify which of `vk_set /
update_template / bindings` is bad); any renderer change (forbidden
until a writer is named).

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.

## 4. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c19…` (LGPL-3.0+ per G7 §4 — cited, not re-read) |
| Granite rev | `16e7395f…` (MIT per G7 §4 — cited); crash/census sites are Granite framework code |
| G14 shims S1–S3 | untouched, still uncommitted in SSD clone only |
| G15 additions | ZERO clone edits — instrumentation is session files only: `g15walk.py` + `g15-a.lldb` (lldb), `g15-mtewrap.c` + `g15-mteprobe.c` (G15-original, trivial prctl probes), hwasan build/run scripts (text, mirrored) |
| NDK r30 | `30.0.16248370` (Apache-2.0 toolchain); lldb-server + hwasan runtime + libc++_shared are NDK-shipped, pushed to the transient device dir only, removed after |
| tombstones/logcats | OS-produced run receipts of our own binaries (no PII; `uid: 2000` shell) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 5. Exact commands

Mac (`/Users/bradrichardson/dev/ps2xGS` unless noted;
`COPYFILE_DISABLE=1` on SSD steps; `NDKBIN=…/toolchains/llvm/prebuilt/darwin-x86_64/bin`):

```text
git -C "<ssd>/parallel-gs-g7" rev-parse HEAD ; status --short   # §2a
shasum -a 256 <g14-binary> (x5: 9138c50b x4 + 5bc93b85 x1) ; md5 (x2)  # anomaly
cmake --build <g14-build> --target parallel-gs-replayer -j2     # relink -> c4cd63b4
shasum -a 256 <g14-binary> (x3 post-relink, stable c4cd63b4)
file <ndk>/lib/clang/21/lib/linux/aarch64/lldb-server ; lldb.sh --version  # choice
lldb -b -s /tmp/g15-*.lldb                                   # instrumented session (Apple lldb)
"$NDKBIN/llvm-addr2line" -e <g14-binary> -f -C -i 0x7fc9b4 ...  # O2 line map (:3505)
"$NDKBIN/llvm-nm" <hwasan-binary> | grep -c __hwasan          # 164 (instrumented)
"$NDKBIN/llvm-readelf" --dynamic/--notes <binaries>           # NEEDED + notes
cmake -S <clone> -B <hwasan-build> ... (see g15-build-hwasan.sh)  # §2f (x2: static fail, shared ok)
cmake --build <hwasan-build> --target parallel-gs-replayer -j2   # exit 0
python3 <ssx3>/G14/g14-diff.py <ps2x-g13> <ps2x-g15>           # §2h (8/8 OK, 0 device, N/A)
du -sk <ssd dirs> ; df -h / "<ssd>"                              # §0
```

adb (`-s 622c49b1` throughout; `/data/local/tmp/g15/` ONLY; `mg/` never touched):

```text
shell 'rm -rf /data/local/tmp/g15 && mkdir -p /data/local/tmp/g15'
push <dump> $G15DIR/g13-dump.gs ; push <g14-binary> $G15DIR/parallel-gs-replayer
push <ndk-lldb-server> $G15DIR/lldb-server
shell 'sha256sum g13-dump.gs parallel-gs-replayer lldb-server'  # all match host
shell './lldb-server v'                                          # 21.0.0
forward tcp:5041 tcp:5041
shell 'cd $G15DIR && ./lldb-server gdbserver "*:5041" -- ./parallel-gs-replayer $G15DIR/g13-dump.gs --iterations 2'  # per launch (bg)
logcat -d -s Granite:V                                           # per launch (before/after)
shell './mteprobe; ./mtewrap ./mteprobe'                         # 0x1 ; prctl rc=-1 (BLOCKED)
push hwasan-replayer + libc++_shared.so + libclang_rt.hwasan-*.so ; sha256sum  # match
shell 'cd $G15DIR && date +%s; LD_LIBRARY_PATH=$G15DIR timeout -s KILL 280 ./hwasan-replayer $G15DIR/g13-dump.gs --iterations 2; echo HW_EXIT=$?; date +%s'  # THE hwasan run: 139
pull /data/tombstones/tombstone_07..11 <ssd>/ps2x-g15/           # (11 = hwasan O2)
shell 'rm -rf /data/local/tmp/g15 && ls /data/local/tmp/'        # DEVICE_CLEAN (mg/ only)
forward --remove tcp:5041
```

## 6. Gaps (what this brief could not do)

1. O1's null-write site is UNNAMED (0 conditional-breakpoint fires in 5
   debugger launches; O1 rate 1/6). The §3 host-ASan run is the
   prescribed naming attempt.
2. O1's victim `(f,i)` is unknown (G14 predates the debugger).
3. O2's bad argument (`vk_set` vs `update_template` vs `bindings`) is
   unknown — needs the queued O2-argument run.
4. 27843's silent SIGABRT mechanism is unknown (no scudo line, unlike
   29193) — same trim site/shape, possibly same cause, not proven.
5. The pre-relink `9138c50b` bytes (+1 transient `5bc93b85` read) are
   unexplained — no writer observed; resolved by deterministic relink,
   not root-caused.
6. Zero on-device scanouts (again) → no pixel score. `g14-diff.py`
   stands ready (8/8 oracle re-verify passes through it).
7. No fix, no clean re-run (correctly: nothing to verify).
8. `tombstone_07..11` remain in OS-owned `/data/tombstones/` (shell
   cannot remove; auto-rotated).
9. No upstream contact (nothing filed).
10. `upstream/` and ps2xGS harness code untouched; no new dumps (per the
    stop rule). Detached-quit lesson: future lldb batches must
    `process kill` before `quit` (done here from batch v3 on).

## 7. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…`
  (G7/G8/G10/G11 hunks + G14 S1–S3 — all uncommitted, untouched by G15).
- G14 build: `/Volumes/Extreme SSD/parallel-gs-g14-android-build/`,
  `tools/parallel-gs-replayer` (265,837,472 B, `c4cd63b4…`, relinked).
- HWASan build: `/Volumes/Extreme SSD/parallel-gs-g15-hwasan-build/`,
  `tools/parallel-gs-replayer` (289,108,176 B, `b93bb69a…`).
- SSD: `/Volumes/Extreme SSD/ps2x-g15/` (5 tombstones + 4 logcats + lldb
  O3 capture + hwasan stdout + stub log + session scripts);
  `ps2x-g13/`/`ps2x-g14/` pristine.
- Tools: `/tmp/g15walk.py`, `/tmp/g15-a.lldb`, `/tmp/g15-mtewrap.c`,
  `/tmp/g15-mteprobe.c`, `/tmp/g15-build-hwasan.sh`,
  `/tmp/g15-run-hwasan.sh`, `/tmp/g15-lldb-a.log`,
  `/tmp/g15-hwasan-build.log`, `/tmp/g15-hwasan-run.txt`
  (scripts mirrored; logs session-only; 266 MB symbol copy removed).
- Commits: ps2xGS `[G15]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G15/` `[G15]` + same trailer (NOT pushed).
