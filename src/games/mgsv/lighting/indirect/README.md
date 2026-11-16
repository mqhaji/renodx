# Diffuse indirect lighting

GT-VBGI supplies ambient occlusion and diffuse light bounced from camera-visible surfaces. It runs on D3D11 before both native skin-scattering passes. Specular transport now belongs to the separate [DX12 reflection chain](../reflections/README.md); the diffuse chain no longer traces reflection rays, integrates specular sectors, or allocates specular histories.

## Files and reading order

| File | Responsibility |
|---|---|
| [runtime.hpp](runtime.hpp) | Settings, input checks, resource reuse, scheduling, dispatches, and state restoration; start at ApplyLightPasses, then follow ao::Apply |
| [ambient_diffuse.hpp](ambient_diffuse.hpp) | Native ambient snapshots and identification of the original diffuse source |
| [source_preparation.hpp](source_preparation.hpp), [source_radiance.hlsli](source_radiance.hlsli) | Linear source radiance prepared beside the native diffuse copy |
| [trace/gtvbgi.cs_5_0.hlsl](trace/gtvbgi.cs_5_0.hlsl) | Shared visibility-bitmask search for diffuse AO and GI |
| [denoise/gtvbgi_temporal.cs_5_0.hlsl](denoise/gtvbgi_temporal.cs_5_0.hlsl) | Camera/object reprojection, history validation, and accumulation |
| [denoise/gtvbgi_filter.cs_5_0.hlsl](denoise/gtvbgi_filter.cs_5_0.hlsl) | Geometry-aware spatial filtering |
| [compose/gtvbgi_add.cs_5_0.hlsl](compose/gtvbgi_add.cs_5_0.hlsl) | Ambient-only AO and diffuse GI application |
| [../environment_specular.hpp](../environment_specular.hpp) | Shared native environment capture, material tables, and reflection composition bindings |

Native shader callbacks are registered in [../../shaders.hpp](../../shaders.hpp). [../../materials/capture.hpp](../../materials/capture.hpp) owns clean material capture. [../../rendering/producer_records.hpp](../../rendering/producer_records.hpp) publishes camera, motion, source, and material-table associations in submission order.

## Native inputs and placement

| Native stage | Input or output used here |
|---|---|
| Sky ambient, SSLighting2_SH_SkyLight_Encode (0x4FBDBB39) | Identifies ambient lighting, separate from direct shadows |
| BilateralUpscale2x2 (0x844CE327) | Pixel t0 half-resolution ambient, t2 full-resolution depth, t3 phase weights, s8 sampler, and b5 sampling constants |
| CopyBuffer (0xD71E843E) | Pixel t0 original diffuse and t1 material; native target 0 retains the skin copy, added target 1 stores FP32 source radiance |
| First skin scatter (0x6DAF0C4A) | Compute t0 copied diffuse, t1 depth, t2 normals, and b0 geometry constants; custom diffuse lighting runs immediately before it |
| Native motion composition | Matching composed velocity, object classification, depth, and camera sample |
| Native material draws | Clean linear color and exact owner codes, including supported wet-material and decal writes |

The order is ambient/direct lighting, diffuse copy, custom AO/GI, both skin scatters, deferred composition and sky/forward work, then stochastic reflections at the native late compositor. Enhanced shadows have already affected direct light. AO does not darken that direct light again.

## Follow one invocation

1. **Collect the actual inputs.** The ambient callback snapshots its half-resolution image, phase weights, sampler, and mutable constant-buffer range. Native clears invalidate it in GPU order. The scatter hook checks dimensions, formats, generations, and aliases before retaining its bound inputs. A retained COM object protects lifetime, not pixels.
2. **Submit the native prefix.** The coordinator executes recorded producers before the custom chain, then resumes the original scatter. Published copy metadata identifies the original diffuse image and its prepared source. Repeated interception or cached-list replay must not apply diffuse lighting twice.
3. **Prepare source radiance once.** The native CopyBuffer draw also writes clean material color times original diffuse lighting into an FP32 target. RGB is linear and pre-exposed; alpha marks material coverage. Unsupported preparation retains direct source loads. Receiver albedo is applied later by deferred composition.
4. **Trace AO and GI together.** Each importance-sampled slice searches both directions. A depth slab covers an angular interval, and a 32-bit mask tracks occupied sectors. Newly occupied sectors gather outgoing diffuse radiance once. Rejected or black source light still blocks visibility. Foliage normals receive the established continuous view-facing adjustment and bounded surface offset.
5. **Accumulate matching history.** The host requires matching scene identities and consecutive submitted camera samples. Native scene and scatter geometry centers already include each scene's projection jitter, so camera reprojection uses those centers without another jitter offset. Object motion is converted separately from its native packed form. The shader validates each bilinear history tap against geometry, adjusts GI for exposure, and limits history against current neighbors. AO is not exposure-scaled. AO and GI share a dispatch while retaining their own rejection statistics.
6. **Filter and apply.** The stochastic spatial kernel shares geometry checks across AO and GI. History stores the temporal result before spatial filtering, so blur is not repeatedly accumulated. Full-resolution application reconstructs native ambient diffuse, applies AO only there, preserves direct diffuse, and adds GI without another AO multiplier. Both the original diffuse image and eligible skin-copy RGB are updated, preserving their different alpha meanings.

Optional coarse depth sampling builds nearest reverse-Z reductions. A GI lookup descends to the correct full-resolution owner before reading radiance. The diffuse lighting grid divides each render axis by 1, 2, 4, or 8, rounding up; labels show total pixel fractions. Neither option changes the reflection grid, which uses full render resolution.

## Color and missing inputs

Native albedo's sRGB view decodes base color once. Native diffuse/specular lighting is already linear and pre-exposed. The source, accumulation, filter, and final light arithmetic all use linear light. Depth, normal, visibility, and ownership data receive no gamma conversion. Later scene textures may contain encoded color despite their floating-point format.

With Diffuse Lighting enabled, the six supported deferred compositions skip native diffuse AO. Custom AO is applied to reconstructed ambient earlier. This bypass follows the setting: missing custom inputs can therefore leave diffuse without AO. Missing ambient reconstruction skips AO while allowing valid GI. Diffuse Lighting set to Vanilla restores native composition. Native AO producers still run in both modes.

The shared material/environment capture and reflection output contracts are documented in the [reflection guide](../reflections/README.md). Missing clean material remains visible in coverage views; it does not mean the game failed to draw that surface.

## Controls and debugging

Diffuse Lighting selects Vanilla or Enhanced. Diffuse Occlusion Strength and Diffuse GI Strength change diffuse application. Reflections has its own Vanilla / Enhanced control, with Reflection Strength for the complete specular replacement and Specular Occlusion Strength for blocking its environment fallback. These four strength controls are under Indirect Lighting (Advanced). The reflection ray supplies that directional visibility; diffuse AO is not multiplied into the specular result. Setting a strength to zero changes the image, not the number of trace passes. Diffuse resolution, direction count, reach, thickness, temporal filtering, and spatial filtering affect this diffuse chain only.

Diffuse debug views show source coverage, AO/GI stages, accepted history, and reconstructed ambient/direct diffuse. Reflection views show reflected light, environment sampling weight, input coverage, and captured environment. Environment weights can exceed one before denoising. Magenta in a reflection view means qualified reflection data is unavailable. A black reflected-light pixel can be a valid miss or a dark hit.

## Build and gameplay checks

Follow the [addon deployment checks](../../README.md#build-and-deploy). Check skin, foliage, unlit blockers, wet materials, decals, loading transitions, moving cameras, exposure changes, and Native/Auto reconstruction. Confirm that AO only darkens ambient light, GI is added once, and both skin passes remain intact. Fixture results do not establish gameplay quality or a speedup.

Measure Enhanced / Vanilla / Enhanced at a fixed camera with probe recording, profiling, and reload disabled. Separate CPU submission cost from GPU stage time. The new reflection chain must be measured alongside the removed specular work before claiming a net improvement.

## Credits and license

The GT-VBGI core is adapted from [Mirko Salm (TinyTexel)'s reference](https://www.shadertoy.com/view/lfdBWn), using its MIT license option. The files in this directory use the MIT license below. Source headers refer here, and the compiled addon includes this notice in its Third-Party Licenses pane.

The visibility-bitmask method is described by Therrien, Levesque, and Gilet in [Screen Space Indirect Lighting with Visibility Bitmask](https://arxiv.org/abs/2301.11376).

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
