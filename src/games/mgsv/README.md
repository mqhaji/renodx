# MGSV RenoDX

RenoDX for Metal Gear Solid V: The Phantom Pain adds HDR output and color grading, higher-precision textures, temporal reconstruction, improved shadows, ambient occlusion (AO), global illumination (GI), and specular occlusion.

## Temporal reconstruction

**Temporal Reconstruction** selects Off (Vanilla FXAA), Analytical TAA, AMD FSR 3.1.5, or NVIDIA DLSS. The temporal methods combine samples from several frames to smooth edges. FSR3 and DLSS can also reconstruct a lower-resolution scene at the output resolution; Analytical TAA uses native resolution. New profiles default to FSR3. Off restores the original camera projection and FXAA. Enabled methods skip FXAA filtering while keeping the draw's other scene processing.

Reconstruction settings stay independent of RenoDX presets. DLSS requires a compatible `nvngx_dlss.dll` beside the game executable; its default model is F when the DLL supports it. Character wobble and some special-view flicker remain unresolved. See the [reconstruction guide](reconstruction/README.md) for controls, startup and shutdown behavior, and testing.

## Bloom

**Bloom Type** selects Vanilla or Enhanced (the default). Enhanced blends the scene with filtered linear light. Bloom strength controls either type. See the [bloom guide](bloom/README.md) for details.

## Indirect Lighting

**Screen-Space Lighting** selects Off or GT-VBGI. One search through scene depth supplies AO contact shading, GI light bouncing from visible surfaces, and broad specular transport. A focused GGX ray covers narrow reflections. Both use the shared lighting filters. Diffuse GI, Specular GI, Ambient Occlusion, and Specular Occlusion have separate strengths; resolution and sampling controls are shared.

AO darkens only qualified native ambient diffuse. Direct light keeps its own shadows, and GI is added without another AO multiplier. The shared lighting chain runs before skin scattering, so skin GI scatters with the native light. Qualified direct highlights also avoid native ambient AO; screen reflections replace the overlapping material-cube and environment contributions before fog. Native SSR keeps tracing to preserve detail that GT-VBGI can miss. Both regular SSR and the sortie menu's floor path keep the original complementary blend against the current scene. Missing ambient inputs skip diffuse AO, and unsupported specular pixels keep native occlusion. Ambient Diffuse, Direct Diffuse, Corrected Diffuse, and Ambient Input Validity debug views help check these boundaries.

Defaults are GT-VBGI, all four strengths at 100%, 1/4 resolution, a 2000-pixel radius, 40 samples per side, three sampling directions, surface thickness 0.15, and surface offset 1/1024 in the game's view-space units. Full/1/4/1/16/1/64 describe total pixel count: the reduced options divide each axis by 2, 4, or 8. Coarse depth sampling and GI backface rejection default to Off. GI sources use diffuse light and albedo. Noise uses IS-FAST.

Surface Thickness controls the assumed depth of sampled geometry. Search Radius changes reach; samples and directions control coverage quality. **Specular GI Strength** scales added reflected light after filtering. **Specular Occlusion Strength** controls replacement of environment light by visible surfaces. Use 100% for the complementary screen/environment split; lower values retain more environment light. It can darken blocked reflections or restore reflections that native AO darkens too much. **Reject Backfacing GI Sources** suppresses source light while retaining its occlusion.

**Temporal Accumulation** and **Spatial Filtering** control AO, GI, and specular visibility together, both On by default. AO and specular visibility share history weight and depth/normal rejection controls, with separate history clamps; GI has its own controls. Setting a history weight to zero uses only current samples for those signals. Defaults are 90% history weight and three A-Trous passes with filter width 5 and radius 8. Each pass keeps the center and samples one quarter of neighboring taps. Radius only affects spacing in later passes.

**Lighting Debug View** shows lighting, coverage, histories, native roughness, captured environment reflections, and raw/temporal/filtered specular visibility. Specular Confidence shows how much the custom estimate can replace native occlusion. Magenta marks missing or unsupported estimates; cyan in specular visibility means the sampled slices missed the reflection cone. Thin edges and grazing surfaces can lack custom support and keep native environment occlusion in normal rendering. Surface color, specular reflections, fog, and later image processing still affect these views.

All signals share the trace and filters. The default chain uses six compute dispatches: trace, temporal accumulation, three spatial filters, and full-resolution application. Optional depth reductions add four. Missing temporal inputs use current samples, and histories retain the temporal results before spatial filtering. See the [lighting reference](gtvbgi/README.md) for the algorithms and MGSV adaptation.

### Native AO compatibility

With Screen-Space Lighting enabled, the six composition shaders skip the game's AO on diffuse because ambient AO was applied earlier. Direct highlights also skip native AO where the environment contribution can be separated. Bitmask visibility replaces native AO on supported environment reflections, blended by confidence. Missing environment capture keeps the original specular AO; missing custom visibility keeps native AO on environment reflections. Separate rain reflections retain native AO. Off restores the original composition. The game still calculates its AO in either mode.

The diffuse bypass follows the setting: if custom lighting cannot run while enabled, diffuse receives neither the game's AO nor custom AO at that insertion. Missing ambient reconstruction similarly skips diffuse AO while allowing valid GI to continue.

Preset Off disables combined lighting and Enhanced Shadows while leaving reconstruction selected. Reset All restores defaults.

## Shadows

Contact-hardening filters soften the native sun, spot, and point shadow maps. Bend screen-space shadows add nearby contact detail from visible geometry before each supported light draw. They affect direct diffuse and specular lighting while preserving environment reflections and native alpha. Enhanced Shadows enables both methods for all supported light types. Shadows contains Enhanced Shadows and Shadow Softness, which ranges from 0% to the default 100%. Contact reach, the spotlight budget, and diagnostics are in Shadows (Advanced).

Sampling is fixed at 12 sun/moon blocker samples, 4 local blocker samples, 4 map-filter samples, and contact contrast 4. Contact Shadow Reach in Advanced defaults to 60 output pixels for all lights and is converted to render pixels when upscaling; larger values search farther and increase cost. Contact shadows run at full rendering resolution with all sample groups. Surface thickness is fixed at 0.5% and the bilinear depth threshold at 2%. Contact-shadow work outside a light's cone or range is always skipped.

Spot Contact Shadow Budget defaults to 34 and selects the first eligible spotlight evaluations between native lighting-buffer clears. Zero is unlimited. Lights beyond the budget retain shadow-map filtering. Compatible point and spot lights without shadow maps share one setup, clear, and trace chain per batch. Each light still supplies its own constants and reads its own mask; other lights keep the per-light path.

See the [shadow guide](shadows/README.md) for controls, debug views, and testing.

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

## Build and deploy

Build the `mgsv` target through CMake Tools with the approved configuration. Builds are allowed while MGSV runs. Stop the game before deploying `renodx-mgsv.addon64` beside `mgsvtpp.exe`, then restart. Camera hooks stay installed until the game exits, so do not unload or replace the addon while playing. Replace the DLSS DLL only with the game stopped too.

After shader or shared-layout changes, verify that generated headers match the compiled shaders and that the deployed addon contains those bytes. The lighting outputs are `gtvbgi`, `gtvbgi_depth_mips`, `gtvbgi_temporal`, `gtvbgi_filter`, and `gtvbgi_add` under `build/mgsv.include/embed`. A successful build or automated test still needs gameplay checks.

## Manual validation

1. Confirm RenoDX loads in ReShade and saved settings survive restart. Exercise Reset All and Preset Off; reconstruction must remain independently selected.
2. Compare Screen-Space Lighting Off/GT-VBGI with the camera and other settings fixed. Vary AO, GI, and Specular Occlusion strengths independently. Check contact shading, color bounce, rough and smooth reflections, hair, foliage, and screen edges. GT-VBGI Off restores native reflections; specular strength 0% retains the environment alongside screen light. Check transitions into unsupported regions, where native reflections remain active.
3. Check Full/1/4/1/16/1/64 and coarse depth sampling Off/On, including odd render dimensions. Look for shifted output, edge strips, leaks, and loss of thin geometry.
4. Check fog, rain, skin scattering, SDR/HDR, camera motion, and reconstruction Off/On. Direct highlights and separate rain reflections should retain their existing treatment. Check cold startup, gameplay, and returning to the menu for missing or stale specular data.
5. Compare temporal and spatial filtering separately and together. Set either history weight to zero to check the other signal independently. Check stationary noise, moving characters, newly revealed surfaces, exposure changes, and the temporal/filtered/history debug views. Toggling filtering should not shift the image or change overall brightness.
6. Follow the [reconstruction validation matrix](reconstruction/README.md#manual-validation) for method changes, animation, special views, and lifecycle checks.
7. Read the fresh ReShade log for repeated input/setup failures. Command-recording messages are not proof of GPU completion or performance.

The existing On/Off RenderDoc captures establish native order and resource relationships but omit addon work. They cannot validate custom execution, histories, or performance. Tests and analysis artifacts remain local-only and outside the standalone addon build.
