# MGSV planned rendering design

This page describes how the planned lighting, atmosphere, water, frame generation and settings fit together, how the mod should survive game updates, how its folders should be laid out, and why each part was chosen. The [roadmap](../ROADMAP.md) says what is built in which order and what each stage must show. The [architecture guide](ARCHITECTURE.md) describes what exists today. Nothing here is built unless a feature guide says so. The last section says how each claim on this page is known.

## 1. Rules

- **Path tracing quality is not reduced for the other methods.** Path tracing is designed for the best result. The other methods may reuse its parts, never the reverse.
- **The raster method is the default, and it stays affordable.** It needs a D3D12 GPU, as reconstruction already does, but no ray-tracing hardware, and it does not wait for the scene capture the traced methods need. It may trace in software where that is cheap. At its default tier it costs at most about 1 ms of GPU time more than today's Global Illumination; anything more expensive belongs to its Ultra tier or to the traced methods.
- **Information before sampling.** The weakness of screen-space lighting is what it cannot see (off-screen light, hidden surfaces, thickness), not how it samples. Knowing more of the scene takes rays against it, so that work belongs to the traced methods; the raster method keeps the game's authored light wherever the screen cannot show what is there (section 4.1).
- **Each direction counted once.** Linear light, energy conserved, no strength above 100%. Where our lighting has information it replaces the game's ambient light; where it has none it leaves the game's ambient light in place. It never adds one on top of the other.
- **Unknown is dark or authored, never bright.** A ray that ends somewhere we know nothing about takes the game's authored light for that place, not the sky.
- **Stored light is recomputed where it can be.** Caches keep what a surface is (albedo), and light it again from the sun's shadow maps, the light list and the cached bounce light. Stored lit color is used only where nothing better exists.
- **Shaders say what, the engine says when.** A shader's CRC32 identifies which shader is running, and since the game's shaders never change, those hashes are permanent names. A shader that many draws share (copies, 2D draws) also needs the draw's role, taken from the targets it binds. Where a pass starts or ends is taken from the engine's functions, because a shader that marks a boundary may not draw at all in some settings (bloom off, for example).
- **Every feature has a Vanilla setting**, and a dependency on another setting is shown in the menu, never silent.
- **Measured, not assumed.** Costs on this page are estimates from published numbers until our measurements replace them.

## 2. What the engine provides

| Input | What it holds | Notes |
|---|---|---|
| G-buffer and depth | Albedo, view-space normal, roughness, specular, material id, depth | Opaque and masked surfaces only. Forward-drawn surfaces use the scene's depth-stencil, not this depth |
| Motion | Composed velocity and a separate object velocity image | Camera motion is computed analytically. The object image marks every visible model whose world matrix the engine set since the last frame: characters (standing ones too), the player, animals and vehicles every frame, doors and physics props only while they move; never buildings, terrain, instanced objects or vertex-shader wind sway |
| Lit scene color | The scene at the start of post-processing, and the scene as it stands at the engine's reflection pass, which today's reflections read | Both include the sky, forward-drawn models and camera fog; at the reflection pass there are no reflections, water or particles yet. The engine keeps neither for the next frame; where the design reads last frame's image, the mod keeps a copy of the reflection-pass image and of the G-buffer depth (two full-resolution copies per frame) |
| Sun shadow maps | Four 4096×4096 cascades in one 8192×8192 atlas; the largest spanned about 123 native units in the frames inspected (native units are not yet calibrated to meters) | A depth view from the sun for points inside the cascades. Each cascade is a sphere around its slice of the view frustum, and casters are culled to a prism along the sun around the whole view frustum, so the cascades cover the view and little else: cascade 0 reaches about 2.5 units behind the camera, cascades 2 and 3 not at all. The moon drives the same cascades at night; in the gap between sun and moon there are none. The whole atlas is cleared and redrawn every frame. Casters are drawn with front faces culled unless the mesh is double-sided, so it holds the sun-away side of the first closed caster |
| Local lights | Point and spot lights read at the engine's local-light pass; spot shadow maps and dual-paraboloid point shadow maps for the lights that have them | Many lights are unshadowed. The list holds only the lights the engine drew this frame, culled to the view. Shadowed lights of one type share one shadow buffer, cleared and redrawn before each light, so after the pass only the last light's map is left |
| Authored light probes | Spherical-harmonic volumes (the engine draws at most 63 per view by default) plus the sky's spherical harmonics, drawn as ambient light | The game's indirect light. Most volumes are baked; the dynamic ones are drawn with occlusion shaders. How well they avoid leaks has not been measured |
| Area reflection cube | One authored cube map per area, chosen by area settings, trigger volumes and the weather script | Does not reflect the live scene |
| Fog image | 512×256 float image, 128 depth slices of 16×64 cells; RGB is in-scattered light, alpha is transmittance | Drawn once at the start of the frame (`GLOBAL_VOLUMETRIC_FOG`). Sampled by deferred composition and most forward shaders; particles without a volumetric-fog variant, the scattering effect and the sky apply fog differently or not at all. No shadow input |
| Terrain height | A 128 × 128 tile map and a 16-bit 1024 × 2048 height atlas, bound to every terrain draw: full resolution (2 units apart) for the loaded tiles nearest the camera, about 640 units around the player, and 1/16 resolution for the rest of the map | The same data the engine keeps in CPU memory, where a job rewrites it each frame without a lock. Decoded as the terrain vertex shaders do; on the Afghanistan hillside capture no G-buffer pixel lies more than 0.5 units below it and ground pixels within centimeters. Africa's map is 4096×4096 samples 2 m apart, heights −32 to 234 m |
| Occluders | Flat polygons of at most seven corners placed along walls and cliffs; the engine keeps about six per view, for culling on the CPU | Too few and too thin to add geometry; not used |
| Forward-drawn surfaces | Sky, emissive and lit forward models, glass, ocean, rivers, particles, liquids | Not in the G-buffer; listed in the [roadmap](../ROADMAP.md#surfaces-the-game-draws-forward) and the [engine notes](render-engine.html#notes) |

## 3. Three lighting methods on one frame

One setting, **Lighting Method**, selects how Enhanced lighting is computed. The Vanilla and Enhanced switches for shadows and for Global Illumination keep their meaning under every method.

| | Raster (default) | Ray Traced | Path Traced |
|---|---|---|---|
| Needs | Any supported GPU | A ray-tracing GPU and the scene on D3D12 | The same, plus DLSS Ray Reconstruction |
| Direct light | The engine's lights; PCSS and contact shadows | The engine's lights; ray-traced shadows for every light | Sampled and shadowed by rays at every bounce |
| Diffuse indirect | The screen-space bitmask gather with Multiple Bounces; directions it finds open keep the game's ambient light | Screen probes whose rays are real ray queries, over the bitmask gather | Paths, ended in a radiance cache |
| Reflections | Screen-space rays, then cube maps and sky | Rays below a roughness cutoff, probes above | Paths |
| Forward surfaces | The game's lighting; water ports trace the screen | Lit from a volume the radiance cache fills; water and glass trace with ray queries | Traced as real surfaces |
| Denoising | None needed for diffuse; NRD for reflections | NRD; Ray Reconstruction optional | Ray Reconstruction |

### 3.1 What runs on D3D11 and what runs on D3D12

The game keeps rendering on D3D11. Replacements of the game's shaders inside its draws (light, composition, forward, water and post-processing ports) stay on D3D11, because those draws belong to the game. Every other pass of ours is placed by what it gains on D3D12 against what it costs to get there.

**What D3D12 gives a pass:**

- Features D3D11 lacks: ray queries; the SDKs (DLSS, FSR, Ray Reconstruction, NRD, frame generation).
- Faster shaders through Shader Model 6: wave operations for reductions, prefix sums, histograms, bit masks and neighbor exchange without shared memory; native 16-bit math; 64-bit atomics for packing depth with a payload when splatting into grids; bindless access to every texture and buffer, which suits the radiance cache and materials at ray hits.
- Faster submission: GPU-driven dispatch counts and per-dispatch constants with `ExecuteIndirect`, so adaptive work (extra probes, tiles that need rays) costs only what it does; and explicit barriers instead of the driver's conservative ones between dependent dispatches. D3D12 work can also be recorded ahead of time on another thread, which would take CPU work off the thread that submits the frame. At the reconstruction point it was not built: the recording it would move took 0.16 to 0.19 ms for DLSS at the title, and the larger CPU cost there, the D3D11 signal, cannot move. It is used only where a measurement shows it pays.
- A compute queue that may overlap the game's graphics work (measured before relied on, see below).

**The rule:**

1. A pass that needs a D3D12-only feature runs on D3D12.
2. A pass at a place where a D3D12 block already runs joins the block and is written for D3D12, so it gets the speed features above at no extra switch.
3. A pass at a place with no block stays on D3D11, unless a measurement shows its D3D12 version saves more than the round trip it would add.
4. Our passes that only exist to hand engine images to D3D12, by copying or converting them on D3D11, are removed once the engine creates those images shareable; D3D12 then reads the engine's images directly.

Applied to the D3D11 compute work the mod has today:

| Pass (today on D3D11) | Plan |
|---|---|
| Reconstruction input preparation (`prepare_reconstruction`) | Into the reconstruction block on D3D12, reading the engine's shareable images |
| Reflection input preparation (`prepare_reflections`) | Split (rule 3 below): what it takes from the G-buffer moves into the main block; what it takes from deferred composition (the replaceable environment light, the receivers' fog transmission, which pixels receive a reflection) stays at the reflection pass on D3D11 |
| Diffuse AO/GI trace, depth mips and application (`gtvbgi*`) | The trace moves to D3D12 only if the overlap test in section 4.2 shows a gain; the application stays on D3D11 |
| Output-size post-processing helpers (`post_*`) | Stay on D3D11. Most run inside the game's post-processing chain, where no block runs. Depth alignment, output-size depth and the stencil draws (the last two are pixel shaders) run at the reconstruction cut, before its hand-off; moving them after the hand-off to overlap the evaluation gained nothing, because the GPU ran the two APIs' work one after the other |
| Contact shadows (`bend_*`) | Stay on D3D11 in the raster method: they feed the game's light draws, before the main block; replaced by ray-traced shadows in the traced methods |

**What a switch costs.** A switch is a round trip: D3D11 hands work to D3D12 and then waits for its result before it can continue. At the reconstruction hand-off the D3D11 timeline sat idle 0.16 to 0.36 ms longer than the D3D12 work took in clear scenes (the title, the sortie menu, the hillside, Mother Base by day), and 0.57 to 1.03 ms longer at Mother Base at night and in rain and in the sandstorm at night, which are CPU-bound. A switch also costs CPU time on the thread that submits the frame: the D3D11 signal that starts it makes the driver submit everything queued so far, 0.19 ms when an earlier hand-off in the frame had already done so and 1.03 ms when it was the first. Work on D3D12 that D3D11 never waits for is not a switch: it adds no idle time, though the work itself still takes GPU time.

**How switches are kept down:**

1. **One block per place, one round trip per block.** Everything D3D12 does at a place is recorded into one submission with one signal and one wait.
2. **Work with no D3D11 input runs independently.** Updating the scene on D3D12 for ray tracing (skinning, acceleration structures) takes its inputs from the CPU (bone matrices captured when the game uploads them, the light list) or from D3D12's results. It is submitted as soon as its inputs exist, D3D11 never waits for it, and the block that needs it waits on D3D12's side, which costs nothing.
3. **Last frame's image instead of a second block.** Reflections need lit surfaces to reflect, and they need what deferred composition decides about each receiver: its replaceable environment light, its fog transmission, and whether it receives a reflection at all. Composition runs after the main block. So reflections are split instead of getting a second block at the engine's reflection pass. The rays and their denoising run in the main block, for the pixels the G-buffer marks as possible receivers, and read last frame's lit scene: a copy the mod keeps of the image today's reflections read at the reflection pass (sky, forward models and camera fog, but no reflections, water or particles), together with last frame's depth. The engine's reflection pass then applies composition's receiver data to a finished texture, a D3D11 draw. Because the copy is taken before the reflection pass, reflections never reflect their previous result, as today. What it costs: one frame of delay in what moving objects reflect, two full-resolution copies per frame, and rays for some pixels that composition then rejects. Whether the G-buffer marks receivers and their roughness well enough is checked first; the roadmap gates the change on that and on the delay, and if either fails, reflections keep a separate block at the reflection pass and the frame makes three round trips.
4. **Whole passes move, not pieces.** In the traced methods the engine's lighting draws are replaced by one D3D12 pass in the main block, instead of a switch for each light draw.
5. **Overlap is measured, not assumed.** Independent D3D12 work may run on a compute queue beside the game's graphics work. A test at the reconstruction hand-off, on D3D12's direct queue, found the two APIs' work running one after the other on this GPU; a compute queue has not been tried. Overlap is kept only where a measurement shows a gain.

**The main block** runs at the diffuse hand-off, before the first skin scatter. Everything it needs is complete there: the G-buffer, depth, motion, the sun's shadow maps and the engine's direct light. Its float inputs (scene color and both motion images) can be engine targets created shareable, read without copies; the 8-bit G-buffer can be shared after a change to a typed format, if the G-buffer pass does not slow down; depth and every shadow map can never be shared, because D3D11 does not share depth-stencil resources, so those are converted, and for the 8192×8192 sun atlas only the cascades the block reads. The engine draws its fog image once, at the start of the frame, so the main block can overwrite it before deferred composition reads it, provided no pass in between reads it; ten captures of the unmodified game showed none (deferred composition is its first reader).

**Raster:**

| Pass | API |
|---|---|
| G-buffer, the engine's ambient and light draws with our shadow filtering | D3D11 |
| Main block: fog volume, reflection rays and their denoising | D3D12 |
| Skin scatter, composition (fog and shafts applied), sky, forward models | D3D11 |
| Reflection composite at the engine's reflection pass, and the copies kept for the next frame | D3D11 |
| Particles, water ports, thermography | D3D11 |
| Reconstruction | D3D12 |
| Post-processing, HUD | D3D11 |
| Frame generation and presentation | D3D12, no return |

**Ray traced:** the same, with ray-traced shadows and the engine's direct lighting computed in the main block. The engine's light draws and its local-light shadow maps are skipped. The sun's shadow maps stay as long as forward shaders (particles, rivers, lit forward models) read them.

**Path traced:**

| Pass | API |
|---|---|
| Scene update for ray tracing | D3D12, independent |
| G-buffer | D3D11 |
| Main block: all lighting (direct and indirect, diffuse and specular, the traced forward layers), fog volume | D3D12 |
| Skin scatter, composition (fog on opaque surfaces), sky, the traced layers composited over them, particles | D3D11 |
| Ray Reconstruction | D3D12 |
| Post-processing, HUD | D3D11 |
| Frame generation and presentation | D3D12, no return |

The engine's light, reflection and ambient draws are skipped, as are its forward draws of glass and water, which the path tracer already traced. The traced layers (glass and water in front of the G-buffer) are kept apart from the opaque result: the main block fogs each layer at the layer's depth from the fog volume and lights its rays that leave the scene with the sky, and the layers are composited after composition and the sky. Particles stay rasterized and are lit from a volume the path tracer fills.

**Round trips per frame:** two for every method, the main block and reconstruction, the same as today's raster frame (today the diffuse pass runs on D3D11, and the reflections and reconstruction each make one). Frame generation adds a one-way hand-off at the end of the frame. Moving the diffuse pass to D3D12 without moving the reflection rays into the same block would make three.

**Why not one block from the start of the frame to reconstruction.** As more passes are replaced, the main block grows; in the path-traced method it already covers everything from the end of the G-buffer to composition. Making it reach from the start of the frame would mean moving the rest onto D3D12 as well: the G-buffer (about 220 material shaders, terrain, foliage, decals, rain and wetness, skin, instanced clones, each draw's state and constants every frame), the shadow maps the forward shaders still read, and the transparent effects (about 130 particle shaders, liquids, heat haze, camouflage, footprints). That is a port of most of the engine's renderer, it would have to reproduce every effect the game draws, and it would give up the rule that primary surfaces keep the game's materials. What it would save is the remaining round trips, 0.2 to 1 ms each depending on the scene. So the block grows only where a pass is worth replacing anyway, never to remove a switch.

Two ways to shrink the path-traced frame further without a port, each an experiment for that stage:

- **Transparent effects as a layer.** Most particles and effects do not read the lit scene. The game can draw them into a separate premultiplied layer on D3D11 while D3D12 computes the lighting, and Ray Reconstruction takes that layer through its transparency input. The few effects that refract the scene copy (heat haze, footprints, camouflage) would draw after reconstruction or move into the layer with a copy of last frame's image. This alone does not remove a round trip, because skin scatter, composition and the sky still run on D3D11 between the main block and Ray Reconstruction; only with those moved into the block as well would the path-traced frame have one.
- **Traced primary surfaces per material class**, where the path tracer's materials are shown to match the G-buffer's. Each class moved that way takes its draws out of the G-buffer pass; the G-buffer itself stays as long as any class still needs it.

What the methods share: the material record and BSDF, the light list, the sky model, the bitmask gather (raster, and the near layer of Ray Traced), the signal formats, and the place of each pass in the frame. What they do not share: path tracing has separate primary-hit handling, sampling, cache and denoiser, and is free to diverge wherever sharing would cost it quality.

## 4. Raster method

### 4.1 What it can see

Global Illumination gathers light from what the current frame shows: a march of the depth buffer finds which directions nearby surfaces block and what light they send back, and Multiple Bounces carries each frame's indirect light into the next frame's source on the surfaces that stay in view. A light source or a sunlit wall that leaves the screen stops lighting the scene, hidden surfaces neither block nor bounce, and forward-drawn lamps contribute nothing. A better estimator on the same data (more rays, reservoirs, a stronger denoiser) cannot fix that, because the data is missing.

Getting that data takes rays against the real scene, so it belongs to the ray-traced method (section 9). The raster method makes the most of what the screen holds and keeps the game's authored ambient light in the directions the screen leaves open. A software record of the scene was built and tried first and then removed (section 4.7).

### 4.2 The gather

Today's visibility-bitmask pass (GT-VBGI), at a quarter of the pixels by default. For each pixel it finds which directions are blocked nearby and what light the blockers send back. The application pass darkens only the game's ambient light by the blocked share, keeps direct light, and adds the bounce light once, so irradiance is the blockers' light in the directions they cover plus the game's ambient light in the directions left open. Multiple Bounces is built ([guide](../lighting/indirect/README.md)).

Planned, from the earlier gather plan (`build/analysis/mgsv/mgsv-gi-unified-20261118/PLAN.md`, step 3):

- A pyramid that stores the nearest surface's depth together with its source light, so one read gives both, and fewer steps where the pyramid makes them redundant.
- Output as low-order spherical harmonics, so each full-resolution pixel is lit with its own normal instead of four low-resolution results interpolated by depth.
- A test of whether a D3D12 gather started right after the G-buffer overlaps the game's shadow and light draws. The trace moves to D3D12 only if it measurably does; without overlap a D3D12 gather would only add a round trip.

The gather keeps what today's diffuse chain has: no per-pixel denoiser and no history for diffuse light beyond the light Multiple Bounces carries forward, which reads reconstruction's camera records and so turns off with Reconstruction Off.

### 4.3 Reflections

Today's chain stays: stochastic GGX rays traced on the screen and denoised by NRD, then the area's cube map and the sky where a ray leaves the screen. Reflections keep real rays at every roughness: in the captured scenes most receivers are medium gloss (half between 0.4 and 0.6, only 2.5% above 0.7), which low-resolution light from probes cannot represent. Planned, from step 4 of the earlier plan:

- Variable ray density: one ray per pixel where the surface is sharp enough to need it, one per 2×2 pixels elsewhere, rotating over four frames. First with NRD's built-in reconstruction of skipped pixels; if that is noisier than today, the trace stores each hit and a reuse pass gives each pixel its neighbors' hits, weighted by its own BRDF over the neighbor's PDF.
- The input pass drops outputs that duplicate each other, and the engine's targets are tried shareable so D3D12 reads them directly.

Where the rays run is decided in section 3.1: in the main block, reading last frame's image, if the G-buffer marks receivers well enough and the delay is not visible; otherwise at the engine's reflection pass, as today.

### 4.4 Direct light

Enhanced shadows stay as they are. Shadows for the local lights the engine leaves unshadowed need rays, and belong to the ray-traced method.

### 4.5 Forward-drawn surfaces

The raster method leaves their lighting to the game: the ambient pair, the sun, nearby lights and authored cube maps. Water is ported in stage 3 (section 6), and the traced methods light the rest (sections 9 and 10).

- **Fakes.** The blob shadow under a character's feet is removed where contact shadows already cover it.
- Unlit particles, screen-space effects and the HUD keep the game's shaders.

### 4.6 Quality tiers

Lighting Quality maps to the gather's resolution and step count and to the reflections' ray density. High, the default, is the level that matches or beats today's default against a converged reference; Low and Medium cost less, and Ultra is measured, not bounded.

Today's cost, measured at 1920×1080 internal (4K output with DLSS Performance) on an RTX 5080: about 2 to 3 ms of GPU time for Global Illumination at the sortie menu and Mother Base (the diffuse gather 0.7 to 1.1 ms, the D3D12 reflections 1.2 to 1.5 ms, the material capture and reflection inputs about 0.3 ms), and about 3.6 ms on the Afghanistan hillside, where the reflections alone took 2.44 ms (2.34 to 2.38 ms in the sandstorm at night). Of that, 0.9 to 2.6 ms shows in frame time, depending on how CPU-bound the scene is. The budget is per scene: the default tier may cost at most about 1 ms more than today's Global Illumination in the same scene. The budget is a requirement, not an estimate: a part that does not fit its tier moves up a tier or to the traced methods.

### 4.7 Considered and not chosen

- **A memory of the scene with a world probe grid** (built in November 2026, removed on 2026-11-21). Camera-centered world grids of seen surfaces, fed from the G-buffer, the sun's shadow maps and the terrain's height textures, and a probe grid traced through them to light the directions the gather leaves open. Interpolating between probes leaked light around corners, the visibility tests that stopped it flickered as the memory changed, and the result changed visibly while walking. Its one measured gain was darker enclosed interiors; outdoors and in half-open spaces it measured the same as the gather alone. Rays against the real scene give that gain without such tests, so it was left to the ray-traced method. Scene memory is in the history as commit `b3dd739b4`.
- **Per-pixel ReSTIR on screen rays** (reservoir resampling as the gather). It improves the estimator, not the information: its samples can only land on what the screen shows, reused samples cannot be checked properly without real rays, its output is correlated and still needs a denoiser, and its clear win is limited to a few small bright sources on screen. Reservoirs belong to the ray-traced and path-traced methods, where a visibility test is a real ray.
- **Radiance cascades** as the gather. Noise-free and fast in the one shipped screen-space use (a fixed camera), but the free-camera 3D prototypes the research found flicker under camera motion, and the published world-space prototypes took 30 to 50 ms per frame on an RTX 3060, far over the budget.
- **Voxel cone tracing.** Noise-free, but prefiltering mixes a thin wall with the air around it, so it leaks by construction, and it needs solid voxelization that depth views cannot give.
- **An irradiance volume alone.** It looks flat (it has no contact detail) and leaks through walls thinner than its probe spacing.
- **Light propagation volumes from the sun's shadow maps.** Low-frequency only, no secondary occlusion, bleeds through thin walls.

## 5. Atmosphere

Shared pieces feed the fog, the clouds, the water and the sky: the frame's light record (the true sun direction, the shadow-casting light's direction, sun color, sky light, exposure), the phase functions and a multiple-scattering table, the fog volume, and an environment map of the sky.

**Fog volume.** A view-aligned volume we compute ourselves, using the game's depth slicing so its result copies straight into the engine's fog image. The game's fog is analytic daylight scattering with the structure of Hoffman and Preetham's model: homogeneous, with no height term and no shadows, its color authored on the scattering coefficients under a white sun. The medium keeps the game's authored density and its curve of optical depth over distance (optical depth grows as distance to a power of 0.7 to 0.9). Its light becomes physical: the sun through the shadow maps (this produces the shafts), the sky light, multiple scattering from the table, and the local lights. At a low sun the engine casts shadows from a light held at 26 to 27 degrees of elevation while the sky shows the true sun, so the fog's shadow lookups follow the shadow-casting light and its phase function follows the true sun. The engine's fog image is enlarged at creation (for example to 160×90 cells per slice). All 174 shaders that read it address it by fractions of a tile (read in each of them; to be confirmed in game), so deferred composition and every forward shader should show the new fog without other changes.

**Sharp shafts on opaque surfaces.** Shafts from the volume are a few cells wide. For opaque pixels the composition shader reads the mod's volume, which keeps the sun's in-scattered light with and without shadows separately (the engine's fog image has room for only one), and replaces the shadowed sun term with the unshadowed term times a per-pixel average sun visibility: 8 to 32 shadow-map samples along the view ray at half resolution, placed so each carries an equal share of the in-scattered light, accumulated over time. Forward surfaces keep the softer volume. Both integrate the same medium, so they differ in sharpness, not brightness.

**The game's painted shafts.** The light shafts the game shows under the African canopy are not fog: they are soft, additive sprite cards drawn with the transparent effects between the reflection pass and post-processing, fixed in place and blind to the shadow maps. In the captured frame they are 11 additive draws of `Prim_Poly_Softblend_DL_VF` with one 128×128 texture; the same shader draws other sprites with other textures. At the game's clear-weather density, real single-scattered shafts are far weaker than these cards (an estimate from that frame's fog constants), so the cards are kept and each card pixel is multiplied by the sharp-shafts pass's sun visibility; they are dimmed only where the fog itself is dense. A texture identity that holds across runs is still to be found.

**Sky environment map.** Built before the physical sky, from linear versions of the game's sky and cloud shaders evaluated for any direction (they need no depth or history; only the sun disc uses the view and projection matrices), with the fog's far value per direction, into a small environment map each frame. It serves reflections only: the reflection fallback, the oceans' hard-coded sky colors, and later every ray that misses. Diffuse sky light stays the game's baked sky light: that bake is 1.6 to 2 times brighter than the drawn sky at noon and 3.5 times in rain, and blue where the drawn sky is red at a low sun, so projecting the drawn sky would change every surface's ambient light.

**Physical sky.** The four tables of Hillaire's 2020 method (transmittance, multiple scattering, sky view, aerial perspective), computed every frame for the sun and the moon. The fit has three authored things to reconcile: the drawn sky, which matches the layout of one layer of the engine's precomputed Bruneton tables (with a Mie term stronger or taller than the paper's defaults, not fitted) tinted per weather, and the sun color and sky light, which come from separate baked per-time tables in a clear and a cloudy set and do not match it. Whether that fit works is an experiment. The difference between the authored sky and the fitted sky is kept as an explicit, smooth correction with a strength control (**Authored Look**), so the game's colors survive by a stated choice and not by a hidden multiplier. Sun and moon color for surfaces, disks, clouds and fog then come from the transmittance table.

**Clouds.** Two draws after the sky: a ray march of 8 samples through two static noise textures on a dome that follows the camera, lit by the sun color with a fixed phase function and a flat ambient term, and camera-attached texture layers. Neither has a world position. Both are ported and relit with the light record, the fog's phase function and multiple scattering. A cloud shadow map is marched from the same noise under a stated convention (anchored to the world in the horizontal plane, at a fixed cloud height) and read by the sun light, the fog and the shafts; the texture layers can only dim the sun uniformly.

**Order.** Fog first (it needs nothing else and improves every surface at once), then sharp shafts, then the clouds (their shadow map feeds the sun light, the fog and the shafts), then the sky environment map (it needs the fog's far value and the cloud ports). The physical sky comes after the raster stages: nothing depends on it, and if the fit is poor for a weather type the game's tables stay.

## 6. Water

Water draws in two passes around the engine's second scene copy of the frame (the first is made before the reflection pass). Before the second copy: the Fourier ocean (Mother Base, ACC), which writes depth, the marsh, and the 24-hour ocean's base board. After it: rivers and the 24-hour ocean, which refract the copy and do not write depth. All draw before or among sorted particles, so each water shader must output its final color itself. The ported shaders therefore do this work in place:

- **Reflection.** A hierarchical screen trace inside the shader. Calm water follows the mirror direction bent by the ripple normals; rough water widens the fetch by roughness instead of adding noise, so no denoiser is needed. Rivers trace the second scene copy, which they already refract; the Fourier ocean draws before that copy but after the first, which still holds this frame's scene, and traces that. The 24-hour ocean and the marsh are in no capture yet. A miss takes the sky environment map from the atmosphere stage; until it exists rivers and the marsh keep their authored cube, and the ocean has none. The sun's shadow never scales the reflection.
- **Fresnel and transmission.** Water's real Fresnel with roughness from wave slope variance, reflection weighted by F and the view through the water by 1 − F.
- **The water body.** Refraction offsets checked against depth, absorption by depth with one coefficient set per water type, and single scattering of sun and sky light in place of the fixed water color. Deferred composition fogs the bed before the scene copy is taken, so the port fogs only the surface terms.
- **Ambient light**: rivers and the marsh keep the game's ambient pair; the oceans have no ambient input (they read only the sun's direction and color), so their ports take ambient light from the light record; the traced methods replace both.
- **Caustics.** The area change of sunlight refracted through the ripple normals, applied to sunlit pixels under water, blurred with depth and faded by absorption. It modulates light that reaches the bed and averages to one.
- **A surface record.** Each port also writes the water's normal, roughness, depth and type to the second render target every water shader already writes (the game leaves it unbound). Nothing reads it in the raster method except reconstruction and frame generation (depth and motion for forward surfaces); the ray-traced and path-traced methods and Ray Reconstruction depend on it.

**Waterfalls** are river shaders for the falling water plus a cloud of liquid spray. Looking down at the base of one in Africa, the transparent effects brightened 61% of the frame: 92 `Primitive_LiqSprt2Final` sprites and 62 `Primitive_Liquid2Final` meshes, which take the sun through its shadow maps and two point lights. The spray is lit like other particles, so its ambient light is the game's ambient pair until the traced methods light it (section 9); the river ports cover the falling sheet.

Wet surfaces are audited against a published wetness model before anything changes. In rain the wetness comes from one full-screen pass that rewrites the G-buffer material (`TppRainFilterGBufferMaterial`), and deferred composition switches to its rain variant (`DR_TonemapRainFilter`); the falling streaks (`Primitive_TppFxRain_Boost`) brightened about 3% of the frame in Africa, and splash particles drew as `Primitive_Liquid2Final` and `Primitive_LiqSprt2Final`.

## 7. Frame generation

All three providers generate and pace frames on a D3D12 swap chain that their SDK creates or wraps; none documents a D3D11 path (community projects have reportedly driven DLSS Frame Generation from D3D11 through Streamline, an undocumented route). The game presents through D3D11. So the real swap chain moves to our D3D12 device: each finished D3D11 frame crosses through a shared texture and a fence, the provider's swap chain presents it, and the provider inserts the generated frames. One provider is active per run.

There are two ways to get there, and the first experiment chooses between them:

- **Replace the game's swap chain.** The game is handed a stand-in swap chain whose back buffer is the shared texture. This is the published route for a D3D11 game, and it is known to work, but the project that ships it had to switch off ReShade's hooks and drive ReShade by hand. For an add-on that lives inside ReShade and uses RenoDX's HDR swap-chain handling, that is a large change.
- **Leave the game's swap chain alone.** ReShade and RenoDX keep working on it unchanged; its finished image is copied across, its presents are withheld, and the provider's swap chain shows the frames in a window placed over the game's. Less to rebuild, but window handling, input focus and pacing have to be proven.

Order of providers: FSR first, because it needs no latency SDK and a complete open example exists; then DLSS Frame Generation through Streamline on the D3D12 device, which requires Reflex markers and its sleep call in the engine's main loop; then XeSS Frame Generation, which requires Intel's latency library and cannot run beside Reflex.

Inputs, produced once for every provider:

- **The image without the HUD**, taken before the engine's 2D list. The HUD and the camera filters are objects in that one list, sorted by priority, and the HUD clears the scene's depth part-way, so a separate HUD layer with alpha is not available cheaply. Whether each provider works from the HUD-free image and the finished frame alone is checked in its documentation first.
- **Depth and dense motion vectors.** Forward-drawn surfaces have no motion today: water and the ocean get it from their surface record; particles and glass rely on the provider's optical flow.
- **Camera data** latched once per frame at a known engine point: unjittered matrices, jitter, near and far planes, a reset flag on camera cuts, and a frame number that rises by exactly one. The decompile shows no setter of the engine's cut flag outside cutscene code, so cuts for aiming, binoculars, field-of-view changes and teleports come from the mod's camera records.

Conditions and limits: HDR output must be HDR10 (two providers refuse scRGB), which the add-on already uses; borderless only, which the add-on already forces; generation is off in menus, pause and loading; every history length in the lighting uses the real frame rate, not the presented one.

Known traps from the published implementation: stale references and newer swap-chain interfaces on a stand-in swap chain, resizing with a buffer count of zero, a camera latched from the wrong pass (which shows as stutter), and several rounds of pacing fixes.

## 8. The scene on D3D12

Ray tracing needs the scene outside the camera's view. The work, in order:

1. **Static geometry and instances** from the engine's model resources and scene object lists, not from its per-view visible lists, which are already culled to the camera. Each mesh's role (opaque, masked, forward, shadow-only stand-in) comes from the same per-mesh flags the engine uses to route it to a pass.
2. **Terrain and vegetation** from the engine's height data and placement lists.
3. **Skinned characters**: the bone matrices that the frame's draws upload, applied by a compute pass that also keeps last frame's positions for motion; acceleration structures refit when a skeleton moves and rebuilt on a schedule. Poses are not read from engine memory when the trace runs, because the engine is already recording the next frame by then.
4. **Materials at a hit**: a simplified record (base color, roughness, emission) from the same textures the G-buffer uses, which is standard practice for secondary hits.
5. **The forward set**: glass, water, emissive models and other blended meshes, flagged so they can be traced apart from opaque geometry.
6. **Foliage and other alpha-tested geometry**: opacity micromaps where the GPU supports them, alpha tests in the trace elsewhere. Alpha testing is expected to be the main cost.
7. **Lights**: the engine list, including the authored boxes that confine some lights, plus emissive triangles for models with no engine light.
8. **A top-level structure rebuilt every frame** within a measured budget, and a memory budget taken from the GPU's reported free memory, not fixed pools.

The first deliverable is a debug view that traces primary rays and compares depth and material against the G-buffer. Ray origins for G-buffer pixels are placed on the traced triangle under the pixel where one is found, because plain normal offsets are reported to leak at this game's scale.

## 9. Ray-traced method

The raster method with real rays wherever a ray helps. It runs on any ray-tracing GPU and denoises with NRD.

- **Shadows.** The sun: one ray per pixel toward its disk, denoised. Local lights: every light can cast a shadow; one light is chosen per pixel by resampling over a world-space light grid and tested with one ray, with RTXDI's ReSTIR direct lighting as the higher setting. The engine's unshadowed lighting math is kept, so only visibility changes. At a low sun the engine's shadow-casting light is held at 26 to 27 degrees of elevation; rays can use the true sun instead, a choice made when the ray-traced method is built, and the cloud shadow map follows the same choice.
- **Diffuse indirect.** Screen probes: one probe per 16×16 pixels placed on the depth buffer, with extra probes where depth changes sharply, each tracing an 8×8 octahedral set of directions as ray queries against the real scene, with hits lit from SHaRC, a world-space radiance cache. Directions are steered toward where last frame's probes found light and where the surface responds most. Probes are filtered against their neighbors in probe space, with hit distances limiting how far a value may spread, then accumulated over time with rejection by depth and normal instead of color clamping. Each probe is reduced to spherical harmonics and interpolated to pixels with plane weights. The bitmask gather stays as the near layer, because not every small detail is in the acceleration structure: blockers it finds light the directions they cover, the probes light the directions it finds open, and the game's ambient light is replaced there, so each direction is counted once.
- **Reflections.** One GGX ray per pixel below a roughness cutoff; screen first, then a ray query; hits lit by the sun and the light grid with shadow rays plus the cache; screen probes above the cutoff.
- **Forward surfaces.** Water and glass trace their reflection and refraction from the surface record. The engine gives every lit forward object an up color and a down color from one function; that function is overridden to return the traced light at the object's position, read back to the CPU every few frames from a world volume the radiance cache fills. Particles, smoke, splashes, forward models, glass, rivers and marsh then follow the scene's light, one color pair per object, with no shader replaced. This needs one more engine entry, and the read-back delay must be shown not to cause visible lag.
- **Emissive forward models.** Lamps, signs and screens are drawn after lighting and are unlit. They write only the scene's depth-stencil, not the G-buffer depth the screen trace marches, so no screen ray finds them. Where an engine light sits at the same lamp, that light already lights the surroundings and the model adds nothing; counting both would double it. Elsewhere the model becomes a separate light, placed from its world bounds and colored from its material, in the light list. The pairing is decided once per model and shown in a debug view.
- **Fog.** Sun and local-light visibility in the volume from rays: one ray per cell per frame, so every local light can cast shafts.

## 10. Path-traced method

The highest-quality method. It requires DLSS Ray Reconstruction, which denoises and upscales in the slot reconstruction uses today, before depth of field.

- **Primary surfaces.** Opaque pixels start from the game's G-buffer, which keeps the game's exact materials, decals, animation and motion vectors. Everything in front of it is traced: a camera-ray segment from the eye to the G-buffer depth against the forward set, with hits treated as real path vertices that reflect and transmit, in up to three layers per pixel. A debug mode traces true primary rays and compares them with the G-buffer. This is a choice for primary-surface fidelity; fully traced primary rays remain possible per material class.
- **Direct light.** The sun and moon: one shadow ray toward the disk at every path vertex. Local lights: a world-space light grid feeding resampling, with ReSTIR direct lighting at the primary vertex. The sky: the physical sky's environment map, importance sampled with the sun kept out of it.
- **Emissive surfaces** are in the acceleration structure so rays find them, and models with no engine light become sampled emitters. A model paired with an engine light stays visible to the camera and in reflections but is not counted again as a light.
- **Indirect light.** One path per pixel, the lobe chosen by chance, ended in SHaRC after a set number of diffuse or glossy bounces (the Bounces setting, two by default). The first hit is always lit fresh; the cache only ends a path, so no two histories are stacked. Turning the cache off must stay possible: that is the correctness reference.
- **Resampling.** ReSTIR for indirect light is off by default until it is tested with Ray Reconstruction: reused samples are correlated, which many denoisers handle poorly, and the guides we have do not say how Ray Reconstruction treats them. ReSTIR GI is a middle option and ReSTIR PT an experimental top option.
- **Transparency.** Glass and water are traced layers. Particles stay rasterized, lit from a volume the tracer fills, and reach Ray Reconstruction through its transparency inputs. Fog is applied by deferred composition to opaque surfaces, as today, and by the main block to each traced layer at the layer's depth, from the same fog volume.
- **Caustics** come after the method is stable. Path tracing at one sample per pixel almost never finds sunlight through water (the sun is a small source seen through a mirror-like surface), so sun caustics are injected from a sun-view caustic map, upgraded to traced rays. The raster water's caustics are computed from the ripple normals in the water shader, a different and cheaper method. Glass caustics are a later, smaller item.
- **Other vendors.** The tracer keeps its outputs split (direct and indirect, diffuse and specular, with hit distances), so NRD or another vendor's denoiser can be tried later without changing the tracer. Neither is a goal.

## 11. Settings

Principles: most players only pick a preset; each feature has one mode switch, and any further control is a quality tier, a stated choice of look (Authored Look, Light Shafts) or a part that can be turned off (Cloud Shadows, Caustics); raw algorithm values (pixel reaches, thicknesses, sample counts) are not added as player settings, because they give resolution-dependent results, with two exceptions: existing controls keep working until the pass they tune is replaced, and the Path Tracing Custom preset exposes bounce and path counts; strengths stop at 100%; a control that cannot apply is disabled with a note saying why; each feature's Advanced section shows its measured GPU time. Existing saved keys keep their meaning. `LightingMethod`, `LightingQuality` and the `PathTracing` keys cover direct and indirect lighting together, so the key-prefix rule in the mod's `AGENTS.md` gains a stated exception when they are added.

**Lighting**

| Control | Key | Values | Default |
|---|---|---|---|
| Lighting Method | `LightingMethod` | Raster, Ray Traced, Path Traced | Raster |
| Lighting Quality | `LightingQuality` | Low, Medium, High, Ultra | High |
| Shadows | `DirectLightingShadowsMode` (existing) | Vanilla, Enhanced | Enhanced |
| Global Illumination | `IndirectLightingMode` (existing) | Vanilla, Enhanced | Enhanced |

Lighting Quality selects the raster tier (4.6), and the ray-traced tiers once stage 8 defines them. Under Path Traced, Lighting Quality and the two mode switches are disabled, with a note that path tracing replaces them and its preset sets the quality.

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

**Direct Lighting (Advanced)** keeps Shadow Softness, Contact Shadow Reach (`DirectLightingContactShadowReach`), Spot Contact Shadow Budget (`DirectLightingSpotContactShadowBudget`) and the debug views as they are, and gains **Local Light Shadows** (`DirectLightingLocalShadows`: Vanilla (default), All Lights; All Lights needs ray tracing).

**Indirect Lighting (Advanced)** keeps the four strength controls, Multiple Bounces and the debug views. The diffuse resolution, rays per pixel, search radius, thickness, offset, direction and sample controls and the depth-mip and backface-rejection switches are retired as the planned gather and reflection changes fix the values they tune; their work moves into Lighting Quality.

**Atmosphere**

| Control | Key | Values | Default |
|---|---|---|---|
| Fog | `AtmosphereFogMode` | Vanilla, Volumetric | Volumetric |
| Light Shafts | `AtmosphereLightShafts` | Off, Soft, Sharp | Soft until Sharp is measured |
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

**Debug views** live in the Advanced sections: the gather's and the reflections' stages as today, fog slices and sun visibility, the water surface record, and for the traced methods the acceleration structure against the G-buffer, screen-probe placement and rays, the radiance cache and emitter pairing.

## 12. Validation

- **Uniform light.** In a scene lit by one constant color every integrator must return that color: the gather, the reflection chain, the forward lighting function, the fog volume, and each traced method.
- **Against the reference.** Once path tracing exists, its accumulated reference is the target the raster and ray-traced methods are compared with, scene by scene.
- **Turn-around test.** With the camera turning away from a sunlit wall or a lamp, the bounce light must stay. This is the test screen-space GI fails.
- **Leak tests.** A dark interior beside a bright exterior, entered for the first time and after looking around.
- **Fixed scenes for cost.** The ACC title, the Afghanistan hillside, Mother Base by day, at night and in rain, measured at a fixed camera with frame time and GPU time separated.

## 13. Surviving a game update

The game still receives small updates: the executable the mod supports was built on 2026-07-07, and the most recent update only changed online services (by the owner's account, it removed support in Russia). The game's rendering does not change in these updates: the renderer's code, its structures and its shaders stay the same. What an update can do is rebuild the executable, which moves code and data to new addresses. So the problem to solve is finding unchanged code at new addresses, not following changed code. What the mod relies on, and what an update does to each:

| The mod relies on | Today | Breaks when |
|---|---|---|
| Code addresses: functions, call sites, return addresses | Most of the 78 entries in `engine/build.hpp`; the rest are the three addresses below, a few values read from the game's data (depth-of-field constants, the spherical-harmonic volume vertex counts), and four functions that are only read to check structure fields. Before the table, about 60 numbers sat in seven files, half of them unnamed | The executable is rebuilt at all: everything after a changed function shifts |
| Structure fields: offsets, strides, shifts, masks and flag bits | 59, named in the same table, each checked at startup in one instruction of a verified entry that uses it. Before the table, about 70 numbers sat in five files, most of them unnamed | Not expected: the renderer's structures do not change |
| Global and virtual-table addresses | Three, in the same table | The executable is rebuilt |
| Shader CRC32s | Every ported shader, in its file name, and the draws we recognize | Not expected: the shaders do not change, so a CRC32 is a permanent name |
| Name hashes of render targets and plugins | Dozens | Never, short of a rename |

Before the table, each hook file checked the executable for itself: its build time and size first, then the first bytes of each function it hooked. That was safe, but it was repeated in six places, any rebuilt executable turned off every engine-side feature at once, and after an update the numbers to fix were scattered through feature code. One site, the projection commit, already searched the game's code for its bytes at startup, skipping the call target inside them.

The approach: a game update means a table update and a new mod build, done by following the [engine table guide](../engine/README.md). The mod does not search for its entries at startup. A search would let an unchanged mod run on an updated game, but it needs a byte pattern that is unique for every entry and a tool to keep those patterns current. This game is updated rarely, and its updates only move code, so a clear guide that a person or an AI can follow costs less than that machinery.

**1. One table describes everything tied to the executable.** `engine/build.hpp` lists every entry under the engine's name, with the bytes expected at its address and a wildcard on every byte that moves when code is relinked (call targets, references to global data). An entry is a function, a place inside a function identified by its own bytes or by the call before it, a data constant, or a global or virtual table slot found through an instruction in another entry that refers to it. Structure fields are compile-time constants in the same table, each with one instruction in a verified entry that uses it. No other file contains an address, an offset or a byte pattern; feature code asks the table by name.

**Shaders by their engine names.** The game's shaders were compiled with debug information, so each dumped shader names its engine source file (for example `0x1B5FA666` is `SSLighting2_Spot_Mask_NSpec_ps`, from its `#line` directives); 1,584 of the 1,591 dumped shaders carry a name. A generated file, `engine/shader_names.hpp`, gives each shader that feature code recognizes a constant named after its engine source (`GBUFFER_VELOCITY_PS = 0x9815404F`); 24 shaders have one. A ported shader's registration in `shaders.hpp` keeps the bare hash, because the same hash names its file (`Name_0xCRC32`) and its embedded code. A lookup from CRC32 to name for logs and debug views is added when one of them needs it.

**2. One check at startup.** Everything is read from the executable file, not from the loaded image: hooks this mod, the probe or another add-on have already installed change the code in memory, and a check that ran after them would fail. The build's identity (time stamp and image size) must match; then every entry's bytes are checked at its address, every reference between entries and the virtual table slot must hold, and every field must still be encoded in its instruction. This costs about a hundred short file reads at startup and nothing while playing: a hook is a plain detour, a resolved address a plain pointer, and a field offset a compile-time constant.

**3. Features fail one at a time.** A feature declares the entries and structure fields it needs. If any of them fails, the feature does not install and turns itself off; the menu names the feature and the log names each entry or field that failed, with the reason. Everything else keeps running. HDR output, tone mapping, color grading and shader replacements need no entries at all, though without the render-target entries the scene stays in the game's 8-bit formats, so HDR quality does depend on them.

**4. Updating for a new build.** The [engine table guide](../engine/README.md) says what the table holds, how to find each kind of entry again in a new executable, and how to check the result. The table supports one build at a time.

**Shaders.** The game's shaders are not expected to change: its updates since release have been maintenance of online services, the most recent, by the owner's account, removing support in Russia. If a ported shader ever stops replacing its draw, its hash is compared with a fresh dump.

**Checked so far:** on the current executable every entry and field verifies and every hook installs at the same address as before; an accidental failure of two entries turned off only the two features that use them, and a deliberately wrong field turned off only upscaling.

## 14. Folder layout

### 14.1 What the audit found

Most features already sit in a folder named for what they do. The audit found the problems below; the stage 1 move fixed the ones marked done.

- **Engine-facing code was spread over three folders.** Done: the view and command-list tracking, render target creation, motion-vector hooks, the local-light hook and the post-processing start hook are now `engine/views.hpp`, `engine/render_targets.hpp`, `engine/motion_vectors.hpp`, `engine/local_lights.hpp` and `engine/post_filter.hpp`. Draw identification did not move: its velocity roles became unnecessary once the lighting history took its motion from the camera-velocity hook, and what remained was feature logic, so the bloom chain's gate is in `postprocess/bloom/bloom.hpp` and the scene-copy handling in `reconstruction/output_size/runtime.hpp`.
- **The scheduler lived in the wrong feature.** Done: `rendering/submission.hpp` cuts the engine's command lists and runs each feature's job there, for contact shadows, the diffuse gather and reflections as well as reconstruction. Reconstruction's own part (its device, history resets and the scene's job) is `reconstruction/dispatch.hpp`.
- **Two folders shared a name for different things.** Done: the redirection that runs post-processing at output size is now `reconstruction/output_size/`; `postprocess/` holds only ports of the game's passes.
- **Dead files.** Done: the unused `tonemap/customtest31.hlsli` (2,693 lines) is gone. The root `common.hlsli` stays; it only includes `shared.h`, but the shaders that include it compile through it.
- **Inconsistent folders.** Done: `UI/` is now `ui/`, and the depth-of-field and motion-blur ports have folders beside bloom.
- **Superseded documents.** Done: `docs/Fox_Engine_Render_Pipeline_Reference.md` and `docs/MGSV_PixelShader_NameMap.md` held conclusions from early analysis that later work corrected; they are archived outside the repository, and the engine reference and the render map replace them.
- **Namespaces did not always match folders.** Done for every moved file.

Kept as they are: `materials/`, `lighting/native/`, `lighting/compose/`, `lighting/shadows/`, `lighting/reflections/`, `lighting/indirect/`, `reconstruction/dlss/`, `fsr/`, `analytical/`, `tonemap/` (`customtest35.hlsli` keeps its upstream name), `copy/`, `antialiasing/`, `effects/`, `sky/native/` and `output/` (until the stages that move them), `water/native/`, `assets/` and `third_party/`.

### 14.2 Rules

- One folder knows the executable: `engine/`. It holds the table of entries and the hook points other features build on (where a pass starts or ends, what a draw is). Feature logic that rides on a hook stays with its feature and takes its addresses from the table; camera jitter and render-size control stay in `reconstruction/`.
- Shared GPU infrastructure (the D3D12 bridge, submission, timing, saved state, the denoiser adapter) is in `rendering/`.
- Each feature has a folder named for what it does, and a port of an engine shader sits with the feature that changes it.
- Folder names are lowercase; a namespace follows its folder (`engine/views.hpp` is `mgsv::engine::views`).
- A folder appears with its first file. A move keeps shader file names and embed identities, and the folder rules in the mod's `AGENTS.md` change in the same commit.

### 14.3 The layout

| Folder | Holds | Change |
|---|---|---|
| (root) | `addon.cpp`, `settings.hpp`, `shaders.hpp`, `shared.h`, `common.hlsli`, `README.md`, `ROADMAP.md`, `AGENTS.md` | Unchanged |
| `engine/` | `build.hpp` and `image.hpp` (the table and its check); `views.hpp` (renderer, view and command-list tracking); `render_targets.hpp`; `motion_vectors.hpp`; `local_lights.hpp`; `post_filter.hpp`; `shader_names.hpp` | Stage 1 done |
| `rendering/` | `d3d11_state.hpp`, `d3d12_bridge.hpp`, `d3d12_compute.hpp`, `d3d12_timing.hpp`, `shared_texture.hpp`, `nrd.hpp`, `inputs.hpp`, `producer_records.hpp`, `submission.hpp`, the probe interface, and the shared shader includes | Stage 1 done |
| `reconstruction/` | The methods (`dlss/`, `fsr/`, `analytical/`), camera jitter, render size and mip bias, frame inputs, settings and state; `output_size/` | Stage 1 done |
| `postprocess/` | `bloom/`, `depth_of_field/` and `motion_blur/` ports | Stage 1 done |
| `ui/` | The subtitle port | Stage 1 done |
| `tonemap/` | The two late tone-map ports and the curve and grading helpers | Stage 1 done |
| `docs/` | Architecture, design, the render map, the engine reference and their shared style and script | Stage 1 done |
| `lighting/native/`, `lighting/compose/`, `lighting/shadows/`, `lighting/indirect/`, `lighting/reflections/`, `materials/` | As today | Unchanged |
| `lighting/forward/` | Lighting for forward-drawn surfaces from the radiance cache | New with stage 8 |
| `atmosphere/` | The fog volume, shafts, sky and clouds | Stage 2: today's `sky/` moves under it |
| `water/` | River, pond and ocean ports, caustics, the surface record | Grows in stage 3 |
| `presentation/` | Frame generation, the D3D12 swap chain, display encoding | Stage 6: today's `output/` moves under it |
| `raytracing/` | The scene on D3D12: geometry, skinning, acceleration structures, materials at a hit | New with stage 7 |
| `lighting/traced/`, `lighting/paths/` | The ray-traced method and the path tracer | New with stages 8 and 9 |
| `copy/`, `antialiasing/`, `effects/`, `assets/`, `third_party/` | As today | Unchanged |

The stage 1 moves were one change with no behavior change: the moved shaders compile to the same bytes, every engine entry verifies and the same hooks install at the title, and only paths, includes and namespaces changed. The local probe followed the moves in the same change.

## 15. Open questions

- How often the engine skips drawing the cascades while keeping their matrices.
- A texture identity for the game's painted light-shaft cards that holds across runs.
- Whether the G-buffer pass slows down when its targets are created shareable in a scene with a heavy G-buffer; at the ACC title it did not (below).
- Whether the authored probes change with time of day and weather.
- The world bounds of forward-drawn emissive models, needed to pair them with engine lights, and how bright those models are next to the engine's lights. Their shaders output an authored color times the exposure, so their values may not be on the same scale as light intensities.
- Whether local-light shadow maps also store the far side of their casters.
- What selects which of two identical shader hashes the game runs; ports are registered under both.
- Each SDK's license before its source enters the repository (RTXDI, SHaRC and NRD share NVIDIA's RTX SDKs license, which forbids use that would place the SDK under an open-source license).
- The DLSS SDK in the repository predates Ray Reconstruction's current default model; updating it is a submodule change that needs approval.

## 16. How each claim is known

- **Read in the game's shaders, the executable or captures during this work:** the fog image's size, layout, channels and writer; the forward shaders' light inputs and the function that sets them; where each reflection cube comes from; the pass order of the ocean, rivers and particles; the shadow-map sizes and reach (earlier shadow work); that most sun shadow casters are drawn with front faces culled (60 draws sampled from the sun atlas and other large shadow maps in the sortie menu capture, not separated by map: 56 cull front faces, 4 do not cull); that an occluder is a list of vertices.
- **Read in primary sources by research readers and spot-checked:** SHaRC's update and termination rules and RTXDI's contents (their documents were read directly).
- **Read in an open implementation and spot-checked:** frame generation for a D3D11 game through a D3D12 swap chain created by the FSR SDK for the game's window, with a shared fence and shared textures (Skyrim Community Shaders).
- **Reported by research readers from primary sources, not re-read:** that no provider documents a D3D11 path, that community projects drove DLSS Frame Generation from D3D11, that XeSS Frame Generation needs Intel's latency library and cannot run beside Reflex, and the other frame generation requirements; Lumen's screen-probe gather and its costs; published costs of froxel fog, Hillaire's atmosphere, screen-space reflections and NRD; the Ray Reconstruction guide's input rules beyond those read for the reconstruction work; the settings shipped path-traced games expose; real-time caustics methods and their costs.
- **Design judgment, not yet tested:** the split between the near layer and the screen probes; pairing emissive models with engine lights; overriding the forward ambient colors from a read-back volume; enlarging the fog image; every cost estimate for our hardware.
- **Read in the executable by research readers, with the deciding lines checked by me in the decompile (2026-11-20):** cascade placement, caster culling, cull mode and the shadow projection's handedness, the moon driving the cascades; which models the object velocity image marks; what occluders are and how many the engine keeps; the terrain height atlas and how to reach it; that the engine creates every render target without sharing flags. The D3D11 sharing rules come from Microsoft's documentation, quoted by the reader. That reflection receivers can be chosen before deferred composition was read by me in the mod's shaders.
- **Captured by us at Mother Base at sunrise (2026-11-20):** the sun atlas holds geometry only around the view (51% of cascade 0 and 75% of cascade 3 left at the clear value); stored sun depths are casters' far sides (above); the object velocity image marks the player and a soldier, standing or walking, and nothing static, and marks nothing while the game is paused. In Africa at night (2026-11-20): the moon drives the cascades, which hold tree canopies and no ground; the object velocity image marks the player in a truck, the truck, nearby trees and a wooden shelter frame, and not the ground, rocks or grass; by day under the canopy, the visible light shafts appear only between the reflection pass and post-processing, as additive sprite cards; at the base of a waterfall, liquid spray particles brighten 61% of the frame; in rain, one full-screen pass wets the G-buffer and the streaks brighten about 3% of the frame; inside the ACC (an interior stage about 1,200 m up), the cascades still run and hold only the helicopter's shell, so sunlight indoors is blocked by the enclosing geometry, and the mask marks the player and his rifle but not the cabin; with the iDroid open there, the hologram map is ordinary 2D UI (about 620 `Draw2D` draws in the engine's UI jobs, nothing in the scene image), and the mask marks the characters and the iDroid in the player's hand; a loading screen (finished, waiting for the player) builds a view with only the sky, the fog image and 2D UI; at Mother Base at night, a fixed world point's fractional texel position in cascade 0 changed every frame while the player moved, so the cascades are not snapped to whole texels while the sun holds still; at a helipad, the mask marks a hovering helicopter's body but not its spinning rotor blades, and stops exactly at the fence bars and handrail in front of the player's legs, so it is depth-tested; at the ACC title, creating every full-size 8-bit color target shareable (the driver accepted B8G8R8A8_TYPELESS) left the G-buffer draws and the frame time unchanged, though the title's G-buffer is small; the terrain height read from the running game, 113.05 m under the camera at 115.84 m, places the camera 2.8 m above the ground behind the driver's seat, which confirms the map's world mapping.
- **Checked by us against the user's RenderDoc captures of the unmodified game (2026-11-20, eight scenes and a Fulton extraction):** the shadowed and unshadowed sun draws are one light split by a stencil volume; the light direction derived from the shadow matrix equals the engine's light vector, sign included (moon too); a cascade lookup from that matrix, replayed on every pixel, agrees with the light the engine added except at penumbrae and where the engine fades shadows with distance; the object velocity image equals stencil bit 0 of the G-buffer depth; the terrain textures and their decoding (section 2); the reflection composite draws without depth or stencil tests. Observed in game the same day: the sun's continuous drift, the one-frame direction flip at the switch between sun and moon, the pause word in the pause menu and with the game in the background, not with the iDroid.
- **Read by us in the game's shader dump and captures of the unmodified game (2026-11-21):** the fog writer's formula, slices and constants in ten scenes; that all 174 shaders reading the fog image address it by fractions of a tile; the painted shaft cards; the two sun directions at a low sun; that the ocean draws after this frame's scene copy; that four ports were registered under the twin hash the game does not run. Read by research readers and spot-checked: the sky, cloud and water shaders' inputs and math, the baked sun and sky light tables, and the end of the frame for frame generation. Notes in `build/analysis/mgsv/mgsv-dependencies-20261121/`.
- **Leads not checked by us:** the scene-capture cautions in section 8 (poses a frame ahead when read from memory, ray origins leaking with normal offsets, alpha testing as the main cost).
- **Measured by us:** today's Global Illumination cost (diffuse gather, reflections, inputs) at the title, the sortie menu, Mother Base by day and night, the hillside and the sandstorm at night, 2026-11-18, at 1920×1080 internal on an RTX 5080; the idle time and the D3D11 signal cost at the reconstruction hand-off in seven scenes, and that moving work after that hand-off gained nothing on this GPU (direct queue only), 2026-11-19; the receivers' roughness distribution in the captured scenes.
- **Read in NVIDIA's guides in `external/DLSS/doc` during the reconstruction work:** that preset F is marked deprecated.
- **The owner's account:** what the latest game update changed.
- **Counted in the mod's source and the executable:** the numbers of addresses, offsets and checks in section 13; the executable's build date from the timestamp the mod checks.
- **Still to be established:** everything in section 15, and which frame generation presentation path works inside ReShade.
