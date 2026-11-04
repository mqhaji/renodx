# Shadows

Soft Shadows filters the game's shadow maps for sun, moon, spot, and point lights. Bend contact shadows add fine detail from geometry visible to the camera. The two features meet in the native light shader, before subsurface scattering, GT-VBGI, and deferred composition. Each affects direct diffuse and specular light while preserving the native environment-reflection contribution and output alpha.

## Soft shadows

The default-on Soft Shadows switch enables every supported PCSS filter. PCSS means percentage-closer soft shadows: it estimates how much of a light source is visible and lets shadow edges soften as the distance between a caster and the receiving surface increases. A caster is the object blocking the light; the receiver is the surface on which its shadow falls.

### How the filter works

1. Search the native shadow map for nearby blockers and estimate their distance from the receiver.
2. Calculate a soft edge radius from that separation and the chosen light-source size. A shadow stays sharp near contact and can become wider farther from its caster.
3. Compare several depths across that radius and average their visibility. This is percentage-closer filtering, or PCF.

The implementation bounds both the search and the final edge radius. If the search misses every blocker, the calculated footprint is smaller than half a shadow texel, or the coordinates are unavailable, it retains the game's original four-tap result. This keeps a sparse search from deleting a thin native shadow. The original four taps are still calculated for that fallback.

The returned visibility follows the native squared response, cone masks, cloud shadows, and distance fades. Source size is a static tuning input; the actual edge radius is calculated per pixel from current geometry. PCSS is a shadow-map approximation based on average blocker distance, so it does not reproduce every overlap or broad area-light shadow exactly.

### Stochastic sampling and reconstruction

Stochastic sampling uses an irregular, changing set of sample positions to estimate visibility with fewer lookups. A fixed sparse grid can produce visible bands or repeated shapes along a soft edge. Changing the pattern distributes that error across pixels and frames.

Our sample directions follow a precomputed golden-angle spiral. Square-root radial spacing spreads the samples across the disk's area. The current sample count determines that spacing: reducing the count keeps the same search and filter footprints, with fewer samples covering them.

PCSS reads the existing IS-FAST noise texture shared with GT-VBGI. It contains two channels in a 128x128 tile with 32 temporal layers. One channel rotates the disk; the other shifts samples within their radial bands. The selected swapchain's Present bookkeeping advances the layer, wrapping after 32 phases. The data is precomputed and deterministic, rather than generated anew on the GPU each frame.

This is shadow-sample jitter: the shadow-map lookups move within their search or filter disk. Camera jitter separately offsets the projection for temporal anti-aliasing. Analytical TAA, FSR, or DLSS later combines the resulting scene lighting over time using motion and its own history checks. PCSS has no dedicated shadow-history buffer, temporal accumulator, or denoising pass.

EA's [FAST project](https://github.com/electronicarts/fastnoise) designs sample textures for spatial and temporal filtering. In this integration, reconstruction helps reduce the changing shadow noise. That makes a low sample count useful, but moving objects and newly revealed surfaces can still expose sampling error. The shadow pattern continues to animate with reconstruction Off, so more grain is expected. Four samples across 32 phases do not guarantee 128 independent accepted samples at every surface.

### Light projections

- Sun and moon use the native four-cascade atlas. A cascade is one shadow-map region covering a particular distance range. The filter starts with the native cascade, moves to a coarser one when needed to fit the search, and keeps samples inside the selected tile.
- Spotlights use perspective shadow maps. Distance is reconstructed from the complete matrix so camera rotation and translation do not change the inferred softness.
- Point lights use two curved hemisphere maps, called dual paraboloids. Each sample is projected into the appropriate hemisphere, including samples across their seam.

Depth comparisons account for the receiver's slope to reduce self-shadowing. Sun blocker distances are measured from the actual receiver, separately from that comparison adjustment. If extending the local slope across the filter could reach a detected blocker, the sun filter retains the native result. This avoids letting light through curved or grazing-angle receivers. These details and the native packing are recorded in [NATIVE_LIGHTING.md](NATIVE_LIGHTING.md).

### Controls

The main Contact-Hardening Shadows section contains one Soft Shadows switch. All tuning and debug controls are in Contact-Hardening Shadows (Advanced), collapsed by default and visible while Soft Shadows is On. Preset Off disables the feature; Reset All restores its defaults.

| Control | Default | Purpose |
|---|---:|---|
| Sun / Moon Angular Radius | 0.27 degrees | Sets the apparent source size for directional shadows |
| Spot Light Radius | 0.05 game units | Sets source size for spot shadows |
| Point Light Radius | 0.05 game units | Sets source size for point shadows |
| Maximum Soft Edge Radius | 0.25 game units per light type | Bounds search reach and final edge width |
| Blocker Samples | 12 for Sun / Moon; 4 for Spot and Point | Controls the reliability of the blocker-distance estimate |
| Filter Samples | 4 per light type | Controls noise in the filtered visibility |
| Debug View | Off per light type | Shows blocker distance, radius, visibility, or map region |

Both sample controls accept typed values, with up to 32 samples per loop. Sun / Moon uses 12 blocker samples to better detect thin casters such as wires. All light types use 4 filter samples; Spot and Point also use 4 blocker samples. Higher counts can help thin casters and broad edges.

Maximum Soft Edge Radius is a bound, not a quality level. Start at 0.25, choose the desired softness with Angular Radius or Light Radius, then raise the maximum only if it restricts that result. A larger search spreads the same samples farther apart and can miss thin geometry or become less stable. Sample counts are the main cost controls; footprint size can also affect filtering early-outs and texture-cache behavior. Game units have not been calibrated to meters.

Source sizes are deliberate manual inputs. The inspected native local lights have zero in their highlight-size field, while the directional field is an angular proxy that differs substantially from our setting. See [native light sizes](NATIVE_LIGHTING.md#native-light-sizes) for the measurements and limits of that evidence. These fields are not automatically connected to PCSS.

### Cost and supported paths

PCSS runs inside the existing light draw. It adds no dispatches, shadow textures, shadow-map rendering, or separate temporal work. Disk directions are precomputed, disk rotation is calculated once per filter invocation, and point lights rotate their sampling basis once. A search miss or subpixel footprint skips the wider filter. The noise texture is uploaded once per device and reused, including with GT-VBGI Off; its displaced shader binding is restored after each draw. The unshadowed sun path skips PCSS noise binding.

| Light | Supported shaders |
|---|---|
| Sun / Moon | [Shadowed](SunLight_DL_CS_ES_0x5AFD2E17.ps_5_0.hlsl), [unshadowed](SunLight_DL_ES_0x712F7222.ps_5_0.hlsl); the unshadowed variant supports Bend but has no native map to soften |
| Spot | [Ordinary](SSLighting2_Spot_0x08292A5E.ps_5_0.hlsl), [distance variant](SSLighting2_Spot_0x1D4DB9C5.ps_5_0.hlsl) |
| Point | [Dual-paraboloid light](SSLighting2_Point_0x80F0A0BC.ps_5_0.hlsl) |

Shadow-map resolution and caster coverage still limit detail. Filtering cannot recover geometry missing from a native map. Broad footprints, thin foliage, slopes, and cascade boundaries need visual checks. There is no measured performance guarantee across scenes or GPUs.

## Bend contact shadows

[Bend Studio's method](https://www.bendstudio.com/blog/inside-bend-screen-space-shadows/) traces toward each light through camera-visible scene depth, sharing depth reads between nearby pixels. Before each supported light draw, [setup](bend_setup.cs_5_0.hlsl) finds its bounds, an indirect dispatch [clears](bend_clear.cs_5_0.hlsl) that area, and another [traces](bend_trace.cs_5_0.hlsl) the shadow. The [light shader](screen_space_shadows.hlsli) applies that visibility to direct lighting alongside the shadow-map result.

PCSS reads depth from the light's viewpoint; Bend reads depth from the camera's viewpoint. Native shadow maps provide offscreen casters, while Bend supplies nearby visible detail they may miss. The [host](screen_space_shadows.hpp) checks the current draw's inputs and restores changed bindings. Missing inputs and intentional quality skips retain native lighting. See [ARCHITECTURE.md](../docs/ARCHITECTURE.md#screen-space-shadows) for ownership and buffer layouts.

The three contact-shadow switches default to On. Their sampling and performance controls are under Screen-Space Shadows (Advanced).

| Control | Default | Effect |
|---|---:|---|
| Sun / Moon, Spot, Point Samples | 60 each | Sets search reach and sample count in render pixels |
| Contact Shadow Contrast | 4 | Darkens and sharpens contact shadows |
| Spot Contact Shadow Budget | 10 | Limits added spotlight evaluations between lighting-buffer clears; 0 is Unlimited |
| Local Shadow Sampling | Full | Evaluates all pixels, 1/2 with checkerboard, or 1/4 by halving both axes |
| Skip Unlit Areas | On | Skips contact-shadow work outside a light's cone or range |
| Local Shadow Distance | 0 | Limits forward camera depth in game units; 0 is Unlimited |
| Local Shadow Fade Range | 25% | Fades over the last part of the distance limit; distance 40 starts fading at 30 |
| Minimum Light Coverage | 0% | Skips small projected bounds and fades to full detail at twice the threshold; 0 disables the limit |

The spotlight budget follows GPU execution order, including deferred replay. It counts evaluations rather than unique lights and does not rank them. Skipped lights keep native shadows; CPU submission still costs time.

Checkerboard retains full-grid casting depths and reconstructs skipped pixels, so it can cost about as much as Full. The 1/4 path uses nearest depth from each 2x2 block and can shimmer in motion. Both reconstruct from current depth and light bounds, without per-light temporal history. Sun / Moon always uses Full.

Fixed Bend settings are 0.5% surface thickness, a 2% bilinear depth threshold, four hard-contact samples, eight end-fade samples, and 64 threads per group. Thickness is relative to nonlinear depth, rather than game units. The depth threshold stops interpolation across edges. Optional edge discard, precision offset, ray-aligned sampling, and group depth-bound early-out are disabled.

## Debug views and verification

Soft-shadow debug views offer Blocker Distance, Filter Radius, and Map Visibility. Sun / Moon also offers Cascade; Point also offers Hemisphere. Blocker Distance displays distance / (1 + distance), Filter Radius displays radius / maximum radius, and Map Visibility shows visibility before native masks, squaring, and fades. Cascade colors are red, green, blue, and yellow from nearest to farthest; point hemispheres are red and green. Magenta marks unavailable coordinates.

Contact Shadow Debug View offers Visibility, Depth Edges, Ray Layout, Shadow Detail, and Without Contact Shadows. Debug Light selects Sun / Moon, Spot, or Point. Shadow Detail distinguishes evaluated and skipped areas. Without Contact Shadows hides Bend's result while keeping its calculations running.

Both sets of diagnostics replace or modify lighting within the selected light draw. Surface color, other lights, GI, fog, and later processing still affect the image. Turn debug views Off before judging final appearance or performance.

1. Compare Soft Shadows Off/On at a fixed camera with the chosen reconstruction mode, first with contact shadows Off and then On. Check contact edges, broad shadows, slopes, foliage, fences, cascade transitions, spotlight rims, and point-light seams.
2. Move the camera and characters, reveal previously hidden surfaces, and check startup, resize, odd dimensions, and different native shadow quality settings. Inspect daylight, moonlight, and overlapping spotlights at Mother Base at night.
3. Compare Full, 1/2, and 1/4 contact-shadow sampling with budget, distance, and coverage limits disabled. Then test those limits separately. Check for lost shadows, cutoff lines, and motion shimmer.

Build the mgsv target and stop the game before deploying. Focused CPU, WARP, hardware, and ReShade fixtures check layouts, settings, projection math, atlas boundaries, and state restoration. They do not replace gameplay checks. The existing native-only RenderDoc captures establish native inputs and order, not current addon execution, history quality, or performance.

The PCSS basis is NVIDIA's [2008 integration guide](https://developer.download.nvidia.com/assets/gamedev/docs/PCSS_Integration.pdf), adapted to MGSV's depth formats and native response. [Native lighting notes](NATIVE_LIGHTING.md) retain measured contracts and sun/moon controller findings. [BEND_LICENSE.md](BEND_LICENSE.md) contains the full Sony notice, also embedded in ReShade's Add-ons tab; the existing EA noise notice is retained in its asset and the Add-ons tab.
