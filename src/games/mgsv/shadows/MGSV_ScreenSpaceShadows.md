# MGSV screen-space shadows — findings

Status: implemented in the sun shader, still being tuned. Branch `mgsv-sss`.
Ported to the spot shader on the same branch, behind its own toggle; not yet
verified in a capture. See *Spot lights* at the end.

The cascade map cannot afford centimetre detail over a hundred metres of range
(see `MGSV_ShadowResolution_And_VSM.md`), so this supplies the high-frequency
half: march the depth buffer toward the light and attenuate where the ray passes
behind a nearer surface.

## What it is

A ray-march in the sun's pixel shader, entirely in view space.

| Input | Where | Notes |
|---|---|---|
| Scene depth | `t2`, `r32_float_x8_uint` | projected, not linear |
| Depth linearisation | `m_projectionParam` | `viewZ = z / (depth - w)` |
| Unprojection x/y scale | `m_projection._m00` / `._m11` | **not** `m_projectionParam.xy` |
| Sun direction | `cPSSystem.m_dominantLightDir` (`b0`) | view space, points away from the light |
| Projection, to project steps | `m_projection` | |
| Surface normal | `t0`, packed | view space |

`cPSSystem` is bound to the sun draw but not declared by the vanilla shader, so
declaring it needs no binding work.

## Findings, each verified by reading the render target

**The target is a light accumulation with a one/one blend.** Anything written to
`o0` is *added*. A shadow cannot replace an additive light contribution, it has
to **scale** it: `r2.xyz *= (1 - ssOcc)`. Writing `1 - ssOcc` to `o0` added a
full unit of light everywhere unoccluded and washed the frame out — which looked
exactly like "the sun is missing" and sent several turns in the wrong direction.

**Probes are additive too**, so they tint rather than replace. To isolate the
term, write it *negative*: `r2.xyz = -ssOcc` subtracts where the march found a
blocker and adds nothing elsewhere.

**The light-accumulation target cannot be read back usefully.** The tonemap
output can — it is the full-resolution scene, after everything including this
term, and is the only place a probe is legible.

**The tonemap auto-exposes.** A region darkened by this term comes back
*brightened*. Read the shape, never the brightness. Several wrong conclusions
came from reading a compensated exposure as the artifact.

**`m_dominantLightDir` points away from the light.** Marching along it unnegated
walks away, `p.z` grows, and every step trivially satisfies the occlusion test:
every pixel occluded, the sun multiplied by zero, no shadows anywhere. The spot
shader negates its light vector (`-r6.xyz`), which is the convention. Being view
space, it also rotates with the camera — which is what made the effect appear and
vanish as the camera turned, and is the real explanation of that symptom.

**`m_dominantLightDir` is not the sun, and a spot light can take it over.** It is
produced by the engine's `DominantLightSearchJob`, which picks whichever light wins
a scene-wide search and interpolates the result over time; scripts can also set it
outright through `Utility::LuaInterface::SetDominantLightDirection`. A spot light
coming into view could therefore steer this term — which belongs to the sun pass —
at a different light, and because the job interpolates the change, the direction
drifted rather than snapped. That reads as the shadows being slightly off, not as a
broken shadow, which is why it survived tuning. The march now uses
`g_psLight.m_lightParams[0]`, the sun's own direction: the vanilla lighting already
uses it as `N·L` (r2 is the decoded surface normal — the reflection at 884-888 only
reduces to `2(N·V)N - V` when it is), so it points *toward* the light and is used
without the negation that `m_dominantLightDir` needs.

**The unprojection x/y needs the projection's own scale.** `m_projectionParam.xy`
produced a scale error, visible as a smooth gradient across a round-trip probe
that reconstructs a view position and projects it back.

**March length must be a fixed *screen* distance.** A fixed world reach covers a
screen distance that grows as the receiver approaches the camera — 1 m at 2 m of
depth is a quarter of the screen width — so the ray sweeps a huge swath of the
frame and everything along it that is nearer than the ray registers as an
occluder. That is a shadow the size of the surrounding terrain around a character
standing near the camera. It stayed oversized even after the reach came down from
a depth-proportional 2.5 m to a fixed 1 m, because both are large on screen at
close range. Constant screen length *is* proportionality to depth; the projection
fixes the constant at 0.029 m per metre of depth, so the reach is `SS_MARCH_NDC`
(0.05, 2.5% of screen height) and measures 5.8 cm at 2 m, 11.5 cm at 4 m.

**The occlusion test cannot separate a caster from layered geometry on this
data.** Eight tests were tried, each measured by tinting the term's footprint red
(`+= float3(ssOcc * 60, 0, 0)`, temporary), loading, capturing the tonemap and
reading it back:

| Test | Character | Grass | Terrain |
|---|---|---|---|
| one-sided, `sz < p.z - t` | whole body flagged | flagged | clean |
| two-sided, between receiver and ray | whole body flagged | flagged | clean |
| occluder within `t` of the ray | whole body flagged | flagged | clean |
| as above, reach 54 px → 5 px | still whole body | flagged | clean |
| view-axis fade `length(lightDir.xy)` | unchanged | flagged | clean |
| receiver's own surface **plane** as the reference | mostly clean in one view, flagged in others | flagged | facet-edge network |
| plane + 25 cm maximum separation | arms and legs flagged | flagged | denser network |
| plane + cap, reach 54 px → 13 px | clean | **none** | clean |

The last row is the decisive one: at a 13 px reach the term flags nothing at all,
grass included. The wanted effect and every artifact live in the same reach band,
so no parameter separates them, and no shape of depth window does either — the
measurements above are the evidence.

The reason is the depth buffer itself. At pixel scale it contains many small steps
that are not casters: layered gear on the character (plates over the under-suit),
overlapping terrain patch meshes (the network follows their boundaries, not the
tessellation — adjacent quads are continuous), and silhouettes. A test of the form
"a surface in front of the reference counts" accepts all of them, and the plane
formulation additionally dives into the solid past any crease, which is why it
decorated the terrain's patch edges.

The reference used is the ray's depth (one-sided). It is not correct — it accepts
the receiver's own surface on a solid object — but with the material gate below it
only ever runs where that does not matter.

**A maximum separation does not help.** The layered-gear and patch-mesh steps are
centimetres, well under any cap large enough to keep grass.

**Resolved by gating on the material, not on the geometry.** The depth buffer
cannot supply the discriminator, but the G-buffer's `specular.z` — the x
coordinate of the material lookup, i.e. the material's index — can. Measured by
colouring three thresholds into R, G and B in one capture: the character's body
sits in 0.25–0.5 and his hair in 0.5–0.75, while grass, terrain and rocks all sit
below 0.25. So the term runs only where `specular.z < SS_MATERIAL_MAX`, which is
one comparison. Read over the material band rather than over the depth, this is
the intended feature: grass and foliage carry the contact term, characters and
terrain do not, and the one-sided reference above is fine once the solid objects
it disagrees with are excluded.

**Thickness must be a bias, not a minimum separation.** As a minimum it rejects
anything thinner than itself. Floored at the step length it reached tens of
centimetres at range, and at 2 cm it exceeded a grass blade, so the ray flew over
the grass and only tall objects were caught.

**A fixed bias cannot work.** Too small and a sloped surface occludes itself (the
screen-space depth gradient across a slope exceeds any small constant); too large
and thin casters are missed. This is what the plane above replaces.

**Step length must be finer than the smallest caster.** Grass blades are 2–5 mm.
At 6 mm per step the ray stepped over most of them; at 2 mm they occlude about as
reliably as thick geometry. This is the single most expensive parameter — 512
steps over a 1 m reach.

## Correction: grass is not excluded

An earlier conclusion in this investigation — that grass is a forward primitive,
absent from the depth buffer, and therefore could never be affected — is **wrong**
and should not be relied on. Grass is drawn into the **deferred masked G-buffer**
as well as the forward softblend pass, so it *is* in the depth buffer and the
term *does* reach it. It is weaker than thick geometry only because of step
resolution, which is the parameter above.

The mistake came from finding `Prim_Poly_Softblend_DL_VF` (a forward pass) and
treating it as the only path for grass. It is a composite on top of the deferred
result, not a replacement for it.

## Parameters

Runtime settings, in the Shadows section of the overlay (enabled by
`Screen-Space Shadows`, off by default). The `SS_*` defines below with no setting
are constants, sized in the comments beside them.

| Setting | Default | Define | Meaning |
|---|---|---|---|
| Screen-Space Shadows | off | `CUSTOM_SS_SHADOWS` | the term's toggle |
| Screen-Space Shadow Strength | 150 (1.5) | `CUSTOM_SS_STRENGTH` | attenuation per unit of occlusion |
| Screen-Space Shadow Reach | 2.5 (% of screen height) | `CUSTOM_SS_REACH` | in ndc.y units, 0.05; ~54 px at 2160p |
| Screen-Space Shadow Thickness | 5 mm | `CUSTOM_SS_THICKNESS` | how far in front of the ray a surface must sit |
| Screen-Space Shadow Material Limit | 25 (0.25) | `CUSTOM_SS_MATERIAL_MAX` | material index the term runs on |
| Screen-Space Shadow Fade Start / End | 40 / 110 m | `CUSTOM_SS_FADE_*` | full strength nearer than start, gone past end |
| — | 64 | `SS_STEPS` | march steps; sized to the reach in pixels |
| — | 1.0 | `SS_SLOPE_BIAS` | multiples of the receiver's depth gradient added to the bias |
| — | 3 mm | `SS_NORMAL_BIAS` | ray start offset along the normal |

Tinting the footprint red (`+= float3(ssOcc * 60, 0, 0)`, temporary) and reading
the tonemap back is the fastest way to see what the term flags, and is what
produced every finding above. The tonemap auto-exposes, so read shapes, not
brightness. The devkit caches a loaded shader: unload before loading after an
edit, or a capture can show the previous build.

## Open

- The material band was measured with three coarse thresholds, so `0.25` is a
  boundary of that measurement, not of the game's own table. Grass and terrain
  both sit below it; separating the two would need finer thresholds if the ground
  ever needs to be excluded. Terrain sharing the band is also why the march still
  runs across the whole near-field ground — the biggest remaining cost lever.
- Distance is unbounded above within the fade: the world reach is proportional to
  depth, so at 110 m it is 3.2 m. A world-space cap on the reach is the first
  thing to add if the term picks up the wrong things at range.
- The jitter uses per-pixel interleaved gradient noise. The sun pass runs before
  TAA, so jittering **per frame** with the engine's TAA jitter and letting TAA
  accumulate would converge the result and allow far fewer steps. `m_cameraCenterOffset`
  is that jitter — currently zero because TAA is off, but required for correct
  unprojection once it is on.
- Not yet combined with the cascade. The intended form is
  `min(cascade, screen-space)`.

## Spot lights

Ported to `SSLighting2_Spot_0x08292A5E` (`shadows/SSLighting2_Spot_0x08292A5E.ps_5_0.hlsl`),
because a spot map has the sun map's problem at a smaller scale: 4096² over a cone
tens of metres deep cannot resolve a grass blade either.

Two things differ from the sun block, both forced by the light rather than chosen:

**The light direction is per pixel.** A spot has a position, not a direction.
`cPSLight.m_lightParams[3].xyz` is the light's view-space position - the vanilla
builds `L = lightPos - viewPos` from it at the top of the shading - so the march
computes and normalises it per pixel. The sun reads one view-space direction from
the constant buffer.

**The receiver position is rebuilt, not reused.** The vanilla already has the view
position in `r5`, but the four shadow taps consume that register before the end of
`main`, so the block reconstructs it from depth with the sun's proven path:
`m_projectionParam` for the linearisation, `m_projection._m00`/`_m11` for the
unprojection. `m_projectionParam.xy` is still the wrong scale, as it was for the sun.

Everything else is the sun's block unchanged: the material gate, the quadratic step
distribution, the bounded caster separation, the fade, and the multiplicative
application. Only the diffuse target (`SV_Target0`) is scaled, as on the sun; the
specular target is left alone, which is worth checking in a capture.

One difference is deliberate and should be back-ported: the sun computes a
slope-scaled bias and then tests against `CUSTOM_SS_THICKNESS` without it, so
`SS_SLOPE_BIAS` never reaches the comparison. The spot port leaves the dead
computation out rather than replicate it. If the sun's comparison is fixed, add the
same bias to both.

**Cost is the open question, and it is why this has its own toggle.** The sun runs
once per frame; this shader is drawn once per shadowed spot light - the snapshot
that identified it showed about 25 per frame - each covering its light's screen
footprint, not the full screen. `Screen-Space Shadows on Spots` is therefore a
separate setting, off by default, sharing the other `Screen-Space Shadow *` dials.
It needs a GPU-timed capture of a lit scene with several shadowed spots before it
is enabled by default.

To verify: build the `renodx-mgsv-shadows` addon, enable `Contact-Hardening Spot
Shadows` plus `Screen-Space Shadows on Spots`, and stand a grass tuft in a
flashlight beam. The tint probe (`r2.xyz += float3(ssOcc * 60, 0, 0)`) still works
for reading the footprint back from the tonemap, with the same caveats as the sun.
