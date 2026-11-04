# Shadows

MGSV has two shadow enhancements: soft filters for the game's shadow maps and Bend contact shadows from camera-visible geometry. Both support sun, moon, spot, and point lights, with separate switches for each light type. GT-VBGI handles indirect lighting separately.

## Soft shadows

The Contact-Hardening Shadows section controls how shadow edges soften away from the object casting them. The current filters estimate softness with nine nearby samples, then use a rotated 32-sample disk where needed. They retain the game's shadow response, cascade transitions, and light-specific fades.

Sun shadows use a cascaded atlas, spotlights use perspective maps, and point lights use two paraboloid maps. These are approximate filters; their softness controls are not calibrated physical light sizes. Softness Response changes how quickly the filter widens, Softness Smoothing blends neighboring estimates, and the local-light controls also adjust darkness and spotlight cone fade.

| Light | Supported shaders |
|---|---|
| Sun / Moon | [Shadowed](SunLight_DL_CS_ES_0x5AFD2E17.ps_5_0.hlsl), [unshadowed](SunLight_DL_ES_0x712F7222.ps_5_0.hlsl); only the shadowed variant has a native map to soften |
| Spot | [Ordinary](SSLighting2_Spot_0x08292A5E.ps_5_0.hlsl), [distance variant](SSLighting2_Spot_0x1D4DB9C5.ps_5_0.hlsl) |
| Point | [Dual-paraboloid light](SSLighting2_Point_0x80F0A0BC.ps_5_0.hlsl) |

## Contact shadows

[Bend Studio's method](https://www.bendstudio.com/blog/inside-bend-screen-space-shadows/) traces toward each light through scene depth, sharing depth reads between nearby pixels. Before each supported light draw, [setup](bend_setup.cs_5_0.hlsl) finds its bounds, an indirect dispatch [clears](bend_clear.cs_5_0.hlsl) that area, and another [traces](bend_trace.cs_5_0.hlsl) the shadow. The [light shader](screen_space_shadows.hlsli) applies it to direct diffuse and specular light, preserving environment reflections and native alpha.

The [host](screen_space_shadows.hpp) uses the current draw's inputs and restores changed bindings. Missing inputs and intentional quality skips retain the game's lighting. Native shadow maps remain responsible for hidden and offscreen geometry. See [ARCHITECTURE.md](../docs/ARCHITECTURE.md#screen-space-shadows) for resource ownership and buffer layouts.

The three contact-shadow switches default to On. Sampling and performance controls are under Screen-Space Shadows (Advanced).

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

Full is the default. Checkerboard retains full-grid casting depths and reconstructs skipped pixels, so it can cost about as much as Full. The 1/4 path uses nearest depth from each 2x2 block and can shimmer in motion. Both reconstruct using current depth and light bounds, with no per-light temporal history. Sun / Moon always uses Full.

Fixed Bend settings are 0.5% surface thickness, a 2% bilinear depth threshold, four hard-contact samples, eight end-fade samples, and 64 threads per group. Thickness is relative to nonlinear depth, not meters or the GT-VBGI thickness setting. The depth threshold stops interpolation across edges. Optional edge discard, precision offset, ray-aligned sampling, and group depth-bound early-out are disabled.

## Debug views and testing

Contact Shadow Debug View offers Visibility, Depth Edges, Ray Layout, and Shadow Detail. Without Contact Shadows hides the result while keeping its calculations running. Debug Light selects Sun / Moon, Spot, or Point. Magenta means diagnostic data is unavailable. Other lights, surface color, fog, and later processing still affect the image.

1. Compare each soft-shadow and contact-shadow switch separately, then together. Check contact edges, slopes, cascade transitions, spotlight rims, point-light seams, grass, hair, and screen edges.
2. Test camera rotation, moving objects, overlapping lights, cold startup, resize, odd dimensions, different native shadow quality settings, and reconstruction Off/On.
3. Compare Full, 1/2, and 1/4 with budget, distance, and coverage limits at 0. Then check budget 10 and Skip Unlit Areas separately. Look for missing shadows, cutoff lines, and motion shimmer.
4. Compare Without Contact Shadows against the selected light switch Off. Direct highlights should receive contact shadows; environment reflections and native alpha should retain their existing behavior.

Build the `mgsv` target; stop the game before deploying. Build and fixture results do not replace gameplay checks. The existing native-only On/Off captures cannot establish custom shadow quality or performance.

[Native lighting notes](NATIVE_LIGHTING.md) retain projection measurements, foliage findings, and sun/moon controller addresses. [BEND_LICENSE.md](BEND_LICENSE.md) contains the full Sony notice, also embedded in ReShade's Add-ons tab.
