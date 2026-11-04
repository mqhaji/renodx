# Native shadow and lighting notes

These findings support the [shadow implementation](README.md). Shader measurements describe inspected frames and quality settings; they are not universal scene constants.

## Shadow maps

Native shadowed light shaders sample depth at `t3` with comparison sampler `s15`. Their original four-tap filter converts average occlusion to visibility with `(1 - average)^2`. Scene constants are at `b2`; local light and material constants are at `b3` and `b4`.

| Light | Inspected map | Projection |
|---|---|---|
| Sun / Moon | 8192x8192, four 4096x4096 tiles | Four cascades in one atlas |
| Spot | 4096x4096 | Perspective; divide projected coordinates by `w` |
| Point | 8192x4096, two 4096x4096 halves | Dual paraboloid; not a cubemap |

The sun atlas used R24G8 typeless storage with an R24 UNORM depth view. The captured projection scale was 0.15202, with cascade multipliers 1, 0.41376, 0.20271, and 0.10708. The corresponding extents were about 13.16, 31.8, 64.9, and 122.9 native units, with texel sizes 0.00321, 0.00776, 0.01585, and 0.03002. Native units have not been independently calibrated to meters.

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

A projection diagonal changes with rotation; the length of a row or column gives the scale for this captured uniformly scaled transform. Spotlight perspective-depth reconstruction and point-light seam handling require their own checks before changing the soft-shadow math.

Increasing cascade range spreads the same texels over more ground. Increasing resolution alone cannot add absent casters. Virtual shadow maps would also require page allocation, caster rendering, and cache invalidation; changing a sampling shader would not supply those systems.

## Foliage

Inspected grass used a masked G-buffer pass plus a forward softblend overlay. Bend can shadow the geometry represented in scene depth. The forward `Prim_Poly_Softblend_DL_VF` shader (`0x94C2F79B`) receives precombined vertex lighting; multiplying its output would also darken ambient light and fog.

The inspected overlay had shadow-map views at `t8` and `t14`, depth at `t13`, and scene constants at `b2`; comparison-sampler availability was not established. The [ocean shader](../tonemap/Material/Primitive_TppFourierOceanHQ_0x1A96649E.ps_5_0.hlsl) demonstrates native forward cascade sampling, but that alone does not establish a suitable grass lighting component to modify.

## Sun and moon controller

Read-only Ghidra inspection on September 21, 2026 used retail 1.0.15.4 EN, image base `0x140000000`. Development-executable names were matched by structure and remain inferred unless independently identified. Addresses below are retail.

| Function | Role |
|---|---|
| `0x1406B0F20` | Calls the atmosphere update |
| `0x14079E3A0` | Updates atmosphere each frame |
| `0x14079EA50` | Recomputes sun/moon directions when time changes |
| `0x14079CE30` | Updates the directional-light controller |
| `0x14079CE70` | Chooses sun, moon, or no directional light |
| `0x14079D330` | Builds the selected direction |
| `0x140262480` | Sets shadow-edge adjustment |
| `0x140262080` | Sets light direction; used by SmoothTimeCycle |

The atmosphere embeds its controller at `+0x1F0`. Sun/moon vectors are at atmosphere `+0x140/+0x160`, fades at `+0x1D0/+0x1D4`, and signs at `+0x1DC/+0x1E0`. Atmosphere `+0x3E0` bit 0 marks changed time. Sun elevation is `-direction.y`.

The controller chooses sun while its weight exceeds 0.0001 or exceeds the moon weight, no light when moon weight is at most 0.0001, and moon otherwise. Color and intensity blend during the transition, but direction switches in one frame. The moon weight rises across a 0.3-degree band below the sun's end angle.

| Controller offset | Observed use |
|---|---|
| `+0x10/+0x20`, `+0x30/+0x40` | Fixed sunrise/sunset and moonrise/moonset vectors |
| `+0x60/+0x70` | Copies of current sun/moon directions |
| `+0x88/+0x8C` | Sun/moon elevation limits |
| `+0x90/+0x94` | Sun fade start/end angles; property-name mapping is incomplete |
| `+0xBC` | Stepped-movement interpolation time, default 3 seconds |
| `+0xC8` | Selected mode: 0 none, 1 sun, 2 moon |
| `+0xCC`, bit 3 | Enables stepped movement |
| `+0xD0` | Stepped-movement divisions, default 175 |

Below the selected elevation limit, a nonzero fixed vector can replace the calculated direction. Stepped movement latches targets above an angular threshold and smoothsteps between them. Its threshold is `2 * sin(((90 - limit) / divisions) * 0.5 degrees)`.

The weather update at `0x1406B96E0` sets stepped movement through `0x1406B3120` (enable, ID `0x23A`), `0x1406B3900` (seconds, ID `0x238`), and `0x1406B2FE0` (divisions, ID `0x239`). These IDs were matched by setter role. The inspected setters did not write the four fade/limit fields.

Property registration at `0x140770B90` includes `dirLightFadeStart` (`0x23C`), `dirLightFadeLength` (`0x240`), `dirLightSunLimitAngle` (`0x264`), and `dirLightMoonLimitAngle` (`0x268`). The reader that fills the controller and runtime values across weather presets remain unverified; notably, the code uses `+0x94` as an end angle although the property name says length.

SmoothTimeCycle's large-angle snap can bypass smoothing at the sun/moon switch. Clearing the stepped-movement flag does not remove that separate direction jump. No sun/moon crossfade change is implemented here.
