# MGSV RenoDX roadmap

This page lists the planned work, in the order it should happen, and what each stage must show before the next one starts. How each part works, why it was chosen, and the planned settings are in the [design](docs/DESIGN.md). Current behavior and known limitations live in the feature guides; this page only tracks work that is not done yet.

## Direction

The goal is a path-traced method with DLSS Ray Reconstruction as the best way to play, on top of a robust raster method that stays the default on any GPU, with a ray-traced method in between.

- **Path tracing quality is never traded away.** Path tracing is designed for the best result first. The other methods may reuse its parts, never the reverse.
- **The raster method stands alone.** It never depends on ray-tracing hardware or on the scene being on D3D12.
- **Information before sampling.** Screen-space lighting is limited by what it cannot see, so the raster work first adds a memory of the scene, then a better gather.
- **Each direction counted once.** Linear light, energy conserved, no strength above 100%. Where our lighting has information it replaces the game's ambient light; where it has none, the game's own look stays.
- **Shaders say what, the engine says when.** Shader CRC32s identify draws and never change; new work attaches where the engine itself starts or ends a pass, and is verified against the executable and against captures before it is relied on.
- **Every new feature has a Vanilla setting.**
- **Measured, not assumed.** Speed claims are measured at a fixed camera in the ACC title, at the hillside and at Mother Base, with frame time and GPU time separated. Image changes are judged side by side.

## Overview

| Stage | Contents | Depends on |
|---|---|---|
| 1. Harden | Engine entries that survive game updates, the `engine/` folder, reconstruction coverage, the motion-precision check, temporal quality, cleanup | — |
| 2. Raster lighting | Scene memory, light field, screen-probe gather, reflection chain, forward-drawn surfaces, new settings, BRDF replacement | 1 |
| 3. Atmosphere | Volumetric fog with light shafts, physical sky, clouds | 1; the fog volume can run beside 2 |
| 4. Water | River, pond and ocean shaders: reflections, absorption, caustics | 2's trace and light field; 3's sky |
| 5. Frame generation | FSR, then DLSS, then XeSS frame generation | A D3D12 presentation path; independent of 2 to 4 |
| 6. The scene on D3D12 | Geometry, skinning, materials, lights, acceleration structures | — |
| 7. Ray-traced lighting | Ray-traced shadows, the gather and reflections on real rays, RTXDI, SHaRC | 6 |
| 8. Path tracing | Path tracing with Ray Reconstruction, then caustics | 7 |
| Lowest priority | XeSS upscaling, Analytical TAA improvements | — |

## Surfaces the game draws forward

Our lighting runs on the G-buffer: GI before skin scattering, reflections at the engine's reflection pass. Everything below is drawn afterwards, on top of the finished scene, and lights itself in its own shader from the same small set of inputs: an ambient color pair (up and down) that the engine derives for each object from its light probes, the sun with its shadow map, at most three nearby lights without shadows, and authored cube maps that never change. None of it receives our GI, our reflections or our contact shadows. Found in eight full RenderDoc frames and read in the executable; the per-shader details are in the [engine reference](docs/render-engine.html#notes).

| Group | Examples | Depth | Lit by today |
|---|---|---|---|
| Sky | Baked sky, cloud layers, ray-marched clouds, cloud dome, stars, moon | no | Baked time-of-day tables and its own cloud model |
| Emissive forward models | Lamps, signs, screens (`fox3dfw_constant_*`) | writes | Unlit: a texture times a material color and the exposure |
| Lit forward models | Lambert and glass materials, rain on cabin glass | writes | The ambient pair, the sun with one shadow cascade, three point lights; glass adds the area's authored cube map |
| Ocean | Fourier ocean (Mother Base, ACC), 24-hour ocean | writes | Constant colors in the shader scaled by the sun color; the sun's shadow near shore; no reflection of the scene |
| Rivers, ponds, marsh | River and marsh meshes | no | The ambient pair and the sun; a cube map authored per river, faded by the sun's shadow; a refracted copy of the scene |
| Particles, unlit | Fire, glows, tracers, beams, rain streaks | no | Unlit; glows divide by the exposure to hold their brightness on screen |
| Particles, lit | Smoke, dust, debris | some | The up color of the ambient pair, the sun without a facing term, one to three point lights |
| Liquids and rain drops | Splashes, spray, rain | no | The ambient pair, the sun's shadow, two or three point lights, one fixed cube map, a refracted scene copy |
| Screen-space effects | Heat haze, optical camouflage, footprints, sand wind, scorch marks and embers around fires | no | They bend or tint the finished scene; the scorch marks read the G-buffer |
| Fakes | Blob shadows under characters' feet | no | An unlit dark blob standing in for a contact shadow |
| Night vision and thermography | Thermography models, grass, collectibles | writes | Unlit flat colors |
| After post-processing starts | Target markers, lens drops, screen filters, HUD | no | Screen effects; they stay as they are in every method |

Skid marks are not on this list: they are G-buffer decals and already receive our lighting. The fog every one of these shaders applies comes from one small image the engine fills once per frame (128 depth slices of 16 by 64 cells, no shadow maps), so replacing what is written there changes fog for all of them at once.

How each lighting method treats these surfaces is in the design: [raster](docs/DESIGN.md#48-forward-drawn-surfaces), [water](docs/DESIGN.md#6-water), [ray-traced](docs/DESIGN.md#9-ray-traced-method) and [path-traced](docs/DESIGN.md#10-path-traced-method).

## Stage 1: harden what exists

Reconstruction runs where the engine starts post-processing; contact shadows run as one batch per lighting pass; Global Illumination is one mode with a reduced material capture.

**Engine entries that survive game updates.** This comes first, because every later stage adds hooks. Today about 60 code addresses and 70 structure offsets sit in seven feature files, half of them as unnamed numbers, five files each check the executable for themselves, and any rebuilt executable turns off every engine-side feature at once. The [design](docs/DESIGN.md#13-surviving-a-game-update) replaces that with one table of named entries, each found by a byte pattern that skips everything that moves with the code, confirmed against the executable's function table and by its relations to other entries, so a small game update needs no new mod build.

1. **The table and one startup check, no behavior change.** Create `engine/build.hpp` and `engine/image.hpp`; move every address, offset and byte check into the table under engine names; generate `engine/shader_names.hpp` from the dumped shaders' debug names and replace the bare shader hashes in the code with those names; features ask the table and fail one at a time with a menu note. Only the known-build path runs. **Gate:** on the current executable the same hooks install and the log is the same; with one entry deliberately broken, only the feature that needs it turns off.
2. **The authoring tool.** Decodes instructions, writes the shortest unique pattern for every entry with wildcards on moving fields, and resolves the table offline against any executable. **Gate:** every pattern unique on the current build and every relation holding.
3. **Search at startup for unknown builds,** with function-table and relation checks. **Gate:** with the current executable treated as unknown, every entry resolves to exactly its stored address.
4. **Trial on another build.** If an older build of the game can be obtained, the whole path is tried on it. Game updates are not expected to change shaders, so only a startup report of ported shaders the game never loads is added.

**Folder layout.** The `engine/` folder arrives with the step above and collects the engine-facing headers as they are converted. Every other move in the [planned layout](docs/DESIGN.md#14-folder-layout) happens with the stage that touches that code.

**Reconstruction coverage.** Check in game, with the probe's start counters equal to its cut counters: bloom off, depth of field off and motion blur off while upscaling (motion blur off is the first run of the motion-vector hook), cutscenes, night vision, binoculars, the half-resolution particle setting, the iDroid, loading, pause and resize. Measure Off, Native and Performance at the hillside and at Mother Base by day and night.

**Open defect.**

- Slight character wobble. First suspect: the precision of object motion. Reconstruction computes camera motion itself from depth and the camera matrices, but for characters and other moving objects it uses the game's own motion vectors. The engine now stores those in RGBA16F, yet the game's shaders still use the 8-bit layout: 0 to 1, with 0.5 meaning no motion and the ends meaning ±64 pixels. Half floats are least precise near 0.5, so small motion is stored in steps of 1/32 of a render pixel in one direction and 1/16 in the other. Test in a probe build: write signed pixel motion (zero at zero) into a separate float image for reconstruction and lighting, leaving the image the game's motion blur reads untouched, and compare wobble at the ACC title and on a walking character. If that is not the cause, match each character's current color to its current motion and its previous motion to the last accumulated color. Native object motion is clamped at 64 input pixels, so fast motion is checked separately. Disruptive motion experiments stay in the probe.

**Projection coverage.** Spherical-harmonic volume kinds other than kind 0, terrain decals at Native input size, thermography's alternate branch, mirrors and other secondary views.

**Temporal quality.**

- Find a native camera-cut signal (aiming, binoculars, field-of-view changes, cuts, teleports, pause) so history resets exactly when it should. Scene memory and frame generation need the same signal. The old clip-space heuristic reset history continuously and stays removed.
- Reactive and transparency masks for FSR and DLSS from verified material membership: rain first, then particles, animated textures and emissive transparency.

**Reconstruction experiments.** Each is kept only if a side-by-side test shows it is better:

- Give DLSS and FSR the scene's own depth-stencil instead of the G-buffer depth, so the ocean and forward-drawn models get their own depth and camera motion.
- Pass the engine's exposure to DLSS and FSR as pre-exposure, judged on a dark-to-bright transition. Low priority: Ray Reconstruction ignores exposure.

**Cleanup.**

- Move the lighting history's motion record to the engine's camera-velocity hook, then remove the per-draw handler and its descriptor tracker.
- Investigate letting the engine create the post-processing targets at output size, one plugin at a time, instead of redirecting them. First deliverables: the redirection's measured cost and a table of where each pass takes its size.
- Decide the DLSS model default: it is preset F, which NVIDIA's current guide marks deprecated.

**Exit:** no file outside `engine/` holds an address, offset or byte pattern, the current build resolves identically by search, a broken entry turns off only its own feature, the coverage list passes, the wobble is explained or fixed, and the measurements are recorded.

## Stage 2: raster lighting

Today's Global Illumination is replaced, not tuned: it can only gather light from what the current frame shows. The new method is described in the [design](docs/DESIGN.md#4-raster-method). Its steps are ordered so each one ships something and the riskiest part is judged first. The earlier plan for a cheaper gather (`build/analysis/mgsv-gi-unified-20261118/PLAN.md`, steps 3 to 5) is superseded; its constraints still apply.

**Step 1: facts.** Settle the open questions the design depends on: whether every shadow cascade is redrawn each frame and what is drawn at night and indoors, how many occluder polygons a scene has and where, whether terrain height data can be read, and whether the engine's targets can be created shareable so D3D12 reads them without copies.

**Step 2: scene memory.** The world-aligned grids, fed by the camera, the sun's shadow maps and (if step 1 supports them) occluder polygons and terrain height, with moving objects kept out. Nothing reads it yet except a debug view. **Gate:** in normal play outdoors, most of the space around the player is known within seconds; interiors entered for the first time fail toward dark, not toward leaks; the cost of upkeep is measured. If the gate fails, the later steps run on the screen and the game's authored probes alone, and the tiers shrink accordingly.

**Step 3: hit lighting and the light field.** The trace function, lighting at world hits from the albedo cache, the sun's shadow maps and the light list, and the world probe grid with its irradiance grid. This step ships the Medium tier: the irradiance grid replaces the game's ambient light as far as the scene is known, with the short bitmask pass for near detail. Forward-drawn surfaces get the grid's light through the one engine function that sets their ambient colors. **Gate:** bounce light stays when the camera turns away from a sunlit wall or a lamp; a uniform-light test passes; a dark interior beside a bright exterior does not leak.

**Step 4: the screen-probe gather.** Probes on the depth buffer, their filters, and the merge with the near layer. This is the High tier and becomes the default. **Gate:** at the default tier the image matches or beats today's in the fixed scenes, stays stable under the upscaler without a diffuse denoiser, and its cost is measured.

**Step 5: the reflection chain.** Rough reflections from the probes, glossy screen rays that continue into scene memory on a miss, and the cube map as the last fallback: corrected for parallax, dimmed by what the ray found in its way, and never brighter than the locally gathered light. The rays move into the main D3D12 block and read the previous frame's scene image, so the engine's reflection pass only composites, which removes one of today's three round trips. **Gate:** side by side with today's current-frame source, moving characters and vehicles in reflections show no visible delay, and smoke, particles and fog, which last frame's image contains but today's source does not, do not smear into reflections; if they do, the reflection block stays where it is and the frame keeps three round trips.

**Step 6: forward-drawn surfaces per pixel.** Ports of the glass and lit forward model shaders that sample the irradiance grid and take reflections from the trace. Emissive models become light for their surroundings where no engine light already does that job. The blob shadows under characters are removed where contact shadows cover them.

**Step 7: settings.** Lighting Method and Lighting Quality arrive; the tuning controls of the old gather are retired. The full layout is in the [design](docs/DESIGN.md#11-settings).

**BRDF replacement (for example EON diffuse).** Possible, and best done here, as the diffuse lobe of the one shared BSDF rather than a patch on individual shaders. Direct diffuse is computed inside the engine's light shaders, which Enhanced shadows already port, so the lobe can change there; ambient, environment, GI application, and later ray and path tracing must all use the same lobe, or energy is counted differently in each. The earlier attempt (kept in `git stash` as the 2026-11-18 EON experiment) changed many shaders before the material model was settled; this time the order is material record first, then a uniform-light test of every consumer, then the visual comparison. Skin keeps the engine's scattering passes; a physical skin model is a later, separate decision.

**Exit:** the High tier is the default and passes its gates; every tier's cost is measured in the fixed scenes; the uniform-light test passes for the gather, the reflection chain and the forward lighting.

## Stage 3: atmosphere

The engine already has the pieces this replaces or feeds: a precomputed-sky plugin (`PRECOMPUTE_SKY`), the sky draws, the fog image (`GLOBAL_VOLUMETRIC_FOG`) and fog applied inside deferred composition. The method is in the [design](docs/DESIGN.md#5-atmosphere).

**Step 1: the fog volume.** Our own view-aligned volume, lit by the sun through its shadow maps, by ambient light and by local lights, written into the engine's fog image enlarged at creation. Every surface, forward-drawn ones included, then shows soft light shafts with no other shader changed. With shadows ignored the result must match the game's fog. **Gate:** the enlarged image works for every consumer.

**Step 2: sharp shafts.** A per-pixel average of sun visibility along the view ray, used by the composition shader for opaque surfaces.

**Step 3: the sky.** Sky and clouds projected to spherical harmonics and a small environment map each frame, for the fog, the reflection fallback and the water. Then the physical sky: per-frame atmosphere tables fitted to the game's baked tables, with the remaining difference kept as the explicit Authored Look control. If the fit is poor for a weather type, the baked tables stay and only the projections are added.

**Step 4: clouds.** Relight the game's clouds from the same sun and sky; add a cloud shadow map read by sun lighting, fog and shafts.

**Exit:** day, night, rain and sandstorm look correct side by side with Vanilla; fog and sky agree at the horizon; GI and reflections see the same sky; cost is measured at Mother Base.

## Stage 4: water

The pond that started this: its reflection is one authored cube map that fades in the sun's shadow, the view through the water ignores Fresnel, and the ocean reflects fixed colors. The method is in the [design](docs/DESIGN.md#6-water).

- **Rivers and ponds first.** Ports that keep the game's ripples, flow and foam and add an in-shader screen reflection, real Fresnel, absorption with depth, ambient light from the irradiance grid, and caustics from the ripple normals.
- **The ocean second**, the same way, reading last frame's image because it draws before the engine's scene copy exists.
- **The surface record.** Every port writes the water's normal, roughness, depth and type to a side target, for reconstruction and frame generation now and for the traced methods later.
- **Wet surfaces** are audited against a published wetness model before anything changes.

**Exit:** the pond and the ocean reflect the scene and the sky in sun and in shade; water looks right side by side with Vanilla at the hillside and at Mother Base in every weather; cost is measured.

## Stage 5: frame generation

After the raster stages and before ray tracing. All three providers present through a D3D12 swap chain; none has a D3D11 path. The method and its inputs are in the [design](docs/DESIGN.md#7-frame-generation).

**Step 1: the presentation path.** An experiment decides between replacing the game's swap chain and leaving it to ReShade and RenoDX while a D3D12 swap chain shows the frames. It must keep the ReShade overlay, HDR10 output and even pacing.

**Step 2: inputs.** The image without the HUD and the HUD as its own layer, split by the engine's draw lists; depth and dense motion, including the water surface record; camera data latched once per frame; the camera-cut signal from stage 1.

**Step 3: providers.** FSR first, then DLSS Frame Generation with Reflex in the engine's main loop, then XeSS (last, as few players have Intel GPUs).

**Exit:** each provider is selectable, the HUD is never interpolated, frame pacing is even, generation is off in menus and loading, and HDR output and the overlay still work.

## Stage 6: the scene on D3D12

The prerequisite for every ray-traced feature, and the largest piece of reverse engineering left. It adds nothing visible on its own. The list is in the [design](docs/DESIGN.md#8-the-scene-on-d3d12): static geometry and instances from the engine's model resources and scene lists, terrain and vegetation, skinned characters from the bone matrices each frame's draws upload, a simplified material at each hit, the forward set flagged apart, alpha-tested geometry, lights with their authored bounds, and a top-level structure rebuilt every frame.

**Exit:** a debug view traces primary rays against the D3D12 scene and matches the G-buffer's depth and materials, including skinned characters and off-screen geometry, at a known update cost and memory use.

## Stage 7: ray-traced lighting

The raster method with real rays wherever a ray helps, on any ray-tracing GPU, denoised by NRD ([design](docs/DESIGN.md#9-ray-traced-method)).

- **Shadows:** one ray toward the sun's disk; every local light can cast a shadow, chosen per pixel by resampling, with RTXDI's ReSTIR direct lighting as the higher setting.
- **Diffuse indirect:** the stage 2 gather with its world trace replaced by ray queries, and hits lit from SHaRC.
- **Reflections:** rays below a roughness cutoff, probes above.
- **Forward surfaces and fog:** water and glass trace from the surface record; the fog volume takes its visibility from rays.

**Exit:** each part is selectable, never darker or brighter than the raster method in a uniform-light test, stable in the fixed scenes, with its cost measured.

## Stage 8: path tracing

The flagship ([design](docs/DESIGN.md#10-path-traced-method)). It requires DLSS Ray Reconstruction.

- Opaque pixels start from the G-buffer; everything in front of it (glass, water, emissive and blended models) is traced as real surfaces in up to three layers.
- Direct light is sampled and shadowed by rays at every bounce, with ReSTIR direct lighting at the first; indirect light is one path per pixel ended in SHaRC; resampling of indirect light is optional.
- Ray Reconstruction runs in the slot reconstruction uses today, before depth of field, with particles and fog delivered through its transparency inputs.
- A reference accumulation mode gives the ground truth every other method is compared with.

**Caustics**, after the method is stable. One sample per pixel cannot find sunlight through water, so sun caustics come from a sun-view caustic map traced through the water surface, the same data the raster water uses. Glass caustics are a later, smaller item.

**Exit:** path tracing is selectable, stable in the fixed scenes, matches its own accumulated reference, and its cost is measured against the other methods.

## Lowest priority

- **XeSS.** A fourth upscaler in the existing reconstruction slot, using the same inputs as DLSS and FSR. Few players have Intel GPUs, which is also why XeSS frame generation comes last.
- **Analytical TAA.** Previous-depth and disocclusion checks before any relaxation of its clipping. DLSS and FSR are the methods that matter.

## Rules that carry through every stage

- Keep custom work on one D3D12 queue unless a measurement shows a second queue helps; share only boundary images between the APIs.
- The game keeps rendering on D3D11. Our passes run on D3D12 where it gives something D3D11 cannot (ray queries, Shader Model 6 features, the SDKs), grouped so every method makes two round trips per frame, as the [design](docs/DESIGN.md#31-what-runs-on-d3d11-and-what-runs-on-d3d12) lists pass by pass; a new switch between the APIs needs a measurement that it pays for itself.
- Keep native shaders running on D3D11 until a whole pass moves.
- Hooks stay loaded for the process lifetime; restart to replace or remove them.
- Pick shader models and hardware features from what the device actually supports, and measure each one inside the pass that uses it.
- Check each SDK's license before it is added to the repository (RTXDI, SHaRC, NRD, Ray Reconstruction, Streamline, FSR, XeSS).
