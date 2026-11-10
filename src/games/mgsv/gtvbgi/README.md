# GT-VBGI

GT-VBGI adds ambient occlusion (AO), diffuse global illumination (GI), and specular occlusion through one screen-space geometry search. AO reduces native ambient light; GI adds light bounced from visible surfaces; specular occlusion limits environment reflections blocked by nearby geometry. It does not generate reflected color or replace native screen-space reflections.

## Files and reading order

| File | Responsibility |
|---|---|
| [lighting.hpp](lighting.hpp) | Native input checks, resource reuse, settings, scheduling, dispatches, and game-state restoration; start at `ApplyLightPasses`, then follow `ao::Apply` |
| [ambient_diffuse.hpp](ambient_diffuse.hpp) | Snapshots the small native ambient inputs and records which original diffuse image supplied the skin copy |
| [temporal_inputs.hpp](temporal_inputs.hpp) | Records native camera, motion, and diffuse-copy producers and publishes them when their commands are submitted |
| [environment_specular.hpp](environment_specular.hpp) | Captures environment color during native lighting and binds its matching visibility result during deferred composition |
| [gtvbgi.cs_5_0.hlsl](gtvbgi.cs_5_0.hlsl) | Shared geometry search and AO/GI/specular estimates |
| [gtvbgi_depth_mips.cs_5_0.hlsl](gtvbgi_depth_mips.cs_5_0.hlsl) | Optional nearest-depth reductions for the shared search |
| [gtvbgi_temporal.cs_5_0.hlsl](gtvbgi_temporal.cs_5_0.hlsl) | Reprojects, rejects, clamps, and blends previous lighting before spatial filtering |
| [gtvbgi_filter.cs_5_0.hlsl](gtvbgi_filter.cs_5_0.hlsl) | Shared spatial filtering with separate accepted weights for each signal |
| [gtvbgi_add.cs_5_0.hlsl](gtvbgi_add.cs_5_0.hlsl) | Full-resolution reconstruction, diffuse application, and specular visibility resolve |
| [indirect_diffuse.hlsli](indirect_diffuse.hlsli) | Shared AO/GI storage and independent missing-data rules |
| [specular_occlusion.hlsli](specular_occlusion.hlsli) | Reflection-cone integration and compatibility checks used by trace and filters |
| [specular_composition.hlsli](specular_composition.hlsli) | Selective environment correction inside the existing deferred shaders |

Shader callbacks are registered in [shaders.hpp](../shaders.hpp). The [rendering contracts](../docs/ARCHITECTURE.md) cover ownership shared with reconstruction and shadows.

## Native inputs and placement

| Native stage | Input or output used here |
|---|---|
| Sky ambient, `SSLighting2_SH_SkyLight_Encode_ps` (`0x4FBDBB39`) | Tags the ambient producer family; it is separate from direct-light shadows |
| Ambient reconstruction, `BilateralUpscale2x2_ps` (`0x844CE327`) | Pixel t0 supplies half-resolution ambient, t2 full-resolution depth, t3 the 4x4 phase weights, s8 the point-wrap sampler, and vertex/pixel b5 the sampling constants |
| Supported sun/environment lighting | Pixel t1 supplies material roughness and t2 depth; native specular is target 1, and an extra target 2 captures only environment specular |
| Diffuse copy, `CopyBuffer_ps` (`0xD71E843E`) | Pixel t0 identifies the original diffuse source; target 0 receives the skin-scattering copy and its material-mask alpha |
| First skin scatter (`0x6DAF0C4A`) | Compute t0 supplies copied diffuse, t1 depth, t2 normals, and b0 the 64-byte geometry constants; this is the insertion point |
| Native motion composition | Supplies composed velocity, object-motion classification, and the associated depth/camera sample through the shared input tracking |
| Six deferred composition variants | Pixel t10/t11 identify native specular/depth; added t16/t17 supply their captured environment and resolved visibility/confidence |

The order is native ambient and direct lighting, diffuse copy, GT-VBGI, both skin-scattering passes, then deferred composition with fog and the early tone curve. Native sky, forward effects, and reflections follow. The custom lighting therefore reaches skin scattering and later brightness measurements before scene color is composed. Enhanced Shadows has already affected direct light inside the native light draws; GT-VBGI does not apply AO to those shadows again.

Native albedo is read through an sRGB view, which decodes base color once. Native diffuse and specular lighting are linear and already multiplied by exposure. GI sources use `source albedo * original diffuse`, before our application changes either diffuse destination. The receiving surface's albedo is applied later by deferred composition. Visibility, depth, normals, and motion are data, so they receive no color-space conversion. Later floating-point scene textures may contain encoded color; their format alone does not establish linear light.

## Follow one lighting invocation

### 1. Collect inputs in native order

The ambient callback copies the half-resolution ambient image, phase weights, and mutable sampling constants beside the native upsample. It keeps the original constant-buffer range and sampler. Native diffuse clears invalidate the snapshot in GPU order, so a missing producer cannot leave an old ambient contribution active.

The first scatter hook retains its actual depth, normal, and copied-diffuse bindings, then checks dimensions, device generation, view formats, and read/write aliases. It also copies the native geometry constants on the GPU. Retaining a COM object protects its lifetime, not its pixels or mutable constants.

Deferred recording does not establish execution order. The coordinator submits the native prefix before running the custom chain and then resumes the original scatter work. The existing camera/motion ledger also publishes the diffuse-copy source at that boundary. This metadata requires no separate command-list split. Copy records do not advance the temporal submission number. One application consumes the source association; repeated interceptions and cached-list replay skip duplicate custom lighting.

### 2. Trace visible geometry once

The lighting grid divides each render axis by 1, 2, 4, or 8 and rounds up. Each pixel uses one corresponding full-resolution depth/normal sample, clamped at the scene edge. Search radius is configured in output pixels and converted to render pixels when upscaling is active. Surface offset and thickness are in reconstructed game-space units; their scale in meters has not been measured.

Each importance-sampled slice searches both directions with increasing step spacing. A finite depth slab covers an angular interval. A 32-bit mask records which weighted angular sectors are occupied; newly covered sectors receive the sampled surface's outgoing diffuse light once. Rejected source light still occludes. No-hit and black-source GI remain valid zero results.

Thin foliage can present normals facing away from the camera. Diffuse integration continuously folds only their view component and bounds the surface offset so the biased origin remains on the correct side. Specular chooses the camera-facing normal side while preserving the original reflected direction. Its separate mask covers the reflection cone, using the same depth samples.

Optional depth reductions keep the nearest reverse-Z depth within each footprint. When GI needs that sample's radiance, the trace descends to the matching full-resolution owner before reading color or normal. This avoids coloring a coarse occluder with an unrelated surface. The optional chain uses four dispatches; unavailable reductions retain full-resolution depth sampling.

### 3. Accumulate matching previous samples

The host requires a new submitted producer, matching device/depth/camera identity, compatible settings, and a consecutive frame phase before admitting history. The shader then validates the native projections and camera bases. It removes current jitter, reconstructs the receiver in the previous camera, and applies native object screen displacement where available.

Four previous lighting taps are tested individually against expected depth and normal before interpolation. Specular also checks roughness, reflected direction in the correct camera basis, and surface side. GI RGB is converted from previous to current exposure; AO and specular visibility are not exposure-scaled. Object motion does not provide previous object depth or deformation, so those changes can reject history.

An 8x8 thread group shares a 10x10 current-sample tile for its 3x3 history clamps. All lanes reach the synchronization barrier before edge or background returns. FP32 statistics limit accepted history to a range around current samples. AO, GI, and specular keep separate validity, rejection weights, and clamp statistics even though they share setup and dispatch.

Cold, disabled, or rejected history copies current samples. The host stores temporal output before spatial filtering, so accumulated history does not repeatedly absorb the spatial blur. A producer is allowed to advance history once; Present alone is insufficient.

### 4. Filter without crossing surfaces

The A-Trous filter grows sample spacing between passes while keeping the same image dimensions. It combines a binomial kernel with receiver-plane distance and normal agreement. Specular additionally rejects incompatible reflected directions, roughness, and opposite sides. Each signal normalizes only its own valid taps; a missing GI sample cannot remove valid AO.

Every pass retains the center and selects one of four neighbor patterns using IS-FAST noise. Neighbor weights are multiplied by four to compensate for their selection probability. Every output pixel is written. The pattern animates when scene reconstruction is selected and stays fixed otherwise; reconstruction helps smooth the animated result. Sparse normalized filtering can still differ near edges. Center-relative FP32 sums preserve constant lighting through FP16 stores.

### 5. Apply diffuse and resolve specular

The application shader reconstructs our lighting at full resolution using four depth-weighted taps. Separately, it reproduces the native ambient upsample using the captured point-wrap sampler, phase weights, raw-depth comparisons, and sampling constants. The custom lighting upsample cannot replace that calculation without changing which part of native light is considered ambient.

The resulting diffuse equation is `direct + ambient * AO + GI`, where `direct = native total - reconstructed ambient`. Missing or inconsistent ambient data skips diffuse AO while preserving GI. The native ambient-volume mask remains intact. GI already includes sampled visibility and is not multiplied by AO again.

Application updates native diffuse RGB and eligible skin-copy RGB while preserving each image's alpha. The skin-copy alpha is a material mask, not general lighting validity. Both native scatter passes then run once. The same dispatch resolves specular visible/open areas into a full-resolution visibility and confidence pair.

### 6. Compose environment specular and restore state

Supported sun/environment draws capture their environment contribution before it is combined with direct specular. Deferred composition matches this capture by native specular/depth resources and uses `direct specular + environment * lerp(native AO, custom visibility, strength * confidence)`. Direct highlights retain their light-specific shadows. Unsupported capture or visibility retains the appropriate native occlusion. Rain's separate reflection term and native SSR retain their existing treatment.

The pass binder clears old inputs before rebinding ping-pong outputs. Unused slots use distinct placeholder outputs, separate from placeholder inputs. The host restores all displaced compute and graphics state once after the chain. Native work continues even when custom input checks fail.

## Storage and reuse

| Image | Format and meaning |
|---|---|
| Shared raw, temporal, and spatial diffuse lighting | RGBA16F: nonnegative GI in RGB, ambient visibility in alpha; R = -1 means missing GI, alpha = -1 means missing AO |
| Raw, temporal, and spatial specular | RGBA16F: visible/open angular areas in RG, native roughness in B, sampling support in alpha; alpha = -1 means missing |
| Previous geometry | R32G32_UINT: FP32 linear-depth bits and two packed 16-bit normal coordinates |
| Environment capture | Full-resolution RGBA16F: linear pre-exposed environment RGB and validity alpha |
| Resolved specular | Full-resolution RG16F: visibility and confidence for deferred composition |

AO and GI share the raw image, two history images, and two filter images. Their missing-data markers are independent and consume no precision from valid values. Valid black GI and fully blocked AO remain valid zeros. Read packed values with point loads, validate them, then interpolate with separate accepted weights; hardware interpolation would mix missing markers into real values. Specular filtering averages angular areas before dividing, since an average of visibility ratios would give poorly sampled cones too much influence.

Sharing diffuse storage removes five separate RG16F AO textures, saving 20 bytes per lighting pixel before driver padding. At a 960x540 lighting grid this saves about 9.9 MiB. It also removes their separate texture reads and writes without lowering sample counts or FP16 precision.

Four cached UAVs cover the native diffuse/skin destinations and their alternating allocations. The cache owns its views, uses bounded round-robin replacement, and releases retired objects outside its lock to allow destruction callbacks. Lighting Off, swapchain retirement, and device teardown retire it. Texture size changes rebuild images and views while keeping unchanged shader pipelines.

## Controls, diagnostics, and limits

Screen-Space Lighting uses Off / GT-VBGI, with separate AO, GI, and specular strengths. Lighting Resolution defaults to 1/4 of render pixels, or half of each axis. Fractions are nominal at odd dimensions. Defaults are three slices, 40 samples per side, a 2000-output-pixel radius, thickness 0.15, temporal accumulation On, and spatial filtering On with width 5, radius 8, and three passes. Temporal and spatial switches do not choose separately compiled shader variants.

| Temporal control | AO | GI | Specular |
|---|---|---|---|
| History weight | 90% | 90% | Shared with AO |
| Depth tolerance | 20% | 5% | Shared with AO |
| Normal threshold | 0 | 0.9 | Shared with AO |
| Clamp in standard deviations | 0.1 | 1 | 2 |

IS-FAST trace noise advances when reconstruction or qualified lighting accumulation can combine frames. The filter uses a separate spatial offset in the same immutable noise asset, also shared with PCSS. Default work is six dispatches per eligible invocation: trace, temporal, three spatial filters, and application. Optional depth reductions add four. This is not a guarantee of six dispatches per frame under every game path.

Useful debug views follow the same stages: Source Lighting and Geometry Coverage inspect trace inputs; Raw/Temporal/Filtered AO inspect filtering; GI Lighting shows final bounced light; Accepted History views show per-pixel history weights. Ambient Diffuse, Direct Diffuse, Corrected Diffuse, and Ambient Input Validity verify selective AO. The validity view is green where ambient reconstruction is qualified and magenta where diffuse AO is skipped.

Specular views include native roughness, captured environment, raw/temporal/filtered visibility, accepted history, and confidence. Cyan means sampled slices missed the reflection cone; magenta means missing or unsupported estimates. Changing accepted-history weights alone does not establish unstable final shading. These debug views replace diffuse lighting before scattering, so surface color, specular, fog, and later processing still affect their appearance.

Specular visibility uses a GGX-based reflection-cone approximation from Jimenez et al., Appendix B. It has not been fitted to MGSV's native Blinn-Phong material model. Camera-visible depth cannot describe hidden surfaces, and a small number of slices may miss narrow reflection cones. Sampling support and material agreement reduce confidence near those cases and at grazing foliage. This is occlusion of environment light, not a new reflection or full-scene GI solution.

## Verification

Build the `mgsv` target with the agreed toolchain and inspect the generated GT-VBGI shader outputs under `build/mgsv.include/embed` and the final addon. Never deploy while `mgsvtpp.exe` is running. Local API/shader tests compare decoded AO/GI, geometry, diagnostics, specular, and native diffuse/skin outputs; those checks do not substitute for observing the game.

In one gameplay checkpoint, compare normal lighting and the stage views on skin, hair, foliage, bright bounce sources, and reflective surfaces. Move the camera, change Lighting Resolution and reconstruction quality, toggle temporal/spatial filtering, and visit loading/menu transitions. Check the log for creation failures and unrecovered missing inputs. Resolution changes log scene/trace dimensions; temporal status distinguishes unavailable inputs, seeding, and submitted history. Messages that commands were recorded do not prove GPU completion or per-pixel history acceptance.

Measure optimizations at a fixed camera with clean On / Off / On runs. Disable probe capture, profiling, and live reload, and ensure cached command lists no longer contain capture copies. Separate CPU submission cost from GPU stage time. Controlled replays can establish equivalent outputs and isolated timings; they cannot establish an ordinary-game FPS gain.

## Credits and license

The GT-VBGI core is adapted from [Mirko Salm (TinyTexel)'s reference](https://www.shadertoy.com/view/lfdBWn), using its MIT license option. The files in this directory use the MIT license below. Source headers refer here, and the compiled addon includes this notice in its Third-Party Licenses pane.

The visibility-bitmask method is described by Therrien, Levesque, and Gilet in [Screen Space Indirect Lighting with Visibility Bitmask](https://arxiv.org/abs/2301.11376). The reflection-cone approximation follows Appendix B of Jimenez et al., [Practical Realtime Strategies for Accurate Indirect Occlusion](https://www.iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf).

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
