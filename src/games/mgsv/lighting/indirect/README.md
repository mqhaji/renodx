# Indirect lighting

GT-VBGI supplies ambient occlusion (AO), diffuse GI, and reflected light from camera-visible surfaces. The shared visibility search handles broad reflections; a focused GGX ray handles narrow lobes that the slices cannot cover reliably. Both estimates feed the same temporal and spatial filters.

AO changes native ambient diffuse. Reflected screen light replaces the overlapping native material-cube and environment contributions before fog. Native SSR keeps tracing and blends its contributing hits against the corrected scene; missing custom results retain the native reflection path. The screen-light response uses GGX; the off-screen fallback retains MGSV's integrated environment shading, which is an explicit approximation.

## Files and reading order

| File | Responsibility |
|---|---|
| [runtime.hpp](runtime.hpp) | Native input checks, resource reuse, settings, scheduling, dispatches, and game-state restoration; start at `ApplyLightPasses`, then follow `ao::Apply` |
| [ambient_diffuse.hpp](ambient_diffuse.hpp) | Snapshots the small native ambient inputs and records which original diffuse image supplied the skin copy |
| [../../rendering/producer_records.hpp](../../rendering/producer_records.hpp) | Records native camera, motion, and diffuse-copy producers and publishes them when their commands are submitted |
| [../../materials/capture.hpp](../../materials/capture.hpp) | Captures clean material color and native guides beside opaque and wet-material draws |
| [trace/specular_trace.hlsli](trace/specular_trace.hlsli) | Perspective-correct focused reflection ray using the existing nearest-depth pyramid |
| [compose/specular_result.hpp](compose/specular_result.hpp) | Per-material full-resolution reflected color, remaining environment, and actual-composition mask |
| [compose/reflection_ownership.hlsli](compose/reflection_ownership.hlsli) | Preserves composed GT-VBGI color where native SSR has no contributing hit |
| [environment_specular.hpp](environment_specular.hpp) | Captures environment color during native lighting and binds its matching visibility result during deferred composition |
| [source_preparation.hpp](source_preparation.hpp) | Adds source-radiance preparation to the native diffuse copy and pairs it with the submitted inputs |
| [CopyBuffer_0xD71E843E.ps_5_0.hlsl](CopyBuffer_0xD71E843E.ps_5_0.hlsl) | Preserves the native skin-scattering copy and writes shared FP32 source radiance |
| [source_radiance.hlsli](source_radiance.hlsli) | Defines source-light evaluation for preparation and the direct-load fallback |
| [trace/gtvbgi.cs_5_0.hlsl](trace/gtvbgi.cs_5_0.hlsl) | Shared geometry search and AO/GI/specular estimates |
| [trace/gtvbgi_depth_mips.cs_5_0.hlsl](trace/gtvbgi_depth_mips.cs_5_0.hlsl) | Optional nearest-depth reductions for the shared search |
| [denoise/gtvbgi_temporal.cs_5_0.hlsl](denoise/gtvbgi_temporal.cs_5_0.hlsl) | Reprojects, rejects, clamps, and blends previous lighting before spatial filtering |
| [denoise/gtvbgi_filter.cs_5_0.hlsl](denoise/gtvbgi_filter.cs_5_0.hlsl) | Shared spatial filtering with separate accepted weights for each signal |
| [compose/gtvbgi_add.cs_5_0.hlsl](compose/gtvbgi_add.cs_5_0.hlsl) | Full-resolution reconstruction, diffuse application, and specular visibility resolve |
| [indirect_diffuse.hlsli](indirect_diffuse.hlsli) | Shared AO/GI storage and independent missing-data rules |
| [trace/specular_occlusion.hlsli](trace/specular_occlusion.hlsli) | Reflection-cone integration and compatibility checks used by trace and filters |
| [compose/specular_composition.hlsli](compose/specular_composition.hlsli) | Selective environment correction inside the existing deferred shaders |
| [../../materials/brdf.hlsli](../../materials/brdf.hlsli) | Material decoding, GGX integration, and paired specular GI filtering rules |
| [../../materials/gtvbgi_brdf.cs_5_0.hlsl](../../materials/gtvbgi_brdf.cs_5_0.hlsl) | Builds the material-independent BRDF table once when specular transport first runs |

`runtime.hpp` owns resources, controls, and scheduling. Tracing, denoising, and composition shaders live in their respective subfolders. Shared native material decoding and BRDF evaluation remain in `materials/brdf.hlsli`; the command-submission bridge remains in reconstruction.

Both reference captures use LocalReflection (`0xD6714838`) and LocalReflectionAddBuffer (`0xDC3A84C3`). Their replacements retain the native reflection algorithm and color handling while preserving the composed lighting where no native hit contributes. The sortie menu uses LocalReflectionTPP (`0xA621C684`) for its planar floor reflection and the same compositor. That trace stays native. Generic deferred-composition, LocalReflectionTPPA1/A2, and LocalReflectionComposite variants also remain native; one-time log messages identify their use for further inspection.

Shader callbacks are registered in [../../shaders.hpp](../../shaders.hpp). The [rendering contracts](../../docs/ARCHITECTURE.md) cover ownership shared with reconstruction and shadows.

## Native inputs and placement

| Native stage | Input or output used here |
|---|---|
| Sky ambient, `SSLighting2_SH_SkyLight_Encode_ps` (`0x4FBDBB39`) | Tags the ambient producer family; it is separate from direct-light shadows |
| Ambient reconstruction, `BilateralUpscale2x2_ps` (`0x844CE327`) | Pixel t0 supplies half-resolution ambient, t2 full-resolution depth, t3 the 4x4 phase weights, s8 the point-wrap sampler, and vertex/pixel b5 the sampling constants |
| Supported sun/environment lighting | Pixel t1 supplies material roughness and t2 depth; native specular is target 1, and an extra target 2 captures only environment specular |
| Diffuse copy, `CopyBuffer_ps` (`0xD71E843E`) | Pixel t0 identifies original diffuse and t1 material; target 0 retains the native skin copy. Added t18..t22 supply albedo, normal, depth, clean color, and owner codes; target 1 receives FP32 source radiance |
| First skin scatter (`0x6DAF0C4A`) | Compute t0 supplies copied diffuse, t1 depth, t2 normals, and b0 the 64-byte geometry constants; this is the insertion point |
| Native motion composition | Supplies composed velocity, object-motion classification, and the associated depth/camera sample through the shared input tracking |
| Six deferred composition variants | Pixel t10/t11 identify native specular/depth; added t16..t21 supply captured environment, fallback visibility, reflected color, remaining environment, clean material color, and reflection readiness |
| Native G-buffer variants | Original targets 0..2 remain unchanged; targets 3/4 receive clean linear material color and native surface guides, and targets 5/6 reset reflection readiness and composition ownership |
| Rain G-buffer variants `0x2C230875`, `0xC3538951` | Original targets 0/1 remain unchanged; extra outputs carry wet clean color and updated guides without another pass |

The order is native ambient and direct lighting, diffuse copy, GT-VBGI, both skin-scattering passes, then deferred composition with fog and the early tone curve. Native sky, forward effects, and reflections follow. The custom lighting therefore reaches skin scattering and later brightness measurements before scene color is composed. Enhanced Shadows has already affected direct light inside the native light draws; GT-VBGI does not apply AO to those shadows again.

Native albedo is read through an sRGB view, which decodes base color once. Native diffuse and specular lighting are linear and already multiplied by exposure. GI sources use `source albedo * original diffuse`, before our application changes either diffuse destination. The receiving surface's albedo is applied later by deferred composition. Visibility, depth, normals, and motion are data, so they receive no color-space conversion. Later floating-point scene textures may contain encoded color; their format alone does not establish linear light.

### Native AO compatibility

With Screen-Space Lighting enabled, the six deferred composition shaders skip native AO on diffuse. Custom AO is applied only to reconstructed ambient diffuse earlier in the chain. This bypass follows the setting, not dispatch success: if custom lighting cannot run, diffuse receives neither native nor custom AO at that insertion. Missing ambient reconstruction also skips diffuse AO while allowing valid GI to continue.

Direct highlights skip native AO where environment separation is available. Supported environment reflections use custom visibility blended by confidence; missing capture or visibility retains the corresponding native specular AO. Separate rain reflections keep native AO. Screen-Space Lighting Off restores native composition. The native AO producers run in either mode.

## Follow one lighting invocation

### 1. Collect inputs in native order

The ambient callback copies the half-resolution ambient image, phase weights, and mutable sampling constants beside the native upsample. It keeps the original constant-buffer range and sampler. Native diffuse clears invalidate the snapshot in GPU order, so a missing producer cannot leave an old ambient contribution active.

The first scatter hook retains its actual depth, normal, and copied-diffuse bindings, then checks dimensions, device generation, view formats, and read/write aliases. It also copies the native geometry constants on the GPU. Retaining a COM object protects its lifetime, not its pixels or mutable constants.

Deferred recording does not establish execution order. The coordinator submits the native prefix before running the custom chain and then resumes the original scatter work. The existing camera/motion ledger also publishes the diffuse-copy source at that boundary. This metadata requires no separate command-list split. Copy records do not advance the temporal submission number. One application consumes the source association; repeated interceptions and cached-list replay skip duplicate custom lighting.

The diffuse-copy draw also validates material ownership and evaluates outgoing diffuse radiance once per scene pixel. Its extra FP32 output preserves the original multiplication range and precision. RGB contains linear, pre-exposed radiance; alpha distinguishes qualified clean material from native albedo fallback. A valid black source remains valid. Both diffuse GI and broad/focused specular GI reuse this image instead of repeating material validation during their searches. Facing, visibility, and the receiver's BRDF remain in the trace because they depend on the receiving surface.

The submitted copy publishes its prepared view through the existing producer ledger. The lighting hook checks generation, copy source, albedo, normal, and depth identities before using it. Unsupported layouts and unavailable preparation retain direct source loads. This adds one image attachment to an existing draw, without an extra dispatch or image copy. Its full-resolution write has a cost; compare total lighting time in a fixed scene before claiming a performance improvement.

### 2. Trace visible geometry once

Tracing also writes the receiver's decoded F0 and mapped roughness to the current material-history slot. Temporal rejection, its current-frame clamp, spatial filtering, and full-resolution reconstruction reuse that guide. The previous slot remains unchanged until its consumers finish. No additional material image or copy is needed; the guide retains its existing FP16 storage precision.

The lighting grid divides each render axis by 1, 2, 4, or 8 and rounds up. Each pixel uses one corresponding full-resolution depth/normal sample, clamped at the scene edge. Search radius is configured in output pixels and converted to render pixels when upscaling is active. Surface offset and thickness are in reconstructed game-space units; their scale in meters has not been measured.

Each importance-sampled slice searches both directions with increasing step spacing. A finite depth slab covers an angular interval. A 32-bit mask records which weighted angular sectors are occupied; newly covered sectors receive the sampled surface's outgoing diffuse light once. Rejected source light still occludes. No-hit and black-source GI remain valid zero results.

Thin foliage can present normals facing away from the camera. Diffuse integration continuously folds only their view component and bounds the surface offset so the biased origin remains on the correct side. Specular chooses the camera-facing normal side while preserving the original reflected direction. Its separate mask covers the reflection cone, using the same depth samples.

Optional depth reductions keep the nearest reverse-Z depth within each footprint. When GI needs that sample's radiance, the trace descends to the matching full-resolution owner before reading color or normal. This avoids coloring a coarse occluder with an unrelated surface. The optional chain uses four dispatches; unavailable reductions retain full-resolution depth sampling.

Missing source material rejects the broad specular estimate. Once that happens, the trace stops its remaining specular-sector work while continuing diffuse lighting and occlusion. The focused reflection fallback still runs. There is no benefit in completing a broad estimate that cannot be used.

### 3. Accumulate matching previous samples

The host requires a new submitted producer, matching device/depth/camera identity, compatible settings, and a consecutive frame phase before admitting history. The shader then validates the native projections and camera bases. It removes current jitter, reconstructs the receiver in the previous camera, and applies native object screen displacement where available.

Four previous lighting taps are tested individually against expected depth and normal before interpolation. Specular also checks roughness, reflected direction in the correct camera basis, and surface side. GI RGB is converted from previous to current exposure; AO and specular visibility are not exposure-scaled. Object motion does not provide previous object depth or deformation, so those changes can reject history.

An 8x8 thread group shares a 10x10 current-sample tile for its 3x3 history clamps. All lanes reach the synchronization barrier before edge or background returns. FP32 statistics limit accepted history to a range around current samples. AO, GI, and specular keep separate validity, rejection weights, and clamp statistics even though they share setup and dispatch.

Cold, disabled, or rejected history copies current samples. The host stores temporal output before spatial filtering, so accumulated history does not repeatedly absorb the spatial blur. A producer is allowed to advance history once; Present alone is insufficient.

The current material guide comes from tracing and is never accumulated. Temporal rejection, spatial filtering, and reconstruction reuse it, including when accumulation is off. An unavailable guide retains direct native-material lookup. The guide uses FP16; the final receiver response still uses the full-resolution native material.

### 4. Filter without crossing surfaces

The A-Trous filter grows sample spacing between passes while keeping the same image dimensions. It combines a binomial kernel with receiver-plane distance and normal agreement. Specular additionally rejects incompatible reflected directions, roughness, and opposite sides. Each signal normalizes only its own valid taps; a missing GI sample cannot remove valid AO.

Every pass retains the center and selects one of four neighbor patterns using IS-FAST noise. Neighbor weights are multiplied by four to compensate for their selection probability. Every output pixel is written. The pattern animates when scene reconstruction is selected and stays fixed otherwise; reconstruction helps smooth the animated result. Sparse normalized filtering can still differ near edges. Center-relative FP32 sums preserve constant lighting through FP16 stores.

### 5. Apply diffuse and resolve specular

The application shader reconstructs our lighting at full resolution using four depth-weighted taps. Separately, it reproduces the native ambient upsample using the captured point-wrap sampler, phase weights, raw-depth comparisons, and sampling constants. The custom lighting upsample cannot replace that calculation without changing which part of native light is considered ambient.

The resulting diffuse equation is `direct + ambient * AO + GI`, where `direct = native total - reconstructed ambient`. Missing or inconsistent ambient data skips diffuse AO while preserving GI. The native ambient-volume mask remains intact. GI already includes sampled visibility and is not multiplied by AO again.

Application updates native diffuse RGB and eligible skin-copy RGB while preserving each image's alpha. The skin-copy alpha is a material mask, not general lighting validity. Both native scatter passes then run once. The same dispatch resolves specular visible/open areas into a full-resolution visibility and confidence pair.

### 6. Replace overlapping reflections before fog

Material shaders keep their original outputs and separately calculate linear color with native cube RGB removed. The compiler can reuse their common texture and geometry work. This includes shaders that sample the cube twice. Native bodies follow the dumps, with split-semantic swizzles repaired against the original assembly where needed. A full opaque write publishes clean color; blended or partial writes invalidate it. Native albedo, normal, and material codes identify its owner within one UNORM rounding step; depth must match exactly. Wet-material passes carry the clean color through the same wetness multiplier and preserve those guides.

Supported sun/environment draws capture their environment contribution before it is combined with direct specular. Application pairs a new reflected-color result with its native material allocation. Deferred composition checks the native specular/depth pair and removes the baked material reflection from diffuse composition. At default strengths, the result is `clean material * diffuse + direct specular + screen reflection + native environment * remaining response`. Here, native environment includes the removed material-cube contribution. Direct highlights keep their own shadows. The game then applies its existing fog and color processing. Rain's separate reflection term keeps its native treatment.

A deferred pixel writes the composition mask only after it actually uses the new result. Native auxiliary outputs, pixel UAVs, or shader class instances retain the original shader. The native reflection compositor reads this mask through the same material allocation. Native SSR keeps tracing every eligible pixel: accepting a GT-VBGI lighting estimate does not mean it contains every visible reflection. G-buffer writes, captured directional-light writes, and native clears reset readiness in the GPU command stream.

Readiness uses a separate R8 image. Surface and environment draws reset that byte without overwriting HDR reflection RGB. Application resets it for every pixel and marks it ready only after writing both reflected light and remaining environment response. Deferred composition reads the pair only when the flag is ready. A skipped or failed application therefore keeps native reflections even if the HDR images still contain an older result. The actual-composition mask remains separate because preparing a result does not prove that a deferred pixel used it.

Material capture caches immutable format, size, and resource checks on each native view. The cache expires with that view and is refreshed for a new selected-device generation. Live bindings, blending, shader instances, read/write hazards, and selected-scene checks still run at each draw. Retained native views protect borrowed resource identities while the callback uses them; cached metadata does not establish fresh pixels or GPU execution.

Regular SSR and the sortie menu's planar floor trace both feed the native reflection compositor. It checks hit alpha and native reflection multipliers before bypassing its original blend. A contributing hit uses that blend: with coverage `a` and gain `g`, the result is `(1 - a) * current_scene + a * native_reflection * g`. Its complementary scene weight also attenuates the GT-VBGI contribution, rather than adding both estimates at full strength. No native hit leaves the GT-VBGI scene intact. A black reflection with positive hit alpha remains valid; brightness does not decide whether to blend. This preserves native reflection detail alongside GT-VBGI, including MGSV's authored whole-scene blend and its shading approximation. It needs no added pass, resource, or scene-color correction image.

The pass binder clears old inputs before rebinding ping-pong outputs. Unused slots use distinct placeholder outputs, separate from placeholder inputs. The host restores all displaced compute and graphics state once after the chain. Native work continues even when custom input checks fail.

## Storage and reuse

| Image | Format and meaning |
|---|---|
| Shared raw, temporal, and spatial diffuse lighting | RGBA16F: nonnegative GI in RGB, ambient visibility in alpha; R = -1 means missing GI, alpha = -1 means missing AO |
| Raw, temporal, and spatial specular | RGBA16F: visible/open angular areas in RG, native roughness in B, sampling support in alpha; alpha = -1 means missing |
| Previous geometry | R32G32_UINT: FP32 linear-depth bits and two packed 16-bit normal coordinates |
| Environment capture | Full-resolution RGBA16F: linear pre-exposed environment RGB and validity alpha |
| Resolved specular visibility | Full-resolution RG16F: fallback visibility and confidence |
| Clean material and owner | Full-resolution RGBA16F linear color/validity and RGBA32_UINT native albedo, normal, material, and depth codes |
| Resolved reflected light | Two full-resolution RGBA16F images: reflected light and remaining environment response |
| Reflection readiness | Full-resolution R8: both reflected-light outputs are ready for this native material |
| Actual composition | Full-resolution R8: one only where deferred composition used the new reflection |

AO and GI share the raw image, two history images, and two filter images. Their missing-data markers are independent and consume no precision from valid values. Valid black GI and fully blocked AO remain valid zeros. Read packed values with point loads, validate them, then interpolate with separate accepted weights; hardware interpolation would mix missing markers into real values. Specular filtering averages angular areas before dividing, since an average of visibility ratios would give poorly sampled cones too much influence.

Four cached UAVs cover the native diffuse/skin destinations and their alternating allocations. The cache owns its views, uses bounded round-robin replacement, and releases retired objects outside its lock to allow destruction callbacks. Lighting Off, swapchain retirement, and device teardown retire it. Texture size changes rebuild images and views while keeping unchanged shader pipelines.

## Controls, diagnostics, and limits

Screen-Space Lighting uses Off / GT-VBGI, with separate strengths for ambient occlusion, diffuse GI, specular GI, and specular occlusion. All default to 100%. Specular GI Strength scales the new reflected light during deferred composition, after filtering. At 0%, specular occlusion and native SSR remain active. It does not reset history or skip tracing and filtering. Compare normal rendering with Lighting Debug View Off; stage diagnostics remain unscaled.

Lighting Resolution defaults to 1/4 of render pixels, or half of each axis. Fractions are nominal at odd dimensions. Defaults are three slices, 40 samples per side, a 2000-output-pixel radius, thickness 0.15, temporal accumulation On, and spatial filtering On with width 5, radius 8, and three passes. Temporal and spatial switches do not choose separately compiled shader variants.

| Temporal control | AO | Diffuse/specular GI | Fallback specular visibility |
|---|---|---|---|
| History weight | 90% | 90% | Shared with AO |
| Depth tolerance | 20% | 5% | Shared with AO |
| Normal threshold | 0 | 0.9 | Shared with AO |
| Clamp in standard deviations | 0.1 | 1 | 2 |

IS-FAST trace noise advances when reconstruction or qualified lighting accumulation can combine frames. The filter uses a separate spatial offset in the same immutable noise asset, also shared with PCSS. Default work is six dispatches per eligible invocation: trace, temporal, three spatial filters, and application. Optional depth reductions add four. This is not a guarantee of six dispatches per frame under every game path.

Useful debug views follow the same stages: Source Lighting and Geometry Coverage inspect trace inputs; Raw/Temporal/Filtered AO inspect filtering; GI Lighting shows final bounced light; Accepted History views show per-pixel history weights. Ambient Diffuse, Direct Diffuse, Corrected Diffuse, and Ambient Input Validity verify selective AO. The validity view is green where ambient reconstruction is qualified and magenta where diffuse AO is skipped.

Specular views include native roughness, captured environment, raw/temporal/filtered visibility, accepted history, and confidence. Cyan means sampled slices missed the reflection cone; magenta means missing or unsupported estimates. Changing accepted-history weights alone does not establish unstable final shading. These debug views replace diffuse lighting before scattering, so surface color, specular, fog, and later processing still affect their appearance.

The visibility-only result remains available for pixels without qualified material transport. It uses the reflection-cone approximation from Jimenez et al., Appendix B. New reflected light uses the mapped GGX response below. Camera-visible depth cannot describe hidden surfaces; rays that leave the image retain environment response.

## Specular transport

Transport runs with GT-VBGI and uses its existing temporal and spatial controls. Lighting Debug View selects the stage to inspect. No separate temporal or spatial dispatch is added. A one-time 128x128 RGBA16F table stores full-lobe and reflection-cone Schlick coefficients, using 2048 visible-normal GGX samples per cell.

| View | What to check |
|---|---|
| Material F0 | Dielectric and metallic materials should have different normal-incidence reflectance; colored metals can have colored reflectance |
| Mapped Specular Roughness | Smooth and rough materials should remain distinguishable without changing their native G-buffer |
| Raw / Temporal / Filtered Specular GI | A nearby lit colored surface should transfer color to a reflective receiver; temporal and spatial switches should affect their respective stages |
| Remaining Environment Response | Bright values retain environment response; darker values replace part of it with a screen surface, including valid black blockers |
| Integrated Specular Response | The material's complete compensated GGX response, including the native reflection rate |
| Specular GI Support | Low support marks directions that the available slices cannot reliably cover |
| Clean Material Color / Removed Material Reflection | Clean color excludes native cube RGB; removed reflection shows the separated contribution |
| Material Input Coverage | Green means the native color/normal/material codes match within one rounding step and depth matches exactly; magenta retains native reflections |
| Resolved Specular Light / Resolved Environment Response | The full-resolution pair used in normal composition; dark screen RGB may be a valid black blocker |
| Specular Input Coverage | Green means a qualified pair was resolved; magenta means native fallback |
| Accepted Specular GI History | The actual history blend after geometry, material, lobe, and neighborhood rejection; zero is expected after a reset or disocclusion |

The native environment draw binds the material palette at t10 and Fresnel lookup at t9. Immutable tables are retained directly. Mutable tables are copied into reusable GPU snapshots beside each draw, so command-list replay restores their pixels too. View descriptions are cached on the native SRV and refreshed for a new device generation.

The existing submission ledger selects the table pair on the native specular allocation before lighting reads it. A worker recording another command list cannot change that selection prematurely. Table records add no command-list split and do not advance temporal history. Missing tables clear the pair and retain current lighting. A one-time log identifies immutable and copied inputs.

The palette's first row supplies specular tint; its second row supplies the Fresnel control. We evaluate the native lookup at normal incidence to recover F0. Native G-buffer red supplies roughness and green remains the separate authored reflection rate. The enhanced BRDF maps the native Blinn-Phong power `2^(12 - 11 * roughness)` to GGX by matching the distribution peak, `alpha^2 = 2 / (power + 2)`. This is a deliberate approximation, not an exact match to the native highlight tails or environment shading.

Newly occupied specular sectors gather the same known linear outgoing diffuse source as diffuse GI, independently of diffuse-sector ownership. Two-point quadrature integrates BRDF times cosine over each sector, including its spherical measure and the slice sampling PDF. Valid black and opaque backfacing sources still block environment light. Transmission, directional specular at the source, and later forward/emissive effects are not represented by this source.

Specular GI stores demodulated screen radiance in RGB and mapped roughness in alpha. A paired RGBA16F image stores the remaining normalized environment response in RGB and sampling support in alpha. Negative alpha means missing data. The ratio estimate normalizes sampled cone weights against the integrated BRDF table; it is biased, but preserves the white-source/white-environment partition. The rest of the GGX lobe remains environment response. Insufficient slice support selects the focused estimate; missing screen hits retain environment response.

Both halves use identical temporal and spatial weights. History additionally rejects incompatible F0 and roughness, corrects radiance for exposure, and applies one shared rejection factor instead of clipping the two halves independently. Geometry, camera transforms, source reads, and dispatches are shared with diffuse lighting and visibility. Extra raw/history/filter packets and material guides cost 96 bytes per lighting pixel before padding, about 47.5 MiB at 960x540, and are allocated only after the required native inputs are available. Clean material/owner storage adds 24 bytes per full-resolution pixel; the resolved reflection pair and its readiness/composition masks add 18 bytes per full-resolution pixel per corresponding native allocation. G-buffer draws write the 24-byte material capture and two one-byte masks; they do not write the HDR reflection pair. These costs need gameplay profiling alongside the custom dispatches.

Where slice support is low, one visible-normal GGX sample traces a focused ray through the same scene depth. Reciprocal view depth is interpolated along the projected segment. A nearest-depth block is skipped only when the entire ray segment lies in front of its nearest surface; possible intersections descend to full-resolution depth. The ray shares the lighting reach and thickness settings and has a 128-step limit. Narrow and broad estimates are blended as alternatives for the same lobe. The focused estimate is a normalized split-sum approximation; it does not claim an unbiased directional BRDF integral.

The captured environment is already shaded by MGSV's native Blinn-Phong model. The RGB material cube is an authored LDR asset. Neither becomes raw GGX HDR environment radiance by multiplying it by our LUT. Keeping their integrated response is a deliberate fallback until a consistent environment-radiance source is available. The new source also omits directional specular at the hit, transmission, later forward/emissive effects, and fog along the reflected segment. This is a raster transport baseline, not path tracing.

The BRDF integration follows [Heitz's visible-normal GGX sampling (2018)](https://jcgt.org/published/0007/04/01/paper.pdf) and the scaled-lobe energy compensation described in [Filament's rendering documentation](https://google.github.io/filament/main/filament.html). The specular extension uses the transport equation and the existing visibility search; it is not a specular GI algorithm claimed by the original diffuse GT-VBGI paper.

## Build and gameplay checks

Follow the [addon build and deployment checks](../../README.md#build-and-deploy). Local API/shader tests compare decoded AO/GI, geometry, diagnostics, specular, and native diffuse/skin outputs; those checks do not substitute for observing the game.

Compare normal lighting and the stage views on skin, hair, foliage, bright bounce sources, unlit blockers, wet surfaces, and dielectric/metallic receivers. Move the camera, vary exposure and Lighting Resolution, compare native and scaled DLSS, toggle temporal/spatial filtering, and visit loading/menu transitions. Check cold history, missing-input fallback, floor/rail reflections, and independent AO/GI/specular strengths in the same visit.

Check the log for allocation failures, missing material tables, and unrecovered inputs. Resolution changes log scene/trace dimensions; temporal status distinguishes unavailable inputs, seeding, and submitted history. Recording a dispatch does not prove GPU completion or per-pixel history acceptance.

Measure optimizations at a fixed camera with clean On / Off / On runs. Disable probe capture, profiling, and live reload, and ensure cached command lists no longer contain capture copies. Separate CPU submission cost from GPU stage time. Controlled replays can establish equivalent outputs and isolated timings; they cannot establish an ordinary-game FPS gain.

## Credits and license

The GT-VBGI core is adapted from [Mirko Salm (TinyTexel)'s reference](https://www.shadertoy.com/view/lfdBWn), using its MIT license option. The files in this directory use the MIT license below. Source headers refer here, and the compiled addon includes this notice in its Third-Party Licenses pane.

The visibility-bitmask method is described by Therrien, Levesque, and Gilet in [Screen Space Indirect Lighting with Visibility Bitmask](https://arxiv.org/abs/2301.11376). The reflection-cone approximation follows Appendix B of Jimenez et al., [Practical Realtime Strategies for Accurate Indirect Occlusion](https://www.iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf).

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
