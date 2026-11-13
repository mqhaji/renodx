# MGSV temporal reconstruction

Temporal reconstruction combines samples from several frames to smooth edges and recover detail. Camera jitter changes which parts of each edge are sampled. Analytical TAA runs at native resolution; AMD FSR 3.1.5 and NVIDIA DLSS support native or reduced rendering resolution. Upscaling runs before bloom preparation and depth of field (DoF), with later scene processing and UI at output resolution.

The [addon guide](../README.md) owns build/deployment checks, the [architecture guide](../docs/ARCHITECTURE.md) covers shared ownership, the [roadmap](ROADMAP.md) tracks future work, and [FSR credits](fsr3/README.md) record SDK versions and licenses.

## Usage and defaults

The Temporal Anti-Aliasing section contains the method, DLSS model, and quality selectors. Diagnostic and tuning controls are in Temporal Anti-Aliasing (Advanced), collapsed by default. Reconstruction stays independent of RenoDX presets; Off restores the original projection and FXAA. Settings load without automatic conversions.

| Control | Behavior and default |
|---|---|
| Temporal Reconstruction | Off (Vanilla FXAA), Analytical TAA, AMD FSR 3.1.5, or NVIDIA DLSS; new profiles use FSR3 |
| DLSS Model | Presets A-F and J-M when supported by the DLL; F is default/reset, saved as 6; unavailable hints fall back to the DLL default |
| Upscaling Quality | Auto, Native, Quality, Balance, Performance, or Ultra Performance; default Auto; available for FSR/DLSS on the supported executable |
| TAA Jitter Pattern | Analytical uses an eight-phase Halton sequence or diagnostic Off; FSR/DLSS always use Halton with a phase count based on the render/output ratio |
| TAA Clip Tightness | Blends broad history bounds toward a tighter cross-shaped neighborhood; default `0.50` |
| TAA History Clip Strength | Restricts history to current color bounds; default `1.00` |
| TAA Current Frame Blend | Maximum filtered current-frame contribution; default `0.15` |
| Unclamp Motion Vectors | Diagnostic, default Off; also changes native motion blur |
| Projection Jitter Path sliders | Nine Analytical scales from `-2x` to `2x`, default/reset `1x`. Alternate Stream stays disabled |
| TAA Diagnostic View / Velocity View Range / Object Motion Source | Temporal Resolve, `8 px`, and Auto-Corrected Native Object Velocity |

Auto follows NVIDIA's [UI guidelines](https://github.com/NVIDIA/DLSS/blob/main/doc/RTX%20UI%20Developer%20Guidelines.pdf) by total output pixels: Native below 1920x1080, Quality through 2560x1440, Performance through 3840x2160, and Ultra Performance above that. This includes ultrawide displays. Auto does not measure frame rate or run dynamic resolution; manual choices override it, and DLSS queries its SDK for the selected mode's supported input size.

FSR uses approximately 2/3, 1/1.7, 1/2, or 1/3 of each output axis for Quality through Ultra Performance, rounded to even dimensions. At 3840x2160 output these are 2560x1440, 2258x1270, 1920x1080, and 1280x720. Both SDK methods use `ceil(8 * (output width / actual render width)^2)` jitter phases, with a minimum of eight. Native uses eight phases; Analytical and Off retain native sizing.

FSR/DLSS add `log2(actual render width / output width) - 1` to the native pixel-sampler mip bias. Native adds -1; Off and Analytical retain the game's bias. The cache keeps one bias value, while bound contexts and recorded command lists retain their own sampler references. Size/quality changes rebuild size-dependent resources and restart history; changing visualization range alone does not reset it.

Failed output allocation, an unsupported native path, or repeated failures with otherwise valid inputs request native-size recovery. The quality choice stays saved. Select Native, then another quality mode to retry. Missing startup camera/motion data can recover normally. Only the native scatter DoF path is supported; other DoF paths request recovery instead of resizing unknown work.

## Code ownership and output routing

[runtime.hpp](runtime.hpp) manages requested/effective methods and insertion. [settings.hpp](settings.hpp) binds controls to addon-owned storage. The shared [input owner](../rendering/inputs.hpp) selects one device/swapchain and retains native material/motion views; [input_capture.hpp](input_capture.hpp) checks reconstruction's camera, sample, size, and texture layout. Velocity precision remains active with reconstruction Off.

[Native view ownership](../rendering/native_context.hpp) follows each scene build into its worker jobs. Camera and motion records are selected by that scene and presentation generation. The coordinator carries the copied scope into immediate-context evaluation. Post-processing checks the same owner before choosing a recording route, a continuation on another command list, or the active output. Full-clear records carry the owner too. This prevents fixed-size rain work from selecting a main-scene route through a pooled texture or a reused worker thread.

The first Present selects the game's main swapchain and connects native ownership to it. Swapchain creation can happen before that selection. When the main view ends, its enclosing renderer retains the scene ID for final display conversion, which runs outside the view scope. Display completion releases that scene's recording sequence while other scenes keep their routes.

[frame_inputs.hpp](frame_inputs.hpp) separates render and output dimensions. [resolution.hpp](resolution.hpp) requests native renderer size changes through its normal update, not around individual draws, and adapts DoF rectangle/sprite-grid arguments. [mip_bias.hpp](mip_bias.hpp) owns replacement samplers. Native hooks check the executable and expected code before installation.

[postprocess/resources.hpp](postprocess/resources.hpp) owns enlarged scene/intermediate textures, typed views, and depth/stencil copies. [postprocess/runtime.hpp](postprocess/runtime.hpp) routes native draws/copies/clears and restores their bindings. Its helper shaders share [32-byte copy constants](postprocess/copy_constants.hlsli). Native source names and reflected texture/constant reads identify supported 2D shaders; no external dump is needed at runtime.

Geometry, lighting, and native SSR use render dimensions. DoF, motion-blur color work, grading, and UI use output dimensions; search reaches are converted from output to render pixels. Fixed-size UI, utility, and scene-thumbnail targets retain their own sizes. Generic CopyRenderBuffer must actually read a routed image at t0 before a destination can be resized; stale unused slots and matching dimensions are insufficient.

Source geometry depth is resampled separately from UI depth/stencil, including current jitter. Native depth and all eight stencil bits are retained. UI depth uses one fullscreen draw and stencil uses eight fixed masked draws. Native copies/clears, rectangles, viewports, scissors, and the size fields actually read by each shader follow output dimensions. Disabled hardware depth/stencil tests allow a DSV to be detached temporarily; active tests require a matching output-sized target. Sampled scene-depth reads remain separate.

Bloom keeps a 246x138 working region inside output-sized reduced buffers. Size constants adjust reduction offsets, flare center/fade, and the final crop independently of Bloom Type. At 4K, quarter-axis buffers remain 960x540 across quality modes. The [bloom guide](../postprocess/bloom/README.md) owns filtering/color behavior. One bloom image is reused for DoF controls, which keep their data range rather than receiving color conversions.

DoF sprites sample their enlarged bake inputs in both vertex and pixel stages. Radius limits and first-bake cutoffs follow output width. Native blur parameters are copied and scaled without changing the shared view. Scaling requires the active native graphics view to match the actual render dimensions; missing or differently sized views keep native parameters. Matching size alone does not identify a secondary camera of the same size. Shaders reading view-to-buffer ratios receive copied size fields, with a small compute adjustment where needed. Multiple adjustments share one state save; native views already at output size are not scaled twice.

The depth-tested flare-sprite pass `0x93A0C8D8` writes a separate half-size layer. Its depth is matched to current scene geometry while typed views, blend equations, native shader, clear color, and alpha are retained. That layer keeps its producer/consumer mapping across frames because another worker can record its scene copy before the flare draws. Frames without flares keep a neutral layer through native clears. Native quality retains the original route.

Each scene reserves its own output and intermediate roles. Workers recording the same scene share that allocation. Deferred contexts retain each output until successful `FinishCommandList` transfers those references to the native command list. This includes earlier draws whose display conversion has finished. Immediate-context calls rely on submission order and release their temporary references after issuing commands. Display recording releases the open sequence, while the pool keeps its scene identity and producer roles for UI recorded later on another worker. Those lookups share the existing image. The pool can assign a new owner after recording contexts, open sequences, and retained command lists release the allocation. Compatible spare allocations remain available for overlapping recordings. Free allocations with a different input layout are retired. D3D11 submission order keeps new GPU writes behind earlier submitted users. Resets retire the pool while outstanding lists retain their resources.

Full clears are recorded by native resource and scene identity. Known output targets receive clears at their original command position. The first qualified cropped or blended write can also recover a clear recorded before allocation. Output acquisition transfers earlier clears to the scene's producer, which retains them across display recording for parallel workers such as lens flares. Output reuse releases those records with the old producer. Startup scenes that never acquire output release their pending clears at display completion, preventing an ever-growing lookup table. Matching scene dimensions alone cannot allocate an enlarged intermediate. Loading UI can reserve scene storage through its qualified clear while ordinary post-processing waits for reconstruction admission.

Creating an enlarged target does not make its pixels valid. Draw routing checks targets, active depth/stencil, rectangles, and required constant buffers before clearing targets or recording intermediate roles. A rejected draw leaves earlier output pixels, producer roles, and native bindings unchanged.

When shader reads are known, the supported native constant layout must fit both the allocation and the bound D3D11.1 range. Missing buffers or insufficient ranges reject the enlarged route, including DoF sprite vertex constants. Constant preparation follows GPU command order. Replacement buffers are bound only after the whole draw is accepted. Allocation or range failure must not mix native dimensions with enlarged targets.

Each output allocation shares immutable size constants between intermediates at the same reduction level. The system and Kawase adjustment shaders copy their complete native prefixes into scratch storage while replacing the required fields. A single buffer copy installs each result. Larger allocations first preserve bytes outside that prefix. Every recorded adjustment still reads its source during GPU execution, including repeated command-list execution after the game changes the source buffer. Reusing a buffer address alone cannot establish that its contents are unchanged.

Only texture slots read by the pixel shader are remapped. Native order remains late tonemap, FXAA, then UI even when UI records first on another worker. Draw2D normal-blending, normal-encoding, and roughness-blending shaders produce packed data, not UI color. Treating all Draw2D shaders as UI previously redirected rain/material work incorrectly.

Generic Draw2D routing also requires one origin-aligned viewport covering the full render or output extent before any bindings change. The game uses the same shaders and pooled scene/depth textures to initialize fixed-size captures, including rain occlusion; those smaller views must keep their native color and stencil writes together. Scissor-clipped main-view UI still routes normally. Explicitly identified post-processing passes retain their own cropped-view rules.

Loading/save reload may draw UI without a new scene. UI and display conversion share the matching output allocation, and native color/depth/stencil clears reach it even when no reconstruction dispatch runs. The original clears still execute. Cancellation or replay fills output from current native scene data before the remaining game work continues.

## Frame and resource contracts

1. The main SetViewMatrixState hook reserves one jitter sample for the native scene, saves its unjittered matrices, and offsets gameplay projection at `ShaderManager+0x680`. Repeated setup and supported worker paths reuse the scene's saved sample. Recording the next scene reserves its next sample independently of reconstruction completion.
2. MotionBlurCameraVelocity supplies depth, object motion, composed motion, and matching camera data to every method through `ValidatedFrameInputs`.
3. The preferred insertion is before bloom reduction `0x72C1D157`, with the scene-color role and matching depth/motion. Fallbacks are first DoF preparation `0xFE1DC3F8`, checked post-DoF/post-motion-preparation copies, then late Tonemap/1DLUT. Smaller/unrelated inputs are skipped.
4. Immediately before evaluation, the dispatcher derives reprojection from the last successfully installed scene. A consecutive reservation with matching camera, viewport, dimensions, and generations can use history. A gap starts fresh history. Only successful evaluation and output installation advance the saved endpoint. Cached replay preserves native commands and refreshes scaled output through the spatial fallback without evaluating the same temporal observation twice.
5. Enabled reconstruction bypasses FXAA filtering but preserves the draw's other scene processing. Scene alpha remains current-frame data for downstream DoF/highlights.

Inputs must match native scene, presentation generation, device, dimensions, layout, and reset generation. Their saved jitter remains valid across Present and newer scene recording. Admission is tracked per scene so an overlapping scene cannot block another scene's insertion. An abandoned recording permits another candidate for its scene. An SDK evaluation that fails output installation invalidates backend history. Size, method, and relevant setting changes also restart accumulation.

[coordinator.hpp](coordinator.hpp) runs all methods on the immediate D3D11 context between preserved native command-list prefixes and remainders. Changing methods cancels obsolete reconstruction, not game commands. [GT-VBGI](../lighting/indirect/README.md) uses the same bridge but its own submitted camera/motion and history ledger, including with reconstruction Off. Retained objects and CPU recording order do not prove matching pixels or GPU completion.

| Signal | Required form |
|---|---|
| Scene color | Render-sized RGBA16F containing MGSV's encoded scene RGB; decode before light calculations |
| Alpha | Current-frame native alpha, preserved at equal size and clamped-bilinear resampled when output is larger |
| Depth | Render-sized reverse-Z from velocity composition; larger raw values are nearer |
| Object/composed velocity | Separate RGBA16F images; native motion is packed in `.ba`, with object mask in `.r` |
| SDK inputs | Linear RGBA16F color and signed RG16F motion; DLSS also uses an R32F reverse-Z depth copy |
| Output | Native reuses the existing scene target; reduced-resolution FSR/DLSS use a separate output-sized target; Analytical requires equal sizes |
| Camera history | Double-precision unjittered matrices, saved only after successful matching output |

Shared SDK preparation constants are 96 bytes. State restoration covers compute/graphics shaders, class instances, texture views, render targets, blending, depth/stencil, descriptors, and native constant-buffer ranges. FSR must preserve the same full state as the other methods.

## Native jitter ownership and lifetime

[projection_jitter.hpp](projection_jitter.hpp) accepts an enabled, full-resolution perspective viewport whose original projection matches the shader manager's copy. Setters change only `P[8] += 2*jitter_uv.x`, `P[9] -= 2*jitter_uv.y`, and required dirty flags. Path scales multiply these offsets; unrelated elements stay intact.

- Original paths are Velocity, Forward, Model, Alpha Model, Overlay Model, and Local Light. Local Light applies the scene's jitter to its completed draw packet before the game copies or queues it for workers. The publication lock covers camera validation and that write, then releases before the native call. Workers inherit the corrected projection through the game's packet ownership.
- Velocity, Thermography, Forward, Model, Alpha Model, Overlay Model, and Local Light validate the scene's saved camera, viewport, original projection/view matrices, dimensions, and reset generation. Later scene recording and Present leave that saved sample intact. One publication lock covers validation and the native write. Secondary cameras require their own qualified scene, and an already-offset projection receives no second offset.
- Experimental paths retain the same scene and reset identity through their temporary matrix edits. Lock order is transition, execution, then camera data. Release guards before native calls, allocation, or logging. A rejected nested call must not inherit another call's valid correction.
- Reject unrelated, stale, orthographic, nonfinite, already-offset, or ambiguous data. Deferred camera packets need their actual producer sample/reset association, not the latest matrix or a matching pointer.

Nine Analytical path controls default to `1x`. SDK methods use fixed `1x` on the six original and three enabled experimental paths. Sky rays and thermography follow the scene sample directly. Alternate Stream stays `0x` in all modes, even with diagnostic UI disabled. Diagnostic controls are selected by `ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS` and `ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS` in [state.hpp](state.hpp). Without motion UI, defaults remain Temporal Resolve, `8 px`, and corrected native motion.

### Experimental projection paths

Addresses below are hexadecimal offsets in the supported executable; verify them before using another build. `P` is projection and `VP` is combined view/projection.

| Control / saved key | Native route and restriction |
|---|---|
| Sun Volume / `FxTaaSunVolumeJitterScale` | `30AC50 -> 30B680`, return `30B07D`: private aligned VP copy for synchronous upload/draw |
| SH Lighting / `FxTaaShFallbackJitterScale` | Deferred producer `281EF0` attaches its owned packet at `283B90`, return `282A8B`. Render P at `+160` supplies both volume coverage and inverse-position evaluation. Immediate helper `27EF80`, return `1F2529`, offsets successful clip XY. |
| Terrain Decal / `FxTaaTerrainDecalJitterScale` | `275280 -> 2D18D0`, return `2755CA`: owned packet P at `+160`; affects inverse-plane reconstruction, not separate raster projection |
| Sky / follows scene sample | `1F50D0`, return `1F5990`, finishes inverse projection rows at parameter offset `+40`. Correct those rows before `306D10` uploads the sky constants. |
| Alternate Stream / disabled | Draw2D billboard/world-coordinate commands at `2E7330/2E4C70 -> 2D7AA0`, returns `2E7484/2E4DD7`: native constant `0x47` passes through unchanged |

Clip translation adds jitter times W to XY, preserving Z/W. The immediate SH helper validates every output before writing, with 8, 6, 18, or 8 vertices for kinds 0 through 3. Native packets retain their reference-count headers and owned storage. Constant `0x47` selects bank 7, vectors 4 through 7. The verified graphics flush at `2C5A10` binds that bank to D3D11 constant-buffer slot 7. Null-camera updates can leave old context matrices, so those matrices must not be changed permanently.

Deferred SH follows the scene through its producer scope. The attachment hook checks the packet's copied view and original projection, then corrects render P before either worker retains it. The sphere-map worker fills the light list. The draw worker at `280060` derives both its projected vertices through `27F450` and its inverse light-volume coordinates from that corrected P/V pair. Worker execution therefore carries the producer's sample even when another scene starts recording. The publication lock ends before the native attachment call.

Sky uses a separate inverse projection to turn fullscreen clip coordinates into world-space rays. The native builder stores inverse rows, and the constant upload copies them directly. The hook verifies that this inverse reverses the scene's original projection, then composes the inverse clip translation into its fourth column. The sky vertex shader receives rays that match the jittered raster projection. The correction validates the scene and reset generation again after the native builder returns.

Terrain decal evaluation reads render P/V from its retained packet, while `SetWorldMatrix` uses the worker command's own P/V for rasterization. The local probe records both pairs to check their agreement in actual draws. Native scatter geometry constants take camera-center offsets from camera `+118` and `+11C`. They describe the camera's projection center. The indirect-lighting temporal pass already removes current temporal jitter from its reconstruction coordinates, so changing these shared constants would also require changing that consumer. A later scene executed the terrain-decal worker at 1920x1080 input. Its packet and raster P/V matched exactly, and its scene and command node joined to executed NewTerrain_Decal draws on the same grid.

Analytical Alternate Stream `1x` reproduced settled waveform instability, and `0x` removed it. The native subsystem is Draw2D, which can switch cameras while handling billboard, world-coordinate, and glyph commands. The exact waveform primitive still needs identification before any selective correction. Keep this path disabled and investigate the separate Preset M rail flicker independently. Terrain alignment, sunlight coverage, and SH coverage still need broader gameplay checks.

The Local Light hook intercepts `RequestDrawAllLocalLightOnJob` at `2A43E0`, called from `PluginLocalLight::MainExec` with return address `20B8A3`. It verifies the packet's render P/V and viewport rectangle against the qualified scene, then offsets render P at full packet offset `+1A0`. The game's unshadowed-light jobs copy this context immediately. Its shadowed-light job retains the packet and copies the same context into per-light jobs later. Culling P/V and frustum planes keep their original values. The shared viewport stays read-only, and an already-offset packet fails the original-matrix check. MainExec's alternate early path only prepares and combines light buffers. The separate local-light selection worker at `2CB160` never reads its packet's VP, so it needs no correction.

Camera hooks remain installed for the process lifetime. Device destruction stops work but does not unpatch hooks or free trampolines; recreation reuses them. Install at device initialization, never from a slider callback. Restart MGSV before replacing/removing the addon. Do not wait for callbacks under a lock they need, and do not assume Off/reset can cancel already submitted work.

## Motion and reconstruction

Native `.ba` decodes to `(ba*2-1)*64/render_size`, a current-minus-previous UV delta. Analytical reads history at `output_uv - velocity`; FSR/DLSS preparation converts to their current-to-previous convention. When a neighboring pixel supplies motion, apply its motion to the output pixel's UV. Camera motion comes from depth and unjittered matrices; object motion retains bone movement with current projection jitter removed.

Analytical selects the nearest depth from the center and four diagonals, carrying velocity, mask, depth, and UV together; the last equal-depth tap wins. FSR/DLSS use the center. Analytical decodes current filtering samples and all sixteen Catmull-Rom history taps before linear-light filtering, then stores encoded history. It has no previous-depth rejection or history locks.

Object Motion Source offers Auto, Matrix Camera Everywhere, jitter-sign comparisons, Direct Object Velocity (6), and Auto Center Pixel (7). Direct Object bypasses composition at the selected pixel; Auto Center removes neighbor dilation only. Matrix Camera Everywhere removes deformation motion and produces animation trails. These diagnostics have not fixed character wobble.

The three velocity ports preserve native packing and alpha tests. Unclamp Motion Vectors bypasses the roughly 64-pixel radial limit only with temporal reconstruction active. Leave it Off for baseline comparisons because native motion blur consumes the same changed signal.

## SDK lifecycle

FSR follows AMD's 3.1.5 pass schedule through the local D3D11/Shader Model 5 backend, not an AMD-provided D3D11 integration. It has no external reactive/transparency/exposure texture; internal shading-change/reactivity analysis remains active. `preExposure = 1`, sharpening and external mask generation are disabled, shading-change scale is `0.15`, and accumulation increment is `1/3`. See [credits and licenses](fsr3/README.md).

DLSS needs a compatible `nvngx_dlss.dll` beside the game executable and NVIDIA's installed NGX core. The addon does not bundle it. Unidentified/non-NVIDIA adapters show an explanation; NVIDIA hardware alone does not establish support.

- Inspect/load DLSS only after explicit or saved selection. Initialize at Present, outside draws. While loading, keep the requested DLSS setting but render with Off. Cancellation preserves game work; support failure stays explained until restart rather than retrying every Present.
- Shared submission/query tracking starts at Present when a temporal method is selected and remains through method switches. Analytical/FSR do not load NGX. A preexisting deferred recording requires a known successful FinishCommandList, or a context created after tracking began; reset/Present/failed Finish is insufficient.
- NGX runs on the immediate context between native prefixes/remainders. It can submit work during creation, before the first preparation pass. Submission/state/query/NGX failures request Off; model rejection is tracked separately per quality mode. DLSS uses automatic exposure and no sharpening.
- Leaving DLSS retains one feature/texture set. Matching device, context, size, and model resume with fresh history; incompatible requests recreate it. At 4K, adapter textures use about 253 MiB plus NGX memory. Inactive Analytical/FSR resources are released.
- Teardown requires completed GPU work before releasing NGX. If completion/release cannot be confirmed within the bounded wait, retain resources and disable DLSS. Reuse reduces teardown risk but does not establish the original heap-corruption cause.

## Known limitations and validation status

Live checks with DLSS at 1920×1080 input and 3840×2160 output passed in the ACC/main menu, a loading screen, sheltered rain, and a paused wormhole effect. Captures verified that the 512×512 rain utility view kept its native size while the main scene used enlarged post-processing targets. They also verified the utility capture's reads by rain materials, deferred composition, and particles, plus the main scene's continuation into final display conversion.

An occasional flash around the transition into rain remains unresolved. The user reported improved stability and could not reproduce the flash during the final walking test. Earlier small image samples contain brief brightness changes before DLSS, but their timing could not be matched confidently to the reported flash. Rain inside the ACC, sortie ground flashes, death/save reload, display changes, and special-view transitions still need targeted checks.

The latest changes make jitter reservations, history advancement, and output reuse follow native scenes. The user confirmed a normal ACC image with DLSS Performance rendering at 1920x1080 and reconstructing to 3840x2160. Output-routing messages stopped after warmup and stayed unchanged during a two-minute observation, with no new reconstruction or allocation failures. Loading and weather transitions still need checks on this build. Offline tests cover overlapping scenes, repeated setup across Present, committed history, failed output installation, retained command-list replay, loading UI, and reuse after several recordings finish together. Tracking the actual submitted writers remains necessary when investigating a flash, and sortie ground stability also depends on forward geometry using the same camera jitter as scene rendering and reconstruction.

The local-light packet hook passed the user's ACC check with a normal image, aligned lights, and normal performance on a fresh saved-DLSS-Auto launch. Its log confirms DLSS evaluation before bloom at 1920x1080 input and 3840x2160 output, flare-layer composition, and AO/GI history submission. The final test also caught a clear record being retired before the parallel flare worker needed it. Keeping those records with the retained scene output fixed the resulting upscaling shutdown. During the follow-up observation, output-routing reports stopped after warmup and no upscaling failure recurred. Startup scenes still release pending clear records, preserving the startup performance fix. Weather transitions and special views still need checks with this change.

The deferred SH and sky checkpoint passed the user's ACC and Mother Base image checks with DLSS, including a Mother Base switch from 1920x1080 input to Native 3840x2160. Consumer records matched each scene's SH projection to its combined VP and corrected sky inverse. The recorded SH blend/occlusion and sky draws retained the same scene and command-node identity through executed lists. SH color and depth/stencil targets used half-size grids on each axis, while sampled depth retained the scene dimensions. Sky used the scene grid. Captures reported no ownership, matrix, or list-finish failures. A follow-up scene confirmed exact terrain packet/raster matrix agreement at reduced input size. All observed SH volumes were kind 0. Other kinds, terrain at Native input size, and special views still need live coverage. No performance gain is claimed by this alignment change.

Controlled GPU/ReShade tests cover overlapping recordings, output-sized native processing, odd dimensions, depth/stencil, ranges, copies, fallbacks, and UI/clears without a new scene. They also reproduce display recording finishing before flare recording and verify the enlarged layer's clear color and alpha by GPU readback. Gameplay verification remains necessary for geometry lost at lower resolution and for transitions between these paths.

- Character wobble remains unexplained. Analytical Jitter Off and Auto/Direct motion comparisons did not help. Matrix Camera Everywhere caused too much ghosting to judge it. DLSS also showed wobble, although newer models hid it better. Readbacks have not proved matching current/previous character poses and accumulated color.
- Thermography has matching clip-position evidence at two guarded setters, and original-shader NVG looked clean in the reported test. The alternate branch and three enabled experimental projection paths still need transition/gameplay coverage.
- There is no proven native camera-cut signal. The removed clip-space heuristic reset history continuously. FSR lacks external masks/exposure; Analytical lacks previous-depth rejection/history locks. Thin detail, transparency, rain, bright effects, and newly exposed surfaces remain difficult. No method adds sharpening.
- Earlier RGBA16F velocity and FSR FL11_0 checks establish those inspected paths, not current character stability or the separate GT-VBGI accumulator. See the [roadmap](ROADMAP.md) for investigation requirements.

## Manual validation

Follow the [addon deployment instructions](../README.md#build-and-deploy), restart with one matching addon, and disable live shader overrides. These checks cover scene reconstruction; the [lighting guide](../lighting/indirect/README.md#build-and-gameplay-checks) covers its separate history/filtering.

1. Check fresh defaults with FSR3, Auto, DLSS F, nine Analytical scales at `1x`, and diagnostic UI. Presets must not change reconstruction, and Off must restore FXAA/projection without a lasting shift.
2. Start Analytical/FSR without selecting DLSS. Check no NGX load, successful accumulation, and no repeated resets in static menus or smaller DoF draws.
3. Compare static detail, characters, hair, foliage, wires, camera motion, binoculars, cuts, menus, pause/resume, fog/rain, bright moving lights, SSR, skin, DoF, motion blur, and night vision. Separate wobble from ghost trails; keep Unclamp Off for baseline tests.
4. Exercise every method/quality and DLSS model switch. Launch with DLSS Auto saved and check performance before changing quality. Test cancellation, missing-DLL/unsupported-device explanations, suspension/resume, resizing, window/fullscreen changes, device lifecycle, loading/save reload, and Off/Analytical restoration. Check UI clipping, subtitles, and scene alpha.
5. Compare FSR Performance and Native, then DLSS qualities. Check render/output dimensions, depth edges, near/far DoF, flare-layer clearing, bloom crop, motion blur, contact-shadow/GI reach, and the reported ACC/Mother Base/sortie problems.
6. Test experimental path scales and rejected/nested/foreign-camera cases separately. Verify a stable settled waveform with Analytical/FSR/DLSS; keep Preset M rail flicker separate from Alternate Stream behavior. Do not repeat inconclusive Auto/Direct sweeps as evidence of a fix.
7. Read the fresh log for allocation/input/query/state/NGX failures and repeated history resets. `insertion=BeforeBloom` identifies the preferred point; fallback names identify later opportunities. Render/output sizes match at Native and differ under upscaling. A recorded dispatch does not prove GPU completion or visual correctness.
