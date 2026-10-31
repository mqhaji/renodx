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

The [trace shader](gtvbgi.cs_5_0.hlsl) calculates AO and GI in one geometry search. Optional [depth reductions](gtvbgi_depth_mips.cs_5_0.hlsl) keep the nearest depth in each smaller region. When GI uses a reduced-depth sample, it finds the matching full-resolution pixel before reading its light. [AO application](gtvbgi_apply_ao.cs_5_0.hlsl) brings visibility back to full resolution and darkens existing diffuse light. [GI addition](gtvbgi_add.cs_5_0.hlsl) then brings bounce light back to full resolution and adds it. These passes run after native subsurface scattering and before the game combines surface color and lighting, adds fog, and applies tonemapping.

MGSV supplies normals and positions in camera coordinates. The albedo texture's sRGB view decodes base color when sampled. Diffuse and specular lighting are linear values already scaled by the game's exposure. Bounce sources use albedo times the original diffuse copy from before skin scattering, with an optional specular contribution. The receiving surface's albedo is applied later by the game's composition shader. A valid black result remains separate from missing data, and the native lighting alpha is preserved.

One Off/GT-VBGI control enables the complete effect, with separate AO and GI strengths. Missing or incompatible inputs skip both. The addon uses IS-FAST noise, and the selected scene reconstruction smooths the final image over time. Separate lighting history, denoising, bent normals, and custom specular occlusion are not implemented.

See the [addon guide](../README.md#indirect-lighting) for controls and [rendering contracts](../docs/ARCHITECTURE.md) for ownership and evidence limits.
