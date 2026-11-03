# MGSV RenoDX

RenoDX for Metal Gear Solid V: The Phantom Pain adds HDR output and color grading, higher-precision textures, temporal reconstruction, improved shadows, ambient occlusion (AO), global illumination (GI), and specular occlusion.

## Temporal reconstruction

**Temporal Reconstruction** selects Off (Vanilla FXAA), Analytical TAA, AMD FSR 3.1.5, or NVIDIA DLSS. These methods combine samples from several frames to smooth edges, all at the game's render resolution. New profiles default to FSR3. Off restores the original camera projection and FXAA. Enabled methods skip FXAA filtering while keeping the draw's other scene processing.

Reconstruction settings stay independent of RenoDX presets. DLSS requires a compatible `nvngx_dlss.dll` beside the game executable; its default model is F when the DLL supports it. Character wobble and some special-view flicker remain unresolved. See the [reconstruction guide](reconstruction/README.md) for controls, startup and shutdown behavior, and testing.

## Indirect Lighting

**Screen-Space Lighting** selects Off or GT-VBGI. One search through scene depth supplies AO contact shading, GI light bouncing from visible surfaces, and specular visibility for environment reflections. AO, GI, and Specular Occlusion have separate strengths; resolution and sampling controls are shared.

Defaults are GT-VBGI, all three strengths at 75%, Quarter resolution, a 2000-pixel radius, 40 samples per side, three sampling directions, surface thickness 0.15, and surface offset 1/1024 in the game's view-space units. Full/Half/Quarter/Eighth scale both image dimensions, so Quarter processes 1/16 as many pixels. Coarse depth sampling and GI backface rejection default to Off; reflections contribute zero to the GI source by default. Noise uses IS-FAST.

Surface Thickness controls the assumed depth of sampled geometry. Search Radius changes reach; samples and directions control coverage quality. **Specular Occlusion Strength** blends from native environment occlusion at 0% to the bitmask estimate at 100%, reduced where sampling is unreliable. It can darken blocked reflections or restore reflections that native AO darkens too much. **Reject Backfacing GI Sources** suppresses source light while retaining its occlusion. **Reflections in GI Source** adds a view-dependent specular approximation to the bounce source.

**Temporal Accumulation** and **Spatial Filtering** control AO, GI, and specular visibility together, both On by default. AO and specular visibility share history weight and depth/normal rejection controls, with separate history clamps; GI has its own controls. Setting a history weight to zero uses only current samples for those signals. Defaults are 90% history weight and three 5-by-5 A-Trous passes with radius 8. Radius only affects later A-Trous passes. The separable Gaussian option filters horizontally and vertically.

**Lighting Debug View** shows lighting, coverage, histories, native roughness, captured environment reflections, and raw/temporal/filtered specular visibility. Specular Confidence shows how much the custom estimate can replace native occlusion. Magenta marks missing diagnostic inputs; cyan in specular visibility means the sampled slices missed the reflection cone. Surface color, specular reflections, fog, and later image processing still affect these views.

The added lighting runs after the game's skin-scattering passes. Its final pass applies AO to existing diffuse light, adds GI, and resolves specular visibility while preserving the game's alpha channel. Deferred composition then applies the environment-reflection correction before fog and tonemapping. All three signals share the trace and filters; the default chain uses six compute dispatches, with four more for optional depth reductions. Missing temporal inputs use current samples, and histories retain the temporal results before spatial filtering. See the [lighting reference](gtvbgi/README.md) for the algorithms and MGSV adaptation.

### Native AO compatibility

With Screen-Space Lighting enabled, the six composition shaders skip the game's AO on diffuse light because custom AO has already darkened it. Bitmask specular visibility replaces native AO only on supported environment reflections where captured data and confidence permit. Direct highlights, unsupported reflections, and separate rain reflections keep their native AO treatment. Off restores the original composition. The game still calculates its AO in either mode.

The diffuse bypass follows the setting: if custom lighting cannot run while enabled, diffuse receives neither the game's AO nor custom AO at that insertion. Custom AO darkens all accumulated diffuse light, including direct lighting; GI is added afterward. Applying diffuse AO selectively to ambient light remains future work.

Preset Off disables combined lighting and the six shadow enhancement switches while leaving reconstruction selected. Reset All restores defaults.

## Shadows

Contact-hardening shadows soften supported sun, spot, and point shadows as estimated blocker separation grows. Screen-space shadows supplement shadow maps with nearby visible geometry. These are replacements inside existing lighting draws, with independent controls for each light type.

See the [sun](shadows/MGSV_SunShadow_ContactHardening.md), [spot](shadows/MGSV_SpotLight_ContactHardening.md), and [screen-space shadow](shadows/MGSV_ScreenSpaceShadows.md) notes for native bindings, filtering, and limitations.

## Precision and rendering structure

Precision upgrades work independently of HDR output and reconstruction selection. They replace selected textures with higher-precision versions, called clones. A float texture can still contain encoded color; each shader must use the matching color conversion.

| Resource | Handling |
|---|---|
| Scene color | Recognized composition and tonemap outputs use RGBA16F clones |
| Depth of field | The checked scene copy before final DoF composition stays upgraded through that composition |
| Motion blur | Checked input copies and alternating blur textures keep float color; copy-back ends their redirection |
| Object and composed velocity | RGBA16F preserves the game's motion packing and precision even with reconstruction Off |
| Color-grading LUT | Native 256x16 builder storage is upgraded to RGBA16F |

Generic copy shader `0x83272BCB` is upgraded only when the surrounding draws establish that it is copying a supported scene image. MGSV also uses it for unrelated textures. Present clears the per-frame tracking while clone textures can remain allocated for reuse.

The [architecture guide](docs/ARCHITECTURE.md) explains which files manage each feature, rendering order, required input formats, and what has been verified. The [shader name map](docs/MGSV_PixelShader_NameMap.md) and [Fox Engine reference](docs/Fox_Engine_Render_Pipeline_Reference.md) retain findings from inspecting the game.

## Building & Deployment

Build the `mgsv` target through CMake Tools with the approved configuration and MGSV fully stopped. Deploy the matching `renodx-mgsv.addon64` beside `mgsvtpp.exe`, then restart. Camera hooks stay installed until the game exits, so do not unload or replace the addon while playing. Replace the DLSS DLL only with the game stopped too.

After shader or shared-layout changes, verify that generated headers match the compiled shaders and that the deployed addon contains those bytes. The lighting outputs are `gtvbgi`, `gtvbgi_depth_mips`, `gtvbgi_temporal`, `gtvbgi_filter`, and `gtvbgi_add` under `build/mgsv.include/embed`. A successful build or automated test still needs gameplay checks.

## Manual validation

1. Confirm RenoDX loads in ReShade and saved settings survive restart. Exercise Reset All and Preset Off; reconstruction must remain independently selected.
2. Compare Screen-Space Lighting Off/GT-VBGI with the camera and other settings fixed. Vary AO, GI, and Specular Occlusion strengths independently. Check contact shading, color bounce, rough and smooth reflections, hair, foliage, and screen edges. Specular strength 0% should restore native environment occlusion. Check transitions into low-confidence regions, which retain native occlusion.
3. Check Full/Half/Quarter/Eighth and coarse depth sampling Off/On, including odd render dimensions. Look for shifted output, edge strips, leaks, and loss of thin geometry.
4. Check fog, rain, skin scattering, SDR/HDR, camera motion, and reconstruction Off/On. Direct highlights and separate rain reflections should retain their existing treatment. Check cold startup, gameplay, and returning to the menu for missing or stale specular data.
5. Compare temporal and spatial filtering separately and together. Set either history weight to zero to check the other signal independently. Check stationary noise, moving characters, newly revealed surfaces, exposure changes, and the temporal/filtered/history debug views. Toggling filtering should not shift the image or change overall brightness.
6. Follow the [reconstruction validation matrix](reconstruction/README.md#manual-validation) for method changes, animation, special views, and lifecycle checks.
7. Read the fresh ReShade log for repeated input/setup failures. Command-recording messages are not proof of GPU completion or performance.

The existing On/Off RenderDoc captures establish native order and resource relationships but omit addon work. They cannot validate custom execution, histories, or performance. Tests and analysis artifacts remain local-only and outside the standalone addon build.
