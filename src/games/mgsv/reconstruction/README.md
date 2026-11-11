# MGSV Temporal Anti-Aliasing

Temporal reconstruction combines samples from several frames to smooth edges and recover detail. A small camera offset, called jitter, changes which parts of each edge are sampled. MGSV supports Analytical TAA at native resolution and AMD FSR 3.1.5 or NVIDIA DLSS at native or reduced rendering resolution. Upscaling runs before bloom preparation and depth of field; interface rendering uses the output resolution.

Upscaling Quality defaults to Auto for FSR and DLSS. Auto follows NVIDIA's [UI guidelines](https://github.com/NVIDIA/DLSS/blob/main/doc/RTX%20UI%20Developer%20Guidelines.pdf), using total output pixels: Native below 1920x1080, Quality through 2560x1440, Performance through 3840x2160, and Ultra Performance above that. This also handles ultrawide displays by pixel count. Auto selects a fixed mode for the current output size; it does not measure frame rate or run dynamic resolution. DLSS still queries its SDK for the selected mode's supported input size. Manual choices override Auto.

See the [addon guide](../README.md) for precision upgrades and build instructions, the [roadmap](ROADMAP.md) for unfinished work, and [FSR credits](fsr3/README.md) for SDK versions and licenses.

FSR and DLSS add `log2(actual render width / output width) - 1` to the game's pixel-sampler mip bias. This keeps finer texture detail available to reconstruction. Native adds -1; Off and Analytical retain the game's bias. The cache keeps one bias value at a time. Existing contexts and command lists keep their own sampler references when that cache changes. Materials that shimmer can be reviewed after gameplay testing.

## Code ownership

[runtime.hpp](runtime.hpp) manages the requested settings, the method that can actually run, and when reconstruction is inserted. [settings.hpp](settings.hpp) connects the controls to storage that remains valid for the lifetime of the addon. While DLSS starts, the requested setting stays DLSS but rendering uses Off. Saved settings load without automatic conversions.

The shared [input owner](../inputs.hpp) keeps references to native material and motion textures and selects one device and swapchain. Velocity precision upgrades work even with reconstruction Off. [input_capture.hpp](input_capture.hpp) selects the motion and camera data for reconstruction and checks that their frame, size, and texture layouts match.

[frame_inputs.hpp](frame_inputs.hpp) keeps the render dimensions separate from the output target. Each backend reports the result it produced, and the coordinator checks it before copying to that target. Native uses the existing scene target; reduced-resolution FSR and DLSS use a separate output. The shared preparation constants remain 96 bytes. The [state guard](../d3d11_compute_state.hpp) restores the game's graphics bindings after reconstruction.

[resolution.hpp](resolution.hpp) requests a lower scene size through the native renderer's normal update. Resource rebuilds happen when that request changes, never around individual draws. Native DoF helpers receive output-based rectangle and sprite-grid dimensions; physical blur terms are copied and converted without modifying the shared native view. Off and Analytical restore the engine's own size. Native hooks check the executable and expected code before installation. This implementation supports the native scatter DoF path; an unsupported DoF path requests recovery to native rendering.

[postprocess/resources.hpp](postprocess/resources.hpp) owns the output textures, matching views, depth/stencil copies, and cached intermediate images. [postprocess/runtime.hpp](postprocess/runtime.hpp) routes the native draws, copies, and clears to those images and restores their original bindings afterward. Encoded scene reads through either the native UNORM view or its float clone use the reconstructed output; native sRGB views retain their hardware decoding. Native 2D shaders are identified through their embedded source names; no external dump is needed at runtime. The helper shaders live beside these headers and share their 32-byte copy constants in [copy_constants.hlsli](postprocess/copy_constants.hlsli).

Source geometry depth is resampled separately from UI depth/stencil, accounting for current camera jitter. Native depth values and all eight stencil bits are retained. Copies, clears, scissors, viewports, and the relevant size fields in copied constants follow the output dimensions. DoF sprites read prepared color and masks in the vertex shader; those inputs follow the enlarged bake targets too. Queued outputs stay alive through submission. If reconstruction is canceled or replayed, the current native scene fills the output before the game's remaining commands run.

Bloom retains the native 246x138 working region inside its output-sized buffers. Cached size constants supply the matching reduction offsets, fullscreen-flare center and edge fade, and final bloom-copy crop. These size corrections are independent of the selected bloom type and add no shader dispatches.

Native lens flares include a depth-tested sprite pass (`0x93A0C8D8`). Its half-size color layer follows output resolution after its bound depth is matched to the current scene geometry. The native shader, typed views, depth test, and blend equations are retained. The layer's clear color and alpha are carried into its enlarged target, and the later blended copy uses that same layer. Native keeps the original route.

For resized draws, a bound hardware depth/stencil target is detached when both tests are disabled, then restored after the draw. Shader texture reads of scene depth remain intact. Active depth or stencil tests still require a qualified output-sized target; rejection logs identify the pass and its depth/stencil state.

[GT-VBGI](../gtvbgi/README.md) keeps separate GI history and uses the same command-submission bridge. Native motion and sun-camera data are tracked through their producer commands, including with reconstruction Off. GI advances history only at its own submitted insertion; it does not borrow scene reconstruction's previous frame. Keeping a texture alive does not prevent the game from overwriting its pixels.

## Usage and defaults

The **Temporal Anti-Aliasing** section contains the reconstruction mode, DLSS model, and upscaling quality. Tuning and diagnostic controls are under **Temporal Anti-Aliasing (Advanced)**, collapsed by default:

| Control | Behavior and default |
|---|---|
| **Temporal Reconstruction** | Off (Vanilla FXAA), Analytical TAA, AMD FSR 3.1.5, NVIDIA DLSS. New profiles default to FSR3. |
| **DLSS Model** | Presets A-F and J-M when supported by the DLL. Default/reset is F, saved as 6. Unsupported hints fall back to the DLL default. |
| **Upscaling Quality** | Auto, Native, Quality, Balance, Performance, Ultra Performance. Default Auto. Shown for FSR/DLSS on the supported executable. DLSS queries its supported input dimensions. |
| **TAA Jitter Pattern** | Analytical uses an eight-phase Halton pattern or diagnostic Off. FSR3/DLSS use Halton with a phase count based on the actual upscaling ratio. The saved Analytical choice stays independent. |
| **TAA Clip Tightness** | Controls the allowed range of history colors. Default `0.50`. |
| **TAA History Clip Strength** | Controls how strongly history is restricted to that range. Default `1.00`. |
| **TAA Current Frame Blend** | Maximum weight of the filtered current frame. Default `0.15`. |
| **Unclamp Motion Vectors** | Diagnostic, default Off. Also affects native motion blur. |
| **Projection Jitter Path sliders** | Ten Analytical controls, each ranging from `-2x` to `2x`, default/reset `1x`. Alternate Stream has no slider and stays disabled. |
| **TAA Diagnostic View / Velocity View Range / Object Motion Source** | Default Temporal Resolve, `8 px`, Auto-Corrected Native Object Velocity. |

These settings appear above the preset slider and remain independent of RenoDX presets. Off restores the original projection and FXAA. Changes that affect history restart accumulation; changing the visualization range alone does not. Weaker history clipping can reduce shimmer while increasing trails behind moving objects.

FSR uses approximately 2/3, 1/1.7, 1/2, or 1/3 of each output axis for Quality through Ultra Performance, rounded to even dimensions. At 3840x2160 output these are 2560x1440, 2258x1270, 1920x1080, and 1280x720. DLSS queries its own supported sizes. Both use `ceil(8 * (output width / actual render width)^2)` jitter phases, with a minimum of eight. Native keeps eight phases. The renderer updates the count when its actual size changes, before camera setup uses the new dimensions.

Geometry, lighting, and native screen-space reflections use the input size. DoF, motion-blur color work, grading, and UI use output-based sizes. GT-VBGI and Bend retain their relative resolution choices. Their pixel-based search reach is converted from output to render pixels so changing upscaling quality does not extend the search on screen.

A failed output allocation, unsupported native path, or repeatedly unsuccessful qualified dispatch requests native-size recovery. The requested quality remains saved, and the menu displays that upscaling is disabled. Select Native, then another quality mode to retry. Missing camera/motion observations during startup can recover normally. Changing quality rebuilds resources and restarts history when their dimensions change.

Generic CopyRenderBuffer draws must read a routed image at t0 before they can resize a destination; unused bound texture slots do not establish that relationship. Fixed-size UI, utility, and scene-thumbnail targets keep their own dimensions. Required output-sized DoF and motion-blur targets still request native recovery if they cannot be created or matched.

Bloom reductions, the bright pass, lens flares, blur, and the final bloom copy use the native fractions of output resolution. At 3840x2160 output, the quarter-axis bloom buffers stay 960x540 across quality modes. The [bloom module](../bloom/README.md) handles float resource upgrades and color conversions; reconstruction adjusts dimensions, offsets, and the final crop. These passes are selected through their routed color input, so unrelated uses of the same shaders keep their native targets.

MGSV reuses textures for different effects, and CPU workers can record the next scene before the current scene's post-processing is finished. A qualified bloom or first DoF draw starts a recording sequence that retains its output until display conversion. Each scene records its own intermediate roles, so another scene's use of the same texture cannot change that selection. Each unfinished command list keeps its route until FinishCommandList succeeds. Resource resets retire both kinds of ownership. Matching dimensions alone cannot qualify an unrelated image.

Native GPU order remains late tonemap, FXAA, then UI, even when another worker records UI first. Only texture slots read by the pixel shader are remapped. The Draw2D normal-blending, normal-encoding, and roughness-blending shaders prepare packed data and keep their native resources. Their textures can become scene-color storage later in the frame, so treating every Draw2D shader as UI caused the rain-related flicker.

Loading and save reload can draw UI without a new scene. UI draws and display conversion use the same matching output allocation in that case. Native color, depth, and stencil clears also reach the output targets, preserving backgrounds and UI clipping. The original clears still run, and no reconstruction dispatch or substitute geometry input is required.

The dedicated lens-flare layer keeps its mapping across frames because the game can record its scene copy before recording the sprite draws on another thread. Native clears follow their original command lists, including clears recorded before reconstruction publishes its output; a frame with no flares therefore keeps a neutral layer. In native DoF scopes, both pixel- and vertex-shader buffer-size constants use the enlarged grid, including its reciprocal dimensions, while native view values and unrelated fields remain intact.

Diagnostic UI is enabled by `ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS` and `ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS` in [state.hpp](state.hpp). FSR3/DLSS use fixed `1x` correction on the six original and four enabled experimental paths. Alternate Stream stays at `0x`, including builds without diagnostic UI. Without motion diagnostic UI, rendering uses Temporal Resolve, `8 px`, and corrected native object motion.

All methods save and restore compute, graphics, and render-target state, including depth-stencil state. FSR uses a `0.15` shading-change scale and a `1/3` accumulation increment.

## Frame and resource contracts

1. The main `SetViewMatrixState` hook records the camera's view and projection matrices before jitter, offsets the gameplay projection at `ShaderManager+0x680`, and records its frame, sample, dimensions, and source camera.
2. Other supported camera paths use that recorded camera data without advancing camera history.
3. `MotionBlurCameraVelocity` supplies depth, object motion, composed motion, and the matching camera data. All methods receive the same checked `ValidatedFrameInputs`.
4. Reconstruction runs before the completed scene is reduced for bloom (`0x72C1D157`). This requires the active scene-color clone role and matching depth/motion dimensions. The first DoF preparation (`0xFE1DC3F8`) remains a fallback, followed by a checked post-DoF copy, post-motion-blur-preparation copy, or late tonemap/LUT draw. Smaller and unrelated reduction inputs are skipped.
5. One resolve per frame writes the reconstructed scene back in MGSV's expected encoding and restores game state. Scheduling and post-processing routing share one draw dispatcher, which selects the destination before routing the first consuming draw. Camera history advances only if the result still matches the current reset counter and sample.
6. With reconstruction enabled, the FXAA shader skips its filter but keeps its other scene processing.

Inputs must match the device, dimensions, texture layout, presentation interval, and temporal sample. A callback can cross Present, so data from the immediately preceding interval is allowed only when the sample also matches. Older or mismatched data is rejected. Missing full-resolution work preserves history; a matching draw that fails at every insertion invalidates it. Creation, resize, relevant setting changes, and missing previous camera data restart accumulation.

All three methods run on the immediate D3D11 context. Deferred command lists are split around the insertion: the game work before reconstruction executes first, reconstruction and copy-back run next, then the remaining game work executes. Changing methods cancels obsolete reconstruction without dropping either part of the game's command list. CPU recording order and retained texture references alone do not prove which pixels the GPU will read.

| Signal | Required form |
|---|---|
| Scene color | Render-sized RGBA16F replacement texture containing MGSV's sRGB-encoded scene color; decode before combining light values. |
| Scene alpha | Preserve current-frame alpha for downstream DoF and highlights. |
| Depth | Render-sized reverse-Z from the velocity pass. Larger raw values are nearer. |
| Object/final velocity | Separate RGBA16F textures; native motion is packed in `.ba`, with the object mask in `.r`. |
| SDK inputs | Linear RGBA16F color and signed RG16F motion at render size; DLSS also receives an R32F reverse-Z depth copy. |
| Reconstruction output | Native uses the existing scene target. Reduced-resolution FSR/DLSS write a separate output before post-processing. Analytical requires equal render/output sizes. |
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

FSR and DLSS can produce a larger output from a smaller input. Shared output encoding maps current-frame alpha onto that output with clamped bilinear sampling; equal-size output preserves alpha exactly. Native renderer resizing and output routing follow the quality control. Isolated GPU checks cover unequal output sizes, odd edges, depth/stencil, constant ranges, scissors, copies, and fallback. Resampling depth does not recover geometry missing from the lower-resolution input.

DLSS requires a compatible `nvngx_dlss.dll` beside the game executable and NVIDIA's installed NGX core. The addon does not bundle that DLL. Startup checks the rendering GPU's vendor; other or unidentified vendors see a disabled option with an explanation. An NVIDIA GPU alone does not guarantee DLSS support.

- DLSS libraries are inspected and loaded only after explicit or saved DLSS selection. Opening the dropdown does not start DLSS. Initialization is requested at Present, outside draw callbacks.
- While loading, the setting stays DLSS but rendering uses Off. Selecting another method cancels the request. A support failure keeps an explanation and effective Off until restart instead of retrying every Present.
- The shared command-submission and query tracking starts at Present when any temporal method is selected. Analytical and FSR do not load NGX. Tracking remains active through method changes until device teardown.
- An existing deferred recording is usable only after a known successful `FinishCommandList` boundary, or when its context was created after tracking began. Reset, Present, or a failed finish does not prove that its GPU queries are safe to split.
- NGX runs between the preserved parts of the native command list on the immediate context. Method and reset changes must cancel only obsolete reconstruction, preserving the game's work.
- Leaving DLSS keeps one feature and texture set for reuse. Matching device, context, size, and model resume it with fresh history; incompatible requests recreate it. At 4K the retained adapter textures use about 253 MiB, plus NGX allocations. Switching does not retain another set each time.
- Teardown checks for GPU completion before releasing NGX resources. If completion or release cannot be confirmed within the allowed wait, retain the resources and disable DLSS.

DLSS uses the selected quality mode, automatic exposure, no sharpening, and the matching supported model hint. Model rejection is tracked separately for each quality mode. Submission, state, query, or NGX failures request a switch to Off. Inactive Analytical/FSR resources are released. Keeping DLSS resources across switches reduces teardown risk; it does not identify the original heap-corruption cause.

## Known limitations and validation status

Open rendering issues reported on November 11, 2026:

- ACC rain: rain appears inside the ACC when Upscaling Quality is not Native.
- Mother Base rain: occasional flashes occur with reconstruction enabled, including at Native quality.
- Sortie menu: the ground occasionally flashes black, apparently near the horizon. Whether this depends on reconstruction or upscaling quality is not yet known.

These symptoms remain unresolved. Their causes and whether they share a cause have not been established.

Isolated GPU and ReShade checks cover overlapping scene recordings, native post-processing at several scaling ratios, and UI display without a new scene, including color and stencil clears. Loading and save-reload gameplay confirmation remains pending. Other scenes, special views, display changes, and failure recovery still need coverage.

- Character wobble remains unexplained. Previous gameplay comparisons found no improvement from Analytical Jitter Off or Auto versus Direct object motion. Matrix Camera Everywhere caused too much ghosting to judge wobble. DLSS also showed wobble, although newer models hid it better. Motion ownership and submission changes have not established a fix.
- Previous readbacks did not prove that the same character's color, motion, geometry, and accumulated history matched. Correspondence between native previous bones/object transforms and the last accumulated frame remains unverified. Camera-only comparisons do not establish a native-pose cause.
- Thermography receives jitter at two native projection setters. Matched capture draws agree on clip positions, and the user reported clean NVG rendering with original shaders. The alternate thermography branch still needs gameplay coverage.
- Four enabled experimental paths need transition and gameplay checks. There is no proven native camera-cut signal; the removed clip-space heuristic incorrectly reset history every frame.
- FSR has no external masks/exposure input. Analytical has no previous-depth rejection or history locks. Thin detail, transparency, rain, bright effects, and newly exposed surfaces need continued testing. No method adds sharpening.

Earlier runtime checks established RGBA16F object/final velocity sampling and FSR3 operation on D3D11 Feature Level 11_0. They do not establish current character stability or validate the separate GT-VBGI accumulator.

## Manual validation

These checks cover scene reconstruction; the [GT-VBGI guide](../gtvbgi/README.md) describes its separate accumulator and filter. Stop MGSV before replacing its addon, following the [addon instructions](../README.md#building--deployment). Use one matching addon without live shader overrides and restart after replacement.

1. Check fresh settings: FSR3, Auto quality, DLSS model F, ten projection scales at `1x`, and diagnostic UI enabled. Presets must leave reconstruction selected. Off must restore FXAA and the original projection without a lasting shift.
2. Start Analytical and FSR without first selecting DLSS. Check that accumulation starts without NGX loading, restarts only when needed, and does not repeatedly reset in static menus or at smaller DoF draws.
3. Check static detail, animated characters, hair, wires, foliage, camera motion, binoculars, cuts, menus, pause/resume, bloom around bright lights, visible near/far DoF and focus changes, motion blur, rain, transparency, night vision, and size/device changes. Record wobble separately from ghost trails.
4. Check experimental projection paths against the actual render/depth grid, including unusual slider values, missing or unrelated cameras, UI boundaries, rapid setting changes, and normal exit.
5. Cycle all reconstruction modes. For DLSS, check saved and manual selection, cancellation, missing-DLL/non-NVIDIA explanations, pause/resume of retained resources, and model/size changes. Look for repeated history restarts, skipped reconstruction, query warnings, or submission failures.
6. Keep Unclamp Off for baseline gameplay. Any motion-source or clamp comparison must also check motion blur and animation trails. Previous Auto/Direct comparisons did not resolve wobble.
7. Check the settled waveform under Analytical, FSR3, and DLSS with stationary and moving cameras, menus, and method changes. Keep the older Preset M rail flicker separate from Alternate Stream instability.
8. Compare FSR Performance with Native. Check moving hair/foliage, wires, SSR, GT-VBGI reach, contact shadows, near/far DoF and highlights, motion blur, text sharpness, subtitles, UI clipping and menus. Repeat under DLSS, then test each quality mode and DLSS model switching. Check windowed/fullscreen transitions, resizing, cold startup, loading, special views, and Off/Analytical restoration. Input/output dimensions in the log must match the selected mode. Investigate the logged cause of an upscaling failure before retrying.

Scaled output adds depth preparation and output-sized intermediates as needed. Native UI depth is copied in one draw, then stencil is restored in eight masked draws. All copies share one dimensions/jitter update; the stencil bit selectors are fixed buffers reused across frames and size changes. Sampled geometry depth stays separate from UI depth. Pixel shaders that need their native view-to-buffer ratio converted use a small system-constant update dispatch; shaders without those constant reads skip it, and native DoF views that were already resized need only a buffer-size copy. A draw that needs several constant updates saves compute state once and restores it before the native draw. Native DoF radius limits and its first-bake cutoff follow output width. These costs reduce the saving from rendering fewer scene pixels. Size changes reuse helper shaders and fixed depth/stencil states. Leaving scaled mode retires textures while outstanding command lists retain the resources they still reference; device teardown also releases the pipeline cache. Dispatch recording and retained references do not prove GPU completion or scene correspondence.

Logs report startup, availability, selection, first camera data, resource creation, history restarts, pause/resume, disable, and the expected notice for a smaller insertion candidate. The backend's accumulation-start message reports `insertion=BeforeBloom` at the preferred point; later names identify native-resolution fallbacks. FSR/DLSS report render and output dimensions separately. They match under Native and differ under upscaling. Repeated restarts with unchanged settings or recurring input/dispatch warnings need investigation. Recording a dispatch does not prove the GPU completed it.

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
| [resolution.hpp](resolution.hpp) | Quality selection, native size requests, mip/jitter calculations, and DoF argument adaptation |
| [postprocess/resources.hpp](postprocess/resources.hpp), [postprocess/runtime.hpp](postprocess/runtime.hpp) | Output resources, native post-processing/UI routing, depth/stencil, and current-frame fallback |
| [mip_bias.hpp](mip_bias.hpp) | Cached pixel samplers with the scaling-based mip adjustment and restoration of native bindings |
| [analytical/](analytical), [motion/](../motion) | Analytical history/resolve and native velocity replacements |
| [fsr3/](fsr3), [dlss/](dlss) | Backend resources, lifecycle, and SDK integration |
