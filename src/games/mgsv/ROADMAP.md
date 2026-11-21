# MGSV RenoDX roadmap

This page lists the planned work, in the order it should happen, and what each stage must show before the next one starts. How each part works, why it was chosen, and the planned settings are in the [design](docs/DESIGN.md). Current behavior and known limitations live in the feature guides; this page only tracks work that is not done yet.

## Direction

The goal is a path-traced method with DLSS Ray Reconstruction as the highest-quality option, an affordable raster method as the default, and a ray-traced method in between.

- **Path tracing quality is not reduced for the other methods.** Path tracing is designed for the best result. The other methods may reuse its parts, never the reverse.
- **The raster method stays affordable.** It needs a D3D12 GPU, as reconstruction already does, but no ray-tracing hardware, and it does not wait for the traced methods' scene capture. It may trace in software where that is cheap. Its default tier costs at most about 1 ms of GPU time more than today's Global Illumination; more expensive work belongs to its Ultra tier or to the traced methods.
- **Information before sampling.** Screen-space lighting is limited by what it cannot see. Seeing more takes rays against the real scene, so that work belongs to the ray-traced method; the raster method keeps the game's authored light wherever the screen cannot show what is there.
- **Each direction counted once.** Linear light, energy conserved, no strength above 100%. Where our lighting has information it replaces the game's ambient light; where it has none, the game's look stays.
- **Shaders say what, the engine says when.** Shader CRC32s identify shaders and never change (a shader many draws share also needs the draw's role); new work attaches where the engine itself starts or ends a pass, and is verified against the executable and against captures before it is relied on.
- **Every new feature has a Vanilla setting.**
- **Measured, not assumed.** Speed claims are measured at a fixed camera in the ACC title, at the hillside and at Mother Base, with frame time and GPU time separated. Image changes are judged side by side.

## Overview

Stage 1, hardening what exists, is done.

| Stage | Contents | Depends on |
|---|---|---|
| 2. Raster lighting | Multiple bounces (built), a cheaper gather, reflections with variable ray density, Lighting Quality, BRDF replacement | — |
| 3. Atmosphere | Volumetric fog with light shafts, physical sky, clouds | The fog volume can run beside 2 |
| 4. Water | River, pond and ocean shaders: reflections, absorption, caustics | 3's sky |
| 5. Frame generation | FSR, then DLSS, then XeSS frame generation | Independent of 2 to 4; it needs its own camera-cut signal (stage 2 did not need one) |
| 6. The scene on D3D12 | Geometry, skinning, materials, lights, acceleration structures | New engine entries: bone uploads, scene lists |
| 7. Ray-traced lighting | Ray-traced shadows, screen probes on real rays over the bitmask gather, reflections on real rays, forward surfaces lit from the radiance cache, RTXDI, SHaRC | 6 |
| 8. Path tracing | Path tracing with Ray Reconstruction, then caustics | 7 |
| Lowest priority | XeSS upscaling, Analytical TAA improvements, reactive masks, projection coverage | — |

## Surfaces the game draws forward

Our lighting runs on the G-buffer: GI before skin scattering, reflections at the engine's reflection pass. Everything below computes its lighting inside its forward shader. The sky, the clouds and the forward models draw before the reflection pass and appear in today's reflections; the rest draws after it. They share a small set of inputs: an ambient color pair (up and down) that the engine derives for each object from its light probes, the sun with its shadow map, at most three nearby lights without shadows, and authored cube maps chosen by area, trigger volumes and weather, which never show the live scene. None of it receives our GI, our reflections or our contact shadows. Found in eight full RenderDoc frames and read in the executable; the per-shader details are in the [engine reference](docs/render-engine.html#notes).

| Group | Examples | Depth | Lit by today |
|---|---|---|---|
| Sky | Baked sky, cloud layers, ray-marched clouds, cloud dome, stars, moon | no | Baked time-of-day tables and the engine's cloud model |
| Emissive forward models | Lamps, signs, screens (`fox3dfw_constant_*`) | writes | Unlit: a texture times a material color and the exposure |
| Lit forward models | Lambert and glass materials, rain on cabin glass | writes | The ambient pair, the sun with one shadow cascade, three point lights; glass adds the area's authored cube map |
| Ocean | Fourier ocean (Mother Base, ACC), 24-hour ocean | Fourier writes; 24-hour no | Constant colors in the shader scaled by the sun color; the sun's shadow in the high-detail rings; no reflection of the scene; the 24-hour ocean also refracts the engine's scene copy |
| Rivers, ponds, marsh | River and marsh meshes | no | Rivers: the ambient pair and the sun; a cube map authored per river, faded by the sun's shadow; a refracted copy of the scene. Marsh: the sun without shadow, two point lights and a cube map set per marsh; no refraction |
| Particles, unlit | Fire, glows, tracers, beams, rain streaks | no | Unlit; glows divide by the exposure to hold their brightness on screen |
| Particles, lit | Smoke, dust, debris | some | The up color of the ambient pair, the sun without a facing term, zero to three point lights |
| Liquids and rain drops | Splashes, spray, rain | no | Varies by shader: the simplest take one point light and no sun shadow; some variants add the sun's shadow, more point lights, a fixed cube map or a refracted scene copy |
| Screen-space effects | Heat haze, optical camouflage, footprints, sand wind, scorch marks and embers around fires | no | They bend or tint the finished scene; the scorch marks read the G-buffer |
| Fakes | Blob shadows under characters' feet | no | An unlit dark blob standing in for a contact shadow |
| Night vision and thermography | Thermography models, grass, collectibles | writes | Unlit flat colors |
| After post-processing starts | Target markers, lens drops, screen filters, HUD | no | Screen effects; they stay as they are in every method |

Skid marks are not on this list: they are G-buffer decals and already receive our lighting. The fog nearly all of these shaders apply comes from one small image the engine fills once per frame (128 depth slices of 16 by 64 cells, no shadow maps); particles without a volumetric-fog variant, the scattering effect and the sky apply fog differently or not at all. Replacing what is written in that image changes fog for the rest at once.

How each lighting method treats these surfaces is in the design: [raster](docs/DESIGN.md#45-forward-drawn-surfaces), [water](docs/DESIGN.md#6-water), [ray-traced](docs/DESIGN.md#9-ray-traced-method) and [path-traced](docs/DESIGN.md#10-path-traced-method).

## Stage 2: raster lighting

Today's Global Illumination is kept and made better within what the screen shows. Light from surfaces off screen, and what the directions the screen leaves open really see, belong to the ray-traced method (stage 7); the raster method keeps the game's ambient light there. The method is in the [design](docs/DESIGN.md#4-raster-method). Its default tier must cost at most about 1 ms of GPU time more than today in the same scene (today: about 2 to 3 ms at the sortie menu and Mother Base and about 3.6 ms on the hillside, at 1920×1080 internal on an RTX 5080). The remaining steps follow the earlier plan for a cheaper gather and reflections (`build/analysis/mgsv/mgsv-gi-unified-20261118/PLAN.md`, steps 3 to 5), whose constraints and validation still apply.

**Step 1: facts.** The executable answered the open questions; the answers are in the design. Captures at Mother Base and in Africa confirmed the shadow, mask and terrain answers; sharing the 8-bit G-buffer targets cost nothing measurable at the ACC title, which is to be repeated in a scene with a heavy G-buffer before the main block relies on it.

**Step 2: multiple bounces (built).** The gather's source includes the previous frame's indirect light, carried through camera and object motion, so light keeps bouncing ([guide](lighting/indirect/README.md)). Before it, a world-space scene memory and a probe grid traced through it were built and then removed; why is in the [design](docs/DESIGN.md#47-considered-and-not-chosen).

**Step 3: a cheaper gather.** A pyramid that stores the nearest surface's depth with its source light, fewer steps where it makes them redundant, and output as spherical harmonics so each full-resolution pixel is lit with its own normal. Then a test of whether a D3D12 gather started after the G-buffer overlaps the game's shadow and light draws; the trace moves to D3D12 only if it does. **Gate:** error against a converged reference no higher than today's default, no more flicker at a static camera, the uniform-light test passes, and the cost is lower.

**Step 4: reflections.** Variable ray density (one ray per pixel on sharp surfaces, one per 2×2 pixels elsewhere), first with NRD's own reconstruction of skipped pixels and, if that is noisier than today, with a pass that reuses neighbors' hits; fewer duplicate outputs from the input pass, and the engine's targets tried shareable. Then the rays and their denoising move into the main D3D12 block for the pixels the G-buffer marks as possible receivers, reading the mod's copies of last frame's reflection-pass image and depth; the engine's reflection pass applies composition's receiver data to the finished texture, so the frame keeps today's two round trips. **Gate:** as step 3, on the reflection output and the final image; the G-buffer marks the same receivers and roughness that composition uses today; and side by side with today's current-frame source, moving characters and vehicles in reflections show no visible delay. If either of the last two fails, the reflection block stays at the reflection pass and the frame makes three round trips.

**Step 5: settings.** Lighting Quality arrives, mapped to the gather's resolution and steps and to the reflection ray density, and the tuning controls the new passes fix are retired. Lighting Method arrives with the ray-traced method. The full layout is in the [design](docs/DESIGN.md#11-settings). The blob shadows under characters are removed where contact shadows cover them.

**BRDF replacement (for example EON diffuse).** Possible, and best done here, as the diffuse lobe of the one shared BSDF rather than a patch on individual shaders. Direct diffuse is computed inside the engine's light shaders, which Enhanced shadows already port, so the lobe can change there; ambient, environment, GI application, and later ray and path tracing must all use the same lobe, or energy is counted differently in each. The earlier attempt (kept in `git stash` as the 2026-11-18 EON experiment) changed many shaders before the material model was settled; this time the order is material record first, then a uniform-light test of every consumer, then the visual comparison. Skin keeps the engine's scattering passes; a physical skin model is a later, separate decision.

**Exit:** the gather and the reflections pass their gates within the budget; every tier's cost is measured in the fixed scenes; the uniform-light test passes for the gather and the reflection chain.

## Stage 3: atmosphere

The engine already has the pieces this replaces or feeds: a precomputed-sky plugin (`PRECOMPUTE_SKY`), the sky draws, the fog image (`GLOBAL_VOLUMETRIC_FOG`) and fog applied inside deferred composition. The method is in the [design](docs/DESIGN.md#5-atmosphere).

**Step 1: the fog volume.** A view-aligned volume computed by the mod, lit by the sun through its shadow maps, by ambient light and by local lights, written into the engine's fog image enlarged at creation. Every surface, forward-drawn ones included, then shows soft light shafts with no other shader changed. With shadows ignored the result must match the game's fog. **Gate:** the enlarged image works for every consumer.

**Step 2: sharp shafts.** A per-pixel average of sun visibility along the view ray, used by the composition shader for opaque surfaces.

**Step 3: the sky.** Sky and clouds projected to spherical harmonics and a small environment map each frame, for the fog, the reflection fallback and the water. Then the physical sky: per-frame atmosphere tables fitted to the game's baked tables, with the remaining difference kept as the explicit Authored Look control. If the fit is poor for a weather type, the baked tables stay and only the projections are added.

**Step 4: clouds.** Relight the game's clouds from the same sun and sky; add a cloud shadow map read by sun lighting, fog and shafts.

**Exit:** day, night, rain and sandstorm look correct side by side with Vanilla; fog and sky agree at the horizon; GI and reflections see the same sky; cost is measured at Mother Base.

## Stage 4: water

The pond that started this: its reflection is one authored cube map that fades in the sun's shadow, the view through the water ignores Fresnel, and the ocean reflects fixed colors. The method is in the [design](docs/DESIGN.md#6-water).

- **Rivers and ponds first.** Ports that keep the game's ripples, flow and foam and add an in-shader screen reflection, real Fresnel, absorption with depth, the game's ambient light, and caustics from the ripple normals.
- **The oceans second**, the same way. The Fourier ocean and the marsh draw before the engine's scene copy exists and read last frame's image instead; the 24-hour ocean draws after it.
- **The surface record.** Every port writes the water's normal, roughness, depth and type to a side target, for reconstruction and frame generation now and for the traced methods later.
- **Wet surfaces** are audited against a published wetness model before anything changes.

**Exit:** the pond and the ocean reflect the scene and the sky in sun and in shade; water looks right side by side with Vanilla at the hillside and at Mother Base in every weather; cost is measured.

## Stage 5: frame generation

After the raster stages and before ray tracing. All three providers present through a D3D12 swap chain; none documents a D3D11 path. The method and its inputs are in the [design](docs/DESIGN.md#7-frame-generation).

**Step 1: the presentation path.** An experiment decides between replacing the game's swap chain and leaving it to ReShade and RenoDX while a D3D12 swap chain shows the frames. It must keep the ReShade overlay, HDR10 output and even pacing.

**Step 2: inputs.** The image without the HUD and the HUD as a separate layer, split by the engine's draw lists; depth and dense motion, including the water surface record; camera data latched once per frame; a camera-cut signal. Stage 2 found only the cutscene "cancel camera blur" bit; aiming, binoculars, field-of-view changes and teleports still need a signal of their own.

**Step 3: providers.** FSR first, then DLSS Frame Generation with Reflex in the engine's main loop, then XeSS (last: it needs Intel's latency library, which cannot run beside Reflex).

**Exit:** each provider is selectable, the HUD is never interpolated, frame pacing is even, generation is off in menus and loading, and HDR output and the overlay still work.

## Stage 6: the scene on D3D12

The prerequisite for every ray-traced feature, and the largest piece of reverse engineering left. It adds nothing visible by itself. The list is in the [design](docs/DESIGN.md#8-the-scene-on-d3d12): static geometry and instances from the engine's model resources and scene lists, terrain and vegetation, skinned characters from the bone matrices each frame's draws upload, a simplified material at each hit, the forward set flagged apart, alpha-tested geometry, lights with their authored bounds, and a top-level structure rebuilt every frame.

**Exit:** a debug view traces primary rays against the D3D12 scene and matches the G-buffer's depth and materials, including skinned characters and off-screen geometry, at a known update cost and memory use.

## Stage 7: ray-traced lighting

The raster method with real rays wherever a ray helps, on any ray-tracing GPU, denoised by NRD ([design](docs/DESIGN.md#9-ray-traced-method)).

- **Shadows:** one ray toward the sun's disk; every local light can cast a shadow, chosen per pixel by resampling, with RTXDI's ReSTIR direct lighting as the higher setting.
- **Diffuse indirect:** screen probes whose rays are ray queries against the real scene, with hits lit from SHaRC, over the bitmask gather as the near layer; they replace the game's ambient light in the directions the gather finds open.
- **Reflections:** rays below a roughness cutoff, screen probes above.
- **Forward surfaces and fog:** water and glass trace from the surface record; lit forward objects take the traced light at their position through the engine function that sets their ambient colors; emissive models without an engine light become lights; the fog volume takes its visibility from rays.

**Exit:** each part is selectable, never darker or brighter than the raster method in a uniform-light test, stable in the fixed scenes, with its cost measured.

## Stage 8: path tracing

The highest-quality method ([design](docs/DESIGN.md#10-path-traced-method)). It requires DLSS Ray Reconstruction.

- Opaque pixels start from the G-buffer; everything in front of it (glass, water, emissive and blended models) is traced as real surfaces in up to three layers.
- Direct light is sampled and shadowed by rays at every bounce, with ReSTIR direct lighting at the first; indirect light is one path per pixel ended in SHaRC; resampling of indirect light is optional.
- Ray Reconstruction runs in the slot reconstruction uses today, before depth of field, with particles delivered through its transparency inputs; fog is applied by composition to opaque surfaces and by the main block to the traced layers.
- A reference accumulation mode gives the ground truth every other method is compared with.

**Caustics**, after the method is stable. One sample per pixel almost never finds sunlight through water, so sun caustics come from a sun-view caustic map traced through the water surface. Glass caustics are a later, smaller item.

**Exit:** path tracing is selectable, stable in the fixed scenes, matches its accumulated reference, and its cost is measured against the other methods.

## Lowest priority

- **XeSS.** A fourth upscaler in the existing reconstruction slot, using the same inputs as DLSS and FSR.
- **Analytical TAA.** Previous-depth and disocclusion checks before any relaxation of its clipping. DLSS and FSR are the methods that matter.
- **Projection coverage.** Spherical-harmonic volume kinds other than kind 0, terrain decals at Native input size, thermography's alternate branch, mirrors and other secondary views.
- **Reactive and transparency masks for DLSS and FSR**, from verified material membership. Optional: rain and particles already look good with DLSS.

## Rules that carry through every stage

- Keep custom work on one D3D12 queue unless a measurement shows a second queue helps; share only boundary images between the APIs.
- The game keeps rendering on D3D11. Our passes run on D3D12 where it gives something D3D11 cannot (ray queries, Shader Model 6 features, the SDKs), grouped so every method makes two round trips per frame, as the [design](docs/DESIGN.md#31-what-runs-on-d3d11-and-what-runs-on-d3d12) lists pass by pass; a new switch between the APIs needs a measurement that it pays for itself.
- Keep native shaders running on D3D11 until a whole pass moves.
- Hooks stay loaded for the process lifetime; restart to replace or remove them.
- Pick shader models and hardware features from what the device actually supports, and measure each one inside the pass that uses it.
- Check each SDK's license before it is added to the repository (RTXDI, SHaRC, NRD, Ray Reconstruction, Streamline, FSR, XeSS).
