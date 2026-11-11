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
| Projection Jitter Path sliders | Ten Analytical scales from `-2x` to `2x`, default/reset `1x`; Alternate Stream has no slider and stays disabled |
| TAA Diagnostic View / Velocity View Range / Object Motion Source | Temporal Resolve, `8 px`, and Auto-Corrected Native Object Velocity |

Auto follows NVIDIA's [UI guidelines](https://github.com/NVIDIA/DLSS/blob/main/doc/RTX%20UI%20Developer%20Guidelines.pdf) by total output pixels: Native below 1920x1080, Quality through 2560x1440, Performance through 3840x2160, and Ultra Performance above that. This includes ultrawide displays. Auto does not measure frame rate or run dynamic resolution; manual choices override it, and DLSS queries its SDK for the selected mode's supported input size.

FSR uses approximately 2/3, 1/1.7, 1/2, or 1/3 of each output axis for Quality through Ultra Performance, rounded to even dimensions. At 3840x2160 output these are 2560x1440, 2258x1270, 1920x1080, and 1280x720. Both SDK methods use `ceil(8 * (output width / actual render width)^2)` jitter phases, with a minimum of eight. Native uses eight phases; Analytical and Off retain native sizing.

FSR/DLSS add `log2(actual render width / output width) - 1` to the native pixel-sampler mip bias. Native adds -1; Off and Analytical retain the game's bias. The cache keeps one bias value, while bound contexts and recorded command lists retain their own sampler references. Size/quality changes rebuild size-dependent resources and restart history; changing visualization range alone does not reset it.

Failed output allocation, an unsupported native path, or repeated failures with otherwise valid inputs request native-size recovery. The quality choice stays saved. Select Native, then another quality mode to retry. Missing startup camera/motion data can recover normally. Only the native scatter DoF path is supported; other DoF paths request recovery instead of resizing unknown work.

## Code ownership and output routing

[runtime.hpp](runtime.hpp) manages requested/effective methods and insertion. [settings.hpp](settings.hpp) binds controls to addon-owned storage. The shared [input owner](../rendering/inputs.hpp) selects one device/swapchain and retains native material/motion views; [input_capture.hpp](input_capture.hpp) checks reconstruction's camera, sample, size, and texture layout. Velocity precision remains active with reconstruction Off.

[frame_inputs.hpp](frame_inputs.hpp) separates render and output dimensions. [resolution.hpp](resolution.hpp) requests native renderer size changes through its normal update, not around individual draws, and adapts DoF rectangle/sprite-grid arguments. [mip_bias.hpp](mip_bias.hpp) owns replacement samplers. Native hooks check the executable and expected code before installation.

[postprocess/resources.hpp](postprocess/resources.hpp) owns enlarged scene/intermediate textures, typed views, and depth/stencil copies. [postprocess/runtime.hpp](postprocess/runtime.hpp) routes native draws/copies/clears and restores their bindings. Its helper shaders share [32-byte copy constants](postprocess/copy_constants.hlsli). Native source names and reflected texture/constant reads identify supported 2D shaders; no external dump is needed at runtime.

Geometry, lighting, and native SSR use render dimensions. DoF, motion-blur color work, grading, and UI use output dimensions; search reaches are converted from output to render pixels. Fixed-size UI, utility, and scene-thumbnail targets retain their own sizes. Generic CopyRenderBuffer must actually read a routed image at t0 before a destination can be resized; stale unused slots and matching dimensions are insufficient.

Source geometry depth is resampled separately from UI depth/stencil, including current jitter. Native depth and all eight stencil bits are retained. UI depth uses one fullscreen draw and stencil uses eight fixed masked draws. Native copies/clears, rectangles, viewports, scissors, and the size fields actually read by each shader follow output dimensions. Disabled hardware depth/stencil tests allow a DSV to be detached temporarily; active tests require a matching output-sized target. Sampled scene-depth reads remain separate.

Bloom keeps a 246x138 working region inside output-sized reduced buffers. Size constants adjust reduction offsets, flare center/fade, and the final crop independently of Bloom Type. At 4K, quarter-axis buffers remain 960x540 across quality modes. The [bloom guide](../postprocess/bloom/README.md) owns filtering/color behavior. One bloom image is reused for DoF controls, which keep their data range rather than receiving color conversions.

DoF sprites sample their enlarged bake inputs in both vertex and pixel stages. Radius limits and first-bake cutoffs follow output width; native physical blur terms are copied/converted without modifying the shared view. Shaders reading native view-to-buffer ratios receive only the required copied constant fields, using a small compute adjustment where necessary. Multiple adjustments share one state save; already resized native views do not receive a second scale.

The depth-tested flare-sprite pass `0x93A0C8D8` writes a separate half-size layer. Its depth is matched to current scene geometry while typed views, blend equations, native shader, clear color, and alpha are retained. That layer keeps its producer/consumer mapping across frames because another worker can record its scene copy before the flare draws. Frames without flares keep a neutral layer through native clears. Native quality retains the original route.

Each scene's recording sequence retains its output and intermediate roles from an accepted bloom/DoF start through display conversion. A new scene can reuse the same native texture while the previous scene is still being recorded; it cannot replace the older sequence's selection. Unfinished command lists retain their route until successful FinishCommandList. Resource resets retire both forms of ownership, with objects retained until outstanding commands release them.

Only texture slots read by the pixel shader are remapped. Native order remains late tonemap, FXAA, then UI even when UI records first on another worker. Draw2D normal-blending, normal-encoding, and roughness-blending shaders produce packed data, not UI color. Treating all Draw2D shaders as UI previously redirected rain/material work incorrectly.

Loading/save reload may draw UI without a new scene. UI and display conversion share the matching output allocation, and native color/depth/stencil clears reach it even when no reconstruction dispatch runs. The original clears still execute. Cancellation or replay fills output from current native scene data before the remaining game work continues.

## Frame and resource contracts

1. The main SetViewMatrixState hook records camera matrices before jitter, offsets gameplay projection at `ShaderManager+0x680`, and records frame/sample, dimensions, and camera identity. Other supported camera paths use that sample without advancing history.
2. MotionBlurCameraVelocity supplies depth, object motion, composed motion, and matching camera data to every method through `ValidatedFrameInputs`.
3. The preferred insertion is before bloom reduction `0x72C1D157`, with the scene-color role and matching depth/motion. Fallbacks are first DoF preparation `0xFE1DC3F8`, checked post-DoF/post-motion-preparation copies, then late Tonemap/1DLUT. Smaller/unrelated inputs are skipped.
4. One accepted resolve produces output and restores state. The shared dispatcher selects that output before routing the first consuming draw. Camera history advances only if the reset counter and sample still match.
5. Enabled reconstruction bypasses FXAA filtering but preserves the draw's other scene processing. Scene alpha remains current-frame data for downstream DoF/highlights.

Inputs must match device, dimensions, layout, temporal sample, and presentation interval. A callback crossing Present may use the preceding interval only for the same sample. Missing full-resolution work preserves history; a matching candidate that fails at every insertion invalidates it. Creation, resize, relevant settings, and missing previous camera data restart accumulation.

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

- Original paths are Velocity, Forward, Model, Alpha Model, Overlay Model, and Local Light. Local Light temporarily changes its source viewport, then restores all sixteen elements; the main viewport remains unchanged.
- Velocity/Thermography may use the preceding Present interval only for matching camera, viewport, original matrices, reset version, and sample. Their publication lock spans validation and the native write.
- Experimental paths own matrix/sequence/reset/sample copies. Lock order is transition, execution, then camera data; release guards before native calls, allocation, or logging. A rejected nested call must not inherit another call's valid correction.
- Reject unrelated, stale, orthographic, nonfinite, already-offset, or ambiguous data. Deferred camera packets need their actual producer sample/reset association, not the latest matrix or a matching pointer.

Ten Analytical path controls default to `1x`; SDK methods use fixed `1x` on the six original and four enabled experimental paths. Alternate Stream stays `0x` in all modes, even with diagnostic UI disabled. Diagnostic controls are selected by `ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS` and `ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS` in [state.hpp](state.hpp); without motion UI, defaults remain Temporal Resolve, `8 px`, and corrected native motion.

### Experimental projection paths

Addresses below are hexadecimal offsets in the supported executable; verify them before using another build. `P` is projection and `VP` is combined view/projection.

| Control / saved key | Native route and restriction |
|---|---|
| Sun Volume / `FxTaaSunVolumeJitterScale` | `30AC50 -> 30B680`, return `30B07D`: private aligned VP copy for synchronous upload/draw |
| SH Fallback / `FxTaaShFallbackJitterScale` | `27EF80`, return `1F2529`: successful output XY only; kinds 0..3 require 8/6/18/8 vertices; preserve Z/W, visibility, and bounds |
| Terrain Decal / `FxTaaTerrainDecalJitterScale` | `275280 -> 2D18D0`, return `2755CA`: owned packet P at `+160`; affects inverse-plane reconstruction, not separate raster projection |
| General Light Packet / `FxTaaLightPacketJitterScale` | `2A4C00 -> 2D3380`, return `2A5342`: owned VP at `+130`; preserve clip-W/selection fields |
| Alternate Stream / disabled | `2E7330/2E4C70 -> 2D7AA0`, returns `2E7484/2E4DD7`: native constant `0x47` passes through unchanged |

Clip translation adds jitter times W to XY, preserving Z/W. SH validates every output before writing. Native packets retain their reference-count headers and owned storage. Constant `0x47` means bank 7, vectors 4..7, not D3D `b7`. Null-camera updates can leave old context matrices, so those matrices must not be changed permanently. Deferred SH packet correction at `27F450` remains disabled pending producer/consumer ownership proof.

Analytical Alternate Stream `1x` reproduced settled waveform instability; `0x` removed it. This supports keeping the path disabled, not a diagnosis of the separate Preset M rail flicker. Terrain alignment, final light consumers, and the other experimental paths still need broader gameplay coverage.

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

Open reports from November 11, 2026 include rain inside the ACC at reduced resolution, occasional Mother Base flashes even at Native quality, and sortie ground/horizon black flashes with an unknown quality dependency. Their causes and whether they are related remain unresolved.

Controlled GPU/ReShade tests cover overlapping recordings, output-sized native processing, odd dimensions, depth/stencil, ranges, copies, fallbacks, and UI/clears without a new scene. They do not recover geometry missing at lower resolution. Loading/save-reload gameplay, display changes, special views, and failure recovery still need broader checks.

- Character wobble remains unexplained. Analytical Jitter Off and Auto/Direct motion comparisons did not help. Matrix Camera Everywhere caused too much ghosting to judge it. DLSS also showed wobble, although newer models hid it better. Readbacks have not proved matching current/previous character poses and accumulated color.
- Thermography has matching clip-position evidence at two guarded setters, and original-shader NVG looked clean in the reported test. The alternate branch and four experimental projection paths still need transition/gameplay coverage.
- There is no proven native camera-cut signal. The removed clip-space heuristic reset history continuously. FSR lacks external masks/exposure; Analytical lacks previous-depth rejection/history locks. Thin detail, transparency, rain, bright effects, and newly exposed surfaces remain difficult. No method adds sharpening.
- Earlier RGBA16F velocity and FSR FL11_0 checks establish those inspected paths, not current character stability or the separate GT-VBGI accumulator. See the [roadmap](ROADMAP.md) for investigation requirements.

## Manual validation

Follow the [addon deployment instructions](../README.md#build-and-deploy), restart with one matching addon, and disable live shader overrides. These checks cover scene reconstruction; the [lighting guide](../lighting/indirect/README.md#build-and-gameplay-checks) covers its separate history/filtering.

1. Check fresh defaults: FSR3, Auto, DLSS F, ten Analytical scales at `1x`, and diagnostic UI. Presets must not change reconstruction; Off must restore FXAA/projection without a lasting shift.
2. Start Analytical/FSR without selecting DLSS. Check no NGX load, successful accumulation, and no repeated resets in static menus or smaller DoF draws.
3. Compare static detail, characters, hair, foliage, wires, camera motion, binoculars, cuts, menus, pause/resume, fog/rain, bright moving lights, SSR, skin, DoF, motion blur, and night vision. Separate wobble from ghost trails; keep Unclamp Off for baseline tests.
4. Exercise every method/quality and DLSS model switch. Test saved/manual startup, cancellation, missing-DLL/unsupported-device explanations, suspension/resume, resizing, window/fullscreen changes, device lifecycle, loading/save reload, and Off/Analytical restoration. Check UI clipping, subtitles, and scene alpha.
5. Compare FSR Performance and Native, then DLSS qualities. Check render/output dimensions, depth edges, near/far DoF, flare-layer clearing, bloom crop, motion blur, contact-shadow/GI reach, and the reported ACC/Mother Base/sortie problems.
6. Test experimental path scales and rejected/nested/foreign-camera cases separately. Verify a stable settled waveform with Analytical/FSR/DLSS; keep Preset M rail flicker separate from Alternate Stream behavior. Do not repeat inconclusive Auto/Direct sweeps as evidence of a fix.
7. Read the fresh log for allocation/input/query/state/NGX failures and repeated history resets. `insertion=BeforeBloom` identifies the preferred point; fallback names identify later opportunities. Render/output sizes match at Native and differ under upscaling. A recorded dispatch does not prove GPU completion or visual correctness.