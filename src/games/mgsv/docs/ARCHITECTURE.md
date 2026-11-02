# MGSV rendering structure

## Ownership

| File or directory | Responsibility |
|---|---|
| `addon.cpp` | Settings storage, assembled controls, ReShade events, and device cleanup |
| `shared.h` | The single CPU/HLSL settings layout |
| `settings.hpp`, `shadows/settings.hpp`, `reconstruction/settings.hpp` | Core, shadow, and reconstruction controls; Reset All and Preset Off policy |
| `shaders.hpp` | Shader registrations and draw/dispatch callbacks |
| `inputs.hpp` | Selected device/swapchain, resize/reset versions, and native material/motion references |
| `resource_upgrades.hpp` | Shared precision upgrades, clone activation, and scoped copies |
| `gtvbgi/lighting.hpp` | Combined lighting textures, controls, input checks, GPU commands, and game-state restoration |
| `gtvbgi/temporal_inputs.hpp` | Native camera/exposure copies and motion records tied to submitted producer commands |
| `gtvbgi/environment_specular.hpp` | Per-device capture state, environment/visibility texture pairs, and scoped native/deferred bindings |
| `gtvbgi/` | Shared trace and filters, diffuse application, selective environment correction, and [reference explanation](../gtvbgi/README.md) |
| `reconstruction/` | Analytical TAA, FSR3, DLSS, input preparation, submission, and SDK lifecycle |
| `output/` | Output selection, display state, and final SDR/HDR encoding |

Precision upgrades support the whole mod, including output, reconstruction, shadows, and GT-VBGI. Each feature keeps its own controls and textures. Feature headers are included at file scope and do not rely on hidden variables in the addon entry point.

## Lighting order

The original second skin-scattering pass (`0xB2601810`) runs once per intercepted invocation. Its callback then records these added lighting passes:

1. Check the game's depth, normals, diffuse source/destination, and saved material references.
2. If enabled, build four smaller depth levels that keep the nearest surface in each region.
3. Search geometry once, writing AO visibility, GI light/validity, and directional visibility.
4. Accumulate the three signals against their separate histories in one temporal dispatch. Keep these results as next-frame histories.
5. Spatially filter all three signals together in each filter dispatch.
6. Reconstruct full-resolution results, write `diffuse * AO + GI` while preserving alpha, and resolve specular visibility/confidence.
7. Restore the game's compute and graphics bindings.

The game then multiplies diffuse lighting by surface color, adds specular lighting with the environment correction, applies fog, and runs its early tone curve. The default complete chain adds six compute dispatches: trace, temporal accumulation, three spatial-filter iterations, and lighting application. Directional visibility and specular occlusion share these passes without another dispatch. Depth reductions add four. A-Trous uses one filter dispatch per iteration; Separable uses two. The callback count can vary, and dispatch counts do not measure GPU time. The first scattering pass uses a generated replacement; the second keeps the native shader with a callback.

Missing trace inputs skip custom lighting while preserving the original game pass. Missing temporal inputs use current samples and still allow spatial filtering. An unsplit deferred fallback never writes submitted history. The shared submission bridge preserves native work around lighting and later scene reconstruction; replays cannot advance history twice. It relies on serialized immediate-context submission, as the existing scene reconstruction does.

The game's diffuse-AO bypass follows the Screen-Space Lighting setting. If enabled but custom lighting cannot run, native diffuse AO is still skipped. Custom AO currently darkens all accumulated native diffuse, including direct light; GI is added afterward. Native AO remains on direct specular and separate rain reflections. GTSO replaces only the captured environment contribution's visibility where inputs and confidence permit; other pixels retain native occlusion. Applying diffuse AO selectively to ambient light remains future work.

Environment RGB comes from the supported sun/environment producers, including the startup menu's environment-only shader. Capture resources and reusable shader/blend objects are owned per device. Each capture owns its RG16F specular visibility/confidence texture, and native specular clears invalidate both together. Deferred composition binds the pair by its actual native specular/depth resources, preserving displaced pixel-shader inputs at t16/t17. These stable views avoid selecting a global latest history/filter index while deferred command lists are being recorded. The correction is `strength * confidence * (new_visibility - native_AO) * environment`, added before fog/tonemapping. Strength zero preserves native occlusion. The full-resolution visibility texture costs about 31.6 MiB at 3840x2160, in addition to the environment capture and directional textures.

Native captures show that later passes also copy the diffuse-light buffer to measure brightness. Applying AO/GI only during final lighting composition would leave that brightness measurement reading different data unless it is updated too.

## Input and layout contracts

- Lighting uses the scatter pass's existing 64-byte geometry constants at compute `b0`. Pixel shaders use `b0` for different data.
- `ShaderInjectData` is 102 floats: 408 CPU bytes and 416 padded GPU bytes at `b13`. `lighting_mode` is at byte 232, specular strength at byte 244, and the noise index at byte 352. AO history validity/activity are at bytes 392/396; directional activity/history validity are at bytes 400/404. Directional activity is a dispatch-local availability flag, not a user control. The shared SDK `PrepareConstants` layout remains 96 bytes.
- Temporal `b1/b2` hold current/previous native scene data, not duplicate settings. Copy the actual native constant-buffer allocation and preserve its bound range; the reflected scene structure is 480 bytes. History owns another GPU copy because replaying a producer command list can overwrite its snapshot.
- AO raw, history, and filter textures use RG16F: R is visibility and G is validity. GI uses RGBA16F with validity in alpha. Directional textures use RGBA16F for an unnormalized moment and raw visibility, with alpha -1 for invalid data. One shared pair of R32G32_UINT geometry textures stores FP32 linear-depth bits and two packed 16-bit normal coordinates. Shader calculations use FP32; no radiance variance textures are allocated.
- Lighting has eleven texture-input slots (SRVs) and five writable-output slots (UAVs). Unused outputs use distinct placeholder textures so they cannot conflict with inputs or each other.
- Normals use the game's packing in camera coordinates. Position reconstruction keeps signed projection scales, including negative X, and the game's camera-offset convention. Matching jitter under live rendering still needs verification.
- The albedo sRGB view decodes base color. Native diffuse/specular lighting is linear and already scaled by exposure. Copied diffuse alpha identifies skin-scattering materials, not every shaded pixel. GI alpha identifies valid results and remains separate from AO visibility.
- The first accepted Present selects one device/swapchain. Resize and destruction invalidate old resource versions. Resizing textures does not rebuild unchanged shader pipelines.
- Holding COM texture references keeps objects alive while the game can still overwrite their pixels. Matching sizes or context/Present identifiers alone cannot prove matching scene content, fresh writes, or execution order.

## Settings and reconstruction

Screen-Space Lighting appears under Indirect Lighting, uses Off / GT-VBGI + GTSO, defaults to enabled, and uses the key `FxIndirectLightingEnabled`. AO, GI, and specular-occlusion strengths appear before Lighting Resolution. Sampling and filtering controls remain in the collapsed advanced section. Bent normals are part of the enabled effect. Preset Off disables lighting and the six shadow switches while leaving reconstruction selected.

Temporal Accumulation, Spatial Filtering, and filter controls are shared through `FxIndirectLightingTemporalAccumulationEnabled`, `FxIndirectLightingSpatialFilteringEnabled`, and `FxIndirectLightingFilter*` keys. Directional visibility uses the AO history-weight, depth-rejection, normal-rejection, and history-clamp controls; GI retains its own controls. Their histories and validity stay separate. Either history weight can be zero while the other signals continue accumulating.

Controls refer to addon-owned storage that must outlive their callbacks. Separate control lists still share the same renderer. Reconstruction uses equal render/output dimensions, and only the selected method runs. Canceling reconstruction must preserve the native commands recorded before and after its insertion. See the [reconstruction guide](../reconstruction/README.md).

Earlier explicit shader registrations take priority over generated entries. `on_inject=false` disables settings injection; replacement bytecode can still run if supplied. Deferred callbacks upgrade scene color before retaining material references. Preserve that order.

## Verification limits

The Desktop On/Off RenderDoc captures show native rendering order and inputs. They omit addon replacements and added passes even though the ReShade log from that session records addon activity. The reason is unresolved. These captures cannot validate custom execution, GPU cost, upgraded texture contents, or histories.

Recording a GPU command does not prove that it executed or completed. Local shader/API tests and bytecode comparisons check specific behavior; gameplay testing checks the actual image. Compare generated shader headers, compiled outputs, and the deployed addon, especially after settings-layout changes. Keep tests and analysis local and remove temporary test registration before building the standalone addon. Never build or deploy while the game is running.
