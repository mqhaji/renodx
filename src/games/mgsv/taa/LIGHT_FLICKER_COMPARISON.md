# MGSV general-gameplay light-flicker comparison

Comparison date: 2026-09-05. Baseline: the user-supplied `src/games/mgsv-old`; candidate: saved files in
`src/games/mgsv`. The baseline is user-reported stable on gameplay lights. Night vision and upscaler-folder
restructuring are deliberately outside this investigation.

**Follow-up implementation:** the catalogue below records the pre-test source comparison. The first isolation build
removes the later light-VS workaround and adds the default-Off **FSR Legacy Compute State** checkbox. Only native
state-preservation calls change between its Off/On settings. Jitter math/guards, camera-history policy, FSR shaders,
DLSS query tracking and deferred scheduling remain unchanged. The initial negative visual report is superseded: the
user later confirmed Legacy On reduces flicker, with minor residual, during a no-DLSS cold-start run. Legacy On becomes
the FSR default; the matched follow-up restores DLSS runtime availability while keeping this improved profile fixed.
Native-hook success rates did not increase with Legacy On, and residual flicker is not proven algorithmic. The detailed
results and [A/B procedure](README.md#general-gameplay-light-flicker-ab) distinguish these stages of the investigation.

## Summary

The alternate native projection hooks were **not removed** between these two folders. At production settings their
addresses, matrix writes, and frame/sample eligibility rules are equivalent. A timing-dependent failure to apply them
is still plausible, but the previous claim that a newly added strict sample check explained this regression was wrong:
that check is already present in the supplied known-good baseline.

The most important changes shared by the AMD method and DLSS are D3D11 state capture/restoration, camera-history commit
semantics, and new DLSS-related work/hooks installed even while the AMD method is selected. The later light-VS workaround
also introduces its own correctness risks. No single cause has yet been isolated by an old/new runtime A/B.

## Baseline identity and complete file inventory

- Both supplied trees implement **FSR3 Upscaler 3.1.5**, through `ffxFsr3UpscalerContextDispatch`, with FidelityFX backend
  interface 2.3.0. Neither contains an active FSR2 implementation. Here, "AMD method" refers to that supplied code.
- Git-normalized blob identity matches HEAD `8ceb5191` for the old addon, shared header, `taa.hpp`, FSR runtime, and
  projection/camera/coordinator/state headers. That baseline already includes the prior temporal restructuring.
- Before adding this catalogue: **166 old files, 170 current files**; **97 byte-identical**, **54 line-ending-only**,
  **15 existing files with content changes**, **4 added files**, **no removed files**.
- The 54 line-ending-only changes are 53 files under `taa/fsr3/ffx/` and `tonemap/customtest30.hlsli`.
- Empty old `boot/` and `taa/dlss/` directories do not constitute additional implementation files.
- Classification used saved-file hashes and newline-normalized comparisons. Some editor reads returned older content;
  the saved-file comparison, not those stale reads, is the basis of this catalogue.

All paths below are relative to `src/games/mgsv`.

| Changed/added file | Behavioral change | Relevance |
| --- | --- | --- |
| `addon.cpp` | Registers VS `0x200DBED9` with a per-draw callback writing the global injection structure. No other addon difference against this baseline. | Direct light-path change, but added after the original failure. |
| `shared.h` | Adds four light-projection floats after the existing fields; documents DLSS mode 3. Existing field offsets remain unchanged; total structure grows from 96 to 112 bytes. | New per-draw data ownership; stale addon/live-shader ABI must be excluded. |
| `taa/shaders/0x200DBED9.vs_5_0.hlsl` (added) | Replaces the original light VS; changes clip XY by absolute target-minus-source projection terms. | Not present in the known-good folder; see concrete defects below. |
| `taa/runtime/projection_jitter.hpp` | Adds atomic per-path counters/periodic reports and publishes absolute projection target terms. | Existing native routing and acceptance rules remain equivalent at stable/default state. Instrumentation adds work. |
| `taa/runtime/camera_state.hpp` | Adds target terms and captured current VP; adds `GetForCapture`; changes `Commit(frame,sample)` to `Commit(CameraFrame)`. | Changes matrix ownership and removes the latest-publication match check from completion for every method. |
| `taa/runtime/input_capture.hpp` | One-line change: capture uses `GetForCapture()` instead of `Get()`. | Adds publication-lock acquisition and pins the current VP alongside captured inputs. |
| `taa/runtime/d3d11_compute_state.hpp` | Expands CS ranges, captures/restores five graphics-stage SRV arrays and OM state, explicitly unbinds OM outputs during capture. | Shared behavior changes even with DLSS never selected. |
| `taa/runtime/coordinator.hpp` | Extracts common dispatch; adds DLSS branch, device binding, Begin/End query hooks, deferred prefix/post bridging, generation checks and pending-list lifetime. | Common completion now uses captured-camera commit. Deferred splitting is DLSS-only, but installed query tracking is not. |
| `taa/runtime/state.hpp` | Adds mode 3, generation and pending-dispatch flag; enables diagnostics; gives projection controls a separate switch and analytical-only effective values. | AMD/DLSS still use Halton-8 and production `1x` scales. DLSS advances samples later, at submission. |
| `taa/settings.hpp` | Adds DLSS capability/model selectors and transition handling, tooltip fix, diagnostic guard/visibility changes. | AMD effective Halton policy is unchanged. The shared Unclamp Motion Vectors setting was also inadvertently hidden outside Analytical TAA; its value still affects SDK inputs. |
| `taa/taa.hpp` | Adds light injection callback, device filtering, DLSS probing/query-bridge setup at Present, pending-dispatch reset exemption, command-list events, and counter logging. | Changes CPU work/synchronization around the Present-derived epoch for all methods. Draw classification and insertion cascade otherwise unchanged. |
| `taa/fsr3/runtime.hpp` | Removes `IsCameraDiscontinuity` and its reset condition. | Actual temporal-policy change: old resets on invalid/large reprojection; current resets only when history is invalidated. |
| `taa/fsr3/shaders/fsr3_prepare_game_inputs.cs_5_0.hlsl` | Uses native velocity if matrix reprojection reports failure. | Valid reprojection math/signs unchanged. Failure cases no longer retain the helper's zero/nonfinite result. |
| `taa/dlss/runtime.hpp` (added) | NGX discovery, capability/model checks, feature/resources, evaluation, failure handling, GPU-wait release. | New method; not part of the old AMD algorithm. |
| `taa/dlss/shaders/dlss_prepare_game_inputs.cs_5_0.hlsl` (added) | Linear color, canonical motion, R32 depth copy. | New method boundary based on the AMD conversion. |
| `taa/dlss/shaders/dlss_encode_game_output.cs_5_0.hlsl` (added) | Re-encodes linear output, preserves current alpha. | New method boundary. |
| `README.md`, `taa/README.md`, `taa/ROADMAP.md` | DLSS/diagnostic/reset/workaround documentation. | No executable behavior. Some prior causal/proof wording is stronger than the collected evidence; this catalogue qualifies it. |

## What did not change

- FSR3 vendor code: no content changes after line-ending normalization. Backend, shader-blob provider, fixed SM5
  wrappers/permutations, resource descriptions, host schedule, history ping-pong, clears and internal algorithm are the
  same.
- FSR flags/tuning: HDR, reverse-Z, native input/output size, sharpening off, unity application exposure,
  shading-change scale `0.15`, accumulation increment `1/3`, and no external reactive/T&C mask are the same.
- Color/motion conversion for valid inputs: sRGB decode, linear reconstruction, sRGB encode, current scene alpha,
  `.ba` motion decode, motion-vector sign/scale, pixel jitter sign/scale, and object-motion de-jitter are the same.
- Analytical runtime and resolve shader, descriptor tracker, frame-input contract file, velocity replacements, FXAA,
  scene/velocity clone setup, post-processing shaders and tonemap/LUT code are unchanged between the supplied folders
  except the listed line endings. The embedded `CameraFrame` type did change despite `frame_inputs.hpp` being identical.
- Among 71 old `.hlsl` files, 60 are byte-identical, 10 differ only in line endings, and only the AMD input adapter has
  a content change. Three `.hlsl` files are newly added.
- Native paths: Velocity, Forward, Model, Alpha Model, Overlay Model and Local Light retain their addresses, executable
  validation, viewport offsets, matrix dirty flags, and local-light exact restoration.
- Main jitter remains projection `[8] += 2*jitter_uv.x`, `[9] -= 2*jitter_uv.y`; sample generation remains Halton-8.
- The primary `DOF_ScatterBakeFirst` insertion, later CopyRenderBuffer/tonemap fallbacks, validated dimensions,
  common output copy-back and barrier ordering are unchanged.

## Failure mechanisms and priority

### 1. Alternate paths can still lose jitter at the Present boundary

Compare old `projection_jitter.hpp:373-402` with current `:401-446`. Both demand a same-epoch publication and matching
sample. Compare both versions of `input_capture.hpp:282-299`: reconstruction permits current **or previous** epoch
provided the sample still matches.

This permits the following sequence:

1. Native main projection publishes `(epoch F, sample S)`.
2. Present advances the global epoch to `F+1` without advancing `S`.
3. An alternate native path rejects the camera as `stale_frame` and renders without its correction.
4. Reconstruction accepts that same camera as a permitted previous-epoch input and proceeds.

Prior current-build logs directly show `stale_frame` rejections. For example, one Alpha Model interval had 300 requests,
13 applications and 287 stale-frame rejections; other intervals had 300/300 applications. Those reports do not contain
the rejected publication/sample/viewport identities, so they cannot prove the exact sequence or tie it to a visible light.
The zero `sample_advanced` counter only covers requests that survived the earlier frame check.

**Conclusion:** this is a real policy mismatch and the strongest match to intermittent missing jitter, but it already
exists in the good baseline. The regression must be a change that exposes it, a different changed path, or both.
Do not simply remove age/sample checks: they prevent using another scene's camera.

New potential timing contributors include the much larger shared state capture/restore, unconditional query tracking,
extra Present work, and capture-side publication locking. A skipped velocity correction also conflicts with the
unchanged adapter's assumption that configured current jitter was applied before it subtracts that jitter from object
motion. None of these timing links is yet runtime-proven.

### 2. Shared D3D11 state changes are the first common-path A/B target

Old `d3d11_compute_state.hpp` saves 2 CS samplers, 16 SRVs, 8 UAVs and 3 constant buffers plus compute shader/layout.
Current `Capture`/`Restore` expands these ranges, saves/restores graphics SRVs and OM state, and unbinds OM outputs at
line 151. Both AMD and DLSS call this path from `coordinator::DispatchValidatedLocked`.

This is not merely adding storage for NGX. It issues new graphics-state commands even when AMD is selected, can change
read/write hazard behavior, and adds substantial getter/AddRef/rebind work to deferred recording. No particular
restored binding is proven incorrect yet. Isolate this difference for the AMD branch before touching AMD shader math.

DLSS-specific Begin/End tracking is also installed unconditionally from `taa::OnPresent`; query callbacks lock a mutex
and maintain an unordered map/set even during AMD rendering. The new secondary-command-list callback takes this mutex
even when no DLSS dispatch is pending. NGX capability probing is cached, not full initialization every frame.

### 3. Camera history semantics changed globally to accommodate delayed DLSS

Old `camera_state::Commit(frame,sample)` validates that the latest publication still matches, then commits the staged
matrix. Current `GetForCapture` pins that matrix earlier; `Commit(CameraFrame)` commits the captured matrix and checks
only valid/matrix-valid flags. This can be correct for delayed submissions, but also changes which publication races
cause reset versus continued history in AMD. Captured current-to-previous reprojection is not recomputed after later
commits. Record capture, publication and commit identities before attributing this to bad camera math.

AMD and Analytical still advance samples during deferred command recording. DLSS now advances them when the split
list is submitted on the immediate context. That submission/recording distinction is DLSS-specific and cannot alone
explain AMD failing on a fresh launch, though the surrounding shared work can affect both.

### 4. The later light-VS workaround is not a clean baseline

The good folder has no `0x200DBED9` replacement. The new one was added after the first failure, so it cannot explain
initial onset. It should be removed/disabled as a separate A/B rather than expanded as an assumed fix.

Concrete issues in the workaround:

- `addon.cpp:758` discards the command-list argument. `taa.hpp:97-124` rewrites four **global non-atomic floats** on
  each matching draw, first resetting them, then setting a target. The shader module later uploads that global memory.
  `mods/shader.hpp:1758-1838`, `utils/constants.hpp:271-318` and the command-action dispatcher provide no lock spanning
  updater and upload across recording contexts. One thread may upload another thread's reset/partial target. A
  draw/context-owned snapshot is needed if retained; locking only the updater is insufficient.
- Qualification uses the latest Present token, not the draw's camera/cbuffer/viewport/dimensions. Absolute target-minus-
  source can overwrite a legitimate different camera's projection offset if incorrectly associated.
- Original dumped ASM uses `v4.z` and `v4.w` for the third/fourth bone influences. Replacement lines 84-93 use `v4.y`
  for both, inheriting a decompiler error. Restore ASM-equivalent indexing before calling this replacement vanilla-
  equivalent. Relevance depends on whether the affected draw uses that skinning branch.

The projection field/sign math itself matches the original transposed cbuffer convention: CPU `[8,9]` correspond to
HLSL `_m20,_m21` / raw `cb2[4].z,cb2[5].z`. Target-minus-source does not inherently double-jitter a correctly matched
projection. Shader activation alone does **not** prove its source matrix is unjittered or that this is the affected light.

### 5. Removed history reset and motion fallback remain separate, lower-priority A/B axes

The old AMD runtime contains the same camera-discontinuity heuristic that was later removed. Removing it changes
history retention across camera changes and can expose behavior the old resets masked. It does not establish the cause
of stationary-light flicker, and restoring it blindly would reintroduce its observed false positives.

The adapter's new native-motion fallback is active only when matrix reprojection fails. Its valid-pixel math is
unchanged. Count failing pixels in the affected region before changing motion signs or tuning.

## Runtime evidence boundaries

- On this investigation's start, MGSV was stopped; no game/binary/live-shader state was modified.
- The latest installed/logged build is baseline-style: SHA-256
  `E91F148C65E403AAB67B547A019D6B52C30931D2C05BEAE9D7549958DA1F2E99`, log dated 2026-09-04, AMD 3.1.5 startup,
  no DLSS messages and no path-counter reports. This is not a verified build of today's current sources.
- That log has 168 AMD accumulation-start messages over the session, zero camera-commit failures and zero stale-capture
  warnings. The resets occur in bursts, not throughout every frame. Reset-message presence alone is not proof of the
  reported flicker, particularly with a user-reported stable baseline.
- The configured DevKit `LivePath` still points to current `src/games/mgsv`. It does not prove replacements were loaded,
  but a future old/new comparison must exclude live overrides. Never load both game addons at once.
- Previous DevKit draw indices/timestamps describe CPU callback order, not proven GPU execution order. Live resource
  analysis is not a frozen snapshot; changing object-mask coverage across separate reads was not proof of the light cause.

## Controlled isolation plan (not executed)

1. Keep `mgsv-old` untouched. Reproduce the same stationary and slow-pan light scene with one matching addon, matching
   settings and no DevKit live overrides. Record binary hash and active method for each run.
2. Establish a current **AMD-only** comparison without activating DLSS. Remove the later light-VS workaround in a
   separately recorded variant; do not conflate its failures with the original regression.
3. Against that variant, test the old compute-only preservation path for AMD, leaving NGX preservation separate.
   Then independently suppress DLSS-only query/secondary-list work when DLSS is unused. Avoid changing the jitter
   gates or algorithm in the same build.
4. If needed, independently restore the old AMD camera capture/commit contract, then the old reset decision. Log which
   old reset condition fires; do not treat successful compilation or fewer reset messages as visual validation.
5. Put the **same bounded telemetry in both compared variants**: mode; native path; viewport/camera identity; producer
   publication, Present, capture and committed frame/sample; chosen jitter; rejection reason; active/source projection
   terms; recording context; submission identity. Counters alone do not show whether the light draw received the offset.
6. For the exact affected light, compare its actual VS `b2` projection and scene-depth grid across Halton phases and
   compare scene color before reconstruction. If it is already misaligned upstream, fix the frame/camera association or
   proven missing producer; do not hide it with history tuning or additional speculative shader patches.
7. Only after AMD parity is restored, reintroduce/validate delayed DLSS submission using the same proven input frame.
   Build only the relevant game target; no root CMake, shared modules, vendor changes, NVG experiments or consolidation.

## RenoDX review checklist

- **Pass:** comparison scoped to the supplied game folders; unchanged HDR/color/resource contracts identified.
- **Pass:** comments distinguish observed behavior, source defects and hypotheses, with independent validation steps.
- **Needs comment:** default gameplay stability and temporal frame ownership remain unproven; use the controlled plan.
- **Needs comment:** new light replacement needs draw-owned injection, camera qualification and ASM-equivalent skinning,
  or removal while isolating the baseline regression.
- **N/A:** full PR commit hygiene, HDR10/scRGB policy, LUT/tonemap design and complete Preset Off review; no changes to
  these subsystems are proposed here.