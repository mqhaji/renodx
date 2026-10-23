# MGSV spot-light contact hardening — findings and plan

Status: shader identified, PCSS port implemented; screen-space shadows ported on
the same branch, not yet verified in a capture. Companion to
`MGSV_SunShadow_ContactHardening.md`, which covers the sun implementation that this
is intended to mirror. The screen-space term is described in
`MGSV_ScreenSpaceShadows.md`, *Spot lights*.

## The target shader

| | |
|---|---|
| CRC32 | `0x08292A5E` |
| Source | `SSLighting2_Spot_ps.hlsl` |
| Shadow map | `t3`, `r24_unorm_x8_uint`, **4096×4096** |
| Comparison sampler | `s15` — the same one the sun uses |
| Filter | 4-tap projective PCF |

Identified from a live devkit snapshot with a shadowed spot light on screen. The
name comes from the DXBC `#line` directives, which embed the original build path:

```
d:\work\master\git_clone\_intermediate\win64\GrModelShaders_dx11_win64\Release\SSLighting2_Spot_ps.hlsl
```

The four shadow fetches:

```
184: sample_c_indexable(texture2d) r4.w, r8.xyxx, t3.xxxx, s15, r8.z  // TFetch2DProjCmp
191: sample_c_indexable(texture2d) r0.y, r8.xyxx, t3.xxxx, s15, r8.z  // TFetch2DProjCmp
198: sample_c_indexable(texture2d) r0.x, r8.xyxx, t3.xxxx, s15, r8.z  // TFetch2DProjCmp
204: sample_c_indexable(texture2d) r0.y, r0.yzyy, t3.xxxx, s15, r0.w  // TFetch2DProjCmp
```

That is the same shape as the sun's vanilla filter — four comparison taps in a
small pattern — which is what the PCSS block replaces on the sun. `TFetch2DProjCmp`
means the fetch is *projective*: the texture coordinate is a `float4` whose `xy`
are divided by `z` before lookup, so UV and depth arrive together rather than as a
UV plus a separate z.

## Why this is the tractable case

1. **One draw per light.** The snapshot shows ~25 draws of this shader per frame,
   each with its own shadow map bound — not a clustered pass covering many lights
   with an index buffer. This was the risk that could have made the port
   intractable; it is not present.
2. **No cascades.** No `tileXY`, no atlas quadrants, no `uvMin`/`uvMax`. The entire
   tile-clamp apparatus in the sun shader has no counterpart here.
3. **Cost scales with the light's screen footprint.** Each draw rasterises the
   light's volume, so only affected pixels are shaded. With ~25 lights this is far
   better than 25 full-screen passes, and the lit-pixel early-out applies on top.

## What transfers unchanged

- The relative penumbra ratio `(z − blocker) / blocker` — projection-independent.
- The own-texel blocker read. The spot shadow map also stores the nearest caster
  along the light ray, so for a shadowed receiver its own texel holds the blocker.
  No search, for the same reasons as the sun: a metric search radius is hundreds of
  texels, and a texel-space one makes the effect depend on resolution.
- The penumbra mask, evaluated in place from `ddx`/`ddy` neighbours.
- The Vogel disk filter with `GatherCmp`, IGN rotation, and the early-out on
  `penumbra01 == 0`.

## What must be re-derived

| Part | Why |
|---|---|
| `baseUV` / `refZ` | The fetch is projective, so they come out of a `float4` divide rather than a precomputed UV and z |
| `texelWorld` | No cascade ladder to read. Must come from the light's own projection and its 4096 map |
| Softness scale | A spot light has no sun-like angular size, so the penumbra magnitude is a different physical scale and wants its own setting |
| Tile clamp | Delete — nothing to clamp to |

## Port plan

1. Dump and decompile `0x08292A5E` to get the register-level source, the way
   `0x5AFD2E17` was dumped.
2. Find the light's constant buffer and identify the projection matrix and the
   shadow map resolution in it — these give `texelWorld`.
3. Derive the penumbra radius the same way: metres, converted to texels per light.
4. Insert the PCSS block, mirroring the sun's structure but without the cascade
   and tile handling.
5. Compile, load through the devkit live path, and iterate. Note that the devkit
   path does not supply `cb13`, so any setting-gated value must be pinned while
   iterating — the same constraint as the sun.
6. Once the look is right, wire settings and build.

## Open questions

- **Cost.** Unknown until measured. The early-out should help, but ~25 lights each
  covering part of the screen is a different profile from one full-screen sun pass.
  Worth measuring before investing in the full port.
- **Which variants are live.** The engine enumerates 24 local-light variants across
  two families (`SSLighting2_*`, `Lighting_*_NA_*`). Only `SSLighting2_Spot` was
  seen here. Point lights and the `Lighting_*` family may need separate work; point
  lights in particular are likely cube maps, which is a different texel-mapping
  problem.
- **Multiple shadow casters per draw.** Assumed one, given one shadow map bound per
  draw. Should be confirmed by checking whether `t3` ever changes between draws of
  this shader within a frame.

## Decompiled source: `tmp/mgsv/original/0x08292A5E.hlsl`

Dumped with `devkit_dump_shader`, decompiled with `bin/cmd_Decompiler.exe -D`.
10,410 bytes against the sun's 15,151 — it is a smaller shader, and most of the
difference is the cascade handling the sun has and this does not.

### Constant buffers

| Register | Name | Notes |
|---|---|---|
| `b2` | `cPSScene` | Identical layout to the sun's, including `m_shadowProjection` and `m_shadowMapResolutions` |
| `b3` | `cPSLight` | `float4 m_lightParams[11]` — the light's own parameters |
| `b4` | `cPSMaterial` | `float4 m_materials[8]` |

`m_shadowProjection` in `cPSScene` is **not** sun-specific: it holds the *current*
shadow-casting light's projection. For the sun that is an orthographic cascade
transform; for a spot light it is a perspective one, which is why this shader
divides by `w` where the sun does not. Same field, repurposed per draw.

### The shadow coordinate

```
191: r9.x = dot(r5.xyzw, m_shadowProjection._m00_m10_m20_m30);
192: r9.y = dot(r5.xyzw, m_shadowProjection._m01_m11_m21_m31);
193: r9.z = dot(r5.xyzw, m_shadowProjection._m02_m12_m22_m32);
194: r9.w = dot(r5.xyzw, m_shadowProjection._m03_m13_m23_m33);
```

`r5` is a view-space position, so `r9` is light clip space and `r9.w` is the
view-space depth along the light's axis, in world units.

### The four taps

```
235: r5.xyzw = r9.wwww / g_psScene.m_shadowMapResolutions.yyyy   // one texel, in clip units
236: r0.xyzw = r5.xyzw * r0.xyzw                                  // r0 is ±0.125 per axis
238: r5.xy  = r9.xy + r0.xy
241: r8.xyz = r5.xyz / r5.www                                     // projective divide
242: r4.w = g_ShadowTexture.SampleCmp(s15, r8.xy, r8.z).x
```

Four taps at ±0.125 texels, dithered from a hash built at lines 227–234 — the same
quarter-texel pattern as the sun's vanilla filter. The tail is identical too:
`0.25 * sum`, then `1 - avg`, then squared (lines 261–265).

Note `m_shadowMapResolutions.y`, where the sun reads `.x`. The two components hold
the two light types' shadow resolutions.

### What this gives the port

| Quantity | Derivation |
|---|---|
| `baseUV` | `r9.xy / r9.w` |
| `refZ` | `r9.z / r9.w` |
| `texel` (UV per texel) | `1 / m_shadowMapResolutions.y` |
| Tile clamp | delete — no cascades, no atlas |
| `texelWorld` | `r9.w / \|m_shadowProjection._m00\| / m_shadowMapResolutions.y` |

The `texelWorld` form needs a caveat. `r9.w / resolution` is one texel in **clip-x
units**, which is the texel size only divided by the projection's own x scale — for
a perspective matrix `_m00` is `1 / (tan(fov/2) * aspect)`. Dividing by `|_m00|`
recovers world units, which is what a metre-valued softness setting needs.

If that scale turns out to be wrong, the fallback is to fold it into the softness
constant: the penumbra stays consistent and correct in shape, but the slider's
units would be off by a constant factor (roughly 2–3 for a typical spot FOV) and
would need documenting rather than being true millimetres.

### Register-level source

```
tmp/mgsv/original/0x08292A5E.cso    352,657 bytes (devkit dump)
tmp/mgsv/original/0x08292A5E.hlsl    10,410 bytes (decompiled)
```

## Reusable method

The identification technique generalises and is worth reusing:

1. `devkit_queue_snapshot`, then `devkit_snapshot_summary`.
2. `devkit_list_draws` filtered to pixel-stage draws with render targets.
3. `devkit_get_draw` on candidates, looking for a depth-format SRV that is neither
   the G-buffer depth (3840×2160) nor the sun map (8192×8192). The spot map's
   4096×4096 size came straight from the mod's `SpotLightLightShadowResolution`.
4. `devkit_get_shader` with `view: "disassembly"` and read the `#line` directives —
   they name the source file outright. No guessing.
