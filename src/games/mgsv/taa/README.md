# MGSV Temporal Anti-Aliasing

Native-resolution Analytical TAA, AMD FSR 3.1.5, and NVIDIA DLSS for Metal Gear Solid V: The Phantom Pain.
See the [addon README](../README.md) for HDR resource handling and build/deployment instructions,
the [roadmap](ROADMAP.md) for outstanding work, and [FSR provenance](fsr3/README.md) for SDK versions and credits.

## Usage and defaults

The **Temporal Anti-Aliasing** section in the ReShade addon overlay contains these controls:

| Control | Current behavior/default |
|---|---|
| **Temporal Reconstruction** | Off (Vanilla FXAA), Analytical TAA, AMD FSR 3.1.5, NVIDIA DLSS. New profiles default to FSR3. |
| **DLSS Model** | Presets A-F and J-M, subject to DLL support. Default/reset is F (persisted value 6), with DLL-default fallback when unsupported. |
| **TAA Jitter Pattern** | Analytical-only Halton (2,3), eight phases, or diagnostic Off. FSR3/DLSS enforce Halton without overwriting the Analytical preference. |
| **TAA Clip Tightness** | Analytical history bounds; default `0.50`. |
| **TAA History Clip Strength** | Analytical color-box clipping; default `1.00`. |
| **TAA Current Frame Blend** | Maximum adaptive filtered-current contribution; default `0.15`. |
| **Unclamp Motion Vectors** | Shared diagnostic, default Off. Also affects native motion blur. |
| **Projection Jitter Path sliders** | Eleven Analytical-only controls, range `-2x..2x`, all default/reset to `1x`. |
| **TAA Diagnostic View / Velocity View Range / Object Motion Source** | Analytical diagnostics; default Temporal Resolve, `8 px`, Auto-Corrected Native Object Velocity. |

Method, model, tuning, and projection scales remain preset-local. Existing saved values are preserved; old split
enable/method keys migrate to `FxTemporalReconstructionMode` without enabling a previously disabled profile.
Preset Off selects **Off (Vanilla FXAA)**, restoring vanilla FXAA and the unjittered projection path.
Changing history-affecting controls resets accumulation; visualization range alone does not.
Relaxing analytical clipping or current-frame blending can reduce shimmer at the cost of ghosting.

Both `ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS` and `ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS` remain `1` in
`runtime/state.hpp`; debug UI is available during gameplay. FSR3/DLSS ignore slider values and use `1x` on the original
six paths and `0x` on the five experimental paths. Projection-macro-0 builds use that same six-on/five-off policy.
Motion-macro-0 builds use Temporal Resolve, `8 px`, and corrected native object motion.

The FSR Legacy Compute State toggle **and compute-only implementation are removed** following the user's successful
Legacy Off validation. All methods use the existing extended compute/graphics/OM preservation, including depth-stencil
state. Old `FxFsrLegacyComputeState` keys are ignored without rewriting INIs. FSR tuning remains `0.15` shading-change
scale and `1/3` accumulation increment. This removal is not a new flicker fix.

Temporary pose/draw-buffer staging, GPU readbacks, geometry matching, binary dumps, jitter-link journals, per-path
telemetry, and unused hook-call atomics are absent. Normal input/camera capture, DLSS query-scope tracking,
command-list ownership, and SDK completion/cleanup safety remain; they are not disposable diagnostic instrumentation.

## Frame and resource contracts

1. The validated main `SetViewMatrixState` call captures canonical no-jitter P/V, applies Halton to the gameplay
   projection copy at `ShaderManager+0x680`, and publishes frame/sample, dimensions, jitter, and camera provenance.
2. Guarded secondary native paths consume that publication without republishing camera history.
3. `MotionBlurCameraVelocity` supplies depth, object velocity, final velocity, and the matching camera publication.
   Scene color is accepted at a dimension-matched insertion. One `ValidatedFrameInputs` value serves all methods.
4. The insertion cascade is full-resolution `DOF_ScatterBakeFirst` (`0xFE1DC3F8`), sequence-qualified post-DoF copy,
   sequence-qualified post-motion-blur-tile copy, then Tonemap/1D-LUT. Lower-resolution candidates are skipped.
5. One resolve per frame produces encoded output for copy-back and restores common D3D11 state. Successful camera
   commit checks reset generation and sample, then advances the sample under the publication lock.
6. Vanilla FXAA becomes pass-through while temporal reconstruction is enabled.

Inputs must agree on device, dimensions, resource type, sample count, presentation epoch, and temporal sample.
Callbacks can straddle Present: current or immediately previous-epoch input is accepted only with the matching sample.
Older or mismatched input fails closed. A Present with no new full-resolution candidate preserves history; a matching
candidate that cannot resolve invalidates it after the fallback cascade. Creation, resize, mode/settings changes, and
missing previous camera history reseed accumulation.

`ValidatedFrameInputs` freezes metadata and ownership, **not GPU resource contents**. Retained SRVs are live resources;
CPU callback order, camera commit, and matching handles do not prove which writes or poses contributed to a pixel.
Analytical/FSR work is recorded on deferred contexts; DLSS completion occurs at immediate-context submission.

| Signal | Contract |
|---|---|
| Scene color | Full-resolution RGBA16F clone in MGSV's sRGB-encoded scene domain; decode for linear reconstruction. |
| Scene alpha | Preserve current-frame alpha exactly for downstream DoF/highlight behavior. |
| Depth | Full-resolution reverse-Z from the velocity pass. |
| Object/final velocity | Separate RGBA16F clones, preserving precision before the composite; native packed motion in `.ba`, object mask in `.r`. |
| SDK inputs | Owned linear RGBA16F color and signed RG16F motion; DLSS also owns an R32F reverse-Z depth copy. Native render/output extents are equal. |
| Camera history | Double-precision no-jitter matrix history, promoted only on a successful matching commit. |

All temporal methods retain full common CS slots/shader/classes, graphics SRVs, OM unbind/restore, render targets,
DSV, blend/depth-stencil state, and tracked compute descriptors. Do not reintroduce reduced-slot FSR preservation.

## Native jitter ownership and lifetime

`runtime/projection_jitter.hpp` validates executable bytes, image bounds, copy context, call targets, and whitelisted
return sites before installing native hooks. The main viewport must be enabled, full-resolution, perspective, and have
a valid camera; canonical viewport P must match the active shader-manager copy before correction.

- Setter corrections change only projection elements 8/9 and their dirty flags: `P[8] += 2*jitter_uv.x`,
  `P[9] -= 2*jitter_uv.y`, scaled for the selected path.
- The original six paths are Velocity, Forward, Model, Alpha Model, Overlay Model, and Local Light. The guarded local-light
  callback temporarily changes its viewport source and restores all sixteen floats before returning. Persistent viewport
  P otherwise stays canonical; no broad mapped-constant-buffer mutation or culling/UI/shadow projection changes.
- Velocity alone permits the previous Present epoch under exact camera/viewport identity, dimensions, byte-exact
  canonical P/V, reset-generation, and sample checks. Its publication guard spans validation and the native write;
  it does not acquire the execution gate. Other original secondary paths retain their existing epoch policy.
- Experimental scopes own P/V, sequence, generation, and sample. Final writes use the common nonblocking execution gate
  then publication guard; scale/reset transactions follow transition -> execution -> publication ordering. Revalidate
  inside the final guarded section. Do not hold these guards across original native calls, allocation, or logging.
- Reject null/orthographic/foreign cameras and nonfinite, already-jittered, or ambiguous matrix matches. Rejected nested
  scopes must shadow outer scopes. Borrowed synchronous data must not escape into deferred work or cross-thread TLS.
- A deferred packet needs proven producer-to-consumer generation/sample ownership. Matching the latest VP or pointer
  is not enough; do not invent a global pointer cache or guessed payload fields to bypass that requirement.

**Native projection hooks and the addon module are pinned for process lifetime.** Device destruction stops admission;
it does not unpatch or free trampolines. Recreation reuses the installation. Restart MGSV to replace or remove the addon;
dynamic native-hook unloading is unsupported. Installation belongs at the established init-device boundary, never a
slider callback. Do not introduce detach waits while holding a lock needed by callbacks. Stopping admission or changing
Off/reset cannot undo previously admitted/submitted work; rapid-transition GPU ordering still requires validation.

### Experimental projection paths

These five controls are implemented and default to `1x` for Analytical, **not declared visually validated**.
Addresses and offsets below are hexadecimal RVAs/offsets; executable validation remains mandatory.

| Control / persisted key | Correction and restriction |
|---|---|
| Sun Volume / `FxTaaSunVolumeJitterScale` | `30AC50 -> 30B680`, return `30B07D`: private aligned VP copy for synchronous volume upload/draw. |
| SH Fallback / `FxTaaShFallbackJitterScale` | `27EF80`, return `1F2529`: successful output XY only; kinds 0..3 use validated 8/6/18/8 vertex counts. Preserve Z/W, visibility, depth bounds. |
| Terrain Decal / `FxTaaTerrainDecalJitterScale` | `275280 -> 2D18D0`, return `2755CA`: native-owned packet P at `+160` before attachment; affects inverse-plane reconstruction, not separately captured raster P. |
| General Light Packet / `FxTaaLightPacketJitterScale` | `2A4C00 -> 2D3380`, return `2A5342`: native-owned VP at `+130` before attachment; preserve clip-W and selection data. |
| Alternate Stream / `FxTaaAlternateCameraJitterScale` | `2E7330/2E4C70 -> 2D7AA0`, returns `2E7484/2E4DD7`: private VP copy at engine constant `0x47` staging, never context P/V mutation. |

Clip translation changes XY by the jitter offset times W, leaving Z/W unchanged. SH validates original success, kind,
bounds, and all finite results before storing. Packet attachment must retain native-owned storage, never a stack payload
or copied refcount header. Alternate constant `0x47` means bank 7/vectors 4..7, not physical D3D `b7`; null-camera updates
can leave old context P/V intact, so persistent context edits are unsafe.

The deferred **SH packet** route (`27F450`) is not implemented or exposed: producer-generation ownership is unresolved.
Live draw-grid correlation, terrain inverse/raster consistency, general-light final consumers, and alternate world/UI
separation remain outstanding. Do not enable these five paths for FSR3/DLSS based on defaults or compilation alone.

## Motion and reconstruction

Native `.ba` decodes as `(ba*2-1)*64/render_size`, a current-minus-previous UV delta. History samples at
`output_uv - velocity`, not at the selected dilation UV. Background motion comes from depth and no-jitter matrices;
object pixels retain native bone-aware motion with the configured current projection jitter removed. Do not change
sign/swizzle/scale, subtract a global jitter delta, or gate correction on a last-success counter without pixel evidence.

Analytical selects center plus four diagonal depth taps using maximum raw reverse-Z (`>=` makes the last equal-depth
tap win), moving mask/motion/depth/UV together. FSR3/DLSS adapters load center pixels; this difference alone does not
establish the cause of warble in any method. Analytical decodes current filtering and all sixteen Catmull-Rom history taps before
linear-light reconstruction, then stores history in MGSV's encoded domain. It has no previous-depth rejection or locks.

The Object Motion Source modes retain Auto, Matrix Camera Everywhere, current-jitter and jitter-delta sign diagnostics,
plus **Direct Object Velocity** (6: bypass composite at the same selected texel) and **Auto Center Pixel**
(7: disable dilation, retain Auto's correction/composite). These are diagnostics, not warble fixes. Matrix Camera
Everywhere discards deformation motion and causes animation ghosting; it is not a production substitute.

The three minimal velocity pixel-shader replacements preserve native encoding and alpha tests. **Unclamp Motion Vectors**
only bypasses the native radial clamp while enabled with temporal reconstruction. The approximately 64-pixel clamp and
native motion-blur compatibility remain reasons to keep it default Off until an owned temporal-only path is proven.

## SDK lifecycle

FSR3 uses AMD's 3.1.5 host schedule through the local D3D11/SM5 backend. It supplies no external reactive,
transparency/composition, or exposure resource; internal shading-change/reactivity/disocclusion analysis remains active.
`preExposure = 1`; sharpening and the separate generate-mask API are disabled even though their pipelines exist.
This is a local source adaptation, not an AMD-supported D3D11 integration. See [provenance and licenses](fsr3/README.md).

DLSS requires a compatible `nvngx_dlss.dll` beside `mgsvtpp.exe` and the installed NVIDIA NGX core; the addon does not
bundle the feature DLL. Startup checks only ReShade's rendering-device vendor ID. Non-NVIDIA/unidentified devices show
a disabled option with a red explanation. NVIDIA eligibility is not proof of feature support.

- No DLL lookup/version/load, NGX call, or native DLSS query-hook installation occurs before explicit or restored DLSS
  selection. Opening the dropdown does not activate it. Settings queue activation for Present, never initialize in a draw.
- While pending, requested mode remains DLSS but effective mode/binding is Off. A newer choice cancels the request.
  Support failures are cached with an explanation and effective Off until restart; no per-Present retry loop.
- Install the query bridge only after successful probing and an uncanceled request. Existing deferred recordings are
  unknown, not query-free: splitting requires a post-install context or a natural successful `FinishCommandList`
  boundary. Reset/Present/failed Finish does not establish that boundary. Skip unqualified recordings.
- NGX evaluates on the immediate context between the original prefix and post-insertion command list. Preserve pending
  original prefixes through mode/generation changes; do not discard game work or evaluate stale generations.
  Query tracking survives ordinary mode changes after first activation; fresh never-DLSS startup is a distinct state.
- Leaving DLSS suspends one feature/texture bundle instead of releasing it or waiting for the GPU from Present.
  Compatible device/context/size/model resumes with fresh history; incompatible requests recreate during dispatch.
  Retention is approximately 253 MiB of adapter textures at 4K plus NGX allocations, not a growing bundle per switch.
- Actual teardown verifies bounded GPU completion and NGX release; failure retains owners and disables DLSS.
  Query completion and retained ownership are safety requirements, not temporary readback instrumentation.

DLSS uses native-resolution quality, auto exposure, no sharpening, and the selected supported model hint. Unsupported
submission/state/query conditions or NGX failure queue a coherent transition to Off. Analytical/FSR inactive graphs
are released; DLSS suspension is a transition-crash mitigation, not identification of the original heap-corrupting write.

## Known limitations and validation status

- **Character warble root cause remains unidentified.** Latest user clarification (2026-09-09): Analytical Jitter Off
  did not improve it, and repeated Auto/Direct comparisons showed no noticeable difference. Matrix Camera Everywhere
  was **inconclusive** because heavy animation ghosting prevented judging warble; it is not evidence that bypassing
  native object motion helped. The narrow velocity ownership correction did not visually fix the observed warble.
  The user tentatively thinks DLSS may not warble while Analytical TAA/FSR are noticeable; this is not confirmed.
- Prior repaired readbacks completed but did not establish same-character color/velocity geometry and contributing
  history correspondence. Native previous bones/object transforms versus the last accumulated frame remain unproved.
  These are coverage gaps, not evidence of a native-pose root cause or grounds to prioritize it from the camera-only test.
  Current source has no automatic capture machinery; no repeated generic capture or old A/B run is requested.
- Rapid night-vision skinned-mesh flicker remains separate and unfixed. Thermography target/depth ownership is unproven.
- Five experimental projection paths still need runtime coverage/transition validation. There is no proven native
  camera-cut signal; do not restore the removed clip-space heuristic that falsely reset SDK history every frame.
- FSR external masks/exposure and analytical previous-depth/disocclusion/lock history remain absent. Thin detail,
  transparency, rain, emissives, and disocclusions require continued validation. No method applies sharpening here.

Earlier runtime tests established RGBA16F object/final velocity sampling and FSR3 dispatch on D3D11 Feature Level 11_0.
They do not establish current character stability. The preceding gameplay/legacy-removal cleanup built successfully as
`mgsv` with the approved MSVC/Ninja Release profile; that cleanup binary was **not deployed or newly gameplay-tested**.
This documentation-only cleanup adds no build, deployment, visual, or performance result.

## Manual validation

Build/deploy only with MGSV fully stopped; use one matching addon and no live shader overrides. Follow the
[addon build instructions](../README.md#building--deployment). Inspect generated Analytical, FSR3, DLSS boundary, and
native velocity shader artifacts. Restart for addon/DLL replacement; do not attempt live native-hook unloading.

1. Check fresh/reset settings: FSR3 method, DLSS F hint, eleven scales `1x`, debug UI enabled. Separately confirm saved
   preset values remain intact. Off must restore vanilla FXAA/projection without persistent shift or stale history.
2. Check FSR3/Analytical selection, hidden SDK tuning/jitter controls, and one accumulation seed per intended reset.
   Lower-resolution DoF candidates and static menus straddling Present must not cause recurring resets/warnings.
3. Cover static detail, idle/skinned motion, hair, wires, foliage, pans, aiming/binoculars, cuts, menus, pause/resume,
   DoF/motion blur, rain/transparency/emissives, NoIR/sonar/night vision, and resolution/device changes. Record warble
   and trails separately; do not infer a fix from a successful dispatch or quieter log.
4. Validate each new projection path independently before promotion: signed/fractional scales, actual RTV/DSV grid,
   packet/vertex effects, foreign/null cameras, world/UI separation, rapid preset/method/Off changes, and normal exit.
5. For DLSS, verify vendor-only cold startup, request-time activation (including a saved request), missing-DLL/non-NVIDIA
   explanations, cancellation, and known recording boundaries. Check FSR/Off/Analytical round trips for suspension/resume
   and fresh seeds, then model/resize recreation separately. One successful switch does not prove crash mitigation.
6. Keep Unclamp Off for baseline gameplay; any clamp or motion-source comparison must also check native motion blur
   and animation trails. Do not repeat the already-negative Auto/Direct sweep as a new warble investigation.

Normal logs cover startup/availability, selection, first publication, context/feature creation, accumulation seeds,
suspension/resume, disable, and one expected lower-resolution-candidate notice. Repeated seeds with unchanged settings
or recurring capture/dispatch warnings remain actionable. There are no per-frame success or projection-path reports.

## Source map

| Files | Responsibility |
|---|---|
| `taa.hpp`, `settings.hpp` | Lifecycle, draw/insertion routing, preset-local controls and transitions |
| `runtime/state.hpp`, `runtime/camera_state.hpp`, `runtime/projection_jitter.hpp` | Sample/reset state, canonical camera history/publication, validated native corrections |
| `runtime/descriptor_tracker.hpp`, `runtime/input_capture.hpp`, `runtime/frame_inputs.hpp` | Per-list bindings, resource lifetime, validated game-native metadata |
| `runtime/coordinator.hpp`, `runtime/d3d11_compute_state.hpp` | SDK bridge, dispatch/copy-back, state restoration, coherent commit |
| `analytical/runtime.hpp`, `shaders/` | Analytical history/resolve and native velocity replacements |
| `fsr3/`, `dlss/` | Method lifecycle and linear color/motion/output adapters; AMD host/backend and NGX integration |