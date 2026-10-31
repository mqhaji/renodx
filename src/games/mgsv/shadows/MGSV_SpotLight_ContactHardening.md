# Spot contact-hardening shadows

The [spot shader](SSLighting2_Spot_0x08292A5E.ps_5_0.hlsl) and its [distance-attenuation variant](SSLighting2_Spot_0x1D4DB9C5.ps_5_0.hlsl) replace the game's fixed shadow filter. Each light draw uses its own projection and settings. The [sun filter](MGSV_SunShadow_ContactHardening.md) splits its map into distance ranges called cascades; a spot light uses one perspective shadow map.

## Native inputs

| Input | Binding or interpretation |
|---|---|
| Shadow map | `t3`, R24 UNORM depth view; 4096x4096 in the inspected snapshot |
| Comparison sampler | `s15` |
| Scene constants | `b2`, including the current light's shadow projection |
| Light constants | `b3`, `m_lightParams[11]` |
| Material constants | `b4`, `m_materials[8]` |
| Shadow resolution | `m_shadowMapResolutions.y` |

Shadow UV and comparison depth come from dividing the light-space coordinate by its `w`. The native filter uses four comparison taps and a squared visibility response.

## Current filter

The penumbra is a shadow's soft border. Its estimated width determines the radius sampled in shadow-map pixels, called texels.

1. Estimate native view-space texel size from the receiver's light depth, the cone half-angle in `m_materials[6].x`, and map resolution: `2 * depth * tan(halfAngle) / resolution`.
2. Build a nine-tap penumbra mask. The implementation linearizes receiver and blocker depths using the light projection before calculating relative separation; using raw perspective depth would distort the radius.
3. Convert softness to texels and evaluate a rotated 32-tap Vogel disk with `GatherCmp` where the mask is nonzero. Rotation uses interleaved gradient noise.
4. Apply the configured cone-edge falloff and occlusion darkness, retaining the native `(1 - average)^2` response. Occlusion stays bounded to prevent that expression from brightening an over-darkened shadow.

Spot softness, response, mask smoothing, darkness, and cone falloff are separate from the sun controls. [Screen-space shadows](MGSV_ScreenSpaceShadows.md) provide an independent near-contact term.

## Verification

Compare Off/On across cone centers and rims, sloped receivers, camera motion, changing light distance, and shadow-map quality settings. Exercise both registered permutations. Native texel-scale estimates do not independently establish a physical meter scale.

GPU cost depends on affected pixels and the number of light draws. A historical snapshot contained roughly 25 spot draws; that observation is not a fixed draw count or a performance measurement. Point shadows use a separate [dual-paraboloid implementation](SSLighting2_Point_0x80F0A0BC.ps_5_0.hlsl), not a cube-map extension of this filter.
