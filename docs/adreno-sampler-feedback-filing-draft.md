# Adreno sampler_feedback compiler-crash filing — DRAFT (NOT FILED)

Status: **DRAFT, not submitted.** Standing order: no upstream contact until
the repo owner has reviewed and explicitly approves filing. This document
exists so the filing package survives locally and is paste-ready when that
decision is made.

- Date: 2026-09-21 (G21; re-verified 30/32 facts + 2 folded corrections).
- Evidence: G18 (cause + fix + O4 discovery), G19 (knob discriminator),
  G20 (pipeline named + SPIR-V + package), G21 (re-verification + survey +
  minrepro + this draft). Reports live beside the lane evidence; the
  crashing SPIR-V, minrepro package, tombstones, and logcats are SSD-side
  receipts referenced below, not in git.
- Workaround arm (local, no upstream): G22 — gate the `sampler_feedback`
  dispatch behind a knob, validate on-device, pixel-diff vs G13. The
  workaround does NOT close this filing.
- Filer identity / tracker choice are BLANK below — both need the owner.

The draft text follows verbatim (paste-ready; keep the code-fence content
byte-stable — re-verify hashes against the receipts before any filing):

```text
DRAFT — Adreno driver bug report (NOT SUBMITTED; G21 filing aid, 2026-09-21)
Filer: [YOUR NAME / ACCOUNT — filing needs the user's identity]
Target tracker: [Qualcomm Adreno GPU developer forum / Snapdragon Profiler
  support / AOSP issue — pick one at file time]

========================================================================
TITLE:
[Adreno 830, 512.800.58] Null-deref SIGSEGV in libllvm-qgl.so on
single-threaded vkCreateComputePipelines for valid SPIR-V 1.3 compute
shader using physical storage buffer addresses
========================================================================

1. SUMMARY

A single-threaded vkCreateComputePipelines call for a fixed, valid
SPIR-V 1.3 compute shader (5049 words, PhysicalStorageBuffer64 memory
model, entry "main", no specialization constants, descriptor-buffer
pipeline) segfaults inside the Adreno shader compiler: SIGSEGV fault
0x0 (null-pointer dereference), pc libllvm-qgl.so+0x472fc0, under
qglinternal::vkCreateComputePipelines+1224. Reproduced with identical
fault address, pc, and stack on two consecutive runs (two different
app binaries: uninstrumented and logging-only instrumented); a third
run on an earlier binary crashed in the same driver entry points from
a worker thread (see §6 determinism note). The input shader passes
spirv-val cleanly, so this is a compiler crash on valid input, with
zero app-side concurrency in the primary repro.

2. ENVIRONMENT

  Device / OS:        Odin3 engineering device, Android 15
  Build fingerprint:  qti/sun/sun:15/AQ3A.250728.001/eng.Odin3.20260204.171202:user/dev-keys
  GPU:                Adreno (TM) 830
  Vulkan API:         1.3.284
  Driver version:     512.800.58
  libllvm-qgl.so:     /vendor/lib64/libllvm-qgl.so
                      BuildId 28a2407fad77f0ce9e9cba56e729e521
  vulkan.adreno.so:   /vendor/lib64/hw/vulkan.adreno.so
                      BuildId d05dded928ad46330f433c26cf9b82ec

3. REPRO (minimal, standalone — attachments A1+A2)

  a. Push the two files to the device into one directory:
       g21-minrepro             (13,792 B arm64 binary, BuildID 3341438c...)
       g21-sampler-feedback.spv (20,196 B crashing shader, sha256 below)
  b. Run:  ./g21-minrepro
  c. Observe: the process prints its progress lines, then dies with
     SIGSEGV after "calling vkCreateComputePipelines ..." (no
     "SURVIVED" line). A tombstone is written; it shows the signature
     in §5.

  What the repro does (source attached, ~300 lines C, MIT, no
  dependencies beyond libvulkan): creates a Vulkan 1.3 instance, picks
  the GPU, creates a device with the 15 device extensions the app used
  (incl. VK_EXT_descriptor_buffer) plus the features the shader
  requires (shaderInt16, storageBuffer16BitAccess,
  uniformAndStorageBuffer16BitAccess, bufferDeviceAddress,
  descriptorBuffer), builds the pipeline layout from the shader's
  declared interface (set0: 3x STORAGE_BUFFER + 2x UNIFORM_BUFFER;
  set1: 1x UNIFORM_BUFFER; 12 bytes push constants), verifies the
  SPIR-V by size + FNV hash, then issues ONE
  vkCreateComputePipelines call with the exact creation inputs (§4).

  NOTE (honest status): this minimized repro was built and
  hash-verified host-side but has NOT yet been run on the target
  device. If it SURVIVEs there, the full-app recipe below is the
  confirmed repro, and the minimization missed an input (candidates:
  pipeline-cache state, compile order, or surrounding device state).

  Alternate repro (full app, confirmed 2/2 with identical signature):
  build paraLLEl-GS @ 3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd
  (Granite @ 16e7395f6a48) for Android arm64, replay the attached
  scene dump (11,537,377 B, sha256
  154d9d8577a210fb794b29a048ee2cf08053f87fe220bb2bf975933a02ad7e32)
  with PGS_SKIP_COMPILATION_TASKS=1 --iterations 2: exit 139 in ~1 s
  during the first frame, after two successful sync compute compiles.

4. CRASHING PIPELINE INPUTS (exact VkComputePipelineCreateInfo)

  stage.stage:            VK_SHADER_STAGE_COMPUTE_BIT
  stage.module:           g21-sampler-feedback.spv (sha256
                          b0a09fa039c1d9f9b4fc02b613233e9f051937d38be42e3761834df06aa96ea1)
  stage.pName:            "main"
  stage.pSpecializationInfo: NULL (0 spec constants)
  flags:                  VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
  pNext:                  VkPipelineCreateFlags2CreateInfo with
                          VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT
                          (no robustness, subgroup-size, or heap pNext)
  layout:                 2 descriptor set layouts + 12-byte push range
                          (see §3; layouts carry DESCRIPTOR_BUFFER_BIT)
  pipeline cache:         valid, empty (fresh process)

  Shader facts: SPIR-V 1.3, 5049 words, magic 0x07230203, generator
  "Google Shaderc over Glslang; 11", LocalSize 256x1x1, memory model
  PhysicalStorageBuffer64 (SPV_KHR_physical_storage_buffer). Interface:
  6 buffer descriptors, 0 images/samplers, workgroup-shared memory +
  2 workgroup barriers, 7 device-scope atomics (IAdd/Or/Exchange),
  16-bit types in buffers. spirv-val (SPIRV-Tools v2026.3, spv1.3):
  VALID, no errors. Disassembly attached (A3).

5. CRASH SIGNATURE (from tombstones T2/T3 — identical)

  signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x0
  Cause: null pointer dereference
  Crashing thread: main thread (pid == tid), single-threaded compile —
    the only other thread is parked in pthread_cond_wait; the app's
    async compile path is provably absent (env knob + zero async
    symbols in the tombstone).
  Backtrace (top):
    #00  libllvm-qgl.so +0x472fc0  (BuildId 28a2407f...)
    #01-#17  libllvm-qgl.so  (18 llvm frames total)
    ...  vulkan.adreno.so  (BuildId d05dded9...)
    #21  qglinternal::vkCreateComputePipelines +1224
    #22  Vulkan::PipelineCache::create_pipeline +260
    #23  Vulkan::CommandBuffer::build_compute_pipeline (+1328 / +1776
         across the two binaries — same function, +1776 is the
         logging-only instrumented build)
    ...  flush_compute_pipeline <- flush_compute_state <- dispatch
         <- dispatch_texture_analysis +824 <- flush_rendering +844
         <- ... <- main

6. DETERMINISM

  - Run A (uninstrumented app, BuildID 0b01b274...): exit 139, ~1 s,
    signature above, main thread. Tombstone T2.
  - Run B (logging-only instrumented app, BuildID a8402ce0...): exit
    139, ~1 s, signature above IN FULL (same fault, same pc, same
    offsets, same stack), main thread. Tombstone T3. The
    instrumentation names the crashing pipeline (last pre-create log
    with no post-create line) and did not perturb the crash.
  - Run C (earlier session, same uninstrumented binary, async compile
    path enabled): SIGSEGV fault 0x0 in the same two driver libraries
    under the same vkCreateComputePipelines+1224 / create_pipeline+260
    offsets, but on a worker thread at libllvm-qgl.so+0x2a5fe8 while
    the main thread was concurrently inside vkCreateComputePipelines.
    Supporting data point, NOT the same pc/thread — the primary repro
    is runs A+B (single-threaded, deterministic).

7. EXPECTED vs ACTUAL

  Expected: vkCreateComputePipelines returns VK_SUCCESS (the shader is
    valid SPIR-V 1.3 and all required features are enabled).
  Actual: the calling thread dies with SIGSEGV fault 0x0 inside
    libllvm-qgl.so; no VkResult is ever returned.

8. ATTACHMENTS

  A1  g21-sampler-feedback.spv (20,196 B; sha256 b0a09fa0...)
  A2  g21-minrepro.c + g21-minrepro (arm64, BuildID 3341438c...) +
      build recipe + run instructions
  A3  g21-sampler-feedback.dis (spirv-dis output, 1176 lines)
  T1  tombstone run C (async-path instance) + logcat run C
  T2  tombstone run A + logcat run A
  T3  tombstone run B (instrumented, names the pipeline) + logcat run B
  D   full-app scene dump (11,537,377 B; sha256 154d9d85...) — only
      needed if the minimized repro does not reproduce

9. CONTACT

  [YOUR NAME, EMAIL, ACCOUNT — the person Qualcomm can reply to]
  Preferred: [public tracker handle / email]

--- end of draft (G21; NOT submitted) ---
```
