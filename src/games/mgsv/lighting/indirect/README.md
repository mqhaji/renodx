# Diffuse indirect lighting

GT-VBGI supplies ambient occlusion and diffuse light bounced from camera-visible surfaces. It runs on D3D11 before both native skin-scattering passes. Specular transport now belongs to the separate [DX12 reflection chain](../reflections/README.md); the diffuse chain no longer traces reflection rays, integrates specular sectors, or allocates specular histories.

## Files and reading order

| File | Responsibility |
|---|---|
| [runtime.hpp](runtime.hpp) | Settings, input checks, resource reuse, scheduling, dispatches, and state restoration; start at ApplyLightPasses, then follow ao::Apply |
| [ambient_diffuse.hpp](ambient_diffuse.hpp) | Native ambient snapshots and identification of the original diffuse source |
| [source_preparation.hpp](source_preparation.hpp), [source_radiance.hlsli](source_radiance.hlsli) | Linear source radiance prepared beside the native diffuse copy |
| [trace/gtvbgi.cs_5_0.hlsl](trace/gtvbgi.cs_5_0.hlsl) | Shared visibility-bitmask search for diffuse AO and GI |
| [compose/gtvbgi_add.cs_5_0.hlsl](compose/gtvbgi_add.cs_5_0.hlsl) | Ambient-only AO and diffuse GI application |
| [../environment_specular.hpp](../environment_specular.hpp) | Shared native environment capture, material tables, and reflection composition bindings |

Native shader callbacks are registered in [../../shaders.hpp](../../shaders.hpp). [../../materials/capture.hpp](../../materials/capture.hpp) owns the records of materials that bake a cube map into albedo. [../../rendering/producer_records.hpp](../../rendering/producer_records.hpp) publishes camera, motion, source, and material-table associations in submission order.

## Native inputs and placement

| Native stage | Input or output used here |
|---|---|
| Sky ambient, SSLighting2_SH_SkyLight_Encode (0x4FBDBB39) | Identifies ambient lighting, separate from direct shadows |
| BilateralUpscale2x2 (0x844CE327) | Pixel t0 half-resolution ambient, t2 full-resolution depth, t3 phase weights, s8 sampler, and b5 sampling constants |
| CopyBuffer (0xD71E843E) | Pixel t0 original diffuse and t1 material; native target 0 retains the skin copy, added target 1 stores FP32 source radiance |
| First skin scatter (0x6DAF0C4A) | Compute t0 copied diffuse, t1 depth, t2 normals, and b0 geometry constants; custom diffuse lighting runs immediately before it |
| Scene camera record | The submitted camera sample, which selects the noise layer |
| Native cube-material draws | Linear color without the baked cube, and a record of what the draw wrote; every other material supplies native albedo directly |

The order is ambient/direct lighting, diffuse copy, custom AO/GI, both skin scatters, deferred composition and sky/forward work, then stochastic reflections at the native late compositor. Enhanced shadows have already affected direct light. AO does not darken that direct light again.

## Follow one invocation

1. **Collect the actual inputs.** The ambient callback snapshots its half-resolution image, phase weights, sampler, and mutable constant-buffer range. Native clears invalidate it in GPU order. The scatter hook checks dimensions, formats, generations, and aliases before retaining its bound inputs. A retained COM object protects lifetime, not pixels.
2. **Submit the native prefix.** The coordinator executes recorded producers before the custom chain, then resumes the original scatter. Published copy metadata identifies the original diffuse image and its prepared source. Repeated interception or cached-list replay must not apply diffuse lighting twice.
3. **Prepare source radiance once.** The native CopyBuffer draw also writes material color without any baked cube, times original diffuse lighting, into an FP32 target. RGB is linear and pre-exposed; alpha is 0 only where a cube material was changed after its draw and native albedo had to stand in. Unsupported preparation retains direct source loads. Receiver albedo is applied later by deferred composition.
4. **Trace AO and GI together.** Each importance-sampled slice searches both directions. A depth slab covers an angular interval, and a 32-bit mask tracks occupied sectors. Newly occupied sectors gather outgoing diffuse radiance once. Rejected or black source light still blocks visibility. Foliage normals receive the established continuous view-facing adjustment and bounded surface offset.
5. **Apply.** Full-resolution application interpolates the trace result with depth weights, reconstructs native ambient diffuse, applies AO only there, preserves direct diffuse, and adds GI without another AO multiplier. Both the original diffuse image and eligible skin-copy RGB are updated, preserving their different alpha meanings.

The trace's noise changes with the camera sample, and the temporal upscaler accumulates it. The chain has no history or spatial filter of its own: with DLSS or FSR they cost about 0.1 ms and made no visible difference at Mother Base in rain (2026-11-18). With Reconstruction Off, a comparison-only mode, the result is noisier.

Optional coarse depth sampling builds nearest reverse-Z reductions. A GI lookup descends to the correct full-resolution owner before reading radiance. The diffuse lighting grid divides each render axis by 1, 2, 4, or 8, rounding up; labels show total pixel fractions. Neither option changes the reflection grid, which uses full render resolution.

## Color and missing inputs

Native albedo's sRGB view decodes base color once. Native diffuse/specular lighting is already linear and pre-exposed. The source, trace, and final light arithmetic all use linear light. Depth, normal, visibility, and material record data receive no gamma conversion. Later scene textures may contain encoded color despite their floating-point format.

In a view that uses Enhanced indirect lighting, the six supported deferred compositions skip native diffuse AO; custom AO was applied to reconstructed ambient earlier. They take that path only when Global Illumination is Enhanced and the host bound the environment capture for the draw, so a view without the custom passes keeps the native composition, including native AO. Missing ambient reconstruction skips AO while allowing valid GI. Vanilla restores native composition. Native AO producers still run in both modes: their result scales the environment light that unresolved reflection rays fall back to.

The shared material/environment capture and reflection output contracts are documented in the [reflection guide](../reflections/README.md). A pixel without a record is an ordinary material whose albedo needs no correction. Coverage views report missing material only for a cube material that was changed after its draw.

## Controls and debugging

Global Illumination selects Vanilla or Enhanced for diffuse AO/GI and reflections together; one check in `../indirect_lighting_mode.hpp` admits the presented main view and every pass asks it. Enhanced needs the RenoDX tone mapper: with the Vanilla tone mapper selected the mode is off, the controls in both Indirect Lighting sections are disabled, and a red note above them says why. Diffuse Occlusion Strength and Diffuse GI Strength change diffuse application. Reflection Strength blends the complete specular replacement, and Specular Occlusion Strength blocks its environment fallback. These four strength controls are under Indirect Lighting (Advanced). The reflection ray supplies that directional visibility; diffuse AO is not multiplied into the specular result. Setting a strength to zero changes the image, not the number of trace passes. Diffuse resolution, direction count, reach, and thickness affect this diffuse chain only.

Diffuse debug views show source coverage, the traced AO and GI, and reconstructed ambient/direct diffuse. Reflection views show reflected light, environment sampling weight, input coverage, and captured environment. Environment weights can exceed one before denoising. Magenta in a reflection view means qualified reflection data is unavailable. A black reflected-light pixel can be a valid miss or a dark hit.

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
