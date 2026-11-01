# MGSV RenoDX

RenoDX for Metal Gear Solid V: The Phantom Pain adds HDR output and color grading, higher-precision textures, temporal reconstruction, improved shadows, ambient occlusion (AO), and global illumination (GI).

## Temporal reconstruction

**Temporal Reconstruction** selects Off (Vanilla FXAA), Analytical TAA, AMD FSR 3.1.5, or NVIDIA DLSS. These methods combine samples from several frames to smooth edges, all at the game's render resolution. New profiles default to FSR3. Off restores the original camera projection and FXAA. Enabled methods skip FXAA filtering while keeping the draw's other scene processing.

Reconstruction settings stay independent of RenoDX presets. DLSS requires a compatible `nvngx_dlss.dll` beside the game executable; its default model is F when the DLL supports it. Character wobble and some special-view flicker remain unresolved. See the [reconstruction guide](reconstruction/README.md) for controls, startup and shutdown behavior, and testing.

## Indirect Lighting

**Screen-Space Lighting** selects Off or GT-VBGI. GT-VBGI combines AO contact shading with GI light bouncing from visible surfaces. Their strengths are independent. Both use the same search through scene depth, controlled by resolution, radius, thickness, offset, directions, samples, and depth sampling.

Defaults are GT-VBGI, AO and GI strengths at 75%, Half resolution, a 2000-pixel radius, 40 samples per side, one slice direction, surface thickness 0.15, and surface offset 1/1024 in the game's view-space units. Full/Half/Quarter/Eighth scale both image dimensions, so Half processes one quarter of the pixels. Coarse depth sampling and GI backface rejection default to Off; reflections contribute zero to the GI source by default. Noise uses IS-FAST.

Surface Thickness controls the assumed depth of sampled geometry. AO Strength controls contact darkening, and Global Illumination Strength controls bounce brightness. Search Radius changes reach; samples and directions control coverage quality. **Reject Backfacing GI Sources** suppresses source light while retaining its occlusion. **Reflections in GI Source** adds a view-dependent specular approximation.

**Temporal Accumulation** and **Spatial Filtering** control AO and GI together, both On by default. One temporal pass combines accepted previous samples, then each spatial pass smooths both results. Each signal keeps its own history weight and rejection controls; setting its history weight to zero uses only current samples. Defaults are 90% history weight and three 5-by-5 A-Trous passes with radius 8. Radius only affects later A-Trous passes. The separable Gaussian option filters horizontally and vertically. The shared switches provide Off comparisons.

**Lighting Debug View** shows bounce, sampled coverage, valid/invalid results, source light, temporal or filtered GI, raw/temporal/filtered AO, and separate AO/GI history acceptance in place of diffuse lighting. The game still applies surface color, specular reflections, fog, and later image processing to these views.

The added lighting runs after the game's skin-scattering passes. Its final pass applies AO to existing diffuse light and then adds GI, preserving the game's alpha channel. The default chain uses six compute dispatches; optional depth reductions add four. The game then combines surface color and lighting, adds fog, and applies its early tone curve. Missing or incompatible trace inputs skip the combined effect. Unavailable temporal inputs fall back to current samples, which can still be spatially filtered. AO and GI retain separate temporal results before spatial filtering, so blur does not feed back into history. Selected scene reconstruction also smooths the final image. See the [GT-VBGI reference](gtvbgi/README.md) for the algorithms and MGSV adaptation.

### Native AO compatibility

With Screen-Space Lighting set to GT-VBGI, the six composition shaders skip the game's AO on diffuse light because custom AO has already darkened it. The game's AO still affects specular light and separate rain reflections. Off restores its original use on both diffuse and specular. The game still calculates its AO in either mode.

The bypass follows the setting: if custom lighting cannot run with GT-VBGI selected, diffuse receives neither the game's AO nor custom AO at that insertion. Custom AO currently darkens all accumulated diffuse light, including direct lighting. GI is added afterward. Applying occlusion separately to ambient light and environment reflections remains future work.

Preset Off disables combined lighting and the six shadow enhancement switches while leaving reconstruction selected. Reset All restores defaults. Existing saved settings load normally; the addon does not rewrite presets to convert old settings.

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
2. Compare Screen-Space Lighting Off/GT-VBGI with the camera and other settings fixed. Vary AO and GI strengths independently and check contact shading, color bounce, hair, foliage, and screen edges.
3. Check Full/Half/Quarter/Eighth and coarse depth sampling Off/On, including odd render dimensions. Look for shifted output, edge strips, leaks, and loss of thin geometry.
4. Check fog, rain, skin scattering, SDR/HDR, camera motion, and reconstruction Off/On. Native specular and rain-reflection AO should remain intact.
5. Compare temporal and spatial filtering separately and together. Set either history weight to zero to check the other signal independently. Check stationary noise, moving characters, newly revealed surfaces, exposure changes, and the temporal/filtered/history debug views. Toggling filtering should not shift the image or change overall brightness.
6. Follow the [reconstruction validation matrix](reconstruction/README.md#manual-validation) for method changes, animation, special views, and lifecycle checks.
7. Read the fresh ReShade log for repeated input/setup failures. Command-recording messages are not proof of GPU completion or performance.

The existing On/Off RenderDoc captures establish native order and resource relationships but omit addon work. They cannot validate custom execution, histories, or performance. Tests and analysis artifacts remain local-only and outside the standalone addon build.
