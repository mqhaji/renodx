# Screen-space shadows

These checks add small contact shadows from camera-visible depth inside supported sun, spot, and point lighting draws. They supplement shadow maps and have independent per-light-type controls under Screen-Space Shadows. They are unrelated to subsurface scattering and the later GT-VBGI trace.

| Light | Shader | Ray direction in view space |
|---|---|---|
| Sun | [SunLight_DL_CS_ES](SunLight_DL_CS_ES_0x5AFD2E17.ps_5_0.hlsl) | The current light's `m_lightParams[0].xyz` |
| Spot | [SSLighting2_Spot](SSLighting2_Spot_0x08292A5E.ps_5_0.hlsl), [distance variant](SSLighting2_Spot_0x1D4DB9C5.ps_5_0.hlsl) | From the receiver toward `m_lightParams[3].xyz` |
| Point | [SSLighting2_Point](SSLighting2_Point_0x80F0A0BC.ps_5_0.hlsl) | From the receiver toward `m_lightParams[3].xyz` |

## Geometry search

1. Reconstruct the receiver from native scene depth and the lighting draw's projection.
2. Convert the configured screen reach to a depth-dependent view-space distance, bounded by the shader's reach cap.
3. Project the ray start and step once, then sample depth along a jittered path with denser steps near the receiver.
4. Accumulate accepted occlusion within a thickness allowance that includes receiver slope bias. Fade with distance and stop when the ray leaves the screen or further samples cannot change the result.
5. Attenuate the current light contribution before writing the additive lighting target. Never write visibility itself as an additive light contribution.

Material limits check both the receiving surface and potential casters. This excludes some material categories from self-shadowing. The search also skips pixels beyond its fade distance or with no light left to darken.

## Important conventions

- Native scene depth is reverse-Z; shadow-map comparisons have their own convention. Preserve each path's reconstruction and depth tests.
- The sun ray uses the light rendered by that draw. The scene's `m_dominantLightDir` can be selected from another light and is unsuitable for this purpose.
- Interleaved gradient noise remains the shadow march's jitter source. The hardcoded IS-FAST texture belongs to GT-VBGI.
- Screen-space depth cannot represent hidden/offscreen casters or complete surface thickness. These shadows cannot replace the native maps.

## Verification

Compare each light type Off/On on grass, slopes, contact edges, characters, and ground receivers. Check material-limit behavior, light/cone boundaries, camera motion, and screen edges. A stronger thickness or reach can hide gaps while introducing false shadows.

Shared accumulation-buffer readbacks include other lights, and final-image brightness can change with exposure. Isolate the relevant draw or compare controlled scene output. Command recording, a stable frame cap, and the existing native-only captures do not measure the replacement's GPU cost.
