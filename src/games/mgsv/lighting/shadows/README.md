# Direct lighting

Shadows selects Vanilla or Enhanced. Enhanced combines PCSS filtering of native shadow maps with Bend screen-space contact shadows. Both affect direct diffuse and specular lighting inside the game's light draws, before GT-VBGI, skin scattering, and deferred composition. Environment reflections, native material masks, distance fades, and output alpha retain their original roles. Vanilla restores native shadows.

## Files

| Location | Contents |
|---|---|
| [lighting/native/](../native/) | Replacements for the game's existing light shaders, including the environment-capture variant; these run in native passes |
| [contact/](contact/) | Added Bend setup, clear, and trace compute shaders, with their shared declarations |
| [pcss.hlsli](pcss.hlsli), [screen_space_shadows.hlsli](screen_space_shadows.hlsli) | Shared shadow-map filtering and contact-mask application used by the native replacements |
| [screen_space_shadows.hpp](screen_space_shadows.hpp), [settings.hpp](settings.hpp) | Resource ownership, command recording, state restoration, and controls |
| [batch.hpp](batch.hpp), [local_lights.hpp](../../engine/local_lights.hpp) | The engine hook where local lighting begins, CPU light constants, and the shared compute batch |

## Coverage and placement

The [shader registry](../../shaders.hpp) covers all 29 direct-light variants identified in the local dump: five sun/moon variants, 16 spotlight variants, and eight point-light variants. This includes projected masks, diffuse-only lighting, both distance-falloff families, and lights without shadow maps. Shadowed lights use PCSS and Bend; unshadowed lights use Bend alone. The largest-cascade sun shader retains its separate world-shadow mask. Shaders absent from the dump have not been qualified.

The ambient skylight shader, `SSLighting2_SH_SkyLight_Encode_ps` (`0x4FBDBB39`), has a separate role. It samples directional sky illumination and blends it with native diffuse accumulation, then applies the native SH mask. It has no direct-light shadow map or single source direction for PCSS or Bend. Its existing callback identifies ambient inputs for GT-VBGI's diffuse AO; Enhanced shadows do not darken this contribution again.

The [host](screen_space_shadows.hpp) runs [setup](contact/bend_setup.cs_5_0.hlsl), a bounded [clear](contact/bend_clear.cs_5_0.hlsl), and a depth [trace](contact/bend_trace.cs_5_0.hlsl). All local lights of a lighting pass share one run of these dispatches; the sun runs them once per pass. Each native light draw then [combines](screen_space_shadows.hlsli) contact visibility with its shadow-map result before coloring the light. Missing contact inputs leave shadow-map lighting available. Unsupported replacement bindings retain the original shader.

## Shadow-map filtering

[PCSS](pcss.hlsli), or percentage-closer soft shadows, searches the native shadow map for blockers, estimates their distance from the receiving surface, and filters visibility over a corresponding soft edge. Edges stay tight near contact and widen with caster separation. Base source sizes are 0.27 degrees for sun/moon and 0.05 game units for local lights, scaled by Shadow Softness. These are chosen values; physical light sizes and game units in meters have not been measured.

- Sun/moon filtering follows the native four-cascade atlas. It selects a coarser cascade when needed to contain the bounded search and keeps samples inside that tile.
- Spotlights use the full perspective matrix to reconstruct blocker distance without treating camera rotation or translation as a change in softness.
- Point lights retain the native dual-paraboloid projection. Samples choose their hemisphere independently, including across the seam.

Receiver-slope comparisons reduce self-shadowing. The sun filter also rejects a slope adjustment that could erase a detected blocker. Searches and final radii are capped at 0.25 game units. A missed blocker, subpixel footprint, or unavailable projection retains the original four-tap filter. Those taps run only when needed. A fully blocked contact result can skip map filtering, except when diagnostics require it. [Native lighting notes](NATIVE_LIGHTING.md) describe the packing, projections, and measured contracts.

### Sampling and reconstruction

PCSS uses golden-angle disk directions with square-root radial spacing. The shared IS-FAST texture rotates the disk and shifts samples within their radial bands: a 128-by-128 tile with 32 temporal layers, uploaded once per device and reused with GT-VBGI Off. Sampling is fixed at 12 sun/moon blocker samples, four local blocker samples, and four filter samples.

The selected swapchain's Present bookkeeping advances the noise layer. This moves shadow-map samples within the filter disk; camera jitter separately offsets the camera projection. Later temporal reconstruction helps stabilize the resulting lighting. There is no dedicated shadow history or denoising pass, and moving objects or newly revealed surfaces can still expose noise. The pattern also animates with reconstruction Off.

## Contact shadows

[Bend's method](https://www.bendstudio.com/blog/inside-bend-screen-space-shadows/) shares camera-depth reads between neighboring receivers. Native maps supply offscreen caster coverage; Bend supplies fine visible detail. Conservative light bounds and cone/range checks skip unlit receivers while retaining the casting depths needed by nearby lit pixels. Spotlight evaluations share a GPU budget; point and directional lights do not consume it. The budget resets beside native lighting-buffer clears, including deferred command-list replay. It counts evaluations in GPU order, without ranking unique lights.

### Local-light batches

The [local-light hook](../../engine/local_lights.hpp) runs when the engine's local-light plugin begins on its build thread, before it branches the jobs that draw unshadowed and shadowed lights. There the [batch](batch.hpp) cuts the build thread's command list and reserves its dispatches at that point. The engine executes that list before every light job's list, and only after all of them have been recorded.

Every local light draw that writes lighting, with or without a shadow map, then adds its light to the batch: the CPU constants its worker has just uploaded, its type, and for shadowed lights its shadow-map projection. The batch uploads all lights when its dispatches execute, so lights recorded after the cut are included. All lights of a batch share depth, lighting target, settings, and camera; the first light fixes them.

Setup builds every light's record in parallel, then admits lights in the order their draws execute: each takes atlas space for its receiver bounds, and spotlights count against the budget. Clear and trace then process all admitted rectangles in parallel, and each light draw reads its own record and packed rectangle. The clear is required because the trace skips ray bundles that miss the light's range and leaves their texels unwritten. Replaying a command list recomputes the masks from that execution's depth; uploaded inputs and GPU resources remain alive until their recorded work is retired.

The sun records a one-light batch beside its first draw of a pass, copying its constants from that draw's bound buffers; the pass's later sun draws reuse its mask. A local light the frame batch cannot take records a one-light batch the same way and logs the reason once. One atlas allocation stays below 256 MiB and the D3D11 texture-size limit. It holds 13 screens of masks, or as many as the spotlight budget, and a light that no longer fits keeps its shadow-map result. On an executable whose hooks cannot be verified, Enhanced shadows keep the native light shaders.

### Soft contact edges

Bend traces toward the native light center. The first solid caster determines a soft-edge estimate using the same source sizes as PCSS. Perspective projection reduces that calculation to screen coordinates. The existing mask stores visibility in R and support plus filter radius in G. The light shader applies a normalized, depth-aware 3-by-3 Gaussian filter only where visibility changes and the radius reaches half a render pixel. It rejects separate surfaces and thin depth peaks, with a maximum radius of two render pixels. Uniform and subpixel contacts keep their raw visibility. This adds no pass, texture, or history buffer.

This bounded filter approximates a finite source; it is not an exact area-light integral. Map and contact visibility are still multiplied. Hidden geometry is unavailable, and thin geometry can change coverage with resolution or camera jitter. Longer reach extends the search without increasing sampling density, so it can expose more screen-space errors. Bend keeps its source centered and adds no animated sampling.

Fixed trace settings are contrast 4, 0.5% relative nonlinear-depth thickness, a 2% bilinear depth threshold, four hard-contact samples, four interleaved soft-sample minima, eight end-fade samples, and 64 threads per group. One sample advances roughly one render pixel along the ray's main screen axis. All sample groups run at the current scene resolution. The sample counts and the neighbor-depth bias are tuned in output pixels: below about 71% render scale the trace uses two hard samples, two minima, and four fade samples, and below about 35% one hard sample and one minimum, while the bias scales directly with render scale. Without that, thin casters lost most of their shadow in upscaled modes. Casters thinner than a render pixel still appear in only some jittered frames, so their accumulated shadow stays lighter than at native resolution. Optional edge discard, precision offset, ray-aligned sampling, and group depth-bound early-out are disabled.

## Controls

| Control | Default | Effect |
|---|---:|---|
| Shadows | Enhanced | Selects native shadows or enhanced shadow-map filtering and contact shadows together |
| Shadow Softness | 100% | Scales the shared source sizes; 0% keeps native map filtering and sharp contact detail |
| Contact Shadow Reach | 0.10 | Shared search length toward the light in game units, from 0.05 to 2; each pixel converts it to screen pixels at its own depth, between 16 and 256 render pixels |
| Spot Contact Shadow Budget | 34 | Limits spotlight contact evaluations between lighting-buffer clears; 0 is unlimited |

Softness, reach, budget, and diagnostics are in Direct Lighting (Advanced), collapsed by default. A local light's center sits inside its lamp, so each trace stops 0.25 game units short of it; closer casters are taken to be the light's own fixture, which the engine's shadow maps leave out too, and receivers that close to the light stay lit. Because reach is a world distance, a contact shadow keeps its length as the camera moves closer or farther; very close to a surface the 256-sample limit shortens it, and far away the 16-sample minimum lengthens it. The earlier pixel reach (`DirectLightingContactShadowReachPixels`) is no longer read; its 60 pixels were only about 2 cm at the ACC title's close camera. Longer reach costs more where the camera is close, since each pixel then needs more samples. Reach changes search distance, not softness. Skipped spotlight contacts retain shadow-map filtering. Preset Off selects Vanilla for Shadows; Reset All restores defaults.

## Diagnostics and verification

Both debug controls use Debug Light to select Sun / Moon, Spot, or Point. Diagnostics affect the selected light draw, so surface color, other lighting, GI, fog, and later processing can still affect the image. Magenta marks unavailable inputs or coordinates.

- Shadow Map Debug View shows Blocker Distance, Filter Radius, Map Visibility, or Region. Distance uses `distance / (1 + distance)`; radius is relative to its fixed maximum. Sun cascades are red, green, blue, and yellow; point hemispheres are red and green.
- Contact Shadow Debug View shows Visibility, Depth Edges, Ray Layout, Without Contact Shadows, Shadow Detail, or Toward Light. Without Contact Shadows keeps the calculations running for comparisons. Shadow Detail marks evaluated areas. Arrows point toward the native light; cast shadows extend away. The visible moon alone does not establish the selected lighting direction; see the [controller notes](NATIVE_LIGHTING.md#sun-and-moon-controller).

Follow the [addon build and deployment checks](../../README.md#build-and-deploy). Turn diagnostics Off for appearance and performance comparisons.

1. Compare Vanilla/Enhanced at a fixed camera, then move the camera and characters. Check contact edges, broad shadows, foliage, fences, slopes, cascades, spotlight rims, and point-light seams.
2. Compare softness at 0%, 50%, and 100%, including the ACC menu. Check different reconstruction resolutions, startup, resize, odd dimensions, and native shadow-quality settings. Thin beard geometry can still show instability.
3. At Mother Base at night, compare spotlight budgets 13 and 0. Confirm omitted contacts retain map shadows. Check daylight and moonlight separately, using Toward Light to identify the source when needed.

In-game checks at Mother Base at night, at Native and DLSS Performance: one batch covered all 495 local light draws of a pass, including 16 shadowed spotlights; each traced light's record matched the projection from its draw's GPU constants within 0.0001 render pixel; and the parallel setup admitted the same lights, with the same bounds, as the earlier serial setup. The outdoor guardrail/moon direction comparison remains unverified. Use a fixed-camera Enhanced / Vanilla / Enhanced comparison for performance, with debug views and capture copies disabled.

## References and licenses

PCSS follows NVIDIA's [2008 integration guide](https://developer.download.nvidia.com/assets/gamedev/docs/PCSS_Integration.pdf), adapted to MGSV's native response. [BEND_LICENSE.md](BEND_LICENSE.md) retains Sony's full notice, also embedded in ReShade's Add-ons tab. The shared [EA FAST noise asset](../../assets/noise/fast_noise_gr8.h) retains its notice and Add-ons attribution.
