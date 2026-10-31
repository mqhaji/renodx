# Sun contact-hardening shadows

The [sun shader](SunLight_DL_CS_ES_0x5AFD2E17.ps_5_0.hlsl) replaces the native fixed shadow filter with an estimate of shadow softness based on blocker separation. Its settings are exposed under Contact-Hardening Shadows. Screen-space shadow checks are a [separate term](MGSV_ScreenSpaceShadows.md) in the same draw.

## Native inputs

| Input | Binding or captured layout |
|---|---|
| Shadow map | `t3`, R24G8 typeless storage with an R24 UNORM SRV |
| Comparison sampler | `s15`; native comparison results measure occlusion |
| Captured atlas | 8192x8192, four 4096x4096 cascade tiles |
| Scene constants | `b2`, including shadow transforms and map resolutions |
| Shadowed sunlight | `0x5AFD2E17`, `SunLight_DL_CS_ES_ps.hlsl` |
| Unshadowed sunlight permutation | `0x712F7222`, `SunLight_DL_ES_ps.hlsl` |

The native filter averages four comparison samples and converts occlusion to lighting with `(1 - average)^2`. The replacement retains the native cascade selection/blending, cloud shadow, fades, and lighting composition.

## Current filter

The penumbra is a shadow's soft border. This filter estimates how wide that border should be, then samples a disk of shadow-map pixels (texels) to soften it.

1. Estimate softness from nine neighboring screen pixels. Each sample uses shadow coordinates and receiver depth calculated for that neighbor, along with the map's comparison result and blocker depth.
2. Average `occlusion * saturate(response * relativeSeparation^2)` over those taps. This smooths radius changes before the expensive filter.
3. Convert the softness setting into texels using the rotation-invariant projection scale and the selected cascade scale. Clamp the footprint to its atlas tile.
4. Where the mask is nonzero, evaluate a rotated 32-tap Vogel disk using `GatherCmp`, then retain the native squared visibility response. Rotation uses interleaved gradient noise.

The implementation samples the nearest depth at each mask tap rather than performing a broad blocker search. This is an approximation affected by shadow-map bias, cascade coverage, and missing geometry; it is not a complete physical light-size model.

## Projection findings

The captured shadow projection combines uniform scaling with rotation. The length of a row or column gives the scale. A diagonal value alone changes as the camera rotates and would make softness camera-dependent.

| Captured quantity | Value |
|---|---|
| Projection scale norm | 0.15202 |
| Cascade-0 extent, `2 / scale` | About 13.16 native view-space units |
| Cascade multipliers | 1, 0.41376, 0.20271, 0.10708 |
| Cascade texel sizes | About 0.00321, 0.00776, 0.01585, 0.03002 native units |

These measurements describe one captured quality setting. They do not independently calibrate native units to meters or establish behavior at other shadow resolutions.

The underlying captured values, with rows as reported:

```text
m_shadowProjection
  -0.12627   0.05348   0.06561   0.00000
   0.04163  -0.06335   0.12742   0.00000
   0.07370   0.12742   0.03798   0.00000
  -0.26470  -0.53163  -0.15322   1.00000

m_shadowProjection2
  -0.29537  -0.59321  -0.17096   0.41376
  -0.39704  -0.79742  -0.22981   0.20271
  -0.44216  -0.88802  -0.25593   0.10708
   0.01056   0.06900   0.12609   1.00000

m_shadowMapResolutions = 8192, 4096, 8192, 4096
```

## Verification

Check contact edges, distant softness, cascade transitions, slopes, grass, and camera rotation with the feature Off/On. Repeat at different shadow-map quality settings. Historical reports of camera-dependent softness require direct comparison before being considered resolved.

Diffuse and specular outputs are shared accumulation targets. A later readback includes other lighting; isolate the shader's contribution at its draw or use controlled final-image comparisons with exposure accounted for. The existing native-only captures do not validate the replacement filter or its GPU cost.
