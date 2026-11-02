# GT-VBGI + GTSO

MGSV's screen-space lighting combines ambient occlusion, bounced light, and environment-reflection occlusion in one shared trace and filter chain.

## MGSV integration

The [trace shader](gtvbgi.cs_5_0.hlsl) calculates AO, GI, and directional visibility in one geometry search. Optional [depth reductions](gtvbgi_depth_mips.cs_5_0.hlsl) keep the nearest depth in each smaller region; GI finds the matching full-resolution pixel before reading its light. One [temporal pass](gtvbgi_temporal.cs_5_0.hlsl) accumulates the three signals, and each [spatial pass](gtvbgi_filter.cs_5_0.hlsl) filters them together. The [application pass](gtvbgi_add.cs_5_0.hlsl) reconstructs them at full resolution, writes `diffuse * AO + GI` while preserving alpha, and resolves specular visibility. These passes run after native subsurface scattering. Deferred composition applies the environment-reflection correction before fog and tonemapping. The default chain uses six dispatches; optional depth reductions add four. Bent normals and specular visibility add no geometry search or dispatch, but still require calculation, texture storage, and bandwidth.

MGSV supplies normals and positions in camera coordinates. The albedo texture's sRGB view decodes base color when sampled. Diffuse and specular lighting are linear values already scaled by the game's exposure. Bounce sources use albedo times the original diffuse copy from before skin scattering, with an optional specular contribution. The receiving surface's albedo is applied later by the game's composition shader. A valid black result remains separate from missing data, and the native lighting alpha is preserved.

One Off / GT-VBGI + GTSO control enables the complete effect, with separate AO, GI, and specular-occlusion strengths. Missing or incompatible trace inputs skip the custom lighting. Missing directional or environment data retains native specular occlusion. IS-FAST noise advances when scene reconstruction or qualified AO or GI accumulation can combine frames.

## Directional visibility and specular occlusion

The trace sums the directions left open by its coverage mask, weighted for diffuse light. This sum, called a directional moment, becomes a bent normal when normalized. It uses the same receiver orientation as AO, independently of GI source rejection and AO strength. Filtering and full-resolution reconstruction preserve the moment's direction and size; normalization happens afterward. The original material normal remains the shading normal.

GTSO estimates reflection visibility by overlapping a cone of open directions with a cone representing the material's reflection spread. Raw visibility controls the first cone's opening; roughness controls the second. The reflected direction comes from the native material normal. This follows Jimenez et al., Section 6 and Appendix B of [Practical Realtime Strategies for Accurate Indirect Occlusion](https://www.iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf).

Native roughness is treated as perceptual roughness, with GGX alpha equal to its square; this mapping has not been fitted to MGSV's reflection model. The result fades toward native occlusion near grazing or back-facing surfaces and when the bent direction is incompatible with the material normal. A single cone cannot preserve every separate opening or produce exact mirror occlusion.

Two sun variants and the startup menu's environment-only variant save their environment reflection in an RGBA16F texture. The six deferred composition variants change only this contribution, using full-resolution visibility and confidence. Specular Occlusion Strength defaults to 75% and blends from native occlusion at 0% to GTSO at 100%, reduced by confidence. Direct highlights, separate rain reflections, and native SSR keep their existing treatment.

## Accumulation and filtering

Temporal and spatial switches, filter type, radius, passes, and taps are shared. Directional visibility uses the AO history controls; GI keeps its own controls. Each signal retains separate history values and validity at the lighting resolution. Temporal accumulation runs first, and history stores its result so spatial blur does not feed back into later frames.

Temporal accumulation follows qualified MGSV motion and camera transforms, checks previous depth and surface normals, and limits history using nearby current samples. Previous GI RGB is adjusted for exposure changes; AO and directional visibility are not exposure-scaled. Directional history rotates between the previous and current camera bases. Cold, disabled, or unavailable history uses current samples without reading previous pixels. Valid black GI and fully blocked AO remain valid. Object motion supplies screen displacement rather than previous object depth or deformation, so those changes can reject history.

| Default | AO and directional visibility | GI |
|---|---|---|
| History weight | 90% | 90% |
| Depth tolerance | 20% | 5% |
| Normal threshold | 0 | 0.9 |
| History clamp, standard deviations | 0.1 | 1 |

Temporal accumulation and spatial filtering default to On. The shared filter defaults are A-Trous, radius 8, three passes, and five taps per axis. Spatial filtering reduces mixing across depth and normal changes. A-Trous uses a two-dimensional binomial kernel; Separable runs horizontal and vertical Gaussian passes. With one A-Trous pass, taps determine the footprint and Radius is unused. The UI disables Radius in that case and disables taps for the unselected filter type. Off preserves the inputs through the same shader paths. A zero history weight disables accumulation for those signals. Only lighting on/off controls log changes; numeric adjustments stay quiet.

AO uses RG16F textures with visibility in R and validity in G. GI uses RGBA16F with radiance in RGB and validity in alpha. Directional visibility uses RGBA16F with the unnormalized moment in RGB and raw visibility in alpha; alpha -1 marks invalid data. The signals share camera snapshots and one geometry-history pair containing FP32 linear depth and two packed 16-bit normal coordinates. AO strength changes reset AO history because the trace already includes that strength; directional visibility stays independent. Accumulation and neighborhood statistics use FP32 math.

Lighting Debug View includes AO/GI histories, raw and filtered bent normals, native roughness, captured environment reflection, specular visibility, and confidence. Bounce Lighting and Filtered GI both show the selected final GI result; disable temporal and spatial filtering to inspect raw GI. Magenta marks missing inputs, while valid black remains black. These views replace diffuse lighting, so surface color, native specular, fog, and later processing affect their appearance.

See the [addon guide](../README.md#indirect-lighting) for controls and [rendering contracts](../docs/ARCHITECTURE.md) for ownership and evidence limits.

## Credits and license

The files in this directory use the MIT license below. Their source headers refer here instead of repeating the full notice. Include this README when distributing these sources. The compiled addon carries the notices in its Third-Party Licenses pane.

| Part | Source and contributors |
|---|---|
| GT-VBGI trace | Mirko Salm (TinyTexel), [GT-VBGI (diffuse)](https://www.shadertoy.com/view/lfdBWn); the adapted core uses its MIT license option |
| Visibility bitmask method | Olivier Therrien, Yannick Levesque, and Guillaume Gilet, [Screen Space Indirect Lighting with Visibility Bitmask](https://arxiv.org/abs/2301.11376) |
| Temporal accumulation and spatial filtering | Toru77's Falcom Engine Plus [temporal](https://github.com/Toru77/renodx/blob/3360c8dff84ae8acf3558ddb80b00f3cbd1ac90e/src/games/falcomengine-plus/world/shaders/world_rtgi_temporal.cs_5_0.hlsl) and [spatial](https://github.com/Toru77/renodx/blob/3360c8dff84ae8acf3558ddb80b00f3cbd1ac90e/src/games/falcomengine-plus/world/shaders/world_rtgi_filter.cs_5_0.hlsl) passes; RenoDX copyright Carlos Lopez Jr. |
| Specular occlusion | Adapted from [three.js GTAONode.js](https://github.com/mrdoob/three.js/blob/62d53aaf7501f2b28f7a574e59692ffa3d404bef/examples/jsm/tsl/display/GTAONode.js) |
| MGSV implementation and adaptations | Musa Haji and ZioPao |

```text
Copyright (c) 2024, 2026 Musa Haji
Copyright (c) 2026 ZioPao
Copyright (c) 2025 Carlos Lopez Jr.
Copyright (c) 2010-2026 three.js authors
```

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
