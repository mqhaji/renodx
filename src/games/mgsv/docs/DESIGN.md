# MGSV planned rendering design

This page describes how the planned lighting, atmosphere, water, frame generation and settings fit together, how the mod should survive game updates, how its folders should be laid out, and why each part was chosen. The [roadmap](../ROADMAP.md) says what is built in which order and what each stage must show. The [architecture guide](ARCHITECTURE.md) describes what exists today. Nothing here is built unless a feature guide says so. The last section says how each claim on this page is known.

## 1. Rules

- **Path tracing quality is never traded away.** Path tracing is designed for the best result first. The other methods may reuse its parts, never the reverse.
- **The raster method is the default and stands alone.** It never needs ray-tracing hardware or the scene on D3D12.
- **Information before sampling.** The weakness of screen-space lighting is what it cannot see (off-screen light, hidden surfaces, thickness), not how it samples. Work goes first into knowing more of the scene, then into estimating it better.
- **Each direction counted once.** Linear light, energy conserved, no strength above 100%. Where our lighting has information it replaces the game's ambient light; where it has none it leaves the game's ambient light in place. It never adds one on top of the other.
- **Unknown is dark or authored, never bright.** A ray that ends somewhere we know nothing about takes the game's own authored light for that place, not the sky.
- **Stored light is recomputed where it can be.** Caches keep what a surface is (albedo), and light it again from the sun's shadow maps, the light list and the cached bounce light. Stored lit color is used only where nothing better exists.
- **Engine boundaries, not shader guesses.** Work attaches where the engine starts or ends a pass.
- **Every feature has a Vanilla setting**, and a dependency on another setting is shown in the menu, never silent.
- **Measured, not assumed.** Costs on this page are estimates from published numbers until our own measurements replace them.

## 2. What the engine provides

| Input | What it holds | Notes |
|---|---|---|
| G-buffer and depth | Albedo, world normal, roughness, specular, material id, depth | Opaque and masked surfaces only |
| Motion | Composed velocity, and object velocity that also marks moving objects | Camera motion is computed analytically |
| Lit scene color | The scene before post-processing, this frame and the last | Includes forward-drawn surfaces |
| Sun shadow maps | Four 4096×4096 cascades in one 8192×8192 atlas, reaching roughly 120 units from the camera in the frames inspected | A depth view from the sun, on and off screen; most casters are drawn with front faces culled, so it holds their far sides |
| Local lights | Point and spot lights read at the engine's local-light pass; spot shadow maps and dual-paraboloid point shadow maps for the lights that have them | Many lights are unshadowed |
| Authored light probes | Up to 63 spherical-harmonic volumes per view plus the sky's spherical harmonics, drawn as ambient light | The game's own indirect light: static and unoccluded, but leak-free by authoring |
| Area reflection cube | One authored cube map per area, chosen by scripts and trigger volumes | Never changes with the scene |
| Fog image | 512×256 float image, 128 depth slices of 16×64 cells; RGB is in-scattered light, alpha is transmittance | Sampled by deferred composition and every forward shader; no shadow input |
| Occluder polygons | Coarse authored polygons the engine uses for its own occlusion culling | A lead: their coverage is not measured yet |
| Forward-drawn surfaces | Sky, emissive and lit forward models, glass, ocean, rivers, particles, liquids | Not in the G-buffer; listed in the [roadmap](../ROADMAP.md#surfaces-the-game-draws-forward) and the [engine notes](render-engine.html#notes) |

## 3. Three lighting methods on one frame

One setting, **Lighting Method**, selects how Enhanced lighting is computed. The Vanilla and Enhanced switches for shadows and for Global Illumination keep their meaning under every method.

| | Raster (default) | Ray Traced | Path Traced |
|---|---|---|---|
| Needs | Any supported GPU | A ray-tracing GPU and the scene on D3D12 | The same, plus DLSS Ray Reconstruction |
| Direct light | The engine's lights; PCSS and contact shadows | The engine's lights; ray-traced shadows for every light | Sampled and shadowed by rays at every bounce |
| Diffuse indirect | Screen probes over scene memory | The same gather, tracing real geometry | Paths, ended in a radiance cache |
| Reflections | Screen-space rays, then scene memory, cube maps and sky | Rays below a roughness cutoff, probes above | Paths |
| Forward surfaces | Lit from the light field; water and glass trace the screen | The same, with ray queries | Traced as real surfaces |
| Denoising | Probe filters; NRD for reflections | NRD; Ray Reconstruction optional | Ray Reconstruction |

The frame has six places where our work attaches. They are the same for every method.

| Place in the frame | Engine boundary | Work |
|---|---|---|
| After the sun's shadow maps | Sun shadow pass ends | Scene memory update from the sun's view; fog volume |
| After the G-buffer | Geometry pass ends | Scene memory update from the camera; probe placement; tracing |
| Diffuse hand-off | Before the first skin scatter (today's GI position) | Replace ambient light with the gathered light |
| Reflection pass | The engine's reflection plugin (today's position) | Specular indirect light |
| Forward passes | Forward models and both primitive passes | Replaced shaders sample the light field and the fog image |
| Start of post-processing | `DgDx11::StartPostFilter` (today's reconstruction position) | DLSS, FSR or Ray Reconstruction; later, frame generation inputs after post-processing |

What the methods share: the material record and BSDF, the light list, the sky model, the light field interface that forward surfaces and fog read, the screen-probe gather (Raster and Ray Traced), the signal formats, and the place of each pass in the frame. What they do not share: path tracing has its own primary-hit handling, sampling, cache and denoiser, and is free to diverge wherever sharing would cost it quality.

## 4. Raster method

### 4.1 Why the current GI falls short

Today's Global Illumination gathers light only from what the current frame shows: one bounce of the diffuse light on visible surfaces, found by marching the depth buffer. A light source or a sunlit wall that leaves the screen stops lighting the scene, hidden surfaces neither block nor bounce, forward-drawn lamps contribute nothing, and nothing is remembered between frames. A better estimator on the same data (more rays, reservoirs, a stronger denoiser) cannot fix that, because the data is missing. The design below adds the missing data first.

### 4.2 Scene memory

A camera-centered, world-aligned set of nested grids (a clipmap) that remembers what has been seen. Starting values: five levels of 128×64×128 cells, 0.15 units at the finest, so the coarsest reaches about 150 units, the same range as the sun's shadow maps.

It holds three things:

- **Geometry**: a narrow-band distance field in which each cell is in one of three states: near a surface (with the distance), known empty, or unknown.
- **Albedo**: a coarser grid of surface color.
- **Lit surfaces**: a small hashed cache of outgoing light for surfaces that rays hit, keyed by position and facing direction.

It is filled from up to four sources:

1. **The camera, over time.** Each frame a few thousand random G-buffer pixels mark their surface into the distance field and blend their albedo and lit color into the caches. Random points along view rays carve away surfaces that the camera can now see through. Pixels the object-velocity image marks as moving are skipped, so characters and vehicles never enter the memory.
2. **The sun's shadow maps, every frame.** They answer "is this point sunlit?" exactly, the way the engine's own lighting uses them, for any point in their range, on or off screen. That is their main job here: a surface the camera saw once keeps being lit correctly by the sun after it leaves the screen and as the time of day changes, so the cache can store what the surface is instead of a stale lit color. They also add geometry, with one limit. The engine draws most shadow casters with their front faces culled (56 of 60 draws sampled in one capture), so a shadow-map depth is usually the far side of the first object along the sun's ray, not its sunlit face. Each texel therefore marks the shaded side of a caster as a surface: the underside of a roof, the far side of a wall. Those are the surfaces that block the sky from inside shadowed places, which the camera often cannot see. The space in front of that depth is either empty or inside the caster, so the sun's view may turn unknown cells into known-empty ones but never removes a surface the camera marked. Sunlit faces the camera has never seen stay unknown until it sees them.
3. **The engine's occluder polygons**, stamped in as solid walls at least one cell thick. They cover exactly the cases depth views miss: big walls and buildings the camera has not looked at yet, and thin walls. This source is used only if a coverage check shows the polygons are dense enough to matter.

4. **The terrain's height data**, if the engine's copy can be read: everything below the ground is solid, known before the camera looks at it. The ground is the largest bounce surface outdoors, and a height field cannot leak. This is a lead, not yet verified in the executable.

A camera cut or teleport clears the memory. Until it refills, everything is unknown, and the rules below make the image fall back to the game's own ambient light.

The memory is judged before anything is built on it: a debug view of the three states in normal play must show that outdoor scenes are known within seconds and that interiors fail toward dark, not toward leaks.

### 4.3 The trace and hit lighting

Every consumer (probes, reflections, water) calls one trace function:

1. **Screen segment.** A hierarchical march of the current depth buffer. A sure hit returns last frame's lit color at that point, if last frame's final depth agrees that the surface was visible there. An uncertain result (the ray passes behind something) is not settled by a thickness guess; it hands over to the world segment.
2. **World segment.** Sphere tracing through the distance field.
3. **Lighting a world hit**, in this order: last frame's lit color if the hit is on screen; otherwise albedo from the cache times the light arriving there, which is the sun (through its shadow maps), the local lights from the light list (through their own shadow maps where they have them), and the bounce light stored in the light field. A surface with no known albedo uses its direct light on a neutral albedo: darker than right, but it cannot leak.
4. **A miss.** A ray that leaves through known-empty space takes the sky. A ray that ends in unknown space takes the game's authored ambient light instead.

All cached light is stored free of exposure and in linear light.

### 4.4 The light field

A world-space grid of probes, three nested levels around the camera, each probe holding the light arriving from every direction (an 8×8 octahedral map). A fraction of the probes is updated each frame by tracing the world segment only. From it an irradiance grid (spherical harmonics per probe) is derived.

It answers three questions that the screen cannot:

- What a shortened screen-probe ray sees beyond its reach.
- How bright the bounce light is at a world hit (this feedback is what gives more than one bounce).
- How much light reaches a point that is not on a G-buffer surface: a particle, a pane of glass, the water surface, a cell of fog.

Probes inside solid cells are flagged invalid and left out of interpolation, lookups step back from the surface before sampling, and a soft depth test per probe limits light arriving through walls.

### 4.5 The final gather

Light for G-buffer pixels is gathered in two layers that split the hemisphere between them, so nothing is counted twice.

**Screen probes (the far layer).** One probe per 16×16 pixels placed on the depth buffer, with extra probes where depth changes sharply, each tracing an 8×8 octahedral set of directions through the trace function with a ray length capped near the light field's probe spacing. Directions are steered toward where last frame's probes found light and where the surface responds most. Probes are filtered against their neighbors in probe space, with hit distances limiting how far a value may spread, then accumulated over time with rejection by depth and normal instead of color clamping. Each probe is reduced to spherical harmonics and interpolated to pixels with plane weights. Each probe also carries how much of its hemisphere ended in unknown space.

**The near layer.** The existing visibility-bitmask pass, kept at full internal resolution but shortened to about one probe spacing. For each pixel it finds which directions are blocked nearby and what light the blockers send back.

**Putting them together.** For a pixel with normal `n`, irradiance is the sum of three terms: light from nearby blockers in the directions they cover; the interpolated probe light in the directions the near layer found open; and the game's authored ambient light, scaled by the share of open directions that the probes reported as unknown. When everything is known the authored ambient light is fully replaced. When nothing is known the result is the game's ambient light under our occlusion, which is what Global Illumination does to ambient light today.

The gather needs no per-pixel denoiser for diffuse light. That removes the stage most sensitive to the temporal upscaler.

### 4.6 Reflections

One chain, chosen per pixel by roughness (cutoffs are starting values in this engine's roughness scale):

- **Rough surfaces** reuse the screen probes: their octahedral maps are resampled with the GGX lobe. No reflection ray is traced.
- **Glossy surfaces** trace the screen as today (one GGX ray per pixel with neighbor reuse). A miss continues through the world segment and is lit from the caches.
- **Near-mirror surfaces** do the same, but where the world segment has no sharp answer they fall back to the area's cube map, corrected for parallax, dimmed by what the trace found blocking the ray, and scaled so its brightness matches the locally gathered light (the cube may only be darkened by this, never brightened). Then the sky.

Specular occlusion for the fallbacks comes from the near layer's open directions weighed against the reflection lobe. NRD stays the denoiser for the traced part.

### 4.7 Direct light

Enhanced shadows stay as they are. Two additions use scene memory: lights the engine leaves unshadowed gain a soft shadow from a short trace through the distance field, and bounce light from local lights reaches surfaces even when the lit wall is off screen, through hit lighting from the light list.

### 4.8 Forward-drawn surfaces

- **Every lit forward shader at once.** The engine gives each forward object an up color and a down color from one function. The irradiance grid is read back to the CPU every few frames and that function is overridden to return the grid's light at the object's position. Particles, smoke, splashes, forward models, glass, rivers and marsh then follow the scene's light with no shader replaced.
- **Large surfaces per pixel.** Ports of the glass, lit forward model, river and ocean shaders sample the irradiance grid per pixel and take reflections from the trace function.
- **Emissive forward models.** Lamps, signs and screens are drawn after lighting and are unlit. They enter the lit-surface cache through last frame's lit color, so they light their surroundings, except where an engine light sits at the same lamp: that light already does the job, and counting both would double it. The pairing is decided once per model from the light list and shown in a debug view.
- **Fakes.** The blob shadow under a character's feet is removed where contact shadows already cover it.
- Unlit particles, screen-space effects and the HUD keep the game's shaders.

### 4.9 Quality tiers

| Tier | Scene memory | Far light | Gather | Reflections |
|---|---|---|---|---|
| Low | Off | The game's authored ambient light | Today's long-reach bitmask pass | Screen rays, cube map |
| Medium | On | Irradiance grid sampled per pixel | Short bitmask pass | Screen rays, then scene memory |
| High (default) | On | Light field | Screen probes every 16 pixels plus the short bitmask pass | Full chain |
| Ultra | Finer and longer | Denser probes, updated faster | Screen probes every 8 pixels, more directions | Full chain at full resolution |

Estimated cost of High, from published numbers for comparable systems: 5 to 8 ms on a mid-range GPU at 1080p internal and 3 to 5 ms on an RTX 5080 at 1440p internal, with today's diffuse pass removed. Our own measurements replace these as each part lands.

### 4.10 Considered and not chosen

- **Per-pixel ReSTIR on screen rays** (reservoir resampling as the gather). It improves the estimator, not the information: its samples can only land on what the screen shows, reused samples cannot be checked properly without real rays, its output is correlated and still needs a denoiser, and its clear win is limited to a few small bright sources on screen. Reservoirs belong to the ray-traced and path-traced methods, where a visibility test is a real ray.
- **Radiance cascades** as the gather. Noise-free and fast in the one shipped screen-space use (a fixed camera), but every measured free-camera 3D prototype flickers under camera motion, and world-space intervals cost ten times the budget. The idea of splitting light into near and far intervals is kept in the gather above.
- **Voxel cone tracing.** Noise-free, but prefiltering mixes a thin wall with the air around it, so it leaks by construction, and it needs solid voxelization that depth views cannot give.
- **An irradiance volume alone** as the top tier. It is the Medium tier here. As the best result it looks flat and leaks, and the teams that shipped it moved on to probe gathers.
- **Light propagation volumes from the sun's shadow maps.** Low-frequency only, no secondary occlusion, bleeds through thin walls.

## 5. Atmosphere

One backbone feeds the sky, the fog and the water: per-frame atmosphere tables, a spherical-harmonic and environment-map projection of the sky, and a fog volume.

**Fog volume.** A view-aligned volume we compute ourselves, using the game's own depth slicing so its result copies straight into the engine's fog image. The medium starts as the same analytic height fog the game writes today, expressed as scattering and extinction, with a per-weather color, forward-scattering strength and wind-driven noise. It is lit by the sun through the shadow maps (which is what makes shafts), by ambient light from the sky, and by the local lights. The engine's fog image is enlarged at creation (for example to 128×128 cells per slice), which no consumer notices because they address it by fractions. Every forward shader and the deferred composition then show shadowed fog with nothing else changed.

**Sharp shafts on opaque surfaces.** Shafts from the volume are a few cells wide. For opaque pixels the composition shader replaces the volume's shadowed sun term with the unshadowed term times a per-pixel average sun visibility: 8 to 32 shadow-map samples along the view ray at half resolution, placed so each carries an equal share of the in-scattered light, accumulated over time. Forward surfaces keep the softer volume. Both integrate the same medium, so they differ in sharpness, not brightness.

**Physical sky.** The four tables of Hillaire's 2020 method (transmittance, multiple scattering, sky view, aerial perspective), computed every frame for the sun and the moon, driven by atmosphere parameters fitted to the game's baked time-of-day tables. The difference between the authored sky and the fitted sky is kept as an explicit, smooth correction with a strength control (**Authored Look**), so the game's colors survive by a stated choice and not by a hidden multiplier. Sun and moon color for surfaces, disks, clouds and fog all come from the transmittance table. The sky is projected to spherical harmonics and a small environment map each frame; these replace the hard-coded ocean colors and the static cubes as the reflection fallback, light the fog and the clouds, and later light every ray that misses.

**Clouds.** In order of ambition: relight the game's clouds from the same sun and sky; add a cloud shadow map marched from the game's own cloud density, read by sun lighting, fog and shafts; replace the cloud march only if its density function can be reproduced.

**Order.** The fog volume comes first, because it needs nothing from the physical sky and improves every surface at once. The fallback for the sky, if the fit is poor for a weather type, is to keep the baked tables and add only the shared projections.

## 6. Water

Rivers and ponds draw after the scene copy; the ocean draws before it and writes depth; all draw before or among sorted particles, so each water shader must output its final color itself. The ported shaders therefore do their own work in place:

- **Reflection.** A hierarchical screen trace inside the shader. Calm water follows the mirror direction bent by the ripple normals; rough water widens the fetch by roughness instead of adding noise, so no denoiser is needed. Rivers trace the engine's scene copy; the ocean, which has none, traces last frame's lit color. A miss takes the sky environment from the atmosphere stage (the game's cube until that exists), dimmed by scene memory along the ray. The sun's shadow never scales the reflection.
- **Fresnel and transmission.** Water's real Fresnel with roughness from wave slope variance, reflection weighted by F and the view through the water by 1 − F.
- **The water body.** Refraction offsets checked against depth, absorption by depth with one coefficient set per water type, and single scattering of sun and sky light in place of the fixed water color. The scene copy already contains fog down to the bed, so the port fogs only the surface terms.
- **Ambient light** from the irradiance grid.
- **Caustics.** The area change of sunlight refracted through the ripple normals, applied to sunlit pixels under water, blurred with depth and faded by absorption. It modulates light that reaches the bed and averages to one.
- **A surface record.** Each port also writes the water's normal, roughness, depth and type to a small side target. Nothing reads it in the raster method except reconstruction and frame generation (depth and motion for forward surfaces); the ray-traced and path-traced methods and Ray Reconstruction depend on it.

Wet surfaces are audited against a published wetness model before anything changes.

## 7. Frame generation

All three providers generate and pace frames on a D3D12 swap chain that their SDK creates or wraps; none has a D3D11 path today. The game presents through D3D11. So the real swap chain moves to our D3D12 device: each finished D3D11 frame crosses through a shared texture and a fence, the provider's swap chain presents it, and the provider inserts the generated frames. One provider is active per run.

There are two ways to get there, and the first experiment chooses between them:

- **Replace the game's swap chain.** The game is handed a stand-in swap chain whose back buffer is the shared texture. This is the published route for a D3D11 game, and it is known to work, but the project that ships it had to switch off ReShade's own hooks and drive ReShade by hand. For an add-on that lives inside ReShade and uses RenoDX's HDR swap-chain handling, that is a large change.
- **Leave the game's swap chain alone.** ReShade and RenoDX keep working on it unchanged; its finished image is copied across, its own presents are withheld, and the provider's swap chain shows the frames in a window placed over the game's. Less to rebuild, but window handling, input focus and pacing have to be proven.

Order of providers: FSR first, because it needs no latency SDK and a complete open example exists; then DLSS Frame Generation through Streamline on the D3D12 device, which requires Reflex markers and its sleep call in the engine's main loop; then XeSS Frame Generation, which requires Intel's own latency library and cannot run beside Reflex.

Inputs, produced once for every provider:

- **The image without the HUD**, after all post-processing, in the output's size and color space.
- **The HUD as its own image with alpha**, so the provider can draw it on real and generated frames alike. The engine's 2D plugins draw both the HUD and camera filters (lens drops, screen noise), so the split follows the engine's draw lists, not the shader used.
- **Depth and dense motion vectors.** Forward-drawn surfaces have no motion today: water and the ocean get it from their surface record; particles and glass rely on the provider's own optical flow.
- **Camera data** latched once per frame at a known engine point: unjittered matrices, jitter, near and far planes, a reset flag on camera cuts, and a frame number that rises by exactly one.

Conditions and limits: HDR output must be HDR10 (two providers refuse scRGB), which is what the add-on already uses; borderless only, which the add-on already forces; generation is off in menus, pause and loading; every history length in the lighting uses the real frame rate, not the presented one.

Known traps from the published implementation: stale references and newer swap-chain interfaces on a stand-in swap chain, resizing with a buffer count of zero, a camera latched from the wrong pass (which shows as stutter), and several rounds of pacing fixes.

## 8. The scene on D3D12

Ray tracing needs the scene outside the camera's view. The work, in order:

1. **Static geometry and instances** from the engine's model resources and scene object lists, not from its per-view visible lists, which are already culled to the camera. Each mesh's role (opaque, masked, forward, shadow-only stand-in) comes from the same per-mesh flags the engine uses to route it to a pass.
2. **Terrain and vegetation** from the engine's own height data and placement lists.
3. **Skinned characters**: the bone matrices that the frame's own draws upload, applied by a compute pass that also keeps last frame's positions for motion; acceleration structures refit when a skeleton moves and rebuilt on a schedule. Poses are not read from engine memory when the trace runs, because the engine is already recording the next frame by then.
4. **Materials at a hit**: a simplified record (base color, roughness, emission) from the same textures the G-buffer uses, which is standard practice for secondary hits.
5. **The forward set**: glass, water, emissive models and other blended meshes, flagged so they can be traced apart from opaque geometry.
6. **Foliage and other alpha-tested geometry**: opacity micromaps where the GPU supports them, alpha tests in the trace elsewhere. Alpha testing is expected to be the main cost.
7. **Lights**: the engine list, including the authored boxes that confine some lights, plus emissive triangles for models with no engine light.
8. **A top-level structure rebuilt every frame** within a measured budget, and a memory budget taken from the GPU's reported free memory, not fixed pools.

The first deliverable is a debug view that traces primary rays and compares depth and material against the G-buffer. Ray origins for G-buffer pixels are placed on the traced triangle under the pixel where one is found, because plain normal offsets are reported to leak at this game's scale.

## 9. Ray-traced method

The raster method with real rays wherever a ray helps. It runs on any ray-tracing GPU and denoises with NRD.

- **Shadows.** The sun: one ray per pixel toward its disk, denoised. Local lights: every light can cast a shadow; one light is chosen per pixel by resampling over a world-space light grid and tested with one ray, with RTXDI's ReSTIR direct lighting as the higher setting. The engine's unshadowed lighting math is kept, so only visibility changes.
- **Diffuse indirect.** The same screen-probe gather; the world segment of the trace becomes a ray query against the real scene, and hits are lit from SHaRC, a world-space radiance cache, in place of scene memory. The near layer stays, because not every small detail is in the acceleration structure.
- **Reflections.** One GGX ray per pixel below a roughness cutoff; screen first, then a ray query; hits lit by the sun and the light grid with shadow rays plus the cache; probes above the cutoff.
- **Forward surfaces.** Water and glass trace their reflection and refraction from the surface record. Particles read the light field, now filled by rays.
- **Fog.** Sun and local-light visibility in the volume from rays: one ray per cell per frame, so every local light can cast shafts.

## 10. Path-traced method

The flagship. It requires DLSS Ray Reconstruction, which denoises and upscales in the slot reconstruction uses today, before depth of field.

- **Primary surfaces.** Opaque pixels start from the game's G-buffer, which keeps the game's exact materials, decals, animation and motion vectors. Everything in front of it is traced: a camera-ray segment from the eye to the G-buffer depth against the forward set, with hits treated as real path vertices that reflect and transmit, in up to three layers per pixel. A debug mode traces true primary rays and compares them with the G-buffer. This is a choice for primary-surface fidelity; fully traced primary rays remain possible per material class.
- **Direct light.** The sun and moon: one shadow ray toward the disk at every path vertex. Local lights: a world-space light grid feeding resampling, with ReSTIR direct lighting at the primary vertex. The sky: the physical sky's environment map, importance sampled with the sun kept out of it.
- **Emissive surfaces** are in the acceleration structure so rays find them, and models with no engine light become sampled emitters. A model paired with an engine light stays visible to the camera and in reflections but is not counted again as a light.
- **Indirect light.** One path per pixel, the lobe chosen by chance, ended early in SHaRC after the first diffuse-like bounce. The first hit is always lit fresh; the cache only ends a path, so no two histories are stacked. Turning the cache off must stay possible: that is the correctness reference.
- **Resampling.** ReSTIR for indirect light is off by default, because Ray Reconstruction assumes independent samples; ReSTIR GI is a middle option and ReSTIR PT an experimental top option.
- **Transparency.** Glass and water are traced layers. Particles stay rasterized, lit from a volume the tracer fills, and reach Ray Reconstruction through its transparency inputs. Fog stays a smooth, already-converged signal delivered the same way.
- **Caustics** come after the method is stable. Path tracing at one sample per pixel cannot find sunlight through water, so sun caustics are injected from a sun-view caustic map, the same data the raster water uses, upgraded to traced rays. Glass caustics are a later, smaller item.
- **Other vendors.** The tracer keeps its outputs split (direct and indirect, diffuse and specular, with hit distances), so NRD or another vendor's denoiser can be tried later without changing the tracer. Neither is a goal.

## 11. Settings

Principles: presets are the front door; each feature has one mode switch and at most one quality tier; raw algorithm values (pixel reaches, thicknesses, sample counts) are not player settings, because they give resolution-dependent results and nobody asks for them; strengths stop at 100%; a control that cannot apply is disabled with a note saying why; each feature's Advanced section shows its measured GPU time. Existing saved keys keep their meaning. `LightingMethod`, `LightingQuality` and the `PathTracing` keys cover direct and indirect lighting together, so the key-prefix rule in the mod's `AGENTS.md` gains a stated exception when they are added.

**Lighting**

| Control | Key | Values | Default |
|---|---|---|---|
| Lighting Method | `LightingMethod` | Raster, Ray Traced, Path Traced | Raster |
| Lighting Quality | `LightingQuality` | Low, Medium, High, Ultra | High |
| Shadows | `DirectLightingShadowsMode` (existing) | Vanilla, Enhanced | Enhanced |
| Global Illumination | `IndirectLightingMode` (existing) | Vanilla, Enhanced | Enhanced |

Lighting Quality maps to the tier table of the selected method. Under Path Traced the two mode switches are disabled, with a note that path tracing replaces both.

**Path Tracing** (enabled under the Path Traced method)

| Control | Key | Values | Default |
|---|---|---|---|
| Preset | `PathTracingPreset` | Performance, Balanced, Quality, Ultra, Custom | Balanced |
| Bounces | `PathTracingBounces` | 1 to 4 diffuse or glossy bounces traced before the cache answers | 2 |
| Mirror and Glass Depth | `PathTracingSpecularDepth` | 2 to 16 mirror or glass interactions per path | 8 |
| Radiance Cache | `PathTracingRadianceCache` | Off, On, Directional | On |
| Light Sampling | `PathTracingLightSampling` | Fast, Balanced, Unbiased | Balanced |
| Indirect Resampling | `PathTracingIndirectResampling` | Off, ReSTIR GI, ReSTIR PT (experimental) | Off |
| Paths per Pixel | `PathTracingPathsPerPixel` | 0.75, 1, 2 | 1 |
| Reference Accumulation | `PathTracingReference` | Off, or 64 to 4096 frames while the camera is still | Off |

The preset sets the rest; changing any of them selects Custom. Reference Accumulation is the ground truth the other methods are checked against.

**Direct Lighting (Advanced)** keeps Shadow Softness and the debug views, and gains **Local Light Shadows** (`DirectLightingLocalShadows`: Game's Own, All Lights; All Lights needs scene memory or ray tracing).

**Indirect Lighting (Advanced)** keeps the four strength controls and the debug views. The diffuse resolution, rays per pixel, search radius, thickness, offset, direction and sample controls are retired when the new gather replaces the pass they tune; their work moves into Lighting Quality.

**Atmosphere**

| Control | Key | Values | Default |
|---|---|---|---|
| Fog | `AtmosphereFogMode` | Vanilla, Volumetric | Volumetric |
| Light Shafts | `AtmosphereLightShafts` | Off, Soft, Sharp | Sharp |
| Fog Quality | `AtmosphereFogQuality` | Medium, High, Ultra | High |
| Sky | `AtmosphereSkyMode` | Vanilla, Physical | Physical |
| Authored Look | `AtmosphereSkyAuthoredLook` | 0 to 100% | 100% |
| Cloud Shadows | `AtmosphereCloudShadows` | Off, On | On |

**Water**

| Control | Key | Values | Default |
|---|---|---|---|
| Water | `WaterMode` | Vanilla, Enhanced | Enhanced |
| Water Reflections | `WaterReflections` | Sky Only, Scene | Scene |
| Caustics | `WaterCaustics` | Off, On | On |

Under the Ray Traced and Path Traced methods, Scene reflections use rays.

**Materials**: Diffuse Model (`MaterialDiffuseModel`: Vanilla, EON).

**Reconstruction and frame generation**

| Control | Key | Values | Default |
|---|---|---|---|
| Temporal Reconstruction Mode | `FxTemporalReconstructionMode` (existing) | Off, Analytical TAA, FSR, DLSS, and DLSS Ray Reconstruction (new) | unchanged |
| Upscaling Quality, DLSS Model | existing keys | unchanged | unchanged |
| Frame Generation | `FrameGenerationMode` | Off, DLSS, FSR, XeSS | Off |
| Generated Frames | `FrameGenerationFrames` | 2x, 3x, 4x where the provider and GPU allow | 2x |
| Low Latency | `FrameGenerationLowLatency` | Off, On, On + Boost (Reflex with DLSS, Intel's library with XeSS, a frame limiter with FSR) | On with frame generation |

DLSS Ray Reconstruction is selectable under the Ray Traced method and forced under Path Traced.

**Stated dependencies.** Global Illumination needs the RenoDX tone mapper (today's rule). Ray Traced and Path Traced need a ray-tracing GPU. Path Traced needs an RTX GPU for Ray Reconstruction. Frame generation needs the D3D12 presentation path; DLSS Frame Generation needs an RTX 40-series GPU or newer.

**Debug views** live in the Advanced sections: scene memory states, albedo and lit caches, probe placement and rays, the unknown share, the light field, emitter pairing, fog slices and sun visibility, water surface record, and for the traced methods the acceleration structure against the G-buffer.

## 12. Validation

- **Uniform light.** In a scene lit by one constant color every integrator must return that color: the gather, the reflection chain, the forward lighting function, the fog volume, and each traced method.
- **Against the reference.** Once path tracing exists, its accumulated reference is the target the raster and ray-traced methods are compared with, scene by scene.
- **Turn-around test.** With the camera turning away from a sunlit wall or a lamp, the bounce light must stay. This is the test screen-space GI fails.
- **Leak tests.** A dark interior beside a bright exterior, entered for the first time and after looking around.
- **Fixed scenes for cost.** The ACC title, the Afghanistan hillside, Mother Base by day, at night and in rain, measured at a fixed camera with frame time and GPU time separated.

## 13. Surviving a game update

The game still receives small updates: the executable the mod supports was built on 2026-07-07. What the mod relies on, and what an update does to each:

| The mod relies on | Today | Breaks when |
|---|---|---|
| Code addresses: functions, call sites, return addresses | About 60, in seven files; half are unnamed numbers | The executable is rebuilt at all: everything after a changed function shifts |
| Structure field offsets | About 70, in five files; most are unnamed numbers | The engine's structures change, which a maintenance update rarely does |
| Global and virtual-table addresses | A few | The executable is rebuilt |
| Shader hashes | Every ported shader, in its file name | The shader archives are rebuilt |
| Name hashes of render targets and plugins | Dozens | Never, short of a rename |

Today each hook file checks the executable for itself: its build time and size first, then the first bytes of each function it hooks. That is safe, but it is repeated in five places, any rebuilt executable turns off every engine-side feature at once, and after an update the numbers to fix are scattered through feature code. One site, the projection commit, already searches the game's code for its bytes at startup, skipping the call target inside them, so the approach below is proven here.

The goal: a small maintenance update needs no new mod build at all, and when something does change, only the affected feature turns off, with a message that says which engine function it could not find.

**1. One table describes everything tied to the executable.** `engine/build.hpp` lists every entry under the engine's own name. An entry is one of:

- a function, found by a byte pattern;
- a place inside a function (a call site, a return address, an instruction to read), found by a pattern searched only inside that function;
- a value taken out of an instruction: the address of a global or a virtual table from an instruction that refers to it, a call's target, or a structure field offset from an instruction that uses it (used to verify the compile-time offset, not to replace it).

Each entry also lists its checks against other entries (below) and, for each known build, the address where it was found. No other file contains an address, an offset or a byte pattern. Feature code asks the table for entries by name.

**2. Patterns are written so they survive a rebuild.** Every byte that changes when code moves is a wildcard: call and jump targets, references to global data, and constants that are themselves addresses. The bytes that encode what the code does stay fixed, including the structure offsets it uses (unless the entry exists to read that offset). A pattern for a function starts at the function's first byte and runs into its body until it is unique; prologues alone are not, since 18 of the 36 byte checks the mod has today appear in several places in the game's code, some in more than two thousand. Patterns are generated by the authoring tool (point 7), not written by hand.

**3. Resolution at startup, in order.**

1. **Known build:** if the build's identity (time stamp, image size and a hash of the code section) matches a build in the table, every entry is taken from its stored address and its pattern is checked there, wildcards included.
2. **Unknown build, cached:** if a previous run already resolved this exact build, the result saved next to the add-on is loaded and checked the same way.
3. **Unknown build, searched:** every function pattern is searched in the game's code. A match counts only if it is the only match, it starts exactly at a function's start in the executable's own function table (x64 executables list the start and end of every function; this one lists 141,291), and every check that involves it passes. Places inside a function are searched only between that function's start and end, so they cannot match anywhere else. Values are decoded from the matched instructions. A complete result is saved for the next start.

Nothing is installed until resolution is finished. The search reads the code once and costs a fraction of a second; it runs only on a build the table does not know.

**Cost while playing: none.** Resolution happens once, before any hook is installed. Afterwards a hook is the same detour it is today, and a resolved address is a plain pointer read once. Structure field offsets stay compile-time constants, so feature code compiles to exactly the same field reads as now; on an unknown build the offsets decoded from the game's instructions are only compared with those constants, and a mismatch (a changed structure, which a maintenance update rarely makes) turns the feature off instead of feeding a runtime offset into code that runs every draw. At startup the known-build path costs a hash of the code section (151 MB, tens of milliseconds) and about a hundred short byte comparisons.

**4. Entries check each other.** A match is accepted only when its relations hold: a call site must call the function the table says it calls; a return address must follow a call to its function; a global decoded in two places must decode to the same address; a virtual table must sit in read-only data and its slots must point at the functions the table names; a function must be called from the places the table says call it. These checks are what make a wrong but unique match very unlikely, and they cost nothing at run time.

**5. Features fail one at a time.** A feature declares the entries it needs. If any of them fails, the feature does not install, turns itself off, and the menu says which engine function was not found. Everything else keeps running. HDR output, tone mapping, color grading and shader replacements need no entries at all.

**6. One log block per start** lists the build, how it was resolved (known, cached or searched) and each entry that failed with the reason (no match, several matches, a check that failed). After an update, that block is the to-do list.

**7. An authoring and checking tool**, kept with the local analysis tools. It decodes instructions (so wildcards fall exactly on moving fields), writes the shortest unique pattern for each entry, regenerates the table, and runs the startup resolution offline against any executable. It is used three ways: when a new entry is added; as a check that every pattern is unique and every relation holds on the current build; and, when the game updates, run against the new executable before players report anything. Whatever it cannot place exactly once it lists for a person to look at in the decompile database; the fix is a new pattern in the table, and the stored address of the new build is added so the next release takes the fast path.

**8. The table keeps old builds.** Stored addresses for each known build stay, so the mod works for a player who has not taken the game's update.

**Shaders.** A ported shader is keyed by the hash of its bytecode. A rebuilt shader archive changes every hash even when nothing in a shader changed. The authoring tool dumps the new shaders, matches each to the old one by the engine source name in its debug information and by its instructions, and renames the ports whose instructions are unchanged; shaders that really changed are listed for review. At startup, any port whose hash the game never loads is listed in the same log block as failed entries.

**Gates** for this work: with the current executable treated as unknown, the search must find exactly the stored address for every entry; with one pattern deliberately broken, only the feature that needs it turns off; and the authoring tool must report every pattern unique and every relation holding.

## 14. Folder layout

Rules: one folder knows the executable, and features never hold addresses; each feature has a folder named for what it does, and a port of an engine shader sits with the feature it belongs to; a folder appears with its first file; a move keeps shader file names and embed identities.

| Folder | Holds | Change |
|---|---|---|
| `engine/` | The executable table and its check, hook installation, typed views of engine structures, the engine's render targets, lights, camera, motion vectors and pass boundaries | New. Collects the engine-facing headers now in `rendering/`, `reconstruction/` and `lighting/shadows/` |
| `rendering/` | Our own shared infrastructure: the D3D12 bridge, submission, timing, saved state | Loses its engine-facing headers; gains submission from `reconstruction/coordinator.hpp` |
| `materials/` | The material record, the BSDF, ports of material shaders | Unchanged |
| `lighting/native/`, `lighting/compose/` | Ports of the engine's light and composition shaders | Unchanged |
| `lighting/shadows/` | Shadow filtering and contact shadows | Its engine hook moves to `engine/` |
| `lighting/memory/` | Scene memory | New with stage 2 |
| `lighting/field/` | The light field and irradiance grid | New with stage 2 |
| `lighting/gather/` | Screen probes and the near layer | Today's `lighting/indirect/`, renamed when the new gather replaces it |
| `lighting/reflections/` | The reflection chain | Unchanged |
| `lighting/forward/` | Lighting for forward-drawn surfaces | New with stage 2 |
| `atmosphere/` | The fog volume, shafts, sky and clouds | New with stage 3; today's `sky/` moves under it |
| `water/` | River, pond and ocean ports, caustics, the surface record | Grows in stage 4 |
| `reconstruction/` | DLSS, FSR, Analytical TAA, later Ray Reconstruction; output-size post-processing | Unchanged |
| `presentation/` | Frame generation, the D3D12 swap chain, display encoding | New with stage 5; today's `output/` moves under it |
| `raytracing/` | The scene on D3D12: geometry, skinning, acceleration structures, materials at a hit | New with stage 6 |
| `lighting/traced/`, `lighting/paths/` | The ray-traced method and the path tracer | New with stages 7 and 8 |
| `postprocess/`, `tonemap/`, `effects/`, `copy/`, `antialiasing/`, `UI/` | As today | Unchanged |

`addon.cpp`, `settings.hpp`, `shaders.hpp` and `shared.h` stay at the root as the place where features are assembled. Moves happen with the stage that touches the code, not all at once; only `engine/` comes first, because every later stage adds hooks. The folder rules in the mod's `AGENTS.md` are updated in the same change as each move.

## 15. Open questions

- The occluder polygons: how many there are, where, and whether they sit inside the visible walls.
- Where the engine keeps terrain height data and whether it can be read each frame.
- Whether every shadow cascade is redrawn every frame, and what is drawn at night and indoors.
- Whether the authored probes change with time of day and weather.
- Where the raster world data should live. Its inputs (shadow-map atlas, G-buffer) are D3D11 textures and its forward consumers are D3D11 shaders, while reflections and NRD run on D3D12. The options are to compute it on D3D11, or to have the engine create the needed targets as shareable so D3D12 reads them without copies. A measurement decides.
- The world bounds of forward-drawn emissive models, needed to pair them with engine lights, and how bright those models are next to the engine's lights. Their shaders output an authored color times the exposure, so their values may not be on the same scale as light intensities.
- Whether local-light shadow maps also store the far side of their casters.
- Whether the fog image's enlarged size holds for every consumer, including non-power-of-two tiles.
- Each SDK's license before its source enters the repository (RTXDI, SHaRC and NRD share NVIDIA's RTX SDKs license, which forbids use that would place the SDK under an open-source license).
- The DLSS SDK in the repository predates Ray Reconstruction's current default model; updating it is a submodule change that needs approval.

## 16. How each claim is known

- **Read in the game's shaders, the executable or captures during this work:** the fog image's size, layout, channels and writer; the forward shaders' light inputs and the function that sets them; where each reflection cube comes from; the pass order of the ocean, rivers and particles; the shadow-map sizes and reach (earlier shadow work); that most sun shadow casters are drawn with front faces culled (60 of 407 shadow draws sampled in the sortie menu capture: 56 cull front faces, 4 do not cull); that an occluder is a list of vertices.
- **Read in primary sources by research readers and spot-checked:** the Dagor engine's open-source GI fills a distance field from G-buffer depth by marking and carving (its two shaders were read directly); SHaRC's update and termination rules and RTXDI's contents (their documents were read directly); the layout of 007 First Light's GI inputs, a shipped game that pairs a short-range screen-space pass with a world probe clipmap (resource names read in its shaders).
- **Read in an open implementation and spot-checked:** frame generation for a D3D11 game through a D3D12 swap chain created by the FSR SDK for the game's window, with a shared fence and shared textures (Skyrim Community Shaders).
- **Reported by research readers from primary sources, not re-read:** that no provider has a D3D11 path today and the other frame generation requirements; Lumen's screen-probe gather and its costs; published costs of froxel fog, Hillaire's atmosphere, screen-space reflections and NRD; the Ray Reconstruction guide's input rules beyond those read for the reconstruction work; the settings shipped path-traced games expose; real-time caustics methods and their costs.
- **Design judgment, not yet tested:** using the sun's shadow maps to mark shaded surfaces and known-empty space in scene memory (no published system was found that does this); the three-state rule and the authored-ambient fallback; the split between the near layer and the probes; pairing emissive models with engine lights; overriding the forward ambient colors from a read-back grid; enlarging the fog image; every cost estimate for our hardware.
- **Leads not checked by us:** the scene-capture cautions in section 8 (poses a frame ahead when read from memory, ray origins leaking with normal offsets, alpha testing as the main cost).
- **Counted in the mod's source and the executable:** the numbers of addresses, offsets and checks in section 13; the executable's build date from the timestamp the mod checks; how many places in the game's code each of the mod's 36 byte checks matches (17 once, 18 several times, 1 only with its call target skipped); the 141,291 entries in the executable's function table.
- **Still to be established:** everything in section 15, and which frame generation presentation path works inside ReShade.
