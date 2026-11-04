# Shadows

Enhanced Shadows combines PCSS filtering of the game's shadow maps with Bend contact shadows for sun, moon, spot, and point lights. The two features meet in the native light shader, before subsurface scattering, GT-VBGI, and deferred composition. Each affects direct diffuse and specular light while preserving the native environment-reflection contribution and output alpha.

## Soft shadows

The default-on Enhanced Shadows switch enables both methods for every supported light. Off retains the game's original shadow filtering. PCSS means percentage-closer soft shadows: it estimates how much of a light source is visible and lets shadow edges soften as the distance between a caster and the receiving surface increases. A caster is the object blocking the light; the receiver is the surface on which its shadow falls.

### How the filter works

1. Search the native shadow map for nearby blockers and estimate their distance from the receiver.
2. Calculate a soft edge radius from that separation and the chosen light-source size. A shadow stays sharp near contact and can become wider farther from its caster.
3. Compare several depths across that radius and average their visibility. This is percentage-closer filtering, or PCF.

The implementation bounds both the search and the final edge radius. If the search misses every blocker, the calculated footprint is smaller than half a shadow texel, or the coordinates are unavailable, it retains the game's original four-tap result. This keeps a sparse search from deleting a thin native shadow. The original four taps run only when that fallback is needed. A valid contact-shadow result that completely blocks direct light skips both PCSS and the native map taps, except when a diagnostic view needs them.

The returned visibility follows the native squared response, cone masks, cloud shadows, and distance fades. Source size is a static tuning input; the actual edge radius is calculated per pixel from current geometry. PCSS is a shadow-map approximation based on average blocker distance, so it does not reproduce every overlap or broad area-light shadow exactly.

### Stochastic sampling and reconstruction

Stochastic sampling uses an irregular, changing set of sample positions to estimate visibility with fewer lookups. A fixed sparse grid can produce visible bands or repeated shapes along a soft edge. Changing the pattern distributes that error across pixels and frames.

Our sample directions follow a precomputed golden-angle spiral. Square-root radial spacing spreads the fixed sample budgets across the search and filter disks.

PCSS reads the existing IS-FAST noise texture shared with GT-VBGI. It contains two channels in a 128x128 tile with 32 temporal layers. One channel rotates the disk; the other shifts samples within their radial bands. The selected swapchain's Present bookkeeping advances the layer, wrapping after 32 phases. The data is precomputed and deterministic, rather than generated anew on the GPU each frame.

This is shadow-sample jitter: the shadow-map lookups move within their search or filter disk. Camera jitter separately offsets the projection for temporal anti-aliasing. Analytical TAA, FSR, or DLSS later combines the resulting scene lighting over time using motion and its own history checks. PCSS has no dedicated shadow-history buffer, temporal accumulator, or denoising pass.

EA's [FAST project](https://github.com/electronicarts/fastnoise) designs sample textures for spatial and temporal filtering. In this integration, reconstruction helps reduce the changing shadow noise. That makes a low sample count useful, but moving objects and newly revealed surfaces can still expose sampling error. The shadow pattern continues to animate with reconstruction Off, so more grain is expected. Four samples across 32 phases do not guarantee 128 independent accepted samples at every surface.

### Light projections

- Sun and moon use the native four-cascade atlas. A cascade is one shadow-map region covering a particular distance range. The filter starts with the native cascade, moves to a coarser one when needed to fit the search, and keeps samples inside the selected tile.
- Spotlights use perspective shadow maps. Distance is reconstructed from the complete matrix so camera rotation and translation do not change the inferred softness.
- Point lights use two curved hemisphere maps, called dual paraboloids. Each sample is projected into the appropriate hemisphere, including samples across their seam.

Depth comparisons account for the receiver's slope to reduce self-shadowing. Sun blocker distances are measured from the actual receiver, separately from that comparison adjustment. If extending the local slope across the filter could reach a detected blocker, the sun filter retains the native result. This avoids letting light through curved or grazing-angle receivers. These details and the native packing are recorded in [NATIVE_LIGHTING.md](NATIVE_LIGHTING.md).

### Controls

Shadows contains Enhanced Shadows (Off / On) and Shadow Softness, which ranges from 0% to the default 100%. Contact reach, the budget, light culling, and diagnostics are in Shadows (Advanced), collapsed by default. Controls are visible while Enhanced Shadows is On. Preset Off disables the feature; Reset All restores its defaults.

Sampling is fixed at 12 sun/moon blocker samples, 4 spot/point blocker samples, and 4 filter samples. Default source sizes are 0.27 degrees for sun/moon and 0.05 game units for local lights. Shadow Softness scales these sizes together without changing light intensity or reflections. At 0%, native map filtering and Bend contact detail remain active. Searches and final edge radii are bounded at 0.25 game units. This bound is not a quality level: a wider search would spread the same samples farther apart and could miss thin casters. Game units have not been calibrated to meters.

Source sizes are deliberate manual inputs. The inspected native local lights have zero in their highlight-size field, while the directional field is an angular proxy that differs substantially from our setting. See [native light sizes](NATIVE_LIGHTING.md#native-light-sizes) for the measurements and limits of that evidence. These fields are not automatically connected to PCSS.

### Cost and supported paths

PCSS runs inside the existing light draw. Bend setup also calculates the sun/spot projection scales once per light and stores five floats in unused dispatch-buffer space. The pixel shader calculates them itself if setup data is unavailable. This adds no dispatches, textures, shadow-map rendering, or separate temporal work. Disk directions are precomputed, disk rotation is calculated once per filter invocation, and point lights rotate their sampling basis once. A search miss or subpixel footprint skips the wider filter. The noise texture is uploaded once per device and reused, including with GT-VBGI Off; its displaced shader binding is restored after each draw. The unshadowed sun path skips PCSS noise binding.

| Light | Supported shaders |
|---|---|
| Sun / Moon | [Shadowed](SunLight_DL_CS_ES_0x5AFD2E17.ps_5_0.hlsl), [unshadowed](SunLight_DL_ES_0x712F7222.ps_5_0.hlsl); the unshadowed variant supports Bend but has no native map to soften |
| Spot | [Ordinary](SSLighting2_Spot_0x08292A5E.ps_5_0.hlsl), [distance variant](SSLighting2_Spot_0x1D4DB9C5.ps_5_0.hlsl) |
| Point | [Dual-paraboloid light](SSLighting2_Point_0x80F0A0BC.ps_5_0.hlsl) |

Shadow-map resolution and caster coverage still limit detail. Filtering cannot recover geometry missing from a native map. Broad footprints, thin foliage, slopes, and cascade boundaries need visual checks. There is no measured performance guarantee across scenes or GPUs.

## Bend contact shadows

[Bend Studio's method](https://www.bendstudio.com/blog/inside-bend-screen-space-shadows/) traces toward each light through camera-visible scene depth, sharing depth reads between nearby pixels. Before each supported light draw, [setup](bend_setup.cs_5_0.hlsl) finds its bounds, an indirect dispatch [clears](bend_clear.cs_5_0.hlsl) that area, and another [traces](bend_trace.cs_5_0.hlsl) the shadow. The [light shader](screen_space_shadows.hlsli) combines that visibility with native map masks and fades before coloring direct light. Fully blocked local-light pixels skip the remaining shading and lookup reads. Required derivatives run before any pixel exits; material and BRDF lookups use explicit level zero because their native textures have one mip. Sun diffuse floors, cloud shadows, environment reflections, and alpha retain their native roles.

PCSS reads depth from the light's viewpoint; Bend reads depth from the camera's viewpoint. Native shadow maps provide offscreen casters, while Bend supplies nearby visible detail they may miss. The [host](screen_space_shadows.hpp) checks the current draw's inputs and restores changed bindings. Missing contact inputs and intentional quality skips leave shadow-map lighting available. See [ARCHITECTURE.md](../docs/ARCHITECTURE.md#screen-space-shadows) for ownership and buffer layouts.

Contact shadows follow Enhanced Shadows and run at full resolution with all sample groups. They need no noise texture or per-light temporal history. Contact Shadow Reach, the spotlight budget, and Skip Unlit Areas are under Shadows (Advanced). Skipped contact tracing still allows PCSS to soften the native shadow map.

| Control | Default | Effect |
|---|---:|---|
| Contact Shadow Reach | 60 px | Search length along the ray's main screen axis, shared by every light; range 16–256 render pixels |
| Spot Contact Shadow Budget | 13 | Limits added spotlight evaluations between lighting-buffer clears; 0 is unlimited |
| Skip Unlit Areas | On | Skips contact-shadow work outside a light's cone or range |

The spotlight budget follows GPU execution order, including deferred replay. It counts evaluations rather than unique lights and does not rank them. Skipped contact work leaves shadow-map filtering active; CPU submission still costs time.

Every lane supplies the casting depths needed by nearby receivers, including lanes outside the current light bounds. Receiver culling happens after shared depth loading. Fully blocked hard contacts skip the farther reduction; other receivers evaluate all four farther-sample groups.

One sample advances roughly one render pixel along the ray's main screen axis. Reach controls search length, not sample density or shadow softness. It is screen-space reach, so its physical distance varies with depth and camera projection. Higher reach reads more shared-depth blocks and checks more potential casters.

Fixed Bend settings are contrast 4, 0.5% surface thickness, a 2% bilinear depth threshold, four hard-contact samples, eight end-fade samples, and 64 threads per group. Thickness is relative to nonlinear depth, rather than game units. The depth threshold stops interpolation across edges. Optional edge discard, precision offset, ray-aligned sampling, and group depth-bound early-out are disabled.

## Debug views and verification

Shadow Map Debug View offers Blocker Distance, Filter Radius, Map Visibility, and Region. Both map and contact diagnostics use the same Debug Light selector. Region identifies the sun cascade or point hemisphere; spotlights have one region. Blocker Distance displays distance / (1 + distance), Filter Radius displays radius / maximum radius, and Map Visibility shows visibility before native masks, squaring, and fades. Cascade colors are red, green, blue, and yellow from nearest to farthest; point hemispheres are red and green. Magenta marks unavailable coordinates.

Contact Shadow Debug View offers Visibility, Depth Edges, Ray Layout, Shadow Detail, and Without Contact Shadows. Debug Light selects Sun / Moon, Spot, or Point. Shadow Detail distinguishes evaluated and skipped areas. Without Contact Shadows hides Bend's result while keeping its calculations running.

Both sets of diagnostics replace or modify lighting within the selected light draw. Surface color, other lights, GI, fog, and later processing still affect the image. Turn debug views Off before judging final appearance or performance.

1. Compare Enhanced Shadows Off/On at a fixed camera with the chosen reconstruction mode. Check contact edges, broad shadows, slopes, foliage, fences, cascade transitions, spotlight rims, and point-light seams.
2. Move the camera and characters, reveal previously hidden surfaces, and check startup, resize, odd dimensions, and different native shadow quality settings. Inspect daylight, moonlight, and overlapping spotlights at Mother Base at night.
3. Compare Shadow Softness at 0%, 50%, and 100%. Check that PCSS changes edge softness while contact detail remains active.
4. At Mother Base at night, compare spotlight budgets including the default 13 and 0 (unlimited). Check which lights lose contact detail and confirm that their shadow maps remain active.

Build the mgsv target and stop the game before deploying. Focused CPU, WARP, hardware, and ReShade fixtures check layouts, settings, projection math, atlas boundaries, and state restoration. They do not replace gameplay checks. The existing native-only RenderDoc captures establish native inputs and order, not current addon execution, history quality, or performance.

The PCSS basis is NVIDIA's [2008 integration guide](https://developer.download.nvidia.com/assets/gamedev/docs/PCSS_Integration.pdf), adapted to MGSV's depth formats and native response. [Native lighting notes](NATIVE_LIGHTING.md) retain measured contracts and sun/moon controller findings. [BEND_LICENSE.md](BEND_LICENSE.md) contains the full Sony notice, also embedded in ReShade's Add-ons tab; the existing EA noise notice is retained in its asset and the Add-ons tab.
