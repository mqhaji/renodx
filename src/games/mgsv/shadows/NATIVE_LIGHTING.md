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

A projection diagonal changes with rotation; the length of a row or column gives the scale for this captured uniformly scaled transform. The PCSS fixtures check each light's depth convention independently. These synthetic checks do not establish runtime quality or performance.

The sun PCSS filter uses row-vector projection: `q = mul(float4(view_position, 1), m_shadowProjection).xyz`. Cascade 0 uses `q`; cascades 1 through 3 use `q * m_shadowProjection2[cascade - 1].w + m_shadowProjection2[cascade - 1].xyz`. Native shadow depth is `(1 + q.z) * m_shadowProjection2._m30`. Larger stored depth is closer to the light, so blockers have depth greater than the receiver. Dividing their positive depth gap by `length(m_shadowProjection._m02_m12_m22) * abs(cascade_scale * m_shadowProjection2._m30)` gives separation in game units. PCSS multiplies that separation by the tangent of the light's angular radius; it does not divide by raw blocker depth.

Spot shadow coordinates are already mapped to texture UV by `m_shadowProjection`; divide by projected `w` without another clip-to-UV conversion. Let `w_axis = M._m03_m13_m23`, `a = dot(M._m02_m12_m22, w_axis) / dot(w_axis, w_axis)`, and `b = M._m32 - a * M._m33`. Stored depth is `a + b / w`, and light-axis distance is `b / ((depth - a) * length(w_axis))`. This handles a rotated or translated perspective matrix. The native cone mask and distance fade are applied after filtering. Local PCSS uses emitter radius times the separation between receiver and blocker divided by blocker distance, bounded by the configured maximum radius.

Point shadow depth is `1 - distance * lightParams[7].z`. The native path offsets the receiver's camera-space Z before transforming it, reverses X in the light-relative vector, and projects into two paraboloid halves. PCSS keeps that input construction. Its projection matches the native two-texel hemisphere shrink and negative half-texel offset; each sample chooses its hemisphere from the sign of its light-space Z. Samples crossing the equator therefore read the other half, rather than clamping to the original tile. Radial comparisons intersect the sample direction with the receiver plane to reduce slope self-shadowing.

Increasing cascade range spreads the same texels over more ground. Increasing resolution alone cannot add absent casters. Virtual shadow maps would also require page allocation, caster rendering, and cache invalidation; changing a sampling shader would not supply those systems.

## Native light sizes

Read-only replay of the two Graphical Enhancements captures and the temporal/filter menu capture verified size inputs in the native highlight calculations. These captures predate the current PCSS implementation. Shader bytecode matched the native dump, and 13 selected constant blocks agreed between raw-byte and reflected-variable reads.

| Input | Native location | Observed values |
|---|---|---|
| Directional highlight size | PS b3, lightParams[1].w, byte 28 | 0.04361938685178757 in all three captures |
| Local-light source size | PS b3, lightParams[4].w, byte 76 | 0 in all 138 spotlight and 12 point-light draws inspected |

The local-light dump names the input size and its conversion AdjustLightSizeFromDistance. It computes approximately saturate(0.9 * size / adjusted_receiver_distance), then uses the result to adjust the specular light direction. Zero size also appears alongside nonzero RGB, so these are not merely empty light buffers. Draw counts include repeated volume draws and are not unique-light counts.

The directional value is passed into the same highlight-size role without the local-distance conversion. It numerically equals sin(2.5 degrees), within float precision. This is evidence for an angular proxy, not proof that the visible sun or a physical emitter has a 2.5-degree radius. Converting it to a 2.5-degree PCSS setting would give roughly 9.265 times our 0.27-degree setting's uncapped radius for the same blocker gap.

The captures include shadowed spotlights, but their point-light draws use unshadowed variants. They do not directly qualify the shadowed-point or distance-spot permutation, a separate moonlight case, or any nonzero local emitter size. The observations do not establish that every MGSV light always has size zero.

PCSS therefore keeps its manual source-size controls. A pure native-size multiplier would leave the observed local sources at zero and remove the added softness. Any future automatic policy needs a deliberate minimum or override for zero-size lights and separate directional calibration. Light range and cone angle describe where illumination reaches; they are not substitute emitter dimensions.

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
