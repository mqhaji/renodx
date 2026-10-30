# MGSV subsurface scatter and ambient occlusion - findings

Status: the two compute passes at `0x6DAF0C4A` / `0xB2601810` are the game's screen-space **subsurface-scattering** chain, not ambient occlusion. The first is registered with its compiled port; the second has a callback-only entry. The addon calculates AO separately after them, then darkens the light buffer.

Source: Therrien, Levesque and Gilet, *Screen Space Indirect Lighting with Visibility Bitmask*, The Visual Computer 2023, Algorithm 1, equations 1-4.

Code:

| File | Role |
|---|---|
| `src/games/mgsv/ao/mgsv_ao.cs_5_0.hlsl` | calculates AO in a separate texture |
| `src/games/mgsv/ao/mgsv_ao_resolve.cs_5_0.hlsl` | upsamples AO and applies it to the light buffer |
| `src/games/mgsv/ao/0x6DAF0C4A.cs_5_0.hlsl` | horizontal scatter port, included in generated replacements |
| `src/games/mgsv/ao/0xB2601810.cs_5_0.hlsl` | vertical scatter port; explicit callback-only registration takes precedence in the tested build |
| `src/games/mgsv/ao/ssss.hlsli` | the shared core of those two ports |
| `src/games/mgsv/game/shaders.hpp` | material capture and the lighting hook before explicit and generated shader registrations, in the same game namespace |

Both ports are compiled into the generated shader set. The effective first-stage entry supplies replacement code, while the explicit second-stage entry is callback-only. Older notes describing both ports as unregistered reference files were inaccurate; the structural cleanup does not change the registrations to match those notes.

The former `game/integration.hpp` contents are consolidated into `game/shaders.hpp`, not a separate integration layer or forwarding header. The lighting callback still passes GI the borrowed raw velocity handle from reconstruction capture, records GI before AO, and restores the surrounding state afterward. This move changes no AO math, resource formats, settings, ownership or precision-activation gates; the measurements below are historical, not new validation of the consolidation.

## The scatter chain

| Pass | Role | Dispatch |
|---|---|---|
| `0x6DAF0C4A` | horizontal sweep | groups on the column axis, 128x1x1 threads |
| `0xB2601810` | the same sweep vertically, then the translucency blend | groups on the row axis |

Both are separable halves of one depth-aware light diffusion. Per pixel:

- 13 taps (`i = -6..6`) at seven bands of a screen-space step: `0, 1/6, ... 1`.
- Each tap weighted by `exp2(-dz^2 * (313629, 515248, 655771))`, so red survives a larger depth difference than blue.
- Each band tinted from white toward warm orange as the distance grows.
- The output is a per-channel weighted **average of the light**, never a visibility term.

Stage 2 then blends that average back over the light:

```
translucency = saturate(cTweakParams.y * (1 - (2 + 1/N.V) * (0.15, 0.18, 0.20)))
mixed        = translucency * diffused + (1 - translucency) * light
output       = cFade * mixed + (1 - cFade) * light
```

so the diffusion is strongest on surfaces facing the camera and vanishes at grazing angles. Alpha is stored as 1.0; the light's own alpha is the "pixel was shaded" flag the chain gates on.

This is the SSSSS structure - separable, per-channel, depth-aware, red-shifted wide kernel, `N.V`-weighted blend - not GTAO and not VBAO.

### Dispatch axis

Stage 2 takes the row on `vThreadID.x` (its bounds guard is `cBufferHeight < tid.x`) and the column on `vThreadID.y`, so the engine dispatches `(ceil(height/128), width)`. The addon previously dispatched `(height, ceil(width/128))`: transposed, and against the engine's `cNumTilesX` most groups landed on the partial-tile path.

## What the mod does instead

The addon hooks `0xB2601810` without replacing it:

1. The game's scatter dispatch is recorded unchanged.
2. `on_drawn` reads the depth, normal, and light views still bound to that context and keeps them alive until the callback returns. No global before/after capture is needed.
3. If indirect lighting is enabled and its inputs are available, its passes are recorded first.
4. If AO is enabled, one pass calculates occlusion in a separate texture. A second pass upsamples it and darkens the light buffer's RGB, preserving alpha. Both use 8x8 thread groups.

The callback records commands after the scatter dispatch; it does not wait for the GPU to finish it. This keeps the game's scattering passes intact. AO still darkens their result, including direct lighting and any added indirect light.

## Why the earlier replacement changed skin shading

The earlier versions had two problems:

1. **The dispatch was taken over.** The old entry returned `false` from `on_draw`, so the engine never ran stage 2: no vertical diffusion, no translucency blend. Stage 1 still ran but its only consumer was stage 2, so it was wasted work.
2. **AO darkened all lighting.** `light * (1 - ao)` affected every consumer of the light, including skin. This remains a limitation of applying AO to the combined light buffer; it is separate from the missing scattering caused by replacing stage 2.

The later "fix" wrote the light through unchanged and split the occlusion into a `u1` buffer, but nothing ever read that buffer, and the dispatch was still taken over - so the scatter stayed broken and the AO was invisible.

The alpha claim in the old notes was a misdiagnosis: retail stores alpha 1.0 at the same point, so alpha was never the differentiator.

## Paper mapping

| Paper | Verdict |
|---|---|
| Sector space centred on the projected normal, `[-PI/2, PI/2]` | follows, at 128 sectors |
| Equal-**measure** sectors: angles converted out of cosine space (section 3.1, equation 3) | follows, via the cosine CDF |
| Band between the sample and a back sample one thickness away | follows, **with the sign flipped** - deviation 1 |
| `AO = 1 - popcount(mask)/Nb` (algorithm line 26) | differs: the bands' exact widths less their measured overlap - deviation 2 |
| Slices averaged with equal weight (equation 2) | **deviation 5** - weighted by each slice's own measure |
| The un-occluded fraction multiplies the light (sections 3.2, 3.3) | follows; the buffer is the whole light, not the ambient term |
| Thickness is a small constant fraction of the radius | follows; the slider exists because the quiet end is real |
| Exponential sample distribution (section 4.3, table 2) | follows; algorithm line 6 says constant |
| Round criterion, half-covered sector | differs: both edges round to nearest for the mask, and the value carries the exact widths - deviation 2 |
| "Determine directions with random offset" (algorithm line 7) | follows: per-pixel interleaved gradient noise, plus a per-frame phase |
| Distributed spatially **and temporally** (section 3.1) | follows; temporal term gated on any reconstruction mode, DLSS included |
| Indirect diffuse (algorithm lines 20-24) | not part of AO; handled by the separate indirect-lighting chain |
| Directionally occluded ambient light (section 3.2) | not implemented |
| Optional thickness that grows linearly with distance (section 3.1) | not implemented; replaced by a distance fade |

## Deviations, and why

Each is deliberate and each is recorded in the shader header. Do not "fix" one without reading its note.

**1. Back sample sign (line 15).** Algorithm 1 subtracts `normalize(p)`; the shader adds it, so the band is one thickness *deeper* along the view ray. The paper never states its view-space handedness, so `normalize(p)` points toward or away from the camera depending on the convention and the line cannot be read literally. Deciding evidence: the paper states that GTAO without falloff is equivalent to an infinite thickness. Only the deeper form converges there, sweeping the band down to the tangent plane the way a horizon does. Mirko Salm's GT-VBAO corrections ship the deeper form.

This is also the line that decides whether coplanar geometry self-occludes, and it has caused two failures already. On the deeper side a coplanar sample's whole band falls past the tangent plane and the CDF clamp collapses it to zero width - verified numerically at five distances across all eight slice directions. On the camera side the band lands inside the hemisphere and a flat floor darkens itself uniformly.

**2. Sector edges (line 18).** Algorithm 1 uses `floor` for the low edge and `ceil` for the high. The shader rounds both to nearest *to build the bit field*, but the value is not the popcount: the slice's occlusion is the sum of the bands' exact widths, less the overlap the mask detected (the sum of the claims it made, minus its popcount). So a union in which nothing overlaps comes out exact - and so does every monotone surface - and only overlapping bands lose anything, by at most what the mask could see.

Measured against exact interval unions:

| scenario | exact | this | previous attempts |
|---|---|---|---|
| 8 bands tiling a wall, no overlap | 0.400 | **0.400** | dither 0.398, first remainder 0.406 |
| 8 bands, 30% overlap | 0.295 | **0.275** | first remainder 0.406 |
| 8 thin bands (0.3 sectors) tiling | 0.075 | **0.075** | first remainder 0.119 |
| one wide band, 6 sectors | 0.188 | **0.188** | - |
| realistic wall, widths 1.5 to 0.3 | 0.197 | **0.197** | first remainder 0.228 |

Two earlier attempts failed and are recorded here so they are not retried: rounding to nearest alone, and a shared stochastic dither. The dither was unbiased in expectation but deterministic per pixel, so on a flat surface every pixel rounded the same way and the error was a fixed pattern - grain - that no sample count could average away, because every band in a slice shares the phase.

**3. Step size (line 6).** Algorithm 1 spaces samples at a constant `r/(Ns+1)`; the shader uses the exponential spacing of section 4.3 and table 2, which the paper recommends for detail around small objects.

**4. Distribution (line 7 and section 3.1).** The paper asks for the integral to be distributed spatially *and* temporally. Both are here: per-pixel interleaved gradient noise spatially, plus `CUSTOM_RANDOM` rotating the slice angles and the step jitter every frame.

The gate is `CUSTOM_TAA != 0`, which is every reconstruction mode except Off, DLSS included. With reconstruction off the term is deliberately inert: a pattern that rotates with nothing accumulating it is shimmer rather than grain.

The AO effect has no dedicated temporal accumulator or spatial denoiser. Temporal smoothing relies on reconstruction when enabled. The AO resolve upsamples and applies the result; it is not a separate denoising stage.

**5. Slice weights (equation 2).** Equation 2 averages the slices with equal weight. The shader weights each by its own cosine-weighted measure, `|projN| (cosN + n sinN)`, and normalizes by the sum. Equal weights are only correct if every slice integrates the same amount of the hemisphere, and equation 3 says they do not; the equal-measure sectors are meaningless without the matching weights. Normalizing by the measured slices also removes a bias the equal average had: a slice dropped as degenerate used to be counted as fully occluded.

## Verification

- **The cosine CDF is a verbatim port of GT-VBAO's `SliceRelCDF_Cos`, checked against an independent definition** - the reference project's own `referenceHorizonRemap`, a 4000-step numerical integral of the density `|sin(psi + n)| cos(psi)`. Over 1314 `(n, h, side)` cases spanning the whole domain including the tangent-plane edges, the pole and both sides: **maximum absolute error below 1e-5**.
- In the normal-facing limit the CDF reduces to `x = (1 -/+ M)/2` with `M = sin^2 theta`, i.e. exactly linear in the measure - the property the construction exists for.
- **Coplanar samples produce zero-width bands**, so a flat surface cannot occlude itself: checked at five distances (0.05 to 1.0 of the radius) across all eight slice directions.
- Compiles to `cs_5_0` under the repo's shader tools.
- **The quantiser is exact where bands do not overlap**; see the table under deviation 2.

For the scatter chain:

- Retail bytecode re-dumped from the running game through DevKit is byte-identical to the baseline (SHA-256 match), so the decompile was never stale.
- The 1:1 port compiles to `cs_5_0` and matches the retail instruction stream: same operations, constants and tgsm layout; 149 instruction slots against retail's 159.
- Live-loaded in gameplay, it is visibly indistinguishable from retail.
- Dispatch proven with three probes: a flat magenta write changed the screen, a marker-plus-identity write showed the port's own file was executing, and a difference map showed the diffusion moving the light on the ground while most pixels are dropped by the retail alpha gate.

Not verified: the reconstructed world scale, the range of `LinearZ`, and the AO's appearance under DLSS specifically.

## Settings

| Setting | Meaning |
|---|---|
| Visibility-Bitmask Ambient Occlusion | Enables the added AO passes. Other effects are controlled separately. |
| AO Strength | Scales the calculated occlusion. Values above 100 increase darkening. |
| AO Sampling Radius | Controls how far around each pixel the effect searches for nearby surfaces. |
| AO Slice Count | Sets how many directions are checked per pixel. With reconstruction enabled, the pattern changes each frame. |
| AO Samples per Side | Sets the number of samples on each side of a slice. Samples are spaced exponentially. |
| AO Thickness Scale | Controls how thick sampled surfaces are assumed to be. The default gives 40% of the sampling radius. |
| AO Depth Fade Scale | Controls fading with depth. Larger values keep AO visible farther from the camera; the depth units remain unverified. |
| AO Resolution | Selects Full, Half, or Quarter resolution for calculating AO. Application to the light buffer stays full resolution. |
| AO Depth Mip Sampling | Uses lower-resolution depth for distant samples to help detect surfaces between sampling steps. |

In the recorded tests, the paper's 5% thickness produced a very subtle effect.

## Open questions

1. **The world scale.** `ViewPos` reconstructs through `cProjectionParam`, and the reconstruction only agrees with the march direction if `cProjectionParam.x/y` carries the aspect the way this assumes. This has never been verified against the retail pass.
2. **`LinearZ`'s range.** If it returns world units rather than a normalised depth, the `saturate(1 - depth * depth)` fade is zero past one metre and the pass is a passthrough.
3. **Temporal behaviour under DLSS.** The rotating pattern is gated on any reconstruction mode; what is unverified is how much ghosting or trailing the view-dependent signal produces under motion.
4. **The AO lands on the whole light**, direct included, so it scales sunlit surfaces too. The paper multiplies only the ambient and indirect terms, which this buffer does not separate.

## Removed, and not to be re-added

- **Replacing the scatter passes.** `0x6DAF0C4A` / `0xB2601810` are the SSS chain. Taking either one is what removed the skin's scattering, and the ports exist only as a reference.
- **MXAO's GTAO core** (MartysMods MXAO, Pascal Gilcher). Proprietary - its header forbids redistribution - so it cannot ship. It is why the port was rewritten from the paper.
- **The GT-VBAO port** (Mirko Salm's corrections, MIT) as a whole. Its cosine-CDF sector measure, which the current shader uses, was the part worth keeping.
- **A diagnostic blur passthrough** on the first blur of the AO chain, registered only so the devkit would track it and its bindings could be read.
- **A hand-rolled 8-tap cross average of the light** (offsets +/-4 and +/-10 px, no depth or normal weighting) used as a lerp target.
- **The lerp of the light toward `t0`** as the application. It moved the light by a few percent and read as screen-space indirect lighting rather than occlusion.
- **The `u1` occlusion buffer.** Nothing ever read it; the AO now scales the light directly, after the scatter.
