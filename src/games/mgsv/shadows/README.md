# Enhanced Shadows

Enhanced Shadows combines PCSS filtering of native shadow maps with Bend screen-space contact shadows. Both affect direct diffuse and specular lighting inside the game's light draws, before GT-VBGI, skin scattering, and deferred composition. Environment reflections, native material masks, distance fades, and output alpha retain their original roles. Off restores native shadows.

## Files

| Location | Contents |
|---|---|
| [native/](native/) | Replacements for the game's existing light shaders, including the environment-capture variant; these run in native passes |
| [contact/](contact/) | Added Bend setup, clear, and trace compute shaders, with their shared declarations |
| [pcss.hlsli](pcss.hlsli), [screen_space_shadows.hlsli](screen_space_shadows.hlsli) | Shared shadow-map filtering and contact-mask application used by the native replacements |
| [screen_space_shadows.hpp](screen_space_shadows.hpp), [settings.hpp](settings.hpp) | Resource ownership, command recording, state restoration, and controls |

Shader basenames and embed identities stay independent of these folders. Moving a shader does not add a pass or change its registration priority.

## Coverage and placement

The [shader registry](../shaders.hpp) covers all 29 direct-light variants identified in the local dump: five sun/moon variants, 16 spotlight variants, and eight point-light variants. This includes projected masks, diffuse-only lighting, both distance-falloff families, and lights without shadow maps. Shadowed lights use PCSS and Bend; unshadowed lights use Bend alone. The largest-cascade sun shader retains its separate world-shadow mask. Shaders absent from the dump have not been qualified.

The ambient skylight shader, `SSLighting2_SH_SkyLight_Encode_ps` (`0x4FBDBB39`), has a separate role. It samples directional sky illumination and blends it with native diffuse accumulation, then applies the native SH mask. It has no direct-light shadow map or single source direction for PCSS or Bend. Its existing callback identifies ambient inputs for GT-VBGI's diffuse AO; Enhanced Shadows does not darken this contribution again.

For each eligible light, the [host](screen_space_shadows.hpp) records [setup](contact/bend_setup.cs_5_0.hlsl), a bounded [clear](contact/bend_clear.cs_5_0.hlsl), and a depth [trace](contact/bend_trace.cs_5_0.hlsl). The native light draw then [combines](screen_space_shadows.hlsli) contact visibility with its shadow-map result before coloring the light. Missing contact inputs leave shadow-map lighting available. Unsupported replacement bindings retain the original shader.

## Shadow-map filtering

[PCSS](pcss.hlsli), or percentage-closer soft shadows, searches the native shadow map for blockers, estimates their distance from the receiving surface, and filters visibility over a corresponding soft edge. Edges stay tight near contact and widen with caster separation. Source sizes are manual inputs, not measured physical light sizes: 0.27 degrees for sun/moon and 0.05 game units for local lights. Game units have not been calibrated to meters.

- Sun/moon filtering follows the native four-cascade atlas. It selects a coarser cascade when needed to contain the bounded search and keeps samples inside that tile.
- Spotlights use the full perspective matrix to reconstruct blocker distance without treating camera rotation or translation as a change in softness.
- Point lights retain the native dual-paraboloid projection. Samples choose their hemisphere independently, including across the seam.

Receiver-slope comparisons reduce self-shadowing. The sun filter also rejects a slope adjustment that could erase a detected blocker. Searches and final radii are capped at 0.25 game units. A missed blocker, subpixel footprint, or unavailable projection retains the original four-tap filter. Those taps run only when needed. A fully blocked contact result can skip map filtering, except when diagnostics require it. [Native lighting notes](NATIVE_LIGHTING.md) describe the packing, projections, and measured contracts.

### Sampling and reconstruction

PCSS uses golden-angle disk directions with square-root radial spacing. The shared IS-FAST texture rotates the disk and shifts samples within their radial bands: a 128-by-128 tile with 32 temporal layers, uploaded once per device and reused with GT-VBGI Off. Sampling is fixed at 12 sun/moon blocker samples, four local blocker samples, and four filter samples.

The selected swapchain's Present bookkeeping advances the noise layer. This moves shadow-map samples within the filter disk; camera jitter separately offsets the camera projection. Later temporal reconstruction helps stabilize the resulting lighting. There is no dedicated shadow history or denoising pass, and moving objects or newly revealed surfaces can still expose noise. The pattern also animates with reconstruction Off.

## Contact shadows

[Bend's method](https://www.bendstudio.com/blog/inside-bend-screen-space-shadows/) shares camera-depth reads between neighboring receivers. Native maps supply offscreen caster coverage; Bend supplies fine visible detail. Conservative light bounds and cone/range checks skip unlit receivers while retaining the casting depths needed by nearby lit pixels. Spotlight evaluations share a GPU budget; point and directional lights do not consume it. The budget resets beside native lighting-buffer clears, including deferred command-list replay. It counts evaluations in GPU order, without ranking unique lights.

Bend traces toward the native light center. The first solid caster determines a soft-edge estimate using the same source sizes as PCSS. Perspective projection reduces that calculation to screen coordinates. The existing mask stores visibility in R and support plus filter radius in G. The light shader applies a normalized, depth-aware 3-by-3 Gaussian filter only where visibility changes and the radius reaches half a render pixel. It rejects separate surfaces and thin depth peaks, with a maximum radius of two render pixels. Uniform and subpixel contacts keep their raw visibility. This adds no pass, texture, or history buffer.

This bounded filter approximates a finite source; it is not an exact area-light integral. Map and contact visibility are still multiplied. Hidden geometry is unavailable, and thin geometry can change coverage with resolution or camera jitter. Longer reach extends the search without increasing sampling density, so it can expose more screen-space errors. Bend keeps its source centered and adds no animated sampling of its own.

Fixed trace settings are contrast 4, 0.5% relative nonlinear-depth thickness, a 2% bilinear depth threshold, four hard-contact samples, eight end-fade samples, and 64 threads per group. One sample advances roughly one render pixel along the ray's main screen axis. All sample groups run at the current scene resolution. Optional edge discard, precision offset, ray-aligned sampling, and group depth-bound early-out are disabled.

## Controls

| Control | Default | Effect |
|---|---:|---|
| Enhanced Shadows | On | Enables shadow-map filtering and contact shadows together |
| Shadow Softness | 100% | Scales the shared source sizes; 0% keeps native map filtering and sharp contact detail |
| Contact Shadow Reach | 60 px | Shared search length, from 16 to 256 output pixels, converted to render pixels with a minimum of 16 render pixels |
| Spot Contact Shadow Budget | 13 | Limits spotlight contact evaluations between lighting-buffer clears; 0 is unlimited |

Reach, budget, and diagnostics are in Shadows (Advanced), collapsed by default. At DLSS Performance, the minimum trace length corresponds to about 32 output pixels. Reach changes search distance, not softness. Skipped spotlight contacts retain shadow-map filtering. Preset Off disables Enhanced Shadows; Reset All restores defaults.

## Diagnostics and verification

Both debug controls use Debug Light to select Sun / Moon, Spot, or Point. Diagnostics affect the selected light draw, so surface color, other lighting, GI, fog, and later processing can still affect the image. Magenta marks unavailable inputs or coordinates.

- Shadow Map Debug View shows Blocker Distance, Filter Radius, Map Visibility, or Region. Distance uses `distance / (1 + distance)`; radius is relative to its fixed maximum. Sun cascades are red, green, blue, and yellow; point hemispheres are red and green.
- Contact Shadow Debug View shows Visibility, Depth Edges, Ray Layout, Without Contact Shadows, Shadow Detail, or Toward Light. Without Contact Shadows keeps the calculations running for comparisons. Shadow Detail marks evaluated areas. Arrows point toward the native light; cast shadows extend away. The visible moon alone does not establish the selected lighting direction; see the [controller notes](NATIVE_LIGHTING.md#sun-and-moon-controller).

Build the `mgsv` target; stop the game before deploying `renodx-mgsv.addon64`. Turn diagnostics Off for appearance and performance comparisons.

1. Compare Off/On at a fixed camera, then move the camera and characters. Check contact edges, broad shadows, foliage, fences, slopes, cascades, spotlight rims, and point-light seams.
2. Compare softness at 0%, 50%, and 100%, including the ACC menu. Check different reconstruction resolutions, startup, resize, odd dimensions, and native shadow-quality settings. Thin beard geometry can still show instability; this cleanup does not resolve it.
3. At Mother Base at night, compare spotlight budgets 13 and 0. Confirm omitted contacts retain map shadows. Check daylight and moonlight separately, using Toward Light to identify the source when needed.

Local GPU fixtures cover native-light preservation, projection signs, static-input stability, constant visibility, depth boundaries, and thin geometry. These checks do not establish gameplay stability or performance across scenes. Native-only RenderDoc captures establish native inputs and order, not addon execution. The outdoor guardrail/moon direction comparison remains unverified.

## References and licenses

PCSS follows NVIDIA's [2008 integration guide](https://developer.download.nvidia.com/assets/gamedev/docs/PCSS_Integration.pdf), adapted to MGSV's native response. [BEND_LICENSE.md](BEND_LICENSE.md) retains Sony's full notice, also embedded in ReShade's Add-ons tab. The shared [EA FAST noise asset](../assets/noise/fast_noise_gr8.h) retains its notice and Add-ons attribution.
