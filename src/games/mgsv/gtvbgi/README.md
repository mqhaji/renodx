# GT-VBGI

MGSV's screen-space lighting combines ambient occlusion, bounced light, and environment-reflection occlusion in one shared trace and filter chain.

## MGSV integration

The [trace shader](gtvbgi.cs_5_0.hlsl) calculates AO, GI, and specular visibility in one geometry search. Optional [depth reductions](gtvbgi_depth_mips.cs_5_0.hlsl) keep the nearest depth in each smaller region; GI finds the matching full-resolution pixel before reading its light. One [temporal pass](gtvbgi_temporal.cs_5_0.hlsl) accumulates the three signals, and each [spatial pass](gtvbgi_filter.cs_5_0.hlsl) filters them together. The [application pass](gtvbgi_add.cs_5_0.hlsl) reconstructs them at full resolution, writes `diffuse * AO + GI` while preserving alpha, and resolves specular visibility. These passes run after native subsurface scattering. Deferred composition applies the environment-reflection correction before fog and tonemapping. The default chain uses six dispatches; optional depth reductions add four.

MGSV supplies normals and positions in camera coordinates. The albedo texture's sRGB view decodes base color when sampled. Diffuse lighting is linear and already scaled by the game's exposure. Bounce sources use albedo times the original diffuse copy from before skin scattering. The receiving surface's albedo is applied later by the game's composition shader. A valid black result remains separate from missing data, and the native lighting alpha is preserved.

One Off / GT-VBGI control enables the complete effect, with separate AO, GI, and specular-occlusion strengths. Missing or incompatible trace inputs skip custom lighting. Missing specular inputs retain native specular occlusion while AO/GI continue. IS-FAST noise advances when scene reconstruction or qualified AO or GI accumulation can combine frames.

## Specular occlusion

Each slice keeps a separate 32-bit visibility mask over the reflection cone, whose direction comes from the native shading normal and whose width comes from material roughness. The same geometry samples update diffuse and specular coverage. The trace sums visible and open angular areas; filtering combines these areas before dividing to obtain visibility. GI source rejection and AO strength do not change specular coverage.

Native roughness is treated as perceptual roughness, with GGX alpha equal to its square. The reflection-cone approximation follows Jimenez et al., Appendix B; it has not been fitted to MGSV's reflection model. The mask preserves separate blocked regions within each slice, but camera-visible depth, low slice counts, and narrow reflections still limit the estimate.

Sampling support, compatible materials, and agreement with the cone's expected open area determine confidence. Unsupported pixels retain native occlusion. Partly supported pixels blend toward it. This fallback also covers missing captures and samples rejected during full-resolution reconstruction.

Two sun variants and the startup menu's environment-only variant save their environment reflection in an RGBA16F texture. The six deferred composition variants change only this contribution, using full-resolution visibility and confidence. Specular Occlusion Strength defaults to 75% and blends from native occlusion at 0% to the bitmask estimate at 100%, reduced by confidence. Direct highlights, separate rain reflections, and native SSR keep their existing treatment.

## Accumulation and filtering

Temporal and spatial switches, filter radius, passes, and width are shared. AO and specular visibility use the occlusion history weight and depth/normal rejection controls; GI keeps its own controls. AO and specular history have separate clamps. Specular History Clamp defaults to 2 standard deviations and limits the visibility ratio without changing its angular sampling weight. Each signal retains separate history values and validity at the lighting resolution. Temporal accumulation runs first, and history stores its result so spatial blur does not feed back into later frames.

Temporal accumulation follows qualified MGSV motion and camera transforms, checks previous depth and surface normals, and limits history using nearby current samples. Previous GI RGB is adjusted for exposure changes; visibility is not exposure-scaled. Specular history reconstructs the previous reflection direction in its saved camera basis. Temporal, spatial, and full-resolution reconstruction reject incompatible roughness or reflection directions. Cold, disabled, or unavailable history uses current samples without reading previous pixels. Valid black GI, fully blocked AO, and valid zero-area specular samples remain separate from missing data. Object motion supplies screen displacement rather than previous object depth or deformation, so those changes can reject history.

| Default | AO | GI | Specular visibility |
|---|---|---|---|
| History weight | 90% | 90% | Shared with AO |
| Depth tolerance | 20% | 5% | Shared with AO |
| Normal threshold | 0 | 0.9 | Shared with AO |
| History clamp, standard deviations | 0.1 | 1 | 2 |

Temporal accumulation and spatial filtering default to On. The filter uses an A-Trous binomial kernel: its sample spacing grows between passes. Defaults are width 5, radius 8, and three passes. Radius limits spacing after the first pass and is disabled in the UI when only one pass runs. Off copies inputs through the same shader. A zero history weight disables accumulation for those signals. Only lighting on/off controls log changes; numeric adjustments stay quiet.

Each spatial pass keeps the center and samples one of four neighbor patterns, using IS-FAST noise separately from the trace. Neighbor weights are multiplied by four, and each signal retains its own valid-weight sum. Every output pixel is written. The pattern changes when scene reconstruction is selected and stays fixed otherwise. Lighting history is saved before filtering, so scene reconstruction smooths the filter's animated pattern. Sparse sampling can change the normalized result near edges.

The pass binder clears previous input views before reusing a ping-pong texture as an output, then binds the current inputs. Surrounding game state is restored once after the whole chain.

AO uses RG16F textures with visibility in R and validity in G. GI uses RGBA16F with radiance in RGB and validity in alpha. Specular visibility uses RGBA16F with visible/open angular areas in RG, roughness in B, and sampling support in alpha; alpha -1 marks missing data. The signals share camera snapshots and one geometry-history pair containing FP32 linear depth and two packed 16-bit normal coordinates. AO strength changes reset AO history because the trace already includes that strength; specular visibility stays independent. Accumulation and neighborhood statistics use FP32 math.

Lighting Debug View includes AO/GI histories, native roughness, captured environment reflections, and raw/temporal/filtered specular visibility. Accepted Specular History shows the history weight accepted at each pixel; changes there do not necessarily indicate unstable final lighting. Specular Confidence shows how much the custom estimate can replace native occlusion. Cyan means the sampled slices missed the reflection cone; magenta means missing inputs. GI Lighting shows the final bounce result; disable temporal and spatial filtering to inspect raw GI. These views replace diffuse lighting, so surface color, native specular, fog, and later processing affect their appearance.

See the [addon guide](../README.md#indirect-lighting) for controls and [rendering contracts](../docs/ARCHITECTURE.md) for ownership and evidence limits.

## Credits and license

The GT-VBGI core is adapted from [Mirko Salm (TinyTexel)'s reference](https://www.shadertoy.com/view/lfdBWn), using its MIT license option. The files in this directory use the MIT license below. Source headers refer here, and the compiled addon includes this notice in its Third-Party Licenses pane.

The visibility-bitmask method is described by Therrien, Levesque, and Gilet in [Screen Space Indirect Lighting with Visibility Bitmask](https://arxiv.org/abs/2301.11376). The reflection-cone approximation follows Appendix B of Jimenez et al., [Practical Realtime Strategies for Accurate Indirect Occlusion](https://www.iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf).

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
