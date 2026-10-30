# MGSV sun shadow — contact hardening (PCSS): implementation notes

Status: **implemented and live in-game; one open issue (see §7).**
Scope: **sun (directional light) shadows only.** Spot/point shadows untouched.
Game: MGSV: TPP retail `1.0.15.4`, DX11, ReShade 6.7.3 addon (`renodx-mgsv`, branch `mgsv-cs`).

| Path | What |
|---|---|
| `src/games/mgsv/shadows/SunLight_DL_CS_ES_0x5AFD2E17.ps_5_0.hlsl` | replacement shader |
| `tmp/mgsv/original/0x5AFD2E17.cso` | dumped retail bytecode (gitignored) |
| `tmp/mgsv/original/0x5AFD2E17.hlsl` | 3Dmigoto decompilation of the retail shader |
| `tmp/mgsv/original/0x5AFD2E17.asm` | annotated disassembly of the retail shader |
| `src/games/mgsv/docs/MGSV_PixelShader_NameMap.md` | all 88 tracked pixel shaders mapped to source filenames |
| `src/games/mgsv/shadows/MGSV_SunShadow_ContactHardening.md` | this file |

---

## 1. Target

One shader applies the sun shadow for the whole game:

| | |
|---|---|
| CRC32 | `0x5AFD2E17` |
| Source file | `SunLight_DL_CS_ES_ps.hlsl` |
| Stage | `ps_5_0` |
| Draws per frame | 1 (fullscreen deferred light pass) |
| Outputs | `SV_Target0` (diffuse, shadow applied), `SV_Target1` |

Verified by scanning all 88 tracked pixel shaders for a shadow-map binding: this is the only one.
`0x712F7222` (`SunLight_DL_ES_ps`) is the no-shadow variant.

## 2. Shadow map (confirmed against the RenderDoc capture)

| Property | Value |
|---|---|
| Binding | `g_ShadowTexture` → `t3` |
| Size | 8192 × 8192 |
| Texture format | `DXGI_FORMAT_R24G8_TYPELESS` |
| Depth-stencil view | `DXGI_FORMAT_D24_UNORM_S8_UINT` |
| Shader resource view | `DXGI_FORMAT_R24_UNORM_X8_TYPELESS` |
| Layout | 2×2 atlas, 4 cascades × 4096 × 4096 |
| Vanilla sampler | `g_samplerComparisonLess_Linear_Clmap` → `s15` (`SamplerComparisonState`) |
| Other samplers | `s8` Point_Wrap, `s9` Point_Clamp, `s10` Linear_Wrap, `s11` Linear_Clamp |

A second `8192 × 4096` R24G8 typeless texture exists — presumably the spot/point atlas.

Because the SRV is the **typeless** view rather than the depth view, the raw depth is readable with a
plain sampler. That is what makes the blocker search possible at all.

## 3. Vanilla filter

```
avg = mean of 4 × SampleCmp(t3, s15) at ±0.5 ±0.125 texel, sub-texel jitter from t12
lit = (1 - avg)^2
```

The disassembly names it `ShadowComparisonFourSampleGaussianFilterWithMicroDither`. Fixed 0.5-texel
radius, independent of blocker distance — no contact hardening.

Constants used by the shadow block (`cb2` = `cPSScene`):

| Offset | Name |
|---|---|
| 192 | `m_shadowProjection` |
| 256 | `m_shadowProjection2` |
| 336 | `m_projectionParam` |
| 464 | `m_shadowMapResolutions` (only `.x`, the atlas width) |

---

## 4. Findings that shaped the implementation

All of these were established by measurement, and each one changes the code.

### 4.1 The comparison sampler returns 1 = occluded

`SampleCmp` returns **1 when the texel is occluded**, 0 when lit. Consistent with vanilla's
`lit = (1 - avg)^2` where `avg` is the occluded fraction.

An early blocker search used `1 - SampleCmp(...)` as the occlusion weight. That inverted the blocker
set — blockers were found in lit areas and not in shadows — producing a **lighter halo around every
shadow**. Fixed by using the comparison result directly.

### 4.2 Blockers have a smaller depth than the receiver

Probed directly: at the receiver's own texel the shadow-map depth is ≤ the receiver depth. So the
blocker test is `d < refZ - bias`.

### 4.3 The light's ortho depth range is very wide

Receiver-to-blocker distances land in **0.001 … 0.02** in normalised shadow depth. A penumbra scale
of 20–300 therefore produces less than one texel of blur everywhere, which clamps to the floor and
looks like a flat, uniform edge rather than hardening. The working range is far higher.

### 4.4 The receiver's own surface pollutes the blocker set

The engine renders casters with a depth bias, so the receiver's own texels register as occluded.
Averaging over the search disk then pulls the blocker depth back up to the receiver depth and
flattens the penumbra again. A bias is required to exclude them.

### 4.5 Reading the depth at the receiver's own texel measures the caster bias

The shadow map stores the depth of the first surface the light ray through a texel
hits, so for a shadowed receiver that surface ought to be the caster, and the depth
at the receiver's own texel ought to be the blocker depth with no search needed.

Measured against a banded probe, it is not. `refZ - depth_at_own_texel` comes back
**slope-shaped**: large on slopes, near zero on flat ground. That is the signature of
a **slope-scaled rasterizer depth bias** applied when the engine renders casters, not
a receiver-to-caster distance. The value being measured is the engine's own bias.

This was originally dismissed for the wrong reason — the difference reads ~0.001 and
looked like no signal at a probe threshold of 0.002. With the true depth range
(622.9 m) that 0.001 is 0.62 m, so the threshold was simply too coarse. The real
reason it cannot be used is the bias, not the magnitude.

### 4.6 A blocker search at this shadow resolution is undersampled

The search radius needed to reach a caster is ~1 m. One texel is 3.2 mm in cascade 0,
so that radius is **~311 texels**, sampled with 16 taps — the taps land ~78 texels
apart and sample unrelated geometry. The blocker depth comes back as noise that is
usually far closer to the light than the receiver, so the penumbra saturates at its
ceiling and never varies along a shadow.

More shadow resolution makes this **worse**, not better: the radius in texels grows
with resolution while the tap count stays fixed.

Together with §4.5 this means **neither route yields a receiver-to-caster distance**,
which is the only quantity PCSS needs. Contact hardening is not implementable on this
engine with a depth-only shadow map and a reasonable tap budget.

### 4.7 The sun light's RTVs are accumulation targets

`SV_Target0` / `SV_Target1` are shared light-accumulation buffers; ambient spherical harmonics and
local lights add into the same resources later in the frame. Consequences for debugging:

- `devkit_analyze_resource` on those views cannot isolate this shader's output. Probes come back
  above 1, or unchanged between reads.
- The engine also rotates the RTV handles between frames, so a handle captured from a snapshot is
  usually stale by the time it is read.
- **Verify on the swapchain backbuffer** (`r8g8b8a8_unorm_srgb`) instead — it is stable and is what
  the player actually sees.
- Writing a probe through this shader removes the sun's diffuse contribution and darkens the whole
  frame. Expected during probing, but it looks alarming in-game.

### 4.7 Cascade-dependence: every texel-space or normalised-depth constant breaks

The softness of the shadows changed radically as the camera moved. Isolated by setting
`PCSS_STRENGTH 0` (vanilla filter through the replacement shader): the jump **disappeared**, proving
it came from PCSS and not from the engine's cascade system.

The cause class is that anything expressed in texels or in normalised depth has a *world* size that
changes with the cascade, and which cascade a pixel lands in depends on camera distance. Quantities
converted to world units so far:

| Quantity | Was | Now |
|---|---|---|
| Penumbra | texels | metres (fraction of receiver-to-blocker distance) |
| Penumbra floor | 1 texel | 1 cm |
| Penumbra ceiling | texels | 50 cm |
| Blocker search radius | 16 texels | 0.5 m |
| Depth bias | 0.002 normalised | 0.75 m |
| Texel size | cascade 0's projection | screen-space derivative of the atlas UV |

The texel-size change matters most: `m_shadowProjection` is **cascade 0's** projection, so using its
diagonal for every pixel makes the penumbra wrong by the cascade ratio. Deriving it per pixel from
`length(ddx(viewPos)) / length(ddx(atlasUV))` uses the final atlas UV, which already contains the
per-cascade scale and offset, so it is correct for whichever cascade the pixel is in.

**This did not resolve the issue.** See §7.

---

### 4.8 The light projection is a rotated uniform scale, not an axis-aligned box

Measured from the capture (RenderDoc, sun light draw, `cPSScene` at offset 192):

```
m_shadowProjection          (rows as reported)
  -0.12627   0.05348   0.06561   0.00000
   0.04163  -0.06335   0.12742   0.00000
   0.07370   0.12742   0.03798   0.00000
  -0.26470  -0.53163  -0.15322   1.00000

m_shadowProjection2         (rows as reported)
  -0.29537  -0.59321  -0.17096   0.41376
  -0.39704  -0.79742  -0.22981   0.20271
  -0.44216  -0.88802  -0.25593   0.10708
   0.01056   0.06900   0.12609   1.00000

m_shadowMapResolutions      8192, 4096, 8192, 4096
```

**All three row norms of `m_shadowProjection` are 0.15202** (and so are the column
norms). The matrix is therefore a *uniform scale times a rotation*, and the
orthographic box is a cube:

| Derived | Value |
|---|---|
| `clipScale` = `length(_m00_m10_m20)` | 0.15202 |
| ortho extent = `2 / clipScale` | 13.16 m |
| `worldPerClip` = `1 / clipScale` | 6.578 m |
| `zScale` = `|m_shadowProjection2._m30|` | 0.01056 |
| `depthRange` = `worldPerClip / zScale` | **622.9 m** per unit of `refZ` |

The diagonal elements are **not** scale factors. Reading `_m00` gives 15.84 m and
`_m22` gives 52.66 m — both wrong. Worse, `m_shadowProjection` maps *view* space
to shadow clip, so its diagonal changes as the camera turns while the true scale
does not. Every world-space conversion in the shader was built on those two
numbers, which is why correcting the search radius, the depth bias and the tile
window individually changed nothing: they all inherited the same camera-dependent
scale. Use the row/column norm.

`m_shadowProjection2`'s rows 0..2 hold `(offset.xyz, scale)` per cascade, and the
`.w` values are the cascade ladder:

| Cascade | scale | ortho extent | texel size |
|---|---|---|---|
| 0 | 1.0 | 13.16 m | 3.21 mm |
| 1 | 0.41376 | 31.81 m | 7.76 mm |
| 2 | 0.20271 | 64.93 m | 15.85 mm |
| 3 | 0.10708 | 122.9 m | 30.02 mm |

(texel = extent / 4096, the cascade tile size in the 8192 atlas.)

Note that one texel spans **3.2 mm in cascade 0 and 30 mm in cascade 3** — a 9.4x
spread. Any constant expressed in texels, including a "safety" cap on the
penumbra, therefore changes the world-space blur by that factor as pixels move
between cascades, and which cascade a pixel lands in depends on camera distance.

---

## 5. Implementation

`src/games/mgsv/shadows/SunLight_DL_CS_ES_0x5AFD2E17.ps_5_0.hlsl`:

- The vanilla register-level decompilation is kept **verbatim** so the baseline is provably
  equivalent. Only the filter block is replaced.
- Untouched: cascade selection, cascade blending, the per-pixel dither bias, cloud shadow (t4),
  distance fades, the ambient floor, and the `(1 - avg)^2` term.
- The Poisson disk is rotated per pixel from the game's own 32×32 noise texture (t12).
- All PCSS taps are clamped to the cascade tile (`[tileOffset, tileOffset + 0.5]`), reconstructed
  from the same blended cascade index vanilla uses, so wide taps cannot bleed across cascades.

```
1. blocker search      : 16 taps, point-sampled raw depth, d < refZ - bias
2. penumbra            : (receiver - blocker) in metres × fraction, floored/capped in metres
3. variable-radius PCF : 16 taps, SampleCmp with the game's comparison sampler
4. lit                 : (1 - avg)^2, unchanged
5. gate                : lerp(vanilla, pcss, PCSS_STRENGTH × hasBlocker)
```

The `hasBlocker` gate matters: where the blocker search finds nothing the pixel is lit, and running a
wide PCF there pulls neighbouring shadow texels in and paints a halo.

Tuning constants (compile-time `#define`s; to move into the cb13 `ShaderInjectData` buffer and
ReShade settings):

| Define | Value | Meaning |
|---|---|---|
| `PCSS_STRENGTH` | 1.0 | 0 = vanilla 4-tap, 1 = full PCSS |
| `PCSS_PENUMBRA_FRACTION` | 0.06 | penumbra = 6% of receiver-to-blocker distance |
| `PCSS_MIN_PENUMBRA_WORLD` | 0.01 m | floor — keeps contacts crisp |
| `PCSS_MAX_PENUMBRA_WORLD` | 0.50 m | ceiling |
| `PCSS_MAX_PENUMBRA_TEXELS` | 32 | perf rail |
| `PCSS_BLOCKER_RADIUS_WORLD` | 0.5 m | blocker search radius |
| `PCSS_BIAS_WORLD` | 0.75 m | excludes the receiver's own surface |
| `PCSS_BLOCKER_TAPS` / `PCSS_PCF_TAPS` | 16 / 16 | tap counts |

---

## 6. Verification method

Run the game with `renodx-devkit.addon64`, load the shader live from `src/games/mgsv/shadows`, and
dump the **swapchain backbuffer** as a PNG with `devkit_analyze_resource`.

What worked:

- Baseline (vanilla decompilation, PCSS off) renders identically to retail.
- PCSS visibly softens shadows, contact points stay tighter than distant edges, terrain micro-shadows
  and grass stay crisp, no halos, no cascade seams.
- 59 FPS at 3840×2160 (the game's own cap) with 16 + 16 taps.

What did **not** work reliably: numeric readback of the sun light's own render targets (see §4.6).

For engine ground truth, `renderdoccmd convert -f capture.rdc -o capture.xml -c xml` produces a
~21 MB XML from a ~1 GB capture, which is a practical way to confirm resource formats without
launching RenderDoc.

---

## 7. Open issue

**Shadow softness still changes with the camera.**

Ruled out by measurement:

- not the engine's cascade system (proved by running with `PCSS_STRENGTH 0`)
- not the blocker search radius (now world-space)
- not the depth bias (now world-space)
- not the tile window (now the UV quadrant containing `baseUV`)
- not the projection diagonal (now rotation-invariant row norms)

Current best hypothesis, not yet confirmed:

**The penumbra is saturating at a texel-space cap.** With the true
`depthRange` (622.9 m) the world-space penumbra is easily large enough to hit
`PCSS_MAX_PENUMBRA_WORLD` (0.5 m), and the cap that then applies in texels is
cascade-dependent: 0.5 m is 156 texels in cascade 0 and 17 texels in cascade 3.
Changing `depthRange` from 52.66 to 622.9 produced no visible change, which is
what you would expect if the penumbra saturates before either value matters.
`PCSS_MAX_PENUMBRA_TEXELS` has since been raised to 256 so it cannot bind.

Confirmed next step: capture the same spot twice, once at the camera angle where
shadows go extremely soft and once at a normal angle, and diff the two
`m_shadowProjection` / `m_shadowProjection2` sets. That distinguishes "the
matrices move with the camera" from "the matrices are stable and the blocker
search is finding different blockers".

## 8. Open items beyond the above

1. Tuning constants are compile-time; move them into `ShaderInjectData` (cb13) and ReShade settings.
2. Spot/point shadows are not covered. `Lighting_Spot_NA*` / `SSLighting2_Spot*` (24 named variants)
   were never tracked because no shadowed local light was visible in the captured scenes.
3. `m_shadowProjection2._m33` was 0 in every scene captured, so the vanilla dither-expanded edge
   threshold path was never exercised.
4. All measurements were at one shadow quality setting (8192² atlas). Lower settings shrink the
   atlas and change the texel scale.
5. `capture1.rdc` (~1 GB) sits in the repo root; it should not be committed.
