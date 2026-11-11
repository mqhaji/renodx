# MGSV RenoDX

RenoDX for Metal Gear Solid V: The Phantom Pain adds HDR output and color grading, higher-precision textures, temporal reconstruction, improved shadows, and screen-space lighting.

## Guides

| Guide | Contents |
|---|---|
| [Temporal reconstruction](reconstruction/README.md) | Reconstruction methods, upscaling quality, controls, compatibility, and testing |
| [Indirect lighting](lighting/indirect/README.md) | AO, diffuse and specular GI, native reflections, diagnostics, and limitations |
| [Enhanced Shadows](lighting/shadows/README.md) | Shadow-map filtering, contact shadows, controls, and testing |
| [Bloom](postprocess/bloom/README.md) | Vanilla and Enhanced bloom, color handling, and testing |
| [Rendering architecture](docs/ARCHITECTURE.md) | File ownership, frame order, shared layouts, and submission rules |
| [Reconstruction roadmap](reconstruction/ROADMAP.md) | Open investigations and future work |
| [Shader name map](docs/MGSV_PixelShader_NameMap.md), [Fox Engine reference](docs/Fox_Engine_Render_Pipeline_Reference.md) | Historical native shader and executable findings, not current addon behavior |

## Temporal reconstruction

**Temporal Reconstruction** selects Off (Vanilla FXAA), Analytical TAA, AMD FSR 3.1.5, or NVIDIA DLSS. Analytical TAA runs at native resolution. FSR and DLSS can reconstruct a lower-resolution scene at the output resolution. New profiles use FSR3 with Auto quality. DLSS requires a compatible `nvngx_dlss.dll` beside the game executable; model F is the default when supported.

Reconstruction stays independent of RenoDX presets. Off restores the original camera projection and FXAA. Character wobble, rain flashes, and some special-view issues remain unresolved; see the [current limitations](reconstruction/README.md#known-limitations-and-validation-status).

## Indirect lighting

**Screen-Space Lighting** selects Off or GT-VBGI, enabled by default. It shares tracing and filtering across ambient occlusion (AO), diffuse GI, specular GI, and specular occlusion, with separate strengths for each. AO darkens only supported native ambient diffuse; direct light keeps its own shadows, and GI is added without another AO multiplier. Lighting is applied before both native skin-scattering passes.

Screen reflections replace overlapping material-cube and environment contributions before fog. Native SSR remains active to preserve detail the custom estimate can miss. Missing material or specular inputs retain native reflections. The [lighting guide](lighting/indirect/README.md) owns sampling defaults, debug views, native AO compatibility, and transport limits.

## Shadows

**Enhanced Shadows**, enabled by default, combines contact-hardening shadow-map filtering with Bend screen-space contact shadows. It affects direct diffuse and specular lighting while preserving native environment reflections, masks, fades, and alpha. **Shadow Softness** controls the shared source size. Contact reach, the spotlight budget, and diagnostics are under Shadows (Advanced); see the [shadow guide](lighting/shadows/README.md).

## Bloom

**Bloom Type** selects Vanilla or Enhanced, with Enhanced as the default. Both reuse the game's passes. Enhanced blends the scene with filtered linear light; native lens flares remain additive. **Bloom** controls strength. See the [bloom guide](postprocess/bloom/README.md).

## Precision and presets

Precision upgrades work independently of HDR output and reconstruction. Selected scene color, DoF, motion-blur, velocity, and color-grading LUT textures use RGBA16F replacements. MGSV reuses shaders and textures for unrelated work, so redirects require an identified resource role rather than a matching size or shader name. Float storage can still contain encoded color.

Preset Off disables custom screen-space lighting and Enhanced Shadows but leaves reconstruction selected. Reset All restores defaults. Vanilla/0 tonemapping is not full Preset Off; other selected effects and grading controls can remain active.

## Build and deploy

Build the `mgsv` target through CMake Tools with the approved configuration. Builds may run while MGSV is open. Stop the game before deploying `renodx-mgsv.addon64` beside `mgsvtpp.exe`, then restart. Camera hooks remain installed until the process exits; do not unload or replace the addon while playing. Replace the DLSS DLL only with the game stopped too.

After shader or shared-layout changes, compare the compiled shaders under `build/mgsv.include/embed`, generated header payloads, and final addon embeds. A successful build alone can leave stale embeds. Tests, probes, captures, and analysis artifacts remain local-only and outside the standalone addon build.

## Manual validation

1. Confirm RenoDX loads and saved settings survive restart. Exercise Reset All and Preset Off; reconstruction must remain independently selected.
2. Follow the [lighting checks](lighting/indirect/README.md#build-and-gameplay-checks) for AO/GI, native reflections, skin, foliage, fog, and missing-input fallback.
3. Follow the [shadow checks](lighting/shadows/README.md#diagnostics-and-verification) for sun/moon, local lights, contact edges, and different rendering resolutions.
4. Follow the [reconstruction matrix](reconstruction/README.md#manual-validation) for method and quality changes, animation, loading, menus, resizing, and special views. Include bloom, DoF, motion blur, SDR/HDR, and UI clipping in the same visit.
5. Check the fresh ReShade log for repeated setup failures. Command-recording messages do not prove GPU completion, valid history, or performance. Compare performance at a fixed camera with clean On / Off / On runs and diagnostics/capture copies disabled.

The existing On/Off RenderDoc captures establish native order and resource relationships but omit addon work. Local API tests and completed probe captures cover specific cases, not every game path. Fresh gameplay checks are still required after structural changes.