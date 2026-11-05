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
| `shadows/screen_space_shadows.hpp` | Per-context Bend scratch resources, current-light compute setup, scoped pixel inputs, and game-state restoration |
| `reconstruction/` | Analytical TAA, FSR3, DLSS, input preparation, submission, and SDK lifecycle |
| `output/` | Output selection, display state, and final SDR/HDR encoding |

Precision upgrades support the whole mod, including output, reconstruction, shadows, and GT-VBGI. Each feature keeps its own controls and textures. Feature headers are included at file scope and do not rely on hidden variables in the addon entry point.

## Lighting order

The original second skin-scattering pass (`0xB2601810`) runs once per intercepted invocation. Its callback then records these added lighting passes:

1. Check the game's depth, normals, diffuse source/destination, and saved material references.
2. If enabled, build four smaller depth levels that keep the nearest surface in each region.
3. Search geometry once, writing AO visibility, GI light/validity, and bitmask specular visibility.
4. Accumulate the three signals against their separate histories in one temporal dispatch. Keep these results as next-frame histories.
5. Spatially filter all three signals together in each filter dispatch.
6. Reconstruct full-resolution results, write `diffuse * AO + GI` while preserving alpha, and resolve specular visibility/confidence.
7. Restore the game's compute and graphics bindings.

The game then multiplies diffuse lighting by surface color, adds specular lighting with the environment correction, applies fog, and runs its early tone curve. The default complete chain adds six compute dispatches: trace, temporal accumulation, three spatial-filter iterations, and lighting application. Specular visibility shares these passes without another dispatch. Depth reductions add four. Each spatial iteration uses one dispatch for all three signals. The callback count can vary, and dispatch counts do not measure GPU time. The first scattering pass uses a generated replacement; the second keeps the native shader with a callback.

Missing trace inputs skip custom lighting while preserving the original game pass. Missing temporal inputs use current samples and still allow spatial filtering. An unsplit deferred fallback never writes submitted history. The shared submission bridge preserves native work around lighting and later scene reconstruction; replays cannot advance history twice. It relies on serialized immediate-context submission, as the existing scene reconstruction does.

The game's diffuse-AO bypass follows the Screen-Space Lighting setting. If enabled but custom lighting cannot run, native diffuse AO is still skipped. Custom AO currently darkens all accumulated native diffuse, including direct light; GI is added afterward. Native AO remains on direct specular and separate rain reflections. Bitmask specular visibility replaces only the captured environment contribution's visibility where inputs and confidence permit; unsupported pixels retain native occlusion, and partly supported pixels blend toward it. Applying diffuse AO selectively to ambient light remains future work.

Environment RGB comes from the supported sun/environment producers, including the startup menu's environment-only shader. Capture resources and reusable shader/blend objects are owned per device. Each capture owns its RG16F specular visibility/confidence texture, and native specular clears invalidate both together. Deferred composition binds the pair by its actual native specular/depth resources, preserving displaced pixel-shader inputs at t16/t17. These stable views avoid selecting a global latest history/filter index while deferred command lists are being recorded. The correction is `strength * confidence * (new_visibility - native_AO) * environment`, added before fog/tonemapping. Strength zero preserves native occlusion. The full-resolution visibility texture costs about 31.6 MiB at 3840x2160, in addition to the environment capture and reduced-resolution specular textures.

Native captures show that later passes also copy the diffuse-light buffer to measure brightness. Applying AO/GI only during final lighting composition would leave that brightness measurement reading different data unless it is updated too.

## Input and layout contracts

- Lighting uses the scatter pass's existing 64-byte geometry constants at compute `b0`. Pixel shaders use `b0` for different data.
- `ShaderInjectData` is 76 floats: 304 CPU bytes and 304 padded GPU bytes at `b13`. One softness value controls all source sizes; map diagnostics share the contact-light selector. Shadow-map sampling counts and search bounds are fixed; Contact Shadow Reach controls the shared Bend sample count. Effect-local copies hold scale, filter steps, history flags, and the current Bend light type. Bend surface thickness and the bilinear threshold are fixed constants. The shared SDK `PrepareConstants` layout remains 96 bytes.
- Temporal `b1/b2` hold current/previous native scene data, not duplicate settings. Copy the actual native constant-buffer allocation and preserve its bound range; the reflected scene structure is 480 bytes. History owns another GPU copy because replaying a producer command list can overwrite its snapshot.
- AO raw, history, and filter textures use RG16F: R is visibility and G is validity. GI uses RGBA16F with validity in alpha. One shared pair of R32G32_UINT geometry textures stores FP32 linear-depth bits and two packed 16-bit normal coordinates. Shader calculations use FP32; no radiance variance textures are allocated.
- Lighting has eleven texture-input slots (SRVs) and five writable-output slots (UAVs). Unused outputs use distinct placeholder textures so they cannot conflict with inputs or each other.
- Specular visibility uses five RGBA16F textures: raw, two temporal, and two spatial results. RG stores normalized visible/open angular areas, B receiver roughness, and A current sampling support. Alpha -1 marks missing data; zero support and zero/zero areas remain valid. History checks the previous reflection direction and roughness, then clamps the visibility ratio. Spatial filtering preserves the center's roughness and support. Visibility is not exposure-scaled. Missing specular inputs retain native occlusion without suppressing AO/GI; missing filter inputs use the last valid stage.
- Normals use the game's packing in camera coordinates. Position reconstruction keeps signed projection scales, including negative X, and the game's camera-offset convention. Matching jitter under live rendering still needs verification.
- The albedo sRGB view decodes base color. Native diffuse/specular lighting is linear and already scaled by exposure. Copied diffuse alpha identifies skin-scattering materials, not every shaded pixel. GI alpha identifies valid results and remains separate from AO visibility.
- The first accepted Present selects one device/swapchain. Resize and destruction invalidate old resource versions. Resizing textures does not rebuild unchanged shader pipelines.
- Holding COM texture references keeps objects alive while the game can still overwrite their pixels. Matching sizes or context/Present identifiers alone cannot prove matching scene content, fresh writes, or execution order.

GT-VBGI uses one A-Trous filter with stochastic 1/4 neighbor sampling. It reads the existing noise texture at filter t5 with a different spatial offset from the trace. The pass binder clears previous input views before binding ping-pong outputs, avoiding D3D11 automatic-unbinding hazards. Surrounding game state is restored once after the chain.

## Screen-space shadows

Bend contact shadows run before each supported sun, spot, or point light draw. GPU setup reads that draw's native scene and light buffers at b2/b3, preserving their bound ranges. Two indirect dispatches clear the current light's bounds and trace its projected rays. The light shader applies visibility to direct diffuse and specular before environment reflections are added. The startup menu's environment-only pass does not need contact shadows.

Scratch resources belong to each recording context. Full-resolution masks use RG8 UNORM where supported, with an RG16F fallback. Pixel t18/t19 expose the mask and dispatch metadata only during the light draw. Its resolved visibility combines with native map masks and fades before coloring direct light. Fully blocked pixels skip map filtering unless diagnostics need it; local lights also skip the remaining BRDF and lookup work. Derivatives run before divergent exits, and the one-mip material/BRDF lookups use explicit level zero. Pixel t20 uses the immutable IS-FAST noise shared with GT-VBGI for PCSS; Bend needs no noise input. The host restores displaced inputs and every changed compute binding, including shader class instances and constant-buffer ranges. Missing inputs retain native lighting.

Setup uses each native light's attenuation and spotlight cone to bound receivers. Shared casting depths remain available beyond those bounds. Skip Unlit Areas defaults to On: whole lights and sweeps can skip depth reads, while individual receivers skip their shadow calculation only after supplying shared depths. Conservative margins retain uncertain edges.

Spot Contact Shadow Budget defaults to 13. A 16-byte GPU counter belongs to the native diffuse-lighting resource and resets beside its full clear, so deferred execution and replay follow GPU order. Once full, later spots skip contact projection and sweep setup. PCSS projection data remains available. Rejected evaluations keep native shadows; CPU submission still runs. This counts evaluations between clears, without ranking lights or identifying unique light objects per Present.

Budget and culling skips publish metadata state 2 with empty clear/trace arguments; consumers must not read the previous mask. State 0 means unavailable inputs, and state 1 means an active trace. Setup resets only the two actual indirect argument triples; active sweep entries are overwritten before use, and the published count excludes unused entries. Five sun/spot projection values occupy bytes 348-367 of the 368-byte dispatch buffer and are computed before contact tracing can be skipped. Pixel shaders fall back to matrix calculations when those values are unavailable.

Tracing defaults to a 60-pixel reach, shared by all light types, with contrast 4, four hard samples, and eight end-fade samples at full resolution. Surface thickness is fixed at 0.005 and the bilinear depth threshold at 0.02. All four farther-sample groups are evaluated; fully blocked hard contacts skip their reduction. Only required shared-depth blocks are read. Shader/sampler objects and immutable view descriptions are cached; current native bindings are still checked per draw. See the [shadow guide](../shadows/README.md) for controls and limitations.

## Settings and reconstruction

Screen-Space Lighting appears under Indirect Lighting, uses Off / GT-VBGI, defaults to enabled, and uses the key `FxIndirectLightingEnabled`. AO, GI, and specular-occlusion strengths appear before Lighting Resolution. Sampling and filtering controls remain in the collapsed advanced section. Enhanced Shadows uses `FxEnhancedShadowsEnabled`, defaults to On, and enables PCSS and Bend for all supported lights. `ShadowSoftness` ranges from 0% to its 100% default. The spotlight budget, light culling, and shared diagnostics are in Advanced. Preset Off disables lighting and Enhanced Shadows while leaving reconstruction selected.

Lighting Resolution labels describe total pixel count: Full, 1/4, 1/16, and 1/64 use axis divisors of 1, 2, 4, and 8. The default is 1/16, with each axis divided by 4. Reduced texture extents round up at odd dimensions, so the fractions are nominal.

Temporal Accumulation, Spatial Filtering, and filter controls are shared through `FxIndirectLightingTemporalAccumulationEnabled`, `FxIndirectLightingSpatialFilteringEnabled`, and `FxIndirectLightingFilter*` keys. AO and specular visibility share history weight and depth/normal rejection, with separate history clamps. GI retains its own controls. Their histories and validity stay separate. Either history weight can be zero while the other signals continue accumulating.

Controls refer to addon-owned storage that must outlive their callbacks. Only the selected reconstruction method runs. FSR/DLSS quality modes request a lower native scene size and use a separate output before bloom preparation. DoF rectangle/grid arguments, post-processing resources, motion-blur units, relevant constant fields, viewports, scissors, copies, and UI depth/stencil follow the output dimensions. Analytical and Off retain native sizing; Native uses the existing scene target. Mip bias and jitter phase count follow the actual render/output ratio. Reconstruction is scheduled before its first consumer is routed to the output. If reconstruction is canceled, the current native scene fills that output before the rest of the game's command list runs. The supported native DoF path is scatter; unsupported paths request native-size recovery. The user reports working scaled gameplay; final-build and special-path checks are listed in the [reconstruction guide](../reconstruction/README.md).

Earlier explicit shader registrations take priority over generated entries. `on_inject=false` disables settings injection; replacement bytecode can still run if supplied. Deferred callbacks upgrade scene color before retaining material references. Preserve that order.

## Verification limits

The Desktop On/Off RenderDoc captures show native rendering order and inputs. They omit addon replacements and added passes even though the ReShade log from that session records addon activity. The reason is unresolved. These captures cannot validate custom execution, GPU cost, upgraded texture contents, or histories.

Recording a GPU command does not prove that it executed or completed. Local shader/API tests and bytecode comparisons check specific behavior; gameplay testing checks the actual image. Compare generated shader headers, compiled outputs, and the deployed addon, especially after settings-layout changes. Keep tests and analysis local and remove temporary test registration before building the standalone addon. Builds may run while the game is open; never deploy while it is running.
