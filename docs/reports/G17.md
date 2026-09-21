# G17 report — O2-argument debugger run NAMES the bad input: `update_template == nullptr` (set 0, `vk_set` valid) at `flush_descriptor_set():3505`, fault `0xf0` at driver `ldr w8, [x2, #0xf0]` — stop with fix-brief input

Brief: G17 (this turn) — executes G16's §4: ONE bounded on-device lldb
run breaking at `flush_descriptor_set():3505` (the O2 site) to name
WHICH of `vk_set / update_template / bindings` is bad, capture the
descriptor-set state, and package the Adreno repro. Tables + hypothesis
+ next-action recommendation, no verdicts beyond the hypothesis. Time
box 6 h (used ~1.2 h). Read first per the brief: `local/research/G16/
REPORT.md` (all: outcome (b) CLEAN-RUN ELIMINATION — ASan+UBSan exit 0,
18/18 frames, 0 sanitizer lines, 7/7 pixel-identical; writer
Android-specific) + G15 §§2c–2e (O2's frames: SIGSEGV fault 0xf0 in
Adreno `vkUpdateDescriptorSetWithTemplate` ← `:3505`, modal 3/6 incl.
under HWASan). No renderer changes, no new dumps (kept).

Machine: same as G8–G16 (Apple M4, macOS 27.0 — no new installs) +
Odin3 (`622c49b1`, Android 15). Pins: paraLLEl-GS
`3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` + G14 S1–S3 shims + G11/G8+G10
hunks (G16 §2a reproduced EXACTLY, §2a) / G14 binary `c4cd63b4…` (same
bytes that produce O2 — reused, NOT rebuilt) / rich dump sha `154d9d85…`
(re-verified host-side AND on-device) / NDK r30 lldb-server + Apple-lldb
entry-stop flow (G15 §2b friction — `process kill` before `quit`, pc+4
`debug_break` skip).

Headline result: hypothesis SUPPORTED — the bad input is NAMED with
captured values. Run 3 (the one retry; run 2 drew O3 in the victim
lottery) caught O2 live under the debugger: the `:3505` conditional
breakpoint FIRED, and the DWARF frame variables at the stop read
`set = 0`, `vk_set = 0xb400007ef64c89d0` (VALID), **`update_template =
nullptr`** (THE BAD INPUT), `first_set = 0`, `set_count = 0`. On
`continue` the inferior died with SIGSEGV fault `0xf0` at the driver's
`ldr w8, [x2, #0xf0]` with crash-time `x1 == vk_set` (exact match) and
`x2 == 0` — the null template dereferenced at offset `0xf0`. Device,
`vk_set`, and `pData` are all valid; ONLY the template is null. The
`:3504 VK_ASSERT(update_template)` compiled out (`VULKAN_DEBUG`-only
macro), and no "Failed to create descriptor update template" appears in
ANY logcat (G17 + G15) — creation never LOGE'd failure. Because the app
passed a NULL handle where Vulkan requires a valid one (Granite's own
`:3504` assert states the contract), the ONE next action (§3) is a
minimal-fix brief, NOT a driver filing first: the driver-filing facts
are assembled (§2f) as the queued fallback. Score: oracles ALL OK,
device scanouts 0 → SCORE N/A. Launch census: r1 harness-invalid (my
trap-skip mask bug, fixed in-session — no launch), r2 O3, r3 O2 (4th
instance; 4/4 launches reaching first draw die at O2).

## 0. Byte caps (declared at session start) vs actuals

| class | cap | actual |
| --- | --- | --- |
| SSD ps2x-g17 retrieval (logcat + 3 lldb logs + 4 scripts) | 100 MB | ~74 KB apparent; 17,408 KiB allocated (ExFAT 1 MiB clusters) ✓ |
| SSD ps2x-g13 / g14 / g15 / g16 (read-only) | 0 growth | 35,840 / 7,168 / 31,744 / 43,008 KiB == G16-end exactly ✓ |
| SSD mac / G14 / HWASan / ASan build dirs (read-only) | 0 growth | 3,808,256 / 4,480,000 / 4,816,896 / 4,577,280 KiB == G16-end exactly ✓ |
| SSD clone (source) | 0 + nothing | HEAD + status + shims + hooks all == G16 §2a; ZERO clone edits ✓ |
| internal volume (`/`) | ≤1 GB delta, no clones/builds | 1.3 Gi avail at start → 3.9 Gi at end (other lanes freed); G16→G17 net ≈ 0 (`/tmp/g17*` residue 92 KB; 266 MB symbol copy removed same session — same honest accounting as G13–G16 §0) |
| device `/data/local/tmp/g17/` (transient) | 600 MB peak | peak ~290 MB (266 + 11.5 + 12.8); 0 remaining — dir removed, `mg/` only ✓ |
| `/tmp/g17-*` host scripts/logs + symbol copy | 600 MB peak, session-only | peak ~266 MB (symbol copy, removed); residue 92 KB scripts/logs ✓ |
| committed to git | text only (report + scripts) | this file (ps2xGS) + REPORT.md + 4 scripts (ssx3 mirror); no captures, no binaries, no build dirs ✓ |

SSD 296 → 288 Gi avail: this brief wrote ~17 MB allocated
(retrieval); the remainder is other-lane. No code copied into any GPL
tree. No P-lane contention: no recomp boots/builds, no P-lane lease (no
fork writes at all), no bytesize/WSL use. No host builds at all (G14
binary reused). Device tombstones: NONE new (dir still maxes at
`tombstone_31` — debugger-held crashes leave none, same as G15's debug
O3s; tabled, not pulled because absent). adb forward `tcp:5041` created
for the debug sessions, removed at end (list empty ✓).

## 1. Experiment contract

| slot | content |
| --- | --- |
| hypothesis | The O2-argument debugger run names the bad descriptor input at `flush_descriptor_set():3505` (`vk_set` vs `update_template` vs `bindings`) with captured values |
| observable signal | `:3505` breakpoint stop (or crash post-mortem): `set` / `vk_set` / `update_template` / `pData` values + null/wild naming + backtrace + call registers + launch census + pulled logcats/lldb logs + diff score |
| alternatives | (a) bad input named → table values + ONE next action (fix-brief input or filing package); (b) lottery misses O2 twice → table exact census + the next naming attempt the numbers justify |
| stop condition | TWO bounded device runs max (instrumented + ONE retry if O1/O3 fires) — no tuning loop, no renderer changes, no new dumps, no source fix |
| outcome → next action | numbers name the next single experiment (§3) |

Outcome: alternative (a) — NAMED on the retry (`update_template =
nullptr`, set 0, everything else valid). No tuning loop was entered: no
source fix, no renderer change, no second instrument after the naming.

Run accounting (honest): THREE stub sessions, TWO classifiable
launches. r1 yielded NO launch (my `debug_break` trap-skip mask bug
re-trapped all 10 stops before app code past init; fixed with a one-line
session-script change — §2c, not an app tuning loop) and is tabled as
harness-invalid method noise. r2 = the instrumented run (drew O3); r3 =
the ONE retry the brief prescribes when O1/O3 fires (caught O2).

## 2. Task 1 — pin verify + O2-argument debugger runs

### 2a. Pin verification (pre-work — G16 §2a reproduced exactly)

| item | observed |
| --- | --- |
| SSD clone HEAD | `3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd` (== pin) |
| tree status | `M CMakeLists.txt` (S1) + `m Granite` (S2) + `M tools/CMakeLists.txt` (S3) + `M gs/gs_interface.cpp` (G11) + `M tools/gs_dump_replayer.cpp` (G8+G10) + ExFAT `._` sidecars — G16 §2a exactly |
| shim markers | `G14 local experiment shim` ×1 in `CMakeLists.txt`, `Granite/application/platforms/CMakeLists.txt`, `tools/CMakeLists.txt` (S1–S3); `G11` ×10; `G8/G10` ×11; `__APPLE__` ×1 in Granite `util/timer.cpp` (G7) |
| Granite rev | `16e7395f6a48` (== pin) |
| rich dump (host AND device) | 11,537,377 B, sha `154d9d8577a210fb794b29a048ee2cf08053f87fe220bb2bf975933a02ad7e32` full-match both ends |
| G14 binary (host AND device AND /tmp symbol copy) | 265,837,472 B, sha `c4cd63b4859ee72480abce6790904f41aa95374eaec3bd39395bb94a42d79db7` full-match (reuse, no rebuild) |
| lldb-server (host AND device) | 12,833,264 B, sha `015cfc93a6ad3af4ba472ff09a45382fc9bb55e5401411f51a830ec460d5dd09` full-match |
| NDK | r30 `30.0.16248370` re-confirmed (`/opt/homebrew/share/android-ndk`, clang 21) |
| mac binary / ASan binary | `935dab2037f45355…` / `ae404762408ee09a…` (== G16 receipts) |
| HWASan binary | **ANOMALY (tabled, unused):** `e2655baed6c9c782…` (2 sha reads + md5 agree; size 289,108,176, mtime Sep 21 04:17 = G15's) ≠ G15 receipt `b93bb69a…` — ExFAT phantom-read shaped (cf. G15 §2a pre-relink); G17 never uses this binary; NO action taken (no rebuilds allowed) |
| device | `622c49b1`, Odin3, Android 15; `/data` 28 G free; `/data/local/tmp/` == `mg/` only at start; tombstones max `_31` at start |
| driver (logcat) | `Adreno (TM) 830`, API 1.3.284, Driver 512.800.58 (== G14) |
| driver (OS oracle `cmd gpu vkjson`) | driverID 8, Adreno Vulkan Driver, build `a9ee82cd83`, 10/08/25 (== G14) |
| macOS `timeout` | ABSENT (as G16) → lldb runs wrapped by `g17-run-lldb.py` (`subprocess.run(timeout=1500)` watchdog; tabled substitution) |

### 2b. Instrument (offline-proven before any device run)

| check | receipt |
| --- | --- |
| O2 line | `grep -n` → line 3505 IS `table.vkUpdateDescriptorSetWithTemplate(device->get_device(), vk_set, update_template, bindings.bindings[set]);` |
| build flags | G14 `CMakeCache.txt`: empty build type, empty C/CXX flags → `-O0`-shaped (DWARF locals expected) |
| breakpoint resolution (Apple lldb, no device) | `breakpoint set --file command_buffer.cpp --line 3505` → `flush_descriptor_set… + 312 … address 0x7fc97c` ✓ |
| DWARF locals (`llvm-dwarfdump --name`) | `vk_set` DIE `0x0031ba1e` (`WSP+24`), `update_template` DIE `0x0031ba2d` (`WSP+16`), typed `VkDescriptorSet` / `VkDescriptorUpdateTemplate` — condition-evaluable ✓ |
| condition design | `(vk_set == 0) \|\| (update_template == 0)` — fires ONLY on a null handle (never in a healthy run); false-hits auto-continue without consuming scripted stops |
| register-vs-locals caveat (predicted pre-run, confirmed §2e) | bp sits at line-start (+312), BEFORE call-arg setup → x0–x3 at the bp stop are NOT call args; DWARF frame variables are authoritative there; x0–x3 authoritative only at the crash stop |

Batch `g17-a.lldb`: entry-stop → cond-bp → 10× (`continue` +
`g17_triage`) → `process kill` → `quit` (G15's detach lesson kept).
`g17_triage`: LOGE-trap skip (pc+4) / `:3505` capture / crash
post-mortem (select `flush_descriptor_set` frame + capture) / exit
report. `bindings.bindings[set]` is `ResourceBinding[NUM]` (an in-object
array address — not itself null-checkable; its contents are probed via
the crash-time `pData`).

### 2c. Launch census (G15 §2c shape — every launch classified)

| launch | pid | mode | path reached | outcome |
| --- | --- | --- | --- | --- |
| r1 | 32339 | lldb (trap-skip mask BUG) | init slab lines only | **HARNESS-INVALID** — all 10 stops re-trapped at LOGE `debug_break` (`logging.hpp:154`, insn `0xd43e0000`); batch `process kill`; NO launch class; no tombstone. Cause: my insn mask `(insn & 0xffe0001f) in (0xd420001f, 0xd440001f)` required Rd=11111, but AArch64 BRK/HLT encode Rd=00000 (observed word masks to `0xd4200000`). Fix (session script only): gate the skip on the symbol alone — `if sym and "debug_break" in sym:` — plus mask broaden to `(insn & 0xffe00000) in (0xd4200000, 0xd4400000)` for the record |
| r2 | 325 | lldb (fixed batch) | init only (`main:92` OLD-pool trim) | **O3**: silent SIGABRT @ `trim():173` via driver frames ← `wait_idle_nolock` ← `init_frame_contexts` ← `main:92` (G15-27843 shape exactly); logcat 49-line init-only, NO scudo line; runner exit 1; no tombstone (debugger-held) |
| r3 | 827 | lldb (same batch — THE retry) | first draw (texture upload) | **O2 (4th instance)**: cond-bp FIRED at `:3505` → `update_template = nullptr` NAMED → SIGSEGV fault `0xf0` at driver `ldr w8, [x2, #0xf0]`; logcat `Running frame` + G11/G10 + `Stalled compile`; runner exit 1; no tombstone (debugger-held) |

Rates: O2 now 4/8 across G15+G17; O3 3/8; O1 1/8. Crisper
arithmetic (observed, not a verdict): **4/4 launches that reach first
draw die at O2** (G15 stray + detached + HWASan + G17 r3) — O2 is
deterministic once past init; the lottery is entirely whether O1/O3
fires in the init trims first.

### 2d. O3 live capture (r2 pid 325 — G15-27843's twin)

Stop at `0x7ff6ae0158` (libc `abort` path), SIGABRT, exit 6. lldb bt:
`trim(this=0xb400007eb6834850) at command_pool.cpp:173:3` ←
`PerFrame::trim_command_pools` ← `wait_idle_nolock(device.cpp:2608)` ←
`init_frame_contexts(device.cpp:2203)` ← `main(gs_dump_replayer.cpp:92)`.
No `:3505` stop preceded it (crash predates all draws). Logcat: Vulkan
init + extension list only; `scudo|corrupt|Running frame|Done|ERROR` =
0 lines. No census was taken (G17's batch captures descriptor state,
not pool state — G15's O3 census stands).

### 2e. O2 live capture (r3 pid 827 — THE naming)

Sequence: entry SIGSTOP → LOGE trap → TRAP-SKIPPED (fix works) →
`breakpoint 1.1` stop at `:3505` → capture → `continue` → SIGSEGV
fault `0xf0` → post-mortem → exit -1 (lost connection after fatal
signal; a benign thread-2 SIGCHLD from a driver compiler worker —
G14-noted `libllvm-qgl.so` — interleaves).

THE naming (DWARF frame variables AT the `:3505` stop — authoritative):

| input | value | verdict |
| --- | --- | --- |
| `set` | `0` | — |
| `vk_set` | `0xb400007ef64c89d0` | VALID (tagged pointer) |
| **`update_template`** | **`nullptr`** | **BAD — THE named input** |
| `first_set` / `set_count` | `0` / `0` | — |
| `device` (crash x0) | `0xb400007e46403730` | VALID |
| `pData` = `bindings.bindings[0]` (crash x3) | `0xb400007d44878ac8` | VALID (tagged; contents live) |

Crash mechanism (live, twice-identical `register read` at frame #0):

| reg | value | meaning |
| --- | --- | --- |
| x0 | `0xb400007e46403730` | device (valid) |
| x1 | `0xb400007ef64c89d0` | vk_set — EXACT match to the DWARF `vk_set` ✓ |
| x2 | `0x0` | update_template — null confirmed at the crash too ✓ |
| x3 | `0xb400007d44878ac8` | pData (valid) |
| fault | `0xf0` at `ldr w8, [x2, #0xf0]` (insn `0xb940f048`) | driver dereferences null-template + `0xf0` |
| lr | `flush_descriptor_set + 372 at command_buffer.cpp:3505:2` | return address = the call site (tombstone_07 said +368; same call, one-insn attribution delta) |

Cross-checks: call path `dispatch_indirect ← upload_texture ←
flush_cache_upload ← flush(FBPointer) ← … ← a_d_XYZ2 ←
write_register(XYZ2)` == tombstone_07's frames; register signature ==
tombstone_07 (`x2/x4/x6/x7/x10 = 0`, `x11 = 0xa00`, `x13 = 1`, `x14 =
2`); fault `0xf0` == all prior O2s. Same crash, now with the input
named.

Static facts sharpening the fix brief (read-only source reads, no
edits): `VK_ASSERT` is `#ifdef VULKAN_DEBUG` else `((void)0)`
(`vulkan_headers.hpp:50-60`) — the `:3504` assert compiled OUT, so the
null sailed to the driver. `update_template[set]` is zero-init
(`shader.hpp:259`), created in `create_update_templates()`
(`shader.cpp:743`, failure only LOGE'd) — but the loop SKIPS sets ∉
`layout.descriptor_set_mask` (`shader.cpp:551-554`), and NO "Failed to
create descriptor update template" appears in ANY logcat (G17 all +
G15 stray/debug/hwasan: 0 lines each) → creation never LOGE'd failure.
Why `update_template[0]` is null (mask-skip vs heap/init path vs
runtime-nulled slot) is the fix brief's static question — NOT resolved
here (no verdict beyond the hypothesis).

Script warts (tabled, naming unaffected): (1) r1 mask bug (§2c). (2)
Result-flush ordering: `print(file=result)` lines flush at python-command
return, AFTER `HandleCommand` outputs — each triage's TRIAGE line
follows its bt/register block; content complete, order shifted. (3)
Post-mortem frame search false-matched `for_each_bit` (#2 — its symbol
contains both match substrings) because lldb's crash-bt attributes the
return address to the caller frame (`:3626`) and elides
`flush_descriptor_set`; hence post-mortem `G17-ARG` zeros
(`FindRegister` on a non-zero frame) and the `frame variable set_count`
undeclared-identifier error — all DISREGARDED; crash-time `register
read` (frame #0) and the `:3505` DWARF stop are the valid readings.
(4) Bp-stop regs (x0–x2 = 0, stale x3) are pre-arg-setup as predicted
(§2b) — disregarded by design.

### 2f. Driver-filing facts (assembled as briefed — queued fallback, §3)

| slot | fact |
| --- | --- |
| device | Odin3 (`622c49b1`), Android 15, `qti/sun/sun:15/AQ3A.250728.001/eng.Odin3.20260204.171202` |
| driver | Adreno (TM) 830, API 1.3.284, Driver 512.800.58; vkjson driverID 8, build `a9ee82cd83`, 10/08/25 |
| API call | `vkUpdateDescriptorSetWithTemplate(device, vk_set, update_template, pData)` from Granite `flush_descriptor_set(command_buffer.cpp:3505)` |
| bad input | `update_template = NULL` (3rd arg, x2 = 0); device / `vk_set` / `pData` valid (values §2e) |
| driver behavior | SIGSEGV fault `0xf0` at `ldr w8, [x2, #0xf0]` (`vulkan.adreno.so`, `qglinternal::vkUpdateDescriptorSetWithTemplate+4`) — null+offset deref, no validation error |
| repro | G14 binary `c4cd63b4…` (265,837,472 B) + rich dump `154d9d85…` (11,537,377 B) + args `<dump> --iterations 2`; trigger = first-draw texture-upload compute dispatch, set 0; deterministic once past the init-trim lottery (4/4) |
| caveat (why this is the fallback) | the app passed a NULL handle where the spec requires valid (Granite's own `:3504` assert states the contract) — invalid app-side usage, which a driver filing cannot fix; validation layers would flag the APP |

### 2g. Device-side receipts pulled

`g17-logcat-all.txt` (80 lines: r1 init + r2 init + r3 first-draw),
`g17-lldb-r1.log` (662 lines, harness-invalid session),
`g17-lldb-r2.log` (169 lines, O3), `g17-lldb-r3.log` (293 lines, O2
naming + mechanism). Tombstones: none new to pull (§0). Device dir
removed after retrieval; `/data/local/tmp/` == `mg/` only ✓.

### 2h. Score vs G13 oracles (`g14-diff.py`, zero new tooling)

`g14-diff.py ps2x-g13 ps2x-g17`: oracles **ALL OK** (pixel-shas
re-verified); device PPMs in `ps2x-g17`: **0** → `SCORE: N/A`. (Vintage
reason string "G14 run crashed…" — the scored fact is 0 scanouts: O2
dies in first draw before any vsync completes.) No diff table is
fabricable and none is faked.

## 3. Hypothesis verdict + the ONE next action

| claim | verdict |
| --- | --- |
| The O2-argument debugger run names the bad descriptor input at `flush_descriptor_set():3505` | **SUPPORTED**: `update_template = nullptr` for set 0 with `vk_set` valid — named at the `:3505` stop (DWARF) and confirmed at the crash (`x2 = 0`, `x1 == vk_set`, fault at `ldr w8, [x2, #0xf0]`). Separately OBSERVED: O2 is deterministic once past init (4/4 first-draw launches); O1/O3 remain lottery crashes in the init trims |

The ONE next action the numbers justify: **a minimal-fix brief fed by
this capture — NOT a driver filing first.** Rationale: the named bad
input is app-side invalid usage (NULL handle where the spec + Granite's
own assert require valid), so the defect is fixable in our trees: the
fix brief statically names why `update_template[0]` is null for this
compute pipeline's layout (mask-skip at `shader.cpp:551-554` vs
heap/init path vs a runtime-nulled slot — the three candidates §2e
leaves open, with "creation failed" already eliminated by the 0-line
LOGE census), adds the minimal guard/handling, and re-runs on-device
for O2 + score. Queued behind it (not this action): the §2f Adreno
filing package (valid as a robustness report — the driver could reject
instead of segfault — but it cannot fix invalid app input); any further
naming runs (O2's input is named; O1/O3's corruption writer stays open
but no longer blocks the first-draw path once the null is handled).

No verdicts beyond the hypothesis. No port, no adoption, no upstream contact.

## 4. License / provenance receipts

| artifact | receipt |
| --- | --- |
| paraLLEl-GS rev | `3a66c19…` (LGPL-3.0+ per G7 §4 — cited, not re-read) |
| Granite rev | `16e7395f…` (MIT per G7 §4 — cited); naming/capture sites are Granite framework code, read-only |
| G14 shims S1–S3 + G7/G8/G10/G11 hunks | untouched, still uncommitted in SSD clone only; ZERO clone edits this brief |
| G17 additions | ZERO clone edits — session files only: `g17walk.py` + `g17-a.lldb` (lldb) + `g17-run-lldb.py` (watchdog wrapper) + `g17-bpcheck.lldb` (offline bp check) — G17-original, text, mirrored |
| NDK r30 | `30.0.16248370` (Apache-2.0 toolchain); lldb-server is NDK-shipped, pushed to the transient device dir only, removed after |
| logcats/lldb logs | OS/debugger-produced run receipts of our own binaries (no PII; `uid: 2000` shell) |
| adoption hunks | NONE — no third-party code copied into any project tree this brief |

## 5. Exact commands

Mac (`/Users/bradrichardson/dev/ssx3` unless noted;
`COPYFILE_DISABLE=1` on SSD steps;
`NDKBIN=…/toolchains/llvm/prebuilt/darwin-x86_64/bin`):

```text
git -C "<ssd>/parallel-gs-g7" rev-parse HEAD ; status --short   # §2a (+ shims/Granite)
shasum -a 256 <g14-binary> <dump> <mac-binary> <asan-binary> <hwasan-binary(x2+md5)>  # pins
grep -n "vkUpdateDescriptorSetWithTemplate(device" .../command_buffer.cpp  # :3505
grep CMAKE_BUILD_TYPE/C_FLAGS/... <g14-build>/CMakeCache.txt    # -O0-shaped
cp <g14-binary> /tmp/g17-symbols ; shasum -a 256                # symbol copy, c4cd63b4
lldb -b -s /tmp/g17-bpcheck.lldb                                # offline bp resolve (+312)
"$NDKBIN/llvm-dwarfdump" --name='vk_set'/'update_template' /tmp/g17-symbols  # DWARF DIEs
python3 /tmp/g17-run-lldb.py /tmp/g17-a.lldb /tmp/g17-lldb-r<N>.log 1500  # r1/r2/r3
grep -c "Failed to create descriptor update template" <g17+g15 logcats>  # 0 everywhere
sed -n .../shader.cpp .../command_buffer.cpp .../vulkan_headers.hpp  # §2e static reads
python3 <ssx3>/G14/g14-diff.py <ps2x-g13> <ps2x-g17>             # §2h (oracles OK, N/A)
du -sk <ssd dirs> ; df -h / "<ssd>"                             # §0
rm -f /tmp/g17-symbols                                          # 266 MB back
```

adb (`-s 622c49b1` throughout; `/data/local/tmp/g17/` ONLY; `mg/` never touched):

```text
shell 'rm -rf /data/local/tmp/g17 && mkdir -p /data/local/tmp/g17'
push <dump> $G17DIR/g13-dump.gs ; push <g14-binary> $G17DIR/parallel-gs-replayer
push <ndk-lldb-server> $G17DIR/lldb-server
shell 'sha256sum g13-dump.gs parallel-gs-replayer lldb-server'  # all match host
shell 'cmd gpu vkjson' | grep -iE 'driver|deviceName|apiVersion'  # §2a oracle
forward tcp:5041 tcp:5041
logcat -d -s Granite:V | tail -3                                 # before (empty)
shell 'cd $G17DIR && ./lldb-server gdbserver "*:5041" -- ./parallel-gs-replayer $G17DIR/g13-dump.gs --iterations 2'  # per launch (managed session)
logcat -d -s Granite:V > <ssd>/ps2x-g17/g17-logcat-all.txt       # after r3 (80 lines)
shell 'ls -la $G17DIR/ ; ls /data/tombstones/ | tail -3'         # 0 scanouts, 0 new tombstones
shell 'rm -rf /data/local/tmp/g17 && ls /data/local/tmp/'        # DEVICE_CLEAN (mg/ only)
forward --remove tcp:5041 ; forward --list                       # empty
```

## 6. Gaps (what this brief could not do)

1. WHY `update_template[0]` is null is UNKNOWN (mask-skip vs heap/init
   path vs runtime-nulled slot — the fix brief's static question; only
   "creation failed" is eliminated, via the 0-line LOGE census).
2. O1's null-write site and O3's corruption writer stay UNNAMED (G16's
   host leg is still closed; the device writer question narrows to the
   init trims now that O2 is explained without it).
3. Whether the OTHER three O2 instances share `update_template[0] ==
   null` exactly (same fault/frames/registers — presumed, not captured
   under the debugger).
4. Zero on-device scanouts (again) → no pixel score. `g14-diff.py`
   stands ready (oracles ALL OK re-verify passes through it).
5. No fix, no guarded re-run (correctly: the fix brief owns both, and
   now has the named input it needed).
6. No tombstones pulled (none new exist — debugger-held crashes; G15's
   `_07.._11` + OS store otherwise untouched).
7. HWASan-binary sha mismatch (`e2655bae` vs `b93bb69a`) unexplained —
   no writer observed; G15-§2a-shaped ExFAT phantom; binary unused.
8. `upstream/` and ps2xGS harness code untouched; no new dumps (per the
   stop rule).
9. No upstream contact (nothing filed).
10. lldb crash-bt elides `flush_descriptor_set` (return attributed to
    the `:3626` caller frame) — mechanism not investigated; the `:3505`
    stop + `lr` + regs make the naming independent of it.

## 7. Receipt paths

- Clone: `/Volumes/Extreme SSD/parallel-gs-g7/` @ `3a66c19…`
  (G7/G8/G10/G11 hunks + G14 S1–S3 — all uncommitted, untouched by G17).
- G14 build: `/Volumes/Extreme SSD/parallel-gs-g14-android-build/`,
  `tools/parallel-gs-replayer` (265,837,472 B, `c4cd63b4…`, reused).
- SSD: `/Volumes/Extreme SSD/ps2x-g17/` (logcat + 3 lldb logs + 4
  session scripts); `ps2x-g13/`–`ps2x-g16/` pristine.
- Tools: `/tmp/g17walk.py`, `/tmp/g17-a.lldb`, `/tmp/g17-run-lldb.py`,
  `/tmp/g17-bpcheck.lldb`, `/tmp/g17-lldb-r1.log`,
  `/tmp/g17-lldb-r2.log`, `/tmp/g17-lldb-r3.log` (scripts mirrored;
  logs session-only + SSD retrieval; 266 MB symbol copy removed).
- Commits: ps2xGS `[G17]` + `Orchestrated-By: Muse Code` (pushed); ssx3
  `local/research/G17/` `[G17]` + same trailer (NOT pushed).
