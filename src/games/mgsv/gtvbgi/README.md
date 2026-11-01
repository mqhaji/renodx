# GT-VBGI

The implementation follows Mirko Salm's [GT-VBGI (diffuse) Shadertoy](https://www.shadertoy.com/view/lfdBWn), which extends GT-VBAO with diffuse light bouncing. It builds on [Screen Space Indirect Lighting with Visibility Bitmask](https://arxiv.org/abs/2301.11376) by Olivier Therrien, Yannick Levesque, and Guillaume Gilet. The adapted reference core uses its MIT license option; its attribution and license remain in the shader.

## What the Shadertoy does

The demo supplies depth, surface directions (normals), base colors (albedo), and direct light leaving each surface. It searches the depth image in planes, called slices, around the viewing direction. Each slice follows the camera's perspective and divides the receiving hemisphere into 32 angular sectors. A 32-bit mask records which sectors have been blocked. The sectors have equal weight for diffuse lighting, and slice directions are sampled according to that same weighting.

Each sampled surface is treated as having some thickness, so it blocks a range of directions. Its light contributes only through sectors that earlier samples have not covered: `fresh = covered & ~occupied`. All covered sectors then join `occupied`, preventing farther surfaces from shining through them. Optional backface rejection suppresses light from the back of a surface while keeping that surface opaque. Both sides of each slice are searched.

The demo combines the bounce with direct lighting as `direct + albedo * bounce`. This estimates light from surfaces visible to the camera; it cannot recover hidden or offscreen geometry. Its direct-light input uses linear AP1 RGB.

| Tab | Purpose |
|---|---|
| Common | Scene, sampling, camera, and shared mathematical helpers |
| Buffer A | Scene normals and input/accumulation state |
| Buffer B | Direct outgoing light and scene depth |
| Buffer C | GT-VBGI and optional temporal averaging |
| Buffer D | Comparison ray marcher; analytic geometry tracing is an optional mode |
| Image | Presentation, controls, and comparison views |

Buffer C can average the direct-plus-bounce image over time at the same pixel, restarting when the demo's inputs change. It does not follow moving objects or reproject history using motion vectors. Buffer D's default comparison also traces the screen's depth image; only its optional analytic mode traces the demo's actual geometry.

## MGSV integration

The [trace shader](gtvbgi.cs_5_0.hlsl) calculates AO and GI in one geometry search. Optional [depth reductions](gtvbgi_depth_mips.cs_5_0.hlsl) keep the nearest depth in each smaller region. When GI uses a reduced-depth sample, it finds the matching full-resolution pixel before reading its light. One [temporal pass](gtvbgi_temporal.cs_5_0.hlsl) accumulates both signals, and each [spatial pass](gtvbgi_filter.cs_5_0.hlsl) filters both. The [application pass](gtvbgi_add.cs_5_0.hlsl) brings their results back to full resolution, multiplies existing diffuse by AO, and adds GI in one write while preserving alpha. These passes run after native subsurface scattering and before the game combines surface color and lighting, adds fog, and applies tonemapping. The default chain uses six dispatches; optional depth reductions add four.

MGSV supplies normals and positions in camera coordinates. The albedo texture's sRGB view decodes base color when sampled. Diffuse and specular lighting are linear values already scaled by the game's exposure. Bounce sources use albedo times the original diffuse copy from before skin scattering, with an optional specular contribution. The receiving surface's albedo is applied later by the game's composition shader. A valid black result remains separate from missing data, and the native lighting alpha is preserved.

One Off/GT-VBGI control enables the complete effect, with separate AO and GI strengths. Missing or incompatible trace inputs skip both. IS-FAST noise advances when scene reconstruction or qualified AO or GI accumulation can combine frames. Bent normals and custom specular occlusion remain separate future work.

## Accumulation and filtering

The denoisers adapt the AO and GI temporal and spatial passes from [Toru77's Falcom Engine Plus](https://github.com/Toru77/renodx/tree/3360c8dff84ae8acf3558ddb80b00f3cbd1ac90e/src/games/falcomengine-plus), retaining their MIT notices. Temporal and spatial switches, filter type, radius, passes, and taps are shared. AO and GI keep separate history values, weights, and rejection controls at the selected lighting resolution. Temporal accumulation runs before spatial filtering, and history stores the temporal result so spatial blur does not feed back into later frames.

Temporal accumulation follows qualified MGSV motion and camera transforms, checks previous depth and surface normals, and limits history using nearby current samples. Previous GI RGB is adjusted for exposure changes; AO visibility is never exposure-scaled. Cold, disabled, or unavailable history uses current samples without reading previous pixels. Valid black GI and fully blocked AO remain valid. Object motion supplies screen displacement rather than previous object depth or deformation, so those changes can reject history.

| Default | AO | GI |
|---|---|---|
| History weight | 90% | 90% |
| Depth tolerance | 20% | 5% |
| Normal threshold | 0 | 0.9 |
| History clamp, standard deviations | 0.1 | 1 |

Temporal accumulation and spatial filtering default to On. The shared filter defaults are A-Trous, radius 8, three passes, and five taps per axis. Spatial filtering averages neighboring values while reducing mixing across depth and normal changes. A-Trous uses a two-dimensional binomial kernel; Separable runs horizontal and vertical Gaussian passes. With one A-Trous pass, taps determine the footprint and Radius is unused. The UI disables Radius in that case and disables taps for the unselected filter type. Off preserves both inputs through the same shader paths. A zero history weight disables accumulation for that signal. Only lighting on/off controls log changes; numeric adjustments stay quiet.

AO uses RG16F textures with visibility in R and validity in G. GI uses separate RGBA16F textures with radiance in RGB and validity in alpha. This keeps both values at the same precision while avoiding duplicated AO color channels. Both use one geometry-history pair containing FP32 linear depth and two packed 16-bit normal coordinates, avoiding two duplicate R32G32_UINT textures. Camera snapshots are shared, but one signal's history cannot validate the other. AO strength changes reset AO history because the trace already includes that strength. Accumulation and neighborhood statistics use FP32 math.

Lighting Debug View includes Raw AO, Temporal AO, Filtered AO, and separate AO/GI Accepted History views. Bounce Lighting and Filtered GI both show the selected final GI result; disable the shared temporal and spatial switches to inspect raw GI. These views replace diffuse lighting, so surface color, native specular, fog, and later processing still affect them. Compare AO and GI independently, including camera motion, moving characters, lighting changes, resolution switches, and disabled-path restoration.

See the [addon guide](../README.md#indirect-lighting) for controls and [rendering contracts](../docs/ARCHITECTURE.md) for ownership and evidence limits.
