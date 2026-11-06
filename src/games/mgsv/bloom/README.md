# Bloom

**Bloom Type** selects Vanilla or Enhanced (the default). **Bloom** controls strength, including fullscreen lens flares. Zero removes their contribution. Both modes reuse the game's passes and bindings with RGBA16F outputs.

Vanilla preserves the native bloom equations and HDR bridge. Its first two reductions filter encoded color; the bright pass decodes it, and later images contain linear light. Explicit conversions and clamps preserve the native view behavior without reproducing 8-bit rounding.

Enhanced blends `lerp(scene, filtered_light, strength * 0.1)` in linear light before grading. At 100, the result contains 90% sharp scene and 10% filtered light. Normalized filters redistribute light instead of adding the blurred image on top. The retained native lens flares are additive.

The Enhanced path decodes scene color before the first reduction. The second reduction integrates cubic filter weights over each source pixel to keep small moving lights stable. Each blur averages four samples offset by a quarter of a working pixel on each axis, keeping the glow close to its source. Final reconstruction uses four-sample [cubic B-spline filtering](https://developer.nvidia.com/gpugems/gpugems2/part-iii-high-quality-rendering/chapter-20-fast-third-order-texture-filtering) to smooth the coarse grid without ringing. RGB stays unclipped through the blur; there is no brightness threshold or shadow mask.

The working region remains 246x138 inside the native buffers. The preparation pass compensates for the half-texel offset in the unchanged [final copy](../copy/CopyRenderBuffer_0x83272BCB.ps_5_0.hlsl). Lens flares retain their native arrangement and clip their source signal per channel. The fixed grid limits fine detail; this is a compact scattering approximation rather than a measured lens model.

Resource upgrades follow the identified bloom chain so unrelated draws using the same shaders keep their original path. New float images start cleared, and native clears continue to reach them. One image is later reused for DoF controls, which retain their original 0-1 range without color conversion. Reconstruction's [post-processing support](../reconstruction/README.md) handles output dimensions and the final crop when upscaling is active.

Build `mgsv`, then compare both types in the ACC/menu, daylight, dark interiors, fog, and bright moving lights. Check zero, intermediate, and full strength, screen edges, flares, DoF, motion blur, and Native/FSR/DLSS quality changes.

