# MGSV grass shadows — feasibility

Status: investigating. Branch `mgsv-grass-shadows` off `mgsv-pcss`.

Question: grass appears to receive no sun shadow. Can it be given one?

## What the naming convention says

The shader name map's legend: `CS` marks a cascade-shadow variant, `NShdw` marks no
shadow. The sun's shadow-casting shader is `SunLight_DL_CS_ES_ps` and carries `CS`;
its sibling `SunLight_DL_ES_ps` does not, and the map already labels that one "sun
light, no cascade shadow".

**No shader in the `Prim_Poly_*` family carries `CS`.** That family is the
forward-lit primitives, which is where grass most likely lives:

```
0x07EFBA51  Prim_Poly_FastDraw_DL_VF_ps.hlsl
0x94C2F79B  Prim_Poly_Softblend_DL_VF_ps.hlsl
0xA21162DF  Prim_Poly_LitDP1_NS_VF_ps.hlsl
0xB170995D  Prim_Poly_LitDP1TA_NS_VF_ps.hlsl
```

(`DL` = directional light, `VF` = vertex-fog, `NS` = no specular.)

## Correction: the inputs are not missing

An early check of `Prim_Poly_FastDraw_SSky` suggested the family had almost nothing
bound — only `cPSMaterial` and `t0`. That was misleading: it is a *sky* shader.

`Primitive_TppSandWind`, a lit forward primitive, binds `cPSScene` (b2),
`cPSObject` (b5), `cPSLight` (b3), `cPSMaterial` (b4) and `cPSSystem` (b0). So
`m_shadowProjection`, `m_shadowProjection2` and `m_shadowMapResolutions` **are**
available to forward primitives.

## Correction: the engine already does this, in a forward shader

`Primitive_TppFourierOceanHQ` (0x1A96649E) contains the whole thing:

```
Texture2D<float4> inShadowTexture : register(t14);

107: // NGetShadow_getShadow: cascade selection, shadow-map comparison, and distance fade.
129: // CalcCascadeShadowUV: choose the active shadow cascade and build projected shadow UV.
230: inShadowTexture.SampleCmp(g_samplerComparisonLess_Linear_Clmap_s, r1.xy, r1.z).x;
```

`CalcCascadeShadowUV` builds the four cascade transforms from
`m_shadowProjection2`'s rows exactly as the sun shader does, reads
`m_lightParams[8]` for the fade and `m_shadowMapResolutions.x` for the directional
map. The receiver position comes from interpolants (`v3.xyz`, `v1.z`) instead of a
G-buffer lookup.

So this is not new work for the engine. It is a *copy* of existing code from one
primitive shader into another.

## Revised plan

1. **Identify the grass shader.** Snapshot with grass in frame, `list_draws`
   filtered to the primitive families, `get_draw` to check bindings. Needs the game
   running. This is also what confirms or kills the whole thing — if grass is not a
   `Prim_Poly_*` draw, the premise is wrong.
2. **Bind the shadow map to `t14`** on the grass draws. It is not bound there now —
   `t3` is a material texture in these shaders, and the shadow map lives at `t14` in
   the family that has it. The addon would capture the SRV from the sun draw, where
   it is bound at `t3`, and re-bind it. C++ work, and the main unknown.
3. **Copy the cascade block** from the ocean shader into the grass shader —
   `CalcCascadeShadowUV` plus the comparison, verbatim.
4. **Add the PCSS block** on top, adapted from the sun's, since the cascade geometry
   is identical. The sun's `texelWorld` derivation applies unchanged.

Estimated **about a day**, plus the resource binding.

## The caveat that may sink it

Cascade 0 texels are **3.2 mm**; grass blades are roughly **2–5 mm** wide. The
shadow map can barely resolve a single blade, so the result would be near-binary
per blade rather than a shadow shape. Contact hardening would soften the aliasing
but cannot invent resolution the map does not have.

Worth judging before investing in the binding work: bind first, look, then decide
whether the cascade block is worth adding.

## CORRECTION: the conclusion below is wrong

The verdict that follows concluded grass could not receive a shadow because it is
vertex-lit and absent from the deferred path. **That second part is wrong.** Grass
is drawn into the **deferred masked G-buffer** as well as the forward softblend
pass, so it *is* in the depth buffer and the screen-space term in the sun shader
does reach it — weakly, because of step resolution, not structurally.

The mistake: `Prim_Poly_Softblend_DL_VF` was found to be a forward pass and treated
as grass's only path. It is a composite on top of the deferred result.

What survives from the analysis below: that shader *is* vertex-lit with no
per-pixel sun term, so porting the cascade lookup into *that* shader would not be
the right route. See `MGSV_ScreenSpaceShadows.md` for what was actually built.

---

## VERDICT: possible, but the payload is wrong

`0x94C2F79B` (`Prim_Poly_Softblend_DL_VF`, 30 draws — the grass) was dumped and
decompiled to `tmp/mgsv/original/0x94C2F79B.hlsl`. Its body is short and has no
lighting of its own:

1. sample `inTexture` (t0), sRGB-decode
2. multiply by interpolants `v3.xyzw` and `v2.xy`/`v2.z`
3. fog, from `g_tex_fog` (t12) and `m_fogParam`
4. alpha test, then sRGB-encode

**There is no per-pixel sun term.** The lighting arrives pre-combined in the vertex
interpolants, so there is nothing to multiply a shadow into except the *whole*
forward contribution — vertex lighting plus fog together. A shadow there would look
plausible but would darken ambient as well as sun, and it would be applied to a
per-vertex lighting signal that has no per-pixel sun component to occlude.

That is a worse starting point than expected, and it is the second time the
`Prim_Poly_*` family has misled: first the sky variant suggested nothing was bound,
now the grass turns out to be vertex-lit.

### What the mechanics do allow

Everything needed is already bound to that draw:

| Resource | Slot | State |
|---|---|---|
| `cPSScene` (shadow projection, cascade ladder, map resolutions) | `b2` | bound and used |
| Sun shadow map, 8192×8192 | `t8` and `t14` | **bound, undeclared** |
| Depth | `t13` | bound and used |

The shader declares only `srv0`, `srv12`, `srv13`. Declaring `t14` and adding the
cascade block would work without any C++ binding work — the resource is already
there. So the plumbing is ready; the problem is that there is nothing correct to do
with it.

No comparison sampler is declared either (`s0`, `s9`, `s11` only), so a port would
either declare `s15` and hope the engine binds it, or read the depth with the point
sampler and compare manually.

### Recommendation

Not worth doing as specified. The honest options are to accept grass shadows as a
crude darkening with visible aliasing, or to leave it. If the goal is grass
receiving shadow, the per-vertex lighting is the obstacle, and that is an engine
design choice rather than something a shader replacement can fix.

## Open questions

- ~~Is grass a `Prim_Poly_*` draw?~~ Yes — `Prim_Poly_Softblend_DL_VF`, 30 draws.
- ~~Is the shadow map bound?~~ Yes, at `t8` and `t14`, undeclared.
- Is `s15` (the comparison sampler) bound to these draws? Unknown; the manual
  compare path avoids the question.

