# MGSV Temporal Anti-Aliasing

Temporal reconstruction combines samples from several frames to smooth edges and recover detail. A small camera offset, called jitter, changes which parts of each edge are sampled. MGSV supports Analytical TAA, AMD FSR 3.1.5, and NVIDIA DLSS at the game's render resolution.

See the [addon guide](../README.md) for precision upgrades and build instructions, the [roadmap](ROADMAP.md) for unfinished work, and [FSR credits](fsr3/README.md) for SDK versions and licenses.

## Code ownership

[runtime.hpp](runtime.hpp) manages the requested settings, the method that can actually run, and when reconstruction is inserted. [settings.hpp](settings.hpp) connects the controls to storage that remains valid for the lifetime of the addon. While DLSS starts, the requested setting stays DLSS but rendering uses Off. Saved settings load without automatic conversions.

The shared [input owner](../inputs.hpp) keeps references to native material and motion textures and selects one device and swapchain. Velocity precision upgrades work even with reconstruction Off. [input_capture.hpp](input_capture.hpp) selects the motion and camera data for reconstruction and checks that their frame, size, and texture layouts match.

[frame_inputs.hpp](frame_inputs.hpp) defines the common inputs, outputs, and 96-byte preparation constants. Each backend converts color and motion into its required form. The [state guard](../d3d11_compute_state.hpp) restores the game's graphics bindings after reconstruction.

[GT-VBGI](../gtvbgi/README.md) currently has no separate temporal history and does not request motion. Keeping a texture alive does not prevent the game from overwriting its pixels. Future GI accumulation needs separate proof that motion, geometry, lighting, and history belong together.

## Usage and defaults

The **Temporal Anti-Aliasing** section contains these controls:

| Control | Behavior and default |
|---|---|
| **Temporal Reconstruction** | Off (Vanilla FXAA), Analytical TAA, AMD FSR 3.1.5, NVIDIA DLSS. New profiles default to FSR3. |
| **DLSS Model** | Presets A-F and J-M when supported by the DLL. Default/reset is F, saved as 6. Unsupported hints fall back to the DLL default. |
| **TAA Jitter Pattern** | Analytical uses an eight-phase Halton pattern or diagnostic Off. FSR3/DLSS always use Halton without changing the saved Analytical choice. |
| **TAA Clip Tightness** | Controls the allowed range of history colors. Default `0.50`. |
| **TAA History Clip Strength** | Controls how strongly history is restricted to that range. Default `1.00`. |
| **TAA Current Frame Blend** | Maximum weight of the filtered current frame. Default `0.15`. |
| **Unclamp Motion Vectors** | Diagnostic, default Off. Also affects native motion blur. |
| **Projection Jitter Path sliders** | Ten Analytical controls, each ranging from `-2x` to `2x`, default/reset `1x`. Alternate Stream has no slider and stays disabled. |
| **TAA Diagnostic View / Velocity View Range / Object Motion Source** | Default Temporal Resolve, `8 px`, Auto-Corrected Native Object Velocity. |

These settings appear above the preset slider and remain independent of RenoDX presets. Off restores the original projection and FXAA. Changes that affect history restart accumulation; changing the visualization range alone does not. Weaker history clipping can reduce shimmer while increasing trails behind moving objects.

Diagnostic UI is enabled by `ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS` and `ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS` in [state.hpp](state.hpp). FSR3/DLSS use fixed `1x` correction on the six original and four enabled experimental paths. Alternate Stream stays at `0x`, including builds without diagnostic UI. Without motion diagnostic UI, rendering uses Temporal Resolve, `8 px`, and corrected native object motion.

All methods save and restore compute, graphics, and render-target state, including depth-stencil state. FSR uses a `0.15` shading-change scale and a `1/3` accumulation increment.

## Frame and resource contracts

1. The main `SetViewMatrixState` hook records the camera's view and projection matrices before jitter, offsets the gameplay projection at `ShaderManager+0x680`, and records its frame, sample, dimensions, and source camera.
2. Other supported camera paths use that recorded camera data without advancing camera history.
3. `MotionBlurCameraVelocity` supplies depth, object motion, composed motion, and the matching camera data. All methods receive the same checked `ValidatedFrameInputs`.
4. Reconstruction runs at one suitable insertion: preferably full-resolution `DOF_ScatterBakeFirst` (`0xFE1DC3F8`), otherwise a checked post-DoF copy, post-motion-blur-preparation copy, or late tonemap/LUT draw. Lower-resolution candidates are skipped.
5. One resolve per frame writes the reconstructed scene back in MGSV's expected encoding and restores game state. Camera history advances only if the result still matches the current reset counter and sample.
6. With reconstruction enabled, the FXAA shader skips its filter but keeps its other scene processing.

Inputs must match the device, dimensions, texture layout, presentation interval, and temporal sample. A callback can cross Present, so data from the immediately preceding interval is allowed only when the sample also matches. Older or mismatched data is rejected. Missing full-resolution work preserves history; a matching draw that fails at every insertion invalidates it. Creation, resize, relevant setting changes, and missing previous camera data restart accumulation.

All three methods run on the immediate D3D11 context. Deferred command lists are split around the insertion: the game work before reconstruction executes first, reconstruction and copy-back run next, then the remaining game work executes. Changing methods cancels obsolete reconstruction without dropping either part of the game's command list. CPU recording order and retained texture references alone do not prove which pixels the GPU will read.

| Signal | Required form |
|---|---|
| Scene color | Full-resolution RGBA16F replacement texture containing MGSV's sRGB-encoded scene color; decode before combining light values. |
| Scene alpha | Preserve current-frame alpha for downstream DoF and highlights. |
| Depth | Full-resolution reverse-Z from the velocity pass. Larger raw values are nearer. |
| Object/final velocity | Separate RGBA16F textures; native motion is packed in `.ba`, with the object mask in `.r`. |
| SDK inputs | Linear RGBA16F color and signed RG16F motion; DLSS also receives an R32F reverse-Z depth copy. Render and output sizes are equal. |
| Camera history | Double-precision matrices without jitter, saved only after a successful matching result. |

State restoration includes compute shaders and their bindings, graphics texture inputs, render targets, depth/stencil, blending, and tracked compute descriptors. FSR must keep the same full state preservation as the other methods.

## Native jitter ownership and lifetime

[projection_jitter.hpp](projection_jitter.hpp) checks the executable and expected call sites before installing camera hooks. The main viewport must be enabled, full-resolution, and perspective, with a valid camera. Its original projection must match the shader manager's copy.

- Projection setters change only elements 8/9 and their dirty flags: `P[8] += 2*jitter_uv.x`, `P[9] -= 2*jitter_uv.y`, multiplied by that path's scale.
- The six original paths are Velocity, Forward, Model, Alpha Model, Overlay Model, and Local Light. The local-light callback temporarily changes its source viewport, then restores all sixteen projection values. The main viewport projection stays unchanged.
- Velocity and Thermography may use the preceding Present interval only when the camera, viewport, dimensions, exact original matrices, reset counter, and sample match. Their camera-data lock covers both the checks and the write.
- Experimental paths save the matrices, sequence, reset counter, and sample for their own call. Final writes acquire the execution guard before the camera-data guard and check the data again. Mode/reset changes acquire locks in this order: transition, execution, camera data. Release guards before native calls, allocation, or logging.
- Reject missing, unrelated, orthographic, nonfinite, already-offset, or ambiguous camera data. A rejected nested call must not inherit a valid outer call's correction. Temporary data must stay within its synchronous call and thread.
- Deferred camera packets need a proven link between the producer and consumer's sample and reset counter. A matching pointer or latest matrix is insufficient.

**Restart MGSV to replace or remove the addon.** Camera hooks and the addon module stay loaded for the process lifetime. Device destruction stops new work without removing hooks; recreation reuses them. Install hooks during device initialization, never from a slider callback. Do not wait for callbacks while holding a lock they need. Off or Reset cannot undo work already recorded or submitted, so rapid transitions still need gameplay checks.

### Experimental projection paths

The four enabled experimental paths use `1x` by default for Analytical and fixed `1x` for FSR3/DLSS. They still need gameplay coverage. Alternate Stream is disabled for every method. Addresses and offsets below are hexadecimal; check the executable before using them. `P` means projection and `VP` means combined view/projection.

| Control / saved key | Correction and restriction |
|---|---|
| Sun Volume / `FxTaaSunVolumeJitterScale` | `30AC50 -> 30B680`, return `30B07D`: private aligned VP copy for synchronous volume upload/draw. |
| SH Fallback / `FxTaaShFallbackJitterScale` | `27EF80`, return `1F2529`: successful output XY only; kinds 0..3 use checked 8/6/18/8 vertex counts. Preserve Z/W, visibility, and depth bounds. |
| Terrain Decal / `FxTaaTerrainDecalJitterScale` | `275280 -> 2D18D0`, return `2755CA`: native-owned packet P at `+160` before attachment; affects inverse-plane reconstruction, not the separately captured raster projection. |
| General Light Packet / `FxTaaLightPacketJitterScale` | `2A4C00 -> 2D3380`, return `2A5342`: native-owned VP at `+130` before attachment; preserve clip-W and selection data. |
| Alternate Stream / disabled | `2E7330/2E4C70 -> 2D7AA0`, returns `2E7484/2E4DD7`: native constant `0x47` passes through unchanged. Hook lifetime is unchanged; there is no slider or setting field. |

The user reproduced waveform instability with Analytical Alternate Stream at `1x`; `0x` removed it after accumulation settled. That supports disabling this path. The underlying mismatch and separate Preset M rail flicker remain unresolved, and the all-method disable still needs fresh gameplay validation.

Clip translation adds the jitter offset times W to XY while preserving Z/W. SH checks that the original operation succeeded and its output kind, bounds, and values are valid. Packets must keep native-owned storage, including their reference-count headers. Alternate constant `0x47` identifies bank 7, vectors 4..7; it is not D3D constant buffer `b7`. A null-camera update can leave old context matrices behind, so those matrices must not be changed permanently.

The deferred SH packet route (`27F450`) remains disabled because ownership between its producer and consumer is unresolved. Actual draw-grid alignment, terrain reconstruction/raster agreement, and final general-light consumers still need verification. Alternate Stream must stay disabled until its world/UI consumers and waveform stability are understood.

## Motion and reconstruction

Native `.ba` decodes to `(ba*2-1)*64/render_size`, giving current-minus-previous UV motion. History is read at `output_uv - velocity`. When a neighboring pixel supplies motion, use that motion to offset the output pixel's UV. Background motion comes from depth and camera matrices without jitter. Object motion retains native bone movement, with the configured current projection jitter removed.

Analytical chooses the nearest depth from the center and four diagonal pixels. It moves the mask, motion, depth, and UV together; the last equal-depth tap wins. FSR3/DLSS read the center pixel. Analytical decodes current-frame filtering samples and all sixteen Catmull-Rom history taps before combining them in linear light, then stores encoded history. It has no previous-depth rejection or history locks.

Object Motion Source includes Auto, Matrix Camera Everywhere, jitter-sign diagnostics, Direct Object Velocity (6), and Auto Center Pixel (7). Direct Object Velocity bypasses the composite at the selected pixel. Auto Center Pixel disables the neighbor search. Matrix Camera Everywhere drops bone/deformation motion and causes animation trails; these modes are diagnostic comparisons rather than fixes for character wobble.

The three velocity replacements preserve native encoding and alpha tests. **Unclamp Motion Vectors** bypasses the roughly 64-pixel radial limit only while temporal reconstruction is enabled. Keep it Off for baseline comparisons because it also affects native motion blur.

## SDK lifecycle

FSR3 follows AMD's 3.1.5 host schedule using a local D3D11/Shader Model 5 backend. It has no external reactive, transparency, or exposure texture; its internal analysis still identifies changing shading and newly visible surfaces. `preExposure = 1`. Sharpening and the separate mask-generation API are disabled. AMD does not provide this D3D11 integration; see [credits and licenses](fsr3/README.md).

DLSS requires a compatible `nvngx_dlss.dll` beside the game executable and NVIDIA's installed NGX core. The addon does not bundle that DLL. Startup checks the rendering GPU's vendor; other or unidentified vendors see a disabled option with an explanation. An NVIDIA GPU alone does not guarantee DLSS support.

- DLSS libraries are inspected and loaded only after explicit or saved DLSS selection. Opening the dropdown does not start DLSS. Initialization is requested at Present, outside draw callbacks.
- While loading, the setting stays DLSS but rendering uses Off. Selecting another method cancels the request. A support failure keeps an explanation and effective Off until restart instead of retrying every Present.
- The shared command-submission and query tracking starts at Present when any temporal method is selected. Analytical and FSR do not load NGX. Tracking remains active through method changes until device teardown.
- An existing deferred recording is usable only after a known successful `FinishCommandList` boundary, or when its context was created after tracking began. Reset, Present, or a failed finish does not prove that its GPU queries are safe to split.
- NGX runs between the preserved parts of the native command list on the immediate context. Method and reset changes must cancel only obsolete reconstruction, preserving the game's work.
- Leaving DLSS keeps one feature and texture set for reuse. Matching device, context, size, and model resume it with fresh history; incompatible requests recreate it. At 4K the retained adapter textures use about 253 MiB, plus NGX allocations. Switching does not retain another set each time.
- Teardown checks for GPU completion before releasing NGX resources. If completion or release cannot be confirmed within the allowed wait, retain the resources and disable DLSS.

DLSS uses native-resolution quality, automatic exposure, no sharpening, and the supported model hint. Submission, state, query, or NGX failures request a switch to Off. Inactive Analytical/FSR resources are released. Keeping DLSS resources across switches reduces teardown risk; it does not identify the original heap-corruption cause.

## Known limitations and validation status

- Character wobble remains unexplained. Previous gameplay comparisons found no improvement from Analytical Jitter Off or Auto versus Direct object motion. Matrix Camera Everywhere caused too much ghosting to judge wobble. DLSS also showed wobble, although newer models hid it better. Motion ownership and submission changes have not established a fix.
- Previous readbacks did not prove that the same character's color, motion, geometry, and accumulated history matched. Correspondence between native previous bones/object transforms and the last accumulated frame remains unverified. Camera-only comparisons do not establish a native-pose cause.
- Thermography receives jitter at two native projection setters. Matched capture draws agree on clip positions, and the user reported clean NVG rendering with original shaders. The alternate thermography branch still needs gameplay coverage.
- Four enabled experimental paths need transition and gameplay checks. There is no proven native camera-cut signal; the removed clip-space heuristic incorrectly reset history every frame.
- FSR has no external masks/exposure input. Analytical has no previous-depth rejection or history locks. Thin detail, transparency, rain, bright effects, and newly exposed surfaces need continued testing. No method adds sharpening.

Earlier runtime checks established RGBA16F object/final velocity sampling and FSR3 operation on D3D11 Feature Level 11_0. They do not establish current character stability or validate a future GT-VBGI accumulator.

## Manual validation

These checks cover scene reconstruction. GT-VBGI currently has no dedicated temporal accumulator. Build or replace DLLs only with MGSV stopped, following the [addon instructions](../README.md#building--deployment). Use one matching addon without live shader overrides and restart after replacement.

1. Check fresh settings: FSR3, DLSS model F, ten projection scales at `1x`, diagnostic UI enabled, and no Alternate Stream slider. Presets must leave reconstruction selected. Off must restore FXAA and the original projection without a lasting shift.
2. Start Analytical and FSR without first selecting DLSS. Check that accumulation starts without NGX loading, restarts only when needed, and does not repeatedly reset in static menus or at smaller DoF draws.
3. Check static detail, animated characters, hair, wires, foliage, camera motion, binoculars, cuts, menus, pause/resume, DoF, motion blur, rain, transparency, night vision, and size/device changes. Record wobble separately from ghost trails.
4. Check experimental projection paths against the actual render/depth grid, including unusual slider values, missing or unrelated cameras, UI boundaries, rapid setting changes, and normal exit.
5. Cycle all reconstruction modes. For DLSS, check saved and manual selection, cancellation, missing-DLL/non-NVIDIA explanations, pause/resume of retained resources, and model/size changes. Look for repeated history restarts, skipped reconstruction, query warnings, or submission failures.
6. Keep Unclamp Off for baseline gameplay. Any motion-source or clamp comparison must also check motion blur and animation trails. Previous Auto/Direct comparisons did not resolve wobble.
7. Check the settled waveform under Analytical, FSR3, and DLSS with stationary and moving cameras, menus, and method changes. Keep the older Preset M rail flicker separate from Alternate Stream instability.

Logs report startup, availability, selection, first camera data, resource creation, history restarts, pause/resume, disable, and the expected notice for a smaller insertion candidate. Repeated restarts with unchanged settings or recurring input/dispatch warnings need investigation. Recording a dispatch does not prove the GPU completed it.

## Source map

| Files | Responsibility |
|---|---|
| [runtime.hpp](runtime.hpp), [settings.hpp](settings.hpp) | Requested settings, active method, controls, draw routing, startup, and teardown |
| [dlss/model.hpp](dlss/model.hpp) | Model names, defaults, and lookup |
| [state.hpp](state.hpp), [camera_state.hpp](camera_state.hpp), [projection_jitter.hpp](projection_jitter.hpp) | Samples, resets, camera history, and projection offsets |
| [inputs.hpp](../inputs.hpp), [input_capture.hpp](input_capture.hpp) | Shared native texture references and the matching reconstruction inputs |
| [frame_inputs.hpp](frame_inputs.hpp), [prepare_inputs.hlsli](prepare_inputs.hlsli), [encode_output.hlsli](encode_output.hlsli) | Common input/output data and color/motion conversions |
| [coordinator.hpp](coordinator.hpp), [d3d11_compute_state.hpp](../d3d11_compute_state.hpp) | Command ordering, GPU queries, copy-back, state restoration, and camera-history updates |
| [logging.hpp](logging.hpp) | Reconstruction logs |
| [analytical/](analytical), [motion/](../motion) | Analytical history/resolve and native velocity replacements |
| [fsr3/](fsr3), [dlss/](dlss) | Backend resources, lifecycle, and SDK integration |
